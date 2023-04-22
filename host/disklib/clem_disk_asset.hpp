#ifndef CLEM_HOST_DISK_STATE_HPP
#define CLEM_HOST_DISK_STATE_HPP

#include "cinek/buffer.hpp"
#include "clem_2img.h"
#include "clem_disk.h"
#include "clem_woz.h"

#include <string>
#include <utility>
#include <variant>
#include <vector>

//  A container for an *external* disk image.
//  The data
struct ClemensDiskAsset {
    enum ImageType { ImageNone, ImageDSK, ImageProDOS, ImageDOS, Image2IMG, ImageWOZ };
    enum DiskType { DiskNone, Disk525, Disk35, DiskHDD };
    enum ErrorType {
        ErrorNone,
        ErrorInvalidImage,
        ErrorImageNotSupported,
        ErrorVersionNotSupported
    };

    ClemensDiskAsset() = default;
    ClemensDiskAsset(const std::string &assetPath);
    ClemensDiskAsset(const std::string &assetPath, ClemensDriveType driveType);
    ClemensDiskAsset(const std::string &assetPath, ClemensDriveType driveType,
                     cinek::ByteBuffer::ConstRange source, ClemensNibbleDisk &nib);
    operator bool() const { return imageType_ != ImageNone; }
    ErrorType errorType() const { return errorType_; }
    ImageType imageType() const { return imageType_; }
    DiskType diskType() const { return diskType_; }
    const std::string &path() const { return path_; }
    size_t getEstimatedEncodedSize() const { return estimatedEncodedSize_; }

    //  Decodes the nibblized disk into the supplied buffer, in combination with
    //  any image specific data left in the asset's processed data buffer.  The
    //  final output will be serializable in full to a file of the asset's original
    //  asset type.
    std::pair<size_t, bool> decode(uint8_t *out, uint8_t *outEnd, const ClemensNibbleDisk &nib);

  private:
    bool nibblizeDisk(struct Clemens2IMGDisk &disk);
    ImageType imageType_ = ImageNone;
    DiskType diskType_ = DiskNone;
    ErrorType errorType_ = ErrorNone;
    size_t estimatedEncodedSize_ = 0;
    std::string path_;
    std::vector<uint8_t> data_;
    std::variant<ClemensWOZDisk, Clemens2IMGDisk> metadata_;
};

struct ClemensDiskDriveStatus {
    enum class Error { None, MountFailed, SaveFailed };
    std::string assetPath;
    bool isWriteProtected = false;
    bool isSpinning = false;
    bool isEjecting = false;
    bool isSaved = false;
    Error error = Error::None;

    void mount(const std::string &path);
    void saveFailed();
    bool isMounted() const;
};

#endif
