#include "clem_2img.h"
#include "clem_disk.h"
#include "external/cross_endian.h"

#include <assert.h>
#include <stdlib.h>
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

/* Derived from Beneath Apple DOS and ProDOS 2020
   Table 3.1 ProDOS Block Conversion Table for Diskettes

    Below documention refers to 5.25" disks

    3.5" documentation follows this section, and also refer to clem_disk.h

    Prodos logical sectors are defined in documentation as 512 bytes (they are
    effectively interleaved on the physical track.)
    As seen in the table, physical sectors 0 and 2 are used by logical prodos
        sector 0.
    Since DOS logical sectors are 256 bytes, we'll divide a logical ProDOS
        sector into 2 - this will keep the nibbilization code relatively
        consistent between DOS and ProDOS images.
    Note for 3.5" drives, the 'sector' size is 512 bytes
*/

static inline size_t _increment_data_ptr(const uint8_t *p, size_t amt, const uint8_t *end) {
    const uint8_t *n = p + amt;
    return n <= end ? amt : n - end;
}

//  all encoding is to little endian order in the final stream
static inline uint16_t _decode_u16(const uint8_t *data) {
    return ((uint16_t)data[1] << 8) | data[0];
}

static inline uint32_t _decode_u32(const uint8_t *data) {
    return (((uint32_t)data[3] << 24) | ((uint32_t)data[2] << 16) | ((uint32_t)data[1] << 8) |
            data[0]);
}

static inline void _encode_u16(uint8_t **data, uint16_t u16) {
    (*data)[1] = (uint8_t)(u16 >> 8);
    (*data)[0] = (uint8_t)(u16 & 0xff);
    *data += 2;
}

static inline void _encode_u32(uint8_t **data, uint32_t u32) {
    (*data)[3] = (uint8_t)(u32 >> 24);
    (*data)[2] = (uint8_t)(u32 >> 16);
    (*data)[1] = (uint8_t)(u32 >> 8);
    (*data)[0] = (uint8_t)(u32 & 0xff);
    *data += 4;
}

static void _encode_mem(uint8_t **data, const uint8_t *mem, uint32_t cnt, bool overlapped) {
    if (overlapped) {
        memmove(*data, mem, cnt);
    } else {
        memcpy(*data, mem, cnt);
    }
    *data += cnt;
}

