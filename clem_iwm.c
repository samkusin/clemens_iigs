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
#include "clem_util.h"

#include <stdlib.h>
#include <string.h>

/*  Enable 3.5 drive seris */
#define CLEM_IWM_FLAG_DRIVE_35              0x00000001
/*  Drive system is active - in tandem with drive index selected */
#define CLEM_IWM_FLAG_DRIVE_ON              0x00000002
/*  Drive 1 selected - note IWM only allows one drive at a time, but the
    disk port has two pins for drive, so emulating that aspect */
#define CLEM_IWM_FLAG_DRIVE_1               0x00000004
/*  Drive 2 selected */
#define CLEM_IWM_FLAG_DRIVE_2               0x00000008
/*  Congolmerate mask for any-drive selected */
#define CLEM_IWM_FLAG_DRIVE_ANY             (CLEM_IWM_FLAG_DRIVE_1 + \
                                             CLEM_IWM_FLAG_DRIVE_2)
/* device flag, 3.5" side 2 (not used for 5.25")
   note, this really is used for 3.5" drive controller actions:
   https://llx.com/Neil/a2/disk
*/
#define CLEM_IWM_FLAG_HEAD_SEL              0x00000010
/*  Write protect for disk for 5.25", and the sense input bit for 3.5" drives*/
#define CLEM_IWM_FLAG_WRPROTECT_SENSE       0x00000080
/*  Read pulse from the disk/drive bitstream is on */
#define CLEM_IWM_FLAG_READ_DATA             0x00000100


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

