#include "clem_debug.h"
#include "clem_device.h"
#include "clem_mmio_defs.h"
#include "clem_util.h"

#include <math.h>
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
 *  a square wave.
 */

#define CLEM_AUDIO_CTL_BUSY         0x80
#define CLEM_AUDIO_CTL_ACCESS_RAM   0x40
#define CLEM_AUDIO_CTL_AUTO_ADDRESS 0x20
#define CLEM_AUDIO_CTL_VOLUME_MASK  0x07

#define CLEM_AUDIO_SAMPLE_AMPLITUDE_SCALAR 0.75f

/* integer only multiply by the ratio of 102300/89489 (1023khz/894.886khz) */
#define CLEM_ENSONIQ_CLOCKS_PER_CYCLE (CLEM_CLOCKS_PHI0_CYCLE * 1023000U / 894886U)

/*
  Ensoniq 5503 DOC emulation.

  The CPU reads and writes control and data instructions to the DOC via the IO
  registers mentioned at the top of this file.  Specifics relating to how data
  I/O is handled (i.e. to and from sound RAM, registers, etc) involves the sound
  GLU section in the Apple IIgs Hardware Reference Manual as mentioned
  implemented later in this file.

  Below are details as to how the DOC is controlled and how it generates its
  final output to the system mixer.  Unlike the Mockingboard, the Ensoniq
  uses wavetables in sound RAM referenced by the DOC.

  The DOC controls 32 oscillators that generate pointers into these wavetable.
  Pointer generation per osciallator is controlled by a set of registers per
  7mhz cycle in succession (in hardware this is necessary since only one
  oscillator at a time can read from sound RAM.)  _sync_ will run through
  each oscillator accordingly and add 2 cycles to account for hardware
  requirements before running through the active oscillator set again.

  The scan rate (number of iterations per second) is 894.88625 Khz / (OSC + 2),
  and accordingly relies on the number of active oscillators.  Per oscillator,
  _sync_ budgets (CLEM_CLOCKS_PHI0_CYCLE * 1023 Khz/894.88625 Khz) clocks.

  Registers:
    FC (0x00 : 0x20, 0x01 : 0x21, ...) define a 16-bit LE increment added to
      the osciallator's Accumulator (A) per cycle as described above

    VOL (0x40, 0x41, ...) define an 8-bit scalar to amplify the

    DATA (0x60, 0x61, ...) current byte read from the wavetable for the
      corresponding oscillator

    ADRP (0x80, 0x81, ...) page number marking the start of the wavetable for
      the corresponding oscillator

    CTRL (0xA0, 0xA1, ...) multi-purpose control register handling channel
      assignment, interrupt enabling and run mode for an osciallator

    TABL (0xC0, 0xC1, ...) defines table size where n is the value and 2^(8+n)
      is the size.  Also defined is the resolution of the oscillator's
      accumulator used when calculating the final address

    OIR (0xE0) identifies the osciallator that triggers an interrupt typically
      read by an IRQ handler

    OENBL (0xE1) enables up to 32 osciallators * 2 (i.e. a value of 64 == 32
      oscillators)

    ATOD (0xE2) input analog signal converted to digital.  A read reads the
      current value, and starts conversion of the next value.  A subsequent
      read will either start a new conversion or return the result of the last
      value.  This is because this read triggers a 26 cycle conversion process
      and the final value will not be available until complete.

  References:
  https://ia600407.us.archive.org/8/items/cortland_manual_set/v4_13_EnsoniqDOC.pdf
*/

#define CLEM_ENSONIQ_OSC_LIMIT        32
#define CLEM_ENSONIQ_REG_OSC_OIR_MASK 0xbe // ~(01000001) are always on and unchanged

static uint16_t s_ensoniq_ptr_bits_mask[8] = {0xff00, 0xfe00, 0xfc00, 0xf800,
                                              0xf000, 0xe000, 0xc000, 0x8000};

