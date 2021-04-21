#include "clem_device.h"
#include "clem_mmio_defs.h"
#include "clem_debug.h"

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
#define CLEM_ADB_STATE_READY            0
/* GLU/controller will receive command data from the host via writes to C026 */
#define CLEM_ADB_STATE_CMD_DATA         1

/* ADB Sync, a combination of SetMode, ClearMode, SetConfig */
#define CLEM_ADB_CMD_SYNC               0x07
#define CLEM_ADB_CMD_DEVICE_ENABLE_SRQ  0x50
#define CLEM_ADB_CMD_DEVICE_FLUSH       0x60
#define CLEM_ADB_CMD_DEVICE_DISABLE_SRQ 0x70
#define CLEM_ADB_CMD_DEVICE_TRANSMIT_2  0x80
#define CLEM_ADB_CMD_DEVICE_POLL_0      0xc0
#define CLEM_ADB_CMD_DEVICE_POLL_1      0xd0
#define CLEM_ADB_CMD_DEVICE_POLL_2      0xe0
#define CLEM_ADB_CMD_DEVICE_POLL_3      0xf0

/* c026 status flags */
#define CLEM_ADB_C026_SRQ               0x08

/* c027 status flags */
#define CLEM_ADB_C027_CMD_FULL          0x01
#define CLEM_ADB_C027_KEY_FULL          0x08

/* This version is returned by the ADB microcontroller based on ROM type */
#define CLEM_ADB_ROM_3                  0x06

/* GLU Device addresses */
#define CLEM_ADB_DEVICE_KEYBOARD        0x02
#define CLEM_ADB_DEVICE_MOUSE           0x03

/* GLU register flags */
#define CLEM_ADB_GLU_REG2_KEY_CLEAR_NUMLOCK 0x0080
#define CLEM_ADB_GLU_REG2_KEY_APPLE         0x0100
#define CLEM_ADB_GLU_REG2_KEY_OPTION        0x0200
#define CLEM_ADB_GLU_REG2_KEY_SHIFT         0x0400
#define CLEM_ADB_GLU_REG2_KEY_CTRL          0x0800
#define CLEM_ADB_GLU_REG2_KEY_RESET         0x1000
#define CLEM_ADB_GLU_REG2_KEY_CAPS          0x2000
/* no scroll lock and LEDs counted - see 'ADB - The Untold Story' refe */
#define CLEM_ADB_GLU_REG2_MODKEY_MASK       0x7f80

#define CLEM_ADB_GLU_REG3_MASK_SRQ          0x2000
#define CLEM_ADB_GLU_REG3_DEVICE_MASK       0x0F00


#define CLEM_ADB_GLU_REG0_MOUSE_BTNUP       0x8000
#define CLEM_ADB_GLU_REG0_MOUSE_DIR_Y       0x4000
#define CLEM_ADB_GLU_REG0_MOUSE_DELTA_Y     0x3f00
#define CLEM_ADB_GLU_REG0_MOUSE_DIR_X       0x0040
#define CLEM_ADB_GLU_REG0_MOUSE_DELTA_X     0x003f

/* ADB Mode Flags */
#define CLEM_ADB_MODE_AUTOPOLL_KEYB         0x00000001
#define CLEM_ADB_MODE_AUTOPOLL_MOUSE        0x00000002

/**
 * SRQs are disabled for all devices on reset
 * Autopoll enabled for all devices on reset
 */
#define CLEM_ADB_GLU_SRQ_60HZ_CYCLES        600

