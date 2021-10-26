#include "clem_2img.h"
#include "contrib/cross_endian.h"

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
}}
;
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
                    disk->image_data_length = _decode_u32(data);
                    if (disk->image_data_length == 0) {
                        //  this is possible for prodos images
                        //  use blocks * 512
                        disk->image_data_length = disk->block_count * 512;
                    }
                    if (disk->image_data_length > 0x000c8000) {
                        //  todo check if we need to convert to le?
                        //  what disk images actually do this?
                        assert(false);
                    }
                    disk->data_end = disk->data + disk->image_data_length;
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

static void _clem_nib_init_encoder(
    struct ClemensNibEncoder* encoder,
    uint8_t* start,
    uint8_t* end
) {

    encoder->cur = start;
    encoder->end = end;
    encoder->bit_index = 0;
    encoder->bit_index_end = (end - start) * 8;
    encoder->overflow_cnt = 0;
}

static void _clem_nib_encode_self_sync_ff(
    struct ClemensNibEncoder* encoder,
    unsigned cnt
) {
    uint8_t* nib_cur = encoder->cur + (encoder->bit_index / 8);
    unsigned nib_bit_index_end = encoder->bit_index + 10 * cnt;
    unsigned sync_nybble = 0x00003FC;
    unsigned bit_count = 10 * cnt;
    unsigned out_shift, in_shift;

    if (encoder->bit_index < encoder->bit_index_end) {
        out_shift = 7 - (encoder->bit_index % 8);
        while (encoder->bit_index < nib_bit_index_end) {
            in_shift = 9 - (bit_count % 10);
            if (sync_nybble & (1 << in_shift)) {
                nib_cur[0] |= (1 << out_shift);
            } else {
                nib_cur[0] &= ~(1 << out_shift);
            }
            --bit_count;
            ++encoder->bit_index;
            if (encoder->bit_index >= encoder->bit_index_end) {
                break;
            }
            out_shift = 7 - (encoder->bit_index % 8);
            if (out_shift == 7) {
                ++nib_cur;
            }
        }
    }
    encoder->overflow_cnt += (nib_bit_index_end - encoder->bit_index);
}

static void _clem_nib_write_one(
    struct ClemensNibEncoder* encoder,
    uint8_t value
) {
    uint8_t* nib_cur = encoder->cur + (encoder->bit_index / 8);
    unsigned nib_bit_index_end = encoder->bit_index + 8;
    unsigned bit_count = 8;
    unsigned out_shift, in_shift;

    if (encoder->bit_index < encoder->bit_index_end) {
        out_shift = 7 - (encoder->bit_index % 8);
        while (encoder->bit_index < nib_bit_index_end) {
            in_shift = 7 - (bit_count % 8);
            if (value & (1 << in_shift)) {
                nib_cur[0] |= (1 << out_shift);
            } else {
                nib_cur[0] &= ~(1 << out_shift);
            }
            --bit_count;
            ++encoder->bit_index;
            if (encoder->bit_index >= encoder->bit_index_end) {
                break;
            }
            out_shift = 7 - (encoder->bit_index % 8);
            if (out_shift == 7) {
                ++nib_cur;
            }
        }
    }
    encoder->overflow_cnt += (nib_bit_index_end - encoder->bit_index);
}

static void _clem_nib_encode_one(
    struct ClemensNibEncoder* encoder,
    uint8_t value
) {
    _clem_nib_write_one(encoder, gcr_6_2_byte[value & 0x3f]);
}

