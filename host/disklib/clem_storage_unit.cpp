#include "clem_storage_unit.hpp"
#include "clem_disk_utils.hpp"

#include "cinek/buffer.hpp"
#include "clem_disk.h"
#include "clem_mmio_types.h"
#include "clem_smartport.h"
#include "disklib/clem_disk_asset.hpp"
#include "disklib/clem_prodos_disk.hpp"
#include "emulator_mmio.h"
#include "external/mpack.h"

#include "fmt/format.h"

#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <vector>

namespace {

constexpr unsigned kDecodingBufferSize = 4 * 1024 * 1024;
constexpr unsigned kSmartPortDiskSize = 32 * 1024 * 1024;

unsigned calculateSlabHeapSize() {

    return CLEM_DISK_525_MAX_DATA_SIZE * 2 + CLEM_DISK_35_MAX_DATA_SIZE * 2 + kSmartPortDiskSize +
           kDecodingBufferSize + 4096;
}
} // namespace

ClemensStorageUnit::ClemensStorageUnit()
    : slab_(calculateSlabHeapSize(), malloc(calculateSlabHeapSize())) {

    allocateBuffers();
}

ClemensStorageUnit::~ClemensStorageUnit() { free(slab_.getHead()); }

void ClemensStorageUnit::allocateBuffers() {
    slab_.reset();

    // buffers for the ClemensNibbleDisks to insert/eject
    uint8_t *buffer;
    buffer = slab_.allocateArray<uint8_t>(CLEM_DISK_35_MAX_DATA_SIZE);
    nibbleBuffers_[kClemensDrive_3_5_D1].first = buffer;
    nibbleBuffers_[kClemensDrive_3_5_D1].second = buffer + CLEM_DISK_35_MAX_DATA_SIZE;

    buffer = slab_.allocateArray<uint8_t>(CLEM_DISK_35_MAX_DATA_SIZE);
    nibbleBuffers_[kClemensDrive_3_5_D2].first = buffer;
    nibbleBuffers_[kClemensDrive_3_5_D2].second = buffer + CLEM_DISK_35_MAX_DATA_SIZE;

    buffer = slab_.allocateArray<uint8_t>(CLEM_DISK_525_MAX_DATA_SIZE);
    nibbleBuffers_[kClemensDrive_5_25_D1].first = buffer;
    nibbleBuffers_[kClemensDrive_5_25_D1].second = buffer + CLEM_DISK_525_MAX_DATA_SIZE;

    buffer = slab_.allocateArray<uint8_t>(CLEM_DISK_525_MAX_DATA_SIZE);
    nibbleBuffers_[kClemensDrive_5_25_D2].first = buffer;
    nibbleBuffers_[kClemensDrive_5_25_D2].second = buffer + CLEM_DISK_525_MAX_DATA_SIZE;

    // create empty backing ProDOS buffer for SmartPort disk
    hardDisks_[0] = ClemensProDOSDisk(cinek::ByteBuffer(
        slab_.allocateArray<uint8_t>(kSmartPortDiskSize + 128), kSmartPortDiskSize));

    // create empty decode scratchpad for saving images to the host's filesystem
    decodeBuffer_ =
        cinek::ByteBuffer(slab_.allocateArray<uint8_t>(kDecodingBufferSize), kDecodingBufferSize);

    diskStatuses_.fill(ClemensDiskDriveStatus{});
    hardDiskStatuses_.fill(ClemensDiskDriveStatus{});
}

bool ClemensStorageUnit::assignSmartPortDisk(ClemensMMIO &mmio, unsigned driveIndex,
                                             const std::string &imagePath) {
    ClemensSmartPortUnit *unit = clemens_smartport_unit_get(&mmio, driveIndex);
    if (!unit) {
        fmt::print(
            stderr,
            "ClemensStorageUnit::assignSmartPortDisk - Invalid smartport unit {} specified\n",
            driveIndex);
        return false;
    }
    if (hardDiskStatuses_[driveIndex].isMounted()) {
        fmt::print(stderr,
                   "ClemensStorageUnit::assignSmartPortDisk - Drive unit {} is already mounted\n",
                   driveIndex);
        return false;
    }

    auto asset = ClemensDiskAsset(imagePath);
    if (asset.imageType() != ClemensDiskAsset::Image2IMG &&
        asset.imageType() != ClemensDiskAsset::ImageWOZ) {
        hardDiskStatuses_[driveIndex].mountFailed();
        return false;
    }

    struct ClemensSmartPortDevice device {};
    hardDiskAssets_[driveIndex] = asset;
    if (!hardDisks_[0].bind(device, hardDiskAssets_[driveIndex])) {
        fmt::print(stderr, "ClemensStorageUnit::assignSmartPortDisk - Bind failed for disk {}:{}\n",
                   driveIndex, imagePath);
        hardDiskStatuses_[driveIndex].mountFailed();
        return false;
    }
    hardDiskStatuses_[driveIndex].mount(imagePath);
    return clemens_assign_smartport_disk(&mmio, driveIndex, &device);
}

