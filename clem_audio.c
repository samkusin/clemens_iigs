#include "clem_device.h"
#include "clem_debug.h"
#include "clem_mmio_defs.h"
#include "clem_util.h"

#include <string.h>
#include <math.h>

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
    /* some GLU reset */
    glu->address = 0;
    glu->ram_read_cntr = 0;

    memset(glu->doc_reg, 0, sizeof(glu->doc_reg));
    glu->addr_auto_inc = false;
    glu->is_access_ram = false;
    glu->is_busy = false;

    glu->a2_speaker = false;
    glu->a2_speaker_tense = false;

    /* indicates IRQB line, so no interrupt */;
    glu->doc_reg[0xe0] = 0x80;

    /* other config - i.e. test tone */
    glu->tone_frequency = 0;

    /* mix buffer reset */
    glu->dt_mix_frame = 0;
    if (glu->mix_buffer.frames_per_second > 0) {
        glu->dt_mix_sample = (CLEM_CLOCKS_MEGA2_CYCLE * CLEM_MEGA2_CYCLES_PER_SECOND) / (
            glu->mix_buffer.frames_per_second);
        glu->tone_frame_delta = (glu->tone_frequency * CLEM_PI_2) / glu->mix_buffer.frames_per_second;
    } else {
        glu->dt_mix_sample = 0;
        glu->tone_frame_delta = 0;
    }
    glu->tone_theta = 0.0f;

#if CLEM_AUDIO_DIAGNOSTICS
    glu->diag_dt_ns = 0;
    glu->diag_delta_frames = 0;
#endif
}

void clem_sound_consume_frames(
    struct ClemensDeviceAudio* glu,
    unsigned consumed
) {
    if (consumed > glu->mix_frame_index) {
        consumed = glu->mix_frame_index;
    }
    if (consumed < glu->mix_frame_index) {
        memcpy(glu->mix_buffer.data,
                glu->mix_buffer.data + consumed * glu->mix_buffer.stride,
                (glu->mix_frame_index - consumed) * glu->mix_buffer.stride);
    }
    glu->mix_frame_index -= consumed;
}

void _clem_sound_do_tone(
    struct ClemensDeviceAudio* glu,
    uint16_t* samples
) {
    float mag = sinf(glu->tone_theta);
    samples[0] = (uint16_t)((mag + 1.0f) * 32767);
    samples[1] = (uint16_t)((mag + 1.0f) * 32767);
    glu->tone_theta += glu->tone_frame_delta;
    if (glu->tone_theta >= CLEM_PI_2) {
        glu->tone_theta -= CLEM_PI_2;
    }
}

void clem_sound_glu_sync(
    struct ClemensDeviceAudio* glu,
    struct ClemensClock* clocks
) {
    clem_clocks_duration_t dt_clocks = clocks->ts - glu->ts_last_frame;
    if (glu->dt_mix_sample > 0) {
        unsigned delta_frames = (glu->dt_mix_frame / glu->dt_mix_sample);
        if (delta_frames > 0) {
            uint8_t* mix_out = glu->mix_buffer.data;
            for (unsigned i = 0; i < delta_frames; ++i) {
                unsigned frame_index = (glu->mix_frame_index + i) % glu->mix_buffer.frame_count;
                uint16_t* samples = (uint16_t*)(&mix_out[frame_index * glu->mix_buffer.stride]);
                /* test tone support */
                samples[0] = 32767;
                samples[1] = 32767;

                if (glu->tone_frame_delta > 0) {
                    _clem_sound_do_tone(glu, samples);
                }
                if (glu->a2_speaker) {
                    /* click! - two speaker pulses = 1 complete wave */
                    if (!glu->a2_speaker_tense) {
                        samples[0] = 32767 + (uint16_t)(24576 * glu->volume/15);
                        samples[1] = 32767 + (uint16_t)(24576 * glu->volume/15);
                    } else {
                        samples[0] = 32767 - (uint16_t)(24576 * glu->volume/15);
                        samples[1] = 32767 - (uint16_t)(24576 * glu->volume/15);
                    }
                    glu->a2_speaker_tense = !glu->a2_speaker_tense;
                    glu->a2_speaker = false;
                }
            }
            glu->mix_frame_index = (glu->mix_frame_index + delta_frames) % (
                glu->mix_buffer.frame_count);
            glu->dt_mix_frame = glu->dt_mix_frame % glu->dt_mix_sample;

        #if CLEM_AUDIO_DIAGNOSTICS
            glu->diag_delta_frames += delta_frames;
        #endif
        }
    }
    glu->dt_mix_frame += dt_clocks;

#if CLEM_AUDIO_DIAGNOSTICS
    glu->diag_dt_ns += _clem_calc_ns_step_from_clocks(dt_clocks, clocks->ref_step);
    glu->diag_dt += dt_clocks;
    if (glu->diag_dt_ns >= CLEM_1SEC_NS) {
        float scalar = ((float)CLEM_1SEC_NS) / glu->diag_dt_ns;
        printf("clem_audio: %.01f frames/sec (dt = %u clocks)\n",
            scalar * glu->diag_delta_frames,
            glu->diag_dt);
        glu->diag_delta_frames = 0;
        glu->diag_dt = 0;
        glu->diag_dt_ns = 0;
    }
#endif

    glu->ts_last_frame = clocks->ts;
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
        case CLEM_MMIO_REG_SPKR:
            glu->a2_speaker = !glu->a2_speaker;
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
            if (!CLEM_IS_IO_READ_NO_OP(flags)) {
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
            if (!CLEM_IS_IO_READ_NO_OP(flags)) {
                glu->a2_speaker = !glu->a2_speaker;
            }
            result = 0x00;
            break;
        default:
            result = 0x00;
            break;
    }
    return result;
}
