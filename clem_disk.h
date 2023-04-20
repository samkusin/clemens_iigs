#ifndef CLEM_DISK_H
#define CLEM_DISK_H

#include <stdbool.h>
#include <stdint.h>

#define CLEM_DISK_TYPE_NONE 0
#define CLEM_DISK_TYPE_5_25 1
#define CLEM_DISK_TYPE_3_5  2

#define CLEM_DISK_FORMAT_DOS    0U
#define CLEM_DISK_FORMAT_PRODOS 1U
#define CLEM_DISK_FORMAT_RAW    2U

#define CLEM_DISK_5_25_BIT_TIMING_NS 4000
#define CLEM_DISK_3_5_BIT_TIMING_NS  2000

#define CLEM_DISK_525_PRODOS_BLOCK_COUNT       280
#define CLEM_DISK_35_PRODOS_BLOCK_COUNT        800
#define CLEM_DISK_35_DOUBLE_PRODOS_BLOCK_COUNT 1600

/* value from woz spec - evaluate if this can be used for blank disks */
#define CLEM_DISK_DEFAULT_TRACK_BIT_LENGTH_525 51200
/* value from dsk2woz2 */
#define CLEM_DISK_BLANK_TRACK_BIT_LENGTH_525 50624

/*  Track Limit for 'nibble-like' disks (i.e. not Smartport drives) */
#define CLEM_DISK_LIMIT_QTR_TRACKS 160
/*  Always 16 sectors per track on DOS/ProDOS 5.25" disks */
#define CLEM_DISK_525_NUM_SECTORS_PER_TRACK 16
/*  Number of tracks on a Disk II (5.25) disk */
#define CLEM_DISK_LIMIT_525_DISK_TRACKS 35

/*  3.5" drives have variable spin speed to maximize space - and these speeds
    are divided into regions, where outer regions have more sectors vs inner
    regions.  See the declared globals below
*/
#define CLEM_DISK_35_NUM_REGIONS 5

/* From ProDOS firmware for 3.5" Apple Disk Drive Format
   Routine from ROM 03 - ff/4197 - ff/428d

   Track (counts are 8-bit bytes)
    1       FF              Padding
    500-1000    Self Sync (GAP 1)
                            4 10-bit bytes * 200 = 800 10-bit bytes, or
                            1000 8-bit byte
                            Bytes Self Sync (GAP 1)
                            Note, this buffer may bump the track size beyond
                            the theoretical size limit on the disk.  The gaps
                            are here to take into account disk speed differences
                            in the real world.  In our world we can be
                            consistent.  let's choose 500

   Sector
     53     Self Sync (GAP 2)
                            13 strings of 4 10-bit self-syncs =
                            53 8-bit bytes, or 42 10-bit bytes

     1      FF              Padding
     3      D5 AA 96        Address Prologue
     5      xx xx xx xx xx  Address Header
     2      D5 AA           Address Epilogue
     1      FF              Padding
     5      Self Sync       1 4 10-bit self-sync (5 bytes)
     1      FF              Padding
     3      D5 AA AD        Data Prologue
     1      xx              Logical Sector
     699    xx xx xx xx ..  Data Body
        Note - this equates to 512 8-bit bytes + 12 8-bit "tag" header bytes
        that are apparently unused on the IIgs (and are assumed to be bytes with
        a value of 0 for ProDOS and GS/OS - custom formatting is not supported
        unless using WOZ disks)
        Ciderpress mentions 'tag' bytes but doesn't document their purpose
        Resource below imply that the IIgs doesn't used these bytes (but the
        ROM requires that they are there.)
        http://dmweb.free.fr/?q=node/1601
        https://www.bigmessowires.com/floppy-emu/ (comments)

     4      xx xx xx xx     6-2 encoded checksum
     2      DE AA           Data Epilogue
     1      FF              Padding

    = 780 bytes per sector base
 */

#ifdef __cplusplus
extern "C" {
#endif

#ifdef __cplusplus
}
#endif

/* Note, these values are for nibbilized data that are most useful for WOZ images.

   Gaps are derived from Beneath Apple DOS/ProDOS - evaluate if DOS values
   should reflect those from Beneath Apple DOS. - anyway these are taken
   from Ciderpress and ROM 03 ProDOS block formatting disassembly

   5.25" Disks
    1: (64) somewhere between 12-85
    2: (6)  somewhere between  5-10
    3: (24) somewhere between 16-28

   3.5" Disks are documented earlier in this header.
*/
#define CLEM_DISK_525_BYTES_PER_TRACK      (13 * 512)
#define CLEM_DISK_525_BYTES_TRACK_GAP_1    64
#define CLEM_DISK_525_BYTES_TRACK_GAP_2    6
#define CLEM_DISK_525_BYTES_TRACK_GAP_3    24
#define CLEM_DISK_35_BYTES_TRACK_GAP_1     500
#define CLEM_DISK_35_BYTES_TRACK_GAP_3     53
#define CLEM_DISK_35_BYTES_PER_SECTOR_BASE 728

