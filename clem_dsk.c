/**
 * @file clem_dsk.c
 * @author your name (you@domain.com)
 * @brief Apple IIgs  disk image utilities
 * @version 0.1
 * @date 2021-05-13
 *
 * @copyright Copyright (c) 2021
 *
 */

#include "clem_dsk.h"

/*
  References:

  Beneath Apple DOS
  Understanding the Apple //e
    - formatting
    - Disk II/5.25" architecture

  140K 5.25" Format:
    35 tracks, 16 sectors/track
    256 bytes per sector
    300 RPM 4us per bit
  800K 3.5" Format:
    https://en.wikipedia.org/wiki/List_of_floppy_disk_formats#cite_note-23
    https://support.apple.com/kb/TA39910?locale=en_US&viewlocale=en_US
    80 tracks
    8-12 sectors per track (80 / 5 = 16 tracks per group),
      group 0 = 12, 1 = 11, 2 = 10, 3 = 9, 4 = 8
    512 bytes per sector
    394-590 RPM 2us per bit
*/

int clem_dsk_load(uint8_t* data, unsigned data_sz) {
  /* 1. discover the image type - may require an additional hint parameter
     2. derive some common attributes from the image type
        a. track count
        b. sector count per track (each track has a sector count)
     3. for DOS disks,
  */

}
