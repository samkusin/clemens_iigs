/**
 * @file clem_woz.c
 * @author your name (you@domain.com)
 * @brief Apple IIgs  disk image utilities
 * @version 0.1
 * @date 2021-05-13
 *
 * @copyright Copyright (c) 2021
 *
 */

#include "clem_woz.h"

#include <assert.h>
#include <stdbool.h>
#include <string.h>

/*
    References:

    WOZ2 Reference: https://applesaucefdc.com/woz/reference2/

    Beneath Apple DOS
    Understanding the Apple //e
        - formatting
        - Disk II/5.25" architecture

    140K 5.25" Format:
        35 tracks, 16 sectors/track
        256 bytes per sector
        300 RPM 4us per bit
    800K 3.5" Format:
        https://en.wikipedia.org/wiki/List_of_floppy_disk_formats#cite_note-23
        https://support.apple.com/kb/TA39910?locale=en_US&viewlocale=en_US
        80 tracks
        8-12 sectors per track (80 / 5 = 16 tracks per group),
        group 0 = 12, 1 = 11, 2 = 10, 3 = 9, 4 = 8
        512 bytes per sector
        394-590 RPM 2us per bit
*/

static uint8_t kChunkINFO[4] = {0x49, 0x4E, 0x46, 0x4F};
static uint8_t kChunkTMAP[4] = {0x54, 0x4D, 0x41, 0x50};
static uint8_t kChunkTRKS[4] = {0x54, 0x52, 0x4B, 0x53};
static uint8_t kChunkWRIT[4] = {0x57, 0x52, 0x49, 0x54};
static uint8_t kChunkMETA[4] = {0x4D, 0x45, 0x54, 0x41};

struct _ClemBufferIterator {
    const uint8_t *cur;
    const uint8_t *end;
};

/* classic CRC32 method taken from the WOZ reference Appendix A */

