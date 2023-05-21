#include "clem_debug.h"
#include "clem_device.h"
#include "clem_mmio_defs.h"
#include "clem_util.h"

#include <string.h>

/*
 * ADB will be implemented in phases based on need
 *
 * General Pattern:
 *  - From Emulator Host (OS) to Emulated Device
 *  - From Emulator Device to GLU (ADB)
 *  - From GLU to Machine Host (MMIO/CPU)
 *
 * The 'GLU' here isn't an accurate emulation of the on-board GLU.  This GLU
 * just implements the ADB commands used by the machine, and forwards input
 * from the emulator host OS into our keyboard and mouse data structures.
 *
 * The GLU layer also provides keyboard/mouse data in its 'raw' form.
 * The GLU layer has an 'autopoll' mode, which updates the mega2 IO registers
 * with keyboard and mouse data automatically.
 *  - if autopoll is not enabled for these devices, the ADB host (machine)
 *    must have an ISR that handles SRQ events and will issue 'TALK' commands
 *    to the GLU
 *  - the TALK commands return data from the GLU logical registers
 *
 * References:
 *  IIgs Hardware Reference
 *  IIgs Firmware Reference
 *  "Inside the Apple IIgs ADB Controller
 *       https://llx.com/Neil/a2/adb.html
 *  "ADB - The Untold Story: Space Aliens Ate My Mouse"
 *      https://developer.apple.com/library/archive/technotes/hw/hw_01.html
 *
 *  and some eyeballing/reassurance from... KEGS.
 *
 *  SRQ: p 138-39 HWRef
 *      - Mouse SRQ issuance prohibited, meaning we only support keyboard SRQs
 *        for now
 *
 *  Key strobe/Any key (c010 vs c010-1f):
 *      - More contradictions, but both the IIgs Firmware and Apple //e tech
 *        references call out only C010 as the IO addr, versus the HW Ref p129,
 *        Table 6-4, bit 7 (writing anwehere to C010-1F).  If there's an app
 *        that breaks because HW Ref was right vs the other docs, then make
 *        that change accordingly for writes.
 */

/* ADB emulated GLU/Controller is ready for a command with a write to C026 */
#define CLEM_ADB_STATE_READY 0
/* GLU/controller will receive command data from the host via writes to C026 */
#define CLEM_ADB_STATE_CMD_DATA 1
/* GLU/controller will send data to the host, read by the host via C026 */
#define CLEM_ADB_STATE_RESULT_DATA 2

#define CLEM_ADB_CMD_ABORT              0x01
#define CLEM_ADB_CMD_SET_MODES          0x04
#define CLEM_ADB_CMD_CLEAR_MODES        0x05
#define CLEM_ADB_CMD_SET_CONFIG         0x06
#define CLEM_ADB_CMD_SYNC               0x07
#define CLEM_ADB_CMD_WRITE_RAM          0x08
#define CLEM_ADB_CMD_READ_MEM           0x09
#define CLEM_ADB_CMD_UNDOCUMENTED_12    0x12
#define CLEM_ADB_CMD_UNDOCUMENTED_13    0x13
#define CLEM_ADB_CMD_VERSION            0x0d
#define CLEM_ADB_CMD_DEVICE_ENABLE_SRQ  0x50
#define CLEM_ADB_CMD_DEVICE_FLUSH       0x60
#define CLEM_ADB_CMD_DEVICE_DISABLE_SRQ 0x70
#define CLEM_ADB_CMD_DEVICE_XMIT_2_R0   0x80
#define CLEM_ADB_CMD_DEVICE_XMIT_2_R1   0x90
#define CLEM_ADB_CMD_DEVICE_XMIT_2_R2   0xA0
#define CLEM_ADB_CMD_DEVICE_XMIT_2_R3   0xB0
#define CLEM_ADB_CMD_DEVICE_POLL_0      0xc0
#define CLEM_ADB_CMD_DEVICE_POLL_1      0xd0
#define CLEM_ADB_CMD_DEVICE_POLL_2      0xe0
#define CLEM_ADB_CMD_DEVICE_POLL_3      0xf0

/* c026 status flags */
#define CLEM_ADB_C026_RECV_READY 0x80
#define CLEM_ADB_C026_DESK_MGR   0x20
#define CLEM_ADB_C026_SRQ        0x08
#define CLEM_ADB_C026_RECV_CNT   0x07

/* c027 status flags */
#define CLEM_ADB_C027_CMD_FULL 0x01
/* HWRef says this is X, firmware ref and testing say this is Y */
#define CLEM_ADB_C027_MOUSE_Y 0x02
/* 0x04 - keyboard interrupts not supported */
#define CLEM_ADB_C027_KEY_IRQ    0x04
#define CLEM_ADB_C027_KEY_FULL   0x08
#define CLEM_ADB_C027_DATA_IRQ   0x10
#define CLEM_ADB_C027_DATA_FULL  0x20
#define CLEM_ADB_C027_MOUSE_IRQ  0x40
#define CLEM_ADB_C027_MOUSE_FULL 0x80

/* This version is returned by the ADB microcontroller based on ROM type */
#define CLEM_ADB_ROM_3 0x06

/* GLU Device addresses */
#define CLEM_ADB_DEVICE_KEYBOARD 0x02
#define CLEM_ADB_DEVICE_MOUSE    0x03

/* ADB Mode Flags */
#define CLEM_ADB_MODE_AUTOPOLL_KEYB  0x00000001
#define CLEM_ADB_MODE_AUTOPOLL_MOUSE 0x00000002

/**
 * SRQs are disabled for all devices on reset
 * Autopoll enabled for all devices on reset
 */
#define CLEM_ADB_GLU_SRQ_60HZ_CYCLES 600

void clem_adb_reset(struct ClemensDeviceADB *adb) {
    int i;

    adb->version = CLEM_ADB_ROM_3; /* TODO - input to reset? */
    adb->mode_flags = CLEM_ADB_MODE_AUTOPOLL_KEYB | CLEM_ADB_MODE_AUTOPOLL_MOUSE;
    adb->keyb.reset_key = false;
    adb->keyb.size = 0;
    adb->mouse.size = 0;
    adb->mouse.tracking_enabled = false;
    adb->mouse.valid_clamp_box = false;
    adb->gameport.ann_mask = 0;
    adb->gameport.btn_mask[0] = adb->gameport.btn_mask[1] = 0;
    adb->irq_dispatch = 0;
    for (i = 0; i < 4; ++i) {
        adb->gameport.paddle[i] = CLEM_GAMEPORT_PADDLE_AXIS_VALUE_INVALID;
        adb->gameport.paddle_timer_ns[i] = 0;
        adb->gameport.paddle_timer_state[i] = 0;
    }
}

static inline void _clem_adb_irq_dispatch(struct ClemensDeviceADB *adb, uint32_t irq) {
    //  Mouse SRQs are not supported per HW Reference
    //  Keyboard Interrupts (not SRQs) are also not supported per HW Reference
    if ((irq & CLEM_IRQ_ADB_DATA) && (adb->cmd_status & CLEM_ADB_C027_DATA_IRQ)) {
        adb->irq_dispatch |= CLEM_IRQ_ADB_DATA;
    }
    if ((irq & CLEM_IRQ_ADB_KEYB_SRQ) && (adb->keyb_reg[3] & CLEM_ADB_GLU_REG3_MASK_SRQ)) {
        adb->irq_dispatch |= CLEM_IRQ_ADB_KEYB_SRQ;
    }
    if ((irq & CLEM_IRQ_ADB_MOUSE_EVT) && (adb->cmd_status & CLEM_ADB_C027_MOUSE_IRQ)) {
        adb->irq_dispatch |= CLEM_IRQ_ADB_MOUSE_EVT;
    }
}

static void _clem_adb_expect_data(struct ClemensDeviceADB *adb, uint8_t data_limit) {
    adb->state = CLEM_ADB_STATE_CMD_DATA;
    adb->cmd_data_sent = 0;
    adb->cmd_data_recv = 0;
    adb->cmd_data_limit = data_limit;
}

static void _clem_adb_add_data(struct ClemensDeviceADB *adb, uint8_t value) {
    CLEM_ASSERT(adb->state == CLEM_ADB_STATE_CMD_DATA);
    if (adb->cmd_data_sent >= adb->cmd_data_limit) {
        CLEM_ASSERT(false);
        /* TODO: should do some kind of error handling? */
        return;
    }
    adb->cmd_data[adb->cmd_data_sent++] = value;
}

static void _clem_adb_glu_command_done(struct ClemensDeviceADB *adb) {
    adb->state = CLEM_ADB_STATE_READY;
    adb->cmd_data_sent = 0;
    adb->cmd_data_recv = 0;
    adb->cmd_data_limit = 0;
}

static void _clem_adb_glu_result_init(struct ClemensDeviceADB *adb, uint8_t data_limit) {
    adb->state = CLEM_ADB_STATE_RESULT_DATA;
    adb->cmd_data_sent = 0;
    adb->cmd_data_recv = 0;
    adb->cmd_data_limit = data_limit;
}

static void _clem_adb_glu_result_data(struct ClemensDeviceADB *adb, uint8_t value) {
    CLEM_ASSERT(adb->state == CLEM_ADB_STATE_RESULT_DATA);
    if (adb->cmd_data_sent >= adb->cmd_data_limit) {
        CLEM_ASSERT(false);
        /* TODO: should do some kind of error handling? */
        return;
    }
    adb->cmd_data[adb->cmd_data_sent++] = value;
    adb->cmd_status |= CLEM_ADB_C027_DATA_FULL;
    _clem_adb_irq_dispatch(adb, CLEM_IRQ_ADB_DATA);
}

static void _clem_adb_glu_device_response(struct ClemensDeviceADB *adb, uint8_t len) {
    adb->state = CLEM_ADB_STATE_READY;
    adb->cmd_data_sent = 0;
    adb->cmd_data_recv = 0;
    adb->cmd_data_limit = len;
    adb->cmd_flags |= CLEM_ADB_C026_RECV_READY;
    adb->cmd_flags &= ~CLEM_ADB_C026_RECV_CNT;
    adb->cmd_flags |= (len & CLEM_ADB_C026_RECV_CNT);
}

/*
 * Keyboard Support
 *
 * Autopolling: at regular intervals, look at registers and update
 *
 * From Emulator Host (OS) to Emulator Device
 *  - Input from the emulator app is converted to simple events representing
 *    an ADB device.
 *
 * From Emulated Device to Emulated ADB GLU/Microcontroller
 *  - during GLU update/sync, sync() will refresh the SRQ line if there is
 *    data available for the device (and SRQ is enabled for the device)
 *
 * GLU listen/talk
 *  - during sync(),
 *      autopoll ready? then acquire data from the device, clear srq
 *      if autopoll off, rely on manual talk commands, clear srq
 *      either way, update MMIO registers
 *
 *
 * References:
 *  Apple IIgs Hardware Reference
 *  https://developer.apple.com/library/archive/technotes/hw/hw_01.html
 */
