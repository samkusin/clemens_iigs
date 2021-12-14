#include "clem_2img.h"
#include "external/cross_endian.h"

#include <string.h>
#include <assert.h>

#define CLEM_2IMG_DISK_TYPE_UNKNOWN     0
#define CLEM_2IMG_DISK_TYPE_5_25        1
#define CLEM_2IMG_DISK_TYPE_3_5         2

#define CLEM_2IMG_DISK_SIZE_5_25        (140 * 1024)
#define CLEM_2IMG_DISK_SIZE_3_5         (800 * 1024)


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

static const uint8_t gcr_6_2_byte[64] = {
	0x96,   0x97,   0x9a,   0x9b,   0x9d,   0x9e,   0x9f,   0xa6,
	0xa7,   0xab,   0xac,   0xad,   0xae,   0xaf,   0xb2,   0xb3,
	0xb4,   0xb5,   0xb6,   0xb7,   0xb9,   0xba,   0xbb,   0xbc,
	0xbd,   0xbe,   0xbf,   0xcb,   0xcd,   0xce,   0xcf,   0xd3,
	0xd6,   0xd7,   0xd9,   0xda,   0xdb,   0xdc,   0xdd,   0xde,
	0xdf,   0xe5,   0xe6,   0xe7,   0xe9,   0xea,   0xeb,   0xec,
	0xed,   0xee,   0xef,   0xf2,   0xf3,   0xf4,   0xf5,   0xf6,
	0xf7,   0xf9,   0xfa,   0xfb,   0xfc,   0xfd,   0xfe,   0xff
};

static const unsigned prodos_to_logical_sector_map_525[1][16] = {{
    0,  8,  1,  9,  2,  10, 3,  11, 4,  12, 5,  13, 6,  14, 7,  15
}};

/* only support 16 sector tracks */
static const unsigned dos_to_logical_sector_map_525[1][16] = {{
    0,  7, 14,  6,  13, 5, 12,  4,  11, 3,  10, 2,  9,  1,  8,  15
}};

