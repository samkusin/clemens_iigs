#include "clem_2img.h"
#include "clem_disk.h"
#include "external/cross_endian.h"

#include <assert.h>
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
// clang-format off
static const uint8_t gcr_6_2_byte[64] = {
    0x96, 0x97, 0x9a, 0x9b, 0x9d, 0x9e, 0x9f, 0xa6,
    0xa7, 0xab, 0xac, 0xad, 0xae, 0xaf, 0xb2, 0xb3,
    0xb4, 0xb5, 0xb6, 0xb7, 0xb9, 0xba, 0xbb, 0xbc,
    0xbd, 0xbe, 0xbf, 0xcb, 0xcd, 0xce, 0xcf, 0xd3,
    0xd6, 0xd7, 0xd9, 0xda, 0xdb, 0xdc, 0xdd, 0xde,
    0xdf, 0xe5, 0xe6, 0xe7, 0xe9, 0xea, 0xeb, 0xec,
    0xed, 0xee, 0xef, 0xf2, 0xf3, 0xf4, 0xf5, 0xf6,
    0xf7, 0xf9, 0xfa, 0xfb, 0xfc, 0xfd, 0xfe, 0xff};
// clang-format on

static const unsigned prodos_to_logical_sector_map_525[1][16] = {
    {0, 8, 1, 9, 2, 10, 3, 11, 4, 12, 5, 13, 6, 14, 7, 15}};

/* only support 16 sector tracks */
static const unsigned dos_to_logical_sector_map_525[1][16] = {
    {0, 7, 14, 6, 13, 5, 12, 4, 11, 3, 10, 2, 9, 1, 8, 15}};

