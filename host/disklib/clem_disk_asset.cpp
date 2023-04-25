#include "clem_disk_asset.hpp"
#include "clem_2img.h"
#include "clem_disk.h"
#include "clem_woz.h"

#include <algorithm>
#include <cassert>
#include <filesystem>
#include <variant>

#include "external/mpack.h"

static constexpr unsigned kClemensWOZMaxSupportedVersion = 2;

void ClemensDiskDriveStatus::mount(const std::string &path) {
    assetPath = path;
    isEjecting = false;
    isSpinning = false;
    isWriteProtected = false;
    isSaved = false;
    error = Error::None;
}

void ClemensDiskDriveStatus::saveFailed() {
    error = Error::SaveFailed;
    isSaved = false;
}

void ClemensDiskDriveStatus::mountFailed() {
    error = Error::MountFailed;
    assetPath.clear();
}

void ClemensDiskDriveStatus::saved() {
    error = Error::None;
    isSaved = true;
}

bool ClemensDiskDriveStatus::isMounted() const { return !assetPath.empty(); }

static void clear2IMGBuffers(Clemens2IMGDisk &disk, unsigned creatorDataSize,
                             unsigned commentDataSize) {
    disk.data = NULL;
    disk.data_end = NULL;
    disk.image_buffer = 0;
    disk.creator_data = 0;
    disk.creator_data_end = disk.creator_data + creatorDataSize;
    disk.comment = disk.creator_data_end;
    disk.comment_end = disk.comment + commentDataSize;
    disk.image_buffer_length = 0;
    disk.image_data_offset = 0;
}

ClemensDiskAsset::ClemensDiskAsset(const std::string &assetPath)
    : ClemensDiskAsset(assetPath, kClemensDrive_Invalid) {
    diskType_ = DiskHDD;
}

ClemensDiskAsset::ClemensDiskAsset(const std::string &assetPath, ClemensDriveType driveType)
    : ClemensDiskAsset() {
    path_ = assetPath;
    auto extension = std::filesystem::path(path_).extension().string();
    if (extension == ".po" || extension == ".PO") {
        imageType_ = ImageProDOS;
    } else if (extension == ".do" || extension == ".DO") {
        imageType_ = ImageDOS;
    } else if (extension == ".dsk" || extension == ".DSK") {
        imageType_ = ImageDSK;
    } else if (extension == ".2mg" || extension == ".2MG") {
        imageType_ = Image2IMG;
    } else if (extension == ".woz" || extension == ".WOZ") {
        imageType_ = ImageWOZ;
    }
    if (driveType == kClemensDrive_3_5_D1 || driveType == kClemensDrive_3_5_D2)
        diskType_ = Disk35;
    else if (driveType == kClemensDrive_5_25_D1 || driveType == kClemensDrive_5_25_D2)
        diskType_ = Disk525;
}