#define CLEM_ADB_KEY_MOD_SHIFT   0x01
#define CLEM_ADB_KEY_MOD_CTRL    0x02
#define CLEM_ADB_KEY_MOD_CAPS    0x04
#define CLEM_ADB_KEY_MOD_CAPITAL 0x08 /* capitalized if caps lock on */
#define CLEM_ADB_KEY_MOD_KEYPAD  0x10
#define CLEM_ADB_KEY_MOD_OPTION  0x40
#define CLEM_ADB_KEY_MOD_APPLE   0x80

/* { default, ctl, shift,  ctrl+shift, extra }
 * Apple //e Technical Reference p 14-16
 * Apple IIgs Hardware Reference p 262-266
 * Extra keys (keypad = )
 * other bytes are for padding or later use
 *
 * No later ADB keyboard support for
 *  - function keys at this time (later ADB keyboards)
 *  - home, pageup, end, pagedown, etc
 */
static uint8_t g_a2_to_ascii[CLEM_ADB_KEY_CODE_LIMIT][8] = {
    /* 0x00 */ {
        'a',
        0x01,
        'A',
        0x01,
        CLEM_ADB_KEY_MOD_CAPITAL,
    },
    /* 0x01 */
    {
        's',
        0x13,
        'S',
        0x13,
        CLEM_ADB_KEY_MOD_CAPITAL,
    },
    /* 0x02 */
    {
        'd',
        0x04,
        'D',
        0x04,
        CLEM_ADB_KEY_MOD_CAPITAL,
    },
    /* 0x03 */
    {
        'f',
        0x06,
        'F',
        0x06,
        CLEM_ADB_KEY_MOD_CAPITAL,
    },
    /* 0x04 */
    {
        'h',
        0x08,
        'H',
        0x08,
        CLEM_ADB_KEY_MOD_CAPITAL,
    },
    /* 0x05 */
    {
        'g',
        0x07,
        'G',
        0x07,
        CLEM_ADB_KEY_MOD_CAPITAL,
    },
    /* 0x06 */
    {
        'z',
        0x1a,
        'Z',
        0x1a,
        CLEM_ADB_KEY_MOD_CAPITAL,
    },
    /* 0x07 */
    {
        'x',
        0x18,
        'X',
        0x18,
        CLEM_ADB_KEY_MOD_CAPITAL,
    },
    /* 0x08 */
    {
        'c',
        0x03,
        'C',
        0x03,
        CLEM_ADB_KEY_MOD_CAPITAL,
    },
    /* 0x09 */
    {
        'v',
        0x16,
        'V',
        0x16,
        CLEM_ADB_KEY_MOD_CAPITAL,
    },
    /* 0x0A */
    {
        0xff,
        0xff,
        0xff,
        0xff,
        0x00,
    },
    /* 0x0B */
    {
        'b',
        0x02,
        'B',
        0x02,
        CLEM_ADB_KEY_MOD_CAPITAL,
    },
    /* 0x0C */
    {
        'q',
        0x11,
        'Q',
        0x11,
        CLEM_ADB_KEY_MOD_CAPITAL,
    },
    /* 0x0D */
    {
        'w',
        0x17,
        'W',
        0x17,
        CLEM_ADB_KEY_MOD_CAPITAL,
    },
    /* 0x0E */
    {
        'e',
        0x05,
        'E',
        0x05,
        CLEM_ADB_KEY_MOD_CAPITAL,
    },
    /* 0x0F */
    {
        'r',
        0x12,
        'R',
        0x12,
        CLEM_ADB_KEY_MOD_CAPITAL,
    },
    /* 0x10 */
    {
        't',
        0x14,
        'T',
        0x14,
        CLEM_ADB_KEY_MOD_CAPITAL,
    },
    /* 0x11 */
    {
        'y',
        0x19,
        'Y',
        0x19,
        CLEM_ADB_KEY_MOD_CAPITAL,
    },
    /* 0x12 */
    {
        '1',
        '1',
        '!',
        '!',
        0x00,
    },
    /* 0x13 */
    {
        '2',
        0x00,
        '@',
        0x00,
        0x00,
    },
    /* 0x14 */
    {
        '3',
        '3',
        '#',
        '#',
        0x00,
    },
    /* 0x15 */
    {
        '4',
        '4',
        '$',
        '$',
        0x00,
    },
    /* 0x16 */
    {
        '6',
        0x1e,
        '^',
        0x1e,
        0x00,
    },
    /* 0x17 */
    {
        '5',
        '5',
        '%',
        '%',
        0x00,
    },
    /* 0x18 */
    {
        '=',
        '=',
        '+',
        '+',
        0x00,
    },
    /* 0x19 */
    {
        '9',
        '9',
        '(',
        '(',
        0x00,
    },
    /* 0x1A */
    {
        '7',
        '7',
        '&',
        '&',
        0x00,
    },
    /* 0x1B */
    {
        '-',
        0x1f,
        '_',
        0x1f,
        0x00,
    },
    /* 0x1C */
    {
        '8',
        '8',
        '*',
        '*',
        0x00,
    },
    /* 0x1D */
    {
        '0',
        '0',
        ')',
        ')',
        0x00,
    },
    /* 0x1E */
    {
        ']',
        0x1d,
        '}',
        0x1d,
        0x00,
    },
    /* 0x1F */
    {
        'o',
        0x0f,
        'O',
        0x0f,
        CLEM_ADB_KEY_MOD_CAPITAL,
    },
    /* 0x20 */
    {
        'u',
        0x15,
        'U',
        0x15,
        CLEM_ADB_KEY_MOD_CAPITAL,
    },
    /* 0x21 */
    {
        '[',
        0x1b,
        '{',
        0x1b,
        0x00,
    },
    /* 0x22 */
    {
        'i',
        0x09,
        'I',
        0x09,
        CLEM_ADB_KEY_MOD_CAPITAL,
    },
    /* 0x23 */
    {
        'p',
        0x10,
        'P',
        0x10,
        CLEM_ADB_KEY_MOD_CAPITAL,
    },
    /* 0x24 */
    {
        0x0d,
        0xff,
        0x0d,
        0xff,
        0x00,
    }, /* CR */
       /* 0x25 */
    {
        'l',
        0x0c,
        'L',
        0x0c,
        CLEM_ADB_KEY_MOD_CAPITAL,
    },
    /* 0x26 */
    {
        'j',
        0x0a,
        'J',
        0x0a,
        CLEM_ADB_KEY_MOD_CAPITAL,
    },
    /* 0x27 */
    {
        0x27,
        0xff,
        0x22,
        0xff,
        0x00,
    }, /* apostrophe */
       /* 0x28 */
    {
        'k',
        0x0b,
        'K',
        0x0b,
        CLEM_ADB_KEY_MOD_CAPITAL,
    },
    /* 0x29 */
    {
        ';',
        ';',
        ':',
        ':',
        0x00,
    },
    /* 0x2A */
    {
        '\\',
        0x1c,
        '|',
        0x1c,
        0x00,
    },
    /* 0x2B */
    {
        ',',
        ',',
        '<',
        '<',
        0x00,
    },
    /* 0x2C */
    {
        '/',
        '/',
        '?',
        '?',
        0x00,
    },
    /* 0x2D */
    {
        'n',
        0x0e,
        'N',
        0x0e,
        CLEM_ADB_KEY_MOD_CAPITAL,
    },
    /* 0x2E */
    {
        'm',
        0x0d,
        'M',
        0x0d,
        CLEM_ADB_KEY_MOD_CAPITAL,
    },
    /* 0x2F */
    {
        '.',
        '.',
        '>',
        '>',
        0x00,
    },
    /* 0x30 */
    {
        0x09,
        0x09,
        0x09,
        0x09,
        0x00,
    }, /* TAB */
       /* 0x31 */
    {
        0x20,
        0x20,
        0x20,
        0x20,
        0x00,
    }, /* SPACE */
       /* 0x32 */
    {
        '`',
        '`',
        '~',
        '~',
        0x00,
    },
    /* 0x33 */
    {
        0x7f,
        0x7f,
        0x7f,
        0x7f,
        0x00,
    }, /* DELETE */
       /* 0x34 */
    {
        0xff,
        0xff,
        0xff,
        0xff,
        0x00,
    },
    /* 0x35 */
    {
        0x1b,
        0x1b,
        0x1b,
        0x1b,
        0x00,
    }, /* ESCAPE */
       /* 0x36 */
    {
        0xff,
        0xff,
        0xff,
        0xff,
        CLEM_ADB_KEY_MOD_CTRL,
    },
    /* 0x37 */
    {
        0xff,
        0xff,
        0xff,
        0xff,
        CLEM_ADB_KEY_MOD_APPLE,
    },
    /* 0x38 */
    {
        0xff,
        0xff,
        0xff,
        0xff,
        CLEM_ADB_KEY_MOD_SHIFT,
    },
    /* 0x39 */
    {
        0xff,
        0xff,
        0xff,
        0xff,
        CLEM_ADB_KEY_MOD_CAPS,
    },
    /* 0x3A */
    {
        0xff,
        0xff,
        0xff,
        0xff,
        CLEM_ADB_KEY_MOD_OPTION,
    },
    /* 0x3B */
    {
        0x08,
        0x08,
        0x08,
        0x08,
        0x00,
    }, /* LEFT */
       /* 0x3C */
    {
        0x15,
        0x15,
        0x15,
        0x15,
        0x00,
    }, /* RIGHT */
       /* 0x3D */
    {
        0x0a,
        0x0a,
        0x0a,
        0x0a,
        0x00,
    }, /* DOWN */
       /* 0x3E */
    {
        0x0b,
        0x0b,
        0x0b,
        0x0b,
        0x00,
    }, /* UP */
       /* 0x3F */
    {
        0xff,
        0xff,
        0xff,
        0xff,
        0x00,
    },
    /* 0x40 */
    {
        0xff,
        0xff,
        0xff,
        0xff,
        0x00,
    },
    /* 0x41 */
    {
        '.',
        '.',
        '.',
        '.',
        CLEM_ADB_KEY_MOD_KEYPAD,
    },
    /* 0x42 */
    {
        0xff,
        0xff,
        0xff,
        0xff,
        0x00,
    },
    /* 0x43 */
    {
        '*',
        '*',
        '*',
        '*',
        CLEM_ADB_KEY_MOD_KEYPAD,
    },
    /* 0x44 */
    {
        0xff,
        0xff,
        0xff,
        0xff,
        0x00,
    },
    /* 0x45 */
    {
        '+',
        '+',
        '+',
        '+',
        CLEM_ADB_KEY_MOD_KEYPAD,
    },
    /* 0x46 */
    {
        0xff,
        0xff,
        0xff,
        0xff,
        0x00,
    },
    /* 0x47 */
    {
        0x18,
        0x18,
        0x18,
        0x18,
        CLEM_ADB_KEY_MOD_KEYPAD,
    },
    /* 0x48 */
    {
        0xff,
        0xff,
        0xff,
        0xff,
        0x00,
    },
    /* 0x49 */
    {
        0xff,
        0xff,
        0xff,
        0xff,
        0x00,
    },
    /* 0x4A */
    {
        0xff,
        0xff,
        0xff,
        0xff,
        0x00,
    },
    /* 0x4B */
    {
        '/',
        '/',
        '/',
        '/',
        CLEM_ADB_KEY_MOD_KEYPAD,
    },
    /* 0x4C */
    {
        0x0d,
        0x0d,
        0x0d,
        0x0d,
        CLEM_ADB_KEY_MOD_KEYPAD,
    },
    /* 0x4D */
    {
        0xff,
        0xff,
        0xff,
        0xff,
        0x00,
    },
    /* 0x4E */
    {
        '-',
        '-',
        '-',
        '-',
        CLEM_ADB_KEY_MOD_KEYPAD,
    },
    /* 0x4F */
    {
        0xff,
        0xff,
        0xff,
        0xff,
        0x00,
    },
    /* 0x50 */
    {
        0xff,
        0xff,
        0xff,
        0xff,
        0x00,
    },
    /* 0x51 */
    {
        '=',
        '=',
        '=',
        '=',
        CLEM_ADB_KEY_MOD_KEYPAD,
    },
    /* 0x52 */
    {
        '0',
        '0',
        '0',
        '0',
        CLEM_ADB_KEY_MOD_KEYPAD,
    },
    /* 0x53 */
    {
        '1',
        '1',
        '1',
        '1',
        CLEM_ADB_KEY_MOD_KEYPAD,
    },
    /* 0x54 */
    {
        '2',
        '2',
        '2',
        '2',
        CLEM_ADB_KEY_MOD_KEYPAD,
    },
    /* 0x55 */
    {
        '3',
        '3',
        '3',
        '3',
        CLEM_ADB_KEY_MOD_KEYPAD,
    },
    /* 0x56 */
    {
        '4',
        '4',
        '4',
        '4',
        CLEM_ADB_KEY_MOD_KEYPAD,
    },
    /* 0x57 */
    {
        '5',
        '5',
        '5',
        '5',
        CLEM_ADB_KEY_MOD_KEYPAD,
    },
    /* 0x58 */
    {
        '6',
        '6',
        '6',
        '6',
        CLEM_ADB_KEY_MOD_KEYPAD,
    },
    /* 0x59 */
    {
        '7',
        '7',
        '7',
        '7',
        CLEM_ADB_KEY_MOD_KEYPAD,
    },
    /* 0x5A */
    {
        0xff,
        0xff,
        0xff,
        0xff,
        0x00,
    },
    /* 0x5B */
    {
        '8',
        '8',
        '8',
        '8',
        CLEM_ADB_KEY_MOD_KEYPAD,
    },
    /* 0x5C */
    {
        '9',
        '9',
        '9',
        '9',
        CLEM_ADB_KEY_MOD_KEYPAD,
    },
    /* 0x5D */
    {
        0xff,
        0xff,
        0xff,
        0xff,
        0x00,
    },
    /* 0x5E */
    {
        0xff,
        0xff,
        0xff,
        0xff,
        0x00,
    },
    /* 0x5F */
    {
        0xff,
        0xff,
        0xff,
        0xff,
        0x00,
    },
    /* 0x60 */
    {
        0xff,
        0xff,
        0xff,
        0xff,
        0x00,
    },
    /* 0x61 */
    {
        0xff,
        0xff,
        0xff,
        0xff,
        0x00,
    },
    /* 0x62 */
    {
        0xff,
        0xff,
        0xff,
        0xff,
        0x00,
    },
    /* 0x63 */
    {
        0xff,
        0xff,
        0xff,
        0xff,
        0x00,
    },
    /* 0x64 */
    {
        0xff,
        0xff,
        0xff,
        0xff,
        0x00,
    },
    /* 0x65 */
    {
        0xff,
        0xff,
        0xff,
        0xff,
        0x00,
    },
    /* 0x66 */
    {
        0xff,
        0xff,
        0xff,
        0xff,
        0x00,
    },
    /* 0x67 */
    {
        0xff,
        0xff,
        0xff,
        0xff,
        0x00,
    },
    /* 0x68 */
    {
        0xff,
        0xff,
        0xff,
        0xff,
        0x00,
    },
    /* 0x69 */
    {
        0xff,
        0xff,
        0xff,
        0xff,
        0x00,
    },
    /* 0x6A */
    {
        0xff,
        0xff,
        0xff,
        0xff,
        0x00,
    },
    /* 0x6B */
    {
        0xff,
        0xff,
        0xff,
        0xff,
        0x00,
    },
    /* 0x6C */
    {
        0xff,
        0xff,
        0xff,
        0xff,
        0x00,
    },
    /* 0x6D */
    {
        0xff,
        0xff,
        0xff,
        0xff,
        0x00,
    },
    /* 0x6E */
    {
        0xff,
        0xff,
        0xff,
        0xff,
        0x00,
    },
    /* 0x6F */
    {
        0xff,
        0xff,
        0xff,
        0xff,
        0x00,
    },
    /* 0x70 */
    {
        0xff,
        0xff,
        0xff,
        0xff,
        0x00,
    },
    /* 0x71 */
    {
        0xff,
        0xff,
        0xff,
        0xff,
        0x00,
    },
    /* 0x72 */
    {
        0xff,
        0xff,
        0xff,
        0xff,
        0x00,
    },
    /* 0x73 */
    {
        0xff,
        0xff,
        0xff,
        0xff,
        0x00,
    },
    /* 0x74 */
    {
        0xff,
        0xff,
        0xff,
        0xff,
        0x00,
    },
    /* 0x75 */
    {
        0xff,
        0xff,
        0xff,
        0xff,
        0x00,
    },
    /* 0x76 */
    {
        0xff,
        0xff,
        0xff,
        0xff,
        0x00,
    },
    /* 0x77 */
    {
        0xff,
        0xff,
        0xff,
        0xff,
        0x00,
    },
    /* 0x78 */
    {
        0xff,
        0xff,
        0xff,
        0xff,
        0x00,
    },
    /* 0x79 */
    {
        0xff,
        0xff,
        0xff,
        0xff,
        0x00,
    },
    /* 0x7A */
    {
        0xff,
        0xff,
        0xff,
        0xff,
        0x00,
    },
    /* 0x7B */
    {
        0xff,
        0xff,
        0xff,
        0xff,
        CLEM_ADB_KEY_MOD_SHIFT,
    },
    /* 0x7C */
    {
        0xff,
        0xff,
        0xff,
        0xff,
        CLEM_ADB_KEY_MOD_OPTION,
    },
    /* 0x7D */
    {
        0xff,
        0xff,
        0xff,
        0xff,
        CLEM_ADB_KEY_MOD_CTRL,
    },
    /* 0x7E */
    {
        0xff,
        0xff,
        0xff,
        0xff,
        0x00,
    },
    /* 0x7F */
    {
        0xff,
        0xff,
        0xff,
        0xff,
        0x00,
    }};

