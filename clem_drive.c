/**
 * @file clem_drive.c
 * @author your name (you@domain.com)
 * @brief
 * @version 0.1
 * @date 2021-06-06
 *
 * @copyright Copyright (c) 2021
 *
 */

#include "clem_drive.h"
#include "clem_debug.h"
#include "clem_shared.h"
#include "clem_util.h"

#include <stdlib.h>

extern int clem_disk_control_35(struct ClemensDrive *drive, unsigned *io_flags, unsigned in_phase,
                                clem_clocks_duration_t clocks_dt);

/*  Disk II stepper emulation

    Much of this is based on Understanding the Apple IIe Chapter 9
        Specifically the section on the head arm mechanism (9-6 to 9-7)

    There are some assumptions made here based on experimentation:
        - The Sector Zero Bootloader in internal ROM not only forces the arm
          to track 0, but also the cog turned by the stepper magnets will
          ensure that the cog is phase aligned to the last activated phase.
        - This is 'proved' by some timings that analyze the sector 0
          bootloader here: https://embeddedmicro.weebly.com/apple-2iie.html
        - The waveform shows that PH0 is held high at the end of boot
        - Also verified in the emulator that PH0 is held high while sector 0
          is loaded into memory

    Our goal is to emulate how the phase magnets move the drive arm.  Stepper
    motors by definition employ magnets to turn a cog that precisely moves the
    arm inward towards the spindle, or outwards towards the outer edge of the
    disk.  The teeth on the cog are polarized so that magnets can move the
    arm in quarter or half track increments.

    Though not mechanically accurate, we can use scale down this cog to a
    'single-tooth' cog, where the tooth is orientated like a compass needle
    (8 cardinal directions.)  Phase magnets are positioned around this cog.

    Given:
    - The cog's orientation: N, NE, E, SE, S, SW, W, NW
    - Magnets at N, E, S, W

    Logic:
    - The cog will move if a phase magnet is active adjacent to it
    - But the cog will not move if the phase magnet lies directly opposite to
      the cog's orientation
    - Special cases are if two *adjacent* phase magnets are on, which can
      allow for 'quarter track' placement
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
static int s_disk2_phase_states[8][16] = {
    /*       00  N0  0E  NE  S0  x0  SE  xE  0W  NW  0x  Nx  SW  xW  Sx  xx */
    /* N  */ {0, 0, 2, 1, 0, 0, 3, 2, -2, -1, 0, 0, -3, -2, 0, 0},
    /* NE */ {0, -1, 1, 0, 3, -1, 2, 1, -3, -2, 1, -1, 0, -3, 3, 0},
    /*  E */ {0, -2, 0, -1, 2, 0, 1, 0, 0, -3, 0, -2, 3, 0, 2, 0},
    /* SE */ {0, -3, -1, -2, 1, 1, 0, -1, 3, 0, 1, -3, 2, 3, 1, 0},
    /* S  */ {0, 0, -2, -3, 0, 0, -1, -2, 2, 3, 0, 0, 1, 2, 0, 0},
    /* SW */ {0, 3, -3, 0, -1, -1, -2, -3, 1, 2, 1, 3, 0, 1, -1, 0},
    /*  W */ {0, 2, 0, 3, -2, 0, -3, 0, 0, 1, 0, 2, -1, 0, -2, 0},
    /* NW */ {0, 1, 3, 2, -3, 1, 0, 3, -1, 0, -1, 1, -2, -1, -3, 0}};

#define CLEM_IWM_DRIVE_MAX_RANDOM_BITS (8 * CLEM_IWM_DRIVE_RANDOM_BYTES)

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

