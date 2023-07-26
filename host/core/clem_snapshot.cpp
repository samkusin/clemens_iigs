#include "clem_snapshot.hpp"
#include "clem_apple2gs.hpp"

#include "clem_disk.h"
#include "clem_host_platform.h"
#include "clem_smartport.h"
#include "external/cross_endian.h"
#include "external/mpack.h"
#include "miniz.h"
#include "spdlog/spdlog.h"

#include <cstdio>
#include <memory>

namespace {
static const char *kValidationStepNames[] = {"None", "Header", "Metadata", "Machine", "Custom"};
static constexpr size_t kUncompressedBlockSize = 65536;

struct ClemensCompressedWriter {
    FILE *fpout = NULL;
    char *buffer = nullptr;
    size_t bufferSize = 0;
    mz_uint8 *compBuffer = nullptr;
    uLong compBufferSize = 0;
    mpack_writer_t writer{};
    unsigned flushCount = 0;

    ClemensCompressedWriter(FILE *fp) : fpout(fp) {
        buffer = new char[kUncompressedBlockSize];
        bufferSize = kUncompressedBlockSize;

        mpack_writer_init(&writer, buffer, bufferSize);
        mpack_writer_set_context(&writer, this);
        mpack_writer_set_flush(&writer, flush);
    }
    ~ClemensCompressedWriter() { finish(); }

    void finish() {
        if (!buffer)
            return;
        mpack_writer_destroy(&writer);
        delete[] buffer;
        buffer = nullptr;
        if (compBuffer) {
            delete[] compBuffer;
            compBuffer = nullptr;
        }
    }

    static void flush(mpack_writer_t *writer, const char *outbuf, size_t count) {
        auto *context = reinterpret_cast<ClemensCompressedWriter *>(mpack_writer_context(writer));

        //  compress
        uLong compSize = compressBound(count);
        if (compSize == 0) {
            mpack_writer_flag_error(writer, mpack_error_io);
            return;
        }

        if (context->compBufferSize < compSize) {
            if (context->compBuffer) {
                delete[] context->compBuffer;
            }
            context->compBuffer = new uint8_t[compSize];
            context->compBufferSize = compSize;
        }

        int compResult = compress(context->compBuffer, &compSize, (const uint8_t *)outbuf, count);
        if (compResult != Z_OK) {
            spdlog::error("ClemensCompressedWriter: error compressing {} bytes, code = {}", count,
                          compResult);
            mpack_writer_flag_error(writer, mpack_error_io);
            return;
        }

        //  write out the compressed data block with a header containing the compressed length
        uint8_t header[16]{};
        header[0] = 'C';
        header[1] = 'L';
        header[2] = 'E';
        header[3] = 'M';
        header[4] = 'S';
        header[5] = 'N';
        header[6] = 'A';
        header[7] = 'P';
        uint32_t compSizeHeader = htole32(compSize);
        memcpy(header + 8, &compSizeHeader, sizeof(compSizeHeader));
        compSizeHeader = htole32((unsigned)count);
        memcpy(header + 12, &compSizeHeader, sizeof(compSizeHeader));

        size_t writeCount = fwrite(header, sizeof(header), 1, context->fpout);
        if (writeCount < 1) {
            spdlog::error("ClemensCompressedWriter: error writing compression header {}",
                          context->flushCount);
            mpack_writer_flag_error(writer, mpack_error_io);
            return;
        }

        writeCount = fwrite(context->compBuffer, 1, compSize, context->fpout);
        if (writeCount < compSize) {
            spdlog::error("ClemensCompressedWriter: error writing {} buffer bytes", compSize);
            mpack_writer_flag_error(writer, mpack_error_io);
        }
        spdlog::info("ClemensCompressedWriter: Chunk {}: original: {}, compressed: {}",
                     context->flushCount, count, compSize);
        context->flushCount++;
    }
};

struct ClemensCompressedReader {
    FILE *fpin = NULL;
    char buffer[8192];
    char *readbuffer = nullptr;
    size_t readbufferHead = 0;
    size_t readbufferTail = 0;
    size_t readbufferSize = 0;
    mz_uint8 *compBuffer = nullptr;
    uLong compBufferSize = 0;
    mpack_reader_t reader{};
    unsigned fillCount = 0;

