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
#include "clem_debug.h"
#include "clem_woz.h"
#include "clem_mmio_defs.h"

/*  Enable 3.5 drive seris */
#define CLEM_IWM_FLAG_DRIVE_35               0x00000001
/* device flag, 3.5" side 2 (not used for 5.25") */
#define CLEM_IWM_FLAG_HEAD_SEL               0x00000002
/*  Drive system is active - in tandem with drive index selected */
#define CLEM_IWM_FLAG_DRIVE_ON               0x00000004
/*  Drive 1 selected - note IWM only allows one drive at a time, but the
    disk port has two pins for drive, so emulating that aspect */
#define CLEM_IWM_FLAG_DRIVE_1                0x00000008
/*  Drive 2 selected */
#define CLEM_IWM_FLAG_DRIVE_2                0x00000010

/*  Disk II stepper emulation

    Applied rotation based on what phase the stepper motor is on currently, and
    the next phase.  The rotor will advance or decline the current
    quarter-track index accordingly.

    Note that opposing states will cancel each other out.   For example, Phase
    0 and 2 are on opposite sides of the rotor.  If both are on, no motion is
    applied... UNLESS Phase 1 or 3 is also on (but not both as Phase 1 and 3
    also are on opposite sides of the rotor.)

    This effectively means that rotations can occur only between effective
    states of the phase magnets:

    0<->1, 1<->2, 2<->3, 3<->0

    This also means that if adjacent phases are enabled, we can advance one
    rotor step (or quarter-track).   This is a special case handled by the
    following state transitions:

    Phase 0 ON, Phase 1 ON, rotates a quarter step.  But also if we transition
    from Phase 0 ON, to Phase 0 OFF + Phase 1 ON + Phase 2 ON, which would
    step the rotor 3 times (0->1 = half track, 2 steps, 1+2 = quarter track)

    The easiest visual representation of this is a state table representing
    rotation before and after PHASE magnet states.   The alternative is writing
    special case code for quarter vs half-track steps, which is harder to
    follow.
*/


#define CLEM_DISK_MOTOR_OFF             0
#define CLEM_DISK_MOTOR_SWITCH_ON       1
#define CLEM_DISK_MOTOR_ON              2
#define CLEM_DISK_MOTOR_SWITCH_OFF      3

/*  Phase magnet effective cardinal positions represented by values (4-bit)
    An empty direction means no force.  An 'xE' means NS are on but cancelled
    with the 'East' force remaining.  A plain 'x; means only a cancelled force.

    If the rotor position does not face an enabled phase, the rotor position
    cannot be determined.   Proper disk controller code should take this into
    account.  We'll apply a 'random' amount if the rotor position doesn't face
    the applied phase.

    Questionable transitions:
        * dual to single phase where dual phase magnets != any of single phase
            seems low-torque transition to single phase - unsure how this
                works in practice

*/
static int s_disk2_phase_states[16][16] = {
        /* 00   N   E  NE   S  x0  SE  xE   W  NW  0x  Nx  SW  xW  Sx  xx */
/* 00 */ {  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0 },
/* N  */ {  0,  0,  2,  1,  0,  0,  3,  2, -2, -1,  0,  0, -3, -2,  0,  0 },
/*  E */ {  0, -2,  0, -1,  2,  0,  1,  0,  0, -3,  0, -2,  3,  0,  2,  0 },
/* NE */ {  0, -1,  1,  0,  3,  0,  2,  1, -3, -2,  0, -1,  0, -3,  3,  0 },
/* S  */ {  0,  0, -2, -3,  0,  0, -1, -2,  2,  3,  0,  0,  1,  2,  0,  0 },
/* x0 */ {  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0 },
/* SE */ {  0, -3, -1, -2,  1,  0,  0, -1,  3,  0,  0, -3,  2,  3,  1,  0 },
/* xE */ {  0, -2,  0, -1,  2,  0,  1,  0,  0, -3,  0, -2,  3,  0,  2,  0 },
/*  W */ {  0,  2,  0,  3, -2,  0, -3,  0,  0,  1,  0,  2, -1,  0, -2,  0 },
/* NW */ {  0,  1,  3,  2, -3,  0,  0,  3, -1,  0,  0,  1, -2, -1, -3,  0 },
/* 0x */ {  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0 },
/* Nx */ {  0,  0,  2,  1,  0,  0,  3,  2, -2, -1,  0,  0, -3, -2,  0,  0 },
/* SW */ {  0,  3, -3,  0, -1,  0, -2, -3,  1,  2,  0,  3,  0,  1, -1,  0 },
/* xW */ {  0,  2,  0,  3, -2,  0, -3,  0,  0,  1,  0,  2, -1,  0, -2,  0 },
/* Sx */ {  0,  0, -2, -3,  0,  0, -1, -2,  2,  3,  0,  0,  1,  2,  0,  0 },
/* xx */ {  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0 }
};