static uint32_t crc32_tab[] = {
    0x00000000, 0x77073096, 0xee0e612c, 0x990951ba, 0x076dc419, 0x706af48f, 0xe963a535, 0x9e6495a3,
    0x0edb8832, 0x79dcb8a4, 0xe0d5e91e, 0x97d2d988, 0x09b64c2b, 0x7eb17cbd, 0xe7b82d07, 0x90bf1d91,
    0x1db71064, 0x6ab020f2, 0xf3b97148, 0x84be41de, 0x1adad47d, 0x6ddde4eb, 0xf4d4b551, 0x83d385c7,
    0x136c9856, 0x646ba8c0, 0xfd62f97a, 0x8a65c9ec, 0x14015c4f, 0x63066cd9, 0xfa0f3d63, 0x8d080df5,
    0x3b6e20c8, 0x4c69105e, 0xd56041e4, 0xa2677172, 0x3c03e4d1, 0x4b04d447, 0xd20d85fd, 0xa50ab56b,
    0x35b5a8fa, 0x42b2986c, 0xdbbbc9d6, 0xacbcf940, 0x32d86ce3, 0x45df5c75, 0xdcd60dcf, 0xabd13d59,
    0x26d930ac, 0x51de003a, 0xc8d75180, 0xbfd06116, 0x21b4f4b5, 0x56b3c423, 0xcfba9599, 0xb8bda50f,
    0x2802b89e, 0x5f058808, 0xc60cd9b2, 0xb10be924, 0x2f6f7c87, 0x58684c11, 0xc1611dab, 0xb6662d3d,
    0x76dc4190, 0x01db7106, 0x98d220bc, 0xefd5102a, 0x71b18589, 0x06b6b51f, 0x9fbfe4a5, 0xe8b8d433,
    0x7807c9a2, 0x0f00f934, 0x9609a88e, 0xe10e9818, 0x7f6a0dbb, 0x086d3d2d, 0x91646c97, 0xe6635c01,
    0x6b6b51f4, 0x1c6c6162, 0x856530d8, 0xf262004e, 0x6c0695ed, 0x1b01a57b, 0x8208f4c1, 0xf50fc457,
    0x65b0d9c6, 0x12b7e950, 0x8bbeb8ea, 0xfcb9887c, 0x62dd1ddf, 0x15da2d49, 0x8cd37cf3, 0xfbd44c65,
    0x4db26158, 0x3ab551ce, 0xa3bc0074, 0xd4bb30e2, 0x4adfa541, 0x3dd895d7, 0xa4d1c46d, 0xd3d6f4fb,
    0x4369e96a, 0x346ed9fc, 0xad678846, 0xda60b8d0, 0x44042d73, 0x33031de5, 0xaa0a4c5f, 0xdd0d7cc9,
    0x5005713c, 0x270241aa, 0xbe0b1010, 0xc90c2086, 0x5768b525, 0x206f85b3, 0xb966d409, 0xce61e49f,
    0x5edef90e, 0x29d9c998, 0xb0d09822, 0xc7d7a8b4, 0x59b33d17, 0x2eb40d81, 0xb7bd5c3b, 0xc0ba6cad,
    0xedb88320, 0x9abfb3b6, 0x03b6e20c, 0x74b1d29a, 0xead54739, 0x9dd277af, 0x04db2615, 0x73dc1683,
    0xe3630b12, 0x94643b84, 0x0d6d6a3e, 0x7a6a5aa8, 0xe40ecf0b, 0x9309ff9d, 0x0a00ae27, 0x7d079eb1,
    0xf00f9344, 0x8708a3d2, 0x1e01f268, 0x6906c2fe, 0xf762575d, 0x806567cb, 0x196c3671, 0x6e6b06e7,
    0xfed41b76, 0x89d32be0, 0x10da7a5a, 0x67dd4acc, 0xf9b9df6f, 0x8ebeeff9, 0x17b7be43, 0x60b08ed5,
    0xd6d6a3e8, 0xa1d1937e, 0x38d8c2c4, 0x4fdff252, 0xd1bb67f1, 0xa6bc5767, 0x3fb506dd, 0x48b2364b,
    0xd80d2bda, 0xaf0a1b4c, 0x36034af6, 0x41047a60, 0xdf60efc3, 0xa867df55, 0x316e8eef, 0x4669be79,
    0xcb61b38c, 0xbc66831a, 0x256fd2a0, 0x5268e236, 0xcc0c7795, 0xbb0b4703, 0x220216b9, 0x5505262f,
    0xc5ba3bbe, 0xb2bd0b28, 0x2bb45a92, 0x5cb36a04, 0xc2d7ffa7, 0xb5d0cf31, 0x2cd99e8b, 0x5bdeae1d,
    0x9b64c2b0, 0xec63f226, 0x756aa39c, 0x026d930a, 0x9c0906a9, 0xeb0e363f, 0x72076785, 0x05005713,
    0x95bf4a82, 0xe2b87a14, 0x7bb12bae, 0x0cb61b38, 0x92d28e9b, 0xe5d5be0d, 0x7cdcefb7, 0x0bdbdf21,
    0x86d3d2d4, 0xf1d4e242, 0x68ddb3f8, 0x1fda836e, 0x81be16cd, 0xf6b9265b, 0x6fb077e1, 0x18b74777,
    0x88085ae6, 0xff0f6a70, 0x66063bca, 0x11010b5c, 0x8f659eff, 0xf862ae69, 0x616bffd3, 0x166ccf45,
    0xa00ae278, 0xd70dd2ee, 0x4e048354, 0x3903b3c2, 0xa7672661, 0xd06016f7, 0x4969474d, 0x3e6e77db,
    0xaed16a4a, 0xd9d65adc, 0x40df0b66, 0x37d83bf0, 0xa9bcae53, 0xdebb9ec5, 0x47b2cf7f, 0x30b5ffe9,
    0xbdbdf21c, 0xcabac28a, 0x53b39330, 0x24b4a3a6, 0xbad03605, 0xcdd70693, 0x54de5729, 0x23d967bf,
    0xb3667a2e, 0xc4614ab8, 0x5d681b02, 0x2a6f2b94, 0xb40bbe37, 0xc30c8ea1, 0x5a05df1b, 0x2d02ef8d};

static uint32_t crc32(uint32_t crc, const void *buf, size_t size) {
    const uint8_t *p;
    p = buf;
    crc = crc ^ ~0U;
    while (size--) {
        crc = crc32_tab[(crc ^ *p++) & 0xFF] ^ (crc >> 8);
    }
    return crc ^ ~0U;
}

/* NOTE: WOZ2 stores little-endian integers.  Our serialization code should
   take this into account
*/

