#include "clem_storage_unit.hpp"
#include "clem_disk_utils.hpp"

#include "cinek/buffer.hpp"
#include "clem_disk.h"
#include "clem_mmio_types.h"
#include "clem_smartport.h"
#include "core/clem_disk_asset.hpp"
#include "core/clem_prodos_disk.hpp"
#include "emulator_mmio.h"
#include "external/mpack.h"

#include "fmt/format.h"
#include "spdlog/spdlog.h"

#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <vector>

namespace {

constexpr unsigned kDecodingBufferSize = 4 * 1024 * 1024;
constexpr unsigned kSmartPortDiskSize = 32 * 1024 * 1024;

unsigned calculateSlabHeapSize() { return kSmartPortDiskSize + kDecodingBufferSize + 4096; }
} // namespace

ClemensStorageUnit::ClemensStorageUnit()
    : slab_(calculateSlabHeapSize(), malloc(calculateSlabHeapSize())) {

    allocateBuffers();
}

ClemensStorageUnit::~ClemensStorageUnit() { free(slab_.getHead()); }

void ClemensStorageUnit::allocateBuffers() {
    slab_.reset();

    // create empty backing ProDOS buffer for SmartPort disk
    hardDisks_[0] = ClemensProDOSDisk(cinek::ByteBuffer(
        slab_.allocateArray<uint8_t>(kSmartPortDiskSize + 128), kSmartPortDiskSize + 128));

    // create empty decode scratchpad for saving images to the host's filesystem
    decodeBuffer_ =
        cinek::ByteBuffer(slab_.allocateArray<uint8_t>(kDecodingBufferSize), kDecodingBufferSize);

    diskStatuses_.fill(ClemensDiskDriveStatus{});
    hardDiskStatuses_.fill(ClemensDiskDriveStatus{});
}

ClemensSmartPortUnit *ClemensStorageUnit::getSmartPortUnit(ClemensMMIO &mmio, unsigned driveIndex) {
    ClemensSmartPortUnit *unit = clemens_smartport_unit_get(&mmio, driveIndex);
    if (!unit) {
        spdlog::error(
            "ClemensStorageUnit::assignSmartPortDisk - Invalid smartport unit {} specified",
            driveIndex);
        return NULL;
    }
    if (hardDiskStatuses_[driveIndex].isMounted()) {
        spdlog::error("ClemensStorageUnit::assignSmartPortDisk - Drive unit {} is already mounted",
                      driveIndex);
        return NULL;
    }
    return unit;
}

bool ClemensStorageUnit::createSmartPortDisk(ClemensMMIO &mmio, unsigned driveIndex,
                                             const std::string &imagePath) {
    ClemensSmartPortUnit *unit = getSmartPortUnit(mmio, driveIndex);
    if (!unit)
        return false;
    return false;
}

bool ClemensStorageUnit::assignSmartPortDisk(ClemensMMIO &mmio, unsigned driveIndex,
                                             const std::string &imagePath) {
    ClemensSmartPortUnit *unit = getSmartPortUnit(mmio, driveIndex);
    if (!unit)
        return false;

    auto asset = ClemensDiskAsset(imagePath);
    if (asset.imageType() != ClemensDiskAsset::Image2IMG &&
        asset.imageType() != ClemensDiskAsset::ImageProDOS) {
        hardDiskStatuses_[driveIndex].mountFailed();
        return false;
    }

    struct ClemensSmartPortDevice device {};
    hardDiskAssets_[driveIndex] = asset;
    if (!hardDisks_[0].bind(device, hardDiskAssets_[driveIndex])) {
        spdlog::error("ClemensStorageUnit::assignSmartPortDisk - Bind failed for disk {}:{}",
                      driveIndex, imagePath);
        hardDiskStatuses_[driveIndex].mountFailed();
        return false;
    }
    hardDiskStatuses_[driveIndex].mount(imagePath);
    spdlog::info("ClemensStorageUnit - smart{}: {} mounted", driveIndex, imagePath);
    return clemens_assign_smartport_disk(&mmio, driveIndex, &device);
}

