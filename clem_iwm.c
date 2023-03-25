/**
 * @file clem_iwm.c
 * @author your name (you@domain.com)
 * @brief
 * @version 0.1
 * @date 2021-05-12
 *
 * @copyright Copyright (c) 2021
 *
 */

#include "clem_debug.h"
#include "clem_device.h"
#include "clem_drive.h"
#include "clem_mmio_defs.h"
#include "clem_mmio_types.h"
#include "clem_shared.h"
#include "clem_smartport.h"
#include "clem_types.h"
#include "clem_util.h"

#include <inttypes.h>
#include <stdio.h>
#include <string.h>

/*
    IWM emulation

    Interface:
        iwm_reset
        iwm_glu_sync
        iwm_write_switch
        iwm_read_switch

    Feeds/Lines:
        io_flags + phase - Disk Port
        Data Bus
        IO Switches
        Clock

    Notes from the 1982 Spec
    http://www.brutaldeluxe.fr/documentation/iwm/apple2_IWM_Spec_Rev19_1982.pdf

    - Reads and writes to drive (GCR encoded 8-bit 'nibbles')
    - Effectively a state machine controlled by Q6+Q7 (two internal flags)
    - Supplementary features controlled by the IO DISKREG and IWM mode
      registers
    - States
        - READ and WRITE DATA states
        - READ STATUS
        - READ HANDSHAKE
        - WRITE MODE

    - READ DATA
        - Wait for read pulse
        - If pulse wait 3 lss cycles
        - Wait for read pulse for up to 8 lss cycles for another pulse
        - If not shift left 1,0


        - Sync latch with "data" bus
        - If in latch hold mode, do not sync

    - READ STATUS
        - On transition to READ STATUS, resets Write Sequencing

    - WRITE DATA
        Every 4us (2us in fast mode), load data into latch if Q6 + Q7 ON
        Every 4us (2us in fast mode), shift left latch if Q6 OFF, Q7 ON
        If Bit 7 is ON, write pulse
        This loops continuously during the WRITE state

*/

#define CLEM_IWM_STATE_READ_DATA       0x00
#define CLEM_IWM_STATE_READ_STATUS     0x01
#define CLEM_IWM_STATE_WRITE_MASK      0x02
#define CLEM_IWM_STATE_WRITE_HANDSHAKE 0x02
#define CLEM_IWM_STATE_WRITE_MODE      0x03
#define CLEM_IWM_STATE_UNKNOWN         0xFF

// Defines how long the IWM should report it's busy for emulator hosts to support
// optimizations like fast disk emulation.
#define CLEM_IWM_DATA_ACCESS_NS_EXPIRATION (500000000);

//  Only for debugging.
#define CLEM_IWM_FILE_LOGGING 1

/* Cribbed this convenient table from
   https://github.com/whscullin/apple2js/blob/f4b0100c98c2c12988f64ffe44426fcdd5ae901b/js/cards/disk2.ts#L107

   The below is a combination of read and write lss commands compiled originally from Jim Sather's
   Understanding the Apple IIe.  In fact much of the IWM implementation relies on Sather's book, the
   IIgs Hardware Reference, the IWM specification and later books like the SWIM chip reference.
*/
static uint8_t s_lss_rom[256] = {
    0x18, 0x18, 0x18, 0x18, 0x0A, 0x0A, 0x0A, 0x0A, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18,
    0x2D, 0x2D, 0x38, 0x38, 0x0A, 0x0A, 0x0A, 0x0A, 0x28, 0x28, 0x28, 0x28, 0x28, 0x28, 0x28, 0x28,
    0xD8, 0x38, 0x08, 0x28, 0x0A, 0x0A, 0x0A, 0x0A, 0x39, 0x39, 0x39, 0x39, 0x3B, 0x3B, 0x3B, 0x3B,
    0xD8, 0x48, 0x48, 0x48, 0x0A, 0x0A, 0x0A, 0x0A, 0x48, 0x48, 0x48, 0x48, 0x48, 0x48, 0x48, 0x48,
    0xD8, 0x58, 0xD8, 0x58, 0x0A, 0x0A, 0x0A, 0x0A, 0x58, 0x58, 0x58, 0x58, 0x58, 0x58, 0x58, 0x58,
    0xD8, 0x68, 0xD8, 0x68, 0x0A, 0x0A, 0x0A, 0x0A, 0x68, 0x68, 0x68, 0x68, 0x68, 0x68, 0x68, 0x68,
    0xD8, 0x78, 0xD8, 0x78, 0x0A, 0x0A, 0x0A, 0x0A, 0x78, 0x78, 0x78, 0x78, 0x78, 0x78, 0x78, 0x78,
    0xD8, 0x88, 0xD8, 0x88, 0x0A, 0x0A, 0x0A, 0x0A, 0x08, 0x08, 0x88, 0x88, 0x08, 0x08, 0x88, 0x88,
    0xD8, 0x98, 0xD8, 0x98, 0x0A, 0x0A, 0x0A, 0x0A, 0x98, 0x98, 0x98, 0x98, 0x98, 0x98, 0x98, 0x98,
    0xD8, 0x29, 0xD8, 0xA8, 0x0A, 0x0A, 0x0A, 0x0A, 0xA8, 0xA8, 0xA8, 0xA8, 0xA8, 0xA8, 0xA8, 0xA8,
    0xCD, 0xBD, 0xD8, 0xB8, 0x0A, 0x0A, 0x0A, 0x0A, 0xB9, 0xB9, 0xB9, 0xB9, 0xBB, 0xBB, 0xBB, 0xBB,
    0xD9, 0x59, 0xD8, 0xC8, 0x0A, 0x0A, 0x0A, 0x0A, 0xC8, 0xC8, 0xC8, 0xC8, 0xC8, 0xC8, 0xC8, 0xC8,
    0xD9, 0xD9, 0xD8, 0xA0, 0x0A, 0x0A, 0x0A, 0x0A, 0xD8, 0xD8, 0xD8, 0xD8, 0xD8, 0xD8, 0xD8, 0xD8,
    0xD8, 0x08, 0xE8, 0xE8, 0x0A, 0x0A, 0x0A, 0x0A, 0xE8, 0xE8, 0xE8, 0xE8, 0xE8, 0xE8, 0xE8, 0xE8,
    0xFD, 0xFD, 0xF8, 0xF8, 0x0A, 0x0A, 0x0A, 0x0A, 0xF8, 0xF8, 0xF8, 0xF8, 0xF8, 0xF8, 0xF8, 0xF8,
    0xDD, 0x4D, 0xE0, 0xE0, 0x0A, 0x0A, 0x0A, 0x0A, 0x88, 0x88, 0x08, 0x08, 0x88, 0x88, 0x08, 0x08};