static bool _clem_woz_check_string(struct _ClemBufferIterator *iter, const char *str, size_t len) {
    const uint8_t *start, *end;
    if (iter->cur >= iter->end)
        return false;
    start = iter->cur;
    end = start + len < iter->end ? start + len : iter->end;
    iter->cur = end;
    return !strncmp(str, (const char *)start, end - start);
}

static size_t _clem_woz_read_bytes(struct _ClemBufferIterator *iter, uint8_t *buf, size_t len) {
    const uint8_t *start = iter->cur;
    const uint8_t *end = iter->cur + len;
    end = end < iter->end ? end : iter->end;
    if (start < end) {
        memcpy(buf, start, end - start);
    }
    iter->cur = end;
    return end - start;
}

static inline uint32_t _clem_woz_read_u32(struct _ClemBufferIterator *iter) {
    const uint8_t *next = iter->cur + sizeof(uint32_t);
    uint32_t v;
    if (next > iter->end)
        return 0xffffffff;
    v = iter->cur[3];
    v <<= 8;
    v |= iter->cur[2];
    v <<= 8;
    v |= iter->cur[1];
    v <<= 8;
    v |= iter->cur[0];
    iter->cur = next;
    return v;
}

static inline uint16_t _clem_woz_read_u16(struct _ClemBufferIterator *iter) {
    const uint8_t *next = iter->cur + sizeof(uint16_t);
    uint16_t v;
    if (next > iter->end)
        return 0xffff;
    v = iter->cur[1];
    v <<= 8;
    v |= iter->cur[0];
    iter->cur = next;
    return v;
}

static inline uint8_t _clem_woz_read_u8(struct _ClemBufferIterator *iter) {
    const uint8_t *next = iter->cur + 1;
    uint8_t v;
    if (iter->cur > iter->end)
        return 0xff;
    v = iter->cur[0];
    iter->cur = next;
    return v;
}

static inline void _clem_woz_iter_inc(struct _ClemBufferIterator *iter, size_t amt) {
    iter->cur += amt;
}

static inline void _clem_woz_iter_dec(struct _ClemBufferIterator *iter, size_t amt,
                                      const uint8_t *start) {
    const uint8_t *cur = iter->cur - amt;
    if (cur > iter->end || cur < start)
        iter->cur = start;
    else
        iter->cur = cur;
}

const uint8_t *clem_woz_check_header(const uint8_t *data, size_t data_sz, uint32_t *crc) {
    /*  1. validate incoming data as WOZ data and fail if not
        2. derive some common attributes from the image type
            a. track count
            b. sector count per track (each track has a sector count)
    */
    struct _ClemBufferIterator woz_iter;
    uint32_t crc32v;
    char version;
    woz_iter.cur = data;
    woz_iter.end = data + data_sz;

    if (!_clem_woz_check_string(&woz_iter, "WOZ", 3)) {
        return NULL;
    }
    version = _clem_woz_read_u8(&woz_iter) - '0';
    if (version > 2 || version < 1) {
        return NULL;
    }
    if (*woz_iter.cur != 0xff) {
        return NULL;
    }
    _clem_woz_iter_inc(&woz_iter, 1);
    if (!_clem_woz_check_string(&woz_iter, "\x0a\x0d\x0a", 3)) {
        return NULL;
    }
    /* TODO: CRC32 check of the file contents */
    crc32v = _clem_woz_read_u32(&woz_iter);
    if (crc)
        *crc = crc32v;
    return woz_iter.cur;
}

const uint8_t *clem_woz_parse_chunk_header(struct ClemensWOZChunkHeader *header,
                                           const uint8_t *data, size_t data_sz) {
    struct _ClemBufferIterator woz_iter;
    uint8_t chunk_id[4];

    header->type = CLEM_WOZ_CHUNK_FINISHED;
    header->data_size = 0;

    woz_iter.cur = data;
    woz_iter.end = data + data_sz;
    if (woz_iter.cur + 8 > woz_iter.end) {
        return NULL;
    }
    _clem_woz_read_bytes(&woz_iter, chunk_id, 4);
    header->data_size = _clem_woz_read_u32(&woz_iter);

    if (!memcmp(chunk_id, kChunkINFO, 4)) {
        header->type = CLEM_WOZ_CHUNK_INFO;
    } else if (!memcmp(chunk_id, kChunkTMAP, 4)) {
        header->type = CLEM_WOZ_CHUNK_TMAP;
    } else if (!memcmp(chunk_id, kChunkTRKS, 4)) {
        header->type = CLEM_WOZ_CHUNK_TRKS;
    } else if (!memcmp(chunk_id, kChunkWRIT, 4)) {
        header->type = CLEM_WOZ_CHUNK_WRIT;
    } else if (!memcmp(chunk_id, kChunkMETA, 4)) {
        header->type = CLEM_WOZ_CHUNK_META;
    } else {
        header->type = CLEM_WOZ_CHUNK_UNKNOWN;
    }

    return woz_iter.cur;
}