static int g_key_delay_ms[8] = {250, 500, 750, 1000, 0, 0, 0, 0};

static int g_key_rate_per_sec[8] = {0, 30, 24, 20, 15, 11, 8, 4};

static inline void _clem_adb_glu_queue_key(struct ClemensDeviceADB *adb, uint8_t key) {
    if (adb->keyb.size >= CLEM_ADB_KEYB_BUFFER_LIMIT) {
        return;
    }
    adb->keyb.keys[adb->keyb.size++] = key;
}

static uint8_t _clem_adb_glu_unqueue_key(struct ClemensDeviceADB *adb) {
    uint8_t i;
    uint8_t key;
    CLEM_ASSERT(adb->keyb.size > 0);
    key = adb->keyb.keys[0];
    if (adb->keyb.size > 0) {
        --adb->keyb.size;
        for (i = 0; i < adb->keyb.size; ++i) {
            adb->keyb.keys[i] = adb->keyb.keys[i + 1];
        }
    }
    return key;
}

static uint8_t _clem_adb_glu_keyb_parse(struct ClemensDeviceADB *adb, uint8_t key_event) {
    uint8_t key_index = key_event & 0x7f;
    bool is_key_down = (key_event & 0x80) == 0; /* up = b7 at this point */
    uint8_t ascii_key;

    uint8_t *ascii_table = clem_adb_ascii_from_a2code(key_index);
    uint16_t modifiers = adb->keyb_reg[2] & CLEM_ADB_GLU_REG2_MODKEY_MASK;
    uint16_t old_modifiers = modifiers;

    adb->is_keypad_down = ascii_table[4] == CLEM_ADB_KEY_MOD_KEYPAD && is_key_down;
    if (ascii_table[4]) { // key is a modifier?
        if (ascii_table[4] == CLEM_ADB_KEY_MOD_APPLE) {
            if (is_key_down)
                modifiers |= CLEM_ADB_GLU_REG2_KEY_APPLE;
            else
                modifiers &= ~CLEM_ADB_GLU_REG2_KEY_APPLE;
        } else if (ascii_table[4] == CLEM_ADB_KEY_MOD_OPTION) {
            if (is_key_down)
                modifiers |= CLEM_ADB_GLU_REG2_KEY_OPTION;
            else
                modifiers &= ~CLEM_ADB_GLU_REG2_KEY_OPTION;
        } else if (ascii_table[4] == CLEM_ADB_KEY_MOD_SHIFT) {
            if (is_key_down)
                modifiers |= CLEM_ADB_GLU_REG2_KEY_SHIFT;
            else
                modifiers &= ~CLEM_ADB_GLU_REG2_KEY_SHIFT;
        } else if (ascii_table[4] == CLEM_ADB_KEY_MOD_CTRL) {
            if (is_key_down)
                modifiers |= CLEM_ADB_GLU_REG2_KEY_CTRL;
            else
                modifiers &= ~CLEM_ADB_GLU_REG2_KEY_CTRL;
        } else if (ascii_table[4] == CLEM_ADB_KEY_MOD_CAPS) {
            if (is_key_down)
                modifiers |= CLEM_ADB_GLU_REG2_KEY_CAPS;
            else
                modifiers &= ~CLEM_ADB_GLU_REG2_KEY_CAPS;
        }
        adb->keyb_reg[2] &= ~CLEM_ADB_GLU_REG2_MODKEY_MASK;
        adb->keyb_reg[2] |= modifiers;
    }
    if (ascii_table[0] != 0xff) {
        //  this is a repeatable key - reset repeat key state here
        if (is_key_down) {
            if (key_index != adb->keyb.last_a2_key_down) {
                adb->keyb.timer_us = 0;
                adb->keyb.repeat_count = 0;
                adb->keyb.last_a2_key_down = key_index;
            }
        } else if (key_index == adb->keyb.last_a2_key_down) {
            adb->keyb.last_a2_key_down = 0;
        }
    }
    /* Additional parsing needed for MMIO registers */
    if (modifiers & CLEM_ADB_GLU_REG2_KEY_SHIFT) {
        if (modifiers & CLEM_ADB_GLU_REG2_KEY_CTRL) {
            ascii_key = ascii_table[3];
        } else {
            ascii_key = ascii_table[2];
        }
    } else if (modifiers & CLEM_ADB_GLU_REG2_KEY_CTRL) {
        ascii_key = ascii_table[1];
    } else {
        if (ascii_table[4] == CLEM_ADB_KEY_MOD_CAPITAL &&
            (adb->keyb_reg[2] & CLEM_ADB_GLU_REG2_KEY_CAPS_TOGGLE)) {
            ascii_key = ascii_table[2];
        } else {
            ascii_key = ascii_table[0];
        }
    }

    //  special key combos that are detected by reading c026 - inform interrupt
    //  handler that this has occurred.
    if (is_key_down && key_index == CLEM_ADB_KEY_ESCAPE) {
        if ((modifiers & CLEM_ADB_GLU_REG2_KEY_CTRL) && (modifiers & CLEM_ADB_GLU_REG2_KEY_APPLE)) {
            adb->cmd_flags |= CLEM_ADB_C026_DESK_MGR;
            _clem_adb_irq_dispatch(adb, CLEM_IRQ_ADB_DATA);
        }
    }

    if (ascii_key != 0xff) {
        // CLEM_LOG("SKS: ascii: %02X", ascii_key);
        if (is_key_down) {
            adb->io_key_last_ascii = 0x80 | ascii_key;
            /* via HWRef, but FWRef contradicts? */
            adb->cmd_status |= CLEM_ADB_C027_KEY_FULL;
            adb->is_asciikey_down = true;
        } else {
            adb->is_asciikey_down = false;
        }
    }

    /* FIXME: sketchy - is this doing what a  modifier key latch does? */
    if ((modifiers ^ old_modifiers) && !adb->is_asciikey_down) {
        adb->has_modkey_changed = true;
    }

    return key_event;
}

