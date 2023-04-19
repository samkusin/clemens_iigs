#include "clem_disk.h"

#include <assert.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>

#define CLEM_DISK_NIB_SECTOR_DATA_TAG_35 12

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

static const uint8_t from_gcr_6_2_byte[128] = {
    0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80,     // 0x80-0x87
    0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80,     // 0x88-0x8F
    0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x00, 0x01,     // 0x90-0x97
    0x80, 0x80, 0x02, 0x03, 0x80, 0x04, 0x05, 0x06,     // 0x98-0x9F
    0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x07, 0x08,     // 0xA0-0xA7
    0x80, 0x80, 0x80, 0x09, 0x0a, 0x0b, 0x0c, 0x0d,     // 0xA8-0xAF
    0x80, 0x80, 0x0e, 0x0f, 0x10, 0x11, 0x12, 0x13,     // 0xB0-0xB7
    0x80, 0x14, 0x15, 0x16, 0x17, 0x18, 0x19, 0x1a,     // 0xB8-0xBF
    0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80,     // 0xC0-0xC7
    0x80, 0x80, 0x80, 0x1b, 0x80, 0x1c, 0x1d, 0x1e,     // 0xC8-0xCF
    0x80, 0x80, 0x80, 0x1f, 0x80, 0x80, 0x20, 0x21,     // 0xD0-0xD7
    0x80, 0x22, 0x23, 0x24, 0x25, 0x26, 0x27, 0x28,     // 0xD8-0xDF
    0x80, 0x80, 0x80, 0x80, 0x80, 0x29, 0x2a, 0x2b,     // 0xE0-0xE7
    0x80, 0x2c, 0x2d, 0x2e, 0x2f, 0x30, 0x31, 0x32,     // 0xE8-0xEF
    0x80, 0x80, 0x33, 0x34, 0x35, 0x36, 0x37, 0x38,     // 0xF0-0xF7
    0x80, 0x39, 0x3a, 0x3b, 0x3c, 0x3d, 0x3e, 0x3f      // 0xF8-0xFF
};

/* only support 16 sector tracks */
/* all logical sectors are interleaved by 2 */
static const unsigned physical_to_prodos_sector_map_525[1][16] = {
    {0, 8, 1, 9, 2, 10, 3, 11, 4, 12, 5, 13, 6, 14, 7, 15}};

static const unsigned physical_to_dos_sector_map_525[1][16] = {
    {0, 7, 14, 6, 13, 5, 12, 4, 11, 3, 10, 2, 9, 1, 8, 15}};

/* 3.5" drives have 512 byte sectors (ProDOS assumed), interleaved by 2 */
static const unsigned physical_to_prodos_sector_map_35[CLEM_DISK_35_NUM_REGIONS][16] = {
    {0, 6, 1, 7, 2, 8, 3, 9, 4, 10, 5, 11, -1, -1, -1, -1},
    {0, 6, 1, 7, 2, 8, 3, 9, 4, 10, 5, -1, -1, -1, -1, -1},
    {0, 5, 1, 6, 2, 7, 3, 8, 4, 9, -1, -1, -1, -1, -1, -1},
    {0, 5, 1, 6, 2, 7, 3, 8, 4, -1, -1, -1, -1, -1, -1, -1},
    {0, 4, 1, 5, 2, 6, 3, 7, -1, -1, -1, -1, -1, -1, -1, -1}};

unsigned g_clem_max_sectors_per_region_35[CLEM_DISK_35_NUM_REGIONS] = {12, 11, 10, 9, 8};
unsigned g_clem_track_start_per_region_35[CLEM_DISK_35_NUM_REGIONS + 1] = {0, 32, 64, 96, 128, 160};

// clang-format on

static unsigned clem_disk_nib_get_region_from_track(unsigned disk_type, unsigned track_index) {
    unsigned disk_region = 0;
    if (disk_type == CLEM_DISK_TYPE_3_5) {
        disk_region = 1;
        for (; disk_region < CLEM_DISK_35_NUM_REGIONS + 1; ++disk_region) {
            if (track_index < g_clem_track_start_per_region_35[disk_region]) {
                disk_region--;
                break;
            }
        }
    }
    return disk_region;
}

unsigned clem_disk_calculate_nib_storage_size(unsigned disk_type) {
    unsigned size = 0;
    switch (disk_type) {
    case CLEM_DISK_TYPE_5_25:
        size = CLEM_DISK_525_MAX_DATA_SIZE;
        break;
    case CLEM_DISK_TYPE_3_5:
        size = CLEM_DISK_35_MAX_DATA_SIZE;
        break;
    default:
        break;
    }
    return size;
}

_ClemensPhysicalSectorMap get_physical_to_logical_sector_map(unsigned disk_type, unsigned format) {
    switch (format) {
    case CLEM_DISK_FORMAT_PRODOS:
        if (disk_type == CLEM_DISK_TYPE_3_5) {
            return physical_to_prodos_sector_map_35;
        } else {
            return physical_to_prodos_sector_map_525;
        }
        break;
    case CLEM_DISK_FORMAT_DOS:
        assert(disk_type == CLEM_DISK_TYPE_5_25);
        return physical_to_dos_sector_map_525;
        break;
    case CLEM_DISK_FORMAT_RAW:
    default:
        assert(false);
        break;
    }
    return NULL;
}

unsigned *clem_disk_create_logical_to_physical_sector_map(unsigned *sectors, unsigned disk_type,
                                                          unsigned format, unsigned disk_region) {
    unsigned i;
    _ClemensPhysicalSectorMap phys_to_logical =
        get_physical_to_logical_sector_map(disk_type, format);
    if (disk_type == CLEM_DISK_TYPE_5_25)
        disk_region = 0;
    for (i = 0; i < 16; ++i) {
        if (phys_to_logical[disk_region][i] != -1) {
            sectors[phys_to_logical[disk_region][i]] = i;
        }
    }
    return sectors;
}

void clem_nib_reset_tracks(struct ClemensNibbleDisk *nib, unsigned track_count, uint8_t *bits_data,
                           uint8_t *bits_data_end) {
    nib->track_count = track_count;
    nib->bits_data = bits_data;
    nib->bits_data_end = bits_data_end;
    memset(nib->meta_track_map, 0xff, sizeof(nib->meta_track_map));
    memset(nib->track_bits_count, 0x00, sizeof(nib->track_bits_count));
    memset(nib->track_byte_count, 0x00, sizeof(nib->track_byte_count));
    memset(nib->track_initialized, 0x00, sizeof(nib->track_initialized));
}

struct ClemensNibbleDiskHead *clem_disk_nib_head_init(struct ClemensNibbleDiskHead *head,
                                                      const struct ClemensNibbleDisk *disk,
                                                      unsigned track) {
    if (track >= disk->track_count)
        return NULL;

    head->bytes = disk->bits_data + disk->track_byte_offset[track];
    head->bits_index = 0;
    head->bits_limit = disk->track_bits_count[track];
    head->track = track;
    return head;
}