void clem_adb_reset(struct ClemensDeviceADB* adb) {
    memset(adb, 0, sizeof(*adb));
    adb->version = CLEM_ADB_ROM_3;      /* TODO - input to reset? */
    adb->mode_flags = CLEM_ADB_MODE_AUTOPOLL_KEYB |
                      CLEM_ADB_MODE_AUTOPOLL_MOUSE;
    adb->keyb.size = 0;
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
#define CLEM_ADB_KEY_MOD_SHIFT  0x01
#define CLEM_ADB_KEY_MOD_CTRL   0x02
#define CLEM_ADB_KEY_MOD_CAPS   0x04
#define CLEM_ADB_KEY_MOD_KEYPAD 0x10
#define CLEM_ADB_KEY_MOD_OPTION 0x40
#define CLEM_ADB_KEY_MOD_APPLE  0x80

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
    /* 0x00 */  { 'a',  0x01,   'A',    0x01,   0x00, },
    /* 0x01 */  { 's',  0x13,   'S',    0x13,   0x00, },
    /* 0x02 */  { 'd',  0x04,   'D',    0x04,   0x00, },
    /* 0x03 */  { 'f',  0x06,   'F',    0x06,   0x00, },
    /* 0x04 */  { 'h',  0x08,   'H',    0x08,   0x00, },
    /* 0x05 */  { 'g',  0x07,   'G',    0x07,   0x00, },
    /* 0x06 */  { 'z',  0x1a,   'Z',    0x1a,   0x00, },
    /* 0x07 */  { 'x',  0x18,   'X',    0x18,   0x00, },
    /* 0x08 */  { 'c',  0x03,   'C',    0x03,   0x00, },
    /* 0x09 */  { 'v',  0x16,   'V',    0x16,   0x00, },
    /* 0x0A */  { 0xff, 0xff,   0xff,   0xff,   0x00, },
    /* 0x0B */  { 'b',  0x02,   'B',    0x02,   0x00, },
    /* 0x0C */  { 'q',  0x11,   'Q',    0x11,   0x00, },
    /* 0x0D */  { 'w',  0x17,   'W',    0x17,   0x00, },
    /* 0x0E */  { 'e',  0x05,   'E',    0x05,   0x00, },
    /* 0x0F */  { 'r',  0x12,   'R',    0x12,   0x00, },
    /* 0x10 */  { 't',  0x14,   'T',    0x14,   0x00, },
    /* 0x11 */  { 'y',  0x19,   'Y',    0x19,   0x00, },
    /* 0x12 */  { '1',  '1',    '!',    '!',    0x00, },
    /* 0x13 */  { '2',  0x00,   '@',    0x00,   0x00, },
    /* 0x14 */  { '3',  '3',    '#',    '#',    0x00, },
    /* 0x15 */  { '4',  '4',    '$',    '$',    0x00, },
    /* 0x16 */  { '6',  0x1e,   '^',    0x1e,   0x00, },
    /* 0x17 */  { '5',  '5',    '%',    '%',    0x00, },
    /* 0x18 */  { '=',  '=',    '+',    '+',    0x00, },
    /* 0x19 */  { '9',  '9',    '(',    '(',    0x00, },
    /* 0x1A */  { '7',  '7',    '&',    '&',    0x00, },
    /* 0x1B */  { '-',  0x1f,   '_',    0x1f,   0x00, },
    /* 0x1C */  { '8',  '8',    '*',    '*',    0x00, },
    /* 0x1D */  { '0',  '0',    ')',    ')',    0x00, },
    /* 0x1E */  { ']',  0x1d,   '}',    0x1d,   0x00, },
    /* 0x1F */  { 'o',  0x0f,   'O',    0x0f,   0x00, },
    /* 0x20 */  { 'u',  0x15,   'U',    0x15,   0x00, },
    /* 0x21 */  { '[',  0x1b,   '{',    0x1b,   0x00, },
    /* 0x22 */  { 'i',  0x09,   'I',    0x09,   0x00, },
    /* 0x23 */  { 'p',  0x10,   'P',    0x10,   0x00, },
    /* 0x24 */  { 0x0d, 0xff,   0x0d,   0xff,   0x00, }, /* CR */
    /* 0x25 */  { 'l',  0x0c,   'L',    0x0c,   0x00, },
    /* 0x26 */  { 'j',  0x0a,   'J',    0x0a,   0x00, },
    /* 0x27 */  { 0x27, 0xff,   0x22,   0xff,   0x00, }, /* apostrophe */
    /* 0x28 */  { 'k',  0x0b,   'K',    0x0b,   0x00, },
    /* 0x29 */  { ';',  ';',    ':',    ':',    0x00, },
    /* 0x2A */  { '\\', 0x1c,   '|',    0x1c,   0x00, },
    /* 0x2B */  { ',',  ',',    '<',    '<',    0x00, },
    /* 0x2C */  { '/',  '/',    '?',    '?',    0x00, },
    /* 0x2D */  { 'n',  0x0e,   'N',    0x0e,   0x00, },
    /* 0x2E */  { 'm',  0x0d,   'M',    0x0d,   0x00, },
    /* 0x2F */  { '.',  '.',    '>',    '>',    0x00, },
    /* 0x30 */  { 0x09, 0x09,   0x09,   0x09,   0x00, }, /* TAB */
    /* 0x31 */  { 0x20, 0x20,   0x20,   0x20,   0x00, }, /* SPACE */
    /* 0x32 */  { '`',  '`',    '~',    '~',    0x00, },
    /* 0x33 */  { 0x7f, 0x7f,   0x7f,   0x7f,   0x00, }, /* DELETE */
    /* 0x34 */  { 0xff, 0xff,   0xff,   0xff,   0x00, },
    /* 0x35 */  { 0x1b, 0x1b,   0x1b,   0x1b,   0x00, }, /* ESCAPE */
    /* 0x36 */  { 0xff, 0xff,   0xff,   0xff,   CLEM_ADB_KEY_MOD_CTRL, },
    /* 0x37 */  { 0xff, 0xff,   0xff,   0xff,   CLEM_ADB_KEY_MOD_APPLE, },
    /* 0x38 */  { 0xff, 0xff,   0xff,   0xff,   CLEM_ADB_KEY_MOD_SHIFT, },
    /* 0x39 */  { 0xff, 0xff,   0xff,   0xff,   CLEM_ADB_KEY_MOD_CAPS, },
    /* 0x3A */  { 0xff, 0xff,   0xff,   0xff,   CLEM_ADB_KEY_MOD_OPTION, },
    /* 0x3B */  { 0x08, 0x08,   0x08,   0x08,   0x00, }, /* LEFT */
    /* 0x3C */  { 0x15, 0x15,   0x15,   0x15,   0x00, }, /* RIGHT */
    /* 0x3D */  { 0x0a, 0x0a,   0x0a,   0x0a,   0x00, }, /* DOWN */
    /* 0x3E */  { 0x0b, 0x0b,   0x0b,   0x0b,   0x00, }, /* UP */
    /* 0x3F */  { 0xff, 0xff,   0xff,   0xff,   0x00, },
    /* 0x40 */  { 0xff, 0xff,   0xff,   0xff,   0x00, },
    /* 0x41 */  { '.',  '.',    '.',    '.',    CLEM_ADB_KEY_MOD_KEYPAD, },
    /* 0x42 */  { 0xff, 0xff,   0xff,   0xff,   0x00, },
    /* 0x43 */  { '*',  '*',    '*',    '*',    CLEM_ADB_KEY_MOD_KEYPAD, },
    /* 0x44 */  { 0xff, 0xff,   0xff,   0xff,   0x00, },
    /* 0x45 */  { '+',  '+',    '+',    '+',    CLEM_ADB_KEY_MOD_KEYPAD, },
    /* 0x46 */  { 0xff, 0xff,   0xff,   0xff,   0x00, },
    /* 0x47 */  { 0x18, 0x18,   0x18,   0x18,   CLEM_ADB_KEY_MOD_KEYPAD, },
    /* 0x48 */  { 0xff, 0xff,   0xff,   0xff,   0x00, },
    /* 0x49 */  { 0xff, 0xff,   0xff,   0xff,   0x00, },
    /* 0x4A */  { 0xff, 0xff,   0xff,   0xff,   0x00, },
    /* 0x4B */  { '/',  '/',    '/',    '/',    CLEM_ADB_KEY_MOD_KEYPAD, },
    /* 0x4C */  { 0x0d, 0x0d,   0x0d,   0x0d,   CLEM_ADB_KEY_MOD_KEYPAD, },
    /* 0x4D */  { 0xff, 0xff,   0xff,   0xff,   0x00, },
    /* 0x4E */  { '-',  '-',    '-',    '-',    CLEM_ADB_KEY_MOD_KEYPAD, },
    /* 0x4F */  { 0xff, 0xff,   0xff,   0xff,   0x00, },
    /* 0x50 */  { 0xff, 0xff,   0xff,   0xff,   0x00, },
    /* 0x51 */  { '=',  '=',    '=',    '=',    CLEM_ADB_KEY_MOD_KEYPAD, },
    /* 0x52 */  { '0',  '0',    '0',    '0',    CLEM_ADB_KEY_MOD_KEYPAD, },
    /* 0x53 */  { '1',  '1',    '1',    '1',    CLEM_ADB_KEY_MOD_KEYPAD, },
    /* 0x54 */  { '2',  '2',    '2',    '2',    CLEM_ADB_KEY_MOD_KEYPAD, },
    /* 0x55 */  { '3',  '3',    '3',    '3',    CLEM_ADB_KEY_MOD_KEYPAD, },
    /* 0x56 */  { '4',  '4',    '4',    '4',    CLEM_ADB_KEY_MOD_KEYPAD, },
    /* 0x57 */  { '5',  '5',    '5',    '5',    CLEM_ADB_KEY_MOD_KEYPAD, },
    /* 0x58 */  { '6',  '6',    '6',    '6',    CLEM_ADB_KEY_MOD_KEYPAD, },
    /* 0x59 */  { '7',  '7',    '7',    '7',    CLEM_ADB_KEY_MOD_KEYPAD, },
    /* 0x5A */  { 0xff, 0xff,   0xff,   0xff,   0x00, },
    /* 0x5B */  { '8',  '8',    '8',    '8',    CLEM_ADB_KEY_MOD_KEYPAD, },
    /* 0x5C */  { '9',  '9',    '9',    '9',    CLEM_ADB_KEY_MOD_KEYPAD, },
    /* 0x5D */  { 0xff, 0xff,   0xff,   0xff,   0x00, },
    /* 0x5E */  { 0xff, 0xff,   0xff,   0xff,   0x00, },
    /* 0x5F */  { 0xff, 0xff,   0xff,   0xff,   0x00, },
    /* 0x60 */  { 0xff, 0xff,   0xff,   0xff,   0x00, },
    /* 0x61 */  { 0xff, 0xff,   0xff,   0xff,   0x00, },
    /* 0x62 */  { 0xff, 0xff,   0xff,   0xff,   0x00, },
    /* 0x63 */  { 0xff, 0xff,   0xff,   0xff,   0x00, },
    /* 0x64 */  { 0xff, 0xff,   0xff,   0xff,   0x00, },
    /* 0x65 */  { 0xff, 0xff,   0xff,   0xff,   0x00, },
    /* 0x66 */  { 0xff, 0xff,   0xff,   0xff,   0x00, },
    /* 0x67 */  { 0xff, 0xff,   0xff,   0xff,   0x00, },
    /* 0x68 */  { 0xff, 0xff,   0xff,   0xff,   0x00, },
    /* 0x69 */  { 0xff, 0xff,   0xff,   0xff,   0x00, },
    /* 0x6A */  { 0xff, 0xff,   0xff,   0xff,   0x00, },
    /* 0x6B */  { 0xff, 0xff,   0xff,   0xff,   0x00, },
    /* 0x6C */  { 0xff, 0xff,   0xff,   0xff,   0x00, },
    /* 0x6D */  { 0xff, 0xff,   0xff,   0xff,   0x00, },
    /* 0x6E */  { 0xff, 0xff,   0xff,   0xff,   0x00, },
    /* 0x6F */  { 0xff, 0xff,   0xff,   0xff,   0x00, },
    /* 0x70 */  { 0xff, 0xff,   0xff,   0xff,   0x00, },
    /* 0x71 */  { 0xff, 0xff,   0xff,   0xff,   0x00, },
    /* 0x72 */  { 0xff, 0xff,   0xff,   0xff,   0x00, },
    /* 0x73 */  { 0xff, 0xff,   0xff,   0xff,   0x00, },
    /* 0x74 */  { 0xff, 0xff,   0xff,   0xff,   0x00, },
    /* 0x75 */  { 0xff, 0xff,   0xff,   0xff,   0x00, },
    /* 0x76 */  { 0xff, 0xff,   0xff,   0xff,   0x00, },
    /* 0x77 */  { 0xff, 0xff,   0xff,   0xff,   0x00, },
    /* 0x78 */  { 0xff, 0xff,   0xff,   0xff,   0x00, },
    /* 0x79 */  { 0xff, 0xff,   0xff,   0xff,   0x00, },
    /* 0x7A */  { 0xff, 0xff,   0xff,   0xff,   0x00, },
    /* 0x7B */  { 0xff, 0xff,   0xff,   0xff,   CLEM_ADB_KEY_MOD_SHIFT, },
    /* 0x7C */  { 0xff, 0xff,   0xff,   0xff,   CLEM_ADB_KEY_MOD_OPTION, },
    /* 0x7D */  { 0xff, 0xff,   0xff,   0xff,   CLEM_ADB_KEY_MOD_CTRL, },
    /* 0x7E */  { 0xff, 0xff,   0xff,   0xff,   0x00, },
    /* 0x7F */  { 0xff, 0xff,   0xff,   0xff,   0x00, }
};

