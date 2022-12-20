#include "clem_smartport_disk.hpp"

#include <cassert>

std::vector<uint8_t> ClemensSmartPortDisk::createData(unsigned block_count) {
    std::vector<uint8_t> data(block_count * 512 + CLEM_2IMG_HEADER_BYTE_SIZE);
    Clemens2IMGDisk disk;
    if (clem_2img_generate_header(&disk, CLEM_2IMG_FORMAT_PRODOS, data.data(),
                                  data.data() + data.size(), CLEM_2IMG_HEADER_BYTE_SIZE)) {
        if (clem_2img_build_image(&disk, disk.image_buffer,
                                  disk.image_buffer + disk.image_buffer_length)) {
            return data;
        }
        data.clear();
    }
    return data;
}

ClemensSmartPortDisk::ClemensSmartPortDisk() : disk_{} {}

ClemensSmartPortDisk::ClemensSmartPortDisk(std::vector<uint8_t> data)
    : disk_{}, image_(std::move(data)) {

    clem_2img_parse_header(&disk_, image_.data(), image_.data() + image_.size());
}

ClemensSmartPortDisk::~ClemensSmartPortDisk() {}

void ClemensSmartPortDisk::write(unsigned block_index, const uint8_t *data) {
    if (block_index >= disk_.block_count)
        return;
    if (disk_.format != CLEM_2IMG_FORMAT_PRODOS)
        return;

    memcpy(disk_.data + block_index * 512, data, 512);
}

void ClemensSmartPortDisk::read(unsigned block_index, uint8_t *data) {
    if (block_index >= disk_.block_count)
        return;
    if (disk_.format != CLEM_2IMG_FORMAT_PRODOS)
        return;

    memcpy(data, disk_.data + block_index * 512, 512);
}

uint8_t ClemensSmartPortDisk::doReadBlock(void *userContext, unsigned driveIndex,
                                          unsigned blockIndex, uint8_t *buffer) {
    return CLEM_SMARTPORT_STATUS_CODE_OFFLINE;
}

uint8_t ClemensSmartPortDisk::doWriteBlock(void *userContext, unsigned driveIndex,
                                           unsigned blockIndex, const uint8_t *buffer) {
    return CLEM_SMARTPORT_STATUS_CODE_OFFLINE;
}

uint8_t ClemensSmartPortDisk::doFlush(void *userCcontext, unsigned driveIndex) {
    return CLEM_SMARTPORT_STATUS_CODE_OFFLINE;
}

ClemensSmartPortDevice *
ClemensSmartPortDisk::createSmartPortDevice(ClemensSmartPortDevice *device) {
    clemensHDD_.block_limit = disk_.block_count;
    clemensHDD_.drive_index = 0;
    clemensHDD_.user_context = this;
    clemensHDD_.read_block = &ClemensSmartPortDisk::doReadBlock;
    clemensHDD_.write_block = &ClemensSmartPortDisk::doWriteBlock;
    clemensHDD_.flush = &ClemensSmartPortDisk::doFlush;
    clem_smartport_prodos_hdd32_initialize(device, &clemensHDD_);
    return device;
}

void ClemensSmartPortDisk::destroySmartPortDevice(ClemensSmartPortDevice *device) {
    assert(device->device_data == &clemensHDD_);
    clem_smartport_prodos_hdd32_uninitialize(device);
}
