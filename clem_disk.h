#ifndef CLEM_DISK_H
#define CLEM_DISK_H

#include <stdbool.h>
#include <stdint.h>

#define CLEM_DISK_TYPE_NONE                 0
#define CLEM_DISK_TYPE_5_25                 1
#define CLEM_DISK_TYPE_3_5                  2

/* value from woz spec - evaluate if this can be used for blank disks */
#define CLEM_DISK_DEFAULT_TRACK_BIT_LENGTH_525  51200
/* value from dsk2woz2 */
#define CLEM_DISK_BLANK_TRACK_BIT_LENGTH_525    50624

/*  Track Limit for 'nibble-like' disks (i.e. not Smartport drives) */
#define CLEM_DISK_LIMIT_QTR_TRACKS              160
/*  Always 16 sectors per track on DOS/ProDOS 5.25" disks */
#define CLEM_DISK_525_NUM_SECTORS_PER_TRACK     16

/*  3.5" drives have variable spin speed to maximize space - and these speeds
    are divided into regions, where outer regions have more sectors vs inner
    regions.  See the declared globals below
*/
#define CLEM_DISK_35_NUM_REGIONS                5

/* From ProDOS firmware for 3.5" Apple Disk Drive Format
   Routine from ROM 03 - ff/4197 - ff/428d

   Track (counts are 8-bit bytes)
    1       FF              Padding
    1000    Self Sync (GAP 1)
                            4 10-bit bytes * 200 = 800 10-bit bytes, or
                            1000 8-bit byte
                            Bytes Self Sync (GAP 1)
                            Note, this buffer may bump the track size beyond
                            the theoretical size limit on the disk.  The gaps
                            are here to take into account disk speed differences
                            in the real world.  In our world we can be
                            consistent.  let's choose 500

   Sector
     65     Self Sync (GAP 2)
                            13 strings of 4 10-bit self-syncs (13 * 5) =
                            42 10-bit bytes, or 65 8-bit bytes
     1      FF              Padding
     3      D5 AA 96        Address Prologue
     5      xx xx xx xx xx  Address Header
     2      D5 AA           Address Epilogue
     1      FF              Padding
     3      D5 AA AD        Data Prologue
     1      xx              Logical Sector
     699    xx xx xx xx ..  Data Body
     3      xx xx xx        6-2 encoded checksum
     2      DE AA           Data Epilogue
     3      FF              Padding (not for the last sector)

    = 788 bytes per sector
    = 785 bytes for the last sector
    + 400 for the track

 */
#define CLEM_DISK_35_BYTES_PER_SECTOR        788
#define CLEM_DISK_35_BYTES_PER_LAST_SECTOR   785
#define CLEM_DISK_35_BYTES_TRACK_GAP_1       768
#define CLEM_DISK_35_CALC_BYTES_FROM_SECTORS(_sectors_) ( \
    CLEM_DISK_35_BYTES_TRACK_GAP_1 + CLEM_DISK_35_BYTES_PER_LAST_SECTOR + \
    (((_sectors_) - 1) * CLEM_DISK_35_BYTES_PER_SECTOR))


#ifdef __cplusplus
extern "C" {
#endif

extern unsigned g_clem_max_sectors_per_region_35[CLEM_DISK_35_NUM_REGIONS];
extern unsigned g_clem_track_start_per_region_35[CLEM_DISK_35_NUM_REGIONS + 1];

/**
 * @brief Represents a nibbilized disk from a WOZ or 2IMG compliant image
 *
 */
struct ClemensNibbleDisk {
    unsigned disk_type;
    unsigned bit_timing_ns;             /* time to read (and write?) */
    unsigned track_count;

    bool is_write_protected;            /**< Write protected data */
    bool is_double_sided;               /**< track 1, 3, 5 represents side 2 */

    /* maps quarter tracks (for 5.25) and 80 tracks per side (for 3.25).  the
       drive head mechanism should track current head position by the meta
       index 0-159.  So for typical DOS disks, our head position will be
       0, 4, 8, 12, ....  A value of 255 is consider an undefined track
    */
    uint8_t meta_track_map[CLEM_DISK_LIMIT_QTR_TRACKS];
    /* Pay attention
        - byte offsets into the bits buffer per track relative to the end of
          the track list (vs the file, as specified in the WOZ spec)
        - bit count for the track (bytes = (count / 8) + 1 if remainder else 0)
    */
    uint32_t track_byte_offset[CLEM_DISK_LIMIT_QTR_TRACKS];
    uint32_t track_byte_count[CLEM_DISK_LIMIT_QTR_TRACKS];
    uint32_t track_bits_count[CLEM_DISK_LIMIT_QTR_TRACKS];
    uint8_t track_initialized[CLEM_DISK_LIMIT_QTR_TRACKS];

    /* This buffer is supplied by the application that can be allocated from
       values within the INFO chunk (written out to max_track_size_bytes), and
       can be released upon eject
    */
    uint8_t* bits_data;
    uint8_t* bits_data_end;
};

#ifdef __cplusplus
}
#endif

#endif
