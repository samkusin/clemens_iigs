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

#include "clem_device.h"
#include "clem_drive.h"
#include "clem_debug.h"
#include "clem_mmio_defs.h"
#include "clem_util.h"

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

#define CLEM_IWM_STATE_READ_DATA            0x00
#define CLEM_IWM_STATE_READ_STATUS          0x01
#define CLEM_IWM_STATE_READ_HANDSHAKE       0x02
#define CLEM_IWM_STATE_WRITE_MODE           0x03
#define CLEM_IWM_STATE_WRITE_DATA           0x13

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
    0xDD, 0x4D, 0xE0, 0xE0, 0x0A, 0x0A, 0x0A, 0x0A, 0x88, 0x88, 0x08, 0x08, 0x88, 0x88, 0x08, 0x08
};


static inline unsigned _clem_iwm_get_access_state(struct ClemensDeviceIWM* iwm) {
    unsigned state = (iwm->q7_switch ? 0x02 : 0x00) | iwm->q6_switch;
    if (state == CLEM_IWM_STATE_WRITE_MODE &&
        iwm->io_flags & CLEM_IWM_FLAG_DRIVE_ON
    ) {
        return CLEM_IWM_STATE_WRITE_DATA;
    }
    return state;
}


void clem_iwm_reset(struct ClemensDeviceIWM* iwm) {
    memset(iwm, 0, sizeof(*iwm));
    iwm->lss_update_dt_ns = CLEM_IWM_SYNC_FRAME_NS;
}

void clem_iwm_insert_disk(
   struct ClemensDeviceIWM* iwm,
   enum ClemensDriveType drive_type
) {
    // set disk
    // reset drive state

}

void clem_iwm_eject_disk(
    struct ClemensDeviceIWM* iwm,
    enum ClemensDriveType drive_type
) {
    // clear disk after timeout
    // after timeout, reset drive state
}

static void _clem_iwm_lss(struct ClemensDeviceIWM* iwm) {
    /* Uses the Disk II sequencer.
       Some assumptions taken from Understanding the Apple //e
       Generally speaking, our IO reads for status, handshake and writes for
       mode use the IWM registers versus the latch generated here.
       Still we execute the LSS for all variations of Q6,Q7 to maintain the
       latch value to maximize compatibility with legacy Disk I/O.
    */
    unsigned adr = (unsigned)(iwm->lss_state) << 4 |
                   (iwm->q7_switch ? 0x08 : 00) |
                   (iwm->q6_switch ? 0x04 : 00) |
                   ((iwm->latch & 0x80) ? 0x02 : 00) |
                   ((iwm->io_flags & CLEM_IWM_FLAG_READ_DATA) ? 0x01 : 00);
    unsigned cmd = s_lss_rom[adr];

    if (cmd & 0x08) {
        switch (cmd & 0xf) {
            case 0x08:              /* NOP */
            case 0x0C:
                break;
            case 0x09:              /* SL0 */
                if (iwm->lss_write_counter & 0x80) {
                    iwm->write_out <<= 1;
                    iwm->write_out |= (iwm->latch & 0x80) ? 0x01 : 0x00;
                    ++iwm->lss_write_counter;
                    if (iwm->lss_write_counter >= 0x88) {
                        CLEM_LOG("diskwr(%u): %02X",
                            iwm->write_out,
                            iwm->lss_write_counter - 0x80);
                    }
                }
                iwm->latch <<= 1;
                break;
            case 0x0A:              /* SR, WRPROTECT -> HI */
            case 0x0E:
                iwm->latch >>= 1;
                if (iwm->io_flags & CLEM_IWM_FLAG_WRPROTECT_SENSE) {
                    iwm->latch |= 0x80;
                }
                break;
            case 0x0B:              /* LD from data to latch */
            case 0x0F:
                iwm->latch = iwm->data;
                if (iwm->state & 0x02) {
                    iwm->lss_write_counter = 0x80;
                    iwm->write_out = 0x00;
                } else {
                    CLEM_WARN("clem_iwm: state: %02X load byte %02X in read?",
                        iwm->state, iwm->data);
                }
                break;
            case 0x0D:              /* SL1 append 1 bit */
                iwm->latch <<= 1;
                iwm->latch |= 0x01;
                break;
        }
    } else {
        /* CLR */
        iwm->latch = 0;
    }

    if (iwm->state & 0x02) {
        /* write mode */
        iwm->io_flags |= CLEM_IWM_FLAG_WRITE_DATA;
    } else {
        /* read mode - data = latch except when holding the current read byte
           note, that the LSS ROM does this for us, but when IIgs latch mode is
           enabled, we need to extend the life of the read-value on the data
           'bus'.  once the hold has expired, we can resume updating the
           'bus' with the latch's current value*/
        iwm->io_flags &= ~CLEM_IWM_FLAG_WRITE_DATA;
        /*
        if ((iwm->latch & 0x80) && iwm->latch_mode) {
            if ns_latch_hold == 0, ns_latch_hold = 16us or 32us

        }
        */
        iwm->data = iwm->latch;
    }

    iwm->lss_state = (cmd & 0xf0) >> 4;
}


