#ifndef CLEM_HOST_SMARTPORT_DISK_HPP
#define CLEM_HOST_SMARTPORT_DISK_HPP

#include "clem_2img.h"
#include "smartport/prodos_hdd32.h"

#include <cstdint>
#include <string>
#include <vector>

class ClemensSmartPortDisk {
  public:
    static std::vector<uint8_t> createData(unsigned block_count);

    ClemensSmartPortDisk();
    ClemensSmartPortDisk(std::vector<uint8_t> data);
    ClemensSmartPortDisk(ClemensSmartPortDisk &&other);
    ~ClemensSmartPortDisk();

    ClemensSmartPortDisk &operator=(ClemensSmartPortDisk &&other);

    void write(unsigned block_index, const uint8_t *data);
    void read(unsigned block_index, uint8_t *data);

    //  the underlying container
    Clemens2IMGDisk &getDisk() { return disk_; }
    const Clemens2IMGDisk &getDisk() const { return disk_; }

    //  the Smartport interface
    ClemensSmartPortDevice *createSmartPortDevice(ClemensSmartPortDevice *device);
    void destroySmartPortDevice(ClemensSmartPortDevice *device);

  private:
    static uint8_t doReadBlock(void *userContext, unsigned driveIndex, unsigned blockIndex,
                               uint8_t *buffer);

    static uint8_t doWriteBlock(void *userContext, unsigned driveIndex, unsigned blockIndex,
                                const uint8_t *buffer);
    static uint8_t doFlush(void *userCcontext, unsigned driveIndex);

  private:
    Clemens2IMGDisk disk_;
    std::string path_;
    std::vector<uint8_t> image_;
    ClemensProdosHDD32 clemensHDD_;
};

#endif