/* 3.5" drives have 512 byte sectors (ProDOS assumed) */
static const unsigned prodos_to_logical_sector_map_35[CLEM_DISK_35_NUM_REGIONS][16] = {{
    0,  6,  1,  7,  2,  8,  3,  9,  4,  10, 5,  11, -1, -1, -1, -1
}, {
    0,  6,  1,  7,  2,  8,  3,  9,  4,  10, 5,  -1, -1, -1, -1, -1
}, {
    0,  5,  1,  6,  2,  7,  3,  8,  4,   9, -1, -1, -1, -1, -1, -1
}, {
    0,  5,  1,  6,  2,  7,  3,  8,  4,  -1, -1, -1, -1, -1, -1, -1
}, {
    0,  4,  1,  5,  2,  6,  3,  7, -1,  -1, -1, -1, -1, -1, -1, -1
}};



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
    uint32_t param32;
    disk->image_buffer = data;
    disk->image_buffer_length = data_end - data;

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
                    disk->is_nibblized = false;
                    param32 = _decode_u32(data);
                    if (param32 & 0x80000000) {
                        disk->is_write_protected = true;
                    }
                    if (disk->format == CLEM_2IMG_FORMAT_DOS) {
                        if (param32 & 0x100) {
                            disk->dos_volume = param32 & 0xff;
                        } else {
                            disk->dos_volume = 254;;
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
                    disk->image_data_offset = _decode_u32(data);
                    disk->data = disk->image_buffer + disk->image_data_offset;
                    ++state;
                } else {
                    return false;
                }
                break;
            case 8:             // data_end - data = size
                data_size = _increment_data_ptr(data, 4, data_end);
                if (data_size == 4) {
                    param32 = _decode_u32(data);
                    if (param32 == 0) {
                        //  this is possible for prodos images
                        //  use blocks * 512
                        param32 = disk->block_count * 512;
                    }
                    if (param32> 0x000c8000) {
                        //  todo check if we need to convert to le?
                        //  what disk images actually do this?
                        assert(false);
                    }
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
                    disk->comment = disk->image_buffer + _decode_u32(data);
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
                    disk->creator_data = disk->image_buffer + _decode_u32(data);
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

bool clem_2img_generate_header(struct Clemens2IMGDisk* disk, uint32_t format,
                               uint8_t* image, uint8_t* image_end) {
    uint32_t disk_size = (uint32_t)(image_end - image);

    strncpy(disk->creator, "CLEM", sizeof(disk->creator));

    /* validate that the input contains only sector data */
    if (format == CLEM_2IMG_FORMAT_PRODOS) {
        disk->block_count = disk_size / 512;
        if (disk_size % 512) {
            return false;
        }
    } else {
        disk->block_count = 0;
        if (disk_size % 256) {
            return false;
        }
    }

    /* TODO: support creator data and comments */
    disk->image_buffer = image;
    disk->image_buffer_length = disk_size;
    disk->image_data_offset = 0;

    disk->data = disk->image_buffer + disk->image_data_offset;
    disk->data_end = disk->data + disk_size;

    disk->creator_data = (char *)image + disk_size;
    disk->creator_data_end = disk->creator_data;
    disk->comment = (char *)image + disk_size;
    disk->comment_end = disk->comment;

    disk->version = 0x0001;
    disk->format = format;
    disk->dos_volume = 0x00;
    disk->is_write_protected = true;
    disk->is_nibblized = false;
    return true;
}


static void _clem_nib_init_encoder(
    struct ClemensNibEncoder* encoder,
    uint8_t* begin,
    uint8_t* end
) {

    encoder->begin = begin;
    encoder->end = end;
    encoder->bit_index = 0;
    encoder->bit_index_end = (end - begin) * 8;
}

static void _clem_nib_write_bytes(
    struct ClemensNibEncoder* encoder,
    unsigned cnt,
    unsigned bit_cnt,
    uint8_t value
) {
    uint8_t* nib_cur = encoder->begin + (encoder->bit_index / 8);
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


static void _clem_nib_encode_self_sync_ff(
    struct ClemensNibEncoder* encoder,
    unsigned cnt
) {
    _clem_nib_write_bytes(encoder, cnt, 10, 0xff);
}

static void _clem_nib_write_one(
    struct ClemensNibEncoder* encoder,
    uint8_t value
) {
    _clem_nib_write_bytes(encoder, 1, 8, value);
}

static void _clem_nib_encode_one_6_2(
    struct ClemensNibEncoder* encoder,
    uint8_t value
) {
    _clem_nib_write_one(encoder, gcr_6_2_byte[value & 0x3f]);
}

static void _clem_nib_encode_one_4_4(
    struct ClemensNibEncoder* encoder,
    uint8_t value
) {
    /* all unused bits are set to '1', so 4x4 encoding to preserve odd bits
       requires shifting the bits to the right  */
    _clem_nib_write_one(encoder, (value >> 1) | 0xaa);
    /* even bits */
    _clem_nib_write_one(encoder, value | 0xaa);
}


static void _clem_nib_encode_data_35(
    struct ClemensNibEncoder* encoder,
    const uint8_t* buf,
    unsigned cnt
) {
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

#define CLEM_NIB_ENCODE_525_6_2_RIGHT_BUFFER_SIZE   86

static void _clem_nib_encode_data_525(
    struct ClemensNibEncoder* encoder,
    const uint8_t* buf,
    unsigned cnt
) {
    /* cannot support anything by cnt = 256 bytes,  with 86 bytes to
       contain the 2 bits nibble = 324 bytes, which is the specified data chunk
       size of the sector on disk */
    uint8_t enc6[256];
    uint8_t enc2[86];
    uint8_t right;
    uint8_t chksum = 0;

    unsigned i6, i2;
    i6 = 0;
    for (i2 = CLEM_NIB_ENCODE_525_6_2_RIGHT_BUFFER_SIZE; i2 > 0; ) {  /* 256 */
        --i2;
        enc6[i6] = buf[i6] >> 2;            /* 6 bits */
        right = (buf[i6] & 1) << 1;         /* lower 2 bits flipped */
        right |= (buf[i6] & 2) >> 1;
        enc2[i2] = right;
        ++i6;
    }
    for (i2 = CLEM_NIB_ENCODE_525_6_2_RIGHT_BUFFER_SIZE; i2 > 0; ) {  /* 170 */
        --i2;
        enc6[i6] = buf[i6] >> 2;            /* 6 bits */
        right = (buf[i6] & 1) << 1;         /* lower 2 bits flipped */
        right |= (buf[i6] & 2) >> 1;
        enc2[i2] |= (right << 2);
        ++i6;
    }
    for (i2 = CLEM_NIB_ENCODE_525_6_2_RIGHT_BUFFER_SIZE; i2 > 2; ) {  /* 84 */
        --i2;
        enc6[i6] = buf[i6] >> 2;            /* 6 bits */
        right = (buf[i6] & 1) << 1;         /* lower 2 bits flipped */
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
bool clem_2img_nibblize_data(
    struct Clemens2IMGDisk* disk
) {
    unsigned clem_max_sectors_per_region[CLEM_DISK_35_NUM_REGIONS];
    unsigned clem_track_start_per_region[CLEM_DISK_35_NUM_REGIONS + 1];
    unsigned self_sync_gap_1_cnt = 0;
    unsigned self_sync_gap_2_cnt = 0;
    unsigned self_sync_gap_3_cnt = 0;
    const unsigned (*to_logical_sector_map)[16] = NULL;
    unsigned num_regions = 0, current_region = 0;
    unsigned track_increment = 0;
    unsigned track_byte_offset;
    unsigned logical_sector_index;
    unsigned i, qtr_track_index;
    uint8_t* nib_begin = disk->nib->bits_data;
    uint8_t* nib_out = nib_begin;
    unsigned data_in_size = disk->data_end - disk->data;
    unsigned in_sector_size;

    /* disk->nib->disk_type must be set before this call
       gap values are derived from a mix of Beneath Apple DOS and ProDOS
            sources and Ciderpress mentions (the 3.5" equivalents, which are
            *very* hard to derive from existing documentation) */
    switch (disk->nib->disk_type) {
        case CLEM_DISK_TYPE_3_5:
            if (disk->block_count > 0) {
                if (disk->block_count == 280) {
                    return false;
                } else if (disk->block_count == 800) {
                    disk->nib->is_double_sided = false;
                } else if (disk->block_count == 1600) {
                    disk->nib->is_double_sided = true;
                } else {
                    return false;
                }
            } else if (data_in_size == 800 * 1024) {
                disk->nib->is_double_sided = true;
            } else {
                return false;
            }
            /* guesswork based on gap 3 size of 36 10-bit bytes and the  */
            self_sync_gap_1_cnt = (CLEM_DISK_35_BYTES_TRACK_GAP_1 * 8) / 10;
            self_sync_gap_2_cnt = 4;
            self_sync_gap_3_cnt = (CLEM_DISK_35_BYTES_TRACK_GAP_3 * 8) / 10;
            num_regions = CLEM_DISK_35_NUM_REGIONS;
            for (i = 0; i < num_regions; ++i ) {
                clem_max_sectors_per_region[i] = g_clem_max_sectors_per_region_35[i];
                clem_track_start_per_region[i] = g_clem_track_start_per_region_35[i];
            }
            clem_track_start_per_region[num_regions] = (
                g_clem_track_start_per_region_35[num_regions]);
            /* 80 tracks for single sided - is this ever true?, otherwise the
               full track listing for double sided 3.5" disks */
            disk->nib->bit_timing_ns = 2000;
            disk->nib->track_count = disk->nib->is_double_sided ? 160 : 80;
            track_increment = disk->nib->is_double_sided ? 1 : 2;
            in_sector_size = 512;
            break;
        case CLEM_DISK_TYPE_5_25:
            num_regions = 1;
            clem_max_sectors_per_region[0] = CLEM_DISK_525_NUM_SECTORS_PER_TRACK;
            clem_track_start_per_region[0] = 0;
            clem_track_start_per_region[1] = CLEM_DISK_LIMIT_QTR_TRACKS;
            /* From Beneath Apple DOS/ProDOS - evaluate if DOS values should
               reflect those from Beneath Apple DOS. - anyway these are taken
               from Ciderpress and ROM 03 ProDOS block formatting disassembly */
            self_sync_gap_1_cnt = 64;           /* somewhere between 12-85 */
            self_sync_gap_2_cnt = 6;            /* somewhere between 5- 10 */
            self_sync_gap_3_cnt = 24;           /* somewhere between 16-28 */
             /* 40 tracks total - always 35 for DOS/ProDOS disks */
            disk->nib->bit_timing_ns = 4000;
            disk->nib->track_count = 35;
            track_increment = 4;
            in_sector_size = 256;
            break;

        default:
            return false;
    }

    switch (disk->format) {
        case CLEM_2IMG_FORMAT_PRODOS:
            if (disk->nib->disk_type == CLEM_DISK_TYPE_3_5) {
                to_logical_sector_map = prodos_to_logical_sector_map_35;
            } else {
                to_logical_sector_map = prodos_to_logical_sector_map_525;
            }
            break;
        case CLEM_2IMG_FORMAT_DOS:
            assert(disk->nib->disk_type == CLEM_DISK_TYPE_5_25);
            to_logical_sector_map = dos_to_logical_sector_map_525;
            break;
        case CLEM_2IMG_FORMAT_RAW:
            assert(false);
        default:
            return false;
    }

    /* encode nibbles using the specified image format layout */
    track_byte_offset = 0;
    logical_sector_index = 0;
    /* clear out meta track map to its defaults, so that we can simply assign
       track_indices to the meta track map for 3.5 and 5.25 disks versus
       adding extra conditionals in the below loop
    */
    memset(disk->nib->meta_track_map, 0xff, sizeof(disk->nib->meta_track_map));
    memset(disk->nib->track_bits_count, 0x00, sizeof(disk->nib->track_bits_count));
    memset(disk->nib->track_byte_count, 0x00, sizeof(disk->nib->track_byte_count));
    memset(disk->nib->track_initialized, 0x00, sizeof(disk->nib->track_initialized));
    for (qtr_track_index = 0; qtr_track_index < CLEM_DISK_LIMIT_QTR_TRACKS;
    ) {
        //  isolate into a function for readability
        unsigned sector_count = clem_max_sectors_per_region[current_region];
        unsigned track_size;
        uint8_t* data_in;
        unsigned sector_index;
        unsigned in_sector_index;
        unsigned logical_track_index = qtr_track_index / 2;
        unsigned side_index = qtr_track_index & 1;
        unsigned nib_track_index = qtr_track_index / track_increment;
        unsigned temp, byte_index;
        struct ClemensNibEncoder nib_encoder;

        if (disk->nib->disk_type == CLEM_DISK_TYPE_3_5) {
            track_size = CLEM_DISK_35_CALC_BYTES_FROM_SECTORS(sector_count);
        } else {
            track_size = CLEM_DISK_525_BYTES_PER_TRACK;
        }

        if (disk->nib->bits_data + track_byte_offset + track_size > disk->nib->bits_data_end) {
            assert(false);
            return false;
        }

        disk->nib->track_byte_offset[nib_track_index] = track_byte_offset;
        disk->nib->track_byte_count[nib_track_index] = track_size;

        /* see clem_disk.h for documentation on the 3.5" prodos format */
        _clem_nib_init_encoder(&nib_encoder, disk->nib->bits_data + track_byte_offset,
            disk->nib->bits_data + track_byte_offset + track_size);

        /* pad */
        if (disk->nib->disk_type == CLEM_DISK_TYPE_3_5) {
            _clem_nib_write_one(&nib_encoder, 0xff);
        } else {
            logical_track_index /= 2;               // tracks 0 - 39
        }

        _clem_nib_encode_self_sync_ff(&nib_encoder, self_sync_gap_1_cnt);

        for (sector_index = 0; sector_index < sector_count; ++sector_index) {
            in_sector_index = to_logical_sector_map[current_region][sector_index];
            data_in = disk->data + (logical_sector_index + in_sector_index) * in_sector_size;

            if (disk->nib->disk_type == CLEM_DISK_TYPE_3_5) {
                _clem_nib_write_one(&nib_encoder, 0xff);
            }
            /* Address */
            _clem_nib_write_one(&nib_encoder, 0xd5);
            _clem_nib_write_one(&nib_encoder, 0xaa);
            _clem_nib_write_one(&nib_encoder, 0x96);
            if (disk->nib->disk_type == CLEM_DISK_TYPE_3_5) {
                /* track, sector, side, format (0x22 or 0x24 - assume 0x24?) */
                /* I THINK 0x22 = 512 byte sectors */
                /*       - 0x24 is 524 byte sectors to allow for the 12 byte
                                empty tag
                */
                unsigned side_index_35 = (side_index << 5) | (logical_track_index >> 6);

                _clem_nib_encode_one_6_2(&nib_encoder, (uint8_t)(logical_track_index));
                _clem_nib_encode_one_6_2(&nib_encoder, (uint8_t)(in_sector_index & 0xff));
                _clem_nib_encode_one_6_2(&nib_encoder, (uint8_t)(side_index_35));
                _clem_nib_encode_one_6_2(&nib_encoder, 0x24);
                temp = (logical_track_index ^ in_sector_index ^ side_index_35 ^ 0x24);
                _clem_nib_encode_one_6_2(&nib_encoder, (uint8_t)(temp));
                _clem_nib_write_one(&nib_encoder, 0xde);
                _clem_nib_write_one(&nib_encoder, 0xaa);
                _clem_nib_write_one(&nib_encoder, 0xff);
            } else {
                _clem_nib_encode_one_4_4(&nib_encoder, (uint8_t)(disk->dos_volume & 0xff));
                _clem_nib_encode_one_4_4(&nib_encoder, logical_track_index);
                _clem_nib_encode_one_4_4(&nib_encoder, sector_index);
                _clem_nib_encode_one_4_4(&nib_encoder,
                    (uint8_t)(disk->dos_volume ^ logical_track_index ^ sector_index));
                _clem_nib_write_one(&nib_encoder, 0xde);
                _clem_nib_write_one(&nib_encoder, 0xaa);
                _clem_nib_write_one(&nib_encoder, 0xeb);
            }

            _clem_nib_encode_self_sync_ff(&nib_encoder, self_sync_gap_2_cnt);

            /* Data */
            if (disk->nib->disk_type == CLEM_DISK_TYPE_3_5) {
                _clem_nib_write_one(&nib_encoder, 0xff);
            }
            _clem_nib_write_one(&nib_encoder, 0xd5);
            _clem_nib_write_one(&nib_encoder, 0xaa);
            _clem_nib_write_one(&nib_encoder, 0xad);
            if (disk->nib->disk_type == CLEM_DISK_TYPE_3_5) {
                _clem_nib_encode_one_6_2(&nib_encoder, (uint8_t)in_sector_index);
                /* TODO: handle cases where input sector data is 524 vs 512 */
                _clem_nib_encode_data_35(&nib_encoder, data_in, 512);
                _clem_nib_write_one(&nib_encoder, 0xde);
                _clem_nib_write_one(&nib_encoder, 0xaa);
                if (sector_index < sector_count - 1) {
                    _clem_nib_write_one(&nib_encoder, 0xff);
                    _clem_nib_write_one(&nib_encoder, 0xff);
                    _clem_nib_write_one(&nib_encoder, 0xff);
                }
            } else {
                _clem_nib_encode_data_525(&nib_encoder, data_in, 256);
                _clem_nib_write_one(&nib_encoder, 0xde);
                _clem_nib_write_one(&nib_encoder, 0xaa);
                _clem_nib_write_one(&nib_encoder, 0xeb);
            }
            if (sector_index + 1 < sector_count) {
                _clem_nib_encode_self_sync_ff(&nib_encoder, self_sync_gap_3_cnt);
            }
        }
        logical_sector_index += sector_count;

        disk->nib->track_bits_count[nib_track_index] = nib_encoder.bit_index_end;
        disk->nib->track_initialized[nib_track_index] = 1;
        if (disk->nib->disk_type == CLEM_DISK_TYPE_3_5) {
            disk->nib->meta_track_map[qtr_track_index] = nib_track_index;
        } else {
            disk->nib->meta_track_map[qtr_track_index] = nib_track_index;
            if (qtr_track_index > 0) {
                /* track is copied onto the one quarter track before and after
                   this qtr track */
                disk->nib->meta_track_map[qtr_track_index - 1] = nib_track_index;
            }
            disk->nib->meta_track_map[
                (qtr_track_index + 1) % CLEM_DISK_LIMIT_QTR_TRACKS] = nib_track_index;
        }

        if (nib_track_index + 1 >= disk->nib->track_count) {
            track_increment = CLEM_DISK_LIMIT_QTR_TRACKS - nib_track_index;
        }

        qtr_track_index += track_increment;
        if (qtr_track_index >= clem_track_start_per_region[current_region + 1]) {
            ++current_region;
        }
        track_byte_offset += track_size;
    }


    return true;
}
