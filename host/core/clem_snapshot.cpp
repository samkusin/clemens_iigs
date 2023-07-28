#include "clem_snapshot.hpp"
#include "clem_apple2gs.hpp"

#include "clem_disk.h"
#include "clem_host_platform.h"
#include "external/cross_endian.h"
#include "external/mpack.h"
#include "miniz.h"
#include "spdlog/spdlog.h"

#include <cstdio>
#include <memory>

namespace {
static const char *kValidationStepNames[] = {"None", "Header", "Metadata", "Machine", "Custom"};
static constexpr size_t kUncompressedBlockSize = 64 * 1024;
static constexpr size_t kUncompressedBlockMinSize = 4096;

struct ClemensCompressedWriter {
    FILE *fpout = NULL;
    char buffer[kUncompressedBlockMinSize];
    uint8_t *rawBuffer = nullptr;
    size_t rawBufferSize = 0;
    size_t rawBufferTail = 0;
    mz_uint8 *compBuffer = nullptr;
    uLong compBufferSize = 0;
    mpack_writer_t writer{};
    unsigned flushCount = 0;

    ClemensCompressedWriter(FILE *fp) : fpout(fp) {
        mpack_writer_init(&writer, buffer, sizeof(buffer));
        mpack_writer_set_context(&writer, this);
        mpack_writer_set_flush(&writer, flush);
        mpack_writer_set_teardown(&writer, teardown);

        reallocateRawBuffer(kUncompressedBlockSize);
    }
    ~ClemensCompressedWriter() { finish(); }

    void finish() { mpack_writer_destroy(&writer); }

    void reallocateRawBuffer(size_t sz) {
        uint8_t *nextBuffer = sz ? new uint8_t[sz] : nullptr;
        if (rawBuffer) {
            if (rawBufferTail > 0 && sz) {
                memmove(nextBuffer, rawBuffer, rawBufferTail);
            }
            delete[] rawBuffer;
        }
        rawBuffer = nextBuffer;
        rawBufferSize = sz;
    }

    uLong checkAndAllocateCompressionBuffer() {
        //  buffer must be able to compress the full rawBuffer contents
        assert(rawBufferTail > 0);
        uLong compLen = compressBound(rawBufferTail);
        if (compBufferSize < compLen) {
            delete[] compBuffer;
            compBuffer = new mz_uint8[compLen];
            compBufferSize = compLen;
        }
        return compLen;
    }

    void compressAndWriteToStream() {
        //  compress
        if (rawBufferTail == 0)
            return;
        uLong compSize = checkAndAllocateCompressionBuffer();
        if (compSize == 0) {
            mpack_writer_flag_error(&writer, mpack_error_io);
            return;
        }
        int compResult = mz_compress(compBuffer, &compSize, rawBuffer, rawBufferTail);
        if (compResult != MZ_OK) {
            spdlog::error("ClemensCompressedWriter: error compressing {} bytes, code = {}",
                          rawBufferTail, compResult);
            mpack_writer_flag_error(&writer, mpack_error_io);
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
        compSizeHeader = htole32((unsigned)rawBufferTail);
        memcpy(header + 12, &compSizeHeader, sizeof(compSizeHeader));

        size_t writeCount = fwrite(header, sizeof(header), 1, fpout);
        if (writeCount < 1) {
            spdlog::error("ClemensCompressedWriter: error writing compression header {}",
                          flushCount);
            mpack_writer_flag_error(&writer, mpack_error_io);
            return;
        }

        writeCount = fwrite(compBuffer, 1, compSize, fpout);
        if (writeCount < compSize) {
            spdlog::error("ClemensCompressedWriter: error writing {} buffer bytes", compSize);
            mpack_writer_flag_error(&writer, mpack_error_io);
        }
        spdlog::debug("ClemensCompressedWriter: Chunk {}: original: {}, compressed: {}", flushCount,
                      rawBufferTail, compSize);

        rawBufferTail = 0;
        flushCount++;
    }

    static void teardown(mpack_writer_t *writer) {
        auto *context = reinterpret_cast<ClemensCompressedWriter *>(mpack_writer_context(writer));
        context->compressAndWriteToStream();
        if (context->compBuffer) {
            delete[] context->compBuffer;
            context->compBuffer = nullptr;
            context->compBufferSize = 0;
        }
        context->reallocateRawBuffer(0);
        context->rawBufferTail = 0;
    }

    static void flush(mpack_writer_t *writer, const char *outbuf, size_t count) {
        //  push to a buffer that will be compressed once large enough
        //  our goal is to keep flushed buffers together - not split them
        //  when writing out to file (as compressed data)
        auto *ctx = reinterpret_cast<ClemensCompressedWriter *>(mpack_writer_context(writer));
        while (ctx->rawBufferTail + count > ctx->rawBufferSize) {
            if (ctx->rawBufferTail >= kUncompressedBlockMinSize) {
                ctx->compressAndWriteToStream();
            } else {
                //  no matter what, not enough space so expand the buffer!
                ctx->reallocateRawBuffer(ctx->rawBufferTail + count);
                //  break now in case our reallocation method expands the buffer
                //  more than the requested amount (i.e. future optimization,
                //  so let's be safe and do it here explicitly.)
                break;
            }
        }
        if (count > 0) {
            memcpy(ctx->rawBuffer + ctx->rawBufferTail, outbuf, count);
            ctx->rawBufferTail += count;
            if (ctx->rawBufferTail > kUncompressedBlockSize) {
                ctx->compressAndWriteToStream();
                ctx->rawBufferTail = 0;
            }
        }
    }
};

struct ClemensCompressedReader {
    FILE *fpin = NULL;
    mpack_reader_t reader{};
    char buffer[kUncompressedBlockMinSize];
    mz_uint8 *compBuffer = nullptr;
    uLong compBufferSize = 0;
    mz_stream compStream{};
    unsigned uncompressedExpected = 0;
    unsigned fillCount = 0;
    bool streamValid = false;

