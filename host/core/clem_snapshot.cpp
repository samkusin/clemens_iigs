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

struct ClemensSnapshotWriter {
    FILE *fpout;
    char *buffer = nullptr;
    size_t bufferSize = 0;
    mz_uint8 *compBuffer = nullptr;
    uLong compBufferSize = 0;
    bool compressionEnabled = false;

    ClemensSnapshotWriter(FILE *fp) : fpout(fp) {
        buffer = new char[65536];
        bufferSize = 65536;
    }
    ~ClemensSnapshotWriter() {
        delete[] buffer;
        if (compBuffer) {
            delete[] compBuffer;
        }
    }
    void enableCompression(bool enable, mpack_writer_t* writer) {
        mpack_writer_flush_message(writer);
        compressionEnabled = enable;
    }
    static void flush(mpack_writer_t *writer, const char *outbuf, size_t count) {
        auto *context = reinterpret_cast<ClemensSnapshotWriter *>(mpack_writer_context(writer));
        size_t result = fwrite(outbuf, 1, count, context->fpout);
        if (result < count) {
            mpack_writer_flag_error(writer, mpack_error_io);
        }
    }
};

struct ClemensSnapshotReader {
    FILE *fpin;
    char *buffer = nullptr;
    size_t bufferSize = 0;

    ClemensSnapshotReader(FILE *fp) : fpin(fp) {
        buffer = new char[65536];
        bufferSize = 65536;
    }
    ~ClemensSnapshotReader() {
        delete[] buffer;
    }
    void expectCompression(bool enable, mpack_reader_t* writer) {
        mpack_writer_flush_message(writer);
        compressionEnabled = enable;
    }
    static size_t fill(mpack_reader_t *reader, char *outbuf, size_t count) {
        auto *context = reinterpret_cast<ClemensSnapshotReader *>(mpack_reader_context(reader));
        if (feof(context->fpin)) {
            mpack_reader_flag_error(reader, mpack_error_eof);
            return 0;
        }
        return fread(outbuf, 1, count, context->fpin);
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

    //  begin mpack
    ClemensSnapshotWriter writerStream(fp);
    mpack_writer_t writer{};
    mpack_writer_init(&writer, writerStream.buffer, writerStream.bufferSize);
    mpack_writer_set_context(&writer, &writerStream);
    mpack_writer_set_flush(&writer, ClemensSnapshotWriter::flush);
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

    //  machine
    validation(ValidationStep::Machine);
    if (success) {
        auto machineSaveResult = gs.save(&writer);
        if (!machineSaveResult.second) {
            spdlog::error("ClemensSnapshot::serialize() - machine save failed @ '{}'",
                          machineSaveResult.first);
            success = false;
        }
    }

    mpack_writer_destroy(&writer);

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

    ClemensSnapshotReader streamReader(fp);
    mpack_reader_init(&reader, streamReader.buffer, streamReader.bufferSize, 0);
    mpack_reader_set_context(&reader, &streamReader);
    mpack_reader_set_fill(&reader, ClemensSnapshotReader::fill);
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
    if (success) {
        validation(ValidationStep::Machine);
        gs = std::make_unique<ClemensAppleIIGS>(&reader, systemListener);
        if (!gs->isOk()) {
            success = false;
        }
    }
    mpack_reader_destroy(&reader);

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
    ClemensSnapshotReader streamReader(fp);
    mpack_reader_init(&reader, streamReader.buffer, streamReader.bufferSize, 0);
    mpack_reader_set_context(&reader, &streamReader);
    mpack_reader_set_fill(&reader, ClemensSnapshotReader::fill);
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