void clem_ensoniq_reset(struct ClemensDeviceEnsoniq *doc) {
    doc->address = 0;
    doc->data_reg = 0;
    doc->dt_budget = 0;
    doc->cycle = 0;
    doc->addr_auto_inc = false;
    doc->is_access_ram = false;
    doc->is_busy = false;

    memset(doc->reg, 0, sizeof(doc->reg));
    memset(doc->acc, 0, sizeof(doc->acc));
    memset(doc->ptr, 0, sizeof(doc->ptr));
    memset(doc->osc_flags, 0, sizeof(doc->osc_flags));
    memset(doc->osc_stack, 0, sizeof(doc->osc_stack));

    // ensures no interrupt triggered
    doc->reg[CLEM_ENSONIQ_REG_OSC_OIR] = 0xff;
    // 1 oscillator x 2 at minimum enabled
    doc->reg[CLEM_ENSONIQ_REG_OSC_ENABLE] = 0;
    // unsigned wave, so 0x80 == 0 signed
    doc->reg[CLEM_ENSONIQ_REG_OSC_ADC] = 0x80;
}

static void _clem_ensoniq_set_irq(struct ClemensDeviceEnsoniq *doc, unsigned osc_index) {
    unsigned stack_index;
    //  Add to stack and populate OIR with bottom of stack (in most cases the oscillator
    //  passed here will be the only one to populate the OIR, so the queue may seem
    //  inefficient.)
    //  Edge cases include oscillators already on the stack, which will be a no-op.
    //
    doc->osc_flags[osc_index] &= ~CLEM_ENSONIQ_OSC_FLAG_CYCLE;
    if (!(doc->osc_flags[osc_index] & CLEM_ENSONIQ_OSC_FLAG_OIR)) {
        doc->osc_flags[osc_index] |= CLEM_ENSONIQ_OSC_FLAG_OIR;
        for (stack_index = 0; stack_index < 32; ++stack_index) {
            if (doc->osc_stack[stack_index] & 0x80)
                continue;
            break;
        }
        doc->osc_stack[stack_index] = osc_index | 0x80;
    }
    if (doc->reg[CLEM_ENSONIQ_REG_OSC_OIR] & 0x80) {
        osc_index = doc->osc_stack[0] & 0x7f;
        doc->reg[CLEM_ENSONIQ_REG_OSC_OIR] &= ~CLEM_ENSONIQ_REG_OSC_OIR_MASK;
        doc->reg[CLEM_ENSONIQ_REG_OSC_OIR] |= ((uint8_t)(osc_index << 1) & 0x3e);
    }
}

static void _clem_ensoniq_next_irq(struct ClemensDeviceEnsoniq *doc) {
    //  called when OIR is read by the application
    unsigned osc_index;
    unsigned stack_index;
    // bottom of stack is the current IRQ
    osc_index = doc->osc_stack[0];
    if (osc_index & 0x80) {
        //  clear out its OIR status
        osc_index &= 0x7f;
        doc->osc_flags[osc_index] &= ~CLEM_ENSONIQ_OSC_FLAG_OIR;
    }
    for (stack_index = 0; (doc->osc_stack[stack_index] & 0x80) && (stack_index < 31);
         ++stack_index) {
        doc->osc_stack[stack_index] = doc->osc_stack[stack_index + 1];
    }
    osc_index = doc->osc_stack[0];
    if (osc_index & 0x80) {
        osc_index &= 0x7f;
        doc->reg[CLEM_ENSONIQ_REG_OSC_OIR] &= ~CLEM_ENSONIQ_REG_OSC_OIR_MASK;
        doc->reg[CLEM_ENSONIQ_REG_OSC_OIR] |= ((uint8_t)(osc_index << 1) & 0x3e);
    } else {
        doc->reg[CLEM_ENSONIQ_REG_OSC_OIR] = 0x80 | ~CLEM_ENSONIQ_REG_OSC_OIR_MASK;
    }
}

void _clem_ensoniq_reset_osc(struct ClemensDeviceEnsoniq *doc, unsigned osc_index) {
    doc->acc[osc_index] = 0;
    doc->ptr[osc_index] = 0;
    doc->osc_flags[osc_index] &= ~CLEM_ENSONIQ_OSC_FLAG_CYCLE;
}

