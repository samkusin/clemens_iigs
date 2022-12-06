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
#include "clem_util.h"

#include <inttypes.h>
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

#define CLEM_IWM_STATE_READ_DATA      0x00
#define CLEM_IWM_STATE_READ_STATUS    0x01
#define CLEM_IWM_STATE_WRITE_MASK     0x02
#define CLEM_IWM_STATE_READ_HANDSHAKE 0x02
#define CLEM_IWM_STATE_WRITE_MODE     0x03
#define CLEM_IWM_STATE_WRITE_DATA     0x13
#define CLEM_IWM_STATE_UNKNOWN        0xFF

/* cribbed this convenient table from
   https://github.com/whscullin/apple2js/blob/f4b0100c98c2c12988f64ffe44426fcdd5ae901b/js/cards/disk2.ts#L107
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

static inline unsigned _clem_iwm_get_access_state(struct ClemensDeviceIWM *iwm) {
    unsigned state = (iwm->q7_switch ? 0x02 : 0x00) | iwm->q6_switch;
    if (state == CLEM_IWM_STATE_WRITE_MODE && (iwm->io_flags & CLEM_IWM_FLAG_DRIVE_ON)) {
        return CLEM_IWM_STATE_WRITE_DATA;
    }
    return state;
}

void clem_iwm_reset(struct ClemensDeviceIWM *iwm) {
    memset(iwm, 0, sizeof(*iwm));
    iwm->lss_update_dt_ns = CLEM_IWM_SYNC_FRAME_NS;
    iwm->state = CLEM_IWM_STATE_UNKNOWN;
}

void clem_iwm_insert_disk(struct ClemensDeviceIWM *iwm, struct ClemensDrive *drive,
                          struct ClemensNibbleDisk *disk) {
    memcpy(&drive->disk, disk, sizeof(drive->disk));
    drive->has_disk = disk->track_count > 0;
    // set disk
    // reset drive state
}

void clem_iwm_debug_start(struct ClemensDeviceIWM *iwm) { iwm->enable_debug = true; }

void clem_iwm_debug_stop(struct ClemensDeviceIWM *iwm) { iwm->enable_debug = false; }

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

struct ClemensDrive *_clem_iwm_select_drive(struct ClemensDeviceIWM *iwm,
                                            struct ClemensDriveBay *drive_bay) {
    struct ClemensDrive *drives;
    if (iwm->io_flags & CLEM_IWM_FLAG_DRIVE_35) {
        drives = drive_bay->slot5;
    } else {
        drives = drive_bay->slot6;
    }
    if (iwm->io_flags & CLEM_IWM_FLAG_DRIVE_1) {
        return &drives[0];
    } else {
        return &drives[1];
    }
}

static void _clem_iwm_reset_lss(struct ClemensDeviceIWM *iwm, struct ClemensDriveBay *drives,
                                struct ClemensClock *clock) {
    struct ClemensDrive *drive = _clem_iwm_select_drive(iwm, drives);
    iwm->ns_drive_hold = 0;
    iwm->last_clocks_ts = clock->ts;
    clem_disk_start_drive(drive);
}

#define CLEM_IWM_WRITE_REG_STATUS_MASK    0xffff0000
#define CLEM_IWM_WRITE_REG_ASYNC_ACTIVE   0x80000000
#define CLEM_IWM_WRITE_REG_ASYNC_UNDERRUN 0x20000000
#define CLEM_IWM_WRITE_REG_LATCH          0x08000000
#define CLEM_IWM_WRITE_REG_LATCH_QA       0x04000000
#define CLEM_IWM_WRITE_REG_DATA           0x01000000

static void _clem_iwm_lss_write_log(struct ClemensDeviceIWM *iwm, struct ClemensClock *clock,
                                    const char *prefix) {
    unsigned ns_write =
        clem_calc_ns_step_from_clocks(clock->ts - iwm->last_write_clocks_ts, clock->ref_step);
    CLEM_LOG("IWM: [%s] write latch %08X, duration dt = %.3f us, flags=%08X, "
             "counter=%u",
             prefix, iwm->latch, ns_write * 0.001f,
             iwm->lss_write_reg & CLEM_IWM_WRITE_REG_STATUS_MASK,
             iwm->lss_write_reg & ~CLEM_IWM_WRITE_REG_STATUS_MASK);
}

static bool _clem_iwm_lss_write_async(struct ClemensDeviceIWM *iwm, struct ClemensClock *clock,
                                      unsigned disk_delta_ns) {
    /* The write sequencer for async writes attempts to emulate the feature as
       designed in the IWM spec.

       This is meant for 3.5" drives but relies on the emulatied IIgs
       application to make sure it doesn't enable async writes for any device
       other than the 3.5" drive - as stated in the HW reference.

       A full bitcell cycle is 8 clocks - (fast or slow)
       It will take 2 or 4 clocks to load the initial write latch (fast vs slow)
    */
    unsigned clock_counter = iwm->lss_write_reg & ~CLEM_IWM_WRITE_REG_STATUS_MASK;
    bool write_signal = (iwm->lss_write_reg & CLEM_IWM_WRITE_REG_LATCH_QA) != 0;
    /* load the write latch as specified (initial delay + subsequent delays per
       8-bit cell = 64 clocks)

       The data register is 'ready' for new data once its copied to the latch
    */
    if (!(iwm->lss_write_reg & CLEM_IWM_WRITE_REG_ASYNC_ACTIVE)) {
        if (clock_counter == 4) {
            /* 1/2 bit cell delay from IWM spec p2 */
            iwm->lss_write_reg |= CLEM_IWM_WRITE_REG_ASYNC_ACTIVE;
            iwm->lss_write_reg &= CLEM_IWM_WRITE_REG_STATUS_MASK;
        }
    }
    if (iwm->lss_write_reg & CLEM_IWM_WRITE_REG_ASYNC_ACTIVE) {
        clock_counter = iwm->lss_write_reg & ~CLEM_IWM_WRITE_REG_STATUS_MASK;
        if ((clock_counter % 64) == 0) {
            iwm->latch = iwm->data;
            if (!(iwm->lss_write_reg & CLEM_IWM_WRITE_REG_DATA)) {
                /* set until cleared by a mode switch - see SWIM chip ref p 11 */
                iwm->lss_write_reg |= CLEM_IWM_WRITE_REG_ASYNC_UNDERRUN;
            }
            /* IWM ready for a new byte */
            iwm->lss_write_reg &= ~CLEM_IWM_WRITE_REG_DATA;
            iwm->lss_write_reg |= CLEM_IWM_WRITE_REG_LATCH;
            iwm->last_write_clocks_ts = clock->ts;
            //_clem_iwm_lss_write_log(iwm, clock, "async-ld");
        }
        if ((clock_counter % 8) == 0) {
            if (iwm->latch & 0x80) {
                /* writes pulse the signal at precise 8 clock intervals */
                /* null bits do not pulse the signal */
                if (!(iwm->lss_write_reg & CLEM_IWM_WRITE_REG_LATCH_QA)) {
                    write_signal = true;
                    iwm->lss_write_reg |= CLEM_IWM_WRITE_REG_LATCH_QA;
                } else {
                    write_signal = false;
                    iwm->lss_write_reg &= ~CLEM_IWM_WRITE_REG_LATCH_QA;
                }
            }
            iwm->latch <<= 1; /* SL0 always before the next write*/
                              //_clem_iwm_lss_write_log(iwm, clock, "async-sh");
        }
    }

    ++clock_counter;

    iwm->lss_write_reg = (iwm->lss_write_reg & CLEM_IWM_WRITE_REG_STATUS_MASK) | clock_counter;

    return write_signal;
}

