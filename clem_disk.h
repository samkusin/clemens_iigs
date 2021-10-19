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

/* Ciderpress 3.5" sector byte estimates * 8 bits = 6180 bits? */
#define CLEM_DISK_35_BITS_PER_SECTOR    6180
#define CLEM_DISK_35_CALC_BITS_FROM_SECTORS(_sectors_) \
    ((_sectors_) * CLEM_DISK_35_BITS_PER_SECTOR)

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