const uint8_t *clem_woz_parse_info_chunk(struct ClemensWOZDisk *disk,
                                         const struct ClemensWOZChunkHeader *header,
                                         const uint8_t *data, size_t data_sz) {
    struct _ClemBufferIterator woz_iter;
    unsigned param;

    if (data_sz < header->data_size)
        return NULL;

    woz_iter.cur = data;
    woz_iter.end = data + header->data_size;

    disk->version = (uint8_t)_clem_woz_read_u8(&woz_iter);
    disk->disk_type = _clem_woz_read_u8(&woz_iter);
    disk->flags = 0;
    if (_clem_woz_read_u8(&woz_iter) != 0) {
        disk->flags |= CLEM_WOZ_IMAGE_WRITE_PROTECT;
    }
    if (_clem_woz_read_u8(&woz_iter) != 0) {
        disk->flags |= CLEM_WOZ_IMAGE_SYNCHRONIZED;
    }
    if (_clem_woz_read_u8(&woz_iter) != 0) {
        disk->flags |= CLEM_WOZ_IMAGE_CLEANED;
    }
    if (disk->nib) {
        disk->nib->is_write_protected = false;
        disk->nib->is_double_sided = false;
        if (disk->flags & CLEM_WOZ_IMAGE_WRITE_PROTECT) {
            disk->nib->is_write_protected = true;
        }
    }
    _clem_woz_read_bytes(&woz_iter, (uint8_t *)disk->creator, sizeof(disk->creator));
    if (disk->version > 1) {
        if (_clem_woz_read_u8(&woz_iter) == 2) {
            disk->flags |= CLEM_WOZ_IMAGE_DOUBLE_SIDED;
            if (disk->nib)
                disk->nib->is_double_sided = true;
        }
        param = _clem_woz_read_u8(&woz_iter);
        if (param == 1) {
            disk->boot_type = CLEM_WOZ_BOOT_5_25_16;
        } else if (param == 2) {
            disk->boot_type = CLEM_WOZ_BOOT_5_25_13;
        } else if (param == 3) {
            disk->boot_type = CLEM_WOZ_BOOT_5_25_MULTI;
        } else {
            disk->boot_type = CLEM_WOZ_BOOT_UNDEFINED;
        }
        /* WOZ timing here is in 125 ns increments */
        disk->bit_timing_ns = _clem_woz_read_u8(&woz_iter) * 125;
        disk->flags |= _clem_woz_read_u16(&woz_iter);
        disk->required_ram_kb = _clem_woz_read_u16(&woz_iter);
        disk->max_track_size_bytes = _clem_woz_read_u16(&woz_iter) * 512;
        if (disk->version > 2) {
            disk->flux_block = _clem_woz_read_u16(&woz_iter);
            disk->largest_flux_track = _clem_woz_read_u16(&woz_iter);
        }
    } else {
        if (disk->disk_type == CLEM_WOZ_DISK_5_25) {
            disk->bit_timing_ns = 4 * 1000;
            disk->max_track_size_bytes = CLEM_WOZ_DISK_5_25_TRACK_SIZE_MAX; /* v1 max track size */
        } else if (disk->disk_type == CLEM_WOZ_DISK_3_5) {
            disk->bit_timing_ns = 2 * 1000;
            disk->max_track_size_bytes = CLEM_WOZ_DISK_3_5_TRACK_SIZE_MAX;
        }
        disk->boot_type = CLEM_WOZ_BOOT_UNDEFINED;
        disk->flux_block = 0;
        disk->largest_flux_track = 0;
    }

    if (disk->disk_type == CLEM_WOZ_DISK_5_25) {
        //  must be set before call
        if (disk->nib && disk->nib->disk_type != CLEM_DISK_TYPE_5_25)
            return NULL;
    } else if (disk->disk_type == CLEM_WOZ_DISK_3_5) {
        if (disk->nib && disk->nib->disk_type != CLEM_DISK_TYPE_3_5)
            return NULL;
    }
    if (disk->nib)
        disk->nib->bit_timing_ns = disk->bit_timing_ns;
    return woz_iter.end;
}