static void _clem_iwm_debug_log(struct ClemensDeviceIWM *iwm, struct ClemensClock *clock,
                                unsigned current_mode) {
    double ts = clem_calc_secs_from_clocks(clock);
    char *logtext;
    unsigned index;
    int logcnt;
}

static void _clem_iwm_debug_null(struct ClemensDeviceIWM *iwm, struct ClemensClock *clock) {}

static void _clem_iwm_debug(struct ClemensDeviceIWM *iwm, struct ClemensDrive *drive,
                            struct ClemensClock *clock) {
    if (iwm->io_flags & CLEM_IWM_FLAG_READ_DATA_FAKE) {
        CLEM_LOG("iwm: @[track=%u, base=%u, offset=%u:%u] fake_bit = %u", drive->real_track_index,
                 drive->disk.track_byte_offset[drive->real_track_index], drive->track_byte_index,
                 drive->track_bit_shift, (iwm->io_flags & CLEM_IWM_FLAG_READ_DATA) ? 1 : 0);
    }
}

static bool _clem_iwm_lss(struct ClemensDeviceIWM *iwm, struct ClemensDrive *drive,
                          struct ClemensClock *clock) {
    /* Uses the Disk II sequencer.
       Some assumptions taken from Understanding the Apple //e
       Generally speaking, our IO reads for status, handshake and writes for
       mode use the IWM registers versus the latch generated here.
       Still we execute the LSS for all variations of Q6,Q7 to maintain the
       latch value to maximize compatibility with legacy Disk I/O.
    */
    unsigned adr, cmd;

    adr = (unsigned)(iwm->lss_state) << 4 | (iwm->q7_switch ? 0x08 : 00) |
          (iwm->q6_switch ? 0x04 : 00) | ((iwm->latch & 0x80) ? 0x02 : 00) |
          ((iwm->io_flags & CLEM_IWM_FLAG_READ_DATA) ? 0x00 : 01);
    cmd = s_lss_rom[adr];

    if (cmd & 0x08) {
        switch (cmd & 0xf) {
        case 0x08: /* NOP */
        case 0x0C:
            break;
        case 0x09: /* SL0 */
            iwm->latch <<= 1;
            if (iwm->lss_write_reg & CLEM_IWM_WRITE_REG_LATCH) {
                iwm->lss_write_reg = (iwm->lss_write_reg & ~CLEM_IWM_WRITE_REG_STATUS_MASK) + 1;
                iwm->lss_write_reg |= CLEM_IWM_WRITE_REG_LATCH;
                //_clem_iwm_lss_write_log(iwm, clock, "sh");
            }
            break;
        case 0x0A: /* SR, WRPROTECT -> HI */
        case 0x0E:
            iwm->latch >>= 1;
            if (iwm->io_flags & CLEM_IWM_FLAG_WRPROTECT_SENSE) {
                iwm->latch |= 0x80;
            }
            break;
        case 0x0B: /* LD from data to latch */
        case 0x0F:
            iwm->latch = iwm->data;
            iwm->lss_write_reg &= ~CLEM_IWM_WRITE_REG_DATA;
            if (iwm->state & CLEM_IWM_STATE_WRITE_MASK) {
                iwm->lss_write_reg = CLEM_IWM_WRITE_REG_LATCH | 1;
                iwm->last_write_clocks_ts = clock->ts;
                //_clem_iwm_lss_write_log(iwm, clock, "ld");
            } else {
                CLEM_WARN("IWM: state: %02X load byte %02X in read?", iwm->state, iwm->data);
            }
            break;
        case 0x0D: /* SL1 append 1 bit */
            /* Note, writes won't use this state.. or they shouldn't! */
            if (iwm->lss_write_reg & CLEM_IWM_WRITE_REG_LATCH) {
                CLEM_ASSERT(false);
            }
            iwm->latch <<= 1;
            iwm->latch |= 0x01;
            break;
        }
    } else {
        /* CLR */
        iwm->latch = 0;
    }

    iwm->lss_state = (cmd & 0xf0) >> 4;
    return (iwm->lss_state & 0x8) != 0;
}