static struct ClemensDrive *_clem_iwm_select_drive(struct ClemensDeviceIWM *iwm,
                                                   struct ClemensDriveBay *drive_bay) {
    // TODO: nit optimization, could cache the current drive pointer and alter
    //       its value based on state changes described below versus per frame
    //       since this is called every frame
    struct ClemensDrive *drives;
    if (iwm->io_flags & CLEM_IWM_FLAG_DRIVE_35) {
        drives = drive_bay->slot5;
    } else {
        drives = drive_bay->slot6;
    }
    if (iwm->io_flags & CLEM_IWM_FLAG_DRIVE_1) {
        return &drives[0];
    } else if (iwm->io_flags & CLEM_IWM_FLAG_DRIVE_2) {
        return &drives[1];
    }
    return NULL;
}

static clem_clocks_duration_t _clem_iwm_select_clocks_step(struct ClemensDeviceIWM *iwm) {
    return iwm->fast_mode ? CLEM_IWM_SYNC_CLOCKS_FAST : CLEM_IWM_SYNC_CLOCKS_NORMAL;
}

#if CLEM_IWM_FILE_LOGGING
#define CLEM_IWM_DEBUG_RECORD_LIMIT 4096
static FILE *s_clem_iwm_log_f = NULL;
struct ClemensIWMDebugRecord {
    uint64_t t;
    char code[8];
    uint8_t data_r;
    uint8_t data_w;
    uint8_t latch;
    uint8_t mode; // over-used miscellaneous state covering multiple properties
    uint8_t drive_byte;
    uint8_t iwm_state;
    int qtr_track_index;
    unsigned track_byte_index;
    unsigned track_bit_shift;
    unsigned track_bit_length;
    unsigned lss_state;
};
struct ClemensIWMDebugRecord s_clem_iwm_debug_records[CLEM_IWM_DEBUG_RECORD_LIMIT];
unsigned s_clem_iwm_debug_count;

static const char *s_state_names[] = {"READ", "STAT", "HAND", "WRIT"};

static void _clem_iwm_debug_print(FILE *fp, struct ClemensIWMDebugRecord *record) {
    bool is_write_mode = (record->mode & 0x80) != 0;
    fprintf(fp, "[%20" PRIu64 "] %c, %s, %s, %s, D%u, Q%d, %u, %u, %u, %02X, ", record->t,
            record->mode & 0x08 ? 'F' : 'S', record->code, s_state_names[record->iwm_state & 0x03],
            (record->mode & 0x20)   ? "SMAR"
            : (record->mode & 0x04) ? " 3.5"
                                    : "5.25",
            (record->mode & 0x03), record->qtr_track_index, record->track_byte_index,
            record->track_bit_shift, record->track_bit_length, record->drive_byte);
    if (is_write_mode) {
        fprintf(fp, "W, %02X, %02X, %c, %c, %08X\n", record->data_w, record->latch,
                record->mode & 0x40 ? '1' : '0', record->mode & 0x10 ? '1' : '0',
                record->lss_state);
    } else {
        fprintf(fp, "R, %02X, %02X, %c, -, %08X\n", record->data_r, record->latch,
                record->mode & 0x40 ? '1' : '0', record->lss_state);
    }
}

static void _clem_iwm_debug_flush(struct ClemensDeviceIWM *iwm) {
    unsigned i;
    for (i = 0; i < s_clem_iwm_debug_count; ++i) {
        struct ClemensIWMDebugRecord *record = &s_clem_iwm_debug_records[i];
        _clem_iwm_debug_print(s_clem_iwm_log_f, record);
    }
    fflush(s_clem_iwm_log_f);
    s_clem_iwm_debug_count = 0;
}

static void _clem_iwm_debug_record(struct ClemensIWMDebugRecord *record,
                                   struct ClemensDeviceIWM *iwm, struct ClemensDrive *drive,
                                   const char *prefix, clem_clocks_time_t t) {
    record->t = t / CLEM_CLOCKS_14MHZ_CYCLE;
    strncpy(record->code, prefix, sizeof(record->code) - 1);
    record->code[sizeof(record->code) - 1] = '\0';
    record->data_r = iwm->data_r;
    record->data_w = iwm->data_w;
    record->latch = iwm->latch;
    record->iwm_state = iwm->state;
    record->mode = (iwm->io_flags & CLEM_IWM_FLAG_DRIVE_1)   ? 1
                   : (iwm->io_flags & CLEM_IWM_FLAG_DRIVE_2) ? 2
                                                             : 0;
    if (iwm->io_flags & CLEM_IWM_FLAG_DRIVE_35)
        record->mode |= 0x04;
    if (iwm->fast_mode)
        record->mode |= 0x08;
    if (iwm->io_flags & CLEM_IWM_FLAG_WRITE_REQUEST) {
        bool write_pulse =
            (iwm->io_flags & CLEM_IWM_FLAG_WRITE_HEAD_ON) == (CLEM_IWM_FLAG_WRITE_HEAD_ON);
        record->lss_state = iwm->write_state;
        record->mode |= 0x80;
        if (iwm->io_flags & CLEM_IWM_FLAG_WRITE_DATA)
            record->mode |= 0x40;
        if (drive->write_pulse != write_pulse)
            record->mode |= 0x10;
    } else {
        record->lss_state = iwm->read_state;
        if (iwm->io_flags & CLEM_IWM_FLAG_READ_DATA)
            record->mode |= 0x40;
    }
    if (iwm->smartport_active) {
        record->mode |= 0x20;
    }
    record->qtr_track_index = drive->qtr_track_index;
    record->track_byte_index = drive->track_byte_index;
    record->track_bit_shift = drive->track_bit_shift;
    record->track_bit_length = drive->track_bit_length;
    record->drive_byte = drive->current_byte;

    //_clem_iwm_debug_print(stdout, record);
}