const uint8_t *clem_woz_parse_tmap_chunk(struct ClemensWOZDisk *disk,
                                         const struct ClemensWOZChunkHeader *header,
                                         const uint8_t *data, size_t data_sz) {
    struct _ClemBufferIterator woz_iter;
    unsigned idx;
    unsigned track_idx = (unsigned)(-1);

    if (data_sz < header->data_size)
        return NULL;

    woz_iter.cur = data;
    woz_iter.end = data + header->data_size;

    for (idx = 0; idx < CLEM_DISK_LIMIT_QTR_TRACKS; ++idx) {
        disk->nib->meta_track_map[idx] = _clem_woz_read_u8(&woz_iter);
        if (disk->nib->meta_track_map[idx] != 0xff) {
            if (track_idx == (unsigned)(-1) || track_idx < disk->nib->meta_track_map[idx]) {
                track_idx = disk->nib->meta_track_map[idx];
            }
        }
    }
    if (track_idx < (unsigned)(-1)) {
        disk->nib->track_count = track_idx + 1;
    }

    return woz_iter.end;
}

const uint8_t *clem_woz_parse_trks_chunk(struct ClemensWOZDisk *disk,
                                         const struct ClemensWOZChunkHeader *header,
                                         const uint8_t *data, size_t data_sz) {
    struct _ClemBufferIterator woz_iter;
    unsigned param, idx;
    unsigned last_byte_offset;

    /* WOZ files always have CLEM_WOZ_LIMIT_QTR_TRACKS entries regardless of
       disk type - it is up to our emulator to limit the used tracks based on
       disk type
    */
    if (data_sz < header->data_size)
        return NULL;

    woz_iter.cur = data;
    woz_iter.end = data + header->data_size;

    last_byte_offset = 0;

    if (disk->version == 1) {
        if (disk->nib->bits_data && disk->nib->bits_data_end) {
            uint8_t *out_bits = disk->nib->bits_data;
            for (idx = 0; idx < disk->nib->track_count; ++idx) {
                disk->nib->track_initialized[idx] = 1;
                _clem_woz_read_bytes(&woz_iter, out_bits, disk->max_track_size_bytes);
                disk->nib->track_byte_count[idx] = _clem_woz_read_u16(&woz_iter);
                disk->nib->track_bits_count[idx] = _clem_woz_read_u16(&woz_iter);
                disk->nib->track_byte_offset[idx] = idx * disk->max_track_size_bytes;
                /* skip write hints since we won't support WOZ writing for now. */
                _clem_woz_iter_inc(&woz_iter, 6);
                out_bits += disk->max_track_size_bytes;
                if (out_bits > disk->nib->bits_data_end) {
                    assert(false);
                    return NULL;
                }
                last_byte_offset += disk->max_track_size_bytes;
            }
            for (; idx < CLEM_DISK_LIMIT_QTR_TRACKS; ++idx) {
                disk->nib->track_initialized[idx] = 0;
            }
        } else {
            assert(false);
            return NULL;
        }
    } else {
        for (idx = 0; idx < CLEM_DISK_LIMIT_QTR_TRACKS; ++idx) {
            param = (uint32_t)_clem_woz_read_u16(&woz_iter) * 512;
            disk->nib->track_byte_count[idx] = ((uint32_t)_clem_woz_read_u16(&woz_iter) * 512);
            disk->nib->track_bits_count[idx] = _clem_woz_read_u32(&woz_iter);
            if (param != 0) {
                disk->nib->track_byte_offset[idx] = param - CLEM_WOZ_OFFSET_TRACK_DATA_V2;
            }

            last_byte_offset += disk->nib->track_byte_count[idx];
        }

        if (disk->nib->bits_data && disk->nib->bits_data_end) {
            uint8_t *out_bits = disk->nib->bits_data;
            for (idx = 0; idx < CLEM_DISK_LIMIT_QTR_TRACKS; ++idx) {
                if (out_bits + disk->nib->track_byte_count[idx] > disk->nib->bits_data_end) {
                    assert(false);
                    return NULL;
                }
                if (disk->nib->track_byte_count[idx]) {
                    disk->nib->track_initialized[idx] = 1;
                    _clem_woz_read_bytes(&woz_iter, out_bits, disk->nib->track_byte_count[idx]);
                    out_bits += disk->nib->track_byte_count[idx];
                }
            }
        } else {
            /* skip the raw data since the user didn't specify a bits buffer */
            _clem_woz_iter_inc(&woz_iter, last_byte_offset);
        }
    }

    return woz_iter.end;
}