#define CLEM_DISK_35_BYTES_PER_SECTOR                                                              \
    (CLEM_DISK_35_BYTES_PER_SECTOR_BASE + CLEM_DISK_35_BYTES_TRACK_GAP_3)
#define CLEM_DISK_35_CALC_BYTES_FROM_SECTORS(_sectors_)                                            \
    (1 + (CLEM_DISK_35_BYTES_TRACK_GAP_1 - CLEM_DISK_35_BYTES_TRACK_GAP_3) +                       \
     ((_sectors_)*CLEM_DISK_35_BYTES_PER_SECTOR))
#define CLEM_DISK_525_MAX_DATA_SIZE (50 * CLEM_DISK_525_BYTES_PER_TRACK)
#define CLEM_DISK_35_MAX_DATA_SIZE  (160 * CLEM_DISK_35_CALC_BYTES_FROM_SECTORS(12))

/* use for read sequencing */
#define CLEM_DISK_READ_STATE_MASK  0xffff0000
#define CLEM_DISK_READ_STATE_START 0x00000000
#define CLEM_DISK_READ_STATE_QA0   0x00010000
#define CLEM_DISK_READ_STATE_QA1   0x00020000
#define CLEM_DISK_READ_STATE_QA1_1 0x00030000

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief
 *
 */
enum ClemensDriveType {
    kClemensDrive_Invalid = -1,
    kClemensDrive_3_5_D1 = 0,
    kClemensDrive_3_5_D2,
    kClemensDrive_5_25_D1,
    kClemensDrive_5_25_D2,
    kClemensDrive_Count
};

/**
 * @brief Represents a nibbilized disk from a WOZ or 2IMG compliant image
 *
 */
struct ClemensNibbleDisk {
    unsigned disk_type;     /* see CLEM_DISK_TYPE_XXX defines */
    unsigned bit_timing_ns; /* time to read (and write?) */
    unsigned track_count;

    bool is_write_protected; /**< Write protected data */
    bool is_double_sided;    /**< track 1, 3, 5 represents side 2 */

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
    uint8_t *bits_data;
    uint8_t *bits_data_end;
};

/**
 * @brief Defines the head position in a nibbilized track for reads/writes
 */
struct ClemensNibbleDiskHead {
    uint8_t *bytes;      /**< Points to current track nibblized bytes */
    unsigned track;      /**< Current track index */
    unsigned bits_index; /**< Current index into track's bits array */
    unsigned bits_limit; /**< Total number of available bits in the track */
};

typedef const unsigned (*_ClemensPhysicalSectorMap)[16];

extern unsigned g_clem_max_sectors_per_region_35[CLEM_DISK_35_NUM_REGIONS];
extern unsigned g_clem_track_start_per_region_35[CLEM_DISK_35_NUM_REGIONS + 1];

/**
 * @brief Get the logical sector map for the specified disk type and format
 *
 * @param nib
 * @param format
 * @return _ClemensPhysicalSectorMap
 */
_ClemensPhysicalSectorMap get_physical_to_logical_sector_map(unsigned disk_type, unsigned format);

/**
 * @brief Creates a reverse mapping of physical to logical sectors for the given disk type/format
 * and region.
 *
 * @param disk_type
 * @param format
 * @param disk_region For 5.25 disks, always 0
 * @return unsigned*
 */
unsigned *clem_disk_create_logical_to_physical_sector_map(unsigned *sectors, unsigned disk_type,
                                                          unsigned format, unsigned disk_region);

/**
 * @brief
 *
 * @param disk_type
 * @return unsigned
 */
unsigned clem_disk_calculate_nib_storage_size(unsigned disk_type);

/**
 * @brief Rests all track points back to empty (keeping the disk type and raw data buffer intact.)
 *
 * @param nib
 */
void clem_nib_reset_tracks(struct ClemensNibbleDisk *nib, unsigned track_count, uint8_t *bits_data,
                           uint8_t *bits_data_end);

/**
 * @brief
 *
 * @param head
 * @param disk
 * @param track
 * @return struct ClemensNibbleDiskHead*
 */
struct ClemensNibbleDiskHead *clem_disk_nib_head_init(struct ClemensNibbleDiskHead *head,
                                                      const struct ClemensNibbleDisk *disk,
                                                      unsigned track);
/**
 * @brief Inspect the next bit in the disk's bitstream
 *
 * @param head The disk head state
 * @return true  A HI pulse
 * @return false A LOW pulse
 */
bool clem_disk_nib_head_peek(struct ClemensNibbleDiskHead *head);

/**
 * @brief Advances the disk head a number of bit cells.
 *
 * @param head  The current disk head state
 * @param cells Number of cells to advance the head
 */
void clem_disk_nib_head_next(struct ClemensNibbleDiskHead *head, unsigned cells);

/**
 * @brief Reads a bit from the disk stream and advances the head
 *
 * @param head
 * @return true  A HI pulse
 * @return false A LOW pulse
 */
