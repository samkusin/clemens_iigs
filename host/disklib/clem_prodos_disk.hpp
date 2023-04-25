#ifndef CLEM_HOST_PRODOS_DISK_HPP
#define CLEM_HOST_PRODOS_DISK_HPP

#include "cinek/buffer.hpp"
#include "clem_shared.h"
#include "disklib/clem_disk_asset.hpp"
#include "smartport/prodos_hdd32.h"

#include "clem_2img.h"

#include <string>

//  forward declarations
typedef struct mpack_reader_t mpack_reader_t;
typedef struct mpack_writer_t mpack_writer_t;

struct ClemensDiskAsset;

struct ClemensUnserializerContext {
    ClemensSerializerAllocateCb allocCb;
    void *allocUserPtr;
};

//  A wrapper for tbe emulator type ClemensProdosHDD32
//  And
class ClemensProDOSDisk {
  public:
    ClemensProDOSDisk();
    ClemensProDOSDisk(cinek::ByteBuffer backingBuffer);

    bool bind(ClemensSmartPortDevice &device, const ClemensDiskAsset &asset);
    bool save();
    void release(ClemensSmartPortDevice &device);

    bool serialize(mpack_writer_t *writer, ClemensSmartPortDevice &device);
    bool unserialize(mpack_reader_t *reader, ClemensSmartPortDevice &device,
                     ClemensUnserializerContext context);

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