const uint8_t *clem_woz_parse_optional_chunk(struct ClemensWOZDisk *disk,
                                             const struct ClemensWOZChunkHeader *header,
                                             const uint8_t *data, size_t data_sz) {
    struct _ClemBufferIterator woz_iter;
    unsigned param;

    if (data_sz < header->data_size)
        return NULL;

    woz_iter.cur = data;
    woz_iter.end = data + header->data_size;

    return woz_iter.end;
}

const uint8_t *clem_woz_unserialize(struct ClemensWOZDisk *disk, const uint8_t *inp,
                                    size_t inp_size, unsigned max_version, int *errc) {
    uint32_t crc = 0;
    const uint8_t *bits_data_current = clem_woz_check_header(inp, inp_size, &crc);
    const uint8_t *bits_data_end;
    const uint8_t *bits_mandatory_end;
    struct ClemensWOZChunkHeader chunkHeader;

    *errc = 0;

    if (!bits_data_current) {
        return NULL;
    }
    bits_mandatory_end = NULL;
    bits_data_end = inp + inp_size;

    while ((bits_data_current = clem_woz_parse_chunk_header(
                &chunkHeader, bits_data_current, bits_data_end - bits_data_current)) != NULL) {
        switch (chunkHeader.type) {
        case CLEM_WOZ_CHUNK_INFO:
            bits_data_current = clem_woz_parse_info_chunk(disk, &chunkHeader, bits_data_current,
                                                          chunkHeader.data_size);
            if (disk->version > max_version) {
                *errc = CLEM_WOZ_UNSUPPORTED_VERSION;
            }
            if (!disk->nib) {
                //  just unserialize the INFO chunk
                bits_mandatory_end = bits_data_current;
                *errc = CLEM_WOZ_NO_NIB;
            }
            break;
        case CLEM_WOZ_CHUNK_TMAP:
            bits_data_current = clem_woz_parse_tmap_chunk(disk, &chunkHeader, bits_data_current,
                                                          chunkHeader.data_size);
            break;
        case CLEM_WOZ_CHUNK_TRKS:
            bits_data_current = clem_woz_parse_trks_chunk(disk, &chunkHeader, bits_data_current,
                                                          chunkHeader.data_size);
            /* TRKS is the last chunk we care about for now - the host application
               can use this point as a marker for mandatory vs optional data */
            bits_mandatory_end = bits_data_current;
        default:
            /* Parse out optional chunks for now */
            bits_data_current = clem_woz_parse_optional_chunk(disk, &chunkHeader, bits_data_current,
                                                              chunkHeader.data_size);
            break;
        }
        if (bits_data_current == NULL)
            *errc = CLEM_WOZ_INVALID_DATA;
        if (*errc)
            break;
    }
    return bits_mandatory_end;
}

static uint8_t kWOZ2[4] = {0x57, 0x4F, 0x5A, 0x32};

struct _ClemBufferWriteIterator {
    uint8_t *cur;
    uint8_t *end;
};

static size_t _clem_woz_write_bytes(struct _ClemBufferWriteIterator *iter, const uint8_t *buf,
                                    size_t len) {
    uint8_t *start = iter->cur;
    uint8_t *end = iter->cur + len;
    end = end < iter->end ? end : iter->end;
    if (start < end) {
        memcpy(start, buf, end - start);
    }
    iter->cur = end;
    return end - start;
}