bool clem_disk_nib_head_read_bit(struct ClemensNibbleDiskHead *head);

/**
 * @brief Emulates a very simple read sequencer for disk nibbles
 *
 * @param latch     The current latch value
 * @param state     The current read sequencer state (see CLEM_DISK_READ_STATE_XXX)
 * @param read_bit  The bit cell pulse incoming from the disk head
 * @return uint8_t  The output latch value (bit 7 high indicates a valid disk byte)
 */
uint8_t clem_disk_nib_read_latch(unsigned *state, uint8_t latch, bool read_bit);

/**
 * @brief Manages serialization of disk nibbles into a buffer for sector/data input.
 *
 * This interface builds on top of the clem_disk_nib_xxx functions providing a
 * a complete method of reading nibbled disks without using the IWM.
 */
struct ClemensNibbleDiskReader {
    struct ClemensNibbleDiskHead head;
    unsigned read_state;
    unsigned first_sector_bits_index;
    unsigned disk_bytes_cnt;
    uint8_t disk_bytes[768];

    uint16_t track_scan_state;
    uint16_t track_scan_state_next;
    uint8_t track_is_35;
    uint8_t sector_found;
    uint8_t latch;
};

#define CLEM_NIB_TRACK_SCAN_FIND_PROLOGUE      0
#define CLEM_NIB_TRACK_SCAN_FIND_ADDRESS_35    1
#define CLEM_NIB_TRACK_SCAN_FIND_ADDRESS_525   2
#define CLEM_NIB_TRACK_SCAN_END_ADDRESS        3
#define CLEM_NIB_TRACK_SCAN_FIND_DATA_PROLOGUE 4
#define CLEM_NIB_TRACK_SCAN_READ_DATA          5
#define CLEM_NIB_TRACK_SCAN_DONE               128
#define CLEM_NIB_TRACK_SCAN_ERROR              (CLEM_NIB_TRACK_SCAN_DONE + 126)
#define CLEM_NIB_TRACK_SCAN_AT_TRACK_END       (CLEM_NIB_TRACK_SCAN_DONE + 127)

void clem_disk_nib_reader_init(struct ClemensNibbleDiskReader *reader,
                               const struct ClemensNibbleDisk *disk, unsigned track);

bool clem_disk_nib_reader_next(struct ClemensNibbleDiskReader *reader);

/**
 * @brief Writes to a nibble track, encoding the data
 *
 */
struct ClemensNibEncoder {
    uint8_t *begin;
    uint8_t *end;
    unsigned bit_index;
    unsigned bit_index_end;
    unsigned wraparound;
};

bool clem_nib_begin_track_encoder(struct ClemensNibEncoder *encoder, struct ClemensNibbleDisk *nib,
                                  unsigned nib_track_index, unsigned bits_data_offset,
                                  unsigned bits_data_size);

void clem_nib_end_track_encoder(struct ClemensNibEncoder *encoder, struct ClemensNibbleDisk *nib,
                                unsigned nib_track_index);

void clem_disk_nib_encode_track_35(struct ClemensNibEncoder *nib_encoder,
                                   unsigned logical_track_index, unsigned side_index,
                                   unsigned sector_format, unsigned logical_sector_index,
                                   unsigned track_sector_count,
                                   const unsigned *to_logical_sector_map, const uint8_t *data);

void clem_disk_nib_encode_track_525(struct ClemensNibEncoder *nib_encoder, uint8_t volume,
                                    unsigned logical_track_index, unsigned logical_sector_index,
                                    unsigned track_sector_count,
                                    const unsigned *to_logical_sector_map, const uint8_t *data);

unsigned clem_disk_nib_decode_nibblized_track_35(const struct ClemensNibbleDisk *nib,
                                                 const unsigned *logical_sector_map,
                                                 unsigned bits_track_index,
                                                 unsigned logical_sector_index, uint8_t *data_start,
                                                 uint8_t *data_end);

unsigned clem_disk_nib_decode_nibblized_track_525(const struct ClemensNibbleDisk *nib,
                                                  const unsigned *logical_sector_map,
                                                  unsigned bits_track_index,
                                                  unsigned logical_sector_index,
                                                  uint8_t *data_start, uint8_t *data_end);

bool clem_disk_nib_encode_35(struct ClemensNibbleDisk *nib, unsigned format, bool double_sided,
                             const uint8_t *data_start, const uint8_t *data_end);

bool clem_disk_nib_encode_525(struct ClemensNibbleDisk *nib, unsigned format, unsigned dos_volume,
                              const uint8_t *data_start, const uint8_t *data_end);

bool clem_disk_nib_decode_35(const struct ClemensNibbleDisk *nib, unsigned format,
                             uint8_t *data_start, uint8_t *data_end);

bool clem_disk_nib_decode_525(const struct ClemensNibbleDisk *nib, unsigned format,
                              uint8_t *data_start, uint8_t *data_end);

#ifdef __cplusplus
}
#endif

#endif
