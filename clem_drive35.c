/* Basis of this implementation comes from the following references:

    "Controlling the 3.5 Drive Hardware on the Apple IIGS"
        https://llx.com/Neil/a2/disk
*/


#include "clem_debug.h"
#include "clem_drive.h"
#include "clem_util.h"
#include "clem_woz.h"


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

#define CLEM_IWM_DISK35_STATUS_STEP_IN      0x0001
#define CLEM_IWM_DISK35_STATUS_IO_HEAD_HI   0x0002
#define CLEM_IWM_DISK35_STATUS_EJECTED      0x0008
#define CLEM_IWM_DISK35_STATUS_EJECTING     0x0010
#define CLEM_IWM_DISK35_STATUS_STROBE       0x8000

#define CLEM_IWM_DISK35_STEP_TIME_NS        (12 * 1000)
#define CLEM_IWM_DISK35_EJECT_TIME_NS       (500 * 1000000)

#include <stdio.h>
#include <assert.h>

/*
 * Emulation of the 3.5" IIgs drive (non Smartport)*
 *
 * Commands are dispatched using the in_phase and io_flags input/output "pins"
 * These commands fall into two categories "query" and "control".
 *
 * The IWM specifies the command via the PHASE0, PHASE1, PHASE2 and HEAD_SEL
 * inputs
 *
 * The PHASE3 input signal indicates a query vs control command.
 *  If LO, then a query is peformed, and its status is returned via the
 *      WRPROTECT_SENSE output.
 *  If HI, then a command is executed.
 */

