#include "clem_2img.h"
#include "contrib/cross_endian.h"

#include <string.h>
/*
https://apple2.org.za/gswv/a2zine/Docs/DiskImage_2MG_Info.txt

struct Clemens2IMGDisk {
    char creator[4];
    uint16_t version;
    uint32_t format;
    uint32_t dos_volume;
    uint32_t block_count;
    bool is_write_protected;
    uint8_t* creator_data;
    uint8_t* creator_data_end;
    char* comment;
    char* comment_end;
    uint8_t* data;
    uint8_t* data_end;
};
*/

static inline size_t _increment_data_ptr(
    const uint8_t* p, size_t amt, const uint8_t* end
) {
    const uint8_t* n = p + amt;
    return n <= end ? amt : n - end;
}

static inline uint16_t _decode_u16(const uint8_t* data) {
    return le16toh(((uint16_t)data[1] << 8) | data[0]);
}

static inline uint16_t _decode_u32(const uint8_t* data) {
    return le32toh(((uint32_t)data[3] << 24) | ((uint32_t)data[2] << 16) |
                   ((uint32_t)data[1] << 8) | data[0]);
}

bool clem_2img_parse_header(
    struct Clemens2IMGDisk* disk,
    uint8_t* data,
    uint8_t* data_end
) {
    size_t allocation_amt = 0;
    int state = 0;
    uint8_t* data_start = data;
    uint32_t param32;
    while (data < data_end) {
        size_t data_size = 0;
        switch (state) {
            case 0:             // is 2IMG?
                data_size = _increment_data_ptr(data, 4, data_end);
                if (!memcmp(data, "2IMG", data_size)) {
                    ++state;
                } else {
                    return false;
                }
                break;
            case 1:             // creator tag
                data_size = _increment_data_ptr(data, 4, data_end);
                if (data_size == 4) {
                    memcpy(disk->creator, data, data_size);
                    ++state;
                } else {
                    return false;
                }
                break;
            case 2:             // header size
                data_size = _increment_data_ptr(data, 2, data_end);
                if (data_size == 2 && _decode_u16(data) == 0x40) {
                    ++state;
                } else {
                    return false;
                }
                break;
            case 3:             // version number
                data_size = _increment_data_ptr(data, 2, data_end);
                if (data_size == 2) {
                    disk->version = _decode_u16(data);
                    ++state;
                } else {
                    return false;
                }
                break;
            case 4:             // image format
                data_size = _increment_data_ptr(data, 4, data_end);
                if (data_size == 4) {
                    disk->format = _decode_u32(data);
                    ++state;
                } else {
                    return false;
                }
                break;
            case 5:             // flags
                data_size = _increment_data_ptr(data, 4, data_end);
                if (data_size == 4) {
                    param32 = _decode_u32(data);
                    ++state;
                } else {
                    return false;
                }
                break;
            case 6:             // ProDOS blocks
                data_size = _increment_data_ptr(data, 4, data_end);
                if (data_size == 4) {
                    disk->block_count = _decode_u32(data);
                    ++state;
                } else {
                    return false;
                }
                break;
            case 7:             // points to the start of the data chunk
                data_size = _increment_data_ptr(data, 4, data_end);
                if (data_size == 4) {
                    disk->data = data_start + _decode_u32(data);
                    ++state;
                } else {
                    return false;
                }
                break;
            case 8:             // data_end - data = size
                data_size = _increment_data_ptr(data, 4, data_end);
                if (data_size == 4) {
                    param32 = _decode_u32(data);
                    disk->data_end = disk->data + param32;
                    allocation_amt += param32;
                    ++state;
                } else {
                    return false;
                }
                break;
            case 9:             // points to the start of the comment chunk
                data_size = _increment_data_ptr(data, 4, data_end);
                if (data_size == 4) {
                    disk->comment = data_start + _decode_u32(data);
                    ++state;
                } else {
                    return false;
                }
                break;
            case 10:             // data_end - data = size
                data_size = _increment_data_ptr(data, 4, data_end);
                if (data_size == 4) {
                    param32 = _decode_u32(data);
                    disk->comment_end = disk->comment + param32;
                    allocation_amt += param32;
                    ++state;
                } else {
                    return false;
                }
                break;
            case 11:             // points to the start of the creator chunk
                data_size = _increment_data_ptr(data, 4, data_end);
                if (data_size == 4) {
                    disk->creator_data = data_start + _decode_u32(data);
                    ++state;
                } else {
                    return false;
                }
                break;
            case 12:             // commend_end - comment = size
                data_size = _increment_data_ptr(data, 4, data_end);
                if (data_size == 4) {
                    param32 = _decode_u32(data);
                    disk->creator_data_end = disk->creator + param32;
                    allocation_amt += param32;
                    ++state;
                } else {
                    return false;
                }
                break;
            case 13:            // empty space
                data_size = _increment_data_ptr(data, 16, data_end);
                if (data_size == 16) {
                    ++state;
                } else {
                    return false;
                }
                break;
            case 14:            // if well formed header, we shouldn't get here
                return true;

        }
        data += data_size;
    }
    return false;
}
