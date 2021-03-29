#include "clem_device.h"
#include "clem_mmio_defs.h"
#include "clem_debug.h"

/*
 * ADB will be implemented in phases based on need
 *
 * General Pattern:
 *  Clemens <-> ADB Translation Layer <-> ADB GLU/Micro <- Client Input Devices
 *
 * Keyboard Support
 *  Clemens <-> ADB R/W switch Keyboard ADB Device at Address 2 <- Client Keyboard
 *              If Autopoll off, Interrupt Forwarding
 *      - Assume Autopoll on, so $c000, etc are updated
 *      - c026 returns 0
 *          assert if c026 is written to
 *      - c027 should reflect keyboard register $C000, no mouse and no interrupts
 *      - c024 returns 0
 *      - c025 contains key modifier flags
 *      - c000 contains keyboard data
 *      - build up from here if we need to during ROM bootup
 *          - ADB commands and interrupts will be the big thing
 */

void clem_adb_reset(struct ClemensDeviceADB* adb) {

}

void clem_adb_write_switch(
    struct ClemensDeviceADB* adb,
    uint8_t ioreg,
    uint8_t value,
    uint8_t flags
) {

}

uint8_t clem_adb_read_switch(
    struct ClemensDeviceADB* adb,
    uint8_t ioreg,
    uint8_t flags
) {

    return 0;
}