    ClemensCompressedReader(FILE *fp) : fpin(fp) {
        //  allocating extra space to contain data that hasn't yet been written
        //  to our fill buffer, but still enough to contain a full uncompressed
        //  data chunk from disk.
        readbuffer = new char[kUncompressedBlockSize + sizeof(buffer)];
        readbufferSize = kUncompressedBlockSize + sizeof(buffer);

        mpack_reader_init(&reader, buffer, sizeof(buffer), 0);
        mpack_reader_set_context(&reader, this);
        mpack_reader_set_fill(&reader, fill);
    }
    ~ClemensCompressedReader() { finish(); }

    void finish() {
        if (!readbuffer)
            return;
        mpack_reader_destroy(&reader);
        delete[] readbuffer;
        readbuffer = nullptr;
        if (compBuffer) {
            delete[] compBuffer;
            compBuffer = nullptr;
        }
    }

    static size_t fill(mpack_reader_t *reader, char *outbuf, size_t count) {
        auto *context = reinterpret_cast<ClemensCompressedReader *>(mpack_reader_context(reader));
        size_t fillAmount = 0;
        //  fill data from the input *uncompressed* data buffer.   when this buffer
        //  is emptied, we need to pull from disk and uncompressed that chunk
        if (context->readbufferHead + sizeof(buffer) > context->readbufferTail) {
            //  starved readbuffer - copy data from
            if (context->readbufferHead < context->readbufferTail) {
                memmove(context->readbuffer, context->readbuffer + context->readbufferHead,
                        context->readbufferTail - context->readbufferHead);
            }
            if (!feof(context->fpin)) {
                //  FILL!
                context->readbufferTail = context->readbufferTail - context->readbufferHead;
                context->readbufferHead = 0;
                uint8_t header[16];
                size_t headerSize = fread(header, 16, 1, context->fpin);
                if (headerSize != 1) {
                    spdlog::error("ClemensCompressedReader: no compression header at {}!",
                                  context->fillCount);
                    mpack_reader_flag_error(reader, mpack_error_io);
                    return 0;
                }
                if (memcmp(header, "CLEMSNAP", 8) != 0) {
                    spdlog::error(
                        "ClemensCompressedReader: invalid compression header at {} ({},{},{},{})",
                        context->fillCount, header[0], header[1], header[2], header[3]);
                    mpack_reader_flag_error(reader, mpack_error_io);
                    return 0;
                }
                //  aligned to 32-bit should be OK on all platforms
                const uint32_t *compSizePtr = (const uint32_t *)(header + 8);
                uint32_t compSize = le32toh(*compSizePtr);
                if (compSize > context->compBufferSize) {
                    if (context->compBuffer) {
                        delete[] context->compBuffer;
                    }
                    context->compBuffer = new mz_uint8[compSize];
                    context->compBufferSize = compSize;
                }
                uint32_t uncompressedExpected = le32toh(*(compSizePtr + 1));
                size_t sizeRead;
                if ((sizeRead = fread(context->compBuffer, 1, compSize, context->fpin)) !=
                    compSize) {
                    spdlog::error(
                        "ClemensCompressedReader: invalid compression chunk at {}, size {}, "
                        "expected {}",
                        context->fillCount, sizeRead, compSize);
                    mpack_reader_flag_error(reader, mpack_error_io);
                    return 0;
                }
                auto readbufferAvail = context->readbufferSize - context->readbufferTail;
                if (readbufferAvail < uncompressedExpected) {
                    spdlog::error("ClemensCompressedReader: no room in read buffer for chunk {}, "
                                  "(available {}, required {})",
                                  context->fillCount, readbufferAvail, kUncompressedBlockSize);
                    mpack_reader_flag_error(reader, mpack_error_io);
                    return 0;
                }
                uLong uncompressedSize = uncompressedExpected;
                int compStatus = uncompress(
                    (unsigned char *)context->readbuffer + context->readbufferTail,
                    &uncompressedSize, (const unsigned char *)context->compBuffer, compSize);
                if (uncompressedSize != uncompressedExpected) {
                    spdlog::error(
                        "ClemensCompressedReader: uncompressed sizes do not match for chunk "
                        "{},  (actual: {}, expected: {})",
                        context->fillCount, uncompressedSize, uncompressedExpected);
                    mpack_reader_flag_error(reader, mpack_error_io);
                    return 0;
                }
                context->readbufferTail += uncompressedSize;
                spdlog::info("ClemensCompressedReader: Chunk {}: original: {}, compressed: {} => "
                             "buffer({}, {})",
                             context->fillCount, uncompressedSize, compSize,
                             context->readbufferHead, context->readbufferTail);
                context->fillCount++;
            }
        }

        //  read from head of our read buffer
        size_t amountToFill = std::min(count, context->readbufferTail - context->readbufferHead);
        memcpy(outbuf, context->readbuffer + context->readbufferHead, amountToFill);
        context->readbufferHead += amountToFill;

        return amountToFill;
    }
};

} // namespace

