#include "clem_disk_asset.hpp"
#include "clem_2img.h"
#include "clem_disk.h"
#include "clem_woz.h"

#include <algorithm>
#include <cassert>
#include <filesystem>
#include <variant>

static constexpr unsigned kClemensWOZMaxSupportedVersion = 2;

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
                                   std::vector<uint8_t> source, ClemensNibbleDisk &nib)
    : ClemensDiskAsset(assetPath, driveType) {

    estimatedEncodedSize_ = source.size();

    //  decode pass (i.e. nibbilization)
    const uint8_t *sourceDataPtr = source.data();
    const uint8_t *sourceDataPtrEnd = source.data() + source.size();
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
                assert(disk.image_buffer == source.data());
                if (creatorDataSize + commentDataSize > 0) {
                    data_.reserve(creatorDataSize + commentDataSize);
                    std::copy(disk.creator_data, disk.creator_data_end, std::back_inserter(data_));
                    std::copy(disk.comment, disk.comment_end, std::back_inserter(data_));
                    disk.data = NULL;
                    disk.data_end = NULL;
                    disk.image_buffer = 0;
                    disk.creator_data = 0;
                    disk.creator_data_end = disk.creator_data + creatorDataSize;
                    disk.comment = disk.creator_data_end;
                    disk.comment_end = disk.comment + commentDataSize;
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

std::pair<size_t, bool> ClemensDiskAsset::encode(uint8_t *out, uint8_t *outEnd,
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
        }
        break;
    case ImageDOS:
    case ImageDSK:
        if (std::holds_alternative<Clemens2IMGDisk>(metadata_)) {
            auto disk = std::get<Clemens2IMGDisk>(metadata_);
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
            //  and so allow this image to be serialized - but should be warn?
            assert(false);
        }
        result.first = (size_t)(out - outStart);
        result.second = true;
    }

    return result;
}