void clem_disk_read_and_position_head_35(
    struct ClemensDrive* drive,
    unsigned *io_flags,
    unsigned in_phase,
    unsigned dt_ns
) {
    bool sense_out = false;
    bool ctl_strobe = (in_phase & 0x8) != 0;
    unsigned cur_step_timer_ns = drive->step_timer_35_ns;
    unsigned ctl_switch;
    unsigned track_cur_pos;
    int qtr_track_index = drive->qtr_track_index;

    track_cur_pos = clem_drive_pre_step(drive, io_flags);
    if (track_cur_pos == CLEM_IWM_DRIVE_INVALID_TRACK_POS) {
        /* should we clear state ? */
        return;
    }

    drive->step_timer_35_ns = clem_util_timer_decrement(
        cur_step_timer_ns, dt_ns);

    if (!drive->step_timer_35_ns && drive->step_timer_35_ns < cur_step_timer_ns) {
        /* step or eject completed */
        if (drive->status_mask_35 & CLEM_IWM_DISK35_STATUS_EJECTING) {
            drive->status_mask_35 &= ~CLEM_IWM_DISK35_STATUS_EJECTING;
            drive->status_mask_35 |= CLEM_IWM_DISK35_STATUS_EJECTED;
            drive->data = NULL;
            CLEM_LOG("clem_drive35: ejected disk");
        } else {
            if (drive->status_mask_35 & CLEM_IWM_DISK35_STATUS_STEP_IN) {
                if (qtr_track_index < 158) {
                    qtr_track_index += 2;
                    CLEM_DEBUG("clem_drive35: stepped in track = %u", qtr_track_index);
                }
            } else {
                if (qtr_track_index >= 2) {
                    qtr_track_index -= 2;
                    CLEM_DEBUG("clem_drive35: stepped out track = %u", qtr_track_index);
                }
            }
        }
    }

    ctl_switch = (*io_flags & CLEM_IWM_FLAG_HEAD_SEL) ? 0x2 : 0x0;
    ctl_switch |= ((in_phase >> 2) & 0x1);     /* PHASE2 */
    ctl_switch |= ((in_phase << 2) & 0x4);     /* PHASE0 */
    ctl_switch |= ((in_phase << 2) & 0x8);     /* PHASE1 */

    if (ctl_strobe) {
        drive->status_mask_35 |= CLEM_IWM_DISK35_STATUS_STROBE;
    } else {
        if (drive->status_mask_35 & CLEM_IWM_DISK35_STATUS_STROBE) {
            drive->status_mask_35 &= ~CLEM_IWM_DISK35_STATUS_STROBE;
            /* control strobe = perform command now */
            switch (ctl_switch) {
                case CLEM_IWM_DISK35_CTL_STEP_IN:
                    drive->status_mask_35 |= CLEM_IWM_DISK35_STATUS_STEP_IN;
                    CLEM_DEBUG("clem_drive35: step to inward tracks");
                    break;
                case CLEM_IWM_DISK35_CTL_STEP_OUT:
                    drive->status_mask_35 &= ~CLEM_IWM_DISK35_STATUS_STEP_IN;
                    CLEM_DEBUG("clem_drive35: step to outward tracks");
                    break;
                case CLEM_IWM_DISK35_CTL_EJECTED_RESET:
                    drive->status_mask_35 &= ~CLEM_IWM_DISK35_STATUS_EJECTED;
                    break;
                case CLEM_IWM_DISK35_CTL_STEP_ONE:
                    if (!(drive->status_mask_35 & CLEM_IWM_DISK35_STATUS_EJECTING)) {
                        drive->step_timer_35_ns = CLEM_IWM_DISK35_STEP_TIME_NS;
                        CLEM_DEBUG("clem_drive35: step from track %u", qtr_track_index);
                    } else {
                        CLEM_LOG("clem_drive35: attempt to step while ejecting");
                    }
                    break;
                case CLEM_IWM_DISK35_CTL_MOTOR_ON:
                    if (!drive->is_spindle_on) {
                        drive->is_spindle_on = true;
                        drive->pulse_ns = 0;
                        drive->read_buffer = 0;
                    }
                    CLEM_DEBUG("clem_drive35: drive motor on");
                    break;
                case CLEM_IWM_DISK35_CTL_MOTOR_OFF:
                    drive->is_spindle_on = false;
                    CLEM_DEBUG("clem_drive35: drive motor off");
                    break;
                case CLEM_IWM_DISK35_CTL_EJECT:
                    if (!(drive->status_mask_35 & CLEM_IWM_DISK35_STATUS_EJECTING)) {
                        drive->is_spindle_on = false;
                        drive->status_mask_35 |= CLEM_IWM_DISK35_STATUS_EJECTING;
                        drive->step_timer_35_ns = CLEM_IWM_DISK35_EJECT_TIME_NS;
                        CLEM_LOG("clem_drive35: ejecting disk");
                    }
                    break;
                default:
                    CLEM_LOG("clem_drive35: ctl %02X not supported?", ctl_switch);
                    break;
            }
            /*
            CLEM_LOG("clem_drive35: control switch %02X <= %02X",
                        ctl_switch, drive->ctl_switch);
            */
        } else {
            /* control query */
            switch (ctl_switch) {
                case CLEM_IWM_DISK35_QUERY_STEP_DIR:
                    sense_out = (drive->status_mask_35 & CLEM_IWM_DISK35_STATUS_STEP_IN) == 0;
                    break;
                case CLEM_IWM_DISK35_QUERY_DISK_IN_DRIVE:
                    sense_out = (drive->data == NULL);
                    break;
                case CLEM_IWM_DISK35_QUERY_IS_STEPPING:
                    sense_out = (drive->step_timer_35_ns == 0);
                    break;
                case CLEM_IWM_DISK35_QUERY_WRITE_PROTECT:
                    if (drive->data) {
                        sense_out = (drive->data && (
                            drive->data->flags & CLEM_WOZ_IMAGE_WRITE_PROTECT)) == 0;
                    } else {
                        sense_out = false;
                    }
                    break;
                case CLEM_IWM_DISK35_QUERY_MOTOR_ON:
                    sense_out = !drive->is_spindle_on;
                    break;
                case CLEM_IWM_DISK35_QUERY_TRACK_0:
                    sense_out = (drive->qtr_track_index != 0);
                    break;
                case CLEM_IWM_DISK35_QUERY_EJECTED:
                    sense_out = (drive->status_mask_35 & CLEM_IWM_DISK35_STATUS_EJECTED) == 0;
                    break;
                case CLEM_IWM_DISK35_QUERY_60HZ_ROTATION:
                    assert(true);
                    break;
                case CLEM_IWM_DISK35_QUERY_IO_HEAD_LOWER:
                    if (drive->status_mask_35 & CLEM_IWM_DISK35_STATUS_IO_HEAD_HI) {
                        qtr_track_index -= 1;
                        drive->status_mask_35 &= ~CLEM_IWM_DISK35_STATUS_IO_HEAD_HI;
                        //CLEM_LOG("clem_drive35: switching to lower head, track = %u", qtr_track_index);
                    }
                    break;
                case CLEM_IWM_DISK35_QUERY_IO_HEAD_UPPER:
                    if (!(drive->status_mask_35 & CLEM_IWM_DISK35_STATUS_IO_HEAD_HI)) {
                        qtr_track_index += 1;
                        drive->status_mask_35 |= CLEM_IWM_DISK35_STATUS_IO_HEAD_HI;
                        //CLEM_LOG("clem_drive35: switching to upper head, track = %u", qtr_track_index);
                    }
                    break;
                case CLEM_IWM_DISK35_QUERY_DOUBLE_SIDED:
                    sense_out = (drive->data && drive->data->flags & CLEM_WOZ_IMAGE_DOUBLE_SIDED) != 0;
                    break;
                case CLEM_IWM_DISK35_QUERY_READ_READY:
                    sense_out = (drive->step_timer_35_ns > 0);
                    break;
                case CLEM_IWM_DISK35_QUERY_ENABLED:
                    /* TODO, can this drive be disabled? */
                    sense_out = false;
                    break;
                default:
                    CLEM_LOG("clem_drive35: query %02X not supported?", ctl_switch);
                    break;
            }
            /*
            if (ctl_switch != drive->ctl_switch || ctl_strobe) {
                CLEM_LOG("clem_drive35: query switch %02X <= %02X",
                         ctl_switch, drive->ctl_switch);
            }
            */
        }
    }
    drive->ctl_switch = ctl_switch;

    track_cur_pos = clem_drive_step(drive,
                                    io_flags,
                                    qtr_track_index,
                                    track_cur_pos, dt_ns);

    if (sense_out) {
        *io_flags |= CLEM_IWM_FLAG_WRPROTECT_SENSE;
    } else {
        *io_flags &= ~CLEM_IWM_FLAG_WRPROTECT_SENSE;
    }
}