    ClemensCompressedReader(FILE *fp) : fpin(fp) {
        mpack_reader_init(&reader, buffer, sizeof(buffer), 0);
        mpack_reader_set_context(&reader, this);
        mpack_reader_set_fill(&reader, fill);
        mpack_reader_set_teardown(&reader, teardown);
        mpack_reader_set_error_handler(&reader, mpackError);
    }
    ~ClemensCompressedReader() { finish(); }

    void finish() { mpack_reader_destroy(&reader); }

    static void teardown(mpack_reader_t *reader) {
        auto *context = reinterpret_cast<ClemensCompressedReader *>(mpack_reader_context(reader));
        if (context->compStream.avail_in > 0) {
            mz_inflateEnd(&context->compStream);
            context->compStream = mz_stream{};
        }
        if (context->compBuffer) {
            delete[] context->compBuffer;
            context->compBuffer = nullptr;
        }
    }

    static void mpackError(mpack_reader_t *reader, mpack_error_t error) {
        auto *context = reinterpret_cast<ClemensCompressedReader *>(mpack_reader_context(reader));
        if (error == mpack_ok)
            return;
        spdlog::error("ClemensCompressedReader: failed unserialization occurred at chunk {}: {}",
                      context->fillCount, mpack_error_to_string(error));
    }