static void _clem_adb_glu_keyb_talk(struct ClemensDeviceADB *adb) {
    uint8_t *ascii_table;
    uint8_t key_event;
    bool is_key_down;

    //  handle repeat logic here so that we can queue repeated keys before
    //      consuming them

    if (adb->keyb.last_a2_key_down && adb->keyb.delay_ms && adb->keyb.rate_per_sec) {
        int timer_ms = adb->keyb.timer_us / 1000;
        if (adb->keyb.repeat_count == 0) {
            if (timer_ms >= adb->keyb.delay_ms) {
                _clem_adb_glu_queue_key(adb, adb->keyb.last_a2_key_down);
                ++adb->keyb.repeat_count;
                adb->keyb.timer_us = 0;
            }
        } else {
            if (timer_ms >= (1000 / adb->keyb.rate_per_sec)) {
                _clem_adb_glu_queue_key(adb, adb->keyb.last_a2_key_down);
                ++adb->keyb.repeat_count;
                adb->keyb.timer_us = 0;
            }
        }
    }

    //  TODO: investigate if the logic below is wiping out key events for quick
    //        taps
    //  i.e. does C000 and C010 have a valid state after a quick up, down
    //  tap?  may need to add logging and debugging

    if (adb->keyb.size <= 0)
        return;
    key_event = _clem_adb_glu_unqueue_key(adb);

    /* The reset key is special - it takes up the whole register, and so
       for the first unqueue, only allow one read from the key queue
       See https://developer.apple.com/library/archive/technotes/hw/hw_01.html
       for the behavior behind the reset key
    */
    if ((key_event & 0x7f) == CLEM_ADB_KEY_RESET) {
        if (key_event & 0x80) {
            adb->keyb_reg[0] = 0x7f7f;
            adb->keyb_reg[2] &= ~CLEM_ADB_GLU_REG2_KEY_RESET;
            if (adb->keyb_reg[2] & CLEM_ADB_GLU_REG2_KEY_CTRL) {
                adb->keyb.reset_key = true;
            }
        } else {
            adb->keyb_reg[2] |= CLEM_ADB_GLU_REG2_KEY_RESET;
            adb->keyb_reg[0] = 0xffff;
        }
    } else {
        adb->keyb_reg[0] = _clem_adb_glu_keyb_parse(adb, key_event);
        if (adb->keyb.size > 0 && adb->keyb.keys[0] != CLEM_ADB_KEY_RESET) {
            //  second key input
            adb->keyb_reg[0] <<= 8;
            key_event = _clem_adb_glu_unqueue_key(adb);
            adb->keyb_reg[0] |= _clem_adb_glu_keyb_parse(adb, key_event);
        }
    }
}

/*
 * Mouse Support
 *
 * Autopolling: at regular intervals, look at registers and update
 *
 * From Emulator Host (OS) to Emulator Device
 *  - Input from the emulator app is converted to simple events representing
 *    an ADB device.
 *
 * GLU listen/talk
 *  - during sync(),
 *      autopoll ready? then acquire data from the device, clear srq
 *      if autopoll off, rely on manual talk commands, clear srq
 *      either way, update MMIO registers
 *
 * References:
 *  Apple IIgs Hardware Reference
 *  https://developer.apple.com/library/archive/technotes/hw/hw_01.html
 */
static void _clem_adb_glu_queue_mouse(struct ClemensDeviceADB *adb, int16_t dx, int16_t dy) {
    /* queue a mouse data register formatted to specs in Table 6-7 HW Ref */
    unsigned mouse = CLEM_ADB_GLU_REG0_MOUSE_ALWAYS_1;
    if (adb->mouse.size >= CLEM_ADB_KEYB_BUFFER_LIMIT) {
        return;
    }

    /*  Conversion to signed 7-bit values with limits +-63 */
    if (dy < -63) {
        dy = -63;
    } else if (dy > 63) {
        dy = 63;
    }
    mouse |= ((uint16_t)(dy & 0x7f) << 8);
    if (dx < -63) {
        dx = -63;
    } else if (dx > 63) {
        dx = 63;
    }
    mouse |= (dx & 0x7f);

    if (adb->mouse.btn_down)
        mouse &= ~CLEM_ADB_GLU_REG0_MOUSE_BTN;
    else
        mouse |= CLEM_ADB_GLU_REG0_MOUSE_BTN;

    adb->mouse.pos[adb->mouse.size++] = mouse;
}

static unsigned _clem_adb_glu_unqueue_mouse(struct ClemensDeviceADB *adb) {
    unsigned i;
    unsigned mouse;
    CLEM_ASSERT(adb->mouse.size > 0);
    mouse = adb->mouse.pos[0];
    if (adb->mouse.size > 0) {
        --adb->mouse.size;
        for (i = 0; i < adb->mouse.size; ++i) {
            adb->mouse.pos[i] = adb->mouse.pos[i + 1];
        }
    }
    return mouse;
}

/* TODO: these should be a part of a 'ROM introspection' utility */

/* Mouse tracking assumes that certain ROM states are set before operating.
   See the above TODO for how to improve upon this approach - as these values
   are initialized by Toolbox code.

   An approach that works and is used by other emulators involves verifying
   that the Event Manager is initialized which assumes a desktop with cursor (or
   a game that uses the Toolbox for mouse input.)

   Since the clamp values and cursor positions are set by Toolbox calls,
   it's possible that titles leverage SetClamp/ReadMouse/ReadMouse2 (beyond the
   ROM's IRQ handler) without invoking the EventManager.   This solution will
   try to account for these titles as well.

   So - if the clamp values seem valid (x0,y0 >= 0 and x1,y1 < 1024 and > x0,y0)
   we'll assume they were set by calls to the Toolbox.

   TODO: Set positions/clamps in Apple 2/Slot 4 Mouse Firmware screen holes
*/
#define CLEM_ADB_MOUSE_IIGS_ROM_XL 0x190
#define CLEM_ADB_MOUSE_IIGS_ROM_XH 0x192
#define CLEM_ADB_MOUSE_IIGS_ROM_YL 0x191
#define CLEM_ADB_MOUSE_IIGS_ROM_YH 0x193

#define CLEM_ADB_MOUSE_IIGS_ROM_CLAMP_X0 0x2b8
#define CLEM_ADB_MOUSE_IIGS_ROM_CLAMP_Y0 0x2ba
#define CLEM_ADB_MOUSE_IIGS_ROM_CLAMP_X1 0x2bc
#define CLEM_ADB_MOUSE_IIGS_ROM_CLAMP_Y1 0x2be

static void _clem_adb_mouse_check_clamping(struct ClemensDeviceADB *adb, uint8_t *e1_bank) {
    uint16_t x0 = ((uint16_t)(e1_bank[CLEM_ADB_MOUSE_IIGS_ROM_CLAMP_X0 + 1]) << 16) |
                  e1_bank[CLEM_ADB_MOUSE_IIGS_ROM_CLAMP_X0];
    uint16_t y0 = ((uint16_t)(e1_bank[CLEM_ADB_MOUSE_IIGS_ROM_CLAMP_Y0 + 1]) << 16) |
                  e1_bank[CLEM_ADB_MOUSE_IIGS_ROM_CLAMP_Y0];
    uint16_t x1 = ((uint16_t)(e1_bank[CLEM_ADB_MOUSE_IIGS_ROM_CLAMP_X1 + 1]) << 16) |
                  e1_bank[CLEM_ADB_MOUSE_IIGS_ROM_CLAMP_X1];
    uint16_t y1 = ((uint16_t)(e1_bank[CLEM_ADB_MOUSE_IIGS_ROM_CLAMP_Y1 + 1]) << 16) |
                  e1_bank[CLEM_ADB_MOUSE_IIGS_ROM_CLAMP_Y1];

    adb->mouse.valid_clamp_box = y0 < y1 && x0 < x1 && y1 < 0x400 && x1 < 0x400;
    adb->mouse.tracking_enabled = adb->mouse.valid_clamp_box;
}