bool clem_disk_nib_head_peek(struct ClemensNibbleDiskHead *head) {
    uint8_t disk_byte = head->bytes[head->bits_index / 8];
    return (disk_byte & (1 << (7 - (head->bits_index % 8)))) != 0;
}

void clem_disk_nib_head_next(struct ClemensNibbleDiskHead *head, unsigned cells) {
    head->bits_index = (head->bits_index + cells) % head->bits_limit;
}

bool clem_disk_nib_head_read_bit(struct ClemensNibbleDiskHead *head) {
    bool bit = clem_disk_nib_head_peek(head);
    clem_disk_nib_head_next(head, 1);
    return bit;
}

//  TODO: this should be leveraged by the IWM since the logic here is
uint8_t clem_disk_nib_read_latch(unsigned *state, uint8_t latch, bool read_bit) {
    switch (*state & CLEM_DISK_READ_STATE_MASK) {
    case CLEM_DISK_READ_STATE_START:
        if (read_bit) {
            latch <<= 1;
            latch |= 0x1;
            *state = (*state & ~CLEM_DISK_READ_STATE_MASK) | CLEM_DISK_READ_STATE_QA0;
        }
        break;
    case CLEM_DISK_READ_STATE_QA0:
        latch <<= 1;
        if (read_bit) {
            latch |= 0x1;
        }
        if (latch & 0x80) {
            *state = (*state & ~CLEM_DISK_READ_STATE_MASK) | CLEM_DISK_READ_STATE_QA1;
        }
        break;
    case CLEM_DISK_READ_STATE_QA1:
        if (read_bit) {
            latch = 0x1;
            *state = (*state & ~CLEM_DISK_READ_STATE_MASK) | CLEM_DISK_READ_STATE_QA1_1;
        }
        break;
    case CLEM_DISK_READ_STATE_QA1_1:
        latch <<= 1;
        if (read_bit) {
            latch |= 0x1;
        }
        *state = (*state & ~CLEM_DISK_READ_STATE_MASK) | CLEM_DISK_READ_STATE_QA0;
        break;
    }
    return latch;
}

void clem_disk_nib_reader_init(struct ClemensNibbleDiskReader *reader,
                               const struct ClemensNibbleDisk *disk, unsigned track) {
    reader->read_state = CLEM_DISK_READ_STATE_START;
    reader->track_scan_state = CLEM_NIB_TRACK_SCAN_FIND_PROLOGUE;
    reader->track_scan_state_next = CLEM_NIB_TRACK_SCAN_FIND_PROLOGUE;
    reader->track_is_35 = disk->disk_type == CLEM_DISK_TYPE_3_5 ? 1 : 0;
    reader->sector_found = 0;
    reader->latch = 0;
    reader->disk_bytes_cnt = 0;
    clem_disk_nib_head_init(&reader->head, disk, track);
    //  sliding bits index will change when hitting the first available sector
    //  prologue, so we can detect wraparound on a synced bit stream.
    reader->first_sector_bits_index = reader->head.bits_index;
}

bool clem_disk_nib_reader_next(struct ClemensNibbleDiskReader *reader) {
    if (reader->track_scan_state_next != reader->track_scan_state) {
        //  buffer should've been processed by caller before this call
        reader->track_scan_state = reader->track_scan_state_next;
        reader->disk_bytes_cnt = 0;
    }
    if (reader->track_scan_state == CLEM_NIB_TRACK_SCAN_AT_TRACK_END)
        return false;

    reader->latch = clem_disk_nib_read_latch(&reader->read_state, reader->latch,
                                             clem_disk_nib_head_read_bit(&reader->head));

    if (reader->head.bits_index == reader->first_sector_bits_index) {
        reader->track_scan_state_next = CLEM_NIB_TRACK_SCAN_AT_TRACK_END;
    }
    if (!(reader->latch & 0x80) ||
        (reader->track_scan_state_next == CLEM_NIB_TRACK_SCAN_AT_TRACK_END)) {
        return reader->track_scan_state != reader->track_scan_state_next;
    }

    //  the disk latch is effectively our data read register, and so
    //  when we've detected a valid disk nibble, we can clear the
    //  latch and wait until the next byte
    reader->disk_bytes[reader->disk_bytes_cnt++] = reader->latch;
    reader->latch = 0;

    switch (reader->track_scan_state) {
    case CLEM_NIB_TRACK_SCAN_FIND_PROLOGUE:
        if (reader->disk_bytes[0] == 0xD5 && reader->disk_bytes_cnt == 1) {
            reader->track_scan_state_next = CLEM_NIB_TRACK_SCAN_FIND_PROLOGUE;
        } else if (reader->disk_bytes[1] == 0xAA && reader->disk_bytes_cnt == 2) {
            reader->track_scan_state_next = CLEM_NIB_TRACK_SCAN_FIND_PROLOGUE;
        } else if (reader->disk_bytes[2] == 0x96 && reader->disk_bytes_cnt == 3) {
            reader->track_scan_state_next = reader->track_is_35
                                                ? CLEM_NIB_TRACK_SCAN_FIND_ADDRESS_35
                                                : CLEM_NIB_TRACK_SCAN_FIND_ADDRESS_525;
            if (!reader->sector_found) {
                if (reader->head.bits_index >= 8 * 3) {
                    reader->first_sector_bits_index = reader->head.bits_index - 24;
                } else {
                    reader->first_sector_bits_index =
                        reader->head.bits_limit - 24 + reader->head.bits_index;
                }
                reader->sector_found = 1;
            }
        } else {
            reader->disk_bytes_cnt = 0;
        }
        break;
    case CLEM_NIB_TRACK_SCAN_FIND_ADDRESS_35:
        //  GCR 6_2 bytes
        if (reader->disk_bytes_cnt == 5) {
            reader->track_scan_state_next = CLEM_NIB_TRACK_SCAN_END_ADDRESS;
        }
        break;
    case CLEM_NIB_TRACK_SCAN_FIND_ADDRESS_525:
        //  2 bytes with 4 bits of actual data (4_4 encoding) per datum
        //  4 total decoded bytes = 4 * 2 disk bytes
        if (reader->disk_bytes_cnt == 8) {
            reader->track_scan_state_next = CLEM_NIB_TRACK_SCAN_END_ADDRESS;
        }
        break;
    case CLEM_NIB_TRACK_SCAN_END_ADDRESS:
        //  0xDE, 0xAA
        if (reader->disk_bytes[0] == 0xDE && reader->disk_bytes_cnt == 1) {
            reader->track_scan_state_next = CLEM_NIB_TRACK_SCAN_END_ADDRESS;
        } else if (reader->disk_bytes[1] == 0xAA && reader->disk_bytes_cnt == 2) {
            reader->track_scan_state_next = CLEM_NIB_TRACK_SCAN_FIND_DATA_PROLOGUE;
        } else {
            reader->track_scan_state_next = CLEM_NIB_TRACK_SCAN_ERROR;
        }
        break;
    case CLEM_NIB_TRACK_SCAN_FIND_DATA_PROLOGUE:
        if (reader->disk_bytes[0] == 0xD5 && reader->disk_bytes_cnt == 1) {
            reader->track_scan_state_next = CLEM_NIB_TRACK_SCAN_FIND_DATA_PROLOGUE;
        } else if (reader->disk_bytes[1] == 0xAA && reader->disk_bytes_cnt == 2) {
            reader->track_scan_state_next = CLEM_NIB_TRACK_SCAN_FIND_DATA_PROLOGUE;
        } else if (reader->disk_bytes[2] == 0xAD) {
            if (reader->disk_bytes_cnt == 3) {
                //  3.5" disks have the sector encoded in the fourth byte
                reader->track_scan_state_next = reader->track_is_35
                                                    ? CLEM_NIB_TRACK_SCAN_FIND_DATA_PROLOGUE
                                                    : CLEM_NIB_TRACK_SCAN_READ_DATA;
            } else if (reader->disk_bytes_cnt == 4) {
                reader->track_scan_state_next = CLEM_NIB_TRACK_SCAN_READ_DATA;
            }
        } else {
            reader->disk_bytes_cnt = 0;
        }
        break;
    case CLEM_NIB_TRACK_SCAN_READ_DATA:
        //  end when we reach the epilogue
        if (reader->disk_bytes_cnt >= 2) {
            uint8_t *epilogue = &reader->disk_bytes[reader->disk_bytes_cnt - 2];
            if (epilogue[0] == 0xDE && epilogue[1] == 0xAA) {
                reader->track_scan_state_next = CLEM_NIB_TRACK_SCAN_FIND_PROLOGUE;
            }
        }
        break;
    }

    if (reader->disk_bytes_cnt >= sizeof(reader->disk_bytes)) {
        reader->track_scan_state_next = CLEM_NIB_TRACK_SCAN_ERROR;
    }

    return reader->track_scan_state != reader->track_scan_state_next;
}

