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
#include "clem_debug.h"

#include <string.h>
#include <stdbool.h>

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

static uint8_t kChunkINFO[8] = { 0x49, 0x4E, 0x46, 0x4F };
static uint8_t kChunkTMAP[8] = { 0x54, 0x4D, 0x41, 0x50 };
static uint8_t kChunkTRKS[8] = { 0x54, 0x52, 0x4B, 0x53 };
static uint8_t kChunkWRIT[8] = { 0x57, 0x52, 0x49, 0x54 };
static uint8_t kChunkMETA[8] = { 0x4D, 0x45, 0x54, 0x41 };


struct _ClemBufferIterator {
    const uint8_t* cur;
    const uint8_t* end;
};

/* NOTE: WOZ2 stores little-endian integers.  Our serialization code should
   take this into account
*/

static bool _clem_woz_check_string(
    struct _ClemBufferIterator* iter,
    const char* str,
    size_t len
) {
    const uint8_t *start, *end;
    if (iter->cur >= iter->end) return false;
    start = iter->cur;
    end = start + len < iter->end ? start + len : iter->end;
    iter->cur = end;
    return !strncmp(str, (const char*)start, end - start);
}

static size_t _clem_woz_read_bytes(
    struct _ClemBufferIterator* iter,
    uint8_t* buf,
    size_t len
) {
    const uint8_t* start = iter->cur;
    const uint8_t* end = iter->cur + len;
    end = end < iter->end ? end : iter->end;
    if (start < end) {
        memcpy(buf, start, end - start);
    }
    iter->cur = end;
    return end - start;
}

static inline uint32_t _clem_woz_read_u32(struct _ClemBufferIterator* iter) {
    const uint8_t* next = iter->cur + sizeof(uint32_t);
    uint32_t v;
    if (next > iter->end) return 0xffffffff;
    v = iter->cur[3];   v <<= 8;
    v |= iter->cur[2];  v <<= 8;
    v |= iter->cur[1];  v <<= 8;
    v |= iter->cur[0];
    iter->cur = next;
    return v;
}

static inline uint16_t _clem_woz_read_u16(struct _ClemBufferIterator* iter) {
    const uint8_t* next = iter->cur + sizeof(uint16_t);
    uint16_t v;
    if (next > iter->end) return 0xffff;
    v = iter->cur[1];   v <<= 8;
    v |= iter->cur[0];
    iter->cur = next;
    return v;
}

static inline uint8_t _clem_woz_read_u8(struct _ClemBufferIterator* iter) {
    const uint8_t* next = iter->cur + 1;
    uint8_t v;
    if (iter->cur > iter->end) return 0xff;
    v = iter->cur[0];
    iter->cur = next;
    return v;
}


static inline void _clem_woz_iter_inc(
    struct _ClemBufferIterator* iter, size_t amt
) {
    iter->cur += amt;
}

static inline void _clem_woz_iter_dec(
    struct _ClemBufferIterator* iter, size_t amt, const uint8_t* start
) {
    const uint8_t* cur = iter->cur - amt;
    if (cur > iter->end || cur < start) iter->cur = start;
    else iter->cur = cur;
}


const uint8_t* clem_woz_check_header(const uint8_t* data, size_t data_sz) {
    /*  1. validate incoming data as WOZ data and fail if not
        2. derive some common attributes from the image type
            a. track count
            b. sector count per track (each track has a sector count)
    */
    struct _ClemBufferIterator woz_iter;
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
    _clem_woz_read_u32(&woz_iter);
    return woz_iter.cur;
}