static void _clem_drive_off(struct ClemensDeviceIWM *iwm) {
    iwm->io_flags &= ~CLEM_IWM_FLAG_DRIVE_ON;
    CLEM_DEBUG("IWM: turning drive off now");
}

void clem_iwm_glu_sync(struct ClemensDeviceIWM *iwm, struct ClemensDriveBay *drives,
                       struct ClemensClock *clock) {
    unsigned delta_ns, lss_time_left_ns;
    int drive_index = (iwm->io_flags & CLEM_IWM_FLAG_DRIVE_2) ? 1 : 0;
    struct ClemensClock next_clock;

    if (iwm->io_flags & CLEM_IWM_FLAG_DRIVE_ON) {
        delta_ns =
            clem_calc_ns_step_from_clocks((clock->ts - iwm->last_clocks_ts), clock->ref_step);
        lss_time_left_ns = delta_ns;

        /* Move shared _clem_iwm_lss logic up here. */
        /* catch up the LSS from the last sync to the current time */
        next_clock.ts = iwm->last_clocks_ts;
        next_clock.ref_step = clock->ref_step;
        while (lss_time_left_ns >= iwm->lss_update_dt_ns) {
            struct ClemensDrive *drive = NULL;
            bool write_signal = false;
            if (iwm->io_flags & CLEM_IWM_FLAG_DRIVE_35) {
                drive = &drives->slot5[drive_index];
                clem_disk_read_and_position_head_35(drive, &iwm->io_flags, iwm->out_phase,
                                                    iwm->lss_update_dt_ns);
            } else if (!iwm->enable2) {
                drive = &drives->slot6[drive_index];
                clem_disk_read_and_position_head_525(drive, &iwm->io_flags, iwm->out_phase,
                                                     iwm->lss_update_dt_ns);
            }
            if (iwm->state & CLEM_IWM_STATE_WRITE_MASK) {
                /* write mode */
                if (!(iwm->io_flags & CLEM_IWM_FLAG_WRITE_REQUEST)) {
                    iwm->io_flags |= CLEM_IWM_FLAG_WRITE_REQUEST;
                    if (drive)
                        drive->write_pulse = false;
                }
            }

            if ((iwm->state & CLEM_IWM_STATE_WRITE_MASK) && iwm->async_write_mode) {
                write_signal = _clem_iwm_lss_write_async(iwm, clock, iwm->lss_update_dt_ns);
            } else {
                write_signal = _clem_iwm_lss(iwm, drive, &next_clock);
            }
            if (iwm->state & CLEM_IWM_STATE_WRITE_MASK) {
                if (write_signal) {
                    iwm->io_flags |= CLEM_IWM_FLAG_WRITE_DATA;
                } else {
                    iwm->io_flags &= ~CLEM_IWM_FLAG_WRITE_DATA;
                }
            } else {
                /* read mode - data = latch except when holding the current read byte
                   note, that the LSS ROM does this for us, but when IIgs latch mode is
                   enabled, we need to extend the life of the read-value on the data
                   'bus'.  once the hold has expired, we can resume updating the
                   'bus' with the latch's current value*/
                iwm->io_flags &= ~CLEM_IWM_FLAG_WRITE_REQUEST;
                /*
                if ((iwm->latch & 0x80) && iwm->latch_mode) {
                    if ns_latch_hold == 0, ns_latch_hold = 16us or 32us

                }
                */
                iwm->data = iwm->latch;
            }
            if (drive) {

                if (iwm->enable_debug) {
                    _clem_iwm_debug(iwm, drive, clock);
                }
                if (iwm->io_flags & CLEM_IWM_FLAG_DRIVE_35) {
                    clem_disk_update_head(drive, &iwm->io_flags, iwm->lss_update_dt_ns);
                } else {
                    clem_disk_update_head(drive, &iwm->io_flags, iwm->lss_update_dt_ns);
                }
            }

            lss_time_left_ns -= iwm->lss_update_dt_ns;
            next_clock.ts +=
                clem_calc_clocks_step_from_ns(iwm->lss_update_dt_ns, next_clock.ref_step);
        }
        /* handle the 1 second drive motor timer */
        if (iwm->ns_drive_hold > 0) {
            iwm->ns_drive_hold = clem_util_timer_decrement(iwm->ns_drive_hold, delta_ns);
            if (iwm->ns_drive_hold == 0 || iwm->timer_1sec_disabled) {
                CLEM_LOG("IWM: turning drive off in sync");
                _clem_drive_off(iwm);
            }
        }
    }

    iwm->last_clocks_ts = clock->ts;
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
                         struct ClemensClock *clock, uint8_t ioreg, uint8_t op) {
    unsigned current_state = iwm->state;

    switch (ioreg) {
    case CLEM_MMIO_REG_IWM_DRIVE_DISABLE:
        if (iwm->io_flags & CLEM_IWM_FLAG_DRIVE_ON) {
            if (iwm->timer_1sec_disabled) {
                _clem_drive_off(iwm);
            } else if (iwm->ns_drive_hold == 0) {
                iwm->ns_drive_hold = CLEM_1SEC_NS;
            }
        }
        break;
    case CLEM_MMIO_REG_IWM_DRIVE_ENABLE:
        if (!(iwm->io_flags & CLEM_IWM_FLAG_DRIVE_ON)) {
            CLEM_DEBUG("IWM: turning drive on");
            iwm->io_flags |= CLEM_IWM_FLAG_DRIVE_ON;
            _clem_iwm_reset_lss(iwm, drives, clock);
        } else if (iwm->ns_drive_hold > 0) {
            iwm->ns_drive_hold = 0;
        }
        break;
    case CLEM_MMIO_REG_IWM_DRIVE_0:
        /*if (!(iwm->io_flags & CLEM_IWM_FLAG_DRIVE_1)) {
        }
        */
        iwm->io_flags &= ~CLEM_IWM_FLAG_DRIVE_2;
        if (!(iwm->io_flags & CLEM_IWM_FLAG_DRIVE_1)) {
            iwm->io_flags |= CLEM_IWM_FLAG_DRIVE_1;
            _clem_iwm_reset_lss(iwm, drives, clock);
        }
        break;
    case CLEM_MMIO_REG_IWM_DRIVE_1:
        /*
        if (!(iwm->io_flags & CLEM_IWM_FLAG_DRIVE_2)) {
        }
        */

        iwm->io_flags &= ~CLEM_IWM_FLAG_DRIVE_1;
        if (!(iwm->io_flags & CLEM_IWM_FLAG_DRIVE_2)) {
            iwm->io_flags |= CLEM_IWM_FLAG_DRIVE_2;
            ;
            _clem_iwm_reset_lss(iwm, drives, clock);
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
            //  Ch 7 IIgs Firmware Reference on /ENABLE2 when high disables Disk II devices
            //  (does not apply to Apple Disk 3.5)
            if ((iwm->out_phase & 2) && (iwm->out_phase & 8)) {
                /* PH1 and PH3 ON this sets the ENABLE2 line (for other
                   smartport devices) */
                iwm->enable2 = true;
            } else {
                iwm->enable2 = false;
            }
        }
        break;
    }

    iwm->state = _clem_iwm_get_access_state(iwm);
    if (current_state != iwm->state) {
        //        if (iwm->io_flags & CLEM_IWM_FLAG_DRIVE_ON) {
        if (!(current_state & CLEM_IWM_STATE_WRITE_MASK) &&
            (iwm->state & CLEM_IWM_STATE_WRITE_MASK)) {
            iwm->lss_state = 0; /* initial write state */
            iwm->lss_write_reg = 0x00;
            iwm->write_out = 0x00;
        }
        if ((current_state & CLEM_IWM_STATE_WRITE_MASK) &&
            !(iwm->state & CLEM_IWM_STATE_WRITE_MASK)) {
            iwm->lss_state = 2; /* initial read state */
            iwm->lss_write_reg = 0x00;
        }
        //        }
        // CLEM_LOG("IWM: state %02X => %02X", current_state, iwm->state);
    }
}