////////////////////////////////////////////////////////////////////////////////
//  Serialization blocks
//
//  {
//    # Header is 16 bytes
//      CLEM
//      SNAP,
//      version (4 bytes)
//      pad (4 bytes)
//    # Begin Mpack
//      metadata: {
//          timestamp:
//          disks: []
//          smart_disks: []
//      },
//      debugger: {
//          breakpoints: []
//      },
//      machine_gs: {
//          ClemensAppleIIGS
//      }
//  }
//
////////////////////////////////////////////////////////////////////////////////

ClemensSnapshot::ClemensSnapshot(const std::string &path)
    : path_(path), validationStep_(ValidationStep::None) {}

void ClemensSnapshot::validation(ValidationStep step) {
    if (validationStep_ == ValidationStep::None) {
        validationData_.clear();
    }
    if (validationData_.empty()) {
        validationStep_ = step;
    }
}

void ClemensSnapshot::validationError(const std::string &tag) {
    if (validationData_.empty()) {
        validationData_ = tag;
    }
}

bool ClemensSnapshot::serialize(
    ClemensAppleIIGS &gs, std::function<bool(mpack_writer_t *, ClemensAppleIIGS &)> customCb) {
    validation(ValidationStep::None);

    FILE *fp = fopen(path_.c_str(), "wb");
    if (!fp) {
        spdlog::error("ClemensSnapshot::serialize() - Failed to open {} - stream write", path_);
        return false;
    }
    //  validation header
    spdlog::info("ClemensSnapshot::serialize() - creating snapshot @{}", path_);
    validation(ValidationStep::Header);

    uint32_t version = htole32(kClemensSnapshotVersion);
    uint32_t mpackVersion = htole32(MPACK_VERSION);
    unsigned writeCount = 0;
    writeCount += fwrite("CLEM", 4, 1, fp);
    writeCount += fwrite("SNAP", 4, 1, fp);
    writeCount += fwrite(&version, sizeof(version), 1, fp);
    writeCount += fwrite(&mpackVersion, sizeof(mpackVersion), 1, fp);
    if (writeCount != 4) {
        spdlog::error("serialize() - failed to write header (count: {})", writeCount);
        fclose(fp);
        return false;
    }

    //  begin mpack - write the header
    mpack_writer_t writer{};
    mpack_writer_init_stdfile(&writer, fp, false);
    if (mpack_writer_error(&writer) != mpack_ok) {
        spdlog::error("serialize() - Failed to initialize writer", path_);
        validationError("stream");
        fclose(fp);
        return false;
    }
    bool success = true;
    //  metadata
    mpack_build_map(&writer);
    mpack_write_kv(&writer, "timestamp", (int64_t)time(NULL));
    mpack_write_kv(&writer, "origin", CLEMENS_PLATFORM_ID);
    mpack_write_cstr(&writer, "disks");
    mpack_start_array(&writer, kClemensDrive_Count);
    for (unsigned i = 0; i < kClemensDrive_Count; i++) {
        auto driveType = static_cast<ClemensDriveType>(i);
        mpack_write_cstr(&writer, gs.getStorage().getDriveStatus(driveType).assetPath.c_str());
    }
    mpack_finish_array(&writer);
    mpack_write_cstr(&writer, "smartDisks");
    mpack_start_array(&writer, kClemensSmartPortDiskLimit);
    for (unsigned i = 0; i < kClemensSmartPortDiskLimit; i++) {
        mpack_write_cstr(&writer, gs.getStorage().getSmartPortStatus(i).assetPath.c_str());
    }
    mpack_finish_array(&writer);
    mpack_complete_map(&writer);

    //  custom
    validation(ValidationStep::Custom);
    if (success) {
        auto customSuccess = customCb(&writer, gs);
        if (!customSuccess) {
            spdlog::error("ClemensSnapshot::serialize() - custom save failed");
            success = false;
        }
    }
    mpack_writer_destroy(&writer);

    //  machine
    ClemensCompressedWriter compressedWriter(fp);
    validation(ValidationStep::Machine);
    if (success) {
        auto machineSaveResult = gs.save(&compressedWriter.writer);
        if (!machineSaveResult.second) {
            spdlog::error("ClemensSnapshot::serialize() - machine save failed @ '{}'",
                          machineSaveResult.first);
            success = false;
        }
    }
    compressedWriter.finish();

    fclose(fp);

    if (mpack_writer_error(&writer) != mpack_ok || !success) {
        spdlog::error("ClemensSnapshot::serialize() - FAILED @ {} : {}!",
                      kValidationStepNames[static_cast<int>(validationStep_)],
                      validationData_.empty() ? "n/a" : validationData_);
        return false;
    }
    return true;
}

