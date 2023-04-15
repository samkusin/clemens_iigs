#include "clem_disk.h"

#include <assert.h>
#include <stddef.h>
#include <string.h>

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
// clang-format on

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
        } else if (reader->disk_bytes[2] == 0xAD && reader->disk_bytes_cnt == 3) {
            //  3.5" disks have the sector encoded in the fourth byte
            reader->track_scan_state_next = reader->track_is_35
                                                ? CLEM_NIB_TRACK_SCAN_FIND_DATA_PROLOGUE
                                                : CLEM_NIB_TRACK_SCAN_READ_DATA;
        } else if (reader->disk_bytes_cnt == 4) {
            reader->track_scan_state_next = CLEM_NIB_TRACK_SCAN_READ_DATA;
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
    nib->track_bits_count[nib_track_index] = encoder->bit_index; // encoder->bit_index_end;
    nib->track_byte_count[nib_track_index] = (encoder->bit_index + 7) / 8;
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

#define CLEM_2IMG_NIB_READER_DECODE_BYTE(_out_, _reader_, _index_)                                 \
    *(_out_) = (from_gcr_6_2_byte[(_reader_)->disk_bytes[_index_] - 0x80]);                        \
    if (*(_out_) == 0x80)                                                                          \
        return;

#define CLEM_2IMG_NIB_READER_DECODE_4_4_BYTES(_out_, _reader_, _index_)                            \
    *(_out_) = ((_reader_)->disk_bytes[_index_] & 0x55) << 1;                                      \
    *(_out_) |= ((_reader_)->disk_bytes[(_index_) + 1] & 0x55);

static void clem_2img_nib_reader_address_35(struct ClemensNibbleDiskReader *reader, uint8_t *track,
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

static void clem_2img_nib_reader_address_525(struct ClemensNibbleDiskReader *reader,
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

bool clem_disk_nib_decode_nibblized_track_35(const struct ClemensNibbleDisk *nib,
                                             unsigned bits_track_index, uint8_t *data_start,
                                             uint8_t *data_end) {
    struct ClemensNibbleDiskReader disk_reader;
    uint8_t track, sector, side, chksum;
    bool track_scan_finished;

    clem_disk_nib_reader_init(&disk_reader, nib, bits_track_index);
    track_scan_finished = false;
    while (!track_scan_finished) {
        if (!clem_disk_nib_reader_next(&disk_reader))
            continue;
        switch (disk_reader.read_state) {
        case CLEM_NIB_TRACK_SCAN_AT_TRACK_END:
            track_scan_finished = true;
            break;
        case CLEM_NIB_TRACK_SCAN_FIND_ADDRESS_35:
            clem_2img_nib_reader_address_35(&disk_reader, &track, &sector, &side, &chksum);
            break;
        }
    }

    return track_scan_finished;
}