static inline uint16_t _clem_ensoniq_calc_waveform_ptr(struct ClemensDeviceEnsoniq *doc,
                                                       unsigned osc_index) {

    unsigned acc = doc->acc[osc_index] & 0x00ffffff; // 24-bit
    unsigned size = ((doc->reg[CLEM_ENSONIQ_REG_OSC_SIZE + osc_index] >> 3) & 0x07);
    unsigned resolution = (doc->reg[CLEM_ENSONIQ_REG_OSC_SIZE + osc_index] & 0x07) + 1;
    uint16_t ptr = ((uint16_t)doc->reg[CLEM_ENSONIQ_REG_OSC_PTR + osc_index]) << 8;
    //  use 16-bits of the accumulator, the resolution determines *which* 16 bits
    // size = 0, use 8 bits of accumulator at ADR0-7
    //      = 1, use 9 bits of accumulator at ADR0-8
    //      etc
    acc = (acc >> resolution) & 0xffff;
    acc = (acc >> (8 - size)) & 0x7fff;
    ptr &= s_ensoniq_ptr_bits_mask[size];
    ptr |= acc;
    return ptr;
}

uint8_t clem_ensoniq_oscillator_cycle(struct ClemensDeviceEnsoniq *doc, unsigned osc_index,
                                      unsigned osc_limit, uint8_t ctl) {
    //  Data is read from sound RAM and sent to one of up to eight output channels
    //  address calculation
    //  ACC <- FREQ + ACC
    //  OFF <- ACC
    //  TODO: precalc these values when their registers change - may save a few
    //        cycles if needed
    //  page aligned pointer into sound RAM
    //  offset into the wavetable
    uint16_t ptr = _clem_ensoniq_calc_waveform_ptr(doc, osc_index);
    unsigned channel = ((ctl & 0xf0) >> 4) & 0x7;
    unsigned other_osc_index = osc_index ^ 1;
    unsigned freq_ctl = (((uint16_t)doc->reg[CLEM_ENSONIQ_REG_OSC_FCHI + osc_index]) << 8) |
                        doc->reg[CLEM_ENSONIQ_REG_OSC_FCLOW + osc_index];

    doc->acc[osc_index] = (doc->acc[osc_index] + freq_ctl) & 0x00ffffff; // next

    //  handle wraparound to start of wavetable, which triggers interrupts and
    //  changes oscillator state based on control mode (one-shot, sync, swap)
    if (ptr < doc->ptr[osc_index]) {
        doc->osc_flags[osc_index] |= CLEM_ENSONIQ_OSC_FLAG_CYCLE;
        if (ctl & CLEM_ENSONIQ_OSC_CTL_M0) {
            if (ctl & CLEM_ENSONIQ_OSC_CTL_SYNC) {
                // swap
                ctl |= CLEM_ENSONIQ_OSC_CTL_HALT;
                if (other_osc_index < osc_limit) {
                    doc->reg[CLEM_ENSONIQ_REG_OSC_CTRL + other_osc_index] &=
                        ~CLEM_ENSONIQ_OSC_CTL_HALT;
                }
            } else {
                // oneshot
                ctl |= CLEM_ENSONIQ_OSC_CTL_HALT;
            }
        } else if (ctl & CLEM_ENSONIQ_OSC_CTL_SYNC) {
            // sync mode since M0 is 0, odd osciallator will reset
            if (other_osc_index < osc_limit && (other_osc_index & 1)) {
                _clem_ensoniq_reset_osc(doc, other_osc_index);
            }
        }
    }

    doc->ptr[osc_index] = ptr;
    doc->reg[CLEM_ENSONIQ_REG_OSC_DATA + osc_index] = doc->sound_ram[ptr];
    if (!doc->reg[CLEM_ENSONIQ_REG_OSC_DATA + osc_index]) {
        ctl |= CLEM_ENSONIQ_OSC_CTL_HALT;
    }
    return ctl;
}