static inline void _clem_woz_write_u32(struct _ClemBufferWriteIterator *iter, uint32_t v) {
    uint8_t *next = iter->cur + sizeof(uint32_t);
    if (next > iter->end)
        return;
    iter->cur[0] = (uint8_t)(v & 0xff);
    v >>= 8;
    iter->cur[1] = (uint8_t)(v & 0xff);
    v >>= 8;
    iter->cur[2] = (uint8_t)(v & 0xff);
    v >>= 8;
    iter->cur[3] = (uint8_t)(v & 0xff);
    v >>= 8;
    iter->cur = next;
}

static inline void _clem_woz_write_u16(struct _ClemBufferWriteIterator *iter, uint16_t v) {
    uint8_t *next = iter->cur + sizeof(uint16_t);
    if (next > iter->end)
        return;
    iter->cur[0] = (uint8_t)(v & 0xff);
    v >>= 8;
    iter->cur[1] = (uint8_t)(v & 0xff);
    v = iter->cur[1];
    iter->cur = next;
}

static inline void _clem_woz_write_u8(struct _ClemBufferWriteIterator *iter, uint8_t v) {
    uint8_t *next = iter->cur + 1;
    if (iter->cur > iter->end)
        ;
    iter->cur[0] = v;
    iter->cur = next;
}

static inline void _clem_woz_write_iter_inc(struct _ClemBufferWriteIterator *iter, size_t amt) {
    iter->cur += amt;
}

static void _clem_woz_write_zero(struct _ClemBufferWriteIterator *iter, size_t len) {
    unsigned i;
    for (i = 0; i < len; ++i) {
        _clem_woz_write_u8(iter, 0);
    }
}

static inline void _clem_woz_write_chunk_start(struct _ClemBufferWriteIterator *iter,
                                               struct _ClemBufferWriteIterator *saved,
                                               const uint8_t *chunk_id) {
    _clem_woz_write_bytes(iter, chunk_id, 4);
    saved->cur = iter->cur;
    saved->end = iter->end;
    _clem_woz_write_iter_inc(iter, 4);
}

static inline size_t
_clem_woz_write_chunk_finish(struct _ClemBufferWriteIterator *iter,
                             struct _ClemBufferWriteIterator *iter_chunk_start) {
    size_t sz = iter->cur - iter_chunk_start->cur - 4;
    _clem_woz_write_u32(iter_chunk_start, sz);
    return sz;
}