bool ClemensStorageUnit::ejectSmartPortDisk(ClemensMMIO &mmio, unsigned driveIndex) {
    if (!hardDiskStatuses_[driveIndex].isMounted())
        return false;

    struct ClemensSmartPortDevice device {};
    clemens_remove_smartport_disk(&mmio, driveIndex, &device);
    hardDisks_[driveIndex].release(device);
    hardDiskStatuses_[driveIndex].unmount();
    spdlog::info("ClemensStorageUnit - smart{}: ejected", driveIndex);
    return true;
}

ClemensDrive *ClemensStorageUnit::getDrive(ClemensMMIO &mmio, ClemensDriveType driveType) {
    ClemensDrive *drive = clemens_drive_get(&mmio, driveType);
    if (!drive)
        return NULL;
    if (diskStatuses_[driveType].isMounted()) {
        ejectDisk(mmio, driveType);
    }
    return drive;
}

bool ClemensStorageUnit::createDisk(ClemensMMIO &mmio, ClemensDriveType driveType,
                                    const std::string &path) {
    ClemensDrive *drive = getDrive(mmio, driveType);
    if (!drive)
        return false;

    //  Use the decode buffer to generate a blank image and leverage the utilities
    //  in ClemensDiskAsset to generate the underlying nib (basically, generate a
    //  blank image...)
    decodeBuffer_.reset();
    auto imageBuffer = decodeBuffer_.forwardSize(decodeBuffer_.getCapacity());
    auto imageGeneratedSource = ClemensDiskAsset::createBlankDiskImage(
        ClemensDiskAsset::fromAssetPathUsingExtension(path),
        ClemensDiskAsset::diskTypeFromDriveType(driveType), true, imageBuffer);
    if (cinek::length(imageGeneratedSource) == 0)
        return false;

    bool mounted = mountDisk(mmio, path, driveType, imageGeneratedSource);
    if (mounted) {
        //  save the new disk NOW
        saveDisk(driveType, drive->disk);
    }
    return mounted;
}