static void _clem_disk_reset_drive(struct ClemensDrive *drive) {
    int rand_max = (int)(RAND_MAX * 0.30f);
    drive->real_track_index = 0xfe;
    drive->random_bit_index = 0;
    drive->qtr_track_index = 0;
    drive->status_mask_35 = 0;

    clem_disk_start_drive(drive);

    /* not going to change the cog orientation since this could be a soft
       reset */
    /* crappy method to randomize 30-ish percent ON bits (30% per WOZ
       recommendation, subject to experimentation
    */
    do {
        unsigned random_byte_index = (drive->random_bit_index / 8);
        unsigned random_bit_shift = (drive->random_bit_index % 8);
        if (rand() < rand_max) {
            drive->random_bits[random_byte_index] |= (1 << random_bit_shift);
        } else {
            drive->random_bits[random_byte_index] &= ~(1 << random_bit_shift);
        }
        ++drive->random_bit_index;
    } while (drive->random_bit_index < CLEM_IWM_DRIVE_MAX_RANDOM_BITS);
    drive->random_bit_index = 0;
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
static inline unsigned _clem_disk_get_track_bit_length(struct ClemensDrive *drive,
                                                       int qtr_track_index, bool is_drive_525) {
    if (drive->disk.meta_track_map[qtr_track_index] != 0xff) {
        return drive->disk.track_bits_count[(drive->disk.meta_track_map[qtr_track_index])];
    }
    /* empty track - use a 6400 byte, 51200 bit track size per WOZ2 spec */
    return 51200;
}

static inline void _clem_disk_write_bit(struct ClemensDrive *drive, bool value) {
    uint8_t *data =
        drive->disk.bits_data + (drive->disk.track_byte_offset[drive->real_track_index]);
    if (value) {
        data[drive->track_byte_index] |= (1 << drive->track_bit_shift);
    } else {
        data[drive->track_byte_index] &= ~(1 << drive->track_bit_shift);
    }
    drive->disk.is_dirty = true;
}

static inline uint8_t _clem_disk_get_byte(struct ClemensDrive *drive) {
    uint8_t *data =
        drive->disk.bits_data + (drive->disk.track_byte_offset[drive->real_track_index]);
    return data[drive->track_byte_index];
}

static inline bool _clem_disk_read_bit(struct ClemensDrive *drive) {
    uint8_t byte_value = _clem_disk_get_byte(drive);
    return (byte_value & (1 << drive->track_bit_shift)) != 0;
}

static inline bool _clem_disk_read_fake_bit_525(struct ClemensDrive *drive) {
    uint8_t random_byte = (drive->random_bits[drive->random_bit_index / 8]);
    bool random_bit = (random_byte & (1 << (drive->random_bit_index % 8))) != 0;
    return random_bit;
}

static inline unsigned _clem_disk_get_track_position(struct ClemensDrive *drive) {
    unsigned track_cur_pos = (drive->track_byte_index * 8) + (7 - drive->track_bit_shift);
    return track_cur_pos;
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
int clem_disk_control_525(struct ClemensDrive *drive, unsigned *io_flags, unsigned in_phase,
                          clem_clocks_duration_t clocks_dt) {
    int qtr_track_index = drive->qtr_track_index;
    int next_qtr_track_index;
    int qtr_track_delta;

    drive->is_spindle_on = true;

    /* clamp quarter track index to 5.25" limits */
    /* turning a cog that can be oriented in one of 8 directions */
    qtr_track_delta = (s_disk2_phase_states[drive->cog_orient & 0x7][in_phase & 0xf]);
    drive->cog_orient = (drive->cog_orient + qtr_track_delta) % 8;
    next_qtr_track_index = qtr_track_index + qtr_track_delta;
    if (next_qtr_track_index < 0) {
        CLEM_DEBUG("IWM: Disk525[%u]: Motor: %u; CLACK",
                   (*io_flags & CLEM_IWM_FLAG_DRIVE_2) ? 2 : 1,
                   (*io_flags & CLEM_IWM_FLAG_DRIVE_ON) ? 1 : 0);

        next_qtr_track_index = 0;
    } else if (next_qtr_track_index >= 160)
        next_qtr_track_index = 160;

    if (next_qtr_track_index != qtr_track_index) {
        CLEM_DEBUG(
            "IWM: Disk525[%u]: Motor: %u; %u -> %u", (*io_flags & CLEM_IWM_FLAG_DRIVE_2) ? 2 : 1,
            (*io_flags & CLEM_IWM_FLAG_DRIVE_ON) ? 1 : 0, qtr_track_index, next_qtr_track_index);
    }

    qtr_track_index = next_qtr_track_index;

    drive->ctl_switch = in_phase;

    if (drive->disk.is_write_protected) {
        *io_flags |= CLEM_IWM_FLAG_WRPROTECT_SENSE;
    }
    return qtr_track_index;
}

////////////////////////////////////////////////////////

void clem_disk_start_drive(struct ClemensDrive *drive) {
    drive->ctl_switch = 0;
    drive->track_byte_index = 0;
    drive->track_bit_shift = 7;
    drive->pulse_clocks_dt = 0;
    drive->read_buffer = 0;
    drive->is_spindle_on = false;
    drive->current_byte = 0x00;
}

void clem_disk_reset_drives(struct ClemensDriveBay *drives) {
    _clem_disk_reset_drive(&drives->slot5[0]);
    _clem_disk_reset_drive(&drives->slot5[1]);
    _clem_disk_reset_drive(&drives->slot6[0]);
    _clem_disk_reset_drive(&drives->slot6[1]);
}

void clem_disk_control(struct ClemensDrive *drive, unsigned *io_flags, unsigned in_phase,
                       clem_clocks_duration_t clocks_dt) {
    unsigned track_cur_pos = _clem_disk_get_track_position(drive);
    int qtr_track_index;
    bool is_drive_525 = (*io_flags & CLEM_IWM_FLAG_DRIVE_35) ? false : true;
    bool valid_disk_data = false;

    *io_flags &= ~CLEM_IWM_FLAG_MASK_PRE_STEP_CLEARED;

    if (is_drive_525) {
        qtr_track_index = clem_disk_control_525(drive, io_flags, in_phase, clocks_dt);
    } else {
        qtr_track_index = clem_disk_control_35(drive, io_flags, in_phase, clocks_dt);
    }
    if (qtr_track_index != drive->qtr_track_index && drive->has_disk) {
        if (drive->disk.meta_track_map[drive->qtr_track_index] !=
            drive->disk.meta_track_map[qtr_track_index]) {
            /* force lookup of the real track if the arm has changed */
            drive->real_track_index = 0xfe;
        }

        drive->qtr_track_index = qtr_track_index;
    }
    if (drive->has_disk) {
        if (drive->real_track_index == 0xfe) {
            unsigned track_prev_len = drive->track_bit_length;
            drive->real_track_index = drive->disk.meta_track_map[drive->qtr_track_index];
            if (drive->real_track_index != 0xff) {
                drive->track_bit_length =
                    _clem_disk_get_track_bit_length(drive, drive->qtr_track_index, is_drive_525);
            } else if (drive->track_bit_length == 0) {
                /* just use the prior bit length if there's no track defined */
                drive->track_bit_length = drive->disk.track_bits_count[0];
            }
            if (track_prev_len) {
                //  TODO: this should be 64-bit math or reexamined - using double here for
                //  expediency until Carmen Sandiego copy protection bug is fixed
                track_cur_pos = (unsigned)(track_cur_pos *
                                           ((double)(drive->track_bit_length) / track_prev_len));
            }
        }
    } else {
        /* also we need to fake it being write protected?  check this? */
        drive->qtr_track_index = qtr_track_index;
        if (is_drive_525) {
            *io_flags |= CLEM_IWM_FLAG_WRPROTECT_SENSE;
        }
    }
    if (drive->track_bit_length > 0) {
        if (track_cur_pos >= drive->track_bit_length) {
            /* wrap to beginning of track */
            track_cur_pos -= drive->track_bit_length;
        }
        CLEM_ASSERT(track_cur_pos < drive->track_bit_length);
    } else {
        track_cur_pos = 0;
    }
    drive->track_byte_index = track_cur_pos / 8;
    drive->track_bit_shift = 7 - (track_cur_pos % 8);
}

void clem_disk_step(struct ClemensDrive *drive, unsigned *io_flags) {
    bool valid_disk_data = false;
    bool is_drive_525 = (*io_flags & CLEM_IWM_FLAG_DRIVE_35) ? false : true;

    valid_disk_data = drive->has_disk && drive->real_track_index != 0xff &&
                      drive->disk.track_initialized[drive->real_track_index];

    if (valid_disk_data) {
        if (_clem_disk_read_bit(drive)) {
            drive->read_buffer |= 0x1;
        }
    }
    /* 3.5" drives don't have the same hardware as the Disk II, so
        I *think* fake bits don't apply.
        WRONG!!  Though the hardware isn't the same, randomness can still
        occur.  No real documentation on how similiar this is to Disk II
        random behavior.
    */
    // if (is_drive_525) {
    if ((drive->read_buffer & 0xf) && valid_disk_data) {
        if (drive->read_buffer & 0x2) {
            *io_flags |= CLEM_IWM_FLAG_READ_DATA;
        }
    } else {
        *io_flags |= CLEM_IWM_FLAG_READ_DATA_FAKE;
        if (_clem_disk_read_fake_bit_525(drive)) {
            *io_flags |= CLEM_IWM_FLAG_READ_DATA;
        }
    }
    //} else {
    //    if (drive->read_buffer & 0x1) {
    //        *io_flags |= CLEM_IWM_FLAG_READ_DATA;
    //    }
    //}

    drive->current_byte = (drive->current_byte & 0xfe) | (drive->read_buffer & 0x01);

    /* read a pulse from the bitstream, following WOZ emulation suggestions
        to emulate errors - this is effectively a copypasta from
        https://applesaucefdc.com/woz/reference2/
       here we pulse the read head, meaning we need a new bit from the buffer */
    drive->read_buffer <<= 1;
    if (*io_flags & CLEM_IWM_FLAG_READ_DATA_FAKE) {
        drive->random_bit_index = (drive->random_bit_index + 1) % CLEM_IWM_DRIVE_MAX_RANDOM_BITS;
    }
}

void clem_disk_write_head(struct ClemensDrive *drive, unsigned *io_flags) {
    bool write_pulse = (*io_flags & CLEM_IWM_FLAG_WRITE_HEAD_ON) == (CLEM_IWM_FLAG_WRITE_HEAD_ON);
    bool write_transition = write_pulse != drive->write_pulse;

    if (!(*io_flags & CLEM_IWM_FLAG_DRIVE_ON)) {
        return;
    }
    if (!drive->has_disk) {
        return;
    }
    if (!(*io_flags & CLEM_IWM_FLAG_WRITE_REQUEST)) {
        drive->write_pulse = false;
        return;
    }

    if (!drive->disk.is_write_protected) {
        if (drive->real_track_index != 0xff) {
            if (!drive->disk.track_initialized[drive->real_track_index]) {
                if (write_transition) {
                    /* first time write to an uninitialized track will start
                    writes at the beginning of the data block.  most
                    likely this occurs via formatting.
                    The first genuine byte will have its high bit will
                    always be on as defined by GCR 6-2 encoding
                    */
                    drive->disk.track_initialized[drive->real_track_index] = 1;
                    drive->track_bit_shift = 7;
                    drive->track_byte_index = 0;
                }
            }
            if (drive->disk.track_initialized[drive->real_track_index]) {
                _clem_disk_write_bit(drive, write_transition);
                drive->current_byte =
                    (drive->current_byte & 0xfe) | (write_transition ? 0x01 : 0x00);
            }
        }
    }
    if (write_transition) {
        *io_flags |= CLEM_IWM_FLAG_WRITE_HI;
    }
    drive->write_pulse = write_pulse;
}

void clem_disk_update_head(struct ClemensDrive *drive, unsigned *io_flags) {
    if (!(*io_flags & CLEM_IWM_FLAG_DRIVE_ON)) {
        return;
    }
    drive->current_byte <<= 1;
    if (drive->track_bit_shift == 0) {
        drive->track_bit_shift = 7;
        ++drive->track_byte_index;
    } else {
        --drive->track_bit_shift;
    }
}