/* 3.5" drives have 512 byte sectors (ProDOS assumed) */
static const unsigned prodos_to_logical_sector_map_35[CLEM_DISK_35_NUM_REGIONS][16] = {
    {0, 6, 1, 7, 2, 8, 3, 9, 4, 10, 5, 11, -1, -1, -1, -1},
    {0, 6, 1, 7, 2, 8, 3, 9, 4, 10, 5, -1, -1, -1, -1, -1},
    {0, 5, 1, 6, 2, 7, 3, 8, 4, 9, -1, -1, -1, -1, -1, -1},
    {0, 5, 1, 6, 2, 7, 3, 8, 4, -1, -1, -1, -1, -1, -1, -1},
    {0, 4, 1, 5, 2, 6, 3, 7, -1, -1, -1, -1, -1, -1, -1, -1}};

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
                disk->is_nibblized = false;
                param32 = _decode_u32(data);
                if (param32 & 0x80000000) {
                    disk->is_write_protected = true;
                }
                if (disk->format == CLEM_2IMG_FORMAT_DOS) {
                    if (param32 & 0x100) {
                        disk->dos_volume = param32 & 0xff;
                    } else {
                        disk->dos_volume = 254;
                        ;
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
                disk->creator_data_end = disk->creator + param32;
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

    if (image_size < source_size + creator_size + comment_size + CLEM_2IMG_HEADER_BYTE_SIZE)
        return false;
    if (disk->format == CLEM_2IMG_FORMAT_PRODOS) {
        if (disk->block_count * 512 != source_size)
            return false;
    } else if (disk->format == CLEM_2IMG_FORMAT_DOS) {
        /* DOS 140K disks*/
        if (source_size != 280 * 512)
            return 0;
    } else {
        return 0;
    }

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
    if (disk->format == CLEM_2IMG_FORMAT_PRODOS) {
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

    _encode_mem(&image_cur, disk->data, source_size, overlapped);
    _encode_mem(&image_cur, (uint8_t *)disk->creator_data, creator_size, overlapped);
    _encode_mem(&image_cur, (uint8_t *)disk->comment, comment_size, overlapped);

    return (unsigned)(image_cur - image);
}

bool clem_2img_generate_header(struct Clemens2IMGDisk *disk, uint32_t format, const uint8_t *image,
                               const uint8_t *image_end, uint32_t image_data_offset) {
    uint32_t disk_size = (uint32_t)(image_end - image) - image_data_offset;

    strncpy(disk->creator, "CLEM", sizeof(disk->creator));

    /* validate that the input contains only sector data */
    if (format == CLEM_2IMG_FORMAT_PRODOS) {
        disk->block_count = disk_size / 512;
        if (disk_size % 512) {
            return false;
        }
    } else if (format == CLEM_2IMG_FORMAT_DOS) {
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
    if (format == CLEM_2IMG_FORMAT_DOS) {
        disk->dos_volume = 0xfe;
    } else {
        disk->dos_volume = 0x00;
    }
    disk->is_write_protected = true;
    disk->is_nibblized = false;
    return true;
}

static void _clem_nib_init_encoder(struct ClemensNibEncoder *encoder, struct ClemensNibbleDisk *nib,
                                   unsigned offset, unsigned size) {

    encoder->begin = nib->bits_data + offset;
    encoder->end = nib->bits_data + offset + size;
    encoder->bit_index = 0;
    encoder->bit_index_end = size * 8;
}

static bool _clem_nib_begin_track_encoder(struct ClemensNibEncoder *encoder,
                                          struct ClemensNibbleDisk *nib, unsigned nib_track_index,
                                          unsigned bits_data_offset, unsigned bits_data_size) {

    if (nib->bits_data + bits_data_offset + bits_data_size > nib->bits_data_end) {
        //  Out of space to write - this shouldn't happen.
        assert(false);
        return false;
    }

    encoder->begin = nib->bits_data + bits_data_offset;
    encoder->end = encoder->begin + bits_data_size;
    encoder->bit_index = 0;
    encoder->bit_index_end = bits_data_size * 8;
    nib->track_byte_offset[nib_track_index] = bits_data_offset;
    nib->track_byte_count[nib_track_index] = bits_data_size;
}

static void _clem_nib_end_track_encoder(struct ClemensNibEncoder *encoder,
                                        struct ClemensNibbleDisk *nib, unsigned nib_track_index) {
    //  TODO: use actual bits/bytes encoded vs the fixed amount per track
    //        (use encoder->bit_index at end of track)
    nib->track_bits_count[nib_track_index] = encoder->bit_index_end;
    nib->track_initialized[nib_track_index] = 1;
}

static void _clem_nib_write_bytes(struct ClemensNibEncoder *encoder, unsigned cnt, unsigned bit_cnt,
                                  uint8_t value) {
    uint8_t *nib_cur = encoder->begin + (encoder->bit_index / 8);
    unsigned bit_count = bit_cnt * cnt;
    unsigned nib_bit_index_end = encoder->bit_index + bit_count;
    unsigned bit_cnt_minus_1 = bit_cnt - 1;
    unsigned out_shift = 7 - (encoder->bit_index % 8);
    unsigned in_shift = 0;

    nib_bit_index_end %= encoder->bit_index_end;

    while (encoder->bit_index != nib_bit_index_end) {
        if (value & (1 << (bit_cnt_minus_1 - in_shift))) {
            nib_cur[0] |= (1 << out_shift);
        } else {
            nib_cur[0] &= ~(1 << out_shift);
        }
        encoder->bit_index = (encoder->bit_index + 1) % encoder->bit_index_end;
        in_shift = (in_shift + 1) % bit_cnt;
        out_shift = 7 - (encoder->bit_index % 8);
        nib_cur = encoder->begin + (encoder->bit_index / 8);
    }
}

static void _clem_nib_encode_self_sync_ff(struct ClemensNibEncoder *encoder, unsigned cnt) {
    _clem_nib_write_bytes(encoder, cnt, 10, 0xff);
}

static void _clem_nib_write_one(struct ClemensNibEncoder *encoder, uint8_t value) {
    _clem_nib_write_bytes(encoder, 1, 8, value);
}

static void _clem_nib_encode_one_6_2(struct ClemensNibEncoder *encoder, uint8_t value) {
    _clem_nib_write_one(encoder, gcr_6_2_byte[value & 0x3f]);
}

static void _clem_nib_encode_one_4_4(struct ClemensNibEncoder *encoder, uint8_t value) {
    /* all unused bits are set to '1', so 4x4 encoding to preserve odd bits
       requires shifting the bits to the right  */
    _clem_nib_write_one(encoder, (value >> 1) | 0xaa);
    /* even bits */
    _clem_nib_write_one(encoder, value | 0xaa);
}

static void _clem_nib_encode_prologue(struct ClemensNibEncoder *encoder) {
    _clem_nib_write_one(encoder, 0xd5);
    _clem_nib_write_one(encoder, 0xaa);
    _clem_nib_write_one(encoder, 0x96);
}

static void _clem_nib_encode_data_35(struct ClemensNibEncoder *encoder, const uint8_t *buf,
                                     unsigned cnt) {
    /* decoded bytes are encoded to GCR 6-2 8-bit bytes*/
    uint8_t scratch0[175], scratch1[175], scratch2[175];
    uint8_t data[524];
    unsigned chksum[3];
    unsigned data_idx = 0, scratch_idx = 0;
    uint8_t v;

    assert(cnt == 512);
    /* IIgs - 12 byte tag header is blank, but....
       TODO: what if it isn't??  */
    data_idx = 12;
    memset(data, 0, 12);
    memcpy(data + data_idx, buf, 512);

    data_idx = 0;

    /* split incoming decoded nibble data into parts for encoding into the
       final encoded buffer

       shamelessly translated from Ciderpress Nibble35.cpp as the encoding
       scheme is quite involved - you stand on the shoulders of giants.
    */
    chksum[0] = chksum[1] = chksum[2] = 0;
    while (data_idx < 524) {
        chksum[0] = (chksum[0] & 0xff) << 1;
        if (chksum[0] & 0x100) {
            ++chksum[0];
        }
        v = data[data_idx++];
        chksum[2] += v;
        if (chksum[0] > 0xff) {
            ++chksum[2];
            chksum[0] &= 0xff;
        }
        scratch0[scratch_idx] = (v ^ chksum[0]) & 0xff;
        v = data[data_idx++];
        chksum[1] += v;
        if (chksum[2] > 0xff) {
            ++chksum[1];
            chksum[2] &= 0xff;
        }
        scratch1[scratch_idx] = (v ^ chksum[2]) & 0xff;

        if (data_idx < 524) {
            v = data[data_idx++];
            chksum[0] += v;
            if (chksum[1] > 0xff) {
                ++chksum[0];
                chksum[1] &= 0xff;
            }
            scratch2[scratch_idx] = (v ^ chksum[1]) & 0xff;
            ++scratch_idx;
        }
    }
    scratch2[scratch_idx++] = 0;

    for (data_idx = 0; data_idx < scratch_idx; ++data_idx) {
        v = (scratch0[data_idx] & 0xc0) >> 2;
        v |= (scratch1[data_idx] & 0xc0) >> 4;
        v |= (scratch2[data_idx] & 0xc0) >> 6;
        _clem_nib_encode_one_6_2(encoder, v);
        _clem_nib_encode_one_6_2(encoder, scratch0[data_idx]);
        _clem_nib_encode_one_6_2(encoder, scratch1[data_idx]);
        if (data_idx < scratch_idx - 1) {
            _clem_nib_encode_one_6_2(encoder, scratch2[data_idx]);
        }
    }

    /* checksum */
    v = (chksum[0] & 0xc0) >> 6;
    v |= (chksum[1] & 0xc0) >> 4;
    v |= (chksum[2] & 0xc0) >> 2;
    _clem_nib_encode_one_6_2(encoder, v);
    _clem_nib_encode_one_6_2(encoder, chksum[2]);
    _clem_nib_encode_one_6_2(encoder, chksum[1]);
    _clem_nib_encode_one_6_2(encoder, chksum[0]);
}

#define CLEM_NIB_ENCODE_525_6_2_RIGHT_BUFFER_SIZE 86

static void _clem_nib_encode_data_525(struct ClemensNibEncoder *encoder, const uint8_t *buf,
                                      unsigned cnt) {
    /* cannot support anything by cnt = 256 bytes,  with 86 bytes to
       contain the 2 bits nibble = 324 bytes, which is the specified data chunk
       size of the sector on disk */
    uint8_t enc6[256];
    uint8_t enc2[86];
    uint8_t right;
    uint8_t chksum = 0;

    unsigned i6, i2;
    i6 = 0;
    for (i2 = CLEM_NIB_ENCODE_525_6_2_RIGHT_BUFFER_SIZE; i2 > 0;) { /* 256 */
        --i2;
        enc6[i6] = buf[i6] >> 2;    /* 6 bits */
        right = (buf[i6] & 1) << 1; /* lower 2 bits flipped */
        right |= (buf[i6] & 2) >> 1;
        enc2[i2] = right;
        ++i6;
    }
    for (i2 = CLEM_NIB_ENCODE_525_6_2_RIGHT_BUFFER_SIZE; i2 > 0;) { /* 170 */
        --i2;
        enc6[i6] = buf[i6] >> 2;    /* 6 bits */
        right = (buf[i6] & 1) << 1; /* lower 2 bits flipped */
        right |= (buf[i6] & 2) >> 1;
        enc2[i2] |= (right << 2);
        ++i6;
    }
    for (i2 = CLEM_NIB_ENCODE_525_6_2_RIGHT_BUFFER_SIZE; i2 > 2;) { /* 84 */
        --i2;
        enc6[i6] = buf[i6] >> 2;    /* 6 bits */
        right = (buf[i6] & 1) << 1; /* lower 2 bits flipped */
        right |= (buf[i6] & 2) >> 1;
        enc2[i2] |= (right << 4);
        ++i6;
    }
    assert(i6 == 256);

    for (i2 = CLEM_NIB_ENCODE_525_6_2_RIGHT_BUFFER_SIZE; i2 > 0;) {
        --i2;
        _clem_nib_encode_one_6_2(encoder, enc2[i2] ^ chksum);
        chksum = enc2[i2];
    }
    for (i6 = 0; i6 < 256; ++i6) {
        _clem_nib_encode_one_6_2(encoder, enc6[i6] ^ chksum);
        chksum = enc6[i6];
    }

    _clem_nib_encode_one_6_2(encoder, chksum);
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

typedef const unsigned (*_ClemensPhysicalSectorMap)[16];

static _ClemensPhysicalSectorMap get_logical_sector_map_for_disk(struct Clemens2IMGDisk *disk) {
    switch (disk->format) {
    case CLEM_2IMG_FORMAT_PRODOS:
        if (disk->nib->disk_type == CLEM_DISK_TYPE_3_5) {
            return prodos_to_logical_sector_map_35;
        } else {
            return prodos_to_logical_sector_map_525;
        }
        break;
    case CLEM_2IMG_FORMAT_DOS:
        assert(disk->nib->disk_type == CLEM_DISK_TYPE_5_25);
        return dos_to_logical_sector_map_525;
        break;
    case CLEM_2IMG_FORMAT_RAW:
    default:
        assert(false);
        break;
    }
    return NULL;
}

static bool _clem_2img_nibblize_data_35(struct Clemens2IMGDisk *disk) {
    _ClemensPhysicalSectorMap to_logical_sector_map;
    unsigned disk_data_size = (unsigned)(disk->data_end - disk->data);
    unsigned disk_region;
    unsigned qtr_track_index;
    unsigned track_byte_offset;
    unsigned qtr_tracks_per_track;
    unsigned logical_sector_index;

    if (disk->nib->disk_type != CLEM_DISK_TYPE_3_5)
        return false;

    //  All 3.5 disks must conform to one of two sizes 400K or 800K, reflected
    //  by their "sided-ness"
    if (disk->block_count > 0) {
        if (disk->block_count == CLEM_DISK_35_PRODOS_BLOCK_COUNT) {
            disk->nib->is_double_sided = false;
        } else if (disk->block_count == CLEM_DISK_35_DOUBLE_PRODOS_BLOCK_COUNT) {
            disk->nib->is_double_sided = true;
        } else {
            return false;
        }
    } else if (disk_data_size == CLEM_DISK_35_PRODOS_BLOCK_COUNT * 512) {
        disk->nib->is_double_sided = false;
    } else if (disk_data_size == CLEM_DISK_35_DOUBLE_PRODOS_BLOCK_COUNT * 512) {
        disk->nib->is_double_sided = true;
    } else {
        return false;
    }

    disk->nib->bit_timing_ns = CLEM_DISK_3_5_BIT_TIMING_NS;
    disk->nib->track_count = disk->nib->is_double_sided ? 160 : 80;

    if (disk->nib->is_double_sided) {
        qtr_tracks_per_track = 1;
    } else {
        qtr_tracks_per_track = 2;
    }

    /* The various self-sync gaps between sectors are derived from the
       ProDOS firmware format method.  See clem_disk.h for details.  */
    disk_region = 0;          // 3.5" disk tracks are divided into regions
    track_byte_offset = 0;    // Offset into nib bits data
    logical_sector_index = 0; // Sector from 0 to 800/1600 on disk
    to_logical_sector_map = get_logical_sector_map_for_disk(disk);
    qtr_track_index = 0;
    while (qtr_track_index < CLEM_DISK_LIMIT_QTR_TRACKS) {
        unsigned track_sector_count = g_clem_max_sectors_per_region_35[disk_region];
        unsigned track_bytes_count = CLEM_DISK_35_CALC_BYTES_FROM_SECTORS(track_sector_count);
        //  TRK 0: (0,1) , TRK 1: (2,3), and so on. and track encoded
        unsigned logical_track_index = qtr_track_index / 2;
        unsigned logical_side_index = qtr_track_index % 2;
        unsigned nib_track_index = qtr_track_index / qtr_tracks_per_track;
        uint8_t side_index_and_track_64 = (logical_side_index << 5) | (logical_track_index >> 6);
        uint8_t sector_format = (disk->nib->is_double_sided ? 0x20 : 0x00) | 0x2;
        // DOS/ProDOS sector index
        unsigned sector;
        unsigned temp;

        struct ClemensNibEncoder nib_encoder;

        if (!_clem_nib_begin_track_encoder(&nib_encoder, disk->nib, nib_track_index,
                                           track_byte_offset, track_bytes_count))
            return false;

        _clem_nib_write_one(&nib_encoder, 0xff);
        _clem_nib_encode_self_sync_ff(&nib_encoder, (CLEM_DISK_35_BYTES_TRACK_GAP_1 * 8) / 10);

        //  populate track with sectors in OS order,
        //  3.5" disk sector data is by definition 512 bytes

        for (sector = 0; sector < track_sector_count; sector++) {
            unsigned logical_sector = to_logical_sector_map[disk_region][sector];
            const uint8_t *source_data = disk->data + (logical_sector_index + logical_sector) * 512;
            _clem_nib_write_one(&nib_encoder, 0xff);
            //  ADDRESS (prologue, header, epilogue) note the combined address
            //  segment differs from the 5.25" version
            //  track, sector, side, format (0x12 or 0x22 or 0x14 or 0x24)
            //      format = sides | interleave where interleave should always be 2
            //
            _clem_nib_write_one(&nib_encoder, 0xd5);
            _clem_nib_write_one(&nib_encoder, 0xaa);
            _clem_nib_write_one(&nib_encoder, 0x96);
            _clem_nib_encode_one_6_2(&nib_encoder, (uint8_t)(logical_track_index & 0xff));
            _clem_nib_encode_one_6_2(&nib_encoder, (uint8_t)(logical_sector & 0xff));
            _clem_nib_encode_one_6_2(&nib_encoder, (uint8_t)(side_index_and_track_64 & 0xff));
            _clem_nib_encode_one_6_2(&nib_encoder, sector_format);
            temp = (logical_track_index ^ logical_sector ^ side_index_and_track_64 ^ 0x24);
            _clem_nib_encode_one_6_2(&nib_encoder, (uint8_t)(temp));
            _clem_nib_write_one(&nib_encoder, 0xde);
            _clem_nib_write_one(&nib_encoder, 0xaa);
            _clem_nib_write_one(&nib_encoder, 0xff);
            //  SELF-SYNC
            _clem_nib_encode_self_sync_ff(&nib_encoder, 4);
            _clem_nib_write_one(&nib_encoder, 0xff);
            //  DATA
            _clem_nib_write_one(&nib_encoder, 0xd5);
            _clem_nib_write_one(&nib_encoder, 0xaa);
            _clem_nib_write_one(&nib_encoder, 0xad);
            _clem_nib_encode_one_6_2(&nib_encoder, (uint8_t)logical_sector);
            _clem_nib_encode_data_35(&nib_encoder, source_data, 512);
            _clem_nib_write_one(&nib_encoder, 0xde);
            _clem_nib_write_one(&nib_encoder, 0xaa);
            if (sector + 1 < track_sector_count) {
                //  all but the last sector of this track
                _clem_nib_write_one(&nib_encoder, 0xff);
                _clem_nib_write_one(&nib_encoder, 0xff);
                _clem_nib_write_one(&nib_encoder, 0xff);
                _clem_nib_encode_self_sync_ff(&nib_encoder,
                                              (CLEM_DISK_35_BYTES_TRACK_GAP_3 * 8) / 10);
            }
        }
        _clem_nib_end_track_encoder(&nib_encoder, disk->nib, nib_track_index);

        disk->nib->meta_track_map[qtr_track_index] = nib_track_index;
        if (qtr_tracks_per_track == 2) {
            //  TODO: treated as empty?  or should we point to nib_track_index?
            //        investigate
            disk->nib->meta_track_map[qtr_track_index + 1] = 0xff;
        }
        logical_sector_index += track_sector_count;
        qtr_track_index += qtr_tracks_per_track;
        if (qtr_track_index >= g_clem_track_start_per_region_35[disk_region + 1]) {
            disk_region++;
        }
        track_byte_offset += track_bytes_count;
    }
    return true;
}

static bool _clem_2img_nibblize_data_525(struct Clemens2IMGDisk *disk) {
    _ClemensPhysicalSectorMap to_logical_sector_map;
    unsigned track_index;
    unsigned track_byte_offset;
    unsigned logical_sector_index;

    if (disk->nib->disk_type != CLEM_DISK_TYPE_5_25)
        return false;

    disk->nib->bit_timing_ns = CLEM_DISK_5_25_BIT_TIMING_NS;
    disk->nib->track_count = 35;
    disk->nib->is_double_sided = false;

    track_byte_offset = 0;    // offset into nib bits data
    logical_sector_index = 0; // 256 byte sector index from 0 to 559
    to_logical_sector_map = get_logical_sector_map_for_disk(disk);

    track_index = 0;
    while (track_index < CLEM_DISK_LIMIT_525_DISK_TRACKS) {
        // 5.25" tracks are grouped like so on the quarter track list (per WOZ spec)
        // QTR: |00| 01 02 03 |04| 05 06 07 |08| 09 0A 0B |0C| 0D ..
        // TRK: |00| 00 FF 01 |01| 01 FF 02 |02| 02 FF 03 |03| 03 ..
        // basically straggling the track at qtr_track_index 0, 4, 8, C.
        // sectors are 256 bytes as well (vs 512 for 3.5" disks)
        unsigned sector;

        struct ClemensNibEncoder nib_encoder;
        if (!_clem_nib_begin_track_encoder(&nib_encoder, disk->nib, track_index, track_byte_offset,
                                           CLEM_DISK_525_BYTES_PER_TRACK))
            return false;

        _clem_nib_encode_self_sync_ff(&nib_encoder, CLEM_DISK_525_BYTES_TRACK_GAP_1);

        for (sector = 0; sector < CLEM_DISK_525_NUM_SECTORS_PER_TRACK; sector++) {
            unsigned logical_sector = to_logical_sector_map[0][sector];
            const uint8_t *source_data = disk->data + (logical_sector_index + logical_sector) * 256;
            // Sector Address Prologue + Body + Epilogue
            //  - The sector written here is the physical sector vs logical?  Ciderpress
            //    AppleWin, etc seem to imply this.  This differs from the 3.5" address
            _clem_nib_write_one(&nib_encoder, 0xd5);
            _clem_nib_write_one(&nib_encoder, 0xaa);
            _clem_nib_write_one(&nib_encoder, 0x96);
            _clem_nib_encode_one_4_4(&nib_encoder, (uint8_t)(disk->dos_volume & 0xff));
            _clem_nib_encode_one_4_4(&nib_encoder, track_index);
            _clem_nib_encode_one_4_4(&nib_encoder, sector);
            _clem_nib_encode_one_4_4(&nib_encoder,
                                     (uint8_t)(disk->dos_volume ^ track_index ^ sector));
            _clem_nib_write_one(&nib_encoder, 0xde);
            _clem_nib_write_one(&nib_encoder, 0xaa);
            _clem_nib_write_one(&nib_encoder, 0xeb);
            // GAP 2
            _clem_nib_encode_self_sync_ff(&nib_encoder, CLEM_DISK_525_BYTES_TRACK_GAP_2);
            // Sector Data Prologue + Body + Epilogue
            _clem_nib_write_one(&nib_encoder, 0xd5);
            _clem_nib_write_one(&nib_encoder, 0xaa);
            _clem_nib_write_one(&nib_encoder, 0xad);
            _clem_nib_encode_data_525(&nib_encoder, source_data, 256);
            _clem_nib_write_one(&nib_encoder, 0xde);
            _clem_nib_write_one(&nib_encoder, 0xaa);
            _clem_nib_write_one(&nib_encoder, 0xeb);
            if (sector + 1 < CLEM_DISK_525_NUM_SECTORS_PER_TRACK) {
                _clem_nib_encode_self_sync_ff(&nib_encoder, CLEM_DISK_525_BYTES_TRACK_GAP_3);
            }
        }
        _clem_nib_end_track_encoder(&nib_encoder, disk->nib, track_index);

        if (track_index != 0) {
            disk->nib->meta_track_map[track_index * 4 - 1] = track_index;
        }
        disk->nib->meta_track_map[track_index * 4] = track_index;
        if (track_index < CLEM_DISK_LIMIT_525_DISK_TRACKS) {
            disk->nib->meta_track_map[track_index * 4 + 1] = track_index;
        }
        logical_sector_index += CLEM_DISK_525_NUM_SECTORS_PER_TRACK;
        track_byte_offset += CLEM_DISK_525_BYTES_PER_TRACK;
        track_index++;
    }
    return true;
}

bool clem_2img_nibblize_data(struct Clemens2IMGDisk *disk) {
    memset(disk->nib->meta_track_map, 0xff, sizeof(disk->nib->meta_track_map));
    memset(disk->nib->track_bits_count, 0x00, sizeof(disk->nib->track_bits_count));
    memset(disk->nib->track_byte_count, 0x00, sizeof(disk->nib->track_byte_count));
    memset(disk->nib->track_initialized, 0x00, sizeof(disk->nib->track_initialized));
    switch (disk->nib->disk_type) {
    case CLEM_DISK_TYPE_5_25:
        return _clem_2img_nibblize_data_35(disk);
    case CLEM_DISK_TYPE_3_5:
        return _clem_2img_nibblize_data_525(disk);
    }
    return false;
}

unsigned clem_2img_encode_nibblized_disk(struct Clemens2IMGDisk *disk, uint8_t *data_start,
                                         uint8_t *data_end, const struct ClemensNibbleDisk *nib,
                                         bool *corrupted) {
    // from the input nibbles,
    // write out the encoded bytes track by track to [data_start, data_end]
    // set disk->data and disk->data_end to data_start + encoded_size

    // sector ordering is important for DOS vs ProDOS images and we'll use the
    // 2IMG disk format to decide on the output format.

    // errors may occur because the nibbles have been corrupted (somehow)
    // and as mentioned in the documentation, should be saved out as a WOZ file

    // different regions have different sector counts
    unsigned num_track_regions = 0;
    unsigned clem_max_sectors_per_region[CLEM_DISK_35_NUM_REGIONS];
    unsigned clem_track_start_per_region[CLEM_DISK_35_NUM_REGIONS + 1];
    unsigned i, qtr_track_index;

    //  iterate through all of the tracks.
    switch (nib->disk_type) {
    case CLEM_DISK_TYPE_3_5:
        num_track_regions = CLEM_DISK_35_NUM_REGIONS;
        for (i = 0; i < num_track_regions; ++i) {
            clem_max_sectors_per_region[i] = g_clem_max_sectors_per_region_35[i];
            clem_track_start_per_region[i] = g_clem_track_start_per_region_35[i];
        }
        clem_track_start_per_region[num_track_regions] =
            (g_clem_track_start_per_region_35[num_track_regions]);
        break;
    case CLEM_DISK_TYPE_5_25:
        num_track_regions = 1;
        clem_max_sectors_per_region[0] = CLEM_DISK_525_NUM_SECTORS_PER_TRACK;
        clem_track_start_per_region[0] = 0;
        clem_track_start_per_region[num_track_regions] = CLEM_DISK_LIMIT_QTR_TRACKS;
        break;
    }

    //  Spin through each valid track and scan for bytes in the track's bitstream
    //  Input sectors are in their physical order since the nibblized disk
    //      represents the actual disk stream as bits are read from a head.
    //  Output sectors in their logical order (i.e. DOS vs ProDOS ordering)
    //
    if (nib->disk_type == CLEM_DISK_TYPE_3_5) {
        uint8_t actual_track_index = 0xff;
        for (qtr_track_index = 0; qtr_track_index < CLEM_DISK_LIMIT_QTR_TRACKS; ++qtr_track_index) {
            //  analagous with a Woz TMAP lookup
            if (nib->meta_track_map[qtr_track_index] == actual_track_index)
                continue;
            //  index into the actual track table (i.e Woz TRKS)
            actual_track_index = nib->meta_track_map[qtr_track_index];
            //
        }
    }
    // clem_2img_encoded_nibblized_track();
    return 0;
}