uint32_t clem_ensoniq_sync(struct ClemensDeviceEnsoniq *doc, clem_clocks_duration_t dt_clocks) {
    // 1 oscillator x 2 at minimum enabled - i.e. we always enable 2 by default
    unsigned osc_cnt = (doc->reg[CLEM_ENSONIQ_REG_OSC_ENABLE] >> 1) + 1;

    doc->dt_budget += dt_clocks;

    while (doc->dt_budget >= CLEM_ENSONIQ_CLOCKS_PER_CYCLE) {
        // 2 extra cycles after running through all active oscillators
        unsigned osc_cycle = doc->cycle % (osc_cnt + 2);
        if (osc_cycle < osc_cnt) {
            uint8_t ctl = doc->reg[CLEM_ENSONIQ_REG_OSC_CTRL + osc_cycle];
            if (ctl & CLEM_ENSONIQ_OSC_CTL_HALT) {
                if (ctl & CLEM_ENSONIQ_OSC_CTL_M0) {
                    //  Pg. 7 Cortland spec (M0 = HALT = 1)
                    _clem_ensoniq_reset_osc(doc, osc_cycle);
                }
            } else {
                ctl = clem_ensoniq_oscillator_cycle(doc, osc_cycle, osc_cnt, ctl);
                //  Pg. 6 Cortland spec (IE = 1, CYCLE DONE)
                if (ctl & CLEM_ENSONIQ_OSC_CTL_IE) {
                    if (doc->osc_flags[osc_cycle] & CLEM_ENSONIQ_OSC_FLAG_CYCLE) {
                        _clem_ensoniq_set_irq(doc, osc_cycle);
                    }
                }
            }
            doc->reg[CLEM_ENSONIQ_REG_OSC_CTRL + osc_cycle] = ctl;
        }

        ++doc->cycle;
        doc->dt_budget -= CLEM_ENSONIQ_CLOCKS_PER_CYCLE;
    }

    return (doc->reg[CLEM_ENSONIQ_REG_OSC_OIR] & 0x80) ? 0 : CLEM_IRQ_AUDIO_OSC;
}

unsigned clem_ensoniq_voices(struct ClemensDeviceEnsoniq *doc) {
    //  run through all enabled non-halted oscillators
    //  if the oscillator is in AM mode (sync, odd oscillator modules the lower
    //  even?) and ignore the volume setting for the oscillator
    unsigned osc_cnt = (doc->reg[CLEM_ENSONIQ_REG_OSC_ENABLE] >> 1) + 1;
    unsigned osc_max_channels = 0;
    unsigned osc_idx, voice_idx;
    for (osc_idx = 0; osc_idx < osc_cnt; ++osc_idx) {
        uint8_t volume = doc->reg[CLEM_ENSONIQ_REG_OSC_VOLUME + osc_idx];
        uint8_t ctl = doc->reg[CLEM_ENSONIQ_REG_OSC_CTRL + osc_idx];
        uint8_t channel = (ctl >> 4);
        uint8_t data = doc->reg[CLEM_ENSONIQ_REG_OSC_DATA + osc_idx];
        bool sync_mode = (ctl & CLEM_ENSONIQ_OSC_CTL_SWAP) == CLEM_ENSONIQ_OSC_CTL_SYNC;
        float level;

        //  HALT indicates an inactive oscillator, or if the oscillator had finished its
        //  waveform (which was mixed in the last frame - so just skip mixing this frame
        //  until reenabled.)
        if (ctl & CLEM_ENSONIQ_OSC_CTL_HALT)
            continue;

        if (channel >= osc_max_channels) {
            for (voice_idx = osc_max_channels; voice_idx < channel + 1; ++voice_idx) {
                doc->voice[voice_idx] = 0.0f;
            }
            osc_max_channels = channel + 1;
        }

        // no value
        if (!data)
            continue;
        //  AM mode is handled in the even oscillator
        if (sync_mode && (osc_idx & 1))
            continue;

        if ((osc_idx + 1) & 1) {
            // TODO: this can be precalculated and stored into osc_flags for the
            //       current channel during the oscillator pass
            if ((doc->reg[CLEM_ENSONIQ_REG_OSC_CTRL + osc_idx + 1] &
                 (CLEM_ENSONIQ_OSC_CTL_HALT + CLEM_ENSONIQ_OSC_CTL_SWAP)) ==
                CLEM_ENSONIQ_OSC_CTL_SYNC) {
                volume = doc->reg[CLEM_ENSONIQ_REG_OSC_DATA + osc_idx + 1];
            }
        }

        level = (2.0f * data / 255.0f) - 1.0f;
        doc->voice[channel] += level * (volume / 255.0f);
    }

    return osc_max_channels;
}

