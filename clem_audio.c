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

#define CLEM_AUDIO_CTL_BUSY         0x80
#define CLEM_AUDIO_CTL_ACCESS_RAM   0x40
#define CLEM_AUDIO_CTL_AUTO_ADDRESS 0x20
#define CLEM_AUDIO_CTL_VOLUME_MASK  0x07

void clem_sound_reset(struct ClemensDeviceAudio* glu) {
    memset(glu, 0, sizeof(*glu));

    /* indicates IRQB line, so no interrupt */;
    glu->doc_reg[0xe0] = 0x80;
}

void clem_sound_glu_sync(struct ClemensDeviceAudio* glu, uint32_t delta_us) {

}

void clem_sound_write_switch(
    struct ClemensDeviceAudio* glu,
    uint8_t ioreg,
    uint8_t value
) {
    if (glu->is_busy) {
        CLEM_WARN("SOUND GLU WRITE BUSY");
        return;
    }
    switch (ioreg) {
        case CLEM_MMIO_REG_AUDIO_CTL:
            glu->is_access_ram = (value & CLEM_AUDIO_CTL_ACCESS_RAM) != 0;
            glu->addr_auto_inc = (value & CLEM_AUDIO_CTL_AUTO_ADDRESS) != 0;
            glu->volume = (value & CLEM_AUDIO_CTL_VOLUME_MASK);
            break;
        case CLEM_MMIO_REG_AUDIO_DATA:
            glu->ram_read_cntr = 0;
            if (glu->is_access_ram) {
                glu->sound_ram[glu->address & 0xffff] = value;
            } else {
                /* TODO: write to register */
            }
            if (glu->addr_auto_inc) {
                ++glu->address;
            }
            break;
        case CLEM_MMIO_REG_AUDIO_ADRLO:
            glu->address &= 0xff00;
            glu->address |= value;
            glu->ram_read_cntr = 0;
            break;
        case CLEM_MMIO_REG_AUDIO_ADRHI:
            glu->address &= 0x00ff;
            glu->address |= ((unsigned)(value) << 8);
            glu->ram_read_cntr = 0;
            break;
    }
}

uint8_t clem_sound_read_switch(
    struct ClemensDeviceAudio* glu,
    uint8_t ioreg,
    uint8_t flags
) {
    uint8_t result = 0x00;
    switch (ioreg) {
        case CLEM_MMIO_REG_AUDIO_CTL:
            if (glu->is_busy) {
                result |= CLEM_AUDIO_CTL_BUSY;
            }
            if (glu->is_access_ram) {
                result |= CLEM_AUDIO_CTL_ACCESS_RAM;
            }
            if (glu->addr_auto_inc) {
                result |= CLEM_AUDIO_CTL_AUTO_ADDRESS;
            }
            result |= glu->volume;
            break;
        case CLEM_MMIO_REG_AUDIO_DATA:
            if (!CLEM_IS_MMIO_READ_NO_OP(flags)) {
                /* refer to HW Ref Chapter 5 - p 107, Read operation,
                   first time read operations upon changing the address
                   require double reads - likely for some kind of switch
                   over/state reset on the hardware.  Will have to evalaute
                   how firmware and ROM handles this (i.e. it appears
                   when we read the interrupt register on the DOC, it's read
                   twice, following the convention here)
                */
                if (!glu->is_access_ram) {
                    /* do not apply this funky read logic to DOC register
                       reads to be safe - even though the ROM implies double
                       reading on address change is required.
                    */
                   result = glu->doc_reg[glu->address & 0xff];
                }
                if (glu->ram_read_cntr > 0) {
                    if (glu->is_access_ram) {
                        result = glu->sound_ram[glu->address & 0xffff];
                    }
                    if (glu->addr_auto_inc) {
                        ++glu->address;
                    }
                }

                ++glu->ram_read_cntr;
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
