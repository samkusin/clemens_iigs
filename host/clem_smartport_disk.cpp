#include "clem_smartport_disk.hpp"

#include "external/mpack.h"

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

ClemensSmartPortDisk::ClemensSmartPortDisk(ClemensSmartPortDisk &&other)
    : image_(std::move(other.image_)) {
    clem_2img_parse_header(&disk_, image_.data(), image_.data() + image_.size());
}

ClemensSmartPortDisk &ClemensSmartPortDisk::operator=(ClemensSmartPortDisk &&other) {
    image_ = std::move(other.image_);
    clem_2img_parse_header(&disk_, image_.data(), image_.data() + image_.size());
    return *this;
}

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

uint8_t ClemensSmartPortDisk::doReadBlock(void *userContext, unsigned /*driveIndex */,
                                          unsigned blockIndex, uint8_t *buffer) {
    auto *self = reinterpret_cast<ClemensSmartPortDisk *>(userContext);
    const uint8_t *data_head = self->disk_.data;
    if (blockIndex >= self->disk_.block_count)
        return CLEM_SMARTPORT_STATUS_CODE_INVALID_BLOCK;
    memcpy(buffer, data_head + blockIndex * 512, 512);
    return CLEM_SMARTPORT_STATUS_CODE_OK;
}

uint8_t ClemensSmartPortDisk::doWriteBlock(void *userContext, unsigned /*driveIndex*/,
                                           unsigned blockIndex, const uint8_t *buffer) {
    auto *self = reinterpret_cast<ClemensSmartPortDisk *>(userContext);
    uint8_t *data_head = self->disk_.data;
    if (blockIndex >= self->disk_.block_count)
        return CLEM_SMARTPORT_STATUS_CODE_INVALID_BLOCK;
    memcpy(data_head + blockIndex * 512, buffer, 512);
    return CLEM_SMARTPORT_STATUS_CODE_OK;
}

uint8_t ClemensSmartPortDisk::doFlush(void * /*userContext*/, unsigned /*driveIndex*/) {
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

void ClemensSmartPortDisk::serialize(mpack_writer_t *writer, ClemensSmartPortDevice *device) const {
    mpack_build_map(writer);

    mpack_write_cstr(writer, "path");
    mpack_write_cstr(writer, path_.c_str());
    mpack_write_cstr(writer, "impl");
    {
        if (clemensHDD_.block_limit > 0) {
            clem_smartport_prodos_hdd32_serialize(writer, device, &clemensHDD_);
        } else {
            mpack_write_nil(writer);
        }
    }

    mpack_write_cstr(writer, "pages");
    {
        unsigned bytesLeft = (unsigned)image_.size();
        unsigned pageCount = (bytesLeft + 4095) / 4096;
        unsigned byteOffset = 0;
        mpack_start_array(writer, pageCount);
        while (bytesLeft > 0) {
            unsigned writeCount = std::min(bytesLeft, 4096U);
            mpack_write_bin(writer, (const char *)image_.data() + byteOffset, writeCount);
            bytesLeft -= writeCount;
            byteOffset += writeCount;
        }
        mpack_finish_array(writer);
    }
    mpack_complete_map(writer);
}

void ClemensSmartPortDisk::unserialize(mpack_reader_t *reader, ClemensSmartPortDevice *device,
                                       ClemensSerializerAllocateCb alloc_cb, void *context) {
    char path[1024];

    mpack_expect_map(reader);
    mpack_expect_cstr_match(reader, "path");
    mpack_expect_cstr(reader, path, sizeof(path));
    path_ = path;
    mpack_expect_cstr_match(reader, "impl");
    if (mpack_peek_tag(reader).type == mpack_type_nil) {
        mpack_expect_nil(reader);
    } else {
        clem_smartport_prodos_hdd32_unserialize(reader, device, &clemensHDD_, alloc_cb, context);
    }
    mpack_expect_cstr_match(reader, "pages");
    {
        unsigned pageCount = mpack_expect_array(reader);
        image_.clear();
        image_.reserve(pageCount * 4096);
        while (pageCount > 0) {
            unsigned byteCount = mpack_expect_bin(reader);
            unsigned byteOffset = (unsigned)image_.size();
            image_.resize(byteOffset + byteCount);
            mpack_read_bytes(reader, (char *)image_.data() + byteOffset, byteCount);
            mpack_done_bin(reader);
            pageCount--;
        }
        mpack_done_array(reader);
    }
    mpack_done_map(reader);
    memset(&disk_, 0, sizeof(disk_));
    clem_2img_parse_header(&disk_, image_.data(), image_.data() + image_.size());
}
