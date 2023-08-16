#ifndef CLEM_HOST_STORAGE_UNIT_HPP
#define CLEM_HOST_STORAGE_UNIT_HPP

#include "cinek/buffer.hpp"
#include "cinek/buffertypes.hpp"
#include "cinek/fixedstack.hpp"
#include "clem_disk.h"
#include "core/clem_apple2gs_config.hpp"
#include "core/clem_disk_asset.hpp"
#include "core/clem_disk_status.hpp"
#include "core/clem_prodos_disk.hpp"

#include <array>
#include <string>

//  forward declarations
typedef struct mpack_reader_t mpack_reader_t;
typedef struct mpack_writer_t mpack_writer_t;

struct ClemensDrive;

struct ClemensMMIO;

//  Covers all emulated disk operations between the host and emulator (MMIO)
//    - assignSmartPortDisk should be called at emulator initialization prior to
//      machine execution (to mount.)
//      - 32MB disk limit
//    - commit should be called to write all disk data to the host's filesystem
//      - implicitly called via ejectDisk()
//      - implicitly called on serialize()
//      - implicitly called on destruction
//
class ClemensStorageUnit {
  public:
    ClemensStorageUnit(const std::string &diskLibraryPath);
    ~ClemensStorageUnit();

    bool assignSmartPortDisk(ClemensMMIO &mmio, unsigned driveIndex, const std::string &imagePath);
    void saveSmartPortDisk(ClemensMMIO &mmio, unsigned driveIndex);
    bool ejectSmartPortDisk(ClemensMMIO &mmio, unsigned driveIndex);
    bool insertDisk(ClemensMMIO &mmio, ClemensDriveType driveType, const std::string &path);
    void saveDisk(ClemensMMIO &mmio, ClemensDriveType driveType);
    bool ejectDisk(ClemensMMIO &mmio, ClemensDriveType driveType);
    void writeProtectDisk(ClemensMMIO &mmio, ClemensDriveType driveType, bool wp);

    void saveAllDisks(ClemensMMIO &mmio);
    void ejectAllDisks(ClemensMMIO &mmio);

    void update(ClemensMMIO &mmio);
    bool serialize(ClemensMMIO &mmio, mpack_writer_t *writer);
    bool unserialize(ClemensMMIO &mmio, mpack_reader_t *reader, ClemensUnserializerContext context);

    const ClemensDiskDriveStatus &getDriveStatus(ClemensDriveType driveType) const;
    const ClemensDiskDriveStatus &getSmartPortStatus(unsigned driveIndex) const;

    cinek::Range<uint8_t> getSmartPortBuffer(unsigned driveIndex);

  private:
    void allocateBuffers();
    bool mountDisk(ClemensMMIO &mmio, const std::string &path, ClemensDriveType driveType,
                   cinek::ConstRange<uint8_t> source);
    void saveDisk(ClemensDriveType driveType, ClemensNibbleDisk &disk);
    void saveHardDisk(unsigned driveIndex, ClemensProDOSDisk &disk);

    ClemensDrive *getDrive(ClemensMMIO &mmio, ClemensDriveType driveType);

    std::string getBackupLocation(const std::string &imagePath,  std::string_view driveName);
    bool saveImage(const std::string& imagePath, std::string_view driveName,
                       const uint8_t *data, size_t dataSize);

  private:
    std::array<ClemensDiskAsset, kClemensDrive_Count> diskAssets_;
    std::array<ClemensDiskDriveStatus, kClemensDrive_Count> diskStatuses_;

    std::array<ClemensProDOSDisk, kClemensSmartPortDiskLimit> smartDisks_;
    std::array<ClemensDiskAsset, kClemensSmartPortDiskLimit> smartDiskAssets_;
    std::array<ClemensDiskDriveStatus, kClemensSmartPortDiskLimit> smartDiskStatuses_;

    //  The slab contains the backing buffers for ClemensNibbleDisk, ClemensProDOSDisk,
    //  and scratch space for decoding disk assets, which remain fixed upon construction
    //  and are reset in unserialize()
    cinek::FixedStack slab_;
    cinek::ByteBuffer decodeBuffer_;
    std::string diskLibraryPath_;
};

#endif