std::unique_ptr<ClemensAppleIIGS>
ClemensSnapshot::unserialize(ClemensSystemListener &systemListener,
                             std::function<bool(mpack_reader_t *, ClemensAppleIIGS &)> customCb) {
    std::unique_ptr<ClemensAppleIIGS> gs;
    validation(ValidationStep::None);

    FILE *fp = fopen(path_.c_str(), "rb");
    if (!fp) {
        spdlog::error("Failed to open {} - stream read", path_);
        return nullptr;
    }

    spdlog::info("ClemensSnapshot::unserialize() - loading snapshot @{}", path_);
    if (!unserializeHeader(fp)) {
        fclose(fp);
        return nullptr;
    }

    mpack_reader_t reader{};
    bool success = true;
    mpack_reader_init_stdfile(&reader, fp, false);
    if (mpack_reader_error(&reader) != mpack_ok) {
        spdlog::error("serialize() - Failed to initialize writer", path_);
        validationError("stream");
        fclose(fp);
        return nullptr;
    }
    //  this is mainly used to summarize a snapshot for listing purposes and
    //  isn't really used otherwise in this function
    if (!unserializeMetadata(reader).second) {
        spdlog::error("serialize() - Failed to initialize writer", path_);
        validationError("stream");
        fclose(fp);
        return nullptr;
    }

    if (success) {
        validation(ValidationStep::Custom);
        auto customSuccess = customCb(&reader, *gs);
        if (!customSuccess) {
            spdlog::error("ClemensSnapshot::unserialize() - custom load failed");
            success = false;
        }
    }
    size_t putbackCount = mpack_reader_remaining(&reader, NULL);
    mpack_reader_destroy(&reader);

    if (fseek(fp, -putbackCount, SEEK_CUR) != 0) {
        spdlog::error("serialize() - Failed to revert overflow into compressed stream");
        success = false;
    }

    if (success) {
        ClemensCompressedReader compressedReader(fp);
        validation(ValidationStep::Machine);
        gs = std::make_unique<ClemensAppleIIGS>(&compressedReader.reader, systemListener);
        if (!gs->isOk()) {
            success = false;
        }
        compressedReader.finish();
    }

    fclose(fp);

    if (mpack_reader_error(&reader) != mpack_ok || !success) {
        spdlog::error("ClemensSnapshot::unserialize() - FAILED @ {} : {}!",
                      kValidationStepNames[static_cast<int>(validationStep_)],
                      validationData_.empty() ? "n/a" : validationData_);
        return nullptr;
    }

    return gs;
}