bool clem_nib_begin_track_encoder(struct ClemensNibEncoder *encoder, struct ClemensNibbleDisk *nib,
                                  unsigned nib_track_index, unsigned bits_data_offset,
                                  unsigned bits_data_size) {

    if (nib->bits_data + bits_data_offset + bits_data_size > nib->bits_data_end) {
        //  Out of space to write - this shouldn't happen.
        printf("clem_nib_begin_track_encoder: (%u, %u, %u) out of space.\n", nib_track_index,
               bits_data_offset, bits_data_size);
        assert(false);
        return false;
    }

    encoder->begin = nib->bits_data + bits_data_offset;
    encoder->end = encoder->begin + bits_data_size;
    encoder->bit_index = 0;
    encoder->bit_index_end = bits_data_size * 8;
    nib->track_byte_offset[nib_track_index] = bits_data_offset;
    nib->track_byte_count[nib_track_index] = bits_data_size;
    return true;
}

void clem_nib_end_track_encoder(struct ClemensNibEncoder *encoder, struct ClemensNibbleDisk *nib,
                                unsigned nib_track_index) {
    //  TODO: use actual bits/bytes encoded vs the fixed amount per track
    //        (use encoder->bit_index at end of track)
    nib->track_bits_count[nib_track_index] = encoder->bit_index_end; // encoder->bit_index_end;
    nib->track_byte_count[nib_track_index] = (encoder->bit_index_end + 7) / 8;
    nib->track_initialized[nib_track_index] = 1;
}

