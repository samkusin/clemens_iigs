#include "clem_disk_utils.hpp"
#include "clem_2img.h"
#include "clem_disk.h"
#include "clem_disk_asset.hpp"

#include <algorithm>
#include <array>
#include <cassert>
#include <cstring>
#include <filesystem>
#include <fstream>

namespace ClemensDiskUtilities {

static std::array<std::string_view, 4> sDriveNames = {"s5d1", "s5d2", "s6d1", "s6d2"};

std::string_view getDriveName(ClemensDriveType driveType) {
    if (driveType == kClemensDrive_Invalid)
        return "invalid";
    return sDriveNames[driveType];
}

ClemensDriveType getDriveType(std::string_view driveName) {
    auto driveIt = std::find(sDriveNames.begin(), sDriveNames.end(), driveName);
    if (driveIt == sDriveNames.end())
        return kClemensDrive_Invalid;
    unsigned driveIndex = unsigned(driveIt - sDriveNames.begin());
    return static_cast<ClemensDriveType>(driveIndex);
}

bool createDisk(cinek::Range<uint8_t> imageBuffer, const std::string &path,
                ClemensDriveType driveType) {
    auto imageGeneratedSource = ClemensDiskAsset::createBlankDiskImage(
        ClemensDiskAsset::fromAssetPathUsingExtension(path),
        ClemensDiskAsset::diskTypeFromDriveType(driveType), true, imageBuffer);
    if (cinek::length(imageGeneratedSource) != 0) {
        std::ofstream out(path, std::ios_base::out | std::ios_base::binary);
        if (!out.fail()) {
            out.write((char *)imageGeneratedSource.first, cinek::length(imageGeneratedSource));
            if (!out.fail() && !out.bad()) {
                return true;
            }
        }
    }
    return false;
}

unsigned createProDOSHardDisk(const std::string &path, unsigned blockCount) {
    //  Since 2IMG and PO disks are supported, we only need to generate the header
    //  for 2IMG images and then write empty blocks directly out to file
    //  for PO images, we skip the header of course.
    uint8_t header[CLEM_2IMG_HEADER_BYTE_SIZE];
    char block[512];
    struct Clemens2IMGDisk disk;
    unsigned blocks_written = 0;

    auto imageType = ClemensDiskAsset::fromAssetPathUsingExtension(path);
    if (imageType == ClemensDiskAsset::Image2IMG) {

        clem_2img_generate_header(&disk, CLEM_DISK_FORMAT_PRODOS, header,
                                  header + CLEM_2IMG_HEADER_BYTE_SIZE, CLEM_2IMG_HEADER_BYTE_SIZE,
                                  blockCount * 512);
        //  this will build only the header
        disk.data = NULL;
        disk.data_end = disk.data + blockCount * 512;
        if (!clem_2img_build_image(&disk, header, header + CLEM_2IMG_HEADER_BYTE_SIZE))
            return 0;
    } else if (imageType != ClemensDiskAsset::ImageProDOS && imageType != ClemensDiskAsset::ImageHDV) {
        return 0;
    }

    std::ofstream out(path, std::ios_base::out | std::ios_base::binary);
    if (out.fail())
        return blocks_written;

    memset(block, 0, sizeof(block));
    if (imageType == ClemensDiskAsset::Image2IMG) {
        out.write((char *)header, CLEM_2IMG_HEADER_BYTE_SIZE);
    }
    bool ok = true;
    while ((ok = (!out.fail() && !out.bad()))) {
        out.write(block, sizeof(block));
        blocks_written++;
        if (blocks_written >= blockCount)
            break;
    }

    return blocks_written;
}

} // namespace ClemensDiskUtilities