void _clem_adb_glu_queue_key(
    struct ClemensDeviceADB* adb,
    uint8_t key
) {
    if (adb->keyb.size >= CLEM_ADB_KEYB_BUFFER_LIMIT) {
        return;
    }
    adb->keyb.keys[adb->keyb.size++] = key;
}

uint8_t _clem_adb_glu_unqueue_key(struct ClemensDeviceADB* adb) {
    uint8_t i;
    uint8_t key;
    CLEM_ASSERT(adb->keyb.size > 0);
    key = adb->keyb.keys[0];
    if (adb->keyb.size > 0) {
        --adb->keyb.size;
        for (i = 0; i < adb->keyb.size; ++i) {
            adb->keyb.keys[i] = adb->keyb.keys[i+1];
        }
    }
    return key;
}

uint8_t _clem_adb_glu_keyb_parse(
    struct ClemensDeviceADB* adb,
    uint8_t key_event
) {
    uint8_t key_index = key_event & 0x7f;
    bool is_key_down = (key_event & 0x80) != 0;
    uint8_t ascii_key;

    uint8_t* ascii_table = &g_a2_to_ascii[key_index][0];
    uint16_t modifiers = adb->keyb_reg[2] & CLEM_ADB_GLU_REG2_MODKEY_MASK;
    uint16_t old_modifiers = modifiers & CLEM_ADB_GLU_REG2_MODKEY_MASK;

    if (is_key_down) {
        adb->io_key_last_a2key = key_index;
    }
    adb->is_keypad_down = ascii_table[4] == CLEM_ADB_KEY_MOD_KEYPAD &&
                          is_key_down;
    if (ascii_table[4]) {       // key is a modifier?
        if (ascii_table[4] == CLEM_ADB_KEY_MOD_APPLE) {
            if (is_key_down) modifiers |= CLEM_ADB_GLU_REG2_KEY_APPLE;
            else modifiers &= ~CLEM_ADB_GLU_REG2_KEY_APPLE;
        } else if (ascii_table[4] == CLEM_ADB_KEY_MOD_OPTION) {
            if (is_key_down) modifiers |= CLEM_ADB_GLU_REG2_KEY_OPTION;
            else modifiers &= ~CLEM_ADB_GLU_REG2_KEY_OPTION;
        } else if (ascii_table[4] == CLEM_ADB_KEY_MOD_SHIFT) {
            if (is_key_down) modifiers |= CLEM_ADB_GLU_REG2_KEY_SHIFT;
            else modifiers &= ~CLEM_ADB_GLU_REG2_KEY_SHIFT;
        } else if (ascii_table[4] == CLEM_ADB_KEY_MOD_CTRL) {
            if (is_key_down) modifiers |= CLEM_ADB_GLU_REG2_KEY_CTRL;
            else modifiers &= ~CLEM_ADB_GLU_REG2_KEY_CTRL;
        } else if (ascii_table[4] == CLEM_ADB_KEY_MOD_CAPS) {
            if (is_key_down) modifiers |= CLEM_ADB_GLU_REG2_KEY_CAPS;
            else modifiers &= ~CLEM_ADB_GLU_REG2_KEY_CAPS;
        }
        adb->keyb_reg[2] = modifiers;
    }
    /* Additional parsing needed for MMIO registers */
    if (modifiers & (CLEM_ADB_GLU_REG2_KEY_CTRL+CLEM_ADB_GLU_REG2_KEY_SHIFT)) {
        ascii_key = ascii_table[3];
    } else if (modifiers & CLEM_ADB_GLU_REG2_KEY_SHIFT) {
        ascii_key = ascii_table[2];
    } else if (modifiers & CLEM_ADB_GLU_REG2_KEY_CTRL) {
        ascii_key = ascii_table[1];
    } else {
        ascii_key = ascii_table[0];
    }
    if (ascii_key != 0xff) {
        if (is_key_down) {
            adb->io_key_last_ascii =  0x80 | ascii_key;
            /* via HWRef, but FWRef contradicts? */
            adb->cmd_status |= CLEM_ADB_C027_KEY_FULL;
            adb->is_asciikey_down = true;
        } else {
            adb->is_asciikey_down = false;
        }
    }

    /* FIXME: sketchy - is this doing what a  modifier key latch does? */
    if ((modifiers ^ old_modifiers) && !adb->is_asciikey_down) {
        modifiers &= CLEM_ADB_GLU_REG2_MODKEY_MASK;
        adb->has_modkey_changed = true;
    }

    return key_event;
}