void clem_iwm_glu_sync(
    struct ClemensDeviceIWM* iwm,
    struct ClemensDriveBay* drives,
    struct ClemensClock* clock
) {
    unsigned delta_ns;
    unsigned spent_ns = 0;

    if (iwm->last_clocks_ts > clock->ts) return;

    delta_ns = _clem_calc_ns_step_from_clocks(
        clock->ts - iwm->last_clocks_ts, clock->ref_step);

    if (iwm->io_flags & CLEM_IWM_FLAG_DRIVE_ON) {
        /* handle the 1 second drive motor timer */
        if (iwm->ns_drive_hold > 0) {
            iwm->ns_drive_hold = clem_util_timer_decrement(
                iwm->ns_drive_hold, delta_ns);
            if (iwm->ns_drive_hold == 0 || iwm->timer_1sec_disabled) {
                CLEM_LOG("clem_iwm: turning drive off in sync");
                iwm->io_flags &= ~CLEM_IWM_FLAG_DRIVE_ON;
            }
        }
    }
    if (iwm->io_flags & CLEM_IWM_FLAG_DRIVE_ON && iwm->lss_update_dt_ns <= delta_ns ) {
        while (spent_ns <= delta_ns - iwm->lss_update_dt_ns) {
            if (iwm->io_flags & CLEM_IWM_FLAG_DRIVE_35) {
                if (iwm->io_flags & CLEM_IWM_FLAG_DRIVE_1) {
                    clem_disk_update_state_35(&drives->slot5[0],
                                              &iwm->io_flags,
                                              iwm->out_phase,
                                              iwm->lss_update_dt_ns);
                }
                if (iwm->io_flags & CLEM_IWM_FLAG_DRIVE_2) {
                    clem_disk_update_state_35(&drives->slot5[1],
                                              &iwm->io_flags,
                                              iwm->out_phase,
                                              iwm->lss_update_dt_ns);
                }
            } else {
                if (iwm->io_flags & CLEM_IWM_FLAG_DRIVE_1) {
                    clem_disk_update_state_525(&drives->slot6[0],
                                               &iwm->io_flags,
                                               iwm->out_phase,
                                               iwm->lss_update_dt_ns);
                }
                if (iwm->io_flags & CLEM_IWM_FLAG_DRIVE_2) {
                    clem_disk_update_state_525(&drives->slot6[1],
                                               &iwm->io_flags,
                                               iwm->out_phase,
                                               iwm->lss_update_dt_ns);
                }
            }
            _clem_iwm_lss(iwm);
            spent_ns += iwm->lss_update_dt_ns;
        }
    }
    iwm->last_clocks_ts = clock->ts - _clem_calc_clocks_step_from_ns(
        delta_ns - spent_ns, clock->ref_step);
}

/*
    Reading IWM addresses only returns data based on the state of Q6, Q7, and
    only if reading from even io addresses.  The few exceptions are addresses
    outside of the C0E0-EF range.

    Disk II treats Q6,Q7 as simple Read or Write/Write Protect state switches.
    The IIgs controller in addition also provides accesses the special IWM
    registers mentioned.
*/