static void _clem_iwm_debug_event(struct ClemensDeviceIWM *iwm, struct ClemensDriveBay *drive_bay,
                                  const char *prefix, clem_clocks_time_t t) {
    struct ClemensDrive *drive = _clem_iwm_select_drive(iwm, drive_bay);
    struct ClemensIWMDebugRecord *record = &s_clem_iwm_debug_records[s_clem_iwm_debug_count];
    _clem_iwm_debug_record(record, iwm, drive, prefix, t);

    if (++s_clem_iwm_debug_count >= CLEM_IWM_DEBUG_RECORD_LIMIT) {
        _clem_iwm_debug_flush(iwm);
    }
}

#define CLEM_IWM_DEBUG_EVENT(_iwm_, _drives_, _prefix_, _t_, _lvl_)                                \
    if ((_iwm_)->enable_debug && (_lvl_) <= (_iwm_)->debug_level) {                                \
        _clem_iwm_debug_event(_iwm_, _drives_, _prefix_, _t_);                                     \
    }

#else
#define CLEM_IWM_DEBUG_EVENT(_iwm_, _drives_, _prefix_, _t_, _lvl_)
#endif

void clem_iwm_debug_start(struct ClemensDeviceIWM *iwm) {
    iwm->enable_debug = true;
#if CLEM_IWM_FILE_LOGGING
    if (s_clem_iwm_log_f == NULL) {
        s_clem_iwm_log_f = fopen("iwm.log", "wt");
        s_clem_iwm_debug_count = 0;
    }
#endif
}

void clem_iwm_debug_stop(struct ClemensDeviceIWM *iwm) {
    iwm->enable_debug = false;
#if CLEM_IWM_FILE_LOGGING
    if (s_clem_iwm_log_f != NULL) {
        _clem_iwm_debug_flush(iwm);
        fclose(s_clem_iwm_log_f);
        s_clem_iwm_log_f = NULL;
    }
#endif
}

void clem_iwm_reset(struct ClemensDeviceIWM *iwm, struct ClemensTimeSpec *tspec) {
    memset(iwm, 0, sizeof(*iwm));
    iwm->cur_clocks_ts = tspec->clocks_spent;
    iwm->clocks_this_step = _clem_iwm_select_clocks_step(iwm);
    iwm->clocks_used_this_step = 0;
    iwm->cur_clocks_ts = tspec->clocks_spent;
    iwm->clocks_at_next_scanline = CLEM_VGC_HORIZ_SCAN_PHI0_CYCLES * CLEM_CLOCKS_PHI0_CYCLE;
    iwm->scanline_phase_ctr = 0;
    iwm->state = CLEM_IWM_STATE_UNKNOWN;
    iwm->debug_level = 1;
}

void clem_iwm_insert_disk(struct ClemensDeviceIWM *iwm, struct ClemensDrive *drive,
                          struct ClemensNibbleDisk *disk) {
    drive->has_disk = disk->track_count > 0;
    drive->pulse_clocks_dt = 0;
    if (disk->bit_timing_ns == 4000) {
        drive->pulse_clocks_dt = CLEM_IWM_SYNC_CLOCKS_NORMAL;
    } else if (disk->bit_timing_ns == 2000) {
        drive->pulse_clocks_dt = CLEM_IWM_SYNC_CLOCKS_FAST;
    }
    if (drive->pulse_clocks_dt > 0) {
        memcpy(&drive->disk, disk, sizeof(drive->disk));
    } else {
        CLEM_ASSERT(drive->pulse_clocks_dt > 0);
        drive->has_disk = false;
    }
}

void clem_iwm_eject_disk(struct ClemensDeviceIWM *iwm, struct ClemensDrive *drive,
                         struct ClemensNibbleDisk *disk) {
    if (drive->disk.disk_type != CLEM_DISK_TYPE_NONE) {
        memcpy(disk, &drive->disk, sizeof(drive->disk));
        if (drive->disk.disk_type == CLEM_DISK_TYPE_3_5) {
            drive->status_mask_35 &= ~CLEM_IWM_DISK35_STATUS_EJECTING;
            drive->status_mask_35 |= CLEM_IWM_DISK35_STATUS_EJECTED;
        }
        drive->has_disk = false;
    }
    memset(&drive->disk, 0, sizeof(drive->disk));
}

bool clem_iwm_eject_disk_async(struct ClemensDeviceIWM *iwm, struct ClemensDrive *drive,
                               struct ClemensNibbleDisk *disk) {

    if (drive->disk.disk_type == CLEM_DISK_TYPE_3_5) {
        if (drive->has_disk) {
            if (!(drive->status_mask_35 & CLEM_IWM_DISK35_STATUS_EJECTING)) {
                clem_disk_35_start_eject(drive);
                return false;
            }
        }
    }
    clem_iwm_eject_disk(iwm, drive, disk);
    return true;
}

bool clem_iwm_is_active(struct ClemensDeviceIWM *iwm, struct ClemensDriveBay *drives) {
    struct ClemensDrive *drive;
    if (!(iwm->io_flags & CLEM_IWM_FLAG_DRIVE_ON))
        return false;
    // check smartport drive
    if (iwm->smartport_active && drives->smartport->device.device_id != 0)
        return true;
    drive = _clem_iwm_select_drive(iwm, drives);
    if (!drive || !drive->has_disk || !drive->is_spindle_on)
        return false;
    if (iwm->data_access_time_ns == 0)
        return false;
    return true;
}

static void _clem_iwm_reset_drive(struct ClemensDeviceIWM *iwm, struct ClemensDriveBay *drives) {
    struct ClemensDrive *drive = _clem_iwm_select_drive(iwm, drives);
    iwm->drive_hold_ns = 0;
    if (drive) {
        clem_disk_start_drive(drive);
    }
}

