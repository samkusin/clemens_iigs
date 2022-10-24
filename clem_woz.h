/**
 * @file clem_woz.h
 * @author your name (you@domain.com)
 * @brief Apple IIgs disk image utilities
 * @version 0.1
 * @date 2021-05-13
 *
 * @copyright Copyright (c) 2021
 *
 */

#ifndef CLEM_WOZ_DISK_H
#define CLEM_WOZ_DISK_H

#include "clem_disk.h"

#include <stddef.h>
#include <stdint.h>

#define CLEM_WOZ_INVALID_DATA (-1)

#define CLEM_WOZ_CHUNK_INFO 0
#define CLEM_WOZ_CHUNK_TMAP 1
#define CLEM_WOZ_CHUNK_TRKS 2
#define CLEM_WOZ_CHUNK_WRIT 3
#define CLEM_WOZ_CHUNK_META 4
#define CLEM_WOZ_CHUNK_UNKNOWN 5
#define CLEM_WOZ_CHUNK_FINISHED (unsigned)(-1)

#define CLEM_WOZ_DISK_5_25 1
#define CLEM_WOZ_DISK_3_5 2

#define CLEM_WOZ_BOOT_UNDEFINED 0
#define CLEM_WOZ_BOOT_5_25_16 1
#define CLEM_WOZ_BOOT_5_25_13 2
#define CLEM_WOZ_BOOT_5_25_MULTI 3

#define CLEM_WOZ_SUPPORT_UNKNOWN 0x0000
#define CLEM_WOZ_SUPPORT_A2 0x0001
#define CLEM_WOZ_SUPPORT_A2_PLUS 0x0002
#define CLEM_WOZ_SUPPORT_A2_E 0x0004
#define CLEM_WOZ_SUPPORT_A2_C 0x0008
#define CLEM_WOZ_SUPPORT_A2_EE 0x0010
#define CLEM_WOZ_SUPPORT_A2_GS 0x0020
#define CLEM_WOZ_SUPPORT_A2_C_PLUS 0x0040

#define CLEM_WOZ_IMAGE_DOUBLE_SIDED 0x10000000
#define CLEM_WOZ_IMAGE_CLEANED 0x20000000
#define CLEM_WOZ_IMAGE_SYNCHRONIZED 0x40000000
#define CLEM_WOZ_IMAGE_WRITE_PROTECT 0x80000000

#define CLEM_WOZ_OFFSET_TRACK_DATA_V1 256
#define CLEM_WOZ_OFFSET_TRACK_DATA_V2 1536

#ifdef __cplusplus
extern "C" {
#endif

struct ClemensWOZChunkHeader {
  size_t data_size;
  unsigned type;
};

struct ClemensWOZDisk {
  unsigned disk_type;       /* CLEM_WOZ_DISK_XXX */
  unsigned boot_type;       /* CLEM_WOZ_BOOT_XXX */
  unsigned flags; /* CLEM_WOZ_SUPPORT, CLEM_WOZ_IMAGE */
  unsigned required_ram_kb;
  unsigned max_track_size_bytes;
  unsigned version;

  /* Extra data not necessary for the backend */
  char creator[32];

  /* This is provided by the caller.  At the very least the nib->bits_data and
     nib->bits_data_end byte vector must be defined so the parser can populate
     this byte vector with nibbles
  */
  struct ClemensNibbleDisk *nib;
};

/*
    These functions are designed to be called in the following order:

        if clem_woz_check_header(buffer) is True:
            done = False
            while not done:
                header = clem_woz_parse_chunk_header(buffer)
                if not buffer or header.type is FINISHED:
                    done = True
                elif header.type is INFO:
                    disk = clem_woz_parse_info_chunk(disk, header, buffer)
                elif header.type is TMAP:
                    disk = clem_woz_parse_tmap_chunk(disk, header, buffer)
                    # by now we can preallocate raw bits data buffers in the
                    # disk object - having a valid bits_data and bits_data_limit
                    # will be necessary to read track data
                elif header.type is TRKS:
                    disk = clem_woz_parse_trks_chunk(disk, header, buffer)
                elif header.type is META:
                    disk = clem_woz_parse_meta_chunk(disk, header, buffer)

    At the conclusion of this loop, the result is either a valid disk image for
    the clemens emulator, or an incomplete/invalid image.
 */

const uint8_t *clem_woz_check_header(const uint8_t *data, size_t data_sz);

const uint8_t *clem_woz_parse_chunk_header(struct ClemensWOZChunkHeader *header,
                                           const uint8_t *data, size_t data_sz);

const uint8_t *
clem_woz_parse_info_chunk(struct ClemensWOZDisk *disk,
                          const struct ClemensWOZChunkHeader *header,
                          const uint8_t *data, size_t data_sz);

const uint8_t *
clem_woz_parse_tmap_chunk(struct ClemensWOZDisk *disk,
                          const struct ClemensWOZChunkHeader *header,
                          const uint8_t *data, size_t data_sz);

const uint8_t *
clem_woz_parse_trks_chunk(struct ClemensWOZDisk *disk,
                          const struct ClemensWOZChunkHeader *header,
                          const uint8_t *data, size_t data_sz);

// uint8_t* clem_woz_parse_writ_chunk(uint8_t* data, size_t data_sz);

const uint8_t *
clem_woz_parse_meta_chunk(struct ClemensWOZDisk *disk,
                          const struct ClemensWOZChunkHeader *header,
                          const uint8_t *data, size_t data_sz);

const uint8_t*
clem_woz_serialize(struct ClemensWOZDisk* disk, uint8_t* out, size_t* out_size);

#ifdef __cplusplus
}
#endif

#endif