const uint8_t* clem_woz_parse_chunk_header(
    struct ClemensWOZChunkHeader* header,
    const uint8_t* data,
    size_t data_sz
) {
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

const uint8_t* clem_woz_parse_info_chunk(
    struct ClemensWOZDisk* disk,
    const struct ClemensWOZChunkHeader* header,
    const uint8_t* data,
    size_t data_sz
) {
    struct _ClemBufferIterator woz_iter;
    unsigned param, version;

    if (data_sz < header->data_size) return NULL;

    woz_iter.cur = data;
    woz_iter.end = data + header->data_size;

    version = _clem_woz_read_u8(&woz_iter);
    disk->disk_type = _clem_woz_read_u8(&woz_iter);
    if (_clem_woz_read_u8(&woz_iter) != 0) {
        disk->flags |= CLEM_WOZ_IMAGE_WRITE_PROTECT;
    }
    if (_clem_woz_read_u8(&woz_iter) != 0) {
        disk->flags |= CLEM_WOZ_IMAGE_SYNCHRONIZED;
    }
    if (_clem_woz_read_u8(&woz_iter) != 0) {
        disk->flags |= CLEM_WOZ_IMAGE_CLEANED;
    }
    _clem_woz_read_bytes(&woz_iter, disk->creator, sizeof(disk->creator));

    if (version > 1) {
        if (_clem_woz_read_u8(&woz_iter) == 2) {
            disk->flags |= CLEM_WOZ_IMAGE_DOUBLE_SIDED;
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
        disk->bit_timing_ns = _clem_woz_read_u8(&woz_iter);
        disk->flags |= _clem_woz_read_u16(&woz_iter);
        disk->required_ram_kb = _clem_woz_read_u16(&woz_iter);
        disk->max_track_size_bytes = _clem_woz_read_u16(&woz_iter) * 512;

    } else {
        if (disk->disk_type == CLEM_WOZ_DISK_5_25) {
            disk->bit_timing_ns = 4;
            disk->max_track_size_bytes = 6646;  /* v1 max track size */
        } else if (disk->disk_type == CLEM_WOZ_DISK_3_5) {
            disk->bit_timing_ns = 2;
            /* this appears to be the upper limit of all tracks on 3.5" disks
               according to experiments with WOZ files - may be overkill
            */
            disk->max_track_size_bytes = 19 * 512;
        }
        disk->boot_type = CLEM_WOZ_BOOT_UNDEFINED;
    }
    return woz_iter.end;
}

const uint8_t* clem_woz_parse_tmap_chunk(
    struct ClemensWOZDisk* disk,
    const struct ClemensWOZChunkHeader* header,
    const uint8_t* data,
    size_t data_sz
) {
    struct _ClemBufferIterator woz_iter;
    unsigned idx;
    unsigned track_idx = (unsigned)(-1);

    if (data_sz < header->data_size) return NULL;

    woz_iter.cur = data;
    woz_iter.end = data + header->data_size;

    for (idx = 0; idx < CLEM_WOZ_LIMIT_QTR_TRACKS; ++idx) {
        disk->meta_track_map[idx] = _clem_woz_read_u8(&woz_iter);
        if (disk->meta_track_map[idx] != 0xff) {
            if (track_idx == (unsigned)(-1) ||
                track_idx < disk->meta_track_map[idx]
            ) {
                track_idx = disk->meta_track_map[idx];
            }
        }
    }
    if (track_idx < (unsigned)(-1)) {
        disk->track_count = track_idx + 1;
    }

    return woz_iter.end;
}

const uint8_t* clem_woz_parse_trks_chunk(
    struct ClemensWOZDisk* disk,
    const struct ClemensWOZChunkHeader* header,
    const uint8_t* data,
    size_t data_sz
) {
    struct _ClemBufferIterator woz_iter;
    unsigned param, idx;
    unsigned last_byte_offset;

    /* WOZ files always have CLEM_WOZ_LIMIT_QTR_TRACKS entries regardless of
       disk type - it is up to our emulator to limit the used tracks based on
       disk type
    */
    if (data_sz < header->data_size) return NULL;

    woz_iter.cur = data;
    woz_iter.end = data + header->data_size;

    last_byte_offset = 0;
    for (idx = 0; idx < CLEM_WOZ_LIMIT_QTR_TRACKS; ++idx) {
        param = (uint32_t)_clem_woz_read_u16(&woz_iter) * 512;
        disk->track_byte_count[idx] = (
            (uint32_t)_clem_woz_read_u16(&woz_iter) * 512);
        disk->track_bits_count[idx] = _clem_woz_read_u32(&woz_iter);
        if (param != 0) {
            disk->track_byte_offset[idx] = param - CLEM_WOZ_OFFSET_TRACK_DATA;
        }
        last_byte_offset += disk->track_byte_count[idx];
    }

    if (disk->bits_data && disk->bits_data_end) {
        uint8_t* out_bits = disk->bits_data;
        for (idx = 0; idx < CLEM_WOZ_LIMIT_QTR_TRACKS; ++idx) {
            if (out_bits + disk->track_byte_count[idx] > disk->bits_data_end) {
                CLEM_WARN("Raw data not found for track %u." \
                    "WOZ file may be corrupt", idx);
                CLEM_ASSERT(false);
                return NULL;
            }
            _clem_woz_read_bytes(
                &woz_iter, out_bits, disk->track_byte_count[idx]);
            out_bits += disk->track_byte_count[idx];
        }
    } else {
        /* skip the raw data since the user didn't specify a bits buffer */
        _clem_woz_iter_inc(&woz_iter, last_byte_offset);
    }

    return woz_iter.end;
}

// uint8_t* clem_woz_parse_writ_chunk(uint8_t* data, size_t data_sz);

const uint8_t* clem_woz_parse_meta_chunk(
    struct ClemensWOZDisk* disk,
    const struct ClemensWOZChunkHeader* header,
    const uint8_t* data,
    size_t data_sz
) {
    struct _ClemBufferIterator woz_iter;
    unsigned param;

    if (data_sz < header->data_size) return NULL;

    woz_iter.cur = data;
    woz_iter.end = data + header->data_size;

    return woz_iter.end;
}
