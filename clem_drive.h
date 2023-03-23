#ifndef CLEM_DRIVE_H
#define CLEM_DRIVE_H

#include "clem_mmio_types.h"
#include "clem_shared.h"

#define CLEM_IWM_FLAG_MASK_PRE_STEP_CLEARED                                                        \
    (CLEM_IWM_FLAG_WRPROTECT_SENSE + CLEM_IWM_FLAG_READ_DATA + CLEM_IWM_FLAG_READ_DATA_FAKE)

#define CLEM_IWM_FLAG_WRITE_HEAD_ON (CLEM_IWM_FLAG_WRITE_DATA + CLEM_IWM_FLAG_WRITE_REQUEST)

#define CLEM_IWM_DRIVE_INVALID_TRACK_POS 0xffffffff

#define CLEM_IWM_DISK35_STATUS_STEP_IN    0x0001
#define CLEM_IWM_DISK35_STATUS_IO_HEAD_HI 0x0002
#define CLEM_IWM_DISK35_STATUS_EJECTED    0x0008
#define CLEM_IWM_DISK35_STATUS_EJECTING   0x0010
#define CLEM_IWM_DISK35_STATUS_STROBE     0x8000

#ifdef __cplusplus
extern "C" {
#endif

void clem_disk_reset_drives(struct ClemensDriveBay *drives);
void clem_disk_start_drive(struct ClemensDrive *drive);

void clem_disk_read_and_position_head_35(struct ClemensDrive *drive, unsigned *io_flags,
                                         unsigned in_phase, clem_clocks_duration_t clocks_dt);

void clem_disk_35_start_eject(struct ClemensDrive *drive);

void clem_disk_read_and_position_head_525(struct ClemensDrive *drive, unsigned *io_flags,
                                          unsigned in_phase, clem_clocks_duration_t clocks_dt);
unsigned clem_drive_pre_step(struct ClemensDrive *drive, unsigned *io_flags);

unsigned clem_drive_step(struct ClemensDrive *drive, unsigned *io_flags, int qtr_track_index,
                         unsigned track_cur_pos, clem_clocks_duration_t clocks_dt);

void clem_disk_write_head(struct ClemensDrive *drive, unsigned *io_flags,
                          clem_clocks_duration_t clocks_dt);

void clem_disk_update_head(struct ClemensDrive *drive, unsigned *io_flags,
                           clem_clocks_duration_t clocks_dt);

#ifdef __cplusplus
} // extern "C"
#endif

#endif