void _clem_iwm_io_switch(
   struct ClemensDeviceIWM* iwm,
   struct ClemensDriveBay* drives,
   struct ClemensClock* clock,
   uint8_t ioreg,
   uint8_t op
) {
    unsigned current_state = iwm->state;

    switch (ioreg) {
        case CLEM_MMIO_REG_IWM_DRIVE_DISABLE:
            if (iwm->io_flags & CLEM_IWM_FLAG_DRIVE_ON) {
                if (iwm->timer_1sec_disabled) {
                    CLEM_LOG("clem_iwm: turning drive off now");
                    iwm->io_flags &= ~CLEM_IWM_FLAG_DRIVE_ON;
                } else if (iwm->ns_drive_hold == 0) {
                    CLEM_LOG("clem_iwm: turning drive off in 1 second");
                    iwm->ns_drive_hold = CLEM_1SEC_NS;
                }
            }
            break;
        case CLEM_MMIO_REG_IWM_DRIVE_ENABLE:
            if (!(iwm->io_flags & CLEM_IWM_FLAG_DRIVE_ON)) {
                CLEM_LOG("clem_iwm: turning drive on");
                iwm->io_flags |= CLEM_IWM_FLAG_DRIVE_ON;
                iwm->lss_state = 0;
                iwm->lss_write_counter = 0;
                iwm->ns_drive_hold = 0;
            }
            break;
        case CLEM_MMIO_REG_IWM_DRIVE_0:
            if (!(iwm->io_flags & CLEM_IWM_FLAG_DRIVE_1)) {
                CLEM_LOG("clem_iwm: setting drive 1");
            }
            iwm->io_flags |= CLEM_IWM_FLAG_DRIVE_1;
            iwm->io_flags &= ~CLEM_IWM_FLAG_DRIVE_2;
            break;
        case CLEM_MMIO_REG_IWM_DRIVE_1:
            if (!(iwm->io_flags & CLEM_IWM_FLAG_DRIVE_2)) {
                CLEM_LOG("clem_iwm: setting drive 2");
            }
            iwm->io_flags |= CLEM_IWM_FLAG_DRIVE_2;
            iwm->io_flags &= ~CLEM_IWM_FLAG_DRIVE_1;
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
            if (ioreg >= CLEM_MMIO_REG_IWM_PHASE0_LO &&
                ioreg <= CLEM_MMIO_REG_IWM_PHASE3_HI
            ) {
                if (ioreg & 1) {
                    iwm->out_phase |= (
                        1 << ((ioreg - CLEM_MMIO_REG_IWM_PHASE0_HI) >> 1));
                } else {
                    iwm->out_phase &= ~(
                        1 << ((ioreg - CLEM_MMIO_REG_IWM_PHASE0_LO) >> 1));
                }
            }
            break;
    }

    iwm->state = _clem_iwm_get_access_state(iwm);
    if (current_state != iwm->state) {
        if (iwm->io_flags & CLEM_IWM_FLAG_DRIVE_ON) {
            if (!(current_state & 0x2) && (iwm->state & 0x02)) {
                iwm->lss_state = 0;     /* initial write state */
                iwm->lss_write_counter = 0x00;
            }
            if ((current_state & 0x02) && !(iwm->state & 0x02)) {
                iwm->lss_state = 2;     /* initial read state */
                iwm->lss_write_counter = 0x00;
            }
            if (iwm->state == CLEM_IWM_STATE_WRITE_DATA) {
                iwm->last_write_clocks_ts = clock->ts;
            }
        }
        CLEM_LOG("clem_iwm: state %02X => %02X", current_state, iwm->state);
    }
}