static void _clem_adb_glu_queue_tracked_mouse(struct ClemensDeviceADB *adb, int16_t mx,
                                              int16_t my) {
    /* This event isn't queued - but instead state variables for tracking are set
       here.  Deltas are calculated on demand on reads to $c024.
    */
    adb->mouse.mx = mx;
    adb->mouse.my = my;
    if (!adb->mouse.tracking_enabled && adb->mouse.valid_clamp_box) {
        /* need an initial position if we're starting to track */
        adb->mouse.mx0 = adb->mouse.mx;
        adb->mouse.mx0 = adb->mouse.my;
        adb->mouse.tracking_enabled = true;
    }
}

static void _clem_adb_glu_mouse_tracking(struct ClemensDeviceADB *adb,
                                         struct ClemensDeviceMega2Memory *m2mem) {
    //  IIGS firmware only
    //  alternate readying X and Y based on the current status flags
    //  if mouse.tracking_enabled we calculate deltas here based on current and previous
    //  mouse positions.  note if the deltas are > than abs(63), then the delta is 0
    //
    //  This relies on ROM code that calls ReadMouse() and memory locations that
    //  will contain the current mouse x and y.
    //
    //  The deltas are then calculated and returned here.  The ROM code will
    //  perform the translation. (i.e. x + dx = x')
    //  If delta is 0, then set current mouse position to the next position
    //
    //  NOTE: This does not take Apple II mouse calls into account.  That
    //  logic should occur in a different location.

    int16_t delta_x;
    int16_t delta_y;

    _clem_adb_mouse_check_clamping(adb, m2mem->e1_bank);
    if (!adb->mouse.tracking_enabled)
        return;

    delta_x = adb->mouse.mx - adb->mouse.mx0;
    delta_y = adb->mouse.my - adb->mouse.my0;
    //  TODO: must account for screen mode (320 vs 640)
    //        maybe this occurs on the host side which will translate coordinates.

    if (delta_x > 63 || delta_x < -63) {
        m2mem->e1_bank[CLEM_ADB_MOUSE_IIGS_ROM_XL] = (uint8_t)(adb->mouse.mx & 0xff);
        m2mem->e1_bank[CLEM_ADB_MOUSE_IIGS_ROM_XH] = (uint8_t)(adb->mouse.mx >> 8);
        delta_x = 0;
    } else {
        m2mem->e1_bank[CLEM_ADB_MOUSE_IIGS_ROM_XL] = (uint8_t)(adb->mouse.mx0 & 0xff);
        m2mem->e1_bank[CLEM_ADB_MOUSE_IIGS_ROM_XH] = (uint8_t)(adb->mouse.mx0 >> 8);
    }
    if (delta_y > 63 || delta_y < -63) {
        m2mem->e1_bank[CLEM_ADB_MOUSE_IIGS_ROM_YL] = (uint8_t)(adb->mouse.my & 0xff);
        m2mem->e1_bank[CLEM_ADB_MOUSE_IIGS_ROM_YH] = (uint8_t)(adb->mouse.my >> 8);
        delta_y = 0;
    } else {
        m2mem->e1_bank[CLEM_ADB_MOUSE_IIGS_ROM_YL] = (uint8_t)(adb->mouse.my0 & 0xff);
        m2mem->e1_bank[CLEM_ADB_MOUSE_IIGS_ROM_YH] = (uint8_t)(adb->mouse.my0 >> 8);
    }
    if (delta_x != 0 || delta_y != 0) {
        _clem_adb_glu_queue_mouse(adb, delta_x, delta_y);
    }
    adb->mouse.mx0 = adb->mouse.mx;
    adb->mouse.my0 = adb->mouse.my;
}

static void _clem_adb_glu_mouse_talk(struct ClemensDeviceADB *adb) {
    //  populate our mouse data register - this will pull all events from the
    //  queue, compressing multiple events over the frame into a single event
    //  to be saved onto the data register.
    //  if mouse interrupts are enabled *and* a valid mouse event is avaiable,
    //  then issue the IRQ (CLEM_IRQ_ADB_MOUSE_EVT)
    uint16_t mouse_reg;

    //  this approach will result in lost events if they are not consumed
    //  fast enough.  reevaluate
    if (adb->mouse.size <= 0) {
        //  TODO: what if autopoll is disabled?
        _clem_adb_glu_queue_mouse(adb, 0, 0);
    }
    mouse_reg = _clem_adb_glu_unqueue_mouse(adb);
    //  do not populate the data register until our client has had some time
    //  to read in the X,Y.
    if (adb->cmd_status & CLEM_ADB_C027_MOUSE_FULL)
        return;

    adb->mouse_reg[0] = mouse_reg;

    adb->cmd_status |= CLEM_ADB_C027_MOUSE_FULL;

    _clem_adb_irq_dispatch(adb, CLEM_IRQ_ADB_MOUSE_EVT);
}

static void _clem_adb_glu_set_mode_flags(struct ClemensDeviceADB *adb, unsigned mode_flags) {
    if (mode_flags & 0x01) {
        adb->mode_flags &= ~CLEM_ADB_MODE_AUTOPOLL_KEYB;
        adb->keyb_reg[0] = 0x0000;
        adb->cmd_status &= ~CLEM_ADB_C027_KEY_FULL;
        CLEM_LOG("ADB: Disable Keyboard Autopoll");
    }
    if (mode_flags & 0x02) {
        adb->mode_flags &= ~CLEM_ADB_MODE_AUTOPOLL_MOUSE;
        adb->mouse_reg[0] = 0x0000;
        adb->cmd_status &= ~CLEM_ADB_C027_MOUSE_FULL;
        CLEM_LOG("ADB: Disable Mouse Autopoll");
    }
    if (mode_flags & 0x000000fc) {
        CLEM_WARN("ADB: SetMode %02X Unimplemented", mode_flags & 0x000000fc);
    }
}

static void _clem_adb_glu_clear_mode_flags(struct ClemensDeviceADB *adb, unsigned mode_flags) {
    if (mode_flags & 0x01) {
        if (!(adb->mode_flags & CLEM_ADB_MODE_AUTOPOLL_KEYB)) {
            adb->mode_flags |= CLEM_ADB_MODE_AUTOPOLL_KEYB;
            CLEM_LOG("ADB: Enable Keyboard Autopoll");
        }
    }
    if (mode_flags & 0x02) {
        if (!(adb->mode_flags & CLEM_ADB_MODE_AUTOPOLL_MOUSE)) {
            adb->mode_flags |= CLEM_ADB_MODE_AUTOPOLL_MOUSE;
            CLEM_LOG("ADB: Enable Mouse Autopoll");
        }
    }
    if (mode_flags & 0x000000fc) {
        CLEM_WARN("ADB: ClearMode %02X Unimplemented", mode_flags & 0x000000fc);
    }
}

static void _clem_adb_glu_set_config(struct ClemensDeviceADB *adb, uint8_t keyb_mouse_adr,
                                     uint8_t keyb_setup, uint8_t keyb_repeat) {
    /* unsure what if anything there's to do for setting the adb addresses */
    CLEM_DEBUG("ADB: setting keyb and mouse addr to %02X", keyb_mouse_adr);
    /* TODO: language settings for keyboard */
    CLEM_DEBUG("ADB: setting keyb language character set to %02X", (keyb_setup & 0xf0) >> 4);
    CLEM_DEBUG("ADB: setting keyb language layout to %02X", (keyb_setup & 0x0f));

    adb->keyb.delay_ms = g_key_delay_ms[(keyb_repeat & 0x70) >> 4];
    adb->keyb.rate_per_sec = g_key_rate_per_sec[keyb_repeat & 0x7];

    CLEM_DEBUG("ADB: setting keyb event delay/repeat to %d ms/%d per sec", adb->keyb.delay_ms,
               adb->keyb.rate_per_sec);

    CLEM_WARN("Partially implemented ADB GLU Set Config");
}

static void _clem_adb_glu_enable_srq(struct ClemensDeviceADB *adb, unsigned device_address,
                                     bool enable) {
    switch (device_address) {
    case CLEM_ADB_DEVICE_KEYBOARD:
        if (enable) {
            adb->keyb_reg[3] |= CLEM_ADB_GLU_REG3_MASK_SRQ;
        } else {
            adb->keyb_reg[3] &= ~CLEM_ADB_GLU_REG3_MASK_SRQ;
            adb->irq_line &= ~CLEM_IRQ_ADB_KEYB_SRQ;
        }
        break;

    case CLEM_ADB_DEVICE_MOUSE:
        if (enable) {
            adb->mouse_reg[3] |= CLEM_ADB_GLU_REG3_MASK_SRQ;
        } else {
            adb->mouse_reg[3] &= ~CLEM_ADB_GLU_REG3_MASK_SRQ;
            adb->irq_line &= ~CLEM_IRQ_ADB_MOUSE_SRQ;
        }
        break;
    default:
        CLEM_WARN("ADB Device Address %u unsupported", device_address);
        break;
    }
}

static uint8_t _clem_adb_glu_read_memory(struct ClemensDeviceADB *adb, uint8_t address,
                                         uint8_t page) {
    uint8_t result;
    /* ADB docs imply only 96 bytes of RAM and about 3-4K ROM.  Consider these
       values when debugging
    */
    if (page == 0x00) {
        /* Its likely some RAM addresses map to GLU register contents.
          Reference https://llx.com/Neil/a2/adb.html
          Try out practically with software
        */
        result = adb->ram[address];
        switch (address) {
        case 0xe2:
            /* No //e keyboard support hardcode results bits 1 and 2 = 1*/
            result = 0x06;
            break;
        case 0xe8:
            /* Support just the apple keys */
            if (adb->keyb_reg[2] & CLEM_ADB_GLU_REG2_KEY_APPLE) {
                result = 0x20;
            } else {
                result = 0x00;
            }
            if (adb->keyb_reg[2] & CLEM_ADB_GLU_REG2_KEY_OPTION) {
                result |= 0x10;
            } else {
                result &= ~0x10;
            }
            break;
        default:
            result = adb->ram[address];
            break;
        }
    } else {
        result = 0x00;
    }
    return result;
}

