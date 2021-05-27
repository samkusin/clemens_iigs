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
    * Head position (i.e. track, half)
    * Read and Write Positions on the current track

    For 5.25 drives, this is trivial relative to 3.5" drives, which employed
    a variable speed motor to increase storage capability of the outer rings
    (which have more surface area compared with the inside rings.)

    Mechanical Summary: 3.5"

    The IWM interface abstracts the 3.5 floppy controller which doesn't provide
    direct control of the stepper motor - so the 4 IWM control registers
    interface with the floppy controller chip.
*/

void _clem_disk_reset_drive(struct ClemensDrive* drive) {
   drive->motor_on = false;
   drive->motor_switch_us = 0;
   drive->qtr_track_index = 0;
   drive->q03_switch = 0;
}

void _clem_disk_update_state_35(
   struct ClemensDrive* drive,
   uint64_t current_time_ns,
   unsigned io_flags,
   unsigned in_phase
) {

}

void _clem_disk_update_state_525(
   struct ClemensDrive* drive,
   uint64_t current_time_ns,
   unsigned io_flags,
   unsigned in_phase
) {
   unsigned dt_us = (unsigned)((current_time_ns - drive->clock_ns) / 1000);
   if (io_flags & CLEM_IWM_FLAG_DRIVE_ON) {
      if (!drive->motor_on) {
         drive->motor_on = true;
      }
      drive->motor_switch_us += dt_us;
      if (drive->motor_switch_us > 1000000) {
         drive->motor_switch_us = 1000000;
      }
   } else {
      if (drive->motor_on) {
         drive->motor_on = false;
      }
      if (drive->motor_switch_us - dt_us > drive->motor_switch_us) {
         drive->motor_switch_us = 0;
      } else {
         drive->motor_switch_us -= dt_us;
      }
   }

   drive->clock_ns = current_time_ns;
}


void clem_disk_reset_drives(struct ClemensDriveBay* drives) {
   _clem_disk_reset_drive(&drives->slot5[0]);
   _clem_disk_reset_drive(&drives->slot5[1]);
   _clem_disk_reset_drive(&drives->slot6[0]);
   _clem_disk_reset_drive(&drives->slot6[1]);
}

void _clem_disk_update_state(
   struct ClemensDriveBay* drives,
   uint64_t current_time_ns,
   unsigned io_flags,
   unsigned in_phase
) {
   if (io_flags & CLEM_IWM_FLAG_DRIVE_35) {
      if (io_flags & CLEM_IWM_FLAG_DRIVE_1) {
         _clem_disk_update_state_35(
            &drives->slot5[0], current_time_ns, io_flags, in_phase);
      } else if (io_flags & CLEM_IWM_FLAG_DRIVE_2) {
         _clem_disk_update_state_35(
            &drives->slot5[1], current_time_ns, io_flags, in_phase);
      }
   } else {
      if (io_flags & CLEM_IWM_FLAG_DRIVE_1) {
         _clem_disk_update_state_525(
            &drives->slot6[0], current_time_ns, io_flags, in_phase);
      } else if (io_flags & CLEM_IWM_FLAG_DRIVE_2) {
         _clem_disk_update_state_525(
            &drives->slot6[1], current_time_ns, io_flags, in_phase);
      }
   }
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
   iwm->current_time_ns += delta_ns;
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
   if (old_io_flags != iwm->io_flags || old_out_phase != iwm->out_phase) {
      _clem_disk_update_state(
         drives, iwm->current_time_ns, iwm->io_flags, iwm->out_phase);
   }
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