/*
    Emulation of disk drives and the IWM Controller.

    Summary: input will come from WOZ files (or coverted to WOZ on the fly by
    emulators using their own tooling.) As a result this isn't a straight
    emulation of the Disk II or 3.5" floppy, but of reading data from generated
    WOZ track data.


    Mechanical Summary: 5.25"

    Each floppy drive head is driven by a 4 phase stepper motor.  Drive
    emulation tracks:

    * Spindle motor status On|Off
    * Spindle Motor spin-up, full-speed and spindown times
    * Stepper motor cog_index and phase magnets
    * Head position (i.e. track, half, quarter)
    * Read and Write Positions on the current track

    For 5.25 drives, this is trivial relative to 3.5" drives, which employed
    a variable speed motor to increase storage capability of the outer rings
    (which have more surface area compared with the inside rings.)

    Reference on quarter tracking:
    www.automate.org/industry-insights/tutorial-the-basics-of-stepper-motors-part-i
         "Half-step single-coil mode"
    Mechanical Summary: 3.5"

    The IWM interface abstracts the 3.5 floppy controller which doesn't provide
    direct control of the stepper motor - so the 4 IWM control registers
    interface with the floppy controller chip.
*/

inline static unsigned _clem_disk_timer_decrement(
    unsigned timer_us,
    unsigned dt_us
) {
    return (timer_us - dt_us < timer_us) ? (timer_us - dt_us) : 0;
}

inline static unsigned _clem_disk_timer_increment(
    unsigned timer_us,
    unsigned timer_max_us,
    unsigned dt_us
) {
    if (timer_us + dt_us > timer_us) {
        timer_us += dt_us;
    }
    if (timer_us + dt_us < timer_us) {
        timer_us = UINT32_MAX;
    } else {
        timer_us += dt_us;
    }
    if (timer_us > timer_max_us) {
        timer_us = timer_max_us;
    }
    return timer_us;
}


static void _clem_disk_reset_drive(struct ClemensDrive* drive) {
    drive->motor_on = false;
    drive->q03_switch = 0;
    drive->motor_switch_us = 0;
    drive->track_position = 0;
}

static void _clem_disk_update_state_35(
    struct ClemensDrive* drive,
    uint64_t delta_ns,
    unsigned io_flags,
    unsigned in_phase
) {

}

/**
 * @brief Emulates a 5.25" Disk II compliant drive
 *
 * Emulation covers:
 * - drive head placement (for WOZ compliant images) based on stepper phases
 * - ensure head accesses data at specific index within a track based on timing
 * - reading of nibble data from image
 * - writing of nibble data to image
 *
 * @param drive
 * @param delta_ns
 * @param io_flags
 * @param in_phase
 */