static void _clem_iwm_write_mode(struct ClemensDeviceIWM *iwm, uint8_t value) {
    if (value & 0x10) {
        iwm->clock_8mhz = true;
        CLEM_WARN("IWM: 8mhz mode requested... and ignored");
    } else {
        iwm->clock_8mhz = false;
    }
    if (value & 0x08) {
        iwm->lss_update_dt_ns = CLEM_IWM_SYNC_FRAME_NS_FAST;
        CLEM_DEBUG("IWM: fast mode");
    } else {
        iwm->lss_update_dt_ns = CLEM_IWM_SYNC_FRAME_NS;
        CLEM_DEBUG("IWM: slow mode");
    }
    if (value & 0x04) {
        iwm->timer_1sec_disabled = true;
    } else {
        iwm->timer_1sec_disabled = false;
    }
    if (value & 0x02) {
        iwm->async_write_mode = true;
        /* TODO: set up counters for handshake register */
    } else {
        iwm->async_write_mode = false;
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
                           struct ClemensClock *clock, uint8_t ioreg, uint8_t value) {
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
                iwm->io_flags |= CLEM_IWM_FLAG_DRIVE_35;
            }
        } else {
            if (old_io_flags & CLEM_IWM_FLAG_DRIVE_35) {
                CLEM_DEBUG("IWM: setting 5.25 drive mode");
                iwm->io_flags &= ~CLEM_IWM_FLAG_DRIVE_35;
            }
        }
        if (value & 0x3f) {
            CLEM_WARN("IWM: setting unexpected diskreg flags %02X", value);
        }
        break;
    default:
        clem_iwm_glu_sync(iwm, drives, clock);
        _clem_iwm_io_switch(iwm, drives, clock, ioreg, CLEM_IO_WRITE);
        if (ioreg & 1) {
            if (!iwm->enable2) {
                iwm->data = value;
                iwm->lss_write_reg |= CLEM_IWM_WRITE_REG_DATA;
            }
            switch (iwm->state) {
            case CLEM_IWM_STATE_WRITE_MODE:
                _clem_iwm_write_mode(iwm, value);
                break;
            case CLEM_IWM_STATE_WRITE_DATA:
                break;
            default:
                break;
            }
        }
        break;
    }
}