#define CLEM_IWM_WRITE_REG_STATUS_MASK    0xffff0000
#define CLEM_IWM_WRITE_REG_COUNTER_MASK   0x0000ffff
#define CLEM_IWM_WRITE_REG_DATA           0x00010000
#define CLEM_IWM_WRITE_REG_LATCH          0x00100000
#define CLEM_IWM_WRITE_REG_LATCH_QA       0x80000000
#define CLEM_IWM_WRITE_REG_ASYNC_UNDERRUN 0x02000000
#define CLEM_IWM_WRITE_REG_ASYNC_ACTIVE   0x04000000

static void _clem_iwm_lss_write_log(struct ClemensDeviceIWM *iwm, clem_clocks_time_t ts,
                                    const char *prefix) {
    unsigned ns_write = clem_calc_ns_step_from_clocks(ts - iwm->last_write_clocks_ts);
    CLEM_LOG("IWM: [%s] write latch %08X, duration dt = %.3f us, flags=%08X, "
             "counter=%u",
             prefix, iwm->latch, ns_write * 0.001f,
             iwm->lss_write_reg & CLEM_IWM_WRITE_REG_STATUS_MASK,
             iwm->lss_write_reg & ~CLEM_IWM_WRITE_REG_STATUS_MASK);
}

static void _clem_iwm_drive_switch(struct ClemensDeviceIWM *iwm, struct ClemensDriveBay *drives,
                                   unsigned io_flags) {
    struct ClemensDrive *drive;
    if (io_flags == iwm->io_flags)
        return;
    drive = _clem_iwm_select_drive(iwm, drives);
    if (drive) {
        clem_disk_start_drive(drive);
    }
    iwm->io_flags = io_flags;
}

static void _clem_drive_off(struct ClemensDeviceIWM *iwm, struct ClemensDriveBay *drives) {
    _clem_iwm_drive_switch(iwm, drives, iwm->io_flags & ~CLEM_IWM_FLAG_DRIVE_ON);
    CLEM_DEBUG("IWM: turning drive off now");
}

static uint8_t _clem_iwm_read_status(struct ClemensDeviceIWM *iwm) {
    uint8_t result = 0;

    if (iwm->io_flags & CLEM_IWM_FLAG_DRIVE_ON && iwm->io_flags & CLEM_IWM_FLAG_DRIVE_ANY) {
        result |= 0x20;
    }

    if (iwm->io_flags & CLEM_IWM_FLAG_WRPROTECT_SENSE) {
        result |= 0x80;
    }
    /* mode flags reflected here */
    // if (iwm->clock_8mhz) {
    //     result |= 0x10;
    // }
    if (iwm->fast_mode) {
        result |= 0x08;
    }
    if (iwm->timer_1sec_disabled) {
        result |= 0x04;
    }
    if (iwm->async_mode) {
        result |= 0x02;
    }
    if (iwm->latch_mode) {
        result |= 0x01;
    }

    return result;
}

/*  Updates the IWM
 *
 *  Specification Notes:
 *      - Q3 clock (2 MHZ from Mega II per spec) for synchronous mode
 *      - FCLK (7 MHZ from the master oscillator) for asynchronous mode
 *
 *  Emulation:
 *      The IWM is a self contained state machine driven by memory mapped I/O and
 *      `drives` on the emulated diskport.   The GLU receives the system
 *      ClemensTimespec `tspec`, which provides a reference clock for both Q3 and
 *      FCLK.  The clock selected depends on the data retrieval mode (synchronous
 *      vs. asynchronous.)
 *
 *      The old implementation advanced the disk by a fixed amount until the
 *      time budget between frames was spent.
 *
 *      Thw new implementation reads a disk one cell at a time based on the IWM
 *      speed (fast vs slow)
 */

static void _clem_iwm_async_write_step(struct ClemensDeviceIWM *iwm, clem_clocks_time_t ts) {}

static void _clem_iwm_write_step(struct ClemensDeviceIWM *iwm) {
    //  This procedure covers a whole bit cell cycle
    //  CLEM_IWM_WRITE_REG_LATCH_QA will oscillate if the the MSB of the latch is HI.
    //  if it's LO, CLEM_IWM_WRITE_REG_LATCH_QA will not change, abd the WRITE_DATA signal will
    //  as a result stay the same.
    //
    //  writes to drives occur when the signal transitions from lo to hi and vice-versa.
    //  no flux if the signal remains at the same level
    unsigned counter = iwm->write_state & CLEM_IWM_WRITE_REG_COUNTER_MASK;
    if (iwm->write_state & CLEM_IWM_WRITE_REG_DATA) {
        iwm->latch = iwm->data_w;
        iwm->write_state &= ~CLEM_IWM_WRITE_REG_DATA;
        iwm->write_state |= CLEM_IWM_WRITE_REG_LATCH;
        counter = 0;
    } else {
        counter++;
        iwm->latch <<= 1;
    }
    iwm->write_state &= CLEM_IWM_WRITE_REG_STATUS_MASK;
    iwm->write_state |= (counter & 0xffff);
    if (iwm->latch & 0x80) {
        iwm->write_state ^= CLEM_IWM_WRITE_REG_LATCH_QA;
    }

    if (iwm->write_state & CLEM_IWM_WRITE_REG_LATCH_QA) {
        iwm->io_flags |= CLEM_IWM_FLAG_WRITE_DATA;
    } else {
        iwm->io_flags &= ~CLEM_IWM_FLAG_WRITE_DATA;
    }
}

#define CLEM_IWM_READ_REG_STATE_MASK  0xffff0000
#define CLEM_IWM_READ_REG_STATE_LATCH 0x0000ffff
#define CLEM_IWM_READ_REG_STATE_START 0x00000000
#define CLEM_IWM_READ_REG_STATE_QA0   0x00010000
#define CLEM_IWM_READ_REG_STATE_QA1   0x00020000
#define CLEM_IWM_READ_REG_STATE_QA1_1 0x00030000