static void _clem_disk_update_state_525(
    struct ClemensDrive* drive,
    uint64_t delta_ns,
    unsigned io_flags,
    unsigned in_phase
) {
    unsigned dt_us = (unsigned)(delta_ns / 1000);

    switch (drive->motor_state) {
        case CLEM_DISK_MOTOR_OFF:
            if (io_flags & CLEM_IWM_FLAG_DRIVE_ON) {
                drive->motor_state = CLEM_DISK_MOTOR_SWITCH_ON;
            }
            break;
        case CLEM_DISK_MOTOR_SWITCH_ON:
        case CLEM_DISK_MOTOR_ON:
        case CLEM_DISK_MOTOR_SWITCH_OFF:
            if (drive->motor_state <= CLEM_DISK_MOTOR_ON) {
                drive->motor_switch_us = _clem_disk_timer_increment(
                    drive->motor_switch_us, 1000000, dt_us);
                if (io_flags & CLEM_IWM_FLAG_DRIVE_ON) {
                    drive->motor_state = CLEM_DISK_MOTOR_ON;
                } else {
                    drive->motor_state = CLEM_DISK_MOTOR_SWITCH_OFF;
                }
            } else {
                drive->motor_switch_us = _clem_disk_timer_decrement(
                    drive->motor_switch_us, dt_us);
                if (drive->motor_switch_us == 0) {
                    drive->motor_switch_us = CLEM_DISK_MOTOR_OFF;
                }
            }
            if (drive->motor_state >= CLEM_DISK_MOTOR_ON) {
                /* We can read or write to this disk,
                   Adjust track position based on time since last update,
                   using optimal bit timing from the WOZ file
                */
               bit_index = (dt_us / 4)
               byte_index = bit_index / 8


            }
            break;
    }

    if (drive->motor_state == CLEM_DISK_MOTOR_OFF) {
        if (io_flags & CLEM_IWM_FLAG_DRIVE_ON) {
            drive->motor_state = CLEM_DISK_MOTOR_SWITCH_ON;
        }
    } else if (drive->motor_state == CLEM_DISK_MOTOR_SWITCH_ON) {

    }
    } else if (drive->motor_on) {
        if (!(io_flags & CLEM_IWM_FLAG_DRIVE_ON)) {
            drive->motor_switch_us = _clem_disk_timer_decrement(
                drive->motor_switch_us, dt_us);
            if (drive->motor_switch_us == 0) {
                drive->motor_on = false;
            }
        } else {
            if (drive->motor_switch_us > 0) {

            }
            drive->motor_switch_us = _clem_disk_timer_increment(
                drive->motor_switch_us, 1000000, dt_us);
        }


    }
    if (in_phase != drive->q03_switch) {
        /* clamp quarter track index to 5.25" limits */
        int offset = s_disk2_phase_states[drive->q03_switch & 0xf][in_phase & 0xf];
        drive->qtr_track_index += offset;
        if (drive->qtr_track_index < 0) drive->qtr_track_index = 0;
        else if (drive->qtr_track_index >= 160) drive->qtr_track_index = 160;
        CLEM_LOG("Disk525[%u]: Motor: %u; Head @ (%d,%d)",
            drive->motor_on,
            (io_flags & CLEM_IWM_FLAG_DRIVE_2) ? 2 : 1,
            drive->qtr_track_index / 4, drive->qtr_track_index % 4);
        drive->q03_switch = in_phase;
    }
}

static void _clem_disk_update_state(
    struct ClemensDriveBay* drives,
    uint64_t delta_ns,
    unsigned io_flags,
    unsigned in_phase
) {
    if (io_flags & CLEM_IWM_FLAG_DRIVE_35) {
        if (io_flags & CLEM_IWM_FLAG_DRIVE_1) {
            _clem_disk_update_state_35(
                &drives->slot5[0], delta_ns, io_flags, in_phase);
        } else if (io_flags & CLEM_IWM_FLAG_DRIVE_2) {
            _clem_disk_update_state_35(
                &drives->slot5[1], delta_ns, io_flags, in_phase);
        }
    } else {
        if (io_flags & CLEM_IWM_FLAG_DRIVE_1) {
            _clem_disk_update_state_525(
                &drives->slot6[0], delta_ns, io_flags, in_phase);
        } else if (io_flags & CLEM_IWM_FLAG_DRIVE_2) {
            _clem_disk_update_state_525(
                &drives->slot6[1], delta_ns, io_flags, in_phase);
        }
    }
}