static void _clem_iwm_write_mode(struct ClemensDeviceIWM* iwm, uint8_t value) {
    if (value & 0x10) {
        iwm->clock_8mhz = true;
        CLEM_WARN("clem_iwm: 8mhz mode requested... and ignored");
    } else {
        iwm->clock_8mhz = false;
    }
    if (value & 0x08) {
        iwm->lss_update_dt_ns = CLEM_IWM_SYNC_FRAME_NS / 2;
    } else {
        iwm->lss_update_dt_ns = CLEM_IWM_SYNC_FRAME_NS;
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
    CLEM_LOG("clem_iwm: write mode %02X", value);
}

void clem_iwm_write_switch(
    struct ClemensDeviceIWM* iwm,
    struct ClemensDriveBay* drives,
    struct ClemensClock* clock,
    uint8_t ioreg,
    uint8_t value
) {
    unsigned old_io_flags = iwm->io_flags;
    switch (ioreg) {
        case CLEM_MMIO_REG_DISK_INTERFACE:
            if (value & 0x80) {
                iwm->io_flags |= CLEM_IWM_FLAG_HEAD_SEL;
            } else {
                iwm->io_flags &= ~CLEM_IWM_FLAG_HEAD_SEL;
            }
            if (value & 0x40) {
                iwm->io_flags |= CLEM_IWM_FLAG_DRIVE_35;
                if (!(old_io_flags & CLEM_IWM_FLAG_DRIVE_35)) {
                    CLEM_LOG("clem_iwm: setting 3.5 drive mode");
                }
            } else {
                iwm->io_flags &= ~CLEM_IWM_FLAG_DRIVE_35;
                if (old_io_flags & CLEM_IWM_FLAG_DRIVE_35) {
                    CLEM_LOG("clem_iwm: setting 5.25 drive mode");
                }
            }
            if (value & 0x3f) {
                CLEM_WARN("clem_iwm: setting unexpected diskreg flags %02X", value);
            }
            break;
        default:
            _clem_iwm_io_switch(iwm, drives, clock, ioreg, CLEM_IO_WRITE);
            if (ioreg & 1) {
                iwm->data = value;
                switch (iwm->state) {
                    case CLEM_IWM_STATE_WRITE_MODE:
                        _clem_iwm_write_mode(iwm, value);
                        break;
                    case CLEM_IWM_STATE_WRITE_DATA:
                        /* TODO: async handshake timer starts here */
                        break;
                    default:
                        break;
                }
            }
            if (iwm->io_flags & CLEM_IWM_FLAG_DRIVE_ON) {
                clem_iwm_glu_sync(iwm, drives, clock);
            }
            break;
    }
}

static uint8_t _clem_iwm_read_status(struct ClemensDeviceIWM* iwm) {
    uint8_t result = 0;

    if (iwm->io_flags & CLEM_IWM_FLAG_DRIVE_ON &&
        iwm->io_flags & CLEM_IWM_FLAG_DRIVE_ANY
    ) {
        result |= 0x20;
    }

    if (iwm->io_flags & CLEM_IWM_FLAG_WRPROTECT_SENSE) {
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

static uint8_t _clem_iwm_read_handshake(
    struct ClemensDeviceIWM* iwm,
    struct ClemensClock* clock,
    bool is_noop
) {
    uint8_t result = 0x80;  /* start with ready */
    unsigned ns_write = _clem_calc_ns_step_from_clocks(
        clock->ts - iwm->last_write_clocks_ts, clock->ref_step);
    /* TODO: IWM is busy during a valid write? Then clear result.
    */
    if ((iwm->lss_write_counter & 0xf) > 8) {
        result |= 0x04;     /* we should be loading the next write byte */
        if (!is_noop) {
            CLEM_WARN("clem_iwm: write_ovr dt = %.3f us", ns_write * 0.001f);
        }
    } else if ((iwm->lss_write_counter % 8) != 0) {
        result &= ~0x80;
    } else {
        if (!is_noop) {
            CLEM_LOG("clem_iwm: write_rdy dt = %.3f us", ns_write * 0.001f);
        }
    }
    return result;
}

uint8_t clem_iwm_read_switch(
    struct ClemensDeviceIWM* iwm,
    struct ClemensDriveBay* drives,
    struct ClemensClock* clock,
    uint8_t ioreg,
    uint8_t flags
) {
    uint8_t result = 0x00;
    bool is_noop = (flags & CLEM_MMIO_READ_NO_OP) != 0;

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
                _clem_iwm_io_switch(iwm, drives, clock, ioreg, CLEM_IO_READ);
            }
            if (iwm->io_flags & CLEM_IWM_FLAG_DRIVE_ON) {
                clem_iwm_glu_sync(iwm, drives, clock);
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
                        result = iwm->data;
                        break;
                }
            }
            break;
    }

    return result;
}