void _clem_adb_glu_set_register(struct ClemensDeviceADB *adb, unsigned device_register,
                                unsigned address, uint8_t hi, uint8_t lo) {
    switch (address) {
    case CLEM_ADB_DEVICE_KEYBOARD:
        if (device_register == 0x3) {
            if ((hi & 0x0f) != CLEM_ADB_DEVICE_KEYBOARD) {
                /* changing to a device address other than what's standard? */
                CLEM_WARN("ADB: change keyboard device address to not "
                          "0x03: %0X",
                          hi);
            }
            CLEM_DEBUG("ADB: keyb device handler to %0X", lo);
        }
        adb->keyb_reg[device_register % 4] = ((unsigned)hi << 8) | lo;
        break;
    case CLEM_ADB_DEVICE_MOUSE:
        if (device_register == 0x3) {
            if ((hi & 0x0f) != CLEM_ADB_DEVICE_MOUSE) {
                /* changing to a device address other than what's standard? */
                CLEM_WARN("ADB: attempt to change mouse device to "
                          "address: %0X",
                          hi);
            }
            CLEM_DEBUG("ADB: mouse device handler to %0X", lo);
        }
        adb->mouse_reg[device_register % 4] = ((unsigned)hi << 8) | lo;
        break;
    default:
        CLEM_WARN("ADB: set device register unsupported: %0X", address);
        break;
    }
}

void _clem_adb_glu_command(struct ClemensDeviceADB *adb) {
    unsigned device_command = 0;
    unsigned device_address = 0;
    uint8_t data[8];

    switch (adb->cmd_reg) {
    case CLEM_ADB_CMD_ABORT:
        CLEM_DEBUG("ADB: ABORT");
        _clem_adb_glu_command_done(adb);
        return;
    case CLEM_ADB_CMD_SET_MODES:
        CLEM_DEBUG("ADB: SET_MODES %02X", adb->cmd_data[0]);
        _clem_adb_glu_set_mode_flags(adb, adb->cmd_data[0]);
        _clem_adb_glu_command_done(adb);
        break;
    case CLEM_ADB_CMD_CLEAR_MODES:
        CLEM_DEBUG("ADB: CLEAR_MODES %02X", adb->cmd_data[0]);
        _clem_adb_glu_clear_mode_flags(adb, adb->cmd_data[0]);
        _clem_adb_glu_command_done(adb);
        break;
    case CLEM_ADB_CMD_SET_CONFIG:
        CLEM_DEBUG("ADB: CONFIG: %02X %02X %02X", adb->cmd_data[0], adb->cmd_data[1],
                   adb->cmd_data[2]);
        _clem_adb_glu_set_config(adb, adb->cmd_data[0], adb->cmd_data[1], adb->cmd_data[2]);
        _clem_adb_glu_command_done(adb);
        return;
    case CLEM_ADB_CMD_SYNC:
        CLEM_DEBUG("ADB: SYNC: %02X %02X %02X %02X", adb->cmd_data[0], adb->cmd_data[1],
                   adb->cmd_data[2], adb->cmd_data[3]);
        CLEM_DEBUG("ADB: SYNC ROM03: %02X %02X %02X %02X", adb->cmd_data[4], adb->cmd_data[5],
                   adb->cmd_data[6], adb->cmd_data[7]);
        _clem_adb_glu_set_mode_flags(adb, adb->cmd_data[0]);
        _clem_adb_glu_set_config(adb, adb->cmd_data[0], adb->cmd_data[1], adb->cmd_data[2]);
        _clem_adb_glu_command_done(adb);
        return;
    case CLEM_ADB_CMD_WRITE_RAM:
        CLEM_DEBUG("ADB: WRITE RAM: %02X:%02X", adb->cmd_data[0], adb->cmd_data[1]);
        adb->ram[adb->cmd_data[0]] = adb->cmd_data[1];
        _clem_adb_glu_command_done(adb);
        return;
    case CLEM_ADB_CMD_READ_MEM:
        CLEM_DEBUG("ADB: READ RAM: %02X:%02X", adb->cmd_data[0], adb->cmd_data[1]);
        _clem_adb_glu_result_init(adb, 1);
        _clem_adb_glu_result_data(
            adb, _clem_adb_glu_read_memory(adb, adb->cmd_data[0], adb->cmd_data[1]));
        return;
    case CLEM_ADB_CMD_VERSION:
        CLEM_DEBUG("ADB: GET VERSION (%02X)", adb->version);
        _clem_adb_glu_result_init(adb, 1);
        _clem_adb_glu_result_data(adb, (uint8_t)adb->version);
        return;
    case CLEM_ADB_CMD_UNDOCUMENTED_12:
        CLEM_DEBUG("ADB: UNDOC 12: %02X, %02X", adb->cmd_data[0], adb->cmd_data[1]);
        _clem_adb_glu_command_done(adb);
        break;
    case CLEM_ADB_CMD_UNDOCUMENTED_13:
        CLEM_DEBUG("ADB: UNDOC 13: %02X, %02X", adb->cmd_data[0], adb->cmd_data[1]);
        _clem_adb_glu_command_done(adb);
        break;
    default:
        device_command = adb->cmd_reg & 0xf0;
        device_address = adb->cmd_reg & 0x0f;
        break;
    }

    switch (device_command) {
    case CLEM_ADB_CMD_DEVICE_ENABLE_SRQ:
        CLEM_DEBUG("ADB: ENABLE SRQ: %0X", device_address);
        _clem_adb_glu_enable_srq(adb, device_address, true);
        _clem_adb_glu_device_response(adb, 0);
        break;

    case CLEM_ADB_CMD_DEVICE_FLUSH:
        CLEM_UNIMPLEMENTED("ADB: FLUSH: %0X", device_address);
        _clem_adb_glu_device_response(adb, 0);
        break;

    case CLEM_ADB_CMD_DEVICE_DISABLE_SRQ:
        CLEM_DEBUG("ADB: DISABLE SRQ: %0X", device_address);
        _clem_adb_glu_enable_srq(adb, device_address, false);
        _clem_adb_glu_device_response(adb, 0);
        break;

    case CLEM_ADB_CMD_DEVICE_XMIT_2_R0:
    case CLEM_ADB_CMD_DEVICE_XMIT_2_R1:
    case CLEM_ADB_CMD_DEVICE_XMIT_2_R2:
    case CLEM_ADB_CMD_DEVICE_XMIT_2_R3:
        CLEM_DEBUG("ADB: XMIT2 ADR: %0X", device_address);
        _clem_adb_glu_set_register(adb, (device_command & 0x80) >> 4, device_address,
                                   adb->cmd_data[0], adb->cmd_data[1]);
        _clem_adb_glu_device_response(adb, 0);
        break;

    case CLEM_ADB_CMD_DEVICE_POLL_0:
        CLEM_UNIMPLEMENTED("ADB: Poll 0: %0X", device_address);
        _clem_adb_glu_device_response(adb, 0);
        break;

    case CLEM_ADB_CMD_DEVICE_POLL_1:
        CLEM_UNIMPLEMENTED("ADB: Poll 1: %0X", device_address);
        _clem_adb_glu_device_response(adb, 0);
        break;

    case CLEM_ADB_CMD_DEVICE_POLL_2:
        CLEM_UNIMPLEMENTED("ADB: Poll 2: %0X", device_address);
        _clem_adb_glu_device_response(adb, 0);
        break;

    case CLEM_ADB_CMD_DEVICE_POLL_3:
        CLEM_UNIMPLEMENTED("ADB: Poll 3: %0X", device_address);
        _clem_adb_glu_device_response(adb, 0);
        break;
    }
}

void clem_gameport_sync(struct ClemensDeviceGameport *gameport, struct ClemensClock *clocks) {
    clem_clocks_duration_t dt_clocks = clocks->ts - gameport->ts_last_frame;
    unsigned delta_ns = (unsigned)(clem_calc_ns_step_from_clocks(dt_clocks));
    int paddle_index;
    uint32_t charge_time;
    for (paddle_index = 0; paddle_index < 4; ++paddle_index) {
        //
        if (!gameport->paddle_timer_state[paddle_index])
            continue;
        charge_time = gameport->paddle_timer_ns[paddle_index];
        // if (paddle_index == 0)
        //     printf("SYNC: PDL(%d) TIME0: %u ns\n", paddle_index, charge_time);
        if (!charge_time)
            continue;
        // if (paddle_index == 0)
        //     printf("SYNC: PDL(%d) TIME1: %u ns\n", paddle_index, charge_time);
        charge_time = clem_util_timer_decrement(charge_time, delta_ns);
        if (charge_time == 0) {
            //    printf("SYNC: PDL(%d) STATE %02X = %02X\n", paddle_index,
            //           gameport->paddle_timer_state[paddle_index], 0x00);
            gameport->paddle_timer_state[paddle_index] = 0x00;
        }
        gameport->paddle_timer_ns[paddle_index] = charge_time;
    }

    gameport->ts_last_frame = clocks->ts;
}

void clem_adb_glu_sync(struct ClemensDeviceADB *adb, struct ClemensDeviceMega2Memory *m2mem,
                       uint32_t delta_us) {
    adb->poll_timer_us += delta_us;
    adb->keyb.timer_us += delta_us;

    /*  On poll expiration, update device registers
     */
    while (adb->poll_timer_us >= CLEM_MEGA2_CYCLES_PER_60TH) {
        /* IIgs prohibits the mouse from issuing SRQs for incoming mouse data,
           so we only do this for keyboards at this time.
        */
        if (adb->mode_flags & CLEM_ADB_MODE_AUTOPOLL_MOUSE) {
            /* TODO: when doesn't this happen?  The mouse may be updated otherwise
                     with the current code */
            _clem_adb_glu_mouse_tracking(adb, m2mem);
            _clem_adb_glu_mouse_talk(adb);
        }
        if (adb->mode_flags & CLEM_ADB_MODE_AUTOPOLL_KEYB) {
            _clem_adb_glu_keyb_talk(adb);
        } else if (adb->keyb_reg[3] & CLEM_ADB_GLU_REG3_MASK_SRQ) {
            if (adb->keyb.size > 0) {
                _clem_adb_glu_keyb_talk(adb);
                _clem_adb_irq_dispatch(adb, CLEM_IRQ_ADB_KEYB_SRQ);
                printf("Key SRQ ON\n");
            }
        }
        adb->poll_timer_us -= CLEM_MEGA2_CYCLES_PER_60TH;
    }

    switch (adb->state) {
    case CLEM_ADB_STATE_CMD_DATA:
        /* Consume incoming command data and execute the command once
        the expected data xfer from the host has completed
        */
        if (adb->cmd_data_sent > adb->cmd_data_recv) {
            ++adb->cmd_data_recv;
        }
        if (adb->cmd_data_sent == adb->cmd_data_recv) {
            //  no more data available for the command register
            adb->cmd_status &= ~CLEM_ADB_C027_CMD_FULL;
        }
        if (adb->cmd_data_recv >= adb->cmd_data_limit) {
            _clem_adb_glu_command(adb);
        }
        break;
    default:
        break;
    }

    adb->irq_line |= adb->irq_dispatch;
    adb->irq_dispatch = 0;

    if (adb->irq_line & (CLEM_IRQ_ADB_KEYB_SRQ + CLEM_IRQ_ADB_MOUSE_SRQ)) {
        adb->cmd_flags |= CLEM_ADB_C026_SRQ;
    }
}