static bool _clem_iwm_read_step(struct ClemensDeviceIWM *iwm) {
    //  This procedure covers a whole bit cell cycle
    //
    //  Goal is to fill up the latch with bits from a drive and ensure proper disk nibbles are
    //  coming in.  Once a valid disk byte is in, then hold it, clear latch and shift in
    //  additional bits but do not reflect latch onto data_r until 2 bits have been shifted into
    //  the latch, then resume original latch -> data reflection.
    //
    //  read_flags = 0 when switching to read mode (write Q6 = 0)
    //  start:  wait for a read pulse from iwm->io_flags, then immediately go to state_qa0
    //  state_qa0: shift in 0 or 1 based on read pulse; data_r = latch, and continue
    //           until latch bit 7 is 1, then switch to state_qa1
    //  state_qa1: wait until read pulse, then shift onto latch, goto state_qa1_1
    //  state_qa1_1: shift 0 or 1 based on read pulse, goto state_qa0
    //      note - if a read_switch() occurs while in state_qa0, copy latch to data
    unsigned read_latch_hold = iwm->read_state & CLEM_IWM_READ_REG_STATE_LATCH;
    if (iwm->state == CLEM_IWM_STATE_READ_STATUS) {
        //  If in read status (Write Protect Sense) mode, the original LSS shifts the latch
        //  right every Q3 cycle.  Since this step runs in 8 Q3 cycles (5.25" mode), we
        //  effectively shift the latch out and (SR runs 8 times)
        iwm->latch = 0x00;
        if (iwm->io_flags & CLEM_IWM_FLAG_WRPROTECT_SENSE) {
            iwm->latch |= 0x80;
        }
        iwm->data_r = iwm->latch;
        read_latch_hold = 0; // no read latching when reading status
    }
    switch (iwm->read_state & CLEM_IWM_READ_REG_STATE_MASK) {
    case CLEM_IWM_READ_REG_STATE_START:
        if (iwm->io_flags & CLEM_IWM_FLAG_READ_DATA) {
            iwm->latch <<= 1;
            iwm->latch |= 0x1;
            iwm->data_r = iwm->latch;
            iwm->read_state =
                (iwm->read_state & ~CLEM_IWM_READ_REG_STATE_MASK) | CLEM_IWM_READ_REG_STATE_QA0;
        }
        break;
    case CLEM_IWM_READ_REG_STATE_QA0:
        iwm->latch <<= 1;
        if (iwm->io_flags & CLEM_IWM_FLAG_READ_DATA) {
            iwm->latch |= 0x1;
        }
        if (!read_latch_hold) {
            //  no read latch hold enabled, update the data register
            iwm->data_r = iwm->latch;
        }
        if (iwm->latch & 0x80) {
            if (iwm->latch_mode) {
                //  latch the byte until the next data access
                iwm->data_r = iwm->latch;
                read_latch_hold = 1;
            }
            iwm->read_state =
                (iwm->read_state & ~CLEM_IWM_READ_REG_STATE_MASK) | CLEM_IWM_READ_REG_STATE_QA1;
        }
        break;
    case CLEM_IWM_READ_REG_STATE_QA1:
        if (iwm->io_flags & CLEM_IWM_FLAG_READ_DATA) {
            iwm->latch = 0x01;
            iwm->read_state =
                (iwm->read_state & ~CLEM_IWM_READ_REG_STATE_MASK) | CLEM_IWM_READ_REG_STATE_QA1_1;
        }
        break;
    case CLEM_IWM_READ_REG_STATE_QA1_1:
        iwm->latch <<= 1;
        if (iwm->io_flags & CLEM_IWM_FLAG_READ_DATA) {
            iwm->latch |= 0x1;
        }
        iwm->read_state =
            (iwm->read_state & ~CLEM_IWM_READ_REG_STATE_MASK) | CLEM_IWM_READ_REG_STATE_QA0;
        break;
    }
    iwm->read_state &= CLEM_IWM_READ_REG_STATE_MASK;
    iwm->read_state |= (read_latch_hold & 0xffff);
    return (iwm->data_r & 0x80) != 0;
}

static bool _clem_iwm_step_current_clocks_ts(struct ClemensDeviceIWM *iwm,
                                             clem_clocks_duration_t dt) {
    // TODO: stretch here!

    iwm->clocks_used_this_step += dt;
    iwm->cur_clocks_ts += dt;
    if (iwm->cur_clocks_ts >= iwm->clocks_at_next_scanline) {
        iwm->scanline_phase_ctr++;
        if (iwm->scanline_phase_ctr & 1) {
            iwm->clocks_at_next_scanline += CLEM_CLOCKS_7MHZ_CYCLE;
            if (!iwm->async_mode) {
                //  use stretch clock cycles only in synchronous mode per spec
                iwm->clocks_this_step += CLEM_CLOCKS_7MHZ_CYCLE;
            }
        } else {
            iwm->clocks_at_next_scanline +=
                CLEM_VGC_HORIZ_SCAN_PHI0_CYCLES * CLEM_CLOCKS_PHI0_CYCLE;
        }
    }
    if (iwm->clocks_used_this_step < iwm->clocks_this_step)
        return false;
    assert(iwm->clocks_used_this_step == iwm->clocks_this_step);
    iwm->clocks_used_this_step = 0;
    iwm->clocks_this_step = _clem_iwm_select_clocks_step(iwm);
    return true;
}

