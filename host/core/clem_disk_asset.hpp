#ifndef CLEM_HOST_DISK_STATE_HPP
#define CLEM_HOST_DISK_STATE_HPP

#include "cinek/buffertypes.hpp"
#include "clem_2img.h"
#include "clem_disk.h"
#include "clem_woz.h"

#include <string>
#include <string_view>
#include <utility>
#include <variant>
#include <vector>

//  forward declarations
typedef struct mpack_reader_t mpack_reader_t;
typedef struct mpack_writer_t mpack_writer_t;

//  A container for an *external* disk image.
//  The data
struct ClemensDiskAsset {
    enum ImageType { ImageNone, ImageDSK, ImageProDOS, ImageDOS, Image2IMG, ImageWOZ, ImageHDV };
    enum DiskType { DiskNone, Disk525, Disk35, DiskHDD };
    enum ErrorType {
        ErrorNone,
        ErrorInvalidImage,
        ErrorImageNotSupported,
        ErrorVersionNotSupported
    };

    static DiskType diskTypeFromDriveType(ClemensDriveType driveType);
    static ClemensDriveType driveTypefromDiskType(DiskType diskType, unsigned driveIndex);
    static const char *imageName(ImageType imageType);
    static ImageType fromAssetPathUsingExtension(const std::string &assetPath);

    //  This utility can be used to generate a disk image with the format's relevant
    //  metadata so it can be unserialized by ClemensDiskAsset.
    //  @assetPath will contain
    //  @doubleSided is relevant for 3.5 disks and is ignored for other types
    //  @buffer should represent enough space to contain the generated disk image
    static cinek::ConstRange<uint8_t> createBlankDiskImage(ImageType imageType, DiskType diskType,
                                                           bool doubleSided,
                                                           cinek::Range<uint8_t> buffer);

    ClemensDiskAsset() = default;
    //  Produces a metadata only Hard Drive asset
    ClemensDiskAsset(const std::string &assetPath);
    //  Produces a metadata only Floppy Disk asset
    ClemensDiskAsset(const std::string &assetPath, ClemensDriveType driveType);
    //  Generates a disk image using an image type derived from the given input path (i.e. .dsk,
    //  .2mg, etc) The input source buffer should contain *decoded* disk informatted formatted into
    //  sectors.
    //  Outputs an encoded nibbilized image from the given input
    ClemensDiskAsset(const std::string &assetPath, ClemensDriveType driveType,
                     cinek::ConstRange<uint8_t> source, ClemensNibbleDisk &nib);
    operator bool() const { return imageType_ != ImageNone; }
    ErrorType errorType() const { return errorType_; }
    ImageType imageType() const { return imageType_; }
    DiskType diskType() const { return diskType_; }
    const std::string &path() const { return path_; }
    size_t getEstimatedEncodedSize() const { return estimatedEncodedSize_; }
    void relocatePath(const std::string& assetLocation);

    //  Decodes the nibblized disk into the supplied buffer, in combination with
    //  any image specific data left in the asset's processed data buffer.  The
    //  final output will be serializable in full to a file of the asset's original
    //  asset type.
    std::pair<size_t, bool> decode(uint8_t *out, uint8_t *outEnd, const ClemensNibbleDisk &nib);

    bool serialize(mpack_writer_t *writer);
    bool unserialize(mpack_reader_t *reader);

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

#endif