void _clem_adb_gameport_paddle(struct ClemensDeviceADB *adb, unsigned paddle_xy_id, int16_t x,
                               int16_t y, uint8_t buttons) {
    unsigned index = paddle_xy_id << 1;
    adb->gameport.paddle[index] = x;
    adb->gameport.paddle[index + 1] = y;
    adb->gameport.btn_mask[paddle_xy_id] = buttons;
    adb->gameport.btn_mask[paddle_xy_id ^ 1] = 0;
}

void _clem_adb_gameport_reset(struct ClemensDeviceADB *adb) {
    //  set up timing logic during sync()
    uint64_t ns;
    int index;

    // translate x and y from the linear 0-1023 range to 150Kohm variable resistor
    // operating on a circuit with a 0.022 microfarad capacitor.  The resulting
    // changing time of the capacitor is the time it takes from paddle input
    // reset (PTRIG $C070) for the paddle read bits at $C064/7 to toggle from high to low.
    //
    // Hence the standard analog component equation for time to capacitor change
    //    T = RC where R = 0 to 1.5e5 scaled from axis value 0 to 1023

    //  reset paddle timers to their values calculated from the paddle inputs if set
    //  before calling sync()

    //  R = Rmax * PDL/PDLmax
    //  t = RC  (C = 0.22 uF)
    //  t = R * (0.022*1e-6 F)
    //  seconds  = (Rmax * PDL / PDLmax)  * 0.022 * 1e-6
    //  microseconds = Rmax * (PDL / PDLmax) * 0.022
    //  nanoseconds = Rmax * PDL * 22 / PDLmax
    //
    for (index = 0; index < 4; ++index) {
        if (index < 2) {
        }
        if (adb->gameport.paddle[index] != CLEM_GAMEPORT_PADDLE_AXIS_VALUE_INVALID) {
            ns = CLEM_GAMEPORT_CALCULATE_TIME_NS(adb, index);
            adb->gameport.paddle_timer_ns[index] = (uint32_t)ns;
            // printf("GPRT: RESET PDL(%d): %u, TIME: %u\n", index, adb->gameport.paddle[index],
            //        (uint32_t)ns);
        } else {
            adb->gameport.paddle_timer_ns[index] = 0;
        }
        adb->gameport.paddle_timer_state[index] = 0x80;
    }
}

void clem_adb_device_input(struct ClemensDeviceADB *adb, const struct ClemensInputEvent *input) {
    //  events are sent to our ADB 'microcontroller'
    //      keyboard events are queued up for buffering by the microcontroller
    //      and picking by the host.
    //      mouse events are polled
    //
    int16_t key_index = input->value_a & 0x7f;
    switch (input->type) {
    case kClemensInputType_None:
        break;
    case kClemensInputType_KeyDown:
        if (input->value_a == key_index) { /* filter unsupported keys */
            // CLEM_LOG("Key Dn: %02X", key_index);
            if (!adb->keyb.states[key_index]) {
                _clem_adb_glu_queue_key(adb, key_index);
                adb->keyb.states[key_index] = 1;
            }
        }
        break;
    case kClemensInputType_KeyUp:
        if (input->value_a == key_index) { /* filter unsupported keys */
            // CLEM_LOG("Key Up: %02X", key_index);
            if (adb->keyb.states[key_index]) {
                _clem_adb_glu_queue_key(adb, 0x80 | key_index);
                adb->keyb.states[key_index] = 0;
            }
        }
        break;
    case kClemensInputType_MouseButtonDown:
        adb->mouse.btn_down = true;
        _clem_adb_glu_queue_mouse(adb, 0, 0);
        break;
    case kClemensInputType_MouseButtonUp:
        adb->mouse.btn_down = false;
        _clem_adb_glu_queue_mouse(adb, 0, 0);
        break;
    case kClemensInputType_MouseMove:
        adb->mouse.tracking_enabled = false;
        _clem_adb_glu_queue_mouse(adb, input->value_a, input->value_b);
        break;
    case kClemensInputType_MouseMoveAbsolute:
        _clem_adb_glu_queue_tracked_mouse(adb, input->value_a, input->value_b);
        break;
    case kClemensInputType_Paddle:
        _clem_adb_gameport_paddle(
            adb, input->gameport_button_mask >> 31, input->value_a, input->value_b,
            (uint8_t)(input->gameport_button_mask & CLEM_GAMEPORT_BUTTON_MASK_BUTTONS));
        break;
    case kClemensInputType_PaddleDisconnected:
        _clem_adb_gameport_paddle(adb, input->gameport_button_mask >> 31,
                                  CLEM_GAMEPORT_PADDLE_AXIS_VALUE_INVALID,
                                  CLEM_GAMEPORT_PADDLE_AXIS_VALUE_INVALID, 0);
        break;
    }

    if (input->type != kClemensInputType_Paddle) {
        clem_adb_device_key_toggle(adb, input->adb_key_toggle_mask);
    }
}

void clem_adb_device_key_toggle(struct ClemensDeviceADB *adb, unsigned enabled) {
    if (enabled & CLEM_ADB_KEYB_TOGGLE_CAPS_LOCK) {
        adb->keyb_reg[2] |= CLEM_ADB_GLU_REG2_KEY_CAPS_TOGGLE;
    } else {
        adb->keyb_reg[2] &= ~CLEM_ADB_GLU_REG2_KEY_CAPS_TOGGLE;
    }
}

uint8_t *clem_adb_ascii_from_a2code(unsigned input) { return &g_a2_to_ascii[input & 0x7f][0]; }

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

static void _clem_adb_start_cmd(struct ClemensDeviceADB *adb, uint8_t value) {
    unsigned device_command = 0;
    unsigned device_address = 0;
    bool parsed_command = true;

    adb->cmd_reg = value;
    adb->cmd_flags &= ~(CLEM_ADB_C026_RECV_READY + CLEM_ADB_C026_RECV_CNT);

    switch (value) {
    case CLEM_ADB_CMD_ABORT:
        _clem_adb_expect_data(adb, 0);
        break;
    case CLEM_ADB_CMD_SET_MODES:
        _clem_adb_expect_data(adb, 1);
        break;
    case CLEM_ADB_CMD_CLEAR_MODES:
        _clem_adb_expect_data(adb, 1);
        break;
    case CLEM_ADB_CMD_SET_CONFIG:
        _clem_adb_expect_data(adb, 3); /* 3 config bytes */
        break;
    case CLEM_ADB_CMD_SYNC:
        /* TODO - verify? */
        /* SetMode, ClearMode, Config */
        if (adb->version >= CLEM_ADB_ROM_3) {
            _clem_adb_expect_data(adb, 8);
        } else {
            _clem_adb_expect_data(adb, 4);
        }
        break;
    case CLEM_ADB_CMD_WRITE_RAM:
        _clem_adb_expect_data(adb, 2); /* address, value */
        break;
    case CLEM_ADB_CMD_READ_MEM:
        _clem_adb_expect_data(adb, 2); /* address, ram(00)/rom(>00) */
        break;
    case CLEM_ADB_CMD_VERSION:
        _clem_adb_expect_data(adb, 0);
        break;
    case CLEM_ADB_CMD_UNDOCUMENTED_12:
        _clem_adb_expect_data(adb, 2);
        break;
    case CLEM_ADB_CMD_UNDOCUMENTED_13:
        _clem_adb_expect_data(adb, 2);
        break;
    default:
        parsed_command = false;
        device_command = value & 0xf0;
        device_address = value & 0x0f;
        break;
    }
    switch (device_command) {
    case 0:
        if (!parsed_command) {
            CLEM_UNIMPLEMENTED("ADB: Command: %02X", value);
        }
        break;
    case CLEM_ADB_CMD_DEVICE_ENABLE_SRQ:
        CLEM_UNIMPLEMENTED("ADB: Enable SRQ: %0X", device_address);
        break;

    case CLEM_ADB_CMD_DEVICE_FLUSH:
        CLEM_UNIMPLEMENTED("ADB: Flush: %0X", device_address);
        break;

    case CLEM_ADB_CMD_DEVICE_DISABLE_SRQ:
        _clem_adb_expect_data(adb, 0);
        break;

    case CLEM_ADB_CMD_DEVICE_XMIT_2_R0:
    case CLEM_ADB_CMD_DEVICE_XMIT_2_R1:
    case CLEM_ADB_CMD_DEVICE_XMIT_2_R2:
    case CLEM_ADB_CMD_DEVICE_XMIT_2_R3:
        /* device will listen for 2 bytes and inject into register RX */
        _clem_adb_expect_data(adb, 2);
        break;

    case CLEM_ADB_CMD_DEVICE_POLL_0:
        CLEM_UNIMPLEMENTED("ADB: Poll 0: %0X", device_address);
        break;

    case CLEM_ADB_CMD_DEVICE_POLL_1:
        CLEM_UNIMPLEMENTED("ADB: Poll 1: %0X", device_address);
        break;

    case CLEM_ADB_CMD_DEVICE_POLL_2:
        CLEM_UNIMPLEMENTED("ADB: Poll 2: %0X", device_address);
        break;

    case CLEM_ADB_CMD_DEVICE_POLL_3:
        CLEM_UNIMPLEMENTED("ADB: Poll 3: %0X", device_address);
        break;

    default: /* unimplemented? */
        CLEM_UNIMPLEMENTED("ADB: Other %02X", value);
        break;
    }
}

static void _clem_adb_write_cmd(struct ClemensDeviceADB *adb, uint8_t value) {
    switch (adb->state) {
    case CLEM_ADB_STATE_READY:
        adb->cmd_status |= CLEM_ADB_C027_CMD_FULL;
        _clem_adb_start_cmd(adb, value);
        break;
    case CLEM_ADB_STATE_CMD_DATA:
        CLEM_DEBUG("ADB: Command Data [%02X]:%02X", adb->cmd_data_sent, value);
        adb->cmd_status |= CLEM_ADB_C027_CMD_FULL;
        _clem_adb_add_data(adb, value);
        break;
    }
}