static void _clem_iwm_step(struct ClemensDeviceIWM *iwm, struct ClemensDriveBay *drives,
                           clem_clocks_time_t end_clocks_ts) {
    struct ClemensDrive *drive = _clem_iwm_select_drive(iwm, drives);
    unsigned delta_ns = clem_calc_ns_step_from_clocks(end_clocks_ts - iwm->cur_clocks_ts);
    bool is_drive_35_sel = (iwm->io_flags & CLEM_IWM_FLAG_DRIVE_35) != 0;

    assert(iwm->cur_clocks_ts <= end_clocks_ts);

    while (iwm->cur_clocks_ts < end_clocks_ts) {
        //  execute write and read steps if we've settled a full bit cell
        clem_clocks_duration_t bit_cell_clocks_dt =
            (iwm->clocks_this_step - iwm->clocks_used_this_step);
        clem_clocks_time_t next_clocks_ts = iwm->cur_clocks_ts + bit_cell_clocks_dt;

        if (next_clocks_ts > end_clocks_ts) {
            bit_cell_clocks_dt = (end_clocks_ts - iwm->cur_clocks_ts);
        }

        if (!_clem_iwm_step_current_clocks_ts(iwm, bit_cell_clocks_dt)) {
            assert(iwm->cur_clocks_ts <= end_clocks_ts);
            continue;
        }

        assert(iwm->cur_clocks_ts <= end_clocks_ts);

        //  process the IWM using the accumulated clocks
        //  BUT, only if the drive is ON - by here we've determined everything needed to calculate
        //  the next time step
        if (!(iwm->io_flags & CLEM_IWM_FLAG_DRIVE_ON)) {
            continue;
        }
        bit_cell_clocks_dt = iwm->clocks_this_step;

        //  obtain write signal from IWM -> io_flags, motor is implied as on
        if (iwm->state & CLEM_IWM_STATE_WRITE_MASK) {
            // force 5.25" drives to use synchronous mode (IWM doesn't support this mode for Disk II
            // devices)

            if (iwm->async_mode && (is_drive_35_sel || iwm->smartport_active)) {
                _clem_iwm_async_write_step(iwm, iwm->cur_clocks_ts);
            } else {
                CLEM_IWM_DEBUG_EVENT(iwm, drives, "DATA_W", iwm->cur_clocks_ts, 1);
                _clem_iwm_write_step(iwm);
            }
        }
        if (!is_drive_35_sel) {
            iwm->smartport_active = clem_smartport_bus(drives->smartport, 1, &iwm->io_flags,
                                                       &iwm->out_phase, bit_cell_clocks_dt);
            if (iwm->smartport_active) {
                /*
                if (iwm->state == CLEM_IWM_STATE_WRITE_DATA &&
                    (iwm->io_flags & CLEM_IWM_FLAG_DRIVE_ON)) {
                    printf("SmartPort: DATA SENT = %02X\n", iwm->data);
                }
                */
                drive = NULL;
            }
        } else {
            iwm->smartport_active = false;
        }
        if (drive) {
            clem_disk_control(drive, &iwm->io_flags, iwm->out_phase, bit_cell_clocks_dt);
            clem_disk_write_head(drive, &iwm->io_flags);
            clem_disk_step(drive, &iwm->io_flags);
        }
        if (!(iwm->state & CLEM_IWM_STATE_WRITE_MASK)) {
            _clem_iwm_read_step(iwm);
        }
        CLEM_IWM_DEBUG_EVENT(iwm, drives, "STEP_D", iwm->cur_clocks_ts, 2);
        if (drive) {
            //  update the head's position
            clem_disk_update_head(drive, &iwm->io_flags);
        }
    }
    assert(iwm->cur_clocks_ts == end_clocks_ts);
    if (iwm->drive_hold_ns > 0) {
        iwm->drive_hold_ns = clem_util_timer_decrement(iwm->drive_hold_ns, delta_ns);
        if (iwm->drive_hold_ns == 0 || iwm->timer_1sec_disabled) {
            CLEM_LOG("IWM: turning drive off in sync");
            _clem_drive_off(iwm, drives);
        }
    }
}

void clem_iwm_glu_sync(struct ClemensDeviceIWM *iwm, struct ClemensDriveBay *drives,
                       struct ClemensTimeSpec *tspec) {
    // IWM Spec: /ENBL1 or /ENBL2 are active = (DRIVE_ON && (DRIVE_1 || DRIVE_2))
    //           the _clem_iwm_step() function will check for an available drive
    //           and if both DRIVE_1 and DRIVE_2 are disabled - and smartport -
    //           then return immediately.
    _clem_iwm_step(iwm, drives, tspec->clocks_spent);
}