/* Cut from Jim Slater's decompilation of the LSS ROM for the Disk II "DOS 3.3"
   5.25 disk encoding, indexed with (sequence in the upper nibble, Qx/Pulse in
   the lower nibble) addresses.   See Understanding the Apple //e for more
   information.   There appears to be no IWM/IIgs equivalent that's readily
   available.
*/
static uint8_t s_lss_525_rom[256] = {
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


/*  Follows the status and control values from https://llx.com/Neil/a2/disk
 */
#define CLEM_IWM_DISK35_QUERY_STEP_DIR      0x00
#define CLEM_IWM_DISK35_QUERY_IO_HEAD_LOWER 0x01
#define CLEM_IWM_DISK35_QUERY_DISK_IN_DRIVE 0x02
#define CLEM_IWM_DISK35_QUERY_IO_HEAD_UPPER 0x03
#define CLEM_IWM_DISK35_QUERY_IS_STEPPING   0x04
#define CLEM_IWM_DISK35_QUERY_WRITE_PROTECT 0x06
#define CLEM_IWM_DISK35_QUERY_MOTOR_ON      0x08
#define CLEM_IWM_DISK35_QUERY_DOUBLE_SIDED  0x09
#define CLEM_IWM_DISK35_QUERY_TRACK_0       0x0A
#define CLEM_IWM_DISK35_QUERY_READ_READY    0x0B
#define CLEM_IWM_DISK35_QUERY_EJECTED       0x0C
#define CLEM_IWM_DISK35_QUERY_60HZ_ROTATION 0x0E
#define CLEM_IWM_DISK35_QUERY_ENABLED       0x0F

#define CLEM_IWM_DISK35_CTL_STEP_IN         0x00
#define CLEM_IWM_DISK35_CTL_STEP_OUT        0x01
#define CLEM_IWM_DISK35_CTL_EJECTED_RESET   0x03
#define CLEM_IWM_DISK35_CTL_STEP_ONE        0x04
#define CLEM_IWM_DISK35_CTL_MOTOR_ON        0x08
#define CLEM_IWM_DISK35_CTL_MOTOR_OFF       0x09
#define CLEM_IWM_DISK35_CTL_EJECT           0x0D

#define CLEM_IWM_DISK35_STATE_MOTOR_ON      0x01
#define CLEM_IWM_DISK35_STATE_STEP_IN       0x02
#define CLEM_IWM_DISK35_STATE_STEP_ONE      0x04

#define CLEM_IWM_STATE_READ_DATA            0x00
#define CLEM_IWM_STATE_READ_STATUS          0x01
#define CLEM_IWM_STATE_WRITE_HANDSHAKE      0x02
#define CLEM_IWM_STATE_WRITE_MODE           0x03
#define CLEM_IWM_STATE_WRITE_DATA           0x13

/*
    Emulation of disk drives and the IWM Controller.

    Summary: input will come from WOZ files (or coverted to WOZ on the fly by
    emulators using their own tooling.) As a result this isn't a straight
    emulation of the Disk II or 3.5" floppy, but of reading data from generated
    WOZ track data.

    The IWM interface abstracts the 3.5 floppy controller which doesn't provide
    direct control of the stepper motor - so the 4 IWM control registers
    interface with the floppy controller chip.
*/

inline static unsigned _clem_disk_timer_decrement(
    unsigned timer_ns,
    unsigned dt_ns
) {
    return (timer_ns - dt_ns < timer_ns) ? (timer_ns - dt_ns) : 0;
}

inline static unsigned _clem_disk_timer_increment(
    unsigned timer_ns,
    unsigned timer_max_ns,
    unsigned dt_ns
) {
    if (timer_ns + dt_ns > timer_ns) {
        timer_ns += dt_ns;
    }
    if (timer_ns + dt_ns < timer_ns) {
        timer_ns = UINT32_MAX;
    } else {
        timer_ns += dt_ns;
    }
    if (timer_ns > timer_max_ns) {
        timer_ns = timer_max_ns;
    }
    return timer_ns;
}

static void _clem_disk_reset_drive(struct ClemensDrive* drive) {
    drive->q03_switch = 0;
    drive->pulse_ns = 0;
    drive->track_byte_index = 0;
    drive->track_bit_shift = 8;
    drive->read_buffer = 0;
    drive->real_track_index = 0xff;
    drive->random_bit_index = 0;
    /* crappy method to randomize 30-ish percent ON bits (30% per WOZ
       recommendation, subject to experimentation
    */
    do {
        if (rand() < RAND_MAX/3) {
            drive->random_bits[drive->random_bit_index / 32] |= (
                1 << (drive->random_bit_index % 8));
        } else {
            drive->random_bits[drive->random_bit_index / 32] &= ~(
                1 << (drive->random_bit_index % 8));
        }
        ++drive->random_bit_index;
    } while (drive->random_bit_index != 0);
    drive->random_bits[0] = 0xf00f003;
}

static unsigned _clem_disk_step_state_35(
    struct ClemensDrive* drive,
    unsigned dt_ns
) {
    if (drive->state_35 & CLEM_IWM_DISK35_STATE_STEP_ONE) {
        drive->step_timer_35_ns = _clem_disk_timer_decrement(
            drive->step_timer_35_ns, dt_ns);
        if (drive->step_timer_35_ns == 0) {
            if (drive->state_35 & CLEM_IWM_DISK35_STATE_STEP_IN) {
                if ((drive->qtr_track_index % 2) < 80) {
                    drive->qtr_track_index += 2;
                }
            } else {
                if ((drive->qtr_track_index % 2) > 1) {
                    drive->qtr_track_index -= 2;
                }
            }
            drive->state_35 &= ~CLEM_IWM_DISK35_STATE_STEP_ONE;
        }
    }
    return drive->state_35;
}


static unsigned _clem_disk_exec_ctl_35(
    struct ClemensDrive* drive,
    unsigned ctl
) {
    unsigned old_state = drive->state_35;
    switch (ctl) {
        case CLEM_IWM_DISK35_CTL_STEP_IN:
            drive->state_35 |= CLEM_IWM_DISK35_STATE_STEP_IN;
            break;
        case CLEM_IWM_DISK35_CTL_STEP_OUT:
            drive->state_35 &= ~CLEM_IWM_DISK35_STATE_STEP_IN;
            break;
        case CLEM_IWM_DISK35_CTL_MOTOR_ON:
            drive->state_35 |= CLEM_IWM_DISK35_STATE_MOTOR_ON;
            break;
        case CLEM_IWM_DISK35_CTL_MOTOR_OFF:
            drive->state_35 &= ~CLEM_IWM_DISK35_STATE_MOTOR_ON;
            break;
        case CLEM_IWM_DISK35_CTL_STEP_ONE:
            drive->state_35 |= CLEM_IWM_DISK35_STATE_STEP_ONE;
            drive->step_timer_35_ns = CLEM_1MS_NS * 5; /* very arbitrary... */
            break;
        case CLEM_IWM_DISK35_CTL_EJECTED_RESET:
            CLEM_LOG("clem_iwm: disk switch reset?");
            break;
    }
    return drive->state_35;
}

/*
    control/status set/get params:
        in_phase = PH0, PH1, PH2
        io_flags = HEAD_SEL

        control is set by toggling the PH3 bit from off -> on -> off
 */
static void _clem_disk_update_state_35(
    struct ClemensDrive* drive,
    unsigned *io_flags,
    unsigned in_phase,
    unsigned dt_ns
) {
    bool next_select = (*io_flags & CLEM_IWM_FLAG_HEAD_SEL) != 0;
    bool query_true = true;

    drive->state_35 = _clem_disk_step_state_35(drive, dt_ns);

    if (in_phase != drive->q03_switch || drive->select_35 != next_select) {
        if (!(drive->q03_switch & 0x08) && (in_phase & 0x08)) {
            /* do control action */
            unsigned ctl = ((in_phase << 2)       & 0x08) |
                           ((in_phase << 2)       & 0x04) |
                           (!next_select ? 0x00 :   0x02) |
                           ((in_phase >> 2)       & 0x01);
            CLEM_LOG("clem_iwm: Disk35[%u]: Power: %u; Ctl: %02X",
                (*io_flags & CLEM_IWM_FLAG_DRIVE_2) ? 2 : 1,
                (*io_flags & CLEM_IWM_FLAG_DRIVE_ON) ? 1 : 0,
                ctl);
            drive->state_35 = _clem_disk_exec_ctl_35(drive, ctl);
        } else if (!(in_phase & 0x08)) {
            /* do status query */
            unsigned query = ((in_phase << 2)     & 0x08) |
                             ((in_phase << 2)     & 0x04) |
                             (!next_select ? 0x00 : 0x02) |
                             ((in_phase >> 2)     & 0x01);
            drive->query_35 = query;
        }
        drive->q03_switch = in_phase;
        drive->select_35 = next_select;
    }

    switch (drive->query_35) {
        case CLEM_IWM_DISK35_QUERY_STEP_DIR:
            if (drive->state_35 & CLEM_IWM_DISK35_STATE_STEP_IN) {
                query_true = true;
            }
            break;
        case CLEM_IWM_DISK35_QUERY_IO_HEAD_LOWER:
            break;
        case CLEM_IWM_DISK35_QUERY_DISK_IN_DRIVE:
            query_true = false;
            break;
        case CLEM_IWM_DISK35_QUERY_IO_HEAD_UPPER:
            break;
        case CLEM_IWM_DISK35_QUERY_IS_STEPPING:
            if (drive->state_35 & CLEM_IWM_DISK35_STATE_STEP_ONE) {
                query_true = true;
            }
            break;
        case CLEM_IWM_DISK35_QUERY_WRITE_PROTECT:
            break;
        case CLEM_IWM_DISK35_QUERY_MOTOR_ON:
            if (drive->state_35 & CLEM_IWM_DISK35_STATE_MOTOR_ON) {
                query_true = true;
            }
            break;
        case CLEM_IWM_DISK35_QUERY_DOUBLE_SIDED:
            if (!drive->data || !(drive->data->flags & CLEM_WOZ_IMAGE_DOUBLE_SIDED)) {
                query_true = false;
            }
            break;
        case CLEM_IWM_DISK35_QUERY_TRACK_0:
            if (drive->qtr_track_index == 0) {
                query_true = true;
            }
            break;
        case CLEM_IWM_DISK35_QUERY_READ_READY:
            query_true = false;
            break;
        case CLEM_IWM_DISK35_QUERY_EJECTED:
            break;
        case CLEM_IWM_DISK35_QUERY_60HZ_ROTATION:
            break;
        case CLEM_IWM_DISK35_QUERY_ENABLED:
            query_true = false;
            break;
    }
    if (!query_true) {
        *io_flags |= CLEM_IWM_FLAG_WRPROTECT_SENSE;
    } else {
        *io_flags &= ~CLEM_IWM_FLAG_WRPROTECT_SENSE;
    }
}

/*  Mechanical Summary: 5.25"

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
*/
static unsigned _clem_disk_get_track_bit_length_525(
    struct ClemensDrive* drive,
    int qtr_track_index
) {
    if (drive->data->meta_track_map[qtr_track_index] != 0xff) {
        return drive->data->track_bits_count[(
            drive->data->meta_track_map[qtr_track_index])];
    }
    /* empty track - use a 6400 byte, 51200 bit track size per WOZ2 spec */
    return 51200;
}

static uint8_t _clem_disk_read_bit_525(struct ClemensDrive* drive) {
    uint8_t* data = drive->data->bits_data + (
        drive->data->track_byte_offset[drive->real_track_index]);
    uint8_t byte_value = data[drive->track_byte_index];
    return (byte_value & (1 << (drive->track_bit_shift-1))) != 0;
}

static inline uint8_t _clem_disk_read_fake_bit_525(struct ClemensDrive* drive) {
    uint8_t random_bit = (drive->random_bits[drive->random_bit_index / 32] & (
        1 << (drive->random_bit_index % 8)));
    ++drive->random_bit_index;
    return random_bit ? CLEM_IWM_FLAG_READ_DATA : 0x00;
}

/**
 * @brief Emulates a 5.25" Disk II compliant drive
 *
 * Emulation covers:
 * - drive head placement (for WOZ compliant images) based on stepper phases
 * - ensure head accesses data at specific index within a track based on timing
 * - reading/writing bit to disk
 * - errors from a MC3470 like processor
 *
 * Does not cover:
 * - reading nibbles, LSS, IWM related data
 *
 * @param drive
 * @param delta_ns
 * @param io_flags
 * @param in_phase
 */
static void _clem_disk_update_state_525(
    struct ClemensDrive* drive,
    unsigned *io_flags,
    unsigned in_phase,
    unsigned dt_ns
) {
    int qtr_track_index = drive->qtr_track_index;
    unsigned track_cur_pos;

    *io_flags &= ~CLEM_IWM_FLAG_READ_DATA;
    *io_flags &= ~CLEM_IWM_FLAG_WRPROTECT_SENSE;

    if (!(*io_flags & CLEM_IWM_FLAG_DRIVE_ON)) {
        return;
    }
    drive->pulse_ns = _clem_disk_timer_increment(drive->pulse_ns, 1000000, dt_ns);
    if (!drive->data) {
        /* no disk - just send empty pulses... right? */
        /* also we need to fake it being write protected?  check this? */
        *io_flags |= CLEM_IWM_FLAG_WRPROTECT_SENSE;
        if (drive->pulse_ns >= 4000) {
            *io_flags &= ~CLEM_IWM_FLAG_READ_DATA;
            drive->pulse_ns -= 4000;
        }
    } else if (drive->pulse_ns >= drive->data->bit_timing_ns) {
        --drive->track_bit_shift;
        if (drive->track_bit_shift == 0) {
            ++drive->track_byte_index;
            drive->track_bit_shift = 8;
        }
        /* read a pulse from the bitstream, following WOZ emulation suggestions
            to emulate errors - this is effectively a copypasta from
            https://applesaucefdc.com/woz/reference2/ */
        drive->read_buffer <<= 1;
        if (drive->real_track_index != 0xff) {
            drive->read_buffer |= _clem_disk_read_bit_525(drive);
            if ((drive->read_buffer & 0x0f) != 0) {
                if (drive->read_buffer & 0x02) {
                    *io_flags |= CLEM_IWM_FLAG_READ_DATA;
                }
            } else {
                *io_flags |= _clem_disk_read_fake_bit_525(drive);
            }
        } else {
            *io_flags |= _clem_disk_read_fake_bit_525(drive);
        }
        drive->pulse_ns -= drive->data->bit_timing_ns;
    }

    if (in_phase != drive->q03_switch) {
        /* clamp quarter track index to 5.25" limits */
        int qtr_track_delta = (
            s_disk2_phase_states[drive->q03_switch & 0xf][in_phase & 0xf]);
        qtr_track_index += qtr_track_delta;
        if (qtr_track_index < 0) qtr_track_index = 0;
        else if (qtr_track_index >= 160) qtr_track_index = 160;
        CLEM_LOG("clem_iwm: Disk525[%u]: Motor: %u; Head @ (%d,%d)",
            (*io_flags & CLEM_IWM_FLAG_DRIVE_2) ? 2 : 1,
            (*io_flags & CLEM_IWM_FLAG_DRIVE_ON) ? 1 : 0,
            qtr_track_index / 4, qtr_track_index % 4);
        drive->q03_switch = in_phase;
    }

    if (!drive->data) {
        return;
    }
    track_cur_pos = (drive->track_byte_index * 8) + (8 - drive->track_bit_shift);
    if (drive->track_bit_length == 0) {
        drive->track_bit_length = _clem_disk_get_track_bit_length_525(
            drive, drive->qtr_track_index);
    }
    if (qtr_track_index != drive->qtr_track_index) {
        unsigned track_next_len = _clem_disk_get_track_bit_length_525(
            drive, qtr_track_index);
        track_cur_pos = track_cur_pos * track_next_len / drive->track_bit_shift;
        drive->track_byte_index = track_cur_pos / 8;
        drive->track_bit_shift = 8 - (track_cur_pos % 8);
        drive->track_bit_length = track_next_len;
        drive->qtr_track_index = qtr_track_index;
        drive->real_track_index = drive->data->meta_track_map[qtr_track_index];
    }
    if (track_cur_pos >= drive->track_bit_length) {
        /* wrap to beginning of track */
        track_cur_pos -= drive->track_bit_length;
        drive->track_byte_index = track_cur_pos / 8;
        drive->track_bit_shift = 8 - (track_cur_pos % 8);
    }

    if (drive->data->flags & CLEM_WOZ_IMAGE_WRITE_PROTECT) {
        *io_flags |= CLEM_IWM_FLAG_WRPROTECT_SENSE;
    }
}

static int _clem_disk_update_state(
    struct ClemensDriveBay* drives,
    unsigned *io_flags,
    unsigned in_phase,
    int ns_budget
) {
    /* update active drive on the IWM, spinning it this frame,
       lag is only used for simulating an active drive so if the drive is not
       spinning, clear our lag
    */
    int ns_spent = 0;
    while (ns_budget >= CLEM_IWM_LSS_CYCLE_NS) {
        if (*io_flags & CLEM_IWM_FLAG_DRIVE_35) {
            if (*io_flags & CLEM_IWM_FLAG_DRIVE_1) {
                _clem_disk_update_state_35(
                    &drives->slot5[0], io_flags, in_phase, CLEM_IWM_LSS_CYCLE_NS);
            }
            if (*io_flags & CLEM_IWM_FLAG_DRIVE_2) {
                _clem_disk_update_state_35(
                    &drives->slot5[1], io_flags, in_phase, CLEM_IWM_LSS_CYCLE_NS);
            }
        } else {
            if (*io_flags & CLEM_IWM_FLAG_DRIVE_1) {
                _clem_disk_update_state_525(
                    &drives->slot6[0], io_flags, in_phase, CLEM_IWM_LSS_CYCLE_NS);
            }
            if (*io_flags & CLEM_IWM_FLAG_DRIVE_2) {
                _clem_disk_update_state_525(
                    &drives->slot6[1], io_flags, in_phase, CLEM_IWM_LSS_CYCLE_NS);
            }
        }
        ns_budget -= CLEM_IWM_LSS_CYCLE_NS;
        ns_spent += CLEM_IWM_LSS_CYCLE_NS;
    }
    return ns_spent;
}


void clem_disk_reset_drives(struct ClemensDriveBay* drives) {
    _clem_disk_reset_drive(&drives->slot5[0]);
    _clem_disk_reset_drive(&drives->slot5[1]);
    _clem_disk_reset_drive(&drives->slot6[0]);
    _clem_disk_reset_drive(&drives->slot6[1]);
}


void clem_iwm_reset(struct ClemensDeviceIWM* iwm) {
    memset(iwm, 0, sizeof(*iwm));

    /* Jim Sather's 'example' initial state - evaluate if it's necessary */
    iwm->lss_seq = 0x02;
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


void _clem_iwm_lss(struct ClemensDeviceIWM* iwm) {
    /* indexing rom instructions generated by Jim Slater,
       seq | read/write | shift/load | QA | pulse
       note that senses like write protect may be acquired using the status
       register - which may always be used in even legacy code?
       if not, then we need to determine if:
        5.25 and synchronous, use the LSS only for A2 compatability
        5.25 and asynchronous, use the LSS and override when needed to read
            from status/handshake registers
    */
    unsigned adr = (unsigned)(iwm->lss_seq) << 4 |
                   (iwm->q7_switch ? 0x08 : 00) |
                   (iwm->q6_switch ? 0x04 : 00) |
                   ((iwm->latch & 0x80) ? 0x02 : 00) |
                   ((iwm->io_flags & CLEM_IWM_FLAG_READ_DATA) ? 0x01 : 00);
    unsigned cmd = s_lss_525_rom[adr];

    if (cmd & 0x08) {
        switch (cmd & 0xf) {
            case 0x08:              /* NOP */
            case 0x0C:
                break;
            case 0x09:              /* SL0 */
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

    iwm->lss_seq = (cmd & 0xf0) >> 4;
}

void clem_iwm_glu_sync(
    struct ClemensDeviceIWM* iwm,
    struct ClemensDriveBay* drives,
    struct ClemensClock* clock
) {
    int delta_ns = _clem_calc_ns_step_from_clocks(
        clock->ts - iwm->last_clocks_ts, clock->ref_step);
    int ns_budget = 0;
    if (iwm->io_flags & CLEM_IWM_FLAG_DRIVE_ON) {
        /* handle the 1 second drive motor timer */
        if (iwm->ns_drive_hold > 0) {
            iwm->ns_drive_hold = _clem_disk_timer_decrement(
                iwm->ns_drive_hold, delta_ns);
            if (iwm->ns_drive_hold == 0 || iwm->timer_1sec_disabled) {
                CLEM_LOG("clem_iwm: turning drive off in sync");
                iwm->io_flags &= ~CLEM_IWM_FLAG_DRIVE_ON;
            }
        }
    }
    if (iwm->io_flags & CLEM_IWM_FLAG_DRIVE_ON) {
        ns_budget = delta_ns;
        while (ns_budget >= CLEM_IWM_LSS_CYCLE_NS) {
            if (iwm->io_flags & CLEM_IWM_FLAG_DRIVE_35) {
                if (iwm->io_flags & CLEM_IWM_FLAG_DRIVE_1) {
                    _clem_disk_update_state_35(&drives->slot5[0],
                                               &iwm->io_flags,
                                               iwm->out_phase,
                                               CLEM_IWM_LSS_CYCLE_NS);
                }
                if (iwm->io_flags & CLEM_IWM_FLAG_DRIVE_2) {
                    _clem_disk_update_state_35(&drives->slot5[1],
                                               &iwm->io_flags,
                                               iwm->out_phase,
                                               CLEM_IWM_LSS_CYCLE_NS);
                }
            } else {
                if (iwm->io_flags & CLEM_IWM_FLAG_DRIVE_1) {
                    _clem_disk_update_state_525(&drives->slot6[0],
                                                &iwm->io_flags,
                                                iwm->out_phase,
                                                CLEM_IWM_LSS_CYCLE_NS);
                }
                if (iwm->io_flags & CLEM_IWM_FLAG_DRIVE_2) {
                    _clem_disk_update_state_525(&drives->slot6[1],
                                                &iwm->io_flags,
                                                iwm->out_phase,
                                                CLEM_IWM_LSS_CYCLE_NS);
                }
            }
            _clem_iwm_lss(iwm);
            ns_budget -= CLEM_IWM_LSS_CYCLE_NS;
        }
    }
    iwm->last_clocks_ts = clock->ts - _clem_calc_clocks_step_from_ns(
        ns_budget, clock->ref_step);
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
    bool old_q6 = iwm->q6_switch, old_q7 = iwm->q7_switch;

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
            }
            iwm->io_flags |= CLEM_IWM_FLAG_DRIVE_ON;
            iwm->ns_drive_hold = 0;
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

    if (old_q6 != iwm->q6_switch || old_q7 != iwm->q7_switch) {
        unsigned last_state = (old_q7 ? 0x02 : 0x00) |
                              (old_q6 ? 0x01 : 0x00);
        unsigned this_state = (iwm->q7_switch ? 0x02 : 0x00) |
                              (iwm->q6_switch ? 0x01 : 0x00);
        CLEM_LOG("clem_iwm: state from %02X to %02X", last_state, this_state);
    }

    if (iwm->io_flags & CLEM_IWM_FLAG_DRIVE_ON) {
        /* emulate the drive just enough to catch up with the current
           cycles spent executing the instruction that invokes this IO.
           we also adjust our lag timer negatively, so that our later
           call to glu_sync() will only IWM frames for the remaining time
           budget this emulation frame
        */
       /* TODO: use clocks instead of cycles here... and in the IWM struct */
       clem_iwm_glu_sync(iwm, drives, clock);
    }
}

static void _clem_iwm_write_mode(struct ClemensDeviceIWM* iwm, uint8_t value) {
    if (value & 0x10) {
        iwm->clock_8mhz = true;
        CLEM_WARN("clem_iwm: 8mhz mode requested");
    } else {
        iwm->clock_8mhz = false;
    }
    if (value & 0x08) {
        iwm->fast_mode = true;
    } else {
        iwm->fast_mode = false;
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
            iwm->data = value;
            _clem_iwm_io_switch(iwm, drives, clock, ioreg, CLEM_IO_WRITE);
            if (ioreg & 1) {
                if (iwm->q7_switch && iwm->q6_switch) {
                    if (!(iwm->io_flags & CLEM_IWM_FLAG_DRIVE_ON)) {
                        _clem_iwm_write_mode(iwm, value);
                    }
                }
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
    /* TODO: a bunch of IIgs specfic modes */
    if (iwm->clock_8mhz) {
        result |= 0x10;
    }
    if (iwm->fast_mode) {
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

uint8_t clem_iwm_read_switch(
    struct ClemensDeviceIWM* iwm,
    struct ClemensDriveBay* drives,
    struct ClemensClock* clock,
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
                _clem_iwm_io_switch(iwm, drives, clock, ioreg, CLEM_IO_READ);
            }
            if (!(ioreg & 1)) {
                /* read data latch */
                if (iwm->q6_switch) {
                    if (!iwm->q7_switch) {
                        result = _clem_iwm_read_status(iwm);
                    }
                } else {
                    if (iwm->q7_switch) {
                        if (iwm->io_flags & CLEM_IWM_FLAG_DRIVE_35) {
                            /* read handshake */
                        } else {
                            /* use latch value generated by lss */
                            result = 0;
                        }
                    } else {
                        result = iwm->latch;
                    }
                }
            }
            break;
    }

    return result;
}