ClemensDiskAsset::ClemensDiskAsset(const std::string &assetPath, ClemensDriveType driveType,
                                   cinek::ByteBuffer::ConstRange source, ClemensNibbleDisk &nib)
    : ClemensDiskAsset(assetPath, driveType) {

    estimatedEncodedSize_ = cinek::length(source);

    //  decode pass (i.e. nibbilization)
    const uint8_t *sourceDataPtr = source.first;
    const uint8_t *sourceDataPtrEnd = source.first + estimatedEncodedSize_;
    const uint8_t *sourceDataPtrTail = sourceDataPtr;
    switch (imageType_) {
    case ImageWOZ: {
        struct ClemensWOZDisk disk {};
        int errc = 0;
        disk.nib = &nib;
        sourceDataPtrTail =
            clem_woz_unserialize(&disk, sourceDataPtr, sourceDataPtrEnd - sourceDataPtr,
                                 kClemensWOZMaxSupportedVersion, &errc);
        if (errc == 0) {
            //  we only want to save the metadata as the nibblized version is managed externally
            errorType_ = ErrorNone;
            disk.nib = NULL;
            metadata_ = disk;
        } else if (errc == CLEM_WOZ_UNSUPPORTED_VERSION) {
            errorType_ = ErrorVersionNotSupported;
        } else {
            errorType_ = ErrorInvalidImage;
        }
        break;
    }
    case Image2IMG: {
        struct Clemens2IMGDisk disk {};
        if (clem_2img_parse_header(&disk, sourceDataPtr, sourceDataPtrEnd)) {
            disk.nib = &nib;
            if (nibblizeDisk(disk)) {
                //  Compress the input source so that only creator and comment
                //  data remains.  Also modify the pointers in disk to be offsets
                //  into the compressed vector
                size_t creatorDataSize = disk.creator_data_end - disk.creator_data;
                size_t commentDataSize = disk.comment_end - disk.comment;
                assert(disk.image_buffer == sourceDataPtr);
                if (creatorDataSize + commentDataSize > 0) {
                    data_.reserve(creatorDataSize + commentDataSize);
                    std::copy(disk.creator_data, disk.creator_data_end, std::back_inserter(data_));
                    std::copy(disk.comment, disk.comment_end, std::back_inserter(data_));
                    clear2IMGBuffers(disk, creatorDataSize, commentDataSize);
                }
                sourceDataPtrTail = sourceDataPtrEnd;
                metadata_ = disk;
            } else {
                errorType_ = ErrorInvalidImage;
            }
        } else {
            errorType_ = ErrorInvalidImage;
        }
        break;
    }
    case ImageProDOS: {
        struct Clemens2IMGDisk disk {};
        if (clem_2img_generate_header(&disk, CLEM_DISK_FORMAT_PRODOS, sourceDataPtr,
                                      sourceDataPtrEnd, 0)) {
            disk.nib = &nib;
            if (nibblizeDisk(disk)) {
                sourceDataPtrTail = sourceDataPtrEnd;
                clear2IMGBuffers(disk, 0, 0);
                metadata_ = disk;
            } else {
                errorType_ = ErrorInvalidImage;
            }
        } else {
            errorType_ = ErrorInvalidImage;
        }
        break;
    }
    case ImageDOS:
    case ImageDSK: {
        struct Clemens2IMGDisk disk {};
        if (clem_2img_generate_header(&disk, CLEM_DISK_FORMAT_DOS, sourceDataPtr, sourceDataPtrEnd,
                                      0)) {
            disk.nib = &nib;
            if (nibblizeDisk(disk)) {
                sourceDataPtrTail = sourceDataPtrEnd;
                clear2IMGBuffers(disk, 0, 0);
                metadata_ = disk;
            } else {
                errorType_ = ErrorInvalidImage;
            }
            break;
        } else {
            errorType_ = ErrorInvalidImage;
        }
        break;
    }
    case ImageNone:
        errorType_ = ErrorImageNotSupported;
        break;
    }

    if (errorType_ == ErrorNone) {
        if (sourceDataPtrEnd - sourceDataPtrTail > 0) {
            //  save off the unprocessed data so it can be reencoded when saved out
            //  with the processed data.
            data_.reserve(sourceDataPtrEnd - sourceDataPtrTail);
            std::copy(sourceDataPtrTail, sourceDataPtrEnd, std::back_inserter(data_));
        }
    }
}

bool ClemensDiskAsset::nibblizeDisk(struct Clemens2IMGDisk &disk) {
    unsigned diskType = diskType_ == Disk35    ? CLEM_DISK_TYPE_3_5
                        : diskType_ == Disk525 ? CLEM_DISK_TYPE_5_25
                                               : CLEM_DISK_TYPE_NONE;
    size_t bits_size = clem_disk_calculate_nib_storage_size(diskType);
    uint8_t *original_bits_data_end = disk.nib->bits_data_end;
    if (bits_size == 0)
        return false;
    assert(disk.nib->bits_data != NULL);
    assert(disk.nib->bits_data_end != NULL);
    assert(disk.nib->bits_data_end > disk.nib->bits_data);
    assert(disk.nib->disk_type == diskType);
    if (bits_size > size_t(disk.nib->bits_data_end - disk.nib->bits_data))
        return false;
    if (disk.nib->disk_type != diskType)
        return false;
    disk.nib->bits_data_end = disk.nib->bits_data + bits_size;
    if (!clem_2img_nibblize_data(&disk)) {
        disk.nib->bits_data_end = original_bits_data_end;
        return false;
    }

    return true;
}