//  down convert voices output into 2 channel mono
void clem_ensoniq_mono(struct ClemensDeviceEnsoniq *doc, unsigned osc_max_channels, float *left,
                       float *right) {
    *left = 0.0f;
    *right = 0.0f;

    for (unsigned channel_idx = 0; channel_idx < osc_max_channels; ++channel_idx) {
        *left += doc->voice[channel_idx];
    }
    if (*left > 1.0f)
        *left = 1.0f;
    else if (*left < -1.0f)
        *left = -1.0f;
    *right = *left;
}

void clem_ensoniq_write_ctl(struct ClemensDeviceEnsoniq *doc, uint8_t value) {
    if (doc->is_busy) {
        CLEM_WARN("[ensoniq]: DOC busy (adr: %04X)", doc->address);
        return;
    }
    doc->is_access_ram = (value & CLEM_AUDIO_CTL_ACCESS_RAM) != 0;
    doc->addr_auto_inc = (value & CLEM_AUDIO_CTL_AUTO_ADDRESS) != 0;
}

void clem_ensoniq_write_data(struct ClemensDeviceEnsoniq *doc, uint8_t value) {
    if (doc->is_access_ram) {
        doc->sound_ram[doc->address & 0xffff] = value;
    } else {
        uint8_t oldvalue = doc->reg[doc->address & 0xff];
        uint8_t reg_idx = doc->address & 0xff;
        /* TODO: write to specific registers that require special handling */
        switch (reg_idx) {
        case CLEM_ENSONIQ_REG_OSC_OIR:
            // appears to be a NOP (no mention of writing to E0 in the cortland docs,
            // and having apps write the IRQ status seems dangerous for hardware to allow)
            CLEM_LOG("DOC: Ignoring direct write to OIR %02x (cur: %02X)", value, oldvalue);
            break;
        case CLEM_ENSONIQ_REG_OSC_ENABLE:
            if (value > 64) {
                CLEM_LOG("DOC: OSC Enable set a value > expected maximum 64 (%02x)", value);
                value &= 0x7f;
            }
            doc->reg[reg_idx] = value;
            break;
        case CLEM_ENSONIQ_REG_OSC_ADC:
            // should be a no-op
            break;
        default:
            doc->reg[reg_idx] = value;
            if (reg_idx >= 0xa0 && reg_idx < 0xc0) {
                if ((oldvalue ^ value) & CLEM_ENSONIQ_OSC_CTL_HALT) {
                    if (oldvalue & CLEM_ENSONIQ_OSC_CTL_HALT) {
                        //  switching oscillator on
                        doc->ptr[reg_idx - 0xa0] =
                            _clem_ensoniq_calc_waveform_ptr(doc, reg_idx - 0xa0);
                    }
                }
            }
            break;
        }
    }
    doc->data_reg = value;
    if (doc->addr_auto_inc) {
        ++doc->address;
    }
}

uint8_t clem_ensoniq_read_ctl(struct ClemensDeviceEnsoniq *doc, uint8_t flags) {
    uint8_t result = 0x00;
    if (doc->is_busy) {
        result |= CLEM_AUDIO_CTL_BUSY;
    }
    if (doc->is_access_ram) {
        result |= CLEM_AUDIO_CTL_ACCESS_RAM;
    }
    if (doc->addr_auto_inc) {
        result |= CLEM_AUDIO_CTL_AUTO_ADDRESS;
    }
    return result;
}

uint8_t clem_ensoniq_read_data(struct ClemensDeviceEnsoniq *doc, uint8_t flags) {
    uint8_t result = doc->data_reg;
    /* refer to HW Ref Chapter 5 - p 107, Read operation,
        basically reads lag by one cycle.  It's uncertain to me whether this
        is just for RAM as the hardware ref says, versus all read accesses
        to registers as well.  Note: it appears when we read the interrupt
        register on the DOC, it's read twice.
    */
    if (CLEM_IS_IO_NO_OP(flags))
        return result;

    if (doc->is_access_ram) {
        doc->data_reg = doc->sound_ram[doc->address & 0xffff];
    } else {
        doc->data_reg = doc->reg[doc->address & 0xff];
    }

    if (!doc->is_access_ram) {
        switch (doc->address & 0xff) {
        case CLEM_ENSONIQ_REG_OSC_OIR:
            //  retrieve next IRQ or clear OIR
            _clem_ensoniq_next_irq(doc);
            break;
        default:
            break;
        }
    }

    if (doc->addr_auto_inc) {
        ++doc->address;
    }

    return result;
}