bool clem_2img_parse_header(struct Clemens2IMGDisk *disk, const uint8_t *data,
                            const uint8_t *data_end) {
    size_t allocation_amt = 0;
    int state = 0;
    uint32_t param32;
    disk->image_buffer = data;
    disk->image_buffer_length = data_end - data;

    while (data <= data_end) {
        size_t data_size = 0;
        switch (state) {
        case 0: // is 2IMG?
            data_size = _increment_data_ptr(data, 4, data_end);
            if (!memcmp(data, "2IMG", data_size)) {
                ++state;
            } else {
                return false;
            }
            break;
        case 1: // creator tag
            data_size = _increment_data_ptr(data, 4, data_end);
            if (data_size == 4) {
                memcpy(disk->creator, data, data_size);
                ++state;
            } else {
                return false;
            }
            break;
        case 2: // header size
            data_size = _increment_data_ptr(data, 2, data_end);
            if (data_size == 2 && _decode_u16(data) == CLEM_2IMG_HEADER_BYTE_SIZE) {
                ++state;
            } else {
                return false;
            }
            break;
        case 3: // version number
            data_size = _increment_data_ptr(data, 2, data_end);
            if (data_size == 2) {
                disk->version = _decode_u16(data);
                ++state;
            } else {
                return false;
            }
            break;
        case 4: // image format
            data_size = _increment_data_ptr(data, 4, data_end);
            if (data_size == 4) {
                disk->format = _decode_u32(data);
                ++state;
            } else {
                return false;
            }
            break;
        case 5: // flags
            data_size = _increment_data_ptr(data, 4, data_end);
            if (data_size == 4) {
                param32 = _decode_u32(data);
                if (param32 & 0x80000000) {
                    disk->is_write_protected = true;
                }
                if (disk->format == CLEM_DISK_FORMAT_DOS) {
                    if (param32 & 0x100) {
                        disk->dos_volume = param32 & 0xff;
                    } else {
                        disk->dos_volume = CLEM_DISK_FORMAT_DOS_VOLUME_DEFAULT;
                    }
                    disk->dos_volume = param32 & 0xff;
                } else {
                    disk->dos_volume = 0;
                }
                ++state;
            } else {
                return false;
            }
            break;
        case 6: // ProDOS blocks
            data_size = _increment_data_ptr(data, 4, data_end);
            if (data_size == 4) {
                disk->block_count = _decode_u32(data);
                ++state;
            } else {
                return false;
            }
            break;
        case 7: // points to the start of the data chunk
            data_size = _increment_data_ptr(data, 4, data_end);
            if (data_size == 4) {
                disk->image_data_offset = _decode_u32(data);
                disk->data = disk->image_buffer + disk->image_data_offset;
                ++state;
            } else {
                return false;
            }
            break;
        case 8: // data_end - data = size
            data_size = _increment_data_ptr(data, 4, data_end);
            if (data_size == 4) {
                param32 = _decode_u32(data);
                if (param32 == 0) {
                    //  this is possible for prodos images
                    //  use blocks * 512
                    param32 = disk->block_count * 512;
                }
                disk->data_end = disk->data + param32;
                allocation_amt += param32;
                ++state;
            } else {
                return false;
            }
            break;
        case 9: // points to the start of the comment chunk
            data_size = _increment_data_ptr(data, 4, data_end);
            if (data_size == 4) {
                disk->comment = (char *)disk->image_buffer + _decode_u32(data);
                ++state;
            } else {
                return false;
            }
            break;
        case 10: // data_end - data = size
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
        case 11: // points to the start of the creator chunk
            data_size = _increment_data_ptr(data, 4, data_end);
            if (data_size == 4) {
                disk->creator_data = (const char *)disk->image_buffer + _decode_u32(data);
                ++state;
            } else {
                return false;
            }
            break;
        case 12: // commend_end - comment = size
            data_size = _increment_data_ptr(data, 4, data_end);
            if (data_size == 4) {
                param32 = _decode_u32(data);
                disk->creator_data_end = disk->creator_data + param32;
                allocation_amt += param32;
                ++state;
            } else {
                return false;
            }
            break;
        case 13: // empty space
            data_size = _increment_data_ptr(data, 16, data_end);
            if (data_size == 16) {
                ++state;
            } else {
                return false;
            }
            break;
        case 14: // if well formed header, we should get here
            return true;
        }
        data += data_size;
    }
    return false;
}

unsigned clem_2img_build_image(struct Clemens2IMGDisk *disk, uint8_t *image, uint8_t *image_end) {
    uint8_t *image_cur = image;
    uint8_t *data_tmp;
    size_t image_size = image_end - image;
    size_t source_size = disk->data_end - disk->data;
    size_t creator_size = disk->creator_data_end - disk->creator_data;
    size_t comment_size = disk->comment_end - disk->comment;
    uint32_t flags;
    bool overlapped;

    if (image_end < disk->image_buffer) {
        overlapped = false;
    } else if (image >= disk->image_buffer + disk->image_buffer_length) {
        overlapped = false;
    } else if (image < disk->image_buffer ||
               (image == disk->image_buffer &&
                image_end == disk->image_buffer + disk->image_buffer_length)) {
        overlapped = true;
    } else {
        /* some overlap where data will be corrupted */
        return 0;
    }

    if (disk->data == NULL) {
        //  header only (so image buffer encompasses metadata only)
        if (image_size < creator_size + comment_size + CLEM_2IMG_HEADER_BYTE_SIZE)
            return false;
    } else {
        if (image_size < source_size + creator_size + comment_size + CLEM_2IMG_HEADER_BYTE_SIZE)
            return false;
    }
    if (disk->format == CLEM_DISK_FORMAT_PRODOS) {
        if (disk->block_count * 512 != source_size)
            return false;
    } else if (disk->format == CLEM_DISK_FORMAT_DOS) {
        /* DOS 140K disks*/
        if (source_size != 280 * 512)
            return 0;
    } else {
        return 0;
    }

    disk->image_buffer = image;

    _encode_mem(&image_cur, (uint8_t *)"2IMG", 4, overlapped);
    _encode_mem(&image_cur, (uint8_t *)"CLEM", 4, overlapped);
    _encode_u16(&image_cur, CLEM_2IMG_HEADER_BYTE_SIZE);
    _encode_u16(&image_cur, disk->version);
    _encode_u32(&image_cur, disk->format);
    flags = 0;
    if (disk->is_write_protected)
        flags |= 0x80000000;
    if (disk->dos_volume < 254)
        flags |= (0x0100 + (disk->dos_volume & 0xff));
    _encode_u32(&image_cur, flags);
    if (disk->format == CLEM_DISK_FORMAT_PRODOS) {
        _encode_u32(&image_cur, disk->block_count);
    } else {
        _encode_u32(&image_cur, 0);
    }
    _encode_u32(&image_cur, CLEM_2IMG_HEADER_BYTE_SIZE);
    _encode_u32(&image_cur, source_size);
    if (creator_size > 0) {
        _encode_u32(&image_cur, CLEM_2IMG_HEADER_BYTE_SIZE + source_size);
        _encode_u32(&image_cur, (uint32_t)(creator_size));
    } else {
        _encode_u32(&image_cur, 0);
        _encode_u32(&image_cur, 0);
    }
    if (comment_size > 0) {
        _encode_u32(&image_cur, CLEM_2IMG_HEADER_BYTE_SIZE + source_size + creator_size);
        _encode_u32(&image_cur, (uint32_t)(comment_size));
    } else {
        _encode_u32(&image_cur, 0);
        _encode_u32(&image_cur, 0);
    }
    /* empty 16-byte buffer*/
    _encode_u32(&image_cur, 0);
    _encode_u32(&image_cur, 0);
    _encode_u32(&image_cur, 0);
    _encode_u32(&image_cur, 0);
    if (image_cur - image != CLEM_2IMG_HEADER_BYTE_SIZE)
        return 0;
    if (disk->data != NULL) {
        data_tmp = image_cur;
        _encode_mem(&image_cur, disk->data, source_size, overlapped);
        disk->data = data_tmp;
        disk->data_end = image_cur;
        _encode_mem(&image_cur, (uint8_t *)disk->creator_data, creator_size, overlapped);
        _encode_mem(&image_cur, (uint8_t *)disk->comment, comment_size, overlapped);
    }
    disk->image_buffer_length = (unsigned)(image_cur - image);

    return disk->image_buffer_length;
}

