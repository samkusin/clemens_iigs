#include "clem_prodos_disk.hpp"
#include "cinek/buffertypes.hpp"
#include "clem_disk_asset.hpp"

#include "clem_2img.h"
#include "smartport/prodos_hdd32.h"

#include <cassert>
#include <cstring>
#include <fstream>
#include <ios>

#include "external/mpack.h"
#include "spdlog/spdlog.h"

//  storage_ will contain g

ClemensProDOSDisk::ClemensProDOSDisk() {}

ClemensProDOSDisk::ClemensProDOSDisk(cinek::ByteBuffer backingBuffer)
    : storage_(std::move(backingBuffer)), blocks_{}, interface_{}, disk_{} {}

bool ClemensProDOSDisk::bind(ClemensSmartPortDevice &device, const ClemensDiskAsset &asset) {
    if (asset.diskType() != ClemensDiskAsset::DiskHDD)
        return false;
    save();
    storage_.reset();

    switch (asset.imageType()) {
    case ClemensDiskAsset::Image2IMG: {
        std::ifstream fsin(asset.path(), std::ios_base::binary);
        if (!fsin.is_open())
            return false;
        auto header = storage_.forwardSize(CLEM_2IMG_HEADER_BYTE_SIZE);
        fsin.read((char *)header.first, CLEM_2IMG_HEADER_BYTE_SIZE);
        if (fsin.bad() || fsin.eof())
            return false;
        if (!clem_2img_parse_header(&disk_, header.first, header.second))
            return false;
        auto sz = int32_t(disk_.data_end - disk_.data);
        if (sz > storage_.getCapacity())
            return false;
        fsin.seekg(disk_.image_data_offset);
        auto input = storage_.forwardSize(int32_t(disk_.data_end - disk_.data));
        if (cinek::length(input) < sz)
            return false;
        fsin.read((char *)input.first, sz);
        if (fsin.bad() || fsin.eof())
            return false;
        disk_.data = input.first;
        disk_.data_end = input.second;
        disk_.image_buffer_length = storage_.getSize();
        disk_.image_buffer = storage_.getHead();
        blocks_ = input;
        break;
    }
    case ClemensDiskAsset::ImageProDOS: {
        std::ifstream fsin(asset.path(), std::ios_base::binary);
        if (!fsin.is_open())
            return false;
        auto sz = std::streamoff(fsin.seekg(0, std::ios_base::end).tellg());
        auto input = storage_.forwardSize(sz + CLEM_2IMG_HEADER_BYTE_SIZE);
        if (cinek::length(input) < sz + CLEM_2IMG_HEADER_BYTE_SIZE)
            return false;
        fsin.seekg(0);
        fsin.read((char *)input.first + CLEM_2IMG_HEADER_BYTE_SIZE, sz);
        if (fsin.bad() || fsin.eof())
            return false;
        if (!clem_2img_generate_header(&disk_, CLEM_DISK_FORMAT_PRODOS, input.first, input.second,
                                       CLEM_2IMG_HEADER_BYTE_SIZE, 0))
            return false;
        blocks_ = cinek::Range<uint8_t>(input.first + CLEM_2IMG_HEADER_BYTE_SIZE, input.second);
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
    const uint8_t *dataStart = nullptr;
    const uint8_t *dataEnd = nullptr;

    auto imageType = ClemensDiskAsset::fromAssetPathUsingExtension(assetPath_);
    switch (imageType) {
    case ClemensDiskAsset::Image2IMG:
        clem_2img_build_image(&disk_, storage_.getHead(), storage_.getTail());
        dataStart = storage_.getHead();
        dataEnd = storage_.getTail();
        break;
    case ClemensDiskAsset::ImageProDOS:
        dataStart = disk_.data;
        dataEnd = disk_.data_end;
        break;
    default:
        assert(false);
        return false;
    }
    assert(dataStart && dataEnd);

    std::ofstream out(assetPath_, std::ios_base::out | std::ios_base::binary);
    if (out.fail())
        return false;
    out.write((char *)dataStart, dataEnd - dataStart);
    if (out.fail() || out.bad())
        return false;

    spdlog::info("ClemensProDOSDisk - {} saved", assetPath_);
    return true;
}

void ClemensProDOSDisk::release(ClemensSmartPortDevice &device) {
    if (!save()) {
        spdlog::error("ClemensProDOSDisk - cannot save {}", assetPath_);
        return;
    }
    assert(device.device_data == &interface_);
    clem_smartport_prodos_hdd32_uninitialize(&device);
    memset(&interface_, 0, sizeof(interface_));
    storage_.reset();
    assetPath_.clear();
}

uint8_t ClemensProDOSDisk::doReadBlock(void *userContext, unsigned /*driveIndex */,
                                       unsigned blockIndex, uint8_t *buffer) {
    auto *self = reinterpret_cast<ClemensProDOSDisk *>(userContext);
    const uint8_t *data_head = self->blocks_.first;
    if (blockIndex >= self->interface_.block_limit)
        return CLEM_SMARTPORT_STATUS_CODE_INVALID_BLOCK;
    memcpy(buffer, data_head + blockIndex * 512, 512);
    return CLEM_SMARTPORT_STATUS_CODE_OK;
}

uint8_t ClemensProDOSDisk::doWriteBlock(void *userContext, unsigned /*driveIndex*/,
                                        unsigned blockIndex, const uint8_t *buffer) {
    auto *self = reinterpret_cast<ClemensProDOSDisk *>(userContext);
    uint8_t *data_head = self->blocks_.first;
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

    mpack_write_cstr(writer, "path");
    mpack_write_cstr(writer, assetPath_.c_str());

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
        unsigned bytesLeft = storage_.getSize();
        unsigned pageCount = (bytesLeft + 4095) / 4096;
        const uint8_t *data = storage_.getHead();
        mpack_start_array(writer, pageCount);
        while (bytesLeft > 0) {
            auto writeCount = std::min((unsigned)(storage_.getTail() - data), 4096U);
            mpack_write_bin(writer, (const char *)data, writeCount);
            bytesLeft -= writeCount;
            data += writeCount;
        }
        mpack_finish_array(writer);
    }

    mpack_complete_map(writer);
    return true;
}

bool ClemensProDOSDisk::unserialize(mpack_reader_t *reader, ClemensSmartPortDevice &device,
                                    ClemensUnserializerContext context) {
    char buf[1024];
    mpack_expect_map(reader);

    mpack_expect_cstr_match(reader, "path");
    mpack_expect_cstr(reader, buf, sizeof(buf));
    assetPath_ = buf;

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

    auto imageType = ClemensDiskAsset::fromAssetPathUsingExtension(assetPath_);
    if (imageType == ClemensDiskAsset::Image2IMG) {
        if (!clem_2img_parse_header(&disk_, storage_.getHead(),
                                    storage_.getHead() + storage_.getSize()))
            return false;

    } else if (imageType == ClemensDiskAsset::ImageProDOS) {
        if (!clem_2img_generate_header(&disk_, CLEM_DISK_FORMAT_PRODOS, storage_.getHead(),
                                       storage_.getTail(), CLEM_2IMG_HEADER_BYTE_SIZE, 0))
            return false;
    } else if (imageType != ClemensDiskAsset::ImageNone) {
        spdlog::error("ClemensProDOSDisk - unsupported asset {}", assetPath_);
        return false;
    }

    blocks_ = cinek::Range<uint8_t>(const_cast<uint8_t *>(disk_.data),
                                    const_cast<uint8_t *>(disk_.data_end));

    //  This may be unnecessary if bind() was not called
    if (imageType != ClemensDiskAsset::ImageNone) {
        interface_.read_block = &ClemensProDOSDisk::doReadBlock;
        interface_.write_block = &ClemensProDOSDisk::doWriteBlock;
        interface_.flush = &ClemensProDOSDisk::doFlush;
        interface_.user_context = this;
    }

    return true;
}