void clem_sound_reset(struct ClemensDeviceAudio *glu) {
    /* some GLU reset */

    clem_ensoniq_reset(&glu->doc);

    glu->a2_speaker = false;
    glu->a2_speaker_tense = false;
    glu->a2_speaker_frame_count = -1;
    glu->a2_speaker_frame_threshold = glu->mix_buffer.frames_per_second / 20;
    glu->a2_speaker_level = 0.0f;

    /* other config - i.e. test tone */
    glu->tone_frequency = 0;
    glu->irq_line = 0;

    /* mix buffer reset */
    glu->dt_mix_frame = 0;
    if (glu->mix_buffer.frames_per_second > 0) {
        glu->dt_mix_sample = (CLEM_CLOCKS_PHI0_CYCLE * CLEM_MEGA2_CYCLES_PER_SECOND) /
                             (glu->mix_buffer.frames_per_second);
        glu->tone_frame_delta =
            (glu->tone_frequency * CLEM_PI_2) / glu->mix_buffer.frames_per_second;
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

void clem_sound_consume_frames(struct ClemensDeviceAudio *glu, unsigned consumed) {
    if (consumed > glu->mix_frame_index) {
        consumed = glu->mix_frame_index;
    }
    if (consumed < glu->mix_frame_index) {
        memcpy(glu->mix_buffer.data, glu->mix_buffer.data + consumed * glu->mix_buffer.stride,
               (glu->mix_frame_index - consumed) * glu->mix_buffer.stride);
    }
    glu->mix_frame_index -= consumed;
}

void _clem_sound_do_tone(struct ClemensDeviceAudio *glu, float *samples) {
    float mag = sinf(glu->tone_theta);
    samples[0] = mag;
    samples[1] = mag;
    glu->tone_theta += glu->tone_frame_delta;
    if (glu->tone_theta >= CLEM_PI_2) {
        glu->tone_theta -= CLEM_PI_2;
    }
}

void clem_sound_glu_sync(struct ClemensDeviceAudio *glu, struct ClemensClock *clocks) {
    clem_clocks_duration_t dt_clocks = clocks->ts - glu->ts_last_frame;

    glu->irq_line = clem_ensoniq_sync(&glu->doc, dt_clocks);

    glu->dt_mix_frame += dt_clocks;

    if (glu->dt_mix_sample > 0) {
        unsigned delta_frames = (glu->dt_mix_frame / glu->dt_mix_sample);
        if (delta_frames > 0) {
            uint8_t *mix_out = glu->mix_buffer.data;
            // note we only support 2 channels max output
            float doc_out[2];
            //  TODO: stereo
            unsigned ensoniq_voice_cnt = clem_ensoniq_voices(&glu->doc);
            clem_ensoniq_mono(&glu->doc, ensoniq_voice_cnt, &doc_out[0], &doc_out[1]);
            for (unsigned i = 0; i < delta_frames; ++i) {
                unsigned frame_index = (glu->mix_frame_index + i) % glu->mix_buffer.frame_count;
                float *samples = (float *)(&mix_out[frame_index * glu->mix_buffer.stride]);
                /* test tone support */
                if (glu->tone_frame_delta > 0) {
                    _clem_sound_do_tone(glu, samples);
                }
                if (glu->a2_speaker_frame_count >= 0) {
                    glu->a2_speaker_frame_count += delta_frames;
                }
                if (glu->a2_speaker_frame_count > glu->a2_speaker_frame_threshold) {
                    glu->a2_speaker_frame_count = -1;
                    glu->a2_speaker_level = 0.0f;
                }
                if (glu->a2_speaker) {
                    /* click! - two speaker pulses = 1 complete wave */
                    glu->a2_speaker_frame_count = 0;
                    if (!glu->a2_speaker_tense) {
                        glu->a2_speaker_level = 0.75f;
                    } else {
                        glu->a2_speaker_level = -0.75f;
                    }
                    glu->a2_speaker_tense = !glu->a2_speaker_tense;
                    glu->a2_speaker = false;
                }
                // TODO: stereo DOC
                samples[0] = CLEM_AUDIO_SAMPLE_AMPLITUDE_SCALAR *
                             ((doc_out[0] + glu->a2_speaker_level) * glu->volume / 15.0f);
                if (samples[0] > 1.0f)
                    samples[0] = 1.0f;
                else if (samples[0] < -1.0f)
                    samples[0] = -1.0f;
                samples[1] = CLEM_AUDIO_SAMPLE_AMPLITUDE_SCALAR *
                             ((doc_out[1] + glu->a2_speaker_level) * glu->volume / 15.0f);
                if (samples[1] > 1.0f)
                    samples[1] = 1.0f;
                else if (samples[1] < -1.0f)
                    samples[1] = -1.0f;
            }
            glu->mix_frame_index =
                (glu->mix_frame_index + delta_frames) % (glu->mix_buffer.frame_count);
            glu->dt_mix_frame = glu->dt_mix_frame % glu->dt_mix_sample;
#if CLEM_AUDIO_DIAGNOSTICS
            glu->diag_delta_frames += delta_frames;
#endif
        }
    }

#if CLEM_AUDIO_DIAGNOSTICS
    glu->diag_dt_ns += clem_calc_ns_step_from_clocks(dt_clocks);
    glu->diag_dt += dt_clocks;
    if (glu->diag_dt_ns >= CLEM_1SEC_NS) {
        float scalar = ((float)CLEM_1SEC_NS) / glu->diag_dt_ns;
        printf("clem_audio: %.01f frames/sec (dt = %u clocks)\n", scalar * glu->diag_delta_frames,
               glu->diag_dt);
        glu->diag_delta_frames = 0;
        glu->diag_dt = 0;
        glu->diag_dt_ns = 0;
    }
#endif

    glu->ts_last_frame = clocks->ts;
}

void clem_sound_write_switch(struct ClemensDeviceAudio *glu, uint8_t ioreg, uint8_t value) {
    switch (ioreg) {
    case CLEM_MMIO_REG_AUDIO_CTL:
        clem_ensoniq_write_ctl(&glu->doc, value);
        glu->volume = (value & CLEM_AUDIO_CTL_VOLUME_MASK);
        break;
    case CLEM_MMIO_REG_AUDIO_DATA:
        clem_ensoniq_write_data(&glu->doc, value);
        break;
    case CLEM_MMIO_REG_AUDIO_ADRLO:
        glu->doc.address &= 0xff00;
        glu->doc.address |= value;
        break;
    case CLEM_MMIO_REG_AUDIO_ADRHI:
        glu->doc.address &= 0x00ff;
        glu->doc.address |= ((unsigned)(value) << 8);
        break;
    case CLEM_MMIO_REG_SPKR:
        glu->a2_speaker = !glu->a2_speaker;
        break;
    }
}

uint8_t clem_sound_read_switch(struct ClemensDeviceAudio *glu, uint8_t ioreg, uint8_t flags) {
    uint8_t result = 0x00;
    switch (ioreg) {
    case CLEM_MMIO_REG_AUDIO_CTL:
        result = clem_ensoniq_read_ctl(&glu->doc, flags);
        result |= glu->volume;
        break;
    case CLEM_MMIO_REG_AUDIO_DATA:
        result = clem_ensoniq_read_data(&glu->doc, flags);
        break;
    case CLEM_MMIO_REG_AUDIO_ADRLO:
        result = (uint8_t)(glu->doc.address & 0x00ff);
        break;
    case CLEM_MMIO_REG_AUDIO_ADRHI:
        result = (uint8_t)((glu->doc.address >> 8) & 0x00ff);
        break;
    case CLEM_MMIO_REG_SPKR:
        if (!CLEM_IS_IO_NO_OP(flags)) {
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