std::pair<size_t, bool> ClemensDiskAsset::decode(uint8_t *out, uint8_t *outEnd,
                                                 const ClemensNibbleDisk &nib) {
    // convert nibblized disk into the asset's image type and output the results
    // onto the out/outEnd buffer
    std::pair<size_t, bool> result{0, false};
    uint8_t *outStart = out;

    //  WOZ images are the easiest to reconstruct - EXCEPT WRIT and FLUX
    //  Our recommendation is to preserve any WOZ files so that you have a fixed
    //  original copy.   If you're going to modify WOZ files, ensure that the
    //  WOZ used doesn't need to confirm 100% to the WOZ 2 spec with all
    //  sections available.
    //
    //  TODO: FLUX is not supported at the moment (to support this, we'll have
    //        to regenerate flux bits on demand)
    //        WRIT would be regenerated from the current track data and clem_woz
    //        would have to implement this
    if (errorType_ != ErrorNone) {
        return result;
    }
    if (metadata_.valueless_by_exception()) {
        return result;
    }
    switch (imageType_) {
    case ImageWOZ:
        if (std::holds_alternative<ClemensWOZDisk>(metadata_)) {
            auto disk = std::get<ClemensWOZDisk>(metadata_);
            disk.nib = const_cast<ClemensNibbleDisk *>(&nib);
            size_t outSize = outEnd - out;
            out = clem_woz_serialize(&disk, out, &outSize);
        }
        break;
    case Image2IMG:
        if (std::holds_alternative<Clemens2IMGDisk>(metadata_)) {
            auto disk = std::get<Clemens2IMGDisk>(metadata_);
            //  write encoded data first
            disk.creator_data = (const char *)data_.data() + (size_t)disk.creator_data;
            disk.creator_data_end = (const char *)data_.data() + (size_t)disk.creator_data_end;
            disk.comment = (const char *)data_.data() + (size_t)disk.comment;
            disk.comment_end = (const char *)data_.data() + (size_t)disk.comment_end;
            //  the encodedBuffer here is guaranteed to be larger than what's actually needed
            std::vector<uint8_t> encodedBuffer(nib.bits_data_end - nib.bits_data);
            if (clem_2img_decode_nibblized_disk(&disk, encodedBuffer.data(),
                                                encodedBuffer.data() + encodedBuffer.size(),
                                                &nib)) {
                clem_2img_build_image(&disk, out, outEnd);
            } else {
                out = nullptr;
            }
        }
        break;
    case ImageProDOS:
        if (std::holds_alternative<Clemens2IMGDisk>(metadata_)) {
            auto disk = std::get<Clemens2IMGDisk>(metadata_);
#pragma message("TODO: ProDOS save")
        }
        break;
    case ImageDOS:
    case ImageDSK:
        if (std::holds_alternative<Clemens2IMGDisk>(metadata_)) {
            auto disk = std::get<Clemens2IMGDisk>(metadata_);
#pragma message("TODO: DOS save")
        }
        break;
    case ImageNone:
        break;
    }

    if (out) {
        if ((size_t)(outEnd - out) >= data_.size()) {
            out = std::copy(data_.begin(), data_.end(), out);
        } else {
            //  at this point, some data will be lost - but not essential data
            //  and so allow this image to be serialized - but should we warn?
            assert(false);
        }
        result.first = (size_t)(out - outStart);
        result.second = true;
    }

    return result;
}

static const char *kImageTypeNames[] = {"None", "DSK", "ProDOS", "DOS", "2IMG", "WOZ", NULL};
static const char *kDiskTypeNames[] = {"None", "525", "35", "HDD", NULL};
static const char *kErrorTypeNames[] = {"None", "Invalid", "ImageNotSupported",
                                        "VersionNotSupported", NULL};