void clem_disk_reset_drives(struct ClemensDriveBay* drives) {
    _clem_disk_reset_drive(&drives->slot5[0]);
    _clem_disk_reset_drive(&drives->slot5[1]);
    _clem_disk_reset_drive(&drives->slot6[0]);
    _clem_disk_reset_drive(&drives->slot6[1]);
}


void clem_iwm_reset(struct ClemensDeviceIWM* iwm) {
    memset(iwm, 0, sizeof(*iwm));
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

void clem_iwm_glu_sync(struct ClemensDeviceIWM* iwm, uint32_t delta_ns) {
    iwm->time_slice_ns += delta_ns;
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
   uint8_t ioreg,
   uint8_t op
) {
    unsigned old_io_flags = iwm->io_flags;
    unsigned old_out_phase = iwm->out_phase;
    bool old_q6 = iwm->q6_switch;
    bool old_q7 = iwm->q7_switch;

    switch (ioreg) {
        case CLEM_MMIO_REG_IWM_DRIVE_DISABLE:
            iwm->io_flags &= ~CLEM_IWM_FLAG_DRIVE_ON;
            break;
        case CLEM_MMIO_REG_IWM_DRIVE_ENABLE:
            iwm->io_flags |= CLEM_IWM_FLAG_DRIVE_ON;
            break;
        case CLEM_MMIO_REG_IWM_DRIVE_0:
            iwm->io_flags |= CLEM_IWM_FLAG_DRIVE_1;
            iwm->io_flags &= ~CLEM_IWM_FLAG_DRIVE_2;
            break;
        case CLEM_MMIO_REG_IWM_DRIVE_1:
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
            iwm->q6_switch = false;
            break;
        case CLEM_MMIO_REG_IWM_Q7_HI:
            iwm->q6_switch = true;
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

    /* IWM state switch */
    if (old_q6 != iwm->q6_switch || old_q7 != iwm->q7_switch) {

    }
    /* Drive state switch */
    _clem_disk_update_state(drives,
                            iwm->time_slice_ns,
                            iwm->io_flags,
                            iwm->out_phase);
    iwm->time_slice_ns = 0;
}

void clem_iwm_write_switch(
    struct ClemensDeviceIWM* iwm,
    struct ClemensDriveBay* drives,
    uint8_t ioreg,
    uint8_t value
) {
    switch (ioreg) {
        case CLEM_MMIO_REG_DISK_INTERFACE:
            if (value & 0x80) {
                iwm->io_flags |= CLEM_IWM_FLAG_HEAD_SEL;
            } else {
                iwm->io_flags &= ~CLEM_IWM_FLAG_HEAD_SEL;
            }
            if (value & 0x40) {
                iwm->io_flags |= CLEM_IWM_FLAG_DRIVE_35;
            } else {
                iwm->io_flags &= ~CLEM_IWM_FLAG_DRIVE_35;
            }
            if (value & 0x3f) {
                CLEM_LOG("Setting unexpected flags for %0X: %02X", ioreg, value);
            }
            break;
        default:
            _clem_iwm_io_switch(iwm, drives, ioreg, CLEM_IO_WRITE);
            if (ioreg & 1) {
                /* write data */
            }
            break;
    }

    /* access IWM registers if ioreg address is odd */
}

uint8_t clem_iwm_read_switch(
    struct ClemensDeviceIWM* iwm,
    struct ClemensDriveBay* drives,
    uint8_t ioreg,
    uint8_t flags
) {
    uint8_t result = 0x00;

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
            if (!(flags & CLEM_MMIO_READ_NO_OP)) {
                _clem_iwm_io_switch(iwm, drives, ioreg, CLEM_IO_READ);
            }
            if (!(ioreg & 1)) {
                /* read data latch */
            }
            break;
    }

    return result;
}
