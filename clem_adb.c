#include "clem_device.h"
#include "clem_mmio_defs.h"
#include "clem_debug.h"

#include <string.h>

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

/* ADB emulated GLU/Controller is ready for a command with a write to C026 */
#define CLEM_ADB_STATE_READY        0
/* GLU/controller will receive command data from the host via writes to C026 */
#define CLEM_ADB_STATE_CMD_DATA     1


/* ADB Sync, a combination of SetMode, ClearMode, SetConfig */
#define CLEM_ADB_CMD_SYNC           0x07

/* c027 status flags */
#define CLEM_ADB_C027_CMD_FULL     0x01

#define CLEM_ADB_ROM_3              0x06


void clem_adb_reset(struct ClemensDeviceADB* adb) {
    memset(adb, 0, sizeof(*adb));
    adb->version = CLEM_ADB_ROM_3;      /* TODO - input to reset? */
}

void clem_adb_device_input(
  struct ClemensDeviceADB* adb,
  struct ClemensInputEvent* input
) {
    // TODO: delegate keyboard input
}

void _clem_adb_glu_command(struct ClemensDeviceADB* adb) {
    switch (adb->cmd_reg) {
        case CLEM_ADB_CMD_SYNC:
            break;

    }
}

void clem_adb_glu(struct ClemensDeviceADB* adb) {
    switch (adb->state) {
        case CLEM_ADB_STATE_CMD_DATA:
            /* Consume incoming command data and execute the command once
               the expected data xfer from the host has completed
            */
            if (adb->cmd_data_sent > adb->cmd_data_recv) {
                ++adb->cmd_data_recv;
            } else {
                adb->cmd_status &= ~CLEM_ADB_C027_CMD_FULL;
            }
            if (adb->cmd_data_recv >= adb->cmd_data_limit) {
                //  TODO: run command immediately
                _clem_adb_glu_command(adb);
                adb->state = CLEM_ADB_STATE_READY;
            }
            break;
        default:
            break;
    }
}

/*  Some of this logic comes from the IIgs  HW and FW references and its
    practical application by the ROM/firmware.  Given that most apps should
    be using the firmware to communicate with ADB devices, this switching logic
    is meant to work with the ROM code and may not be a 100% accurate
    reimplementation of the ADB GLU/Microcrontroller.


    ADB Command/Data is Read, then the Status register bit 0 is cleared
    ADB Command Byte 0 = MODE or DEV/REG command
    ADB Command Byte 1 - XX if a command takes parameters

    ADB Command Read will read the current status and reset the command state

 */

static void _clem_adb_expect_data(
    struct ClemensDeviceADB* adb,
    unsigned state,
    uint8_t data_limit
) {
    adb->state = state;
    adb->cmd_data_sent = 0;
    adb->cmd_data_recv = 0;
    adb->cmd_data_limit = data_limit;
}

static void _clem_adb_start_cmd(struct ClemensDeviceADB* adb, uint8_t value) {
    adb->cmd_reg = value;
    adb->cmd_status |= CLEM_ADB_C027_CMD_FULL;

    switch (value) {
        case CLEM_ADB_CMD_SYNC:
            /* TODO - verify? */
            /* SetMode, ClearMode, Config */
            if (adb->version >= CLEM_ADB_ROM_3) {
                _clem_adb_expect_data(adb, CLEM_ADB_STATE_CMD_DATA, 8);
            } else {
                _clem_adb_expect_data(adb, CLEM_ADB_STATE_CMD_DATA, 4);
            }
            break;
        default:
            /* unimplemented? */
            CLEM_WARN("ADB command %02X unimplemented", value);
            break;
    }
}

static void _clem_adb_add_data(struct ClemensDeviceADB* adb, uint8_t value) {
    CLEM_ASSERT(adb->state == CLEM_ADB_STATE_CMD_DATA);
    if (adb->cmd_data_sent >= adb->cmd_data_limit) {
        CLEM_ASSERT(false);
        /* TODO: should do some kind of error handling? */
        return;
    }
    adb->cmd_data[adb->cmd_data_sent++] = value;
    adb->cmd_status |= CLEM_ADB_C027_CMD_FULL;
}

static void _clem_adb_write_cmd(struct ClemensDeviceADB* adb, uint8_t value) {
    switch (adb->state) {
        case CLEM_ADB_STATE_READY:
            _clem_adb_start_cmd(adb, value);
            break;
        case CLEM_ADB_STATE_CMD_DATA:
            CLEM_LOG("ADB Command Data [%02X]:%02X", adb->cmd_data_sent, value);
            _clem_adb_add_data(adb, value);
            break;
    }
}

void clem_adb_write_switch(
    struct ClemensDeviceADB* adb,
    uint8_t ioreg,
    uint8_t value
) {
    switch (ioreg) {
        case CLEM_MMIO_REG_ADB_CMD_DATA:
            _clem_adb_write_cmd(adb, value);
            break;
        default:
            CLEM_WARN("Unimplemented ADB write %02X,%02X", ioreg, value);
            break;
    }
}

static uint8_t _clem_adb_read_cmd(struct ClemensDeviceADB* adb, uint8_t flags) {
    uint8_t result = 0x00;
    switch (adb->state) {
        case CLEM_ADB_STATE_READY:
            if (!CLEM_IS_MMIO_READ_NO_OP(flags)) {
                adb->cmd_status &= ~CLEM_ADB_C027_CMD_FULL;
            }
            break;
        case CLEM_ADB_STATE_CMD_DATA:
            /* TODO: read back current data? *clear cmd valid? */
            break;
    }
    return result;
}

uint8_t clem_adb_read_switch(
    struct ClemensDeviceADB* adb,
    uint8_t ioreg,
    uint8_t flags
) {
    bool is_noop = (flags & CLEM_MMIO_READ_NO_OP) != 0;
    switch (ioreg) {
        case CLEM_MMIO_REG_ADB_CMD_DATA:
            return _clem_adb_read_cmd(adb, flags);
        case CLEM_MMIO_REG_ADB_STATUS:
            return adb->cmd_status;
        default:
            if (!CLEM_IS_MMIO_READ_NO_OP(flags)) {
                CLEM_WARN("Unimplemented ADB read %02X", ioreg);
            }
            break;
    }
    return 0;
}
