#include "clem_device.h"
#include "clem_debug.h"
#include "clem_mmio_defs.h"

#include <string.h>

/**
 * @brief Sound GLU emulation
 *
 * Interface to the GLU from the emulator uses three registers:
 *
 * - Control Register
 * - Data Register
 * - Lo/Hi Address Registers
 *
 * There are two destinations for data - the DOC and Sound RAM
 * Address Register supports auto increment
 * Sending data requires setting the Address Register and Data register
 * Reading data requires setting the Address Register loading from the data
 *  register once to 'prime' the GLU, and N more times to read N bytes of data
 *
 * IO register addresses are:
 *  Control: $C03C
 *  Data: $C03D
 *  Address: $C03E,F
 *
 * Also support the old speaker click register:
 *  Speaker Toggle: $C030
 *  Timing is important here as there must be some delay between toggle on/off
 *  to produce a click.  Performing this switch multiple times will generate
 *  a sound waveform (squarish).
 */

#define CLEM_AUDIO_CTL_READ_MODE    0x00000100
#define CLEM_AUDIO_CTL_C03C_MASK    0x000000ff
#define CLEM_AUDIO_CTL_WRITE_MASK   0x0000007f
#define CLEM_AUDIO_CTL_BUSY         0x00000080
#define CLEM_AUDIO_CTL_ACCESS_RAM   0x00000040
#define CLEM_AUDIO_CTL_AUTO_ADDRESS 0x00000020
#define CLEM_AUDIO_CTL_VOLUME_MASK  0x00000007

void clem_sound_reset(struct ClemensDeviceAudio* glu) {
    memset(glu, 0, sizeof(*glu));
}

void clem_sound_glu_sync(struct ClemensDeviceAudio* glu, uint32_t delta_us) {

}

void clem_sound_write_switch(
    struct ClemensDeviceAudio* glu,
    uint8_t ioreg,
    uint8_t value
) {
    if (glu->status & CLEM_AUDIO_CTL_BUSY) {
        CLEM_WARN("SOUND GLU WRITE BUSY");
        return;
    }
    switch (ioreg) {
        case CLEM_MMIO_REG_AUDIO_CTL:
            glu->status |= (value & CLEM_AUDIO_CTL_WRITE_MASK);
            break;
        case CLEM_MMIO_REG_AUDIO_DATA:
            if (glu->status & CLEM_AUDIO_CTL_READ_MODE) {
                glu->status &= ~CLEM_AUDIO_CTL_READ_MODE;
            }
            if (glu->status & CLEM_AUDIO_CTL_ACCESS_RAM) {
                glu->sound_ram[glu->address & 0xffff] = value;
            } else {

            }
            if (glu->status & CLEM_AUDIO_CTL_AUTO_ADDRESS) {
                ++glu->address;
            }
            break;
        case CLEM_MMIO_REG_AUDIO_ADRLO:
            glu->address &= 0xff00;
            glu->address |= value;
            break;
        case CLEM_MMIO_REG_AUDIO_ADRHI:
            glu->address &= 0x00ff;
            glu->address |= ((unsigned)(value) << 8);
            break;
    }
}

uint8_t clem_sound_read_switch(
    struct ClemensDeviceAudio* glu,
    uint8_t ioreg,
    uint8_t flags
) {
    uint8_t result;
    switch (ioreg) {
        case CLEM_MMIO_REG_AUDIO_CTL:
            result = (uint8_t)(glu->status & CLEM_AUDIO_CTL_C03C_MASK);
            break;
        case CLEM_MMIO_REG_AUDIO_DATA:
            if (!CLEM_IS_MMIO_READ_NO_OP(flags)) {
                if (!(glu->status & CLEM_AUDIO_CTL_READ_MODE)) {
                    glu->status |= CLEM_AUDIO_CTL_READ_MODE;
                    result = 0x00;
                } else {
                    if (glu->status & CLEM_AUDIO_CTL_ACCESS_RAM) {
                        result = glu->sound_ram[glu->address & 0xffff];
                    } else {
                        result = 0x00;
                    }
                    if (glu->status & CLEM_AUDIO_CTL_AUTO_ADDRESS) {
                        ++glu->address;
                    }
                }
            } else {
                result = 0x00;
            }
            break;
        case CLEM_MMIO_REG_AUDIO_ADRLO:
            result = (uint8_t)(glu->address & 0x00ff);
            break;
        case CLEM_MMIO_REG_AUDIO_ADRHI:
            result = (uint8_t)((glu->address >> 8) & 0x00ff);
            break;
        case CLEM_MMIO_REG_SPKR:
            if (!CLEM_IS_MMIO_READ_NO_OP(flags)) {
            }
            result = 0x00;
            break;
        default:
            result = 0x00;
            break;
    }
    return result;
}