static uint8_t _clem_iwm_read_status(struct ClemensDeviceIWM *iwm) {
    uint8_t result = 0;

    if (iwm->io_flags & CLEM_IWM_FLAG_DRIVE_ON && iwm->io_flags & CLEM_IWM_FLAG_DRIVE_ANY) {
        result |= 0x20;
    }

    if (iwm->io_flags & CLEM_IWM_FLAG_WRPROTECT_SENSE) {
        result |= 0x80;
    }
    if (iwm->enable2) {
        /* Forcing ACK on the SmartPort bus to indicate the bus is no longer busy.
           This will obviously need to change when Smartport support is actually
           implemented. */
        result |= 0x80;
    }
    /* mode flags reflected here */
    if (iwm->clock_8mhz) {
        result |= 0x10;
    }
    if (iwm->lss_update_dt_ns != CLEM_IWM_SYNC_FRAME_NS) {
        result |= 0x08;
    }
    if (iwm->timer_1sec_disabled) {
        result |= 0x04;
    }
    if (iwm->async_write_mode) {
        result |= 0x02;
    }
    if (iwm->latch_mode) {
        result |= 0x01;
    }

    return result;
}

static uint8_t _clem_iwm_read_handshake(struct ClemensDeviceIWM *iwm, struct ClemensClock *clock,
                                        bool is_noop) {
    uint8_t result = 0x80;
    bool sync_fast_mode = iwm->lss_update_dt_ns == CLEM_IWM_SYNC_FRAME_NS_FAST;
    unsigned ns_write_overflow =
        sync_fast_mode ? (8 * 2 * 4 * iwm->lss_update_dt_ns) : (8 * 4 * 4 * iwm->lss_update_dt_ns);
    if (iwm->enable2) {
        return result;
    }
    result |= 0x1f; /* SWIM ref p.11 bits 0-5 are always 1 */
    if (iwm->lss_write_reg & CLEM_IWM_WRITE_REG_ASYNC_ACTIVE) {
        if (iwm->lss_write_reg & CLEM_IWM_WRITE_REG_DATA) {
            result &= ~0x80; /* data register is full - not latched yet */
        }
        if (iwm->lss_write_reg & CLEM_IWM_WRITE_REG_ASYNC_UNDERRUN) {
            if (!is_noop) {
                _clem_iwm_lss_write_log(iwm, clock, "async-under");
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
                             struct ClemensClock *clock, uint8_t ioreg, uint8_t flags) {
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
            clem_iwm_glu_sync(iwm, drives, clock);
            _clem_iwm_io_switch(iwm, drives, clock, ioreg, CLEM_IO_READ);
        }
        if (!(ioreg & 1)) {
            switch (iwm->state) {
            case CLEM_IWM_STATE_READ_STATUS:
                result = _clem_iwm_read_status(iwm);
                break;
            case CLEM_IWM_STATE_READ_HANDSHAKE:
                result = _clem_iwm_read_handshake(iwm, clock, is_noop);
                break;
            default:
                if (iwm->enable2) {
                    /* all ones, empty (SWIM Chip Ref p 11 doc) */
                    result = 0xff;
                } else {
                    result = iwm->data;
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
        tspec->clocks_step = tspec->clocks_step_mega2;
        return;
    }
    if (mmio->speed_c036 & CLEM_MMIO_SPEED_FAST_ENABLED) {
        tspec->clocks_step = tspec->clocks_step_fast;

        if (old_disk_motor_on) {
            CLEM_LOG("SPEED FAST Disk: %02X", iwm->disk_motor_on);
        }
    } else {
        tspec->clocks_step = tspec->clocks_step_mega2;
        if (old_disk_motor_on) {
            CLEM_LOG("SPEED SLOW Disk: %02X", iwm->disk_motor_on);
        }
    }
}