bool ClemensStorageUnit::insertDisk(ClemensMMIO &mmio, ClemensDriveType driveType,
                                    const std::string &path) {
    ClemensDrive *drive = getDrive(mmio, driveType);
    if (!drive)
        return false;

    std::ifstream input(path, std::ios_base::in | std::ios_base::binary);
    if (!input.is_open()) {
        diskStatuses_[driveType].mountFailed();
        return false;
    }
    auto inputImageSize = input.seekg(0, std::ios_base::end).tellg();
    if (inputImageSize > decodeBuffer_.getCapacity()) {
        assert(false);
        spdlog::error("ClemensStorageUnit::insertDisk - image {}:{} size is greater than disk "
                      "buffer size {}",
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
        spdlog::error("ClemensStorageUnit::insertDisk - image {}:{} size is greater than disk "
                      "buffer size {}",
                      ClemensDiskUtilities::getDriveName(driveType), path,
                      decodeBuffer_.getCapacity());
        diskStatuses_[driveType].mountFailed();
        return false;
    }
    input.close();

    return mountDisk(mmio, path, driveType, decodeBuffer_.getRange());
}

bool ClemensStorageUnit::mountDisk(ClemensMMIO &mmio, const std::string &path,
                                   ClemensDriveType driveType, cinek::ConstRange<uint8_t> source) {
    struct ClemensNibbleDisk *disk = clemens_insert_disk(&mmio, driveType);
    if (!disk)
        return false;
    diskAssets_[driveType] = ClemensDiskAsset(path, driveType, source, *disk);
    if (diskAssets_[driveType].errorType() != ClemensDiskAsset::ErrorNone) {
        diskAssets_[driveType] = ClemensDiskAsset();
        clemens_eject_disk(&mmio, driveType);
        diskStatuses_[driveType].mountFailed();
        return false;
    }
    diskStatuses_[driveType].mount(path);
    spdlog::info("ClemensStorageUnit - {}: {} mounted",
                 ClemensDiskUtilities::getDriveName(driveType), path);
    return true;
}

bool ClemensStorageUnit::ejectDisk(ClemensMMIO &mmio, ClemensDriveType driveType) {
    if (!clemens_drive_get(&mmio, driveType)->has_disk || !diskStatuses_[driveType].isMounted())
        return false;

    struct ClemensNibbleDisk *disk = clemens_eject_disk(&mmio, driveType);
    saveDisk(driveType, *disk);
    spdlog::info("ClemensStorageUnit - {}: ejected", ClemensDiskUtilities::getDriveName(driveType));
    diskStatuses_[driveType].unmount();
    return true;
}

void ClemensStorageUnit::ejectAllDisks(ClemensMMIO &mmio) {
    ejectDisk(mmio, kClemensDrive_3_5_D1);
    ejectDisk(mmio, kClemensDrive_3_5_D2);
    ejectDisk(mmio, kClemensDrive_5_25_D1);
    ejectDisk(mmio, kClemensDrive_5_25_D2);
    ejectSmartPortDisk(mmio, 0);
    for (unsigned i = 0; i < (unsigned)hardDisks_.size(); ++i) {
        ejectSmartPortDisk(mmio, i);
    }
}

void ClemensStorageUnit::writeProtectDisk(ClemensMMIO &mmio, ClemensDriveType driveType, bool wp) {
    ClemensDrive *drive = clemens_drive_get(&mmio, driveType);
    if (!drive)
        return;
    if (drive->has_disk) {
        drive->disk.is_write_protected = wp;
    }
}

void ClemensStorageUnit::update(ClemensMMIO &mmio) {
    for (auto it = diskStatuses_.begin(); it != diskStatuses_.end(); ++it) {
        auto driveType = static_cast<ClemensDriveType>(it - diskStatuses_.begin());
        auto &status = diskStatuses_[driveType];
        if (!status.isMounted()) {
            status.isEjecting = false;
            status.isSpinning = false;
            status.isWriteProtected = false;
            continue;
        }
        auto *drive = clemens_drive_get(&mmio, driveType);
        status.isSpinning = drive->is_spindle_on;
        status.isWriteProtected = drive->disk.is_write_protected;
        if (drive->disk.disk_type == CLEM_DISK_TYPE_3_5) {
            auto ejectStatus = clemens_eject_disk_in_progress(&mmio, driveType);
            status.isEjecting = ejectStatus == CLEM_EJECT_DISK_STATUS_IN_PROGRESS;
            if (ejectStatus == CLEM_EJECT_DISK_STATUS_EJECTED) {
                //  user initiated eject will have already called clemens_eject_disk()
                //  this flow handles an eject initiated by the emulator
                struct ClemensNibbleDisk *disk = clemens_eject_disk(&mmio, driveType);
                assert(disk);
                saveDisk(driveType, *disk);
                diskStatuses_[driveType].unmount();
                spdlog::info("ClemensStorageUnit - {}: auto ejected",
                             ClemensDiskUtilities::getDriveName(driveType));
            }
        }
    }
    for (auto it = hardDiskStatuses_.begin(); it != hardDiskStatuses_.end(); ++it) {
        auto driveIndex = static_cast<unsigned>(it - hardDiskStatuses_.begin());
        auto &status = hardDiskStatuses_[driveIndex];
        if (!status.isMounted()) {
            status.isEjecting = false;
            status.isSpinning = false;
            status.isWriteProtected = false;
            continue;
        }
        auto *drive = clemens_smartport_unit_get(&mmio, driveIndex);
        status.isSpinning = drive->bus_enabled;
        status.isEjecting = false;
        status.isWriteProtected = false; // TODO: smartport write protection?
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
        auto imagePath = diskAssets_[driveType].path();
        std::ofstream out(imagePath, std::ios_base::out | std::ios_base::binary);
        if (!out.fail()) {
            out.write((char *)writeOut.first, decodedDisk.first);
            if (!out.fail() && !out.bad()) {
                diskStatuses_[driveType].saved();
                spdlog::info("ClemensStorageUnit - {}: {} saved",
                             ClemensDiskUtilities::getDriveName(driveType),
                             diskStatuses_[driveType].assetPath);
                return;
            }
        }
    }
    spdlog::error("{}: {} failed to save", ClemensDiskUtilities::getDriveName(driveType),
                  diskStatuses_[driveType].assetPath);
    diskStatuses_[driveType].saveFailed();
}

void ClemensStorageUnit::saveHardDisk(unsigned driveIndex, ClemensProDOSDisk &disk) {
    if (!hardDiskStatuses_[driveIndex].isMounted())
        return;
    if (!disk.save()) {
        spdlog::error("ClemensStorageUnit - Smart{}: {} failed to save", driveIndex,
                      hardDiskStatuses_[driveIndex].assetPath);
        hardDiskStatuses_[driveIndex].saveFailed();
        return;
    }
    spdlog::error("ClemensStorageUnit - Smart{}: {} saved", driveIndex,
                  hardDiskStatuses_[driveIndex].assetPath);
    hardDiskStatuses_[driveIndex].saved();
}

bool ClemensStorageUnit::serialize(ClemensMMIO &mmio, mpack_writer_t *writer) {
    //  drive status is polled and doesn't need to be saved
    //  the backing buffers are reallocated on unserialize and the data is
    //  read from the emulator serialize module
    //  so we serialize disk asset objects

    mpack_build_map(writer);
    mpack_write_cstr(writer, "disk.assets");
    mpack_start_array(writer, diskAssets_.size());
    bool success = true;
    for (auto assetIt = diskAssets_.begin(); assetIt != diskAssets_.end(); ++assetIt) {
        auto driveType = static_cast<ClemensDriveType>(assetIt - diskAssets_.begin());
        if (!diskAssets_[driveType].serialize(writer)) {
            success = false;
            break;
        }
    }
    mpack_finish_array(writer);

    mpack_write_cstr(writer, "smartport.assets");
    mpack_start_array(writer, hardDiskAssets_.size());
    for (auto assetIt = hardDiskAssets_.begin(); assetIt != hardDiskAssets_.end(); ++assetIt) {
        auto driveIndex = static_cast<unsigned>(assetIt - hardDiskAssets_.begin());
        if (!hardDiskAssets_[driveIndex].serialize(writer)) {
            success = false;
            break;
        }
    }
    mpack_finish_array(writer);

    mpack_write_cstr(writer, "smartport.data");
    mpack_start_array(writer, hardDisks_.size());
    for (unsigned i = 0; i < hardDisks_.size(); ++i) {
        auto *device = clemens_smartport_unit_get(&mmio, i);
        if (!hardDisks_[i].serialize(writer, device->device)) {
            success = false;
            break;
        }
    }
    mpack_finish_array(writer);

    mpack_complete_map(writer);

    return success;
}

bool ClemensStorageUnit::unserialize(ClemensMMIO &mmio, mpack_reader_t *reader,
                                     ClemensUnserializerContext context) {
    allocateBuffers();

    if (!mpack_expect_map(reader))
        return false;

    bool success = true;

    mpack_expect_cstr_match(reader, "disk.assets");
    mpack_expect_array(reader);
    for (auto assetIt = diskAssets_.begin(); assetIt != diskAssets_.end(); ++assetIt) {
        auto driveType = static_cast<ClemensDriveType>(assetIt - diskAssets_.begin());
        if (!diskAssets_[driveType].unserialize(reader)) {
            success = false;
            break;
        }
        diskStatuses_[driveType].mount(diskAssets_[driveType].path());
    }
    mpack_done_array(reader);

    mpack_expect_cstr_match(reader, "smartport.assets");
    mpack_expect_array(reader);
    for (auto assetIt = hardDiskAssets_.begin(); assetIt != hardDiskAssets_.end(); ++assetIt) {
        auto driveIndex = static_cast<unsigned>(assetIt - hardDiskAssets_.begin());
        if (!hardDiskAssets_[driveIndex].unserialize(reader)) {
            success = false;
            break;
        }
        hardDiskStatuses_[driveIndex].mount(hardDiskAssets_[driveIndex].path());
    }
    mpack_done_array(reader);

    mpack_expect_cstr_match(reader, "smartport.data");
    mpack_expect_array(reader);
    for (unsigned i = 0; i < hardDisks_.size(); ++i) {
        auto *device = clemens_smartport_unit_get(&mmio, i);
        if (!hardDisks_[i].unserialize(reader, device->device, context)) {
            success = false;
            break;
        }
    }
    mpack_done_array(reader);

    mpack_done_map(reader);

    return success;
}