void clem_adb_write_switch(struct ClemensDeviceADB *adb, uint8_t ioreg, uint8_t value) {
    switch (ioreg) {
    case CLEM_MMIO_REG_ANYKEY_STROBE:
        /* always clear strobe bit */
        adb->io_key_last_ascii &= ~0x80;
        break;
    case CLEM_MMIO_REG_ADB_MODKEY:
        CLEM_WARN("ADB: IO Write %02X (MODKEY)", ioreg);
        break;
    case CLEM_MMIO_REG_ADB_STATUS:
        /* TODO: Throw a warning if keyboard data interrupt enabled - not
                 supported according to docs
        */
        if (value & CLEM_ADB_C027_DATA_IRQ) {
            adb->cmd_status |= CLEM_ADB_C027_DATA_IRQ;
        } else {
            adb->cmd_status &= ~CLEM_ADB_C027_DATA_IRQ;
            adb->irq_line &= ~CLEM_IRQ_ADB_DATA;
        }
        if (value & CLEM_ADB_C027_MOUSE_IRQ) {
            adb->cmd_status |= CLEM_ADB_C027_MOUSE_IRQ;
        } else {
            adb->cmd_status &= ~CLEM_ADB_C027_MOUSE_IRQ;
            adb->irq_line &= ~CLEM_IRQ_ADB_MOUSE_EVT;
        }
        if (value & CLEM_ADB_C027_KEY_IRQ) {
            CLEM_WARN("ADB: Unimplemented keyboard interrupts! write %02X,%02X", ioreg, value);
        }
        break;
    case CLEM_MMIO_REG_ADB_CMD_DATA:
        _clem_adb_write_cmd(adb, value);
        break;
    case CLEM_MMIO_REG_PTRIG:
        // printf("MMIO: PTRIG WRITE\n");
        _clem_adb_gameport_reset(adb);
        break;
    default:
        CLEM_WARN("ADB: Unimplemented write %02X,%02X", ioreg, value);
        break;
    }
}

static uint8_t _clem_adb_read_cmd(struct ClemensDeviceADB *adb, uint8_t flags) {
    uint8_t result = 0x00;
    /* generally speaking, when we read from here, the data */
    switch (adb->state) {
    case CLEM_ADB_STATE_READY:
        result = adb->cmd_flags;
        if (!CLEM_IS_IO_NO_OP(flags)) {
            adb->cmd_status &= ~CLEM_ADB_C027_CMD_FULL;
            adb->irq_line &= ~CLEM_IRQ_ADB_DATA;
            adb->cmd_flags = 0;
            /* TODO if response data was queued (sent) then switch state to
             * CLEM_ADB_STATE_RESULT_DATA */
        }
        break;
    case CLEM_ADB_STATE_CMD_DATA:
        /* TODO: read back current data? *clear cmd valid? */
        break;
    case CLEM_ADB_STATE_RESULT_DATA:
        result = adb->cmd_data[adb->cmd_data_recv];
        if (!CLEM_IS_IO_NO_OP(flags)) {
            if (adb->cmd_data_sent > adb->cmd_data_recv) {
                ++adb->cmd_data_recv;
            }
            if (adb->cmd_data_sent == adb->cmd_data_recv) {
                adb->cmd_status &= ~CLEM_ADB_C027_DATA_FULL;
                adb->irq_line &= ~CLEM_IRQ_ADB_DATA;
            }
            if (adb->cmd_data_recv >= adb->cmd_data_limit) {
                _clem_adb_glu_command_done(adb);
            }
        }
        break;
    }
    return result;
}

static uint8_t _clem_adb_read_modkeys(struct ClemensDeviceADB *adb) {
    uint8_t modkeys = 0;
    if (adb->keyb_reg[2] & CLEM_ADB_GLU_REG2_KEY_APPLE) {
        modkeys |= 0x80;
    }
    if (adb->keyb_reg[2] & CLEM_ADB_GLU_REG2_KEY_OPTION) {
        modkeys |= 0x40;
    }
    if (adb->is_keypad_down) {
        modkeys |= 0x10;
    }
    if (adb->keyb_reg[2] & CLEM_ADB_GLU_REG2_KEY_CAPS) {
        modkeys |= 0x04;
    }
    if (adb->keyb_reg[2] & CLEM_ADB_GLU_REG2_KEY_CTRL) {
        modkeys |= 0x02;
    }
    if (adb->keyb_reg[2] & CLEM_ADB_GLU_REG2_KEY_SHIFT) {
        modkeys |= 0x01;
    }
    if (adb->is_asciikey_down) {
        /* FIXME: should this be any key like c010, or anykey at all? HW Ref
           implies a 'key is being held down' - and we're assuming ascii vs
           scan code here... */
        modkeys |= 0x08;
    }
    if (adb->has_modkey_changed) {
        modkeys |= 0x20;
    }
    return modkeys;
}

static uint8_t _clem_adb_read_mouse_data(struct ClemensDeviceADB *adb, uint8_t flags) {
    uint8_t result = 0x00;
    if (adb->cmd_status & CLEM_ADB_C027_MOUSE_Y) {
        result |= (adb->mouse_reg[0] >> 8);
    } else {
        result |= (adb->mouse_reg[0] & 0xff);
    }
    if (!(flags & CLEM_OP_IO_NO_OP)) {
        adb->cmd_status ^= CLEM_ADB_C027_MOUSE_Y;
        if (!(adb->cmd_status & CLEM_ADB_C027_MOUSE_Y)) {
            adb->cmd_status &= ~CLEM_ADB_C027_MOUSE_FULL;
        }
    }
    return result;
}

uint8_t clem_adb_read_mega2_switch(struct ClemensDeviceADB *adb, uint8_t ioreg, uint8_t flags) {
    bool is_noop = (flags & CLEM_OP_IO_NO_OP) != 0;
    if (ioreg > CLEM_MMIO_REG_KEYB_READ && ioreg < CLEM_MMIO_REG_ANYKEY_STROBE) {
        ioreg = CLEM_MMIO_REG_KEYB_READ;
    }
    switch (ioreg) {
    case CLEM_MMIO_REG_KEYB_READ:
        if (!is_noop) {
            adb->cmd_status &= ~CLEM_ADB_C027_KEY_FULL;
            // CLEM_LOG("c0%02x: %02X", ioreg, adb->io_key_last_ascii);
        }
        return adb->io_key_last_ascii;
    case CLEM_MMIO_REG_ANYKEY_STROBE:
        /* clear strobe bit and return any-key status */
        if (!is_noop) {
            adb->io_key_last_ascii &= ~0x80;
            //  CLEM_LOG("c0%02x: %02X, %02X", ioreg, adb->is_asciikey_down,
            //  adb->io_key_last_ascii & 0x7f);
        }
        return (adb->is_asciikey_down ? 0x80 : 0x00) | (adb->io_key_last_ascii & 0x7f);
    case CLEM_MMIO_REG_MEGA2_MOUSE_DX:
    case CLEM_MMIO_REG_MEGA2_MOUSE_DY:
    default:
        if (!CLEM_IS_IO_NO_OP(flags)) {
            CLEM_WARN("ADB: Unimplemented read %02X", ioreg);
        }
        break;
    };
    return 0;
}

uint8_t clem_adb_read_switch(struct ClemensDeviceADB *adb, uint8_t ioreg, uint8_t flags) {
    bool is_noop = (flags & CLEM_OP_IO_NO_OP) != 0;
    uint8_t tmp;
    switch (ioreg) {
    case CLEM_MMIO_REG_ADB_MOUSE_DATA:
        return _clem_adb_read_mouse_data(adb, flags);
    case CLEM_MMIO_REG_ADB_MODKEY:
        return _clem_adb_read_modkeys(adb);
    case CLEM_MMIO_REG_ADB_CMD_DATA:
        return _clem_adb_read_cmd(adb, flags);
    case CLEM_MMIO_REG_ADB_STATUS:
        /* FIXME: report back if cmd_flags is set to some value as this was
                  likely triggered by a data interrupt
        */
        tmp = adb->cmd_status;
        if (adb->cmd_flags != 0) {
            tmp |= CLEM_ADB_C027_DATA_FULL;
        }
        if (!is_noop) {
            adb->cmd_status &= ~(CLEM_ADB_C027_KEY_FULL);
            adb->irq_line &= ~(CLEM_IRQ_ADB_MOUSE_EVT);
        }
        return tmp;
    case CLEM_MMIO_REG_SW0:
        if (adb->keyb_reg[2] & CLEM_ADB_GLU_REG2_KEY_APPLE) {
            return 0x80;
        } else if (adb->gameport.btn_mask[0] & 0x55) {
            return 0x80;
        }
        break;
    case CLEM_MMIO_REG_SW1:
        if (adb->keyb_reg[2] & CLEM_ADB_GLU_REG2_KEY_OPTION) {
            return 0x80;
        } else if (adb->gameport.btn_mask[0] & 0xAA) {
            return 0x80;
        }
        break;
    case CLEM_MMIO_REG_SW2:
        if (adb->gameport.btn_mask[1] & 0x55) { // button 0, 2, 4, ...
            return 0x80;
        }
        break;
    case CLEM_MMIO_REG_SW3:
        if (adb->gameport.btn_mask[1] & 0xAA) { // button 1, 3, 5, ...
            return 0x80;
        }
        break;
    case CLEM_MMIO_REG_PTRIG:
        if (!is_noop) {
            // printf("MMIO: PTRIG READ\n");
            _clem_adb_gameport_reset(adb);
        }
        break;
    case CLEM_MMIO_REG_PADDL0:
    case CLEM_MMIO_REG_PADDL1:
    case CLEM_MMIO_REG_PADDL2:
    case CLEM_MMIO_REG_PADDL3:
        // if (!is_noop) {
        //     printf("MMIO: PDL(%u) = %02x\n", ioreg - CLEM_MMIO_REG_PADDL0,
        //            adb->gameport.paddle_timer_state[ioreg - CLEM_MMIO_REG_PADDL0]);
        // }
        return adb->gameport.paddle_timer_state[ioreg - CLEM_MMIO_REG_PADDL0];
    case CLEM_MMIO_REG_AN0_OFF:
        adb->gameport.ann_mask &= ~0x1;
        break;
    case CLEM_MMIO_REG_AN0_ON:
        adb->gameport.ann_mask |= 0x1;
        break;
    case CLEM_MMIO_REG_AN1_OFF:
        adb->gameport.ann_mask &= ~0x2;
        break;
    case CLEM_MMIO_REG_AN1_ON:
        adb->gameport.ann_mask |= 0x2;
        break;
    case CLEM_MMIO_REG_AN2_OFF:
        adb->gameport.ann_mask &= ~0x4;
        break;
    case CLEM_MMIO_REG_AN2_ON:
        adb->gameport.ann_mask |= 0x4;
        break;
    case CLEM_MMIO_REG_AN3_OFF:
        adb->gameport.ann_mask &= ~0x8;
        break;
    case CLEM_MMIO_REG_AN3_ON:
        adb->gameport.ann_mask |= 0x8;
        break;
    default:
        if (!CLEM_IS_IO_NO_OP(flags)) {
            CLEM_WARN("ADB: Unimplemented read %02X", ioreg);
        }
        break;
    }
    return 0;
}