void _clem_adb_glu_keyb_talk(struct ClemensDeviceADB* adb) {
    uint8_t* ascii_table;
    uint8_t key_event;
    uint8_t key_index;
    bool is_key_down;

    adb->keyb_reg[0] = 0x0000;

    if (adb->keyb.size <= 0) return;
    key_event = _clem_adb_glu_unqueue_key(adb);

    /* The reset key is special - it takes up the whole register, and so
       for the first unqueue, only allow one read from the key queue
    */
    if ((key_event & 0x7f) == CLEM_ADB_KEY_RESET) {
        if (key_event & 0x80) adb->keyb_reg[0] = 0x7f7f;
        else adb->keyb_reg[0] = 0xffff;
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
void _clem_adb_glu_mouse_talk(struct ClemensDeviceADB* adb) {



}

void _clem_adb_glu_set_mode_flags(
    struct ClemensDeviceADB* adb,
    unsigned mode_flags
) {
    if (!(mode_flags ^ (adb->mode_flags & 0xff))) return;

    if (mode_flags & 0x01) {
        adb->mode_flags &= ~CLEM_ADB_MODE_AUTOPOLL_KEYB;
    } else {
        adb->mode_flags |= CLEM_ADB_MODE_AUTOPOLL_KEYB;
    }
    if (mode_flags & 0x02) {
        adb->mode_flags &= ~CLEM_ADB_MODE_AUTOPOLL_MOUSE;
    } else {
        adb->mode_flags |= CLEM_ADB_MODE_AUTOPOLL_MOUSE;
    }
    if (mode_flags & 0x000000fc) {
        CLEM_WARN("ADB SetMode %02X Unimplemented", mode_flags & 0x000000fc);
    }
}

void _clem_adb_glu_enable_srq(
    struct ClemensDeviceADB* adb,
    unsigned device_address,
    bool enable
) {
    switch (device_address) {
        case CLEM_ADB_DEVICE_KEYBOARD:
            if (enable) {
                adb->keyb_reg[3] |= CLEM_ADB_GLU_REG3_MASK_SRQ;
            } else {
                adb->keyb_reg[3] &= ~CLEM_ADB_GLU_REG3_MASK_SRQ;
            }
            break;

        case CLEM_ADB_DEVICE_MOUSE:
            if (enable) {
                adb->mouse_reg[3] |= CLEM_ADB_GLU_REG3_MASK_SRQ;
            } else {
                adb->mouse_reg[3] &= ~CLEM_ADB_GLU_REG3_MASK_SRQ;
            }
            break;
        default:
            CLEM_WARN("ADB Device Address %u unsupported", device_address);
            break;
    }
}


void _clem_adb_glu_command(struct ClemensDeviceADB* adb) {
    unsigned device_command = 0;
    unsigned device_address = 0;
    unsigned param;
    switch (adb->cmd_reg) {
        case CLEM_ADB_CMD_SYNC:
            CLEM_LOG("ADB SYNC: %02X %02X %02X %02X",
                     adb->cmd_data[0],
                     adb->cmd_data[1],
                     adb->cmd_data[2],
                     adb->cmd_data[3]);
            CLEM_LOG("ADB SYNC ROM03: %02X %02X %02X %02X",
                     adb->cmd_data[4],
                     adb->cmd_data[5],
                     adb->cmd_data[6],
                     adb->cmd_data[7]);
            _clem_adb_glu_set_mode_flags(adb, adb->cmd_data[0]);
            break;
        default:
            device_command = adb->cmd_reg & 0xf0;
            device_address = adb->cmd_reg & 0x0f;
            break;
    }

    switch (device_command) {
        case 0:
            break;

        case CLEM_ADB_CMD_DEVICE_ENABLE_SRQ:
            CLEM_LOG("ADB ENABLE SRQ: %0X", device_address);
            _clem_adb_glu_enable_srq(adb, device_address, true);
            break;

        case CLEM_ADB_CMD_DEVICE_FLUSH:
            CLEM_UNIMPLEMENTED("ADB Flush: %0X", device_address);
            break;

        case CLEM_ADB_CMD_DEVICE_DISABLE_SRQ:
            CLEM_LOG("ADB DISABLE SRQ: %0X", device_address);
            _clem_adb_glu_enable_srq(adb, device_address, false);
            break;

        case CLEM_ADB_CMD_DEVICE_TRANSMIT_2:
            CLEM_UNIMPLEMENTED("ADB Transmit: %0X", device_address);
            break;

        case CLEM_ADB_CMD_DEVICE_POLL_0:
            CLEM_UNIMPLEMENTED("ADB Poll 0: %0X", device_address);
            break;

        case CLEM_ADB_CMD_DEVICE_POLL_1:
            CLEM_UNIMPLEMENTED("ADB Poll 1: %0X", device_address);
            break;

        case CLEM_ADB_CMD_DEVICE_POLL_2:
            CLEM_UNIMPLEMENTED("ADB Poll 2: %0X", device_address);
            break;

        case CLEM_ADB_CMD_DEVICE_POLL_3:
            CLEM_UNIMPLEMENTED("ADB Poll 3: %0X", device_address);
            break;
    }

}

uint32_t clem_adb_glu_sync(
    struct ClemensDeviceADB* adb,
    uint32_t delta_us,
    uint32_t irq_line
) {
    adb->poll_timer_us += delta_us;

  /*  On poll expiration, update device registers */
    while (adb->poll_timer_us  >= CLEM_MEGA2_CYCLES_PER_60TH) {
        if (adb->mode_flags & CLEM_ADB_MODE_AUTOPOLL_MOUSE) {
            _clem_adb_glu_mouse_talk(adb);
        }
        if (adb->mode_flags & CLEM_ADB_MODE_AUTOPOLL_KEYB) {
            _clem_adb_glu_keyb_talk(adb);
            irq_line &= ~CLEM_IRQ_ADB_SRQ;
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
                adb->cmd_status &= ~CLEM_ADB_C027_CMD_FULL;
            }
            if (adb->cmd_data_recv >= adb->cmd_data_limit) {
                _clem_adb_glu_command(adb);
                adb->state = CLEM_ADB_STATE_READY;
            }
            break;
        default:
            break;
    }



    if (adb->keyb_reg[3] & CLEM_ADB_GLU_REG3_MASK_SRQ) {
        if (adb->keyb.size > 0) {
            irq_line |= CLEM_IRQ_ADB_KEYB_SRQ;
        }
    } else {
        irq_line &= ~CLEM_IRQ_ADB_KEYB_SRQ;
    }
    if (irq_line & CLEM_IRQ_ADB_SRQ) {
        adb->cmd_flags |= CLEM_ADB_C026_SRQ;
    } else {
        adb->cmd_flags &= ~CLEM_ADB_C026_SRQ;
    }

    return irq_line;
}


void clem_adb_device_input(
  struct ClemensDeviceADB* adb,
  const struct ClemensInputEvent* input
) {
    //  events are sent to our ADB 'microcontroller'
    //      keyboard events are queued up for buffering by the microcontroller
    //      and picking by the host.
    //      mouse events are polled
    //
    unsigned key_index = input->value & 0x7f;
    switch (input->type) {
        case kClemensInputType_KeyDown:
            CLEM_LOG("Key Dn: %02X", input->value);
            if (!adb->keyb.states[key_index]) {
                _clem_adb_glu_queue_key(adb, key_index);
                adb->keyb.states[key_index] = 1;
            }
            break;
        case kClemensInputType_KeyUp:
            CLEM_LOG("Key Up: %02X", input->value);
            if (adb->keyb.states[key_index]) {
                _clem_adb_glu_queue_key(adb, 0x80 | key_index);
                adb->keyb.states[key_index] = 0;
            }
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
    unsigned device_command = 0;
    unsigned device_address = 0;
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
            device_command = value & 0xf0;
            device_address = value & 0x0f;
            break;
    }
    switch (device_command) {
        case 0:
            break;
        case CLEM_ADB_CMD_DEVICE_ENABLE_SRQ:
            CLEM_UNIMPLEMENTED("ADB Enable SRQ: %0X", device_address);
            break;

        case CLEM_ADB_CMD_DEVICE_FLUSH:
            CLEM_UNIMPLEMENTED("ADB Flush: %0X", device_address);
            break;

        case CLEM_ADB_CMD_DEVICE_DISABLE_SRQ:
            _clem_adb_expect_data(adb, CLEM_ADB_STATE_CMD_DATA, 0);
            break;

        case CLEM_ADB_CMD_DEVICE_TRANSMIT_2:
            CLEM_UNIMPLEMENTED("ADB Transmit: %0X", device_address);
            break;

        case CLEM_ADB_CMD_DEVICE_POLL_0:
            CLEM_UNIMPLEMENTED("ADB Poll 0: %0X", device_address);
            break;

        case CLEM_ADB_CMD_DEVICE_POLL_1:
            CLEM_UNIMPLEMENTED("ADB Poll 1: %0X", device_address);
            break;

        case CLEM_ADB_CMD_DEVICE_POLL_2:
            CLEM_UNIMPLEMENTED("ADB Poll 2: %0X", device_address);
            break;

        case CLEM_ADB_CMD_DEVICE_POLL_3:
            CLEM_UNIMPLEMENTED("ADB Poll 3: %0X", device_address);
            break;

        default:                /* unimplemented? */
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
        case CLEM_MMIO_REG_ANYKEY_STROBE:
            /* always clear strobe bit */
            adb->io_key_last_ascii &= ~0x80;
            break;
        case CLEM_MMIO_REG_ADB_MODKEY:
            CLEM_LOG("IO Write %02X", ioreg);
            break;
        case CLEM_MMIO_REG_ADB_STATUS:
            /* TODO: support enabling mouse interrupts when mouse data reg is
                     filled
               TODO: Throw a warning if keyboard data interrupt enabled - not
                     supported according to docs
               TODO: support command data interrupts
            */
            break;
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

static uint8_t _clem_adb_read_modkeys(struct ClemensDeviceADB* adb) {
    uint8_t modkeys = 0;
    if (adb->keyb_reg[2] & CLEM_ADB_KEY_MOD_APPLE) {
        modkeys |= 0x80;
    }
    if (adb->keyb_reg[2] & CLEM_ADB_KEY_MOD_OPTION) {
        modkeys |= 0x40;
    }
    if (adb->is_keypad_down) {
        modkeys |= 0x10;
    }
    if (adb->keyb_reg[2] & CLEM_ADB_KEY_CAPSLOCK) {
        modkeys |= 0x04;
    }
    if (adb->keyb_reg[2] & CLEM_ADB_KEY_MOD_CTRL) {
        modkeys |= 0x02;
    }
    if (adb->keyb_reg[2] & CLEM_ADB_KEY_MOD_SHIFT) {
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

uint8_t clem_adb_read_switch(
    struct ClemensDeviceADB* adb,
    uint8_t ioreg,
    uint8_t flags
) {
    bool is_noop = (flags & CLEM_MMIO_READ_NO_OP) != 0;
    uint8_t tmp;

    switch (ioreg) {
        case CLEM_MMIO_REG_KEYB_READ:
            /* FIXME: HWRef says this is cleared when reading here */
            adb->cmd_status &= ~CLEM_ADB_C027_KEY_FULL;
            return adb->io_key_last_ascii;
            break;
        case CLEM_MMIO_REG_ANYKEY_STROBE:
            /* clear strobe bit and return any-key status */
            adb->io_key_last_ascii &= ~0x80;
            return (adb->is_asciikey_down ? 0x80 : 0x00) |
                   (adb->io_key_last_ascii & 0x7f);
            break;
        case CLEM_MMIO_REG_ADB_MODKEY:
            if (adb->keyb_reg[2] & CLEM_ADB_KEY_MOD_APPLE) {
                return _clem_adb_read_modkeys(adb);
            }
            break;
        case CLEM_MMIO_REG_ADB_CMD_DATA:
            return _clem_adb_read_cmd(adb, flags);
        case CLEM_MMIO_REG_ADB_STATUS:
            /* FIXME: HWRef docs are vague here - which bits are actually
               cleared? eyboard data bit only according to the HWRef.  But FW
               Ref says 'never use, won't work' for the keyboard bits here...
            */
            tmp = adb->cmd_status;
            adb->cmd_status &= ~CLEM_ADB_C027_KEY_FULL;
            return tmp;
        case CLEM_MMIO_REG_BTN0:
            if (adb->keyb_reg[2] & CLEM_ADB_GLU_REG2_KEY_APPLE) {
                return 0x80;
            }
            break;
        case CLEM_MMIO_REG_BTN1:
              if (adb->keyb_reg[2] & CLEM_ADB_GLU_REG2_KEY_OPTION) {
                return 0x80;
            }
            break;
        default:
            if (!CLEM_IS_MMIO_READ_NO_OP(flags)) {
                CLEM_WARN("Unimplemented ADB read %02X", ioreg);
            }
            break;
    }
    return 0;
}