bool clem_2img_generate_header(struct Clemens2IMGDisk *disk, uint32_t format, const uint8_t *image,
                               const uint8_t *image_end, uint32_t image_data_offset,
                               unsigned data_size) {
    uint32_t disk_size =
        data_size > 0 ? data_size : (uint32_t)(image_end - image) - image_data_offset;

    strncpy(disk->creator, "CLEM", sizeof(disk->creator));

    /* validate that the input contains only sector data */
    if (format == CLEM_DISK_FORMAT_PRODOS) {
        disk->block_count = disk_size / 512;
        if (disk_size % 512) {
            return false;
        }
    } else if (format == CLEM_DISK_FORMAT_DOS) {
        disk->block_count = 0;
        if (disk_size % 256) {
            return false;
        }
    }

    /* TODO: support creator data and comments */
    disk->image_buffer = image;
    disk->image_buffer_length = (uint32_t)(image_end - image);
    disk->image_data_offset = image_data_offset;

    disk->data = disk->image_buffer + disk->image_data_offset;
    disk->data_end = disk->data + disk_size;

    disk->creator_data = (char *)disk->data_end;
    disk->creator_data_end = disk->creator_data;
    disk->comment = disk->creator_data_end;
    disk->comment_end = disk->comment;

    disk->version = 0x0001;
    disk->format = format;
    if (format == CLEM_DISK_FORMAT_DOS) {
        disk->dos_volume = 0xfe;
    } else {
        disk->dos_volume = 0x00;
    }
    disk->is_write_protected = false;
    return true;
}

/* Much of this implementation derives from the formatting section detailed in
   Beneath Apple DOS and Beneath Apple ProDOS, Chapter 3

    - self-sync gaps are encoded as 10-bit nibbles decoded to 0xff by the
      emulated system
    - these gaps may differ depending on DOS vs ProDOS disk formatting
    - sectors are are interleaved during nibbilization from their incoming
      sequential order to account for a 'spinning' disk and to give programs
      the time to decode data into memory before reading the next sector in
      sequence
    - in general DOS and ProDOS formatting is interchangeable at this level
*/