static void _clem_nib_encode_data(
    struct ClemensNibEncoder* encoder,
    const uint8_t* value,
    unsigned cnt
) {

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
    unsigned i, qtr_track_index;
    uint8_t* nib_out = disk->nib->bits_data;
    bool is_35_disk;

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
                /* guesswork based on gap 3 size of 36 10-bit bytes and the  */
                self_sync_gap_1_cnt = 36;
                self_sync_gap_2_cnt = 6;
                self_sync_gap_3_cnt = 1;
            }
            num_regions = CLEM_DISK_35_NUM_REGIONS;
            for (i = 0; i < num_regions; ++i ) {
                clem_max_sectors_per_region[i] = g_clem_max_sectors_per_region_35[i];
                clem_track_start_per_region[i] = g_clem_track_start_per_region_35[i];
            }
            clem_track_start_per_region[num_regions] = (
                g_clem_track_start_per_region_35[num_regions]);
            /* 80 tracks for single sided - is this ever true?, otherwise the
               full track listing for double sided 3.5" disks */
            track_increment = disk->nib->is_double_sided ? 1 : 2;
            is_35_disk = true;
            break;
        case CLEM_DISK_TYPE_5_25:
            num_regions = 1;
            clem_max_sectors_per_region[0] = CLEM_DISK_525_NUM_SECTORS_PER_TRACK;
            clem_track_start_per_region[0] = 0;
            clem_track_start_per_region[1] = CLEM_DISK_LIMIT_QTR_TRACKS;
             /* 40 tracks total - always 35 for DOS/ProDOS disks */
            track_increment = 4;
            /* From Beneath Apple DOS/ProDOS - evaluate if DOS values should
               reflect those from Beneath Apple DOS. - anyway these are taken
               from Ciderpress */
            self_sync_gap_1_cnt = 48;           /* somewhere between 12-85 */
            self_sync_gap_2_cnt = 6;            /* somewhere between 5- 10 */
            self_sync_gap_3_cnt = 27;           /* somewhere between 16-28 */
            is_35_disk = false;
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
            assert(false);
            return false;
        case CLEM_2IMG_FORMAT_RAW:
        default:
            return false;
    }

    /* encode nibbles using the specified image format layout */
    for (qtr_track_index = 0; qtr_track_index < CLEM_DISK_LIMIT_QTR_TRACKS;
    ) {
        //  isolate into a function for readability
        unsigned sector_count = clem_max_sectors_per_region[current_region];
        uint8_t* nib_end = disk->nib->bits_data + (
            CLEM_DISK_35_CALC_BYTES_FROM_SECTORS(sector_count));
        uint8_t* data_in = disk->data;
        uint8_t* nib_cur = nib_out;
        unsigned sector_index;
        unsigned in_sector_index;
        unsigned track_index = qtr_track_index / 2;
        unsigned side_index = ((qtr_track_index & 1) << 5) | (track_index >> 6);
        unsigned temp;
        struct ClemensNibEncoder nib_encoder;

        if (nib_end >= disk->nib->bits_data_end) {
            nib_end = disk->nib->bits_data_end;
        }

        _clem_nib_init_encoder(&nib_encoder, nib_out, nib_end);

        for (sector_index = 0; sector_index < sector_count; ++sector_index) {
            _clem_nib_encode_self_sync_ff(&nib_encoder, self_sync_gap_1_cnt);
             in_sector_index = to_logical_sector_map[current_region][sector_index];
            data_in = disk->data + (in_sector_index * 512);

            /* Address */
            _clem_nib_write_one(&nib_encoder, 0xd5);
            _clem_nib_write_one(&nib_encoder, 0xaa);
            _clem_nib_write_one(&nib_encoder, 0x96);
            if (is_35_disk) {
                /* track, sector, side, format (0x22 or 0x24 - assume 0x22?) */
                _clem_nib_encode_one(&nib_encoder, (uint8_t)(track_index & 0x3f));
                _clem_nib_encode_one(&nib_encoder, (uint8_t)(in_sector_index & 0xff));
                _clem_nib_encode_one(&nib_encoder, (uint8_t)(side_index));
                _clem_nib_encode_one(&nib_encoder, 0x22);
                temp = (track_index ^ in_sector_index ^ side_index ^ 0x22);
                _clem_nib_encode_one(&nib_encoder, (uint8_t)(temp & 0x3f));
                _clem_nib_write_one(&nib_encoder, 0xde);
                _clem_nib_write_one(&nib_encoder, 0xaa);
            } else {
                assert(false);
                /* volume */
                /* track */
                /* sector */
                /* checksum */
                /* epilogue 5.25*/
            }

            _clem_nib_encode_self_sync_ff(&nib_encoder, self_sync_gap_2_cnt);

            /* Data */
            _clem_nib_write_one(&nib_encoder, 0xd5);
            _clem_nib_write_one(&nib_encoder, 0xaa);
            _clem_nib_write_one(&nib_encoder, 0xad);
            if (is_35_disk) {
                _clem_nib_encode_one(&nib_encoder, (uint8_t)(in_sector_index & 0x3f));
                _clem_nib_encode_data(&nib_encoder, data_in, 512);
                _clem_nib_write_one(&nib_encoder, 0xde);
                _clem_nib_write_one(&nib_encoder, 0xaa);
            } else {
                assert(false);
            }

            nib_out = nib_end;
        }

        qtr_track_index += track_increment;
        if (qtr_track_index >= clem_track_start_per_region[current_region + 1]) {
            ++current_region;
        }
    }


    return false;
}
