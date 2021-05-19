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
#include "clem_woz.h"

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

void clem_iwm_reset(struct ClemensDeviceIWM* glu) {

}

void clem_iwm_insert_disk(
   struct ClemensDeviceIWM* glu,
   enum ClemensDriveType drive_type,
   struct ClemensWOZDisk* disk
) {

}

void clem_iwm_eject_disk(
   struct ClemensDeviceIWM* glu,
   enum ClemensDriveType drive_type
) {

}

void clem_iwm_glu_sync(struct ClemensDeviceIWM* glu, uint32_t delta_us) {

}

void clem_iwm_write_switch(
   struct ClemensDeviceIWM* glu,
   uint8_t ioreg,
   uint8_t value
) {

}

uint8_t clem_iwm_read_switch(
   struct ClemensDeviceIWM* glu,
   uint8_t ioreg,
   uint8_t flags
) {
   return 0x00;
}