uint8_t *clem_woz_serialize(struct ClemensWOZDisk *disk, uint8_t *out, size_t *out_size) {
    size_t out_limit = *out_size;
    unsigned block_cnt, write_cnt, i;
    uint32_t crc = 0;
    uint16_t track_idx, block_idx;

    /*  version 2 output - even if the input comes from version 1 */
    struct _ClemBufferWriteIterator iter;
    struct _ClemBufferWriteIterator iter_chunk_start;
    struct _ClemBufferWriteIterator iter_crc;
    iter.cur = out;
    iter.end = out + out_limit;

    /* WOZ2 header */
    _clem_woz_write_bytes(&iter, kWOZ2, sizeof(kWOZ2));
    _clem_woz_write_u8(&iter, 0xff);
    _clem_woz_write_u8(&iter, 0x0a);
    _clem_woz_write_u8(&iter, 0x0d);
    _clem_woz_write_u8(&iter, 0x0a);
    /* skip crc32 until we can calculate it */
    memcpy(&iter_crc, &iter, sizeof(iter_crc));
    _clem_woz_write_iter_inc(&iter, 4);

    /* INFO min version 2, otherwise maintain 2.1 or later */
    _clem_woz_write_chunk_start(&iter, &iter_chunk_start, kChunkINFO);
    _clem_woz_write_u8(&iter, disk->version < CLEM_WOZ_SUPPORTED_VERSION
                                  ? CLEM_WOZ_SUPPORTED_VERSION
                                  : disk->version);
    _clem_woz_write_u8(&iter, (uint8_t)(disk->disk_type));
    _clem_woz_write_u8(&iter, (disk->flags & CLEM_WOZ_IMAGE_WRITE_PROTECT) ? 1 : 0);
    _clem_woz_write_u8(&iter, (disk->flags & CLEM_WOZ_IMAGE_SYNCHRONIZED) ? 1 : 0);
    _clem_woz_write_u8(&iter, (disk->flags & CLEM_WOZ_IMAGE_CLEANED) ? 1 : 0);
    _clem_woz_write_bytes(&iter, (uint8_t *)disk->creator, sizeof(disk->creator));
    _clem_woz_write_u8(&iter, (disk->flags & CLEM_WOZ_IMAGE_DOUBLE_SIDED) ? 2 : 1);
    _clem_woz_write_u8(&iter, (uint8_t)(disk->boot_type));
    _clem_woz_write_u8(&iter, (uint8_t)(disk->bit_timing_ns / 125));
    _clem_woz_write_u16(&iter, (uint16_t)(disk->flags & 0xffff));
    _clem_woz_write_u16(&iter, (uint16_t)(disk->required_ram_kb));
    _clem_woz_write_u16(&iter, (uint16_t)((disk->max_track_size_bytes + 511) / 512));
    if (disk->version > 2) {
        _clem_woz_write_u16(&iter, disk->flux_block);
        _clem_woz_write_u16(&iter, disk->largest_flux_track);
        _clem_woz_write_zero(&iter, 10); /* should be 10 bytes as of 2.1 */
    } else {
        _clem_woz_write_zero(&iter, 14); /* should be 14 bytes as of 2.0 */
    }
    if (_clem_woz_write_chunk_finish(&iter, &iter_chunk_start) != 60) {
        *out_size = iter.cur - out;
        return NULL;
    }
    if (disk->nib) {
        /* TMAP - derived from the disk->nib data - this must start at offset 80
           of the file
        */
        _clem_woz_write_chunk_start(&iter, &iter_chunk_start, kChunkTMAP);
        _clem_woz_write_bytes(&iter, disk->nib->meta_track_map, CLEM_DISK_LIMIT_QTR_TRACKS);
        _clem_woz_write_chunk_finish(&iter, &iter_chunk_start);

        /* TRKS - derived from the disk->nib data - the block index starts at 3
                  since the first chunk of bits data is located at byte offset 1536
                  per specification.
        */
        _clem_woz_write_chunk_start(&iter, &iter_chunk_start, kChunkTRKS);
        for (track_idx = 0, block_idx = 3; track_idx < CLEM_DISK_LIMIT_QTR_TRACKS; ++track_idx) {
            /* write TRK 8 bytes */
            if (track_idx < disk->nib->track_count) {
                block_cnt = (disk->nib->track_byte_count[track_idx] + 511) / 512;
                _clem_woz_write_u16(&iter, block_idx);
                _clem_woz_write_u16(&iter, (uint16_t)(block_cnt & 0xffff));
                _clem_woz_write_u32(&iter, disk->nib->track_bits_count[track_idx]);
                block_idx += block_cnt;
            } else {
                _clem_woz_write_u16(&iter, 0);
                _clem_woz_write_u16(&iter, 0);
                _clem_woz_write_u32(&iter, 0);
            }
        }
        for (track_idx = 0, block_idx = 3; track_idx < CLEM_DISK_LIMIT_QTR_TRACKS; ++track_idx) {
            /* BITS */
            *out_size = iter.cur - out;
            if (track_idx < disk->nib->track_count) {
                if (iter.cur - out != block_idx * 512) {
                    return iter.cur;
                }
                block_cnt = (disk->nib->track_byte_count[track_idx] + 511) / 512;
                write_cnt = block_cnt * 512;
                _clem_woz_write_bytes(
                    &iter, disk->nib->bits_data + disk->nib->track_byte_offset[track_idx],
                    disk->nib->track_byte_count[track_idx]);
                _clem_woz_write_zero(&iter, write_cnt - disk->nib->track_byte_count[track_idx]);
                block_idx += block_cnt;
            }
        }
        _clem_woz_write_chunk_finish(&iter, &iter_chunk_start);
    }

    /* Up to this point is the minimal WOZ compliant file.  Other data can be
       serialized after this point (META/WRIT/FLUX) all TODOs

       CRC32 written out at the end
    */
    if (disk->extra_data_start != disk->extra_data_end) {
        _clem_woz_write_bytes(&iter, disk->extra_data_start,
                              disk->extra_data_end - disk->extra_data_start);
    }
    crc = crc32(crc, iter_crc.cur + 4, iter.cur - iter_crc.cur - 4);
    _clem_woz_write_u32(&iter_crc, crc);
    *out_size = iter.cur - out;
    return iter.cur;
}