bool ClemensStorageUnit::insertDisk(ClemensMMIO &mmio, ClemensDriveType driveType,
                                    const std::string &path) {
    ClemensDrive *drive = clemens_drive_get(&mmio, driveType);
    if (!drive)
        return false;
    if (diskStatuses_[driveType].isMounted()) {
        fmt::print(stderr, "ClemensStorageUnit::insertDisk - Drive type {} is already mounted\n",
                   ClemensDiskUtilities::getDriveName(driveType));
        return false;
    }

    std::ifstream input(path, std::ios_base::in | std::ios_base::binary);
    if (!input.is_open()) {
        diskStatuses_[driveType].mountFailed();
        return false;
    }
    size_t inputImageSize = input.seekg(0, std::ios_base::end).tellg();
    if (inputImageSize > decodeBuffer_.getCapacity()) {
        assert(false);
        fmt::print(stderr,
                   "ClemensStorageUnit::insertDisk - image {}:{} size is greater than disk "
                   "buffer size {}\n",
                   ClemensDiskUtilities::getDriveName(driveType), path,
                   decodeBuffer_.getCapacity());
        diskStatuses_[driveType].mountFailed();
        return false;
    }

    decodeBuffer_.reset();

    auto bits = decodeBuffer_.forwardSize(inputImageSize);
    input.seekg(0);
    input.read((char *)bits.first, inputImageSize);
    if (!input.good()) {
        fmt::print(stderr,
                   "ClemensStorageUnit::insertDisk - image {}:{} size is greater than disk "
                   "buffer size {}\n",
                   ClemensDiskUtilities::getDriveName(driveType), path,
                   decodeBuffer_.getCapacity());
        diskStatuses_[driveType].mountFailed();
        return false;
    }
    input.close();

    struct ClemensNibbleDisk disk {};

    switch (driveType) {
    case kClemensDrive_3_5_D1:
    case kClemensDrive_3_5_D2:
        clem_nib_init_disk(&disk, CLEM_DISK_TYPE_3_5, nibbleBuffers_[driveType].first,
                           nibbleBuffers_[driveType].second);
        break;
    case kClemensDrive_5_25_D1:
    case kClemensDrive_5_25_D2:
        clem_nib_init_disk(&disk, CLEM_DISK_TYPE_5_25, nibbleBuffers_[driveType].first,
                           nibbleBuffers_[driveType].second);
        break;
    default:
        return false;
    }

    diskAssets_[driveType] = ClemensDiskAsset(path, driveType, decodeBuffer_.getRange(), disk);
    diskStatuses_[driveType].mount(path);
    return clemens_assign_disk(&mmio, driveType, &disk);
}

bool ClemensStorageUnit::ejectDisk(ClemensMMIO &mmio, ClemensDriveType driveType) {
    if (!clemens_drive_get(&mmio, driveType)->has_disk || !diskStatuses_[driveType].isMounted())
        return false;
    if (diskStatuses_[driveType].isEjecting)
        return true;

    struct ClemensNibbleDisk disk {};
    clemens_eject_disk_async(&mmio, driveType, &disk);
    //  save immediately - this can be done again when fully ejected
    saveDisk(driveType, disk);
    return true;
}

void ClemensStorageUnit::update(ClemensMMIO &mmio) {
    for (auto it = diskStatuses_.begin(); it != diskStatuses_.end(); ++it) {
        auto driveType = static_cast<ClemensDriveType>(it - diskStatuses_.begin());
        auto *drive = clemens_drive_get(&mmio, driveType);
        auto &status = diskStatuses_[driveType];
        if (!diskStatuses_[driveType].isMounted()) {
            continue;
        }
    }
}

const ClemensDiskDriveStatus &ClemensStorageUnit::getDriveStatus(ClemensDriveType driveType) const {
    return diskStatuses_[driveType];
}

const ClemensDiskDriveStatus &ClemensStorageUnit::getSmartPortStatus(unsigned driveIndex) const {
    return hardDiskStatuses_[driveIndex];
}

void ClemensStorageUnit::saveDisk(ClemensDriveType driveType, ClemensNibbleDisk &disk) {
    if (!diskStatuses_[driveType].isMounted())
        return;

    decodeBuffer_.reset();
    auto writeOut = decodeBuffer_.forwardSize(decodeBuffer_.getCapacity());
    auto decodedDisk = diskAssets_[driveType].decode(writeOut.first, writeOut.second, disk);
    if (decodedDisk.second) {
        diskStatuses_[driveType].saveFailed();

        auto imagePath = diskAssets_[driveType].path();
        std::ofstream out(imagePath, std::ios_base::out | std::ios_base::binary);
        if (!out.fail()) {
            out.write((char *)writeOut.first, decodedDisk.first);
            if (!out.fail() && !out.bad()) {
                diskStatuses_[driveType].saved();
                return;
            }
        }
    }

    diskStatuses_[driveType].saveFailed();
}

void ClemensStorageUnit::saveHardDisk(unsigned driveIndex, ClemensProDOSDisk &disk) {
    if (!hardDiskStatuses_[driveIndex].isMounted())
        return;
    if (!disk.save()) {
        hardDiskStatuses_[driveIndex].saveFailed();
        return;
    }
    hardDiskStatuses_[driveIndex].saved();
}

bool ClemensStorageUnit::serialize(mpack_writer_t *writer) { return false; }

bool ClemensStorageUnit::unserialize(mpack_reader_t *reader) {
    allocateBuffers();
    return false;
}
