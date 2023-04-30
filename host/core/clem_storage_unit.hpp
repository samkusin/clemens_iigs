#ifndef CLEM_HOST_STORAGE_UNIT_HPP
#define CLEM_HOST_STORAGE_UNIT_HPP

#include "cinek/buffer.hpp"
#include "cinek/buffertypes.hpp"
#include "cinek/fixedstack.hpp"
#include "clem_disk.h"
#include "core/clem_disk_asset.hpp"
#include "core/clem_disk_status.hpp"
#include "core/clem_prodos_disk.hpp"

#include <array>
#include <string>

//  forward declarations
typedef struct mpack_reader_t mpack_reader_t;
typedef struct mpack_writer_t mpack_writer_t;

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
    ClemensStorageUnit();
    ~ClemensStorageUnit();

    bool assignSmartPortDisk(ClemensMMIO &mmio, unsigned driveIndex, const std::string &imagePath);
    bool insertDisk(ClemensMMIO &mmio, ClemensDriveType driveType, const std::string &path);
    bool ejectDisk(ClemensMMIO &mmio, ClemensDriveType driveType);
    void ejectAllDisks(ClemensMMIO &mmio);
    void update(ClemensMMIO &mmio);
    bool serialize(ClemensMMIO &mmio, mpack_writer_t *writer);
    bool unserialize(ClemensMMIO &mmio, mpack_reader_t *reader, ClemensUnserializerContext context);

    const ClemensDiskDriveStatus &getDriveStatus(ClemensDriveType driveType) const;
    const ClemensDiskDriveStatus &getSmartPortStatus(unsigned driveIndex) const;

    cinek::Range<uint8_t> getDiskBuffer(ClemensDriveType driveType);
    cinek::Range<uint8_t> getSmartPortBuffer(unsigned driveIndex);

  private:
    void allocateBuffers();
    void saveDisk(ClemensDriveType driveType, ClemensNibbleDisk &disk);
    void saveHardDisk(unsigned driveIndex, ClemensProDOSDisk &disk);

  private:
    std::array<cinek::Range<uint8_t>, kClemensDrive_Count> nibbleBuffers_;
    std::array<ClemensDiskAsset, kClemensDrive_Count> diskAssets_;
    std::array<ClemensDiskDriveStatus, kClemensDrive_Count> diskStatuses_;

    std::array<ClemensProDOSDisk, CLEM_SMARTPORT_DRIVE_LIMIT> hardDisks_;
    std::array<ClemensDiskAsset, CLEM_SMARTPORT_DRIVE_LIMIT> hardDiskAssets_;
    std::array<ClemensDiskDriveStatus, CLEM_SMARTPORT_DRIVE_LIMIT> hardDiskStatuses_;

    //  The slab contains the backing buffers for ClemensNibbleDisk, ClemensProDOSDisk,
    //  and scratch space for decoding disk assets, which remain fixed upon construction
    //  and are reset in unserialize()
    cinek::FixedStack slab_;
    cinek::ByteBuffer decodeBuffer_;
};

#endif