/*
    Reading IWM addresses only returns data based on the state of Q6, Q7, and
    only if reading from even io addresses.  The few exceptions are addresses
    outside of the C0E0-EF range.

    Disk II treats Q6,Q7 as simple Read or Write/Write Protect state switches.
    The IIgs controller in addition also provides accesses the special IWM
    registers mentioned.
*/
void _clem_iwm_io_switch(struct ClemensDeviceIWM *iwm, struct ClemensDriveBay *drives,
                         struct ClemensTimeSpec *tspec, uint8_t ioreg, uint8_t op) {
    struct ClemensDrive *drive;
    unsigned current_state = iwm->state; // Q6,Q7 alterations below can set a new state

    switch (ioreg) {
    case CLEM_MMIO_REG_IWM_DRIVE_DISABLE:
        if (iwm->io_flags & CLEM_IWM_FLAG_DRIVE_ON) {
            if (iwm->timer_1sec_disabled) {
                _clem_drive_off(iwm, drives);
            } else if (iwm->drive_hold_ns == 0) {
                iwm->drive_hold_ns = CLEM_1SEC_NS;
            }
            iwm->data_access_time_ns = 0;
        }
        break;
    case CLEM_MMIO_REG_IWM_DRIVE_ENABLE:
        if (!(iwm->io_flags & CLEM_IWM_FLAG_DRIVE_ON)) {
            CLEM_DEBUG("IWM: turning drive on");
            _clem_iwm_drive_switch(iwm, drives, iwm->io_flags | CLEM_IWM_FLAG_DRIVE_ON);
        } else if (iwm->drive_hold_ns > 0) {
            iwm->drive_hold_ns = 0;
        }
        break;
    case CLEM_MMIO_REG_IWM_DRIVE_0:
        /*if (!(iwm->io_flags & CLEM_IWM_FLAG_DRIVE_1)) {
        }
        */
        _clem_iwm_drive_switch(iwm, drives, iwm->io_flags & ~CLEM_IWM_FLAG_DRIVE_2);
        if (!(iwm->io_flags & CLEM_IWM_FLAG_DRIVE_1)) {
            _clem_iwm_drive_switch(iwm, drives, iwm->io_flags | CLEM_IWM_FLAG_DRIVE_1);
        }
        break;
    case CLEM_MMIO_REG_IWM_DRIVE_1:
        /*
        if (!(iwm->io_flags & CLEM_IWM_FLAG_DRIVE_2)) {
        }
        */
        _clem_iwm_drive_switch(iwm, drives, iwm->io_flags & ~CLEM_IWM_FLAG_DRIVE_1);
        if (!(iwm->io_flags & CLEM_IWM_FLAG_DRIVE_2)) {
            _clem_iwm_drive_switch(iwm, drives, iwm->io_flags | CLEM_IWM_FLAG_DRIVE_2);
        }
        break;
    case CLEM_MMIO_REG_IWM_Q6_LO:
        iwm->q6_switch = false;
        break;
    case CLEM_MMIO_REG_IWM_Q6_HI:
        iwm->q6_switch = true;
        break;
    case CLEM_MMIO_REG_IWM_Q7_LO:
        iwm->q7_switch = false;
        break;
    case CLEM_MMIO_REG_IWM_Q7_HI:
        iwm->q7_switch = true;
        break;
    default:
        if (ioreg >= CLEM_MMIO_REG_IWM_PHASE0_LO && ioreg <= CLEM_MMIO_REG_IWM_PHASE3_HI) {
            if (ioreg & 1) {
                iwm->out_phase |= (1 << ((ioreg - CLEM_MMIO_REG_IWM_PHASE0_HI) >> 1));
            } else {
                iwm->out_phase &= ~(1 << ((ioreg - CLEM_MMIO_REG_IWM_PHASE0_LO) >> 1));
            }
        }
        break;
    }

    iwm->state = (iwm->q7_switch ? CLEM_IWM_STATE_WRITE_MASK : 0x00) | (iwm->q6_switch & 0x1);
    if (current_state != iwm->state) {
        if (iwm->state == CLEM_IWM_STATE_READ_DATA) {
            //  restart read state machine so we can start reading new data from the latch
            iwm->latch = 0;
            iwm->read_state = 0;
        }
        if (!(current_state & CLEM_IWM_STATE_WRITE_MASK) &&
            (iwm->state & CLEM_IWM_STATE_WRITE_MASK)) {
            if (iwm->io_flags & CLEM_IWM_FLAG_DRIVE_ON) {
                drive = _clem_iwm_select_drive(iwm, drives);
                if (drive)
                    drive->write_pulse = false;
                iwm->io_flags |= CLEM_IWM_FLAG_WRITE_REQUEST;
            }
            iwm->write_state = 0;
            iwm->latch = 0;
        }
        if ((current_state & CLEM_IWM_STATE_WRITE_MASK) &&
            !(iwm->state & CLEM_IWM_STATE_WRITE_MASK)) {
            iwm->io_flags &= ~CLEM_IWM_FLAG_WRITE_REQUEST;
            iwm->read_state = 0;
            iwm->data_r = iwm->latch;
            iwm->latch = 0;
        }
        // CLEM_LOG("IWM: state %02X => %02X", current_state, iwm->state);
    }
}

static void _clem_iwm_write_mode(struct ClemensDeviceIWM *iwm, uint8_t value) {
    if (value & 0x10) {
        //    iwm->clock_8mhz = true;
        CLEM_WARN("IWM: 8mhz mode requested... and ignored");
    }
    // else {
    //     iwm->clock_8mhz = false;
    // }
    if (value & 0x08) {
        iwm->fast_mode = true;
        CLEM_DEBUG("IWM: fast mode");
    } else {
        iwm->fast_mode = false;
        CLEM_DEBUG("IWM: slow mode");
    }
    if (value & 0x04) {
        iwm->timer_1sec_disabled = true;
    } else {
        iwm->timer_1sec_disabled = false;
    }
    if (value & 0x02) {
        iwm->async_mode = true;
        /* TODO: set up counters for handshake register */
    } else {
        iwm->async_mode = false;
    }
    /* TODO: hold latch for a set time using the ns_latch_hold when reading and
             latch MSB == 1*/
    if (value & 0x01) {
        iwm->latch_mode = true;
    } else {
        iwm->latch_mode = false;
    }
    // CLEM_LOG("IWM: write mode %02X", value);
}

void clem_iwm_write_switch(struct ClemensDeviceIWM *iwm, struct ClemensDriveBay *drives,
                           struct ClemensTimeSpec *tspec, uint8_t ioreg, uint8_t value) {
    unsigned old_io_flags = iwm->io_flags;
    switch (ioreg) {
    case CLEM_MMIO_REG_DISK_INTERFACE:
        if (value & 0x80) {
            iwm->io_flags |= CLEM_IWM_FLAG_HEAD_SEL;
        } else {
            iwm->io_flags &= ~CLEM_IWM_FLAG_HEAD_SEL;
        }
        if (value & 0x40) {
            if (!(old_io_flags & CLEM_IWM_FLAG_DRIVE_35)) {
                CLEM_DEBUG("IWM: setting 3.5 drive mode");
                _clem_iwm_drive_switch(iwm, drives, iwm->io_flags | CLEM_IWM_FLAG_DRIVE_35);
            }
        } else {
            if (old_io_flags & CLEM_IWM_FLAG_DRIVE_35) {
                CLEM_DEBUG("IWM: setting 5.25 drive mode");
                _clem_iwm_drive_switch(iwm, drives, iwm->io_flags & ~CLEM_IWM_FLAG_DRIVE_35);
            }
        }
        if (value & 0x3f) {
            CLEM_WARN("IWM: setting unexpected diskreg flags %02X", value);
        }
        break;
    default:
        clem_iwm_glu_sync(iwm, drives, tspec);
        _clem_iwm_io_switch(iwm, drives, tspec, ioreg, CLEM_IO_WRITE);
        if (ioreg & 1) {
            if (iwm->state == CLEM_IWM_STATE_WRITE_MODE) {
                if (iwm->io_flags & CLEM_IWM_FLAG_DRIVE_ON) {
                    iwm->data_w = value;
                    iwm->write_state |= CLEM_IWM_WRITE_REG_DATA;
                    iwm->data_access_time_ns = CLEM_IWM_DATA_ACCESS_NS_EXPIRATION;
                    iwm->io_flags |= CLEM_IWM_FLAG_WRITE_REQUEST;
                    CLEM_IWM_DEBUG_EVENT(iwm, drives, "MPU_W", tspec->clocks_spent, 2);
                } else {
                    _clem_iwm_write_mode(iwm, value);
                }
            }
        }
        break;
    }
}