static bool _clem_2img_nibblize_data_35(struct Clemens2IMGDisk *disk) {
    unsigned disk_data_size = (unsigned)(disk->data_end - disk->data);
    //  All 3.5 disks must conform to one of two sizes 400K or 800K, reflected
    //  by their "sided-ness"
    bool is_double_sided;
    if (disk->block_count > 0) {
        if (disk->block_count == CLEM_DISK_35_PRODOS_BLOCK_COUNT) {
            is_double_sided = false;
            disk->nib->track_count = 80;
        } else if (disk->block_count == CLEM_DISK_35_DOUBLE_PRODOS_BLOCK_COUNT) {
            is_double_sided = true;
            disk->nib->track_count = 160;
        } else {
            return false;
        }
    } else if (disk_data_size == CLEM_DISK_35_PRODOS_BLOCK_COUNT * 512) {
        disk->nib->track_count = 80;
        is_double_sided = false;
    } else if (disk_data_size == CLEM_DISK_35_DOUBLE_PRODOS_BLOCK_COUNT * 512) {
        disk->nib->track_count = 160;
        is_double_sided = true;
    } else {
        return false;
    }

    clem_nib_reset_tracks(disk->nib, disk->nib->track_count, disk->nib->bits_data,
                          disk->nib->bits_data_end);
    disk->nib->is_write_protected = disk->is_write_protected;
    return clem_disk_nib_encode_35(disk->nib, disk->format, is_double_sided, disk->data,
                                   disk->data_end);
}

static bool _clem_2img_nibblize_data_525(struct Clemens2IMGDisk *disk) {
    disk->nib->is_write_protected = disk->is_write_protected;
    disk->nib->track_count = 35;
    clem_nib_reset_tracks(disk->nib, disk->nib->track_count, disk->nib->bits_data,
                          disk->nib->bits_data_end);
    return clem_disk_nib_encode_525(disk->nib, disk->format, disk->dos_volume, disk->data,
                                    disk->data_end);
}

bool clem_2img_nibblize_data(struct Clemens2IMGDisk *disk) {
    switch (disk->nib->disk_type) {
    case CLEM_DISK_TYPE_5_25:
        return _clem_2img_nibblize_data_525(disk);
    case CLEM_DISK_TYPE_3_5:
        return _clem_2img_nibblize_data_35(disk);
    }
    return false;
}

bool _clem_2img_decode_nibblized_disk_35(struct Clemens2IMGDisk *disk, uint8_t *data_start,
                                         uint8_t *data_end, const struct ClemensNibbleDisk *nib) {
    disk->is_write_protected = nib->is_write_protected;
    disk->data = data_start;
    disk->data_end = clem_disk_nib_decode_35(nib, disk->format, data_start, data_end);
    if (!disk->data_end)
        return false;
    return true;
}

bool _clem_2img_decode_nibblized_disk_525(struct Clemens2IMGDisk *disk, uint8_t *data_start,
                                          uint8_t *data_end, const struct ClemensNibbleDisk *nib) {
    disk->is_write_protected = nib->is_write_protected;
    disk->data = data_start;
    disk->data_end = clem_disk_nib_decode_525(nib, disk->format, data_start, data_end);
    if (!disk->data_end)
        return false;
    return true;
}

bool clem_2img_decode_nibblized_disk(struct Clemens2IMGDisk *disk, uint8_t *data_start,
                                     uint8_t *data_end, const struct ClemensNibbleDisk *nib) {
    // The nibblized data is converted back to practical bytes for storage into
    // DOS or ProDOS images, removing the sync bytes, headers, and data is
    // converted from GCR to real bytes.

    // Iterate through each track in the map.  Sectors are serialized into the
    // the supplied buffer in the defined order (DOS or ProDOS) and in track
    // + side order.  All 3.5 disks have 512 byte sectors - 5.25 disks have 256
    // byte sectors.

    // For each nibblized track:
    //  If any scan error occurs, flag `corrupted` as TRUE
    //  Scan track bit by bit for the address prologue
    //  Decode track number, sector number, etc and validate with the decoded chksum
    //  From this we can calculate the sector offset into a disk
    //      - a bit more complicated for 3.5 disks as they need to account for region
    //  Scan for data prologue and body
    //      - read the whole data chunk so it can be decoded from GCR nibbles
    //  Continue until the we reach the point where we started the scan for the
    //      track
    //  Advance to next track and continue until we are at the end of all tracks
    //
    switch (disk->nib->disk_type) {
    case CLEM_DISK_TYPE_3_5:
        return _clem_2img_decode_nibblized_disk_35(disk, data_start, data_end, nib);
    case CLEM_DISK_TYPE_5_25:
        return _clem_2img_decode_nibblized_disk_525(disk, data_start, data_end, nib);
    }

    return false;
}