static void clem_nib_write_bytes(struct ClemensNibEncoder *encoder, unsigned cnt, unsigned bit_cnt,
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

static void clem_nib_encode_self_sync_ff(struct ClemensNibEncoder *encoder, unsigned cnt) {
    clem_nib_write_bytes(encoder, cnt, 10, 0xff);
}

static void clem_nib_write_one(struct ClemensNibEncoder *encoder, uint8_t value) {
    clem_nib_write_bytes(encoder, 1, 8, value);
}

static void clem_nib_encode_one_6_2(struct ClemensNibEncoder *encoder, uint8_t value) {
    clem_nib_write_one(encoder, gcr_6_2_byte[value & 0x3f]);
}

static void clem_nib_encode_one_4_4(struct ClemensNibEncoder *encoder, uint8_t value) {
    /* all unused bits are set to '1', so 4x4 encoding to preserve odd bits
       requires shifting the bits to the right  */
    clem_nib_write_one(encoder, (value >> 1) | 0xaa);
    /* even bits */
    clem_nib_write_one(encoder, value | 0xaa);
}

static void clem_nib_encode_data_35(struct ClemensNibEncoder *encoder, const uint8_t *buf,
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
    memset(data, 0, CLEM_DISK_NIB_SECTOR_DATA_TAG_35);
    memcpy(data + CLEM_DISK_NIB_SECTOR_DATA_TAG_35, buf, 512);

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
        if (chksum[0] & 0x100) {
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
        clem_nib_encode_one_6_2(encoder, v);
        clem_nib_encode_one_6_2(encoder, scratch0[data_idx]);
        clem_nib_encode_one_6_2(encoder, scratch1[data_idx]);
        if (data_idx < scratch_idx - 1) {
            clem_nib_encode_one_6_2(encoder, scratch2[data_idx]);
        }
    }

    /* checksum */
    v = (chksum[0] & 0xc0) >> 6;
    v |= (chksum[1] & 0xc0) >> 4;
    v |= (chksum[2] & 0xc0) >> 2;
    clem_nib_encode_one_6_2(encoder, v);
    clem_nib_encode_one_6_2(encoder, chksum[2]);
    clem_nib_encode_one_6_2(encoder, chksum[1]);
    clem_nib_encode_one_6_2(encoder, chksum[0]);
}

#define CLEM_NIB_ENCODE_525_6_2_RIGHT_BUFFER_SIZE 86

static void clem_nib_encode_data_525(struct ClemensNibEncoder *encoder, const uint8_t *buf,
                                     unsigned cnt) {
    /* cannot support anything by cnt = 256 bytes,  with 86 bytes to
       contain the 2 bits nibble = 324 bytes, which is the specified data chunk
       size of the sector on disk */
    uint8_t enc6[256];
    uint8_t enc2[CLEM_NIB_ENCODE_525_6_2_RIGHT_BUFFER_SIZE];
    unsigned enc2pos, enc2shift, chksum;
    int i6, i2;
    uint8_t rbyte;
    uint8_t tmp;

    memset(enc2, 0, sizeof(enc2));
    for (i2 = 0, enc2pos = CLEM_NIB_ENCODE_525_6_2_RIGHT_BUFFER_SIZE - 1, enc2shift = 0; i2 < 256;
         i2++) {
        rbyte = buf[i2];
        enc6[i2] = rbyte >> 2;
        tmp = enc2[enc2pos];
        tmp |= (((rbyte & 1) << 1) | ((rbyte & 2) >> 1));
        tmp <<= enc2shift;
        enc2[enc2pos] = tmp;
        if (enc2pos == 0) {
            enc2pos = CLEM_NIB_ENCODE_525_6_2_RIGHT_BUFFER_SIZE;
            enc2shift += 2;
        }
        enc2pos--;
    }

    chksum = 0;
    for (i2 = CLEM_NIB_ENCODE_525_6_2_RIGHT_BUFFER_SIZE; i2 > 0;) {
        --i2;
        clem_nib_encode_one_6_2(encoder, enc2[i2] ^ chksum);
        chksum = enc2[i2];
    }
    for (i6 = 0; i6 < 256; ++i6) {
        clem_nib_encode_one_6_2(encoder, enc6[i6] ^ chksum);
        chksum = enc6[i6];
    }

    clem_nib_encode_one_6_2(encoder, chksum);
}

void clem_disk_nib_encode_track_35(struct ClemensNibEncoder *nib_encoder,
                                   unsigned logical_track_index, unsigned side_index,
                                   unsigned sector_format, unsigned logical_sector_index,
                                   unsigned track_sector_count,
                                   const unsigned *to_logical_sector_map, const uint8_t *data) {
    uint8_t side_index_and_track_64 = (side_index << 5) | (logical_track_index >> 6);
    unsigned sector;

    clem_nib_write_one(nib_encoder, 0xff);
    clem_nib_encode_self_sync_ff(nib_encoder, (CLEM_DISK_35_BYTES_TRACK_GAP_1 * 8) / 10);

    //  populate track with sectors in OS order,
    //  3.5" disk sector data is by definition 512 bytes

    for (sector = 0; sector < track_sector_count; sector++) {
        unsigned logical_sector = to_logical_sector_map[sector];
        const uint8_t *source_data = data + (logical_sector_index + logical_sector) * 512;
        unsigned temp;

        clem_nib_write_one(nib_encoder, 0xff);
        //  ADDRESS (prologue, header, epilogue) note the combined address
        //  segment differs from the 5.25" version
        //  track, sector, side, format (0x12 or 0x22 or 0x14 or 0x24)
        //      format = sides | interleave where interleave should always be 2
        //
        clem_nib_write_one(nib_encoder, 0xd5);
        clem_nib_write_one(nib_encoder, 0xaa);
        clem_nib_write_one(nib_encoder, 0x96);
        clem_nib_encode_one_6_2(nib_encoder, (uint8_t)(logical_track_index & 0xff));
        clem_nib_encode_one_6_2(nib_encoder, (uint8_t)(logical_sector & 0xff));
        clem_nib_encode_one_6_2(nib_encoder, (uint8_t)(side_index_and_track_64 & 0xff));
        clem_nib_encode_one_6_2(nib_encoder, sector_format);
        temp = (logical_track_index ^ logical_sector ^ side_index_and_track_64 ^ sector_format);
        clem_nib_encode_one_6_2(nib_encoder, (uint8_t)(temp));
        clem_nib_write_one(nib_encoder, 0xde);
        clem_nib_write_one(nib_encoder, 0xaa);
        clem_nib_write_one(nib_encoder, 0xff);
        //  SELF-SYNC
        clem_nib_encode_self_sync_ff(nib_encoder, 4);
        clem_nib_write_one(nib_encoder, 0xff);
        //  DATA
        clem_nib_write_one(nib_encoder, 0xd5);
        clem_nib_write_one(nib_encoder, 0xaa);
        clem_nib_write_one(nib_encoder, 0xad);
        clem_nib_encode_one_6_2(nib_encoder, (uint8_t)logical_sector);
        clem_nib_encode_data_35(nib_encoder, source_data, 512);
        clem_nib_write_one(nib_encoder, 0xde);
        clem_nib_write_one(nib_encoder, 0xaa);
        if (sector + 1 < track_sector_count) {
            //  all but the last sector of this track
            clem_nib_write_one(nib_encoder, 0xff);
            clem_nib_write_one(nib_encoder, 0xff);
            clem_nib_write_one(nib_encoder, 0xff);
            clem_nib_encode_self_sync_ff(nib_encoder, (CLEM_DISK_35_BYTES_TRACK_GAP_3 * 8) / 10);
        }
    }
}

void clem_disk_nib_encode_track_525(struct ClemensNibEncoder *nib_encoder, uint8_t volume,
                                    unsigned track_index, unsigned logical_sector_index,
                                    unsigned track_sector_count,
                                    const unsigned *to_logical_sector_map, const uint8_t *data) {
    unsigned sector;
    clem_nib_encode_self_sync_ff(nib_encoder, CLEM_DISK_525_BYTES_TRACK_GAP_1);

    for (sector = 0; sector < CLEM_DISK_525_NUM_SECTORS_PER_TRACK; sector++) {
        unsigned logical_sector = to_logical_sector_map[sector];
        const uint8_t *source_data = data + (logical_sector_index + logical_sector) * 256;
        // Sector Address Prologue + Body + Epilogue
        //  - The sector written here is the physical sector vs logical?  Ciderpress
        //    AppleWin, etc seem to imply this.  This differs from the 3.5" address
        clem_nib_write_one(nib_encoder, 0xd5);
        clem_nib_write_one(nib_encoder, 0xaa);
        clem_nib_write_one(nib_encoder, 0x96);
        clem_nib_encode_one_4_4(nib_encoder, (uint8_t)(volume & 0xff));
        clem_nib_encode_one_4_4(nib_encoder, track_index);
        clem_nib_encode_one_4_4(nib_encoder, sector);
        clem_nib_encode_one_4_4(nib_encoder, (uint8_t)(volume ^ track_index ^ sector));
        clem_nib_write_one(nib_encoder, 0xde);
        clem_nib_write_one(nib_encoder, 0xaa);
        clem_nib_write_one(nib_encoder, 0xeb);
        // GAP 2
        clem_nib_encode_self_sync_ff(nib_encoder, CLEM_DISK_525_BYTES_TRACK_GAP_2);
        // Sector Data Prologue + Body + Epilogue
        clem_nib_write_one(nib_encoder, 0xd5);
        clem_nib_write_one(nib_encoder, 0xaa);
        clem_nib_write_one(nib_encoder, 0xad);
        clem_nib_encode_data_525(nib_encoder, source_data, 256);
        clem_nib_write_one(nib_encoder, 0xde);
        clem_nib_write_one(nib_encoder, 0xaa);
        clem_nib_write_one(nib_encoder, 0xeb);
        if (sector + 1 < CLEM_DISK_525_NUM_SECTORS_PER_TRACK) {
            clem_nib_encode_self_sync_ff(nib_encoder, CLEM_DISK_525_BYTES_TRACK_GAP_3);
        }
    }
}

/******************************************************************************/
#define CLEM_NIB_DECODE_BYTE(_out_, _byte_) *(_out_) = (from_gcr_6_2_byte[(_byte_)-0x80]);

#define CLEM_2IMG_NIB_READER_DECODE_BYTE(_out_, _reader_, _index_)                                 \
    CLEM_NIB_DECODE_BYTE(_out_, (_reader_)->disk_bytes[_index_])                                   \
    if (*(_out_) == 0x80)                                                                          \
        return;

#define CLEM_2IMG_NIB_READER_DECODE_4_4_BYTES(_out_, _reader_, _index_)                            \
    *(_out_) = ((_reader_)->disk_bytes[_index_] & 0x55) << 1;                                      \
    *(_out_) |= ((_reader_)->disk_bytes[(_index_) + 1] & 0x55);

static void clem_disk_nib_reader_address_35(struct ClemensNibbleDiskReader *reader, uint8_t *track,
                                            uint8_t *sector, uint8_t *side, uint8_t *chksum) {
    uint8_t rbyte;
    if (reader->track_scan_state != CLEM_NIB_TRACK_SCAN_FIND_ADDRESS_35)
        return;
    // index 3 is the sector_format - but this shouldn't be needed since the
    // nibblized track can tell us whether the image is double sided or not
    CLEM_2IMG_NIB_READER_DECODE_BYTE(&rbyte, reader, 0)
    *track = rbyte;
    CLEM_2IMG_NIB_READER_DECODE_BYTE(&rbyte, reader, 2)
    *track = ((rbyte & 0x1) << 6) | *track;
    *side = (rbyte >> 5);
    CLEM_2IMG_NIB_READER_DECODE_BYTE(&rbyte, reader, 1)
    *sector = rbyte;
    CLEM_2IMG_NIB_READER_DECODE_BYTE(&rbyte, reader, 4)
    *chksum = rbyte;
}

static void clem_disk_nib_reader_address_525(struct ClemensNibbleDiskReader *reader,
                                             uint8_t *volume, uint8_t *track, uint8_t *sector,
                                             uint8_t *chksum) {
    uint8_t rbyte;
    if (reader->track_scan_state != CLEM_NIB_TRACK_SCAN_FIND_ADDRESS_525)
        return;
    CLEM_2IMG_NIB_READER_DECODE_4_4_BYTES(&rbyte, reader, 0);
    *volume = rbyte;
    CLEM_2IMG_NIB_READER_DECODE_4_4_BYTES(&rbyte, reader, 2);
    *track = rbyte;
    CLEM_2IMG_NIB_READER_DECODE_4_4_BYTES(&rbyte, reader, 4);
    *sector = rbyte;
    CLEM_2IMG_NIB_READER_DECODE_4_4_BYTES(&rbyte, reader, 6);
    *chksum = rbyte;
}

static bool clem_disk_nib_reader_data_35(struct ClemensNibbleDiskReader *reader,
                                         uint8_t *data_start, uint8_t *data_end,
                                         uint8_t *chksum_out, uint8_t *chksum_calc) {
    // The data will be read serially and decoded to 524 bytes (one sector + 12 byte tag)
    // Incoming data is organized in strings of 4 GCR 6-2 bytes (3 GCR 6-2 bytes for the
    // last string).  Additional decoding involves a reversal of what was done in the
    // encode version (again ported from the Ciderpress implementation.)

    const uint8_t *disk_bytes = &reader->disk_bytes[0];
    uint8_t *data_cur;
    uint8_t scratch0[175], scratch1[175], scratch2[175];
    unsigned chksum[3];
    unsigned source_idx;
    uint8_t rbyte6[3];
    uint8_t rbyte;

    source_idx = 0;
    while (source_idx < sizeof(scratch0)) {
        // bits 4,5 or rbyte are linked to rbyte6[0]
        // bits 2,3 or rbyte are linked to rbyte6[1]
        // bits 0,1 or rbyte are linked to rbyte6[2]
        CLEM_NIB_DECODE_BYTE(&rbyte, *disk_bytes++);
        if (rbyte == 0x80)
            return false;
        CLEM_NIB_DECODE_BYTE(&rbyte6[0], *disk_bytes++);
        if (rbyte6[0] == 0x80)
            return false;
        CLEM_NIB_DECODE_BYTE(&rbyte6[1], *disk_bytes++);
        if (rbyte6[1] == 0x80)
            return false;
        if (source_idx < 174) {
            CLEM_NIB_DECODE_BYTE(&rbyte6[2], *disk_bytes++);
            if (rbyte6[2] == 0x80)
                return false;
        } else {
            rbyte6[2] = 0x00;
        }
        scratch0[source_idx] = ((rbyte << 2) & 0xc0) | rbyte6[0];
        scratch1[source_idx] = ((rbyte << 4) & 0xc0) | rbyte6[1];
        scratch2[source_idx] = ((rbyte << 6) & 0xc0) | rbyte6[2];
        source_idx++;
    }
    if ((disk_bytes - &reader->disk_bytes[0]) > reader->disk_bytes_cnt) {
        return false;
    }
    //  decode the scratch bytes using the calculated checksum
    chksum[0] = chksum[1] = chksum[2] = 0;
    source_idx = 0;
    data_cur = data_start;
    while (source_idx < sizeof(scratch0)) {
        chksum[0] = (chksum[0] & 0xff) << 1;
        if (chksum[0] & 0x100) {
            ++chksum[0];
        }
        rbyte6[0] = scratch0[source_idx] ^ chksum[0];

        chksum[2] += rbyte6[0];
        if (chksum[0] & 0x100) {
            ++chksum[2];
            chksum[0] &= 0xff;
        }
        if (source_idx >= CLEM_DISK_NIB_SECTOR_DATA_TAG_35 / 3)
            *data_cur++ = rbyte6[0];

        rbyte6[1] = scratch1[source_idx] ^ chksum[2];
        chksum[1] += rbyte6[1];
        if (chksum[2] >= 0x100) {
            ++chksum[1];
            chksum[2] &= 0xff;
        }
        if (source_idx >= CLEM_DISK_NIB_SECTOR_DATA_TAG_35 / 3)
            *data_cur++ = rbyte6[1];

        if (data_cur - data_start == 512) {
            assert(source_idx == sizeof(scratch0) - 1);
            break;
        }

        rbyte6[2] = scratch2[source_idx] ^ chksum[1];
        chksum[0] += rbyte6[2];
        if (chksum[1] >= 0x100) {
            ++chksum[0];
            chksum[1] &= 0xff;
        }
        if (source_idx >= CLEM_DISK_NIB_SECTOR_DATA_TAG_35 / 3)
            *data_cur++ = rbyte6[2];
        ++source_idx;
    }

    chksum_calc[0] = chksum[0];
    chksum_calc[1] = chksum[1];
    chksum_calc[2] = chksum[2];

    source_idx = (unsigned)(disk_bytes - &reader->disk_bytes[0]);
    if (reader->disk_bytes_cnt <= source_idx)
        return false;
    if (reader->disk_bytes_cnt - source_idx < 4)
        return false;

    CLEM_NIB_DECODE_BYTE(&rbyte, *disk_bytes++);
    CLEM_NIB_DECODE_BYTE(&chksum[2], *disk_bytes++);
    CLEM_NIB_DECODE_BYTE(&chksum[1], *disk_bytes++);
    CLEM_NIB_DECODE_BYTE(&chksum[0], *disk_bytes++);

    if (rbyte == 0x80 || chksum[0] == 0x80 || chksum[1] == 0x80 || chksum[2] == 0x80)
        return false;

    chksum_out[0] = ((rbyte << 6) & 0xc0) | chksum[0];
    chksum_out[1] = ((rbyte << 4) & 0xc0) | chksum[1];
    chksum_out[2] = ((rbyte << 2) & 0xc0) | chksum[2];

    return true;
}

unsigned clem_disk_nib_decode_nibblized_track_35(const struct ClemensNibbleDisk *nib,
                                                 const unsigned *logical_sector_map,
                                                 unsigned bits_track_index,
                                                 unsigned logical_sector_index, uint8_t *data_start,
                                                 uint8_t *data_end) {
    struct ClemensNibbleDiskReader disk_reader;
    unsigned i, sz, disk_region;
    uint8_t track, sector, side, chksum;
    bool track_scan_finished;
    uint8_t *sector_data_start;
    uint8_t data_chksum_ondisk[3];
    uint8_t data_chksum[3];

    clem_disk_nib_reader_init(&disk_reader, nib, bits_track_index);
    sz = 0;
    track_scan_finished = false;
    while (!track_scan_finished) {
        if (disk_reader.track_scan_state == CLEM_NIB_TRACK_SCAN_AT_TRACK_END) {
            track_scan_finished = true;
            continue;
        }
        if (!clem_disk_nib_reader_next(&disk_reader))
            continue;
        switch (disk_reader.track_scan_state) {
        case CLEM_NIB_TRACK_SCAN_FIND_ADDRESS_35:
            clem_disk_nib_reader_address_35(&disk_reader, &track, &sector, &side, &chksum);
            break;
        case CLEM_NIB_TRACK_SCAN_READ_DATA:
            sector_data_start = data_start + (logical_sector_index + sector) * 512;
            if ((sector_data_start >= data_end || sector_data_start + 512 > data_end) ||
                !clem_disk_nib_reader_data_35(&disk_reader, sector_data_start,
                                              sector_data_start + 512, data_chksum_ondisk,
                                              data_chksum)) {
                sz = 0;
                track_scan_finished = true;
            } else {
                sz += 512;
            }
            break;
        case CLEM_NIB_TRACK_SCAN_ERROR:
            track_scan_finished = true;
            sz = 0;
            break;
        }
    }

    return track_scan_finished ? sz : 0;
}

static bool clem_disk_nib_reader_data_525(struct ClemensNibbleDiskReader *reader,
                                          uint8_t *data_start, uint8_t *data_end,
                                          uint8_t *chksum_out, uint8_t *chksum_calc) {
    /* Expecting 256 + CLEM_NIB_ENCODE_525_6_2_RIGHT_BUFFER_SIZE + 1 bytes (checksum) */
    /* Output will be the 256 byte sector and the calculated checksum */
    /* Like the 3.5" disk encode/decode, this has been ported from Ciderpress, though
       the method for 5.25" is **way** easier to comprehend than the one used for 3.5"
       disks. */
    const uint8_t *disk_bytes = &reader->disk_bytes[0];
    uint8_t enc2_unpacked[CLEM_NIB_ENCODE_525_6_2_RIGHT_BUFFER_SIZE * 3];
    unsigned chksum, i2, i6;
    uint8_t rbyte;
    uint8_t tmp;

    if (data_end - data_start < 256)
        return false;

    /* Generate a table of 2-bit parts for each 6-bit nibble (256 total.)  The
       extra two bytes aren't actually used and will always decode to byte values of 0 */
    chksum = 0;
    for (i2 = 0; i2 < CLEM_NIB_ENCODE_525_6_2_RIGHT_BUFFER_SIZE; i2++) {
        CLEM_NIB_DECODE_BYTE(&rbyte, *disk_bytes++);
        if (rbyte == 0x80)
            return false;
        chksum ^= rbyte;
        /* bits 0,1   2,3   4,5  switched and shifted to the first two bits*/
        enc2_unpacked[i2] = ((chksum & 0x1) << 1) | ((chksum & 0x2) >> 1);
        enc2_unpacked[i2 + CLEM_NIB_ENCODE_525_6_2_RIGHT_BUFFER_SIZE] =
            ((chksum & 0x4) >> 1) | ((chksum & 0x8) >> 3);
        enc2_unpacked[i2 + CLEM_NIB_ENCODE_525_6_2_RIGHT_BUFFER_SIZE * 2] =
            ((chksum & 0x10) >> 3) | ((chksum & 0x20) >> 5);
    }
    /* Decoded the 6-bit value, and now combine with the 2-bit value from our table */
    for (i6 = 0; i6 < 256; ++i6) {
        CLEM_NIB_DECODE_BYTE(&rbyte, *disk_bytes++);
        if (rbyte == 0x80)
            return false;
        chksum ^= rbyte;
        data_start[i6] = ((chksum & 0xff) << 2) | enc2_unpacked[i6];
    }
    *chksum_calc = chksum;

    CLEM_NIB_DECODE_BYTE(&rbyte, *disk_bytes++);
    if (rbyte == 0x80)
        return false;
    *chksum_out = rbyte;

    return true;
}

unsigned clem_disk_nib_decode_nibblized_track_525(const struct ClemensNibbleDisk *nib,
                                                  const unsigned *logical_sector_map,
                                                  unsigned bits_track_index,
                                                  unsigned logical_sector_index,
                                                  uint8_t *data_start, uint8_t *data_end) {
    struct ClemensNibbleDiskReader disk_reader;
    unsigned i, sz, disk_region;
    uint8_t volume, sector, track, chksum;
    bool track_scan_finished;
    uint8_t *sector_data_start;
    uint8_t data_chksum_ondisk;
    uint8_t data_chksum;

    clem_disk_nib_reader_init(&disk_reader, nib, bits_track_index);
    sz = 0;
    track_scan_finished = false;
    while (!track_scan_finished) {
        if (!clem_disk_nib_reader_next(&disk_reader))
            continue;
        switch (disk_reader.track_scan_state) {
        case CLEM_NIB_TRACK_SCAN_AT_TRACK_END:
            track_scan_finished = true;
            break;
        case CLEM_NIB_TRACK_SCAN_FIND_ADDRESS_525:
            clem_disk_nib_reader_address_525(&disk_reader, &volume, &track, &sector, &chksum);
            if (sector >= 16) {
                track_scan_finished = true;
                return 0;
            }
            break;
        case CLEM_NIB_TRACK_SCAN_READ_DATA:
            sector_data_start =
                data_start + (logical_sector_index + logical_sector_map[sector]) * 256;
            if ((sector_data_start >= data_end || sector_data_start + 256 > data_end) ||
                !clem_disk_nib_reader_data_525(&disk_reader, sector_data_start,
                                               sector_data_start + 256, &data_chksum_ondisk,
                                               &data_chksum)) {
                sz = 0;
                track_scan_finished = true;
            } else {
                sz += 256;
            }
            break;
        case CLEM_NIB_TRACK_SCAN_ERROR:
            track_scan_finished = true;
            sz = 0;
            break;
        }
    }

    return track_scan_finished ? sz : 0;
}

/******************************************************************************/

bool clem_disk_nib_encode_35(struct ClemensNibbleDisk *nib, unsigned format, bool double_sided,
                             const uint8_t *data_start, const uint8_t *data_end) {
    _ClemensPhysicalSectorMap to_logical_sector_map;
    unsigned qtr_tracks_per_track, disk_region;
    unsigned qtr_track_index;
    unsigned track_byte_offset;
    unsigned logical_sector_index;

    if (nib->disk_type != CLEM_DISK_TYPE_3_5)
        return false;

    nib->is_double_sided = double_sided;
    if (nib->is_double_sided) {
        if (data_end - data_start < CLEM_DISK_35_DOUBLE_PRODOS_BLOCK_COUNT * 512)
            return false;
        qtr_tracks_per_track = 1;
    } else {
        if (data_end - data_start < CLEM_DISK_35_PRODOS_BLOCK_COUNT * 512)
            return false;
        qtr_tracks_per_track = 2;
    }

    nib->bit_timing_ns = CLEM_DISK_3_5_BIT_TIMING_NS;

    /* The various self-sync gaps between sectors are derived from the
       ProDOS firmware format method.  See clem_disk.h for details.  */
    disk_region = 0;          // 3.5" disk tracks are divided into regions
    track_byte_offset = 0;    // Offset into nib bits data
    logical_sector_index = 0; // Sector from 0 to 800/1600 on disk
    to_logical_sector_map = get_physical_to_logical_sector_map(nib->disk_type, format);
    qtr_track_index = 0;
    while (qtr_track_index < CLEM_DISK_LIMIT_QTR_TRACKS) {
        unsigned track_sector_count = g_clem_max_sectors_per_region_35[disk_region];
        unsigned track_bytes_count = CLEM_DISK_35_CALC_BYTES_FROM_SECTORS(track_sector_count);
        //  TRK 0: (0,1) , TRK 1: (2,3), and so on. and track encoded
        unsigned logical_track_index = qtr_track_index / 2;
        unsigned logical_side_index = qtr_track_index % 2;
        unsigned nib_track_index = qtr_track_index / qtr_tracks_per_track;
        uint8_t side_index_and_track_64 = (logical_side_index << 5) | (logical_track_index >> 6);
        uint8_t sector_format = (nib->is_double_sided ? 0x20 : 0x00) | 0x2;
        // DOS/ProDOS sector index
        unsigned sector;
        unsigned temp;

        struct ClemensNibEncoder nib_encoder;

        if (nib_track_index >= nib->track_count)
            break;

        if (!clem_nib_begin_track_encoder(&nib_encoder, nib, nib_track_index, track_byte_offset,
                                          track_bytes_count))
            return false;
        clem_disk_nib_encode_track_35(&nib_encoder, logical_track_index, logical_side_index,
                                      sector_format, logical_sector_index, track_sector_count,
                                      to_logical_sector_map[disk_region], data_start);
        clem_nib_end_track_encoder(&nib_encoder, nib, nib_track_index);

        nib->meta_track_map[qtr_track_index] = nib_track_index;
        if (qtr_tracks_per_track == 2) {
            //  TODO: treated as empty?  or should we point to nib_track_index?
            //        investigate
            nib->meta_track_map[qtr_track_index + 1] = 0xff;
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

bool clem_disk_nib_encode_525(struct ClemensNibbleDisk *nib, unsigned format, unsigned dos_volume,
                              const uint8_t *data_start, const uint8_t *data_end) {
    _ClemensPhysicalSectorMap to_logical_sector_map;
    unsigned track_index;
    unsigned track_byte_offset;
    unsigned logical_sector_index;

    if (data_end - data_start < 140 * 1024)
        return false;

    if (nib->disk_type != CLEM_DISK_TYPE_5_25)
        return false;

    nib->is_double_sided = false;
    nib->bit_timing_ns = CLEM_DISK_5_25_BIT_TIMING_NS;

    track_byte_offset = 0;    // offset into nib bits data
    logical_sector_index = 0; // 256 byte sector index from 0 to 559
    to_logical_sector_map = get_physical_to_logical_sector_map(nib->disk_type, format);

    track_index = 0;
    while (track_index < CLEM_DISK_LIMIT_525_DISK_TRACKS) {
        // 5.25" tracks are grouped like so on the quarter track list (per WOZ spec)
        // QTR: |00| 01 02 03 |04| 05 06 07 |08| 09 0A 0B |0C| 0D ..
        // TRK: |00| 00 FF 01 |01| 01 FF 02 |02| 02 FF 03 |03| 03 ..
        // basically straggling the track at qtr_track_index 0, 4, 8, C.
        // sectors are 256 bytes as well (vs 512 for 3.5" disks)
        unsigned sector;

        struct ClemensNibEncoder nib_encoder;
        if (track_index >= nib->track_count)
            break;

        if (!clem_nib_begin_track_encoder(&nib_encoder, nib, track_index, track_byte_offset,
                                          CLEM_DISK_525_BYTES_PER_TRACK))
            return false;

        clem_disk_nib_encode_track_525(&nib_encoder, dos_volume, track_index, logical_sector_index,
                                       CLEM_DISK_525_NUM_SECTORS_PER_TRACK,
                                       to_logical_sector_map[0], data_start);

        clem_nib_end_track_encoder(&nib_encoder, nib, track_index);

        if (track_index != 0) {
            nib->meta_track_map[track_index * 4 - 1] = track_index;
        }
        nib->meta_track_map[track_index * 4] = track_index;
        if (track_index < CLEM_DISK_LIMIT_525_DISK_TRACKS) {
            nib->meta_track_map[track_index * 4 + 1] = track_index;
        }
        logical_sector_index += CLEM_DISK_525_NUM_SECTORS_PER_TRACK;
        track_byte_offset += CLEM_DISK_525_BYTES_PER_TRACK;
        track_index++;
    }
    return true;
}

bool clem_disk_nib_decode_35(const struct ClemensNibbleDisk *nib, unsigned format,
                             uint8_t *data_start, uint8_t *data_end) {
    _ClemensPhysicalSectorMap to_logical_sector_map;
    unsigned track_index, bits_track_index;
    unsigned logical_sector_index;
    unsigned disk_bytes_cnt;
    unsigned disk_region;
    uint8_t disk_bytes[640];

    logical_sector_index = 0;
    for (track_index = 0, bits_track_index = 0xff; track_index < CLEM_DISK_LIMIT_QTR_TRACKS;
         ++track_index) {

        // next available track? (if single sided, meta_track_map will alternate between
        // available and unavailable track mappings.)
        if (bits_track_index == nib->meta_track_map[track_index])
            continue;
        bits_track_index = nib->meta_track_map[track_index];
        if (bits_track_index == 0xff)
            continue;

        to_logical_sector_map = get_physical_to_logical_sector_map(nib->disk_type, format);
        disk_region = clem_disk_nib_get_region_from_track(nib->disk_type, track_index);
        if (!clem_disk_nib_decode_nibblized_track_35(nib, to_logical_sector_map[disk_region],
                                                     bits_track_index, logical_sector_index,
                                                     data_start, data_end)) {
            return false; // ERROR!
        }

        logical_sector_index += g_clem_max_sectors_per_region_35[disk_region];
    }

    return true;
}

bool clem_disk_nib_decode_525(const struct ClemensNibbleDisk *nib, unsigned format,
                              uint8_t *data_start, uint8_t *data_end) {
    _ClemensPhysicalSectorMap to_logical_sector_map;
    unsigned track_index, bits_track_index;
    unsigned logical_sector_index;
    unsigned disk_bytes_cnt;
    unsigned disk_region;
    uint8_t disk_bytes[640];

    logical_sector_index = 0;
    for (track_index = 0, bits_track_index = 0xff; track_index < CLEM_DISK_LIMIT_QTR_TRACKS;
         ++track_index) {

        // next available track? (if single sided, meta_track_map will alternate between
        // available and unavailable track mappings.)
        // also, since adjacent 5.25 disk quarter tracks can point to the same actual
        //  data track, we only want to decode the first reference.
        if (bits_track_index == nib->meta_track_map[track_index])
            continue;
        bits_track_index = nib->meta_track_map[track_index];
        if (bits_track_index == 0xff)
            continue;
        to_logical_sector_map = get_physical_to_logical_sector_map(nib->disk_type, format);
        disk_region = clem_disk_nib_get_region_from_track(nib->disk_type, track_index);
        if (!clem_disk_nib_decode_nibblized_track_525(nib, to_logical_sector_map[disk_region],
                                                      bits_track_index, logical_sector_index,
                                                      data_start, data_end)) {
            return false; // ERROR!
        }
        logical_sector_index += g_clem_max_sectors_per_region_35[disk_region];
    }

    return true;
}

#if defined(CLEM_SAMPLE_APP)
/** Sample App */
#include <stdio.h>
#include <stdlib.h>

static bool sample_encode_disk(struct ClemensNibbleDisk *nib_disk, unsigned format, uint8_t *data,
                               uint8_t *data_end) {
    return clem_disk_nib_encode_35(nib_disk, format, true, data, data_end);
}

static bool sample_decode_disk(uint8_t *data, uint8_t *data_end, struct ClemensNibbleDisk *nib_disk,
                               unsigned format) {
    return clem_disk_nib_decode_35(nib_disk, format, data, data_end);
}

static void sample_output(FILE *out, const uint8_t *data, const uint8_t *data_end,
                          unsigned bytes_per_line) {
    unsigned i;
    unsigned data_size = (unsigned)(data_end - data);
    const uint8_t *out_data;
    out_data = data;
    for (i = 0; i < data_size; ++i) {
        if ((i % bytes_per_line) == 0) {
            fprintf(out, "%06X: ", i);
        }
        if ((i % bytes_per_line) == (bytes_per_line - 1) || (i + 1) == data_size) {
            fprintf(out, "%02X\n", out_data[i]);
        } else {
            fprintf(out, "%02X ", out_data[i]);
        }
    }
}

int main(int argc, const char *argv[]) {
    //  generate some data to encode
    struct ClemensNibbleDisk nib_disk;
    uint8_t *source;
    uint8_t *source_end;
    uint8_t *source_p;
    uint8_t *encoded;
    unsigned region_idx, track_idx, sec_idx, i, sz;
    uint8_t *decoded;
    uint8_t *decoded_end;

    source = malloc(512 * CLEM_DISK_35_DOUBLE_PRODOS_BLOCK_COUNT);
    encoded = malloc(CLEM_DISK_35_MAX_DATA_SIZE);
    decoded = malloc(512 * CLEM_DISK_35_DOUBLE_PRODOS_BLOCK_COUNT);
    decoded_end = decoded + 512 * CLEM_DISK_35_DOUBLE_PRODOS_BLOCK_COUNT;

    printf("DISK 3.5 Single Sector Encode\n");
    printf("-----------------------------\n");
    printf("Empty Sector\n");
    source_end = source;
    for (region_idx = 0; region_idx < CLEM_DISK_35_NUM_REGIONS; ++region_idx) {
        for (track_idx = g_clem_track_start_per_region_35[region_idx];
             track_idx < g_clem_track_start_per_region_35[region_idx + 1]; ++track_idx) {
            for (sec_idx = 0; sec_idx < g_clem_max_sectors_per_region_35[region_idx]; ++sec_idx) {
                for (i = 0; i < 512; ++i) {
                    source_end[i] = sec_idx << ((track_idx & 1) ? 4 : 0);
                }
                source_end += 512;
            }
        }
    }
    memset(&nib_disk, 0, sizeof(struct ClemensNibbleDisk));
    nib_disk.disk_type = CLEM_DISK_TYPE_3_5;
    nib_disk.is_double_sided = false;
    nib_disk.is_write_protected = false;
    clem_nib_reset_tracks(&nib_disk, 2, encoded, encoded + CLEM_DISK_35_MAX_DATA_SIZE);
    sample_encode_disk(&nib_disk, CLEM_DISK_FORMAT_PRODOS, source, source_end);
    sample_decode_disk(decoded, decoded_end, &nib_disk, CLEM_DISK_FORMAT_PRODOS);

    for (track_idx = 0; track_idx < nib_disk.track_count; ++track_idx) {
        printf("Encoded track(%u), : %u bytes\n", track_idx, nib_disk.track_byte_count[track_idx]);
        sample_output(stdout, nib_disk.bits_data + nib_disk.track_byte_offset[track_idx],
                      nib_disk.bits_data + nib_disk.track_byte_offset[track_idx] +
                          nib_disk.track_byte_count[track_idx],
                      16);
    }

    source_p = decoded;
    for (region_idx = 0; region_idx < CLEM_DISK_35_NUM_REGIONS; ++region_idx) {
        for (track_idx = g_clem_track_start_per_region_35[region_idx];
             track_idx < g_clem_track_start_per_region_35[region_idx + 1]; ++track_idx) {
            for (sec_idx = 0; sec_idx < g_clem_max_sectors_per_region_35[region_idx]; ++sec_idx) {
                if (nib_disk.meta_track_map[track_idx] != 0xff) {
                    printf("Decoded track(%u), sector(%u)\n", track_idx, sec_idx);
                    sample_output(stdout, source_p, source_p + 512, 16);
                }
                source_p += 512;
            }
        }
    }

    return 0;
}
#endif