bool ClemensDiskAsset::serialize(mpack_writer_t *writer) {
    mpack_build_map(writer);

    mpack_write_cstr(writer, "image_type");
    mpack_write_cstr(writer, kImageTypeNames[static_cast<int>(imageType_)]);
    mpack_write_cstr(writer, "disk_type");
    mpack_write_cstr(writer, kDiskTypeNames[static_cast<int>(diskType_)]);
    mpack_write_cstr(writer, "error_type");
    mpack_write_cstr(writer, kErrorTypeNames[static_cast<int>(errorType_)]);
    mpack_write_cstr(writer, "estimated_encoded_size");
    mpack_write_u32(writer, (unsigned)estimatedEncodedSize_);
    mpack_write_cstr(writer, "path");
    mpack_write_cstr(writer, path_.c_str());
    mpack_write_cstr(writer, "data");
    mpack_write_bytes(writer, (const char *)data_.data(), (unsigned)data_.size());
    mpack_write_cstr(writer, "metadata");
    if (std::holds_alternative<ClemensWOZDisk>(metadata_)) {
        auto &woz = std::get<ClemensWOZDisk>(metadata_);
        mpack_write_cstr(writer, "type");
        mpack_write_cstr(writer, "woz");
        mpack_write_cstr(writer, "woz.version");
        mpack_write_u32(writer, woz.version);
        mpack_write_cstr(writer, "woz.disk_type");
        mpack_write_u32(writer, woz.disk_type);
        mpack_write_cstr(writer, "woz.boot_type");
        mpack_write_u32(writer, woz.boot_type);
        mpack_write_cstr(writer, "woz.flags");
        mpack_write_u32(writer, woz.flags);
        mpack_write_cstr(writer, "woz.required_ram_kb");
        mpack_write_u32(writer, woz.required_ram_kb);
        mpack_write_cstr(writer, "woz.max_track_size_bytes");
        mpack_write_u32(writer, woz.max_track_size_bytes);
        mpack_write_cstr(writer, "woz.bit_timing_ns");
        mpack_write_u32(writer, woz.bit_timing_ns);
        mpack_write_cstr(writer, "woz.flux_block");
        mpack_write_u16(writer, woz.flux_block);
        mpack_write_cstr(writer, "woz.largest_flux_track");
        mpack_write_u16(writer, woz.largest_flux_track);
        mpack_write_cstr(writer, "woz.creator");
        mpack_write_bin(writer, woz.creator, sizeof(woz.creator));
    } else if (std::holds_alternative<Clemens2IMGDisk>(metadata_)) {
        auto &disk = std::get<Clemens2IMGDisk>(metadata_);
        mpack_write_cstr(writer, "type");
        mpack_write_cstr(writer, "2img");
        mpack_write_cstr(writer, "creator");
        mpack_write_bin(writer, disk.creator, sizeof(disk.creator));
        mpack_write_cstr(writer, "version");
        mpack_write_u16(writer, disk.version);
        mpack_write_cstr(writer, "format");
        mpack_write_uint(writer, disk.format);
        mpack_write_cstr(writer, "dos_volume");
        mpack_write_uint(writer, disk.dos_volume);
        mpack_write_cstr(writer, "block_count");
        mpack_write_uint(writer, disk.block_count);
        mpack_write_cstr(writer, "creator_data_end");
        mpack_write_u64(writer, (uintptr_t)disk.creator_data_end);
        mpack_write_cstr(writer, "comment_end");
        mpack_write_u64(writer, (uintptr_t)disk.comment_end);
        mpack_write_cstr(writer, "is_write_protected");
        mpack_write_bool(writer, disk.is_write_protected);
    } else {
        mpack_write_cstr(writer, "type");
        mpack_write_cstr(writer, "none");
    }

    mpack_complete_map(writer);
    return true;
}

