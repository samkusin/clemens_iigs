#ifndef CLEM_DRIVE_H
#define CLEM_DRIVE_H

#include "clem_types.h"


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
/* Places drive in 'write' mode */
#define CLEM_IWM_FLAG_WRITE_REQUEST         0x00000040
/*  Write protect for disk for 5.25", and the sense input bit for 3.5" drives*/
#define CLEM_IWM_FLAG_WRPROTECT_SENSE       0x00000080
/*  Read pulse from the disk/drive bitstream is on */
#define CLEM_IWM_FLAG_READ_DATA             0x00000100
/*  Write pulse input to drive */
#define CLEM_IWM_FLAG_WRITE_DATA            0x00000200
/*  For debugging only */
#define CLEM_IWM_FLAG_PULSE_HIGH            0x00008000

#define CLEM_IWM_FLAG_WRITE_HEAD_ON \
    (CLEM_IWM_FLAG_WRITE_DATA + CLEM_IWM_FLAG_WRITE_REQUEST)

#define CLEM_IWM_DRIVE_INVALID_TRACK_POS    0xffffffff

#ifdef __cplusplus
extern "C" {
#endif

void clem_disk_reset_drives(struct ClemensDriveBay* drives);

void clem_disk_start_drive(struct ClemensDrive* drive);

void clem_disk_read_and_position_head_35(
    struct ClemensDrive* drive,
    unsigned *io_flags,
    unsigned in_phase,
    unsigned delta_ns
);

void clem_disk_update_head_35(
    struct ClemensDrive* drive,
    unsigned *io_flags,
    unsigned delta_ns
);

void clem_disk_read_and_position_head_525(
    struct ClemensDrive* drive,
    unsigned *io_flags,
    unsigned in_phase,
    unsigned delta_ns
);

void clem_disk_update_head(
    struct ClemensDrive* drive,
    unsigned *io_flags,
    unsigned delta_ns
);

unsigned clem_drive_pre_step(
    struct ClemensDrive* drive,
    unsigned *io_flags
);

unsigned clem_drive_step(
    struct ClemensDrive* drive,
    unsigned *io_flags,
    int qtr_track_index,
    unsigned track_cur_pos,
    unsigned dt_ns
);

#ifdef __cplusplus
} // extern "C"
#endif


#endif
