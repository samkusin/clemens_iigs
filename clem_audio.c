#include "clem_device.h"
#include "clem_mmio_defs.h"

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
    switch (ioreg) {
        case CLEM_MMIO_REG_AUDIO_CTL:
            break;
        case CLEM_MMIO_REG_AUDIO_DATA:
            break;
        case CLEM_MMIO_REG_AUDIO_ADRLO:
            break;
        case CLEM_MMIO_REG_AUDIO_ADRHI:
            break;
    }
}

uint8_t clem_adb_read_switch(
    struct ClemensDeviceAudio* glu,
    uint8_t ioreg,
    uint8_t flags
) {
    uint8_t result;
    switch (ioreg) {
        case CLEM_MMIO_REG_AUDIO_CTL:
            break;
        case CLEM_MMIO_REG_AUDIO_DATA:
            break;
        case CLEM_MMIO_REG_AUDIO_ADRLO:
            break;
        case CLEM_MMIO_REG_AUDIO_ADRHI:
            break;
        default:
            result = 0x00;
            break;
    }
    return result;
}