static uint8_t _clem_iwm_write_handshake(struct ClemensDeviceIWM *iwm, clem_clocks_time_t ts,
                                         bool is_noop) {
    uint8_t result = 0x80;
    result |= 0x1f; /* SWIM ref p.11 bits 0-5 are always 1 */
    if (iwm->lss_write_reg & CLEM_IWM_WRITE_REG_ASYNC_ACTIVE) {
        if (iwm->lss_write_reg & CLEM_IWM_WRITE_REG_DATA) {
            result &= ~0x80; /* data register is full - not latched yet */
        }
        if (iwm->lss_write_reg & CLEM_IWM_WRITE_REG_ASYNC_UNDERRUN) {
            if (!is_noop) {
                _clem_iwm_lss_write_log(iwm, ts, "async-under");
            }
        } else {
            result |= 0x40;
        }
    }
    /* TODO: read handshake read ready?  latch mode and holding the latch
             for a fixed period of time?
    */
    return result;
}

uint8_t clem_iwm_read_switch(struct ClemensDeviceIWM *iwm, struct ClemensDriveBay *drives,
                             struct ClemensTimeSpec *tspec, uint8_t ioreg, uint8_t flags) {
    uint8_t result = 0x00;
    bool is_noop = (flags & CLEM_OP_IO_NO_OP) != 0;

    switch (ioreg) {
    case CLEM_MMIO_REG_DISK_INTERFACE:
        if (iwm->io_flags & CLEM_IWM_FLAG_HEAD_SEL) {
            result |= 0x80;
        } else {
            result &= ~0x80;
        }
        if (iwm->io_flags & CLEM_IWM_FLAG_DRIVE_35) {
            result |= 0x40;
        } else {
            result &= 0x40;
        }
        break;
    default:
        if (!is_noop) {
            clem_iwm_glu_sync(iwm, drives, tspec);
            _clem_iwm_io_switch(iwm, drives, tspec, ioreg, CLEM_IO_READ);
        }
        if (!(ioreg & 1)) {
            switch (iwm->state) {
            case CLEM_IWM_STATE_READ_STATUS:
                result = _clem_iwm_read_status(iwm);
                break;
            case CLEM_IWM_STATE_WRITE_HANDSHAKE:
                result = _clem_iwm_write_handshake(iwm, tspec->clocks_spent, is_noop);
                break;
            default:
                iwm->data_access_time_ns = CLEM_IWM_DATA_ACCESS_NS_EXPIRATION;
                if (!(iwm->io_flags & CLEM_IWM_FLAG_DRIVE_ON)) {
                    /* all ones, IWM spec p.7 */
                    result = 0xff;
                } else {
                    if (!is_noop) {
                        if ((iwm->read_state & CLEM_IWM_READ_REG_STATE_MASK) ==
                            CLEM_IWM_READ_REG_STATE_QA0) {
                            //  exceptional case, when reading before the next bit cell is processed
                            //  in sync()
                            if (!(iwm->read_state & CLEM_IWM_READ_REG_STATE_LATCH)) {
                                //  but don't reset the data register if we're latch hold is
                                //  enabled
                                iwm->data_r = iwm->latch;
                            }
                        }
                        //  a data read will release the latch hold
                        iwm->read_state &= ~CLEM_IWM_READ_REG_STATE_LATCH;
                        if (iwm->data_r & 0x80) {
                            CLEM_IWM_DEBUG_EVENT(iwm, drives, "MPU_R", tspec->clocks_spent, 1);
                        } else {
                            CLEM_IWM_DEBUG_EVENT(iwm, drives, "POLL_R", tspec->clocks_spent, 2);
                        }
                    }

                    result = iwm->data_r;
                }
                break;
            }
        }
        break;
    }

    return result;
}

void clem_iwm_speed_disk_gate(ClemensMMIO *mmio, struct ClemensTimeSpec *tspec) {
    struct ClemensDeviceIWM *iwm = &mmio->dev_iwm;
    uint8_t old_disk_motor_on = iwm->disk_motor_on;
    uint8_t speed_slot_mask = mmio->speed_c036 & 0xf;
    bool drive_on = (iwm->io_flags & CLEM_IWM_FLAG_DRIVE_ON) != 0;
    bool drive_35 = (iwm->io_flags & CLEM_IWM_FLAG_DRIVE_35) != 0;

    iwm->disk_motor_on = 0x00;
    if (speed_slot_mask & 0x2) {
        if (drive_35 && drive_on) {
            iwm->disk_motor_on |= 0x02;
        }
    }
    if (speed_slot_mask & 0x4) {
        if (!drive_35 && drive_on) {
            iwm->disk_motor_on |= 0x04;
        }
    }
    if (iwm->disk_motor_on) {
        if (!old_disk_motor_on) {
            CLEM_LOG("SPEED SLOW Disk: %02X", iwm->disk_motor_on);
        }
        tspec->clocks_step = CLEM_CLOCKS_PHI0_CYCLE;
        return;
    }
    if (mmio->speed_c036 & CLEM_MMIO_SPEED_FAST_ENABLED) {
        tspec->clocks_step = tspec->clocks_step_fast;

        if (old_disk_motor_on) {
            CLEM_LOG("SPEED FAST Disk: %02X", iwm->disk_motor_on);
        }
    } else {
        tspec->clocks_step = CLEM_CLOCKS_PHI0_CYCLE;
        if (old_disk_motor_on) {
            CLEM_LOG("SPEED SLOW Disk: %02X", iwm->disk_motor_on);
        }
    }
}
