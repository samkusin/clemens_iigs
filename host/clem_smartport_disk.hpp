#ifndef CLEM_HOST_SMARTPORT_DISK_HPP
#define CLEM_HOST_SMARTPORT_DISK_HPP

#include "clem_2img.h"
#include "smartport/prodos_hdd32.h"

#include <cstdint>
#include <string>
#include <vector>

class ClemensSmartPortDisk {
  public:
    // DO NOT CHANGE THE ORDERING OF THESE ENUM VALUES (Serialization Note)
    enum ImageType { ImageUndefined, ImageProDOS, Image2IMG };

    static std::vector<uint8_t> createData(unsigned block_count);

    ClemensSmartPortDisk();
    ClemensSmartPortDisk(std::vector<uint8_t> data);
    ClemensSmartPortDisk(ClemensSmartPortDisk &&other);
    ~ClemensSmartPortDisk();

    ClemensSmartPortDisk &operator=(ClemensSmartPortDisk &&other);

    bool hasImage() const { return imageType_ != ImageUndefined; }

    void write(unsigned block_index, const uint8_t *data);
    void read(unsigned block_index, uint8_t *data);

    //  the underlying container as a 2IMG
    Clemens2IMGDisk &getDisk() { return disk_; }
    const Clemens2IMGDisk &getDisk() const { return disk_; }

    //  the Smartport interface
    ClemensSmartPortDevice *createSmartPortDevice(ClemensSmartPortDevice *device);
    void destroySmartPortDevice(ClemensSmartPortDevice *device);

    void serialize(mpack_writer_t *writer, ClemensSmartPortDevice *device) const;
    void unserialize(mpack_reader_t *reader, ClemensSmartPortDevice *device,
                     ClemensSerializerAllocateCb alloc_cb, void *context);

  private:
    static uint8_t doReadBlock(void *userContext, unsigned driveIndex, unsigned blockIndex,
                               uint8_t *buffer);

    static uint8_t doWriteBlock(void *userContext, unsigned driveIndex, unsigned blockIndex,
                                const uint8_t *buffer);
    static uint8_t doFlush(void *userCcontext, unsigned driveIndex);

    ClemensSmartPortDisk &moveFrom(ClemensSmartPortDisk &other);
    ImageType initializeContainer();

  private:
    Clemens2IMGDisk disk_;
    std::vector<uint8_t> image_;
    ImageType imageType_;
    ClemensProdosHDD32 clemensHDD_;
};

#endif