bool ClemensDiskAsset::unserialize(mpack_reader_t *reader) {
    char tmp[1024];

    mpack_expect_map(reader);

    mpack_expect_cstr_match(reader, "image_type");
    mpack_expect_cstr(reader, tmp, sizeof(tmp));
    imageType_ = ImageInvalid;
    for (unsigned i = 0; kImageTypeNames[i] != NULL; ++i) {
        if (strncmp(tmp, kImageTypeNames[i], sizeof(tmp)) == 0) {
            imageType_ = static_cast<ImageType>(i);
            break;
        }
    }
    if (imageType_ == ImageInvalid) {
        mpack_done_map(reader);
        return false;
    }
    mpack_expect_cstr_match(reader, "disk_type");
    mpack_expect_cstr(reader, tmp, sizeof(tmp));
    diskType_ = DiskInvalid;
    for (unsigned i = 0; kDiskTypeNames[i] != NULL; ++i) {
        if (strncmp(tmp, kDiskTypeNames[i], sizeof(tmp)) == 0) {
            diskType_ = static_cast<DiskType>(i);
            break;
        }
    }
    if (diskType_ == DiskInvalid) {
        mpack_done_map(reader);
        return false;
    }
    mpack_expect_cstr_match(reader, "error_type");
    mpack_expect_cstr(reader, tmp, sizeof(tmp));
    errorType_ = ErrorInvalid;
    for (unsigned i = 0; kErrorTypeNames[i] != NULL; ++i) {
        if (strncmp(tmp, kErrorTypeNames[i], sizeof(tmp)) == 0) {
            errorType_ = static_cast<ErrorType>(i);
            break;
        }
    }
    if (errorType_ == ErrorInvalid) {
        mpack_done_map(reader);
        return false;
    }

    mpack_expect_cstr_match(reader, "estimated_encoded_size");
    estimatedEncodedSize_ = mpack_expect_u32(reader);
    mpack_expect_cstr_match(reader, "path");
    mpack_expect_cstr(reader, tmp, sizeof(tmp));
    path_ = tmp;
    mpack_expect_cstr_match(reader, "data");
    unsigned sz = mpack_expect_bin(reader);
    data_.clear();
    data_.resize(sz);
    mpack_read_bytes(reader, (char *)data_.data(), sz);
    mpack_expect_cstr_match(reader, "metadata");
    mpack_read_bytes(reader, (char *)data_.data(), sz);
    mpack_expect_cstr_match(reader, "type");
    mpack_expect_cstr(reader, tmp, 16);
    if (strncmp(tmp, "woz", 16) == 0) {
        ClemensWOZDisk disk{};
        mpack_expect_cstr_match(reader, "woz.version");
        disk.version = mpack_expect_u32(reader);
        mpack_expect_cstr_match(reader, "woz.disk_type");
        disk.disk_type = mpack_expect_u32(reader);
        mpack_expect_cstr_match(reader, "woz.boot_type");
        disk.boot_type = mpack_expect_u32(reader);
        mpack_expect_cstr_match(reader, "woz.flags");
        disk.flags = mpack_expect_u32(reader);
        mpack_expect_cstr_match(reader, "woz.required_ram_kb");
        disk.required_ram_kb = mpack_expect_u32(reader);
        mpack_expect_cstr_match(reader, "woz.max_track_size_bytes");
        disk.max_track_size_bytes = mpack_expect_u32(reader);
        mpack_expect_cstr_match(reader, "woz.bit_timing_ns");
        disk.bit_timing_ns = mpack_expect_u32(reader);
        mpack_expect_cstr_match(reader, "woz.flux_block");
        disk.flux_block = mpack_expect_u16(reader);
        mpack_expect_cstr_match(reader, "woz.largest_flux_track");
        disk.largest_flux_track = mpack_expect_u16(reader);
        mpack_expect_cstr_match(reader, "woz.creator");
        mpack_expect_bin_buf(reader, disk.creator, sizeof(disk.creator));
        metadata_ = disk;
    } else if (strncmp(tmp, "2img", 16) == 0) {
        Clemens2IMGDisk disk{};
        mpack_expect_cstr_match(reader, "creator");
        mpack_expect_bin_buf(reader, disk.creator, sizeof(disk.creator));
        mpack_expect_cstr_match(reader, "version");
        disk.version = mpack_expect_u16(reader);
        mpack_expect_cstr_match(reader, "format");
        disk.format = mpack_expect_uint(reader);
        mpack_expect_cstr_match(reader, "dos_volume");
        disk.dos_volume = mpack_expect_uint(reader);
        mpack_expect_cstr_match(reader, "block_count");
        disk.block_count = mpack_expect_uint(reader);
        mpack_expect_cstr_match(reader, "creator_data_end");
        disk.creator_data_end = (const char *)mpack_expect_u64(reader);
        mpack_expect_cstr_match(reader, "comment_end");
        disk.comment_end = (const char *)mpack_expect_u64(reader);
        mpack_expect_cstr_match(reader, "is_write_protected");
        disk.is_write_protected = mpack_expect_bool(reader);
        metadata_ = disk;
    } else if (strncmp(tmp, "none", 16) != 0) {
        mpack_done_map(reader);
        return false;
    }

    mpack_done_map(reader);
    return true;
}