std::pair<ClemensSnapshotMetadata, bool> ClemensSnapshot::unserializeMetadata() {
    std::pair<ClemensSnapshotMetadata, bool> result;
    result.second = false;

    FILE *fp = fopen(path_.c_str(), "rb");
    if (!fp) {
        spdlog::error("Failed to open {} - stream read", path_);
        return result;
    }

    spdlog::info("ClemensSnapshot::unserialize() - loading snapshot @{}", path_);
    if (!unserializeHeader(fp)) {
        fclose(fp);
        return result;
    }
    mpack_reader_t reader{};
    mpack_reader_init_stdfile(&reader, fp, false);
    if (mpack_reader_error(&reader) != mpack_ok) {
        spdlog::error("serialize() - Failed to initialize writer", path_);
        validationError("stream");
        fclose(fp);
        return result;
    }
    //  this is mainly used to summarize a snapshot for listing purposes and
    //  isn't really used otherwise in this function
    auto metadata = unserializeMetadata(reader);
    if (!metadata.second) {
        spdlog::error("serialize() - Failed to initialize writer", path_);
        validationError("stream");
    }
    mpack_reader_destroy(&reader);

    fclose(fp);

    return metadata;
}

std::pair<ClemensSnapshotMetadata, bool>
ClemensSnapshot::unserializeMetadata(mpack_reader_t &reader) {
    std::pair<ClemensSnapshotMetadata, bool> result;
    char path[1024];
    char origin[8];
    uint32_t arraySize;

    validation(ValidationStep::Metadata);
    mpack_expect_map(&reader);
    mpack_expect_cstr_match(&reader, "timestamp");
    result.first.timestamp = mpack_expect_i64(&reader);
    mpack_expect_cstr_match(&reader, "origin");
    mpack_expect_cstr(&reader, origin, sizeof(origin));
    origin_ = origin;

    mpack_expect_cstr_match(&reader, "disks");
    arraySize = mpack_expect_array_max(&reader, kClemensDrive_Count);
    for (unsigned i = 0; i < arraySize; i++) {
        mpack_expect_cstr(&reader, path, sizeof(path));
        result.first.disks[i] = path;
    }
    mpack_done_array(&reader);
    mpack_expect_cstr_match(&reader, "smartDisks");
    arraySize = mpack_expect_array_max(&reader, kClemensSmartPortDiskLimit);
    for (unsigned i = 0; i < arraySize; i++) {
        mpack_expect_cstr(&reader, path, sizeof(path));
        result.first.smartDisks[i] = path;
    }
    mpack_done_array(&reader);
    mpack_done_map(&reader);

    if (mpack_reader_error(&reader) == mpack_ok) {
        result.second = true;
    } else {
        result.second = false;
    }
    return result;
}

bool ClemensSnapshot::unserializeHeader(FILE *fp) {
    uint32_t version = 0;
    uint32_t mpackVersion = 0;
    char head[4];
    unsigned readCount = 0;
    bool headerOk = true;

    validation(ValidationStep::Header);

    readCount += fread(head, 4, 1, fp);
    headerOk = memcmp(head, "CLEM", 4) == 0 && headerOk;
    readCount += fread(head, 4, 1, fp);
    headerOk = memcmp(head, "SNAP", 4) == 0 && headerOk;
    readCount += fread(&version, sizeof(version), 1, fp);
    readCount += fread(&mpackVersion, sizeof(mpackVersion), 1, fp);
    headerOk = readCount == 4 && headerOk;
    version = le32toh(version);
    if (version > kClemensSnapshotVersion) {
        spdlog::error("ClemensSnapshot::unserializeHeader() - snapshot version {} not supported",
                      version);
        validationError("version");
        headerOk = false;
    }
    mpackVersion = le32toh(mpackVersion);
    if (mpackVersion > MPACK_VERSION) {
        spdlog::error("ClemensSnapshot::unserializeHeader() - msgpack version {:0x} not supported",
                      mpackVersion);
        validationError("mpack");
        headerOk = false;
    }
    if (!headerOk) {
        spdlog::error("ClemensSnapshot::unserializeHeader() - header validation failed (count: {}, "
                      "version: {}, mpack: {})",
                      readCount, version, mpackVersion);
        return false;
    }
    return true;
}
