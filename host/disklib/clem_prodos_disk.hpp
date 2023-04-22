#ifndef CLEM_HOST_PRODOS_DISK_HPP
#define CLEM_HOST_PRODOS_DISK_HPP

#include "cinek/buffer.hpp"
#include "disklib/clem_disk_asset.hpp"
#include "smartport/prodos_hdd32.h"

#include "clem_2img.h"

#include <string>

struct ClemensDiskAsset;

//  A wrapper for tbe emulator type ClemensProdosHDD32
//  And
class ClemensProDOSDisk {
  public:
    ClemensProDOSDisk();
    ClemensProDOSDisk(cinek::ByteBuffer backingBuffer);

    bool bind(ClemensSmartPortDevice &device, const ClemensDiskAsset &asset);
    bool save();
    void release(ClemensSmartPortDevice &device);

  private:
    static uint8_t doReadBlock(void *userContext, unsigned driveIndex, unsigned blockIndex,
                               uint8_t *buffer);

    static uint8_t doWriteBlock(void *userContext, unsigned driveIndex, unsigned blockIndex,
                                const uint8_t *buffer);
    static uint8_t doFlush(void *userContext, unsigned driveIndex);

    cinek::ByteBuffer storage_;
    ClemensProdosHDD32 interface_;

    std::string assetPath_;
    Clemens2IMGDisk disk_;
};

#endif
