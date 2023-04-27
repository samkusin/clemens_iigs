#include "clem_prodos_disk.hpp"
#include "clem_disk_asset.hpp"

#include "clem_2img.h"
#include "smartport/prodos_hdd32.h"

#include <cassert>
#include <cstring>
#include <fstream>
#include <ios>

#include "external/mpack.h"

ClemensProDOSDisk::ClemensProDOSDisk() {}

ClemensProDOSDisk::ClemensProDOSDisk(cinek::ByteBuffer backingBuffer)
    : storage_(std::move(backingBuffer)), disk_{} {}

bool ClemensProDOSDisk::bind(ClemensSmartPortDevice &device, const ClemensDiskAsset &asset) {
    //  load 2IMG or PO.
    //  DO IT! (look at clem_smartport_disk.hpp)
    if (asset.diskType() != ClemensDiskAsset::DiskHDD)
        return false;
    save();
    storage_.reset();

    switch (asset.imageType()) {
    case ClemensDiskAsset::Image2IMG: {
        std::ifstream fsin(asset.path(), std::ios_base::binary);
        if (!fsin.is_open())
            return false;
        auto input = storage_.forwardSize(CLEM_2IMG_HEADER_BYTE_SIZE);
        fsin.read((char *)input.first, cinek::length(input));
        if (fsin.bad() || fsin.eof())
            return false;
        if (!clem_2img_parse_header(&disk_, input.first, input.second))
            return false;
        auto sz = int32_t(disk_.data_end - disk_.data);
        if (sz > storage_.getCapacity())
            return false;
        fsin.seekg(disk_.image_data_offset);
        input = storage_.forwardSize(int32_t(disk_.data_end - disk_.data));
        if (cinek::length(input) < sz)
            return false;
        fsin.read((char *)storage_.getHead(), sz);
        if (fsin.bad() || fsin.eof())
            return false;
        break;
    }
    case ClemensDiskAsset::ImageProDOS: {
        std::ifstream fsin(asset.path(), std::ios_base::binary);
        if (!fsin.is_open())
            return false;
        auto sz = fsin.seekg(0, std::ios_base::end).tellg();
        auto input = storage_.forwardSize(CLEM_2IMG_HEADER_BYTE_SIZE + sz);
        if (cinek::length(input) < CLEM_2IMG_HEADER_BYTE_SIZE + sz)
            return false;
        fsin.seekg(0);
        fsin.read((char *)input.first + CLEM_2IMG_HEADER_BYTE_SIZE, sz);
        if (fsin.bad() || fsin.eof())
            return false;
        if (!clem_2img_generate_header(&disk_, CLEM_DISK_FORMAT_PRODOS, input.first, input.second,
                                       CLEM_2IMG_HEADER_BYTE_SIZE))
            return false;
        break;
    }
    default:
        return false;
    }

    assetPath_ = asset.path();

    interface_.read_block = &ClemensProDOSDisk::doReadBlock;
    interface_.write_block = &ClemensProDOSDisk::doWriteBlock;
    interface_.flush = &ClemensProDOSDisk::doFlush;
    interface_.block_limit = disk_.block_count;
    interface_.drive_index = 0;
    interface_.user_context = this;

    clem_smartport_prodos_hdd32_initialize(&device, &interface_);

    return true;
}

bool ClemensProDOSDisk::save() {
    if (assetPath_.empty())
        return true;
    assert(!storage_.isEmpty());
    std::ofstream out(assetPath_, std::ios_base::out | std::ios_base::binary);
    if (out.fail())
        return false;
    //  TODO: write 2IMG out or PO out
    // out.write((char *)storage_.getHead(), disk.image_buffer_length);
    if (out.fail() || out.bad())
        return false;
    return true;
}

void ClemensProDOSDisk::release(ClemensSmartPortDevice &device) {
    save();
    assert(device.device_data == &interface_);
    clem_smartport_prodos_hdd32_uninitialize(&device);
    memset(&interface_, 0, sizeof(interface_));
    storage_.reset();
    assetPath_.clear();
}

uint8_t ClemensProDOSDisk::doReadBlock(void *userContext, unsigned /*driveIndex */,
                                       unsigned blockIndex, uint8_t *buffer) {
    auto *self = reinterpret_cast<ClemensProDOSDisk *>(userContext);
    const uint8_t *data_head = (uint8_t *)self->storage_.getHead();
    if (blockIndex >= self->interface_.block_limit)
        return CLEM_SMARTPORT_STATUS_CODE_INVALID_BLOCK;
    memcpy(buffer, data_head + blockIndex * 512, 512);
    return CLEM_SMARTPORT_STATUS_CODE_OK;
}

uint8_t ClemensProDOSDisk::doWriteBlock(void *userContext, unsigned /*driveIndex*/,
                                        unsigned blockIndex, const uint8_t *buffer) {
    auto *self = reinterpret_cast<ClemensProDOSDisk *>(userContext);
    uint8_t *data_head = (uint8_t *)self->storage_.getHead();
    if (blockIndex >= self->interface_.block_limit)
        return CLEM_SMARTPORT_STATUS_CODE_INVALID_BLOCK;
    memcpy(data_head + blockIndex * 512, buffer, 512);
    return CLEM_SMARTPORT_STATUS_CODE_OK;
}

uint8_t ClemensProDOSDisk::doFlush(void * /*userContext*/, unsigned /*driveIndex*/) {
    return CLEM_SMARTPORT_STATUS_CODE_OFFLINE;
}

bool ClemensProDOSDisk::serialize(mpack_writer_t *writer, ClemensSmartPortDevice &device) {
    mpack_build_map(writer);

    mpack_write_cstr(writer, "impl");
    {
        if (interface_.block_limit > 0) {
            clem_smartport_prodos_hdd32_serialize(writer, &device, &interface_);
        } else {
            mpack_write_nil(writer);
        }
    }

    //  this will be either a 2IMG or a ProDOS image
    mpack_write_cstr(writer, "pages");
    {
        unsigned bytesLeft = (unsigned)storage_.getSize();
        unsigned pageCount = (bytesLeft + 4095) / 4096;
        unsigned byteOffset = 0;
        mpack_start_array(writer, pageCount);
        while (bytesLeft > 0) {
            unsigned writeCount = std::min(bytesLeft, 4096U);
            mpack_write_bin(writer, (const char *)storage_.getHead() + byteOffset, writeCount);
            bytesLeft -= writeCount;
            byteOffset += writeCount;
        }
        mpack_finish_array(writer);
    }

    mpack_complete_map(writer);
    return true;
}

bool ClemensProDOSDisk::unserialize(mpack_reader_t *reader, ClemensSmartPortDevice &device,
                                    ClemensUnserializerContext context) {
    mpack_expect_map(reader);

    mpack_expect_cstr_match(reader, "impl");
    if (mpack_peek_tag(reader).type == mpack_type_nil) {
        mpack_expect_nil(reader);
    } else {
        clem_smartport_prodos_hdd32_unserialize(reader, &device, &interface_, context.allocCb,
                                                context.allocUserPtr);
    }
    mpack_expect_cstr_match(reader, "pages");
    {
        unsigned pageCount = mpack_expect_array(reader);
        storage_.reset();
        assert((unsigned)storage_.getCapacity() >= pageCount * 4096);
        while (pageCount > 0) {
            unsigned byteCount = mpack_expect_bin(reader);
            auto bytes = storage_.forwardSize(byteCount);
            mpack_read_bytes(reader, (char *)bytes.first, byteCount);
            mpack_done_bin(reader);
            pageCount--;
        }
        mpack_done_array(reader);
    }

    mpack_done_map(reader);
    return true;
}