    static size_t fill(mpack_reader_t *reader, char *outbuf, size_t count) {
        //  On every fill, we pull the requested data from our uncompressed
        //  intermediate buffer (readbuffer)
        //  If this buffer is starved, pull a compressed chunk from the input
        //  stream, (making space if needed)
        auto *ctx = reinterpret_cast<ClemensCompressedReader *>(mpack_reader_context(reader));
        int compStatus;

        //  If there's no data available, grab some from the input stream and
        //  decompress into the read buffer first
        if (!ctx->streamValid) {
            uint8_t header[16];
            size_t headerSize = fread(header, 16, 1, ctx->fpin);
            if (headerSize == 0) {
                if (!feof(ctx->fpin)) {
                    spdlog::error("ClemensCompressedReader: no compression header at {}!",
                                  ctx->fillCount);
                    mpack_reader_flag_error(reader, mpack_error_io);
                }
                return 0;
            }
            if (memcmp(header, "CLEMSNAP", 8) != 0) {
                spdlog::error(
                    "ClemensCompressedReader: invalid compression header at {} ({},{},{},{})",
                    ctx->fillCount, header[0], header[1], header[2], header[3]);
                mpack_reader_flag_error(reader, mpack_error_io);
                return 0;
            }
            //  aligned to 32-bit should be OK on all platforms
            const uint32_t *compSizePtr = (const uint32_t *)(header + 8);
            uint32_t compSize = le32toh(*compSizePtr);
            if (compSize > ctx->compBufferSize) {
                delete[] ctx->compBuffer;
                ctx->compBuffer = new mz_uint8[compSize];
                ctx->compBufferSize = compSize;
            }
            ctx->uncompressedExpected = le32toh(*(compSizePtr + 1));
            size_t sizeRead;
            if ((sizeRead = fread(ctx->compBuffer, 1, compSize, ctx->fpin)) != compSize) {
                spdlog::error("ClemensCompressedReader: invalid compression chunk at {}, size {}, "
                              "expected {}",
                              ctx->fillCount, sizeRead, compSize);
                mpack_reader_flag_error(reader, mpack_error_io);
                return 0;
            }

            spdlog::debug("ClemensCompressedReadaer: Chunk {}: original: {}, compressed: {}",
                          ctx->fillCount, ctx->uncompressedExpected, compSize);

            ctx->compStream = mz_stream{};
            ctx->compStream.avail_in = compSize;
            ctx->compStream.next_in = ctx->compBuffer;

            compStatus = mz_inflateInit(&ctx->compStream);
            if (compStatus != MZ_OK) {
                spdlog::error("ClemensCompressedReader: failed to initialize stream for chunk {} "
                              "(result: {})",
                              ctx->fillCount, compStatus);
                mpack_reader_flag_error(reader, mpack_error_io);
                return 0;
            }
            ctx->streamValid = true;
        }
        //  compStream is valid
        unsigned fillAmt = (unsigned)count;
        ctx->compStream.next_out = (unsigned char *)outbuf;
        ctx->compStream.avail_out = fillAmt;
        int flush =
            ((ctx->uncompressedExpected - ctx->compStream.total_out) <= ctx->compStream.avail_out)
                ? MZ_FINISH
                : MZ_NO_FLUSH;
        compStatus = mz_inflate(&ctx->compStream, flush);
        if (compStatus == MZ_STREAM_END) {
            ctx->streamValid = false;
        } else if (compStatus != MZ_OK) {
            spdlog::error("ClemensCompressedReader: stream uncompress failed for chunk {}: "
                          "in:{},out:{} (result: {})",
                          ctx->fillCount, ctx->compStream.avail_in, ctx->compStream.avail_out,
                          compStatus);
            mpack_reader_flag_error(reader, mpack_error_io);
            return 0;
        }
        fillAmt -= ctx->compStream.avail_out;
        if (!ctx->streamValid) {
            unsigned uncompressedActual = ctx->compStream.total_out;
            mz_inflateEnd(&ctx->compStream);
            if (ctx->uncompressedExpected != uncompressedActual) {
                spdlog::error("ClemensCompressedReader: uncompressed sizes do not match for chunk "
                              "{},  (actual: {}, expected: {})",
                              ctx->fillCount, uncompressedActual, ctx->uncompressedExpected);
                mpack_reader_flag_error(reader, mpack_error_io);
                return 0;
            }
            ctx->fillCount++;
        }

        return fillAmt;
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
    ClemensAppleIIGS &gs, const ClemensSnapshotPNG &image,
    std::function<bool(mpack_writer_t *, ClemensAppleIIGS &)> customCb) {
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
    mpack_start_map(&writer, 5);
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
    mpack_write_cstr(&writer, "screen");
    if (image.data != NULL) {
        mpack_write_bin(&writer, (const char *)image.data, (unsigned)image.size);
    } else {
        mpack_write_nil(&writer);
    }
    mpack_finish_map(&writer);
    //  custom
    validation(ValidationStep::Custom);
    if (success) {
        auto customSuccess = customCb(&writer, gs);
        if (!customSuccess) {
            spdlog::error("ClemensSnapshot::serialize() - custom save failed");
            success = false;
        }
    }
    mpack_writer_flush_message(&writer);
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

    mpack_expect_cstr_match(&reader, "screen");
    if (mpack_peek_tag(&reader).type != mpack_type_nil) {
        uint32_t cnt = mpack_expect_bin(&reader);
        result.first.imageData.resize(cnt);
        mpack_read_bytes(&reader, (char *)result.first.imageData.data(), cnt);
        mpack_done_bin(&reader);
    } else {
        mpack_expect_nil(&reader);
    }

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
