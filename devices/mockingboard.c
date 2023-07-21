#include "mockingboard.h"
#include "clem_debug.h"
#include "serializer.h"

#include <float.h>
#include <math.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>

/*
  References:
  - Programming IO primer for the A2 Mockingboard
      https://www.apple2.org.za/gswv/a2zine/Docs/Mockingboard_MiniManual.html
  - AY-3-8910 Datasheet
  - 6522 MOS and Rockwell Datasheets
    https://www.princeton.edu/~mae412/HANDOUTS/Datasheets/6522.pdf
    https://github.com/deater/dos33fsprogs/blob/master/asm_routines/mockingboard_a.s
  - Resources from
      https://wiki.reactivemicro.com/Mockingboard including the schematic which
      has been very helpful interpreting how the VIA communicates with the AY3
*/
/* MB-AUDIT LOG
   Retry reset test as the reset functionality may not be working
*/

#define CLEM_VIA_6522_PORT_B       0x00
#define CLEM_VIA_6522_PORT_A       0x01
#define CLEM_VIA_6522_REG_DATA     0x00
#define CLEM_VIA_6522_REG_DDR      0x02
#define CLEM_VIA_6522_REG_TIMER1CL 0x04
#define CLEM_VIA_6522_REG_TIMER1CH 0x05
#define CLEM_VIA_6522_REG_TIMER1LL 0x06
#define CLEM_VIA_6522_REG_TIMER1LH 0x07
#define CLEM_VIA_6522_REG_TIMER2CL 0x08
#define CLEM_VIA_6522_REG_TIMER2CH 0x09
#define CLEM_VIA_6522_REG_SR       0x0a
#define CLEM_VIA_6522_REG_ACR      0x0b
#define CLEM_VIA_6522_REG_PCR      0x0c
#define CLEM_VIA_6522_REG_IRQ_IFR  0x0d
#define CLEM_VIA_6522_REG_IRQ_IER  0x0e
#define CLEM_VIA_6522_PORT_A_ALT   0x0f

#define CLEM_VIA_6522_TIMER1_ONESHOT 0x00
#define CLEM_VIA_6522_TIMER1_FREERUN 0x40
#define CLEM_VIA_6522_TIMER1_PB7     0x80

#define CLEM_VIA_6522_TIMER2_ONESHOT 0x00
#define CLEM_VIA_6522_TIMER2_PB6     0x20

#define CLEM_VIA_6522_IER_TIMER1 0x40
#define CLEM_VIA_6522_IER_TIMER2 0x20

#define CLEM_AY3_QUEUE_SIZE 64

#define CLEM_AY3_REG_A_TONE_PERIOD_FINE   0x00
#define CLEM_AY3_REG_A_TONE_PERIOD_COARSE 0x01
#define CLEM_AY3_REG_B_TONE_PERIOD_FINE   0x02
#define CLEM_AY3_REG_B_TONE_PERIOD_COARSE 0x03
#define CLEM_AY3_REG_C_TONE_PERIOD_FINE   0x04
#define CLEM_AY3_REG_C_TONE_PERIOD_COARSE 0x05
#define CLEM_AY3_REG_NOISE_PERIOD         0x06
#define CLEM_AY3_REG_ENABLE               0x07
#define CLEM_AY3_REG_A_AMPLITUDE          0x08
#define CLEM_AY3_REG_B_AMPLITUDE          0x09
#define CLEM_AY3_REG_C_AMPLITUDE          0x0a
#define CLEM_AY3_REG_ENVELOPE_COARSE      0x0b
#define CLEM_AY3_REG_ENVELOPE_FINE        0x0c
#define CLEM_AY3_REG_ENVELOPE_SHAPE       0x0d
#define CLEM_AY3_REG_IO_A                 0x0e
#define CLEM_AY3_REG_IO_B                 0x0f

#define CLEM_AY3_TONE_LEVEL_HIGH    0x80000000
#define CLEM_AY3_TONE_LEVEL_ENABLED 0x40000000
#define CLEM_AY3_TONE_NOISE_ENABLED 0x20000000

#define CLEM_AY3_AMP_VARIABLE_MODE_FLAG 0x10
#define CLEM_AY3_AMP_FIXED_LEVEL_MASK   0x0f
#define CLEM_AY3_AMP_VARIABLE_MODE_FLAG 0x10
#define CLEM_AY3_AMP_ENVELOPE_HOLD      0x01
#define CLEM_AY3_AMP_ENVELOPE_ALTERNATE 0x02
#define CLEM_AY3_AMP_ENVELOPE_ATTACK    0x04
#define CLEM_AY3_AMP_ENVELOPE_CONTINUE  0x08

//  TODO: evaluate from sources this is cribbed from KEGS
static float s_ay3_8913_ampl_factor_westcott[16] = {
    0.000f, // level[0]
    0.010f, // level[1]
    0.015f, // level[2]
    0.022f, // level[3]
    0.031f, // level[4]
    0.046f, // level[5]
    0.064f, // level[6]
    0.106f, // level[7]
    0.132f, // level[8]
    0.216f, // level[9]
    0.297f, // level[10]
    0.391f, // level[11]
    0.513f, // level[12]
    0.637f, // level[13]
    0.819f, // level[14]
    1.000f, // level[15]
};

// TODO: other interrupts

enum ClemensVIA6522TimerStatus {
    kClemensVIA6522TimerStatus_NoLatch,
    kClemensVIA6522TimerStatus_Inactive,
    kClemensVIA6522TimerStatus_LoadCounter,
    kClemensVIA6522TimerStatus_Active
};

/**
 * @brief
 *
 * The PSG here is the AY-3-891x chip (there were multiple models, the 8913
 *  seems to be one specific to the Mockingboard but functionally they are the
 *  same.)
 *
 * To remove the need for IO ports, and to keep in spec with various
 * mockingboards, we'll implement a 8913.
 *
 * For performance, audio PCM data is generated in ay3_render()
 *
 * Commands from the 6522 are queued inside ay3_update(), but AY3
 * tone/noise/envelope generation happens in ay3_render().  This ensures that
 * audio data is not generated per emulated CPU cycle.  This is possible because
 * the AY3 effectively has no output besides the speaker.
 *
 * ay3_render() renders audio from the various tone and noise channels as their
 * state is set by the queued commands referenced above.
 *
 * Since audio commands shouldn't be that frequent, we can keep the queue small
 * as long as ay3_render() is called frequently enough (i.e. even if called once
 * per second, we shouldn't be receiving many commands from the 6522 in that
 * period of time... of course we should be calling ay3_update at something like
 * 15-60fps to avoid latency)
 */

struct ClemensAY38913 {
    /* register reflection */
    uint16_t channel_tone_period[3];
    uint16_t envelope_period;
    uint8_t channel_amplitude[3];
    uint8_t noise_period;
    uint8_t enable;
    uint8_t envelope_shape;

    /* rendering event queue built by application writes to the AY3 for this
       window - consumed by clem_card_ay3_render(...).  times are offsets from
       the render_slice_start_ts.

       queue items are combination of register + value*/
    uint32_t queue[CLEM_AY3_QUEUE_SIZE];
    clem_clocks_duration_t queue_time[CLEM_AY3_QUEUE_SIZE];
    uint32_t queue_tail;

    /* reference time step per tick (set at mega2 reference step)  whicih should
       translate to 1.023mhz */
    float clock_freq_hz;
    /* bus counter to detect bdir changes */
    uint8_t bus_control;
    /* Current register ID latched for read/write */
    uint8_t reg_latch;

    /* mixer settings and state */
    uint16_t mixer_tone_period_reg[3];
    float mixer_tone_half_period[3];
    float mixer_tone_time[3];
    uint32_t mixer_tone_level[3];
    float mixer_noise_half_period;
    float mixer_noise_time;
    unsigned mixer_noise_level;
    unsigned noise_seed;
    uint8_t mixer_amp[3];
    uint8_t mixer_envelope_control;
    float mixer_envelope_time;
    uint16_t mixer_envelope_period_reg;
    float mixer_envelope_period;
};

static void _ay3_reset(struct ClemensAY38913 *psg, clem_clocks_duration_t ref_step) {
    float old_freq_hz = psg->clock_freq_hz;
    memset(psg, 0, sizeof(*psg));
    if (ref_step != 0) {
        psg->clock_freq_hz =
            ((float)CLEM_CLOCKS_PHI0_CYCLE / ref_step) * CLEM_MEGA2_CYCLES_PER_SECOND;
    } else {
        psg->clock_freq_hz = old_freq_hz;
    }
    psg->noise_seed = 0xa0102035;
    psg->mixer_amp[0] = 0x0f;
    psg->mixer_amp[1] = 0x0f;
    psg->mixer_amp[2] = 0x0f;
}

static void _ay3_tone_setup(struct ClemensAY38913 *psg, unsigned channel_id, uint8_t value,
                            uint8_t byte_index) {
    uint16_t current_period = psg->mixer_tone_period_reg[channel_id];
    if (byte_index) {
        current_period &= (0x00ff);
        current_period |= ((uint16_t)(value) << 8);
    } else {
        current_period &= (0x0f00);
        current_period |= value;
    }
    psg->mixer_tone_period_reg[channel_id] = current_period;
    psg->mixer_tone_half_period[channel_id] = (current_period * 8.0f) / psg->clock_freq_hz;

    if (psg->mixer_tone_time[channel_id] > psg->mixer_tone_half_period[channel_id])
        psg->mixer_tone_time[channel_id] = psg->mixer_tone_half_period[channel_id];
}

static void _ay3_amp_setup(struct ClemensAY38913 *psg, unsigned channel_id, uint8_t value) {
    psg->mixer_amp[channel_id] = value;
}

static void _ay3_envelope_setup(struct ClemensAY38913 *psg, uint8_t value, uint8_t byte_index) {
    uint16_t current_period = psg->mixer_envelope_period_reg;

    if (byte_index) {
        current_period &= (0x00ff);
        current_period |= ((uint16_t)(value) << 8);
    } else {
        current_period &= (0xff00);
        current_period |= value;
    }
    psg->mixer_envelope_period_reg = current_period;
    psg->mixer_envelope_period = (current_period * 256.0f) / psg->clock_freq_hz;

    // TODO: evaluate this... if period shrinks, do we want to clamp or wraparound?
    if (psg->mixer_envelope_time > psg->mixer_envelope_period)
        psg->mixer_envelope_time = psg->mixer_envelope_period;
}

static void _ay3_envelope_control(struct ClemensAY38913 *psg, uint8_t value) {
    psg->mixer_envelope_control = value & 0xf;
}

static void _ay3_noise_setup(struct ClemensAY38913 *psg, uint8_t value) {
    psg->mixer_noise_half_period = (value * 8.0f) / psg->clock_freq_hz;

    if (psg->mixer_noise_time > psg->mixer_noise_half_period)
        psg->mixer_noise_time = psg->mixer_noise_half_period;
}

static unsigned _ay3_noise_gen(struct ClemensAY38913 *psg, float sample_dt) {
    float dt_wave;

    if (psg->mixer_noise_half_period < FLT_EPSILON) {
        return 0;
    }

    dt_wave = psg->mixer_noise_time + sample_dt;
    if (dt_wave >= psg->mixer_noise_half_period) {
        dt_wave -= psg->mixer_noise_half_period;
        psg->mixer_noise_level = psg->noise_seed & 1;
        psg->noise_seed = ((psg->noise_seed * 3) + 4) % 7;
    }
    psg->mixer_noise_time = dt_wave;
    return psg->mixer_noise_level;
}

static float _ay3_tone_render(struct ClemensAY38913 *psg, unsigned channel_id, unsigned noise,
                              float sample_dt) {
    float dt_wave;
    float mag;
    unsigned level;

    if (psg->mixer_tone_half_period[channel_id] < FLT_EPSILON) {
        return 0.0f;
    }

    dt_wave = psg->mixer_tone_time[channel_id];

    if (psg->mixer_tone_level[channel_id] & CLEM_AY3_TONE_LEVEL_ENABLED) {
        level = (psg->mixer_tone_level[channel_id] & CLEM_AY3_TONE_LEVEL_HIGH) ? 1 : 0;
        if (psg->mixer_tone_level[channel_id] & CLEM_AY3_TONE_NOISE_ENABLED) {
            level &= noise;
        }
        mag = (float)((int)(level << 1) - 1);
    } else {
        mag = 0.0f;
    }

    dt_wave += sample_dt;

    if (dt_wave >= psg->mixer_tone_half_period[channel_id]) {
        dt_wave -= psg->mixer_tone_half_period[channel_id];
        psg->mixer_tone_level[channel_id] ^= CLEM_AY3_TONE_LEVEL_HIGH;
    }
    psg->mixer_tone_time[channel_id] = dt_wave;
    return mag;
}

static unsigned _ay3_envelope_gen(struct ClemensAY38913 *psg, float sample_dt) {
    unsigned level = 0;
    float dt_envelope;
    uint8_t cycle;

    if (!((psg->mixer_amp[0] | psg->mixer_amp[1] | psg->mixer_amp[2]) &
          CLEM_AY3_AMP_VARIABLE_MODE_FLAG)) {
        return level;
    }

    cycle = psg->mixer_envelope_control >> 4;

    dt_envelope = psg->mixer_envelope_time;

    //  this is rather brute force - there's probably a better way to do this,
    //  like evaluating each state and look at the cycle count within the if block
    //  but get a reference working first.
    if (cycle & 1) {
        // alternate cycle
        if (psg->mixer_envelope_control & CLEM_AY3_AMP_ENVELOPE_CONTINUE) {
            if (psg->mixer_envelope_control & CLEM_AY3_AMP_ENVELOPE_HOLD) {
                if (psg->mixer_envelope_control & CLEM_AY3_AMP_ENVELOPE_ATTACK) {
                    level =
                        (psg->mixer_envelope_control & CLEM_AY3_AMP_ENVELOPE_ALTERNATE) ? 0 : 15;
                } else {
                    level =
                        (psg->mixer_envelope_control & CLEM_AY3_AMP_ENVELOPE_ALTERNATE) ? 15 : 0;
                }
            } else {
                if (psg->mixer_envelope_control & CLEM_AY3_AMP_ENVELOPE_ATTACK) {
                    if (psg->mixer_envelope_control & CLEM_AY3_AMP_ENVELOPE_ALTERNATE) {
                        level = 15 - (unsigned)(dt_envelope * 16 / psg->mixer_envelope_period);
                    } else {
                        level = (unsigned)(dt_envelope * 16 / psg->mixer_envelope_period);
                    }
                } else {
                    if (psg->mixer_envelope_control & CLEM_AY3_AMP_ENVELOPE_ALTERNATE) {
                        level = (unsigned)(dt_envelope * 16 / psg->mixer_envelope_period);
                    } else {
                        level = 15 - (unsigned)(dt_envelope * 16 / psg->mixer_envelope_period);
                    }
                }
            }
        } else {
            //  hold at level 0
            level = 0;
        }
    } else {
        //  hold doesn't matter here (see the state switch at end of period logic
        //  above, where cycle will always be 1)
        if (psg->mixer_envelope_control & CLEM_AY3_AMP_ENVELOPE_ATTACK) {
            level = (unsigned)(dt_envelope * 16 / psg->mixer_envelope_period);
        } else {
            level = 15 - (unsigned)(dt_envelope * 16 / psg->mixer_envelope_period);
        }
    }

    dt_envelope += sample_dt;
    if (dt_envelope >= psg->mixer_envelope_period) {
        // note the !CONTINUE conditional it's effectively a hold
        if (!(psg->mixer_envelope_control & CLEM_AY3_AMP_ENVELOPE_CONTINUE)) {
            psg->mixer_envelope_control = 0x10 | (psg->mixer_envelope_control & 0xf);
        } else if ((psg->mixer_envelope_control & CLEM_AY3_AMP_ENVELOPE_HOLD)) {
            psg->mixer_envelope_control = 0x10 | (psg->mixer_envelope_control & 0xf);
        } else {
            psg->mixer_envelope_control += 0x10;
        }
        dt_envelope -= psg->mixer_envelope_period;
    }
    psg->mixer_envelope_time = dt_envelope;
    return level;
}

static float _ay3_amp_modify(struct ClemensAY38913 *psg, unsigned channel_id, float sample_in,
                             unsigned envelope, float sample_dt) {
    float sample_out;
    if (psg->mixer_amp[channel_id] & CLEM_AY3_AMP_VARIABLE_MODE_FLAG) {
        sample_out = sample_in * s_ay3_8913_ampl_factor_westcott[envelope];
    } else {
        sample_out = sample_in * s_ay3_8913_ampl_factor_westcott[psg->mixer_amp[channel_id] &
                                                                 CLEM_AY3_AMP_FIXED_LEVEL_MASK];
    }

    return sample_out;
}

static void _ay3_tone_enable(struct ClemensAY38913 *psg, uint8_t value) {
    if (value & 0x01) {
        psg->mixer_tone_level[0] &= ~CLEM_AY3_TONE_LEVEL_ENABLED;
    } else {
        psg->mixer_tone_level[0] |= CLEM_AY3_TONE_LEVEL_ENABLED;
    }
    if (value & 0x02) {
        psg->mixer_tone_level[1] &= ~CLEM_AY3_TONE_LEVEL_ENABLED;
    } else {
        psg->mixer_tone_level[1] |= CLEM_AY3_TONE_LEVEL_ENABLED;
    }
    if (value & 0x04) {
        psg->mixer_tone_level[2] &= ~CLEM_AY3_TONE_LEVEL_ENABLED;
    } else {
        psg->mixer_tone_level[2] |= CLEM_AY3_TONE_LEVEL_ENABLED;
    }
    if (value & 0x08) {
        psg->mixer_tone_level[0] &= ~CLEM_AY3_TONE_NOISE_ENABLED;
    } else {
        psg->mixer_tone_level[0] |= CLEM_AY3_TONE_NOISE_ENABLED;
    }
    if (value & 0x10) {
        psg->mixer_tone_level[1] &= ~CLEM_AY3_TONE_NOISE_ENABLED;
    } else {
        psg->mixer_tone_level[1] |= CLEM_AY3_TONE_NOISE_ENABLED;
    }
    if (value & 0x20) {
        psg->mixer_tone_level[2] &= ~CLEM_AY3_TONE_NOISE_ENABLED;
    } else {
        psg->mixer_tone_level[2] |= CLEM_AY3_TONE_NOISE_ENABLED;
    }
}

static void _ay3_mix_event(struct ClemensAY38913 *psg, uint32_t event) {
    uint8_t event_reg = (uint8_t)((event >> 8) & 0xff);
    uint8_t event_value = (uint8_t)(event & 0xff);

    switch (event_reg) {
    case CLEM_AY3_REG_A_TONE_PERIOD_COARSE:
        _ay3_tone_setup(psg, 0, event_value, 1);
        break;
    case CLEM_AY3_REG_A_TONE_PERIOD_FINE:
        _ay3_tone_setup(psg, 0, event_value, 0);
        break;
    case CLEM_AY3_REG_B_TONE_PERIOD_COARSE:
        _ay3_tone_setup(psg, 1, event_value, 1);
        break;
    case CLEM_AY3_REG_B_TONE_PERIOD_FINE:
        _ay3_tone_setup(psg, 1, event_value, 0);
        break;
    case CLEM_AY3_REG_C_TONE_PERIOD_COARSE:
        _ay3_tone_setup(psg, 2, event_value, 1);
        break;
    case CLEM_AY3_REG_C_TONE_PERIOD_FINE:
        _ay3_tone_setup(psg, 2, event_value, 0);
        break;
    case CLEM_AY3_REG_ENABLE:
        _ay3_tone_enable(psg, event_value);
        break;
    case CLEM_AY3_REG_NOISE_PERIOD:
        _ay3_noise_setup(psg, event_value);
        break;
    case CLEM_AY3_REG_A_AMPLITUDE:
        _ay3_amp_setup(psg, 0, event_value);
        break;
    case CLEM_AY3_REG_B_AMPLITUDE:
        _ay3_amp_setup(psg, 1, event_value);
        break;
    case CLEM_AY3_REG_C_AMPLITUDE:
        _ay3_amp_setup(psg, 2, event_value);
        break;
    case CLEM_AY3_REG_ENVELOPE_COARSE:
        _ay3_envelope_setup(psg, event_value, 1);
        break;
    case CLEM_AY3_REG_ENVELOPE_FINE:
        _ay3_envelope_setup(psg, event_value, 0);
        break;
    case CLEM_AY3_REG_ENVELOPE_SHAPE:
        _ay3_envelope_control(psg, event_value);
        break;
    }
}

static uint32_t _ay3_queue_event(struct ClemensAY38913 *psg, uint8_t value) {
    return (0x80000000 | ((uint16_t)psg->reg_latch << 8) | value);
}

unsigned _ay3_render(struct ClemensAY38913 *psg, clem_clocks_duration_t duration, unsigned channel,
                     float *out, unsigned out_limit, unsigned samples_per_frame,
                     unsigned samples_per_second) {

    float render_window_secs = clem_calc_ns_step_from_clocks(duration) * 1e-9f;
    float sample_dt = 1.0f / samples_per_second;
    unsigned sample_count = 0;
    clem_clocks_duration_t render_dt = clem_calc_clocks_step_from_ns(sample_dt * 1e9f);
    clem_clocks_duration_t render_ts = 0;
    float render_t;
    uint32_t queue_index = 0;
    float sample[3];
    float current;
    float acc;
    float noise;
    unsigned envelope;

    //  TODO: we can just persist tone_period + half_tone_period  instead of
    //        frequency and trim back and forth calculations in _ay3_tone_setup
    for (render_t = 0.0f; render_t < render_window_secs && sample_count < out_limit;
         render_t += sample_dt, out += samples_per_frame) {
        if (queue_index < psg->queue_tail) {
            while (psg->queue_time[queue_index] <= render_ts && queue_index < psg->queue_tail) {
                uint32_t queue_event = psg->queue[queue_index++];
                _ay3_mix_event(psg, queue_event);
            }
        }
        noise = _ay3_noise_gen(psg, sample_dt);
        sample[0] = _ay3_tone_render(psg, 0, noise, sample_dt);
        sample[1] = _ay3_tone_render(psg, 1, noise, sample_dt);
        sample[2] = _ay3_tone_render(psg, 2, noise, sample_dt);
        envelope = _ay3_envelope_gen(psg, sample_dt);
        sample[0] = _ay3_amp_modify(psg, 0, sample[0], envelope, sample_dt);
        sample[1] = _ay3_amp_modify(psg, 1, sample[1], envelope, sample_dt);
        sample[2] = _ay3_amp_modify(psg, 2, sample[2], envelope, sample_dt);
        current = out[channel];
        acc = (sample[0] + sample[1] + sample[2]) * 0.166667f;
        current = out[channel] + acc;
        if (current > 0.75f)
            current = 0.75f;
        else if (current < -0.75f)
            current = -0.75;
        out[channel] = current;

        render_ts += render_dt;
        sample_count++;
    }

    //  consume remaining events to prevent data loss if necessary
    while (queue_index < psg->queue_tail) {
        uint32_t queue_event = psg->queue[queue_index++];
        _ay3_mix_event(psg, queue_event);
    }

    //  TODO: consume events until end of time window
    psg->queue_tail = 0;
    return sample_count;
}

static uint8_t _ay3_get(struct ClemensAY38913 *psg) {
    switch (psg->reg_latch) {
    case CLEM_AY3_REG_A_TONE_PERIOD_FINE:
        return psg->channel_tone_period[0] & 0xff;
    case CLEM_AY3_REG_A_TONE_PERIOD_COARSE:
        return (psg->channel_tone_period[0] >> 8) & 0xff;
    case CLEM_AY3_REG_B_TONE_PERIOD_FINE:
        return psg->channel_tone_period[1] & 0xff;
    case CLEM_AY3_REG_B_TONE_PERIOD_COARSE:
        return (psg->channel_tone_period[1] >> 8) & 0xff;
    case CLEM_AY3_REG_C_TONE_PERIOD_FINE:
        return psg->channel_tone_period[2] & 0xff;
    case CLEM_AY3_REG_C_TONE_PERIOD_COARSE:
        return (psg->channel_tone_period[2] >> 8) & 0xff;
    case CLEM_AY3_REG_NOISE_PERIOD:
        return psg->noise_period;
    case CLEM_AY3_REG_ENABLE:
        return psg->enable;
    case CLEM_AY3_REG_A_AMPLITUDE:
        return psg->channel_amplitude[0];
    case CLEM_AY3_REG_B_AMPLITUDE:
        return psg->channel_amplitude[1];
    case CLEM_AY3_REG_C_AMPLITUDE:
        return psg->channel_amplitude[2];
    case CLEM_AY3_REG_ENVELOPE_FINE:
        return (uint8_t)(psg->envelope_period & 0xff);
    case CLEM_AY3_REG_ENVELOPE_COARSE:
        return (uint8_t)(psg->envelope_period >> 8);
    case CLEM_AY3_REG_ENVELOPE_SHAPE:
        return psg->envelope_shape;
    default:
        break;
    }
    return 0;
}

static void _ay3_set(struct ClemensAY38913 *psg, uint8_t data) {
    switch (psg->reg_latch) {
    case CLEM_AY3_REG_A_TONE_PERIOD_COARSE:
        psg->channel_tone_period[0] &= 0x00ff;
        psg->channel_tone_period[0] |= ((uint16_t)data << 8);
        break;
    case CLEM_AY3_REG_A_TONE_PERIOD_FINE:
        psg->channel_tone_period[0] &= 0xff00;
        psg->channel_tone_period[0] |= data;
        break;
    case CLEM_AY3_REG_B_TONE_PERIOD_COARSE:
        psg->channel_tone_period[1] &= 0x00ff;
        psg->channel_tone_period[1] |= ((uint16_t)data << 8);
        break;
    case CLEM_AY3_REG_B_TONE_PERIOD_FINE:
        psg->channel_tone_period[1] &= 0xff00;
        psg->channel_tone_period[1] |= data;
        break;
    case CLEM_AY3_REG_C_TONE_PERIOD_COARSE:
        psg->channel_tone_period[2] &= 0x00ff;
        psg->channel_tone_period[2] |= ((uint16_t)data << 8);
        break;
    case CLEM_AY3_REG_C_TONE_PERIOD_FINE:
        psg->channel_tone_period[2] &= 0xff00;
        psg->channel_tone_period[2] |= data;
        break;
    case CLEM_AY3_REG_NOISE_PERIOD:
        psg->noise_period = data;
        break;
    case CLEM_AY3_REG_ENABLE:
        psg->enable = data;
        break;
    case CLEM_AY3_REG_A_AMPLITUDE:
        psg->channel_amplitude[0] = data;
        break;
    case CLEM_AY3_REG_B_AMPLITUDE:
        psg->channel_amplitude[1] = data;
        break;
    case CLEM_AY3_REG_C_AMPLITUDE:
        psg->channel_amplitude[2] = data;
        break;
    case CLEM_AY3_REG_ENVELOPE_COARSE:
        psg->envelope_period &= 0x00ff;
        psg->envelope_period |= ((uint16_t)data << 8);
        break;
    case CLEM_AY3_REG_ENVELOPE_FINE:
        psg->envelope_period &= 0xff00;
        psg->envelope_period |= data;
        break;
    case CLEM_AY3_REG_ENVELOPE_SHAPE:
        psg->envelope_shape = data;
        break;
    default:
        break;
    }
}

/*
    Queues commands for audio rendering via clem_ay3_render(...).  Fortunately
    the AY3 here doesn't deal with port output - just taking commands.
    For debugging and possible register reads, we keep a record of current
    register values as well.
 */
static void _ay3_update(struct ClemensAY38913 *psg, uint8_t *bus, uint8_t *bus_control,
                        clem_clocks_duration_t render_slice_dt) {
    uint8_t bc1 = *bus_control & 0x1;
    uint8_t bdir = *bus_control & 0x2;
    uint8_t reset_b = *bus_control & 0x4;
    uint32_t queue_event = 0;
    if (*bus_control == psg->bus_control) {
        return;
    }
    if (!reset_b) {
        _ay3_reset(psg, 0);
        return;
    }

    // CLEM_LOG("AY3: reset_b=%c bdir=%c bc1=%c", reset_b ? '1' : '0',
    //            bdir ? '1' : '0', bc1 ? '1' : '0');

    switch (*bus_control & 0x3) {
    case 0x3:
        /* LATCH_ADDRESS */
        psg->reg_latch = *bus;
        break;
    case 0x1:
        /* READ FROM PSG */
        *bus = _ay3_get(psg);
        break;
    case 0x2:
        /* WRITE TO PSG */
        _ay3_set(psg, *bus);
        queue_event = _ay3_queue_event(psg, *bus);
        break;
    default:
        /* INACTIVE */
        break;
    }

    if (queue_event) {
        if (psg->queue_tail < CLEM_AY3_QUEUE_SIZE) {
            psg->queue[psg->queue_tail] = queue_event;
            psg->queue_time[psg->queue_tail] = render_slice_dt;
            psg->queue_tail++;
        } else {
            CLEM_WARN("ay3_update: lost synth event (%08x)", queue_event);
        }
    }

    psg->bus_control = *bus_control;
}

/**
 * @brief
 *
 * For now, port_a_dir and port_b_dir should be 0xff, set by the emulated
 * application when initializing access to the Mockingboard
 */
struct ClemensVIA6522 {
    uint8_t data_dir[2]; /**< DDRB/A */
    uint8_t data[2];     /**< ORB/A register */
    uint8_t data_in[2];  /**< TODO: unsupported. IRB/A latch */
    uint16_t timer1[2];  /**< Timer 1 Latch and counter */
    uint16_t timer2[2];  /**< Timer 2 Latch (partial) and counter */
    uint8_t sr;          /**< SR (shift register) */
    uint8_t ier;         /**< interrupt enable flags */
    uint8_t ifr;         /**< interrupt flags */
    uint8_t acr;         /**< auxillary control register */
    uint8_t pcr;         /**< peripheral control register */

    enum ClemensVIA6522TimerStatus timer1_status;
    enum ClemensVIA6522TimerStatus timer2_status;
    bool timer1_wraparound;
};

/* The Mockingboard Device here is a 6 channel (2 chip) version

 Below describes the AY-3-891x implementation

 Each PSG has 3 Square Wave Tone Generators (TG)
  Tone frequency is a 12-bit value that combines 'coarse' and 'fine' registers
 Each PSG has 1 Noise Generator (NG)
  Frequency is a 5-bit value
  Each square wave crest has a pseudo-random varying amplitide

 TG[A,B,C] + NG are mixed separately (A + NG, B + NG, C + NG)
  => A, B, C
  => The outputs are modified based on the Mixer settings (i.e. noise on
     select channels, tone on select channels, neither, either, or)

 Each channel (A, B, C) has an amplitude that is controlled *either*
  by a scalar or the current envelope

 Envelope Generation
  Envelope wave has a 16-bit period (coarse + fine registers)
  Envelope wave has a shape (square, triangle, sawtooth, etc)

 6522 <-> AY3 communication
    a.) Instigated by register ORA, ORB writes
    b.) 6522.PortA -> AY3 Bus
    c.) 6522.PortB[0:2] -> AY3 Bus Control
    d.) Allow reads of AY3 registers (for mb-audit validation)

 6522 functions
    a.) DDRA, DDRB offers control of which port pins map to inputs vs outputs
        For Mockingboard programs this should be set to $FF (all output), but
        for accuracy this implementation will follow the rules outlined in the
        datasheet
    b.) T1L, T1H, T2L, T2H operate two 16-bit timers (hench the L, H and 1, 2)
        nomenclature.  Timers decrement at the clock rate and on hitting zero
        trigger an IRQ (if enabled)
    c.) More notes on timers - timer 1 and 2 have subtle differences best
        explained in the implementation comments
    d.) SR [NOT IMPLEMENTED] offers a shift register that functions on the CB2
        pin - which has no use on the Mockingboard
    c.) PCR [NOT IMPLEMENTED] offers handshaking control on the CBx pins -
        which has no use on the Mockingboard (maybe SSI-263 CA1? - TBD)
    c.) IFR, IER offer IRQ control and detection.  For the Mockingboard we
        only care about Timer IRQs (Handshaking and shift register is not
        supported as this time.)

    io_sync() handles timer, IRQ signaling and AY3 execution
    io_write() handles communication with the AY3 and setting of the timer +
               interrupt registers
    io_read() handles reading timer state, port A/B data and interrupt status
    io_reset() resets both the 6522 and signals reset to the AY3
*/
typedef struct {
    struct ClemensVIA6522 via[2];
    struct ClemensAY38913 ay3[2];
    uint8_t via_ay3_bus[2];
    uint8_t via_ay3_bus_control[2];
    /* timestamp within current render window */
    clem_clocks_duration_t sync_time_budget;
    clem_clocks_duration_t ay3_render_slice_duration;
    struct ClemensClock last_clocks;
} ClemensMockingboardContext;

static inline struct ClemensVIA6522 *_mmio_via_addr_parse(ClemensMockingboardContext *context,
                                                          uint8_t ioreg, unsigned *reg) {
    *reg = (ioreg & 0xf);                      /* 0 = ORx/IRxg, 2 = DDRx, etc */
    return &context->via[(ioreg & 0x80) >> 7]; /* chip select */
}

static inline bool _mmio_via_irq_active(struct ClemensVIA6522 *via) {
    uint8_t tmp = (via->ier & via->ifr) & 0x7f;
    return tmp != 0;
}

/* The 6522 VIA update deals mainly with timer state updates
 */

void _clem_via_update_state(struct ClemensVIA6522 *via, uint8_t *port_a, uint8_t *port_b) {
    uint8_t timer1_mode = via->acr & 0xc0;
    uint8_t timer2_mode = via->acr & 0x20;

    via->data_in[CLEM_VIA_6522_PORT_A] &= via->data_dir[CLEM_VIA_6522_PORT_A];
    via->data_in[CLEM_VIA_6522_PORT_A] |= (*port_a & ~via->data_dir[CLEM_VIA_6522_PORT_A]);
    *port_a &= ~via->data_dir[CLEM_VIA_6522_PORT_A];
    *port_a |= (via->data[CLEM_VIA_6522_PORT_A] & via->data_dir[CLEM_VIA_6522_PORT_A]);

    via->data_in[CLEM_VIA_6522_PORT_B] &= via->data_dir[CLEM_VIA_6522_PORT_B];
    via->data_in[CLEM_VIA_6522_PORT_B] |= (*port_b & ~via->data_dir[CLEM_VIA_6522_PORT_B]);
    *port_b &= ~via->data_dir[CLEM_VIA_6522_PORT_B];
    *port_b |= (via->data[CLEM_VIA_6522_PORT_B] & via->data_dir[CLEM_VIA_6522_PORT_B]);

    // PB7 toggling not supported (unneeded)

    // Timer 1 operation:
    --via->timer1[1];
    if (via->timer1_status == kClemensVIA6522TimerStatus_LoadCounter) {
        via->timer1[1] = via->timer1[0];
        if (via->timer1_wraparound) {
            if ((timer1_mode & 0x40) == CLEM_VIA_6522_TIMER1_ONESHOT) {
                via->timer1_status = kClemensVIA6522TimerStatus_Inactive;
            } else if ((timer1_mode & 0x40) == CLEM_VIA_6522_TIMER1_FREERUN) {
                via->timer1_status = kClemensVIA6522TimerStatus_Active;
            }
        } else {
            via->timer1_status = kClemensVIA6522TimerStatus_Active;
        }
        via->timer1_wraparound = false;
    } else if (via->timer1_status != kClemensVIA6522TimerStatus_NoLatch) {
        if (via->timer1[1] == 0xffff) {
            via->timer1_wraparound = true;
            if (via->timer1_status == kClemensVIA6522TimerStatus_Active) {
                via->ifr |= CLEM_VIA_6522_IER_TIMER1;
            }
            via->timer1_status = kClemensVIA6522TimerStatus_LoadCounter;
        }
    }

    // PB6 pulse updated counter not supported (timer 2 pulse mode)
    // The T2 one-shot continues decrementing (no latch reload) once fired
    --via->timer2[1];
    if (via->timer2_status == kClemensVIA6522TimerStatus_LoadCounter) {
        via->timer2[1] = via->timer2[0];
        via->timer2_status = kClemensVIA6522TimerStatus_Active;
    } else if (via->timer2_status != kClemensVIA6522TimerStatus_NoLatch) {
        if (via->timer2[1] == 0xffff) {
            if (via->timer2_status == kClemensVIA6522TimerStatus_Active) {
                via->ifr |= CLEM_VIA_6522_IER_TIMER2;
            }
            if ((timer2_mode & 0x20) == CLEM_VIA_6522_TIMER2_ONESHOT) {
                via->timer2_status = kClemensVIA6522TimerStatus_Inactive;
            } else if ((timer2_mode & 0x20) == CLEM_VIA_6522_TIMER2_PB6) {
                CLEM_ASSERT(false);
                via->timer2_status = kClemensVIA6522TimerStatus_Active;
            }
        }
    }
}

/* io_read and io_write sets the port/control values on the 6522

   io_sync:
    * performs the 6522 <-> AY-3-8910 operations to control the synthesizer
    * the 6522 specific operations (mainly IRQ/timer related)
*/

static void io_reset(struct ClemensClock *clock, void *context) {
    ClemensMockingboardContext *board = (ClemensMockingboardContext *)context;
    memset(&board->via[0], 0, sizeof(struct ClemensVIA6522));
    memset(&board->via[1], 0, sizeof(struct ClemensVIA6522));
    _ay3_reset(&board->ay3[0], clock->ref_step);
    _ay3_reset(&board->ay3[1], clock->ref_step);
    memcpy(&board->last_clocks, clock, sizeof(board->last_clocks));
    board->via_ay3_bus[0] = 0x00;
    board->via_ay3_bus[1] = 0x00;
    board->via_ay3_bus_control[0] = 0x00;
    board->via_ay3_bus_control[1] = 0x00;
    board->ay3_render_slice_duration = 0;
    board->sync_time_budget = 0;
}

static uint32_t io_sync(struct ClemensClock *clock, void *context) {
    ClemensMockingboardContext *board = (ClemensMockingboardContext *)context;
    clem_clocks_duration_t dt_clocks = clock->ts - board->last_clocks.ts;

    board->sync_time_budget += dt_clocks;

    while (board->sync_time_budget > clock->ref_step) {
        _clem_via_update_state(&board->via[0], &board->via_ay3_bus[0],
                               &board->via_ay3_bus_control[0]);
        _ay3_update(&board->ay3[0], &board->via_ay3_bus[0], &board->via_ay3_bus_control[0],
                    board->ay3_render_slice_duration);
        _clem_via_update_state(&board->via[1], &board->via_ay3_bus[1],
                               &board->via_ay3_bus_control[1]);
        _ay3_update(&board->ay3[1], &board->via_ay3_bus[1], &board->via_ay3_bus_control[1],
                    board->ay3_render_slice_duration);
        board->sync_time_budget -= clock->ref_step;
        board->ay3_render_slice_duration += clock->ref_step;
    }

    memcpy(&board->last_clocks, clock, sizeof(board->last_clocks));

    return (_mmio_via_irq_active(&board->via[0]) || _mmio_via_irq_active(&board->via[1]))
               ? CLEM_CARD_IRQ
               : 0;
}

static void io_read(struct ClemensClock *clock, uint8_t *data, uint8_t addr, uint8_t flags,
                    void *context) {
    ClemensMockingboardContext *board = (ClemensMockingboardContext *)context;
    unsigned reg;
    struct ClemensVIA6522 *via;

    if (!(flags & CLEM_OP_IO_DEVSEL)) {
        *data = 0;
        return;
    }

    via = _mmio_via_addr_parse(board, addr, &reg);

    switch (reg) {
    case CLEM_VIA_6522_PORT_A_ALT:
    case CLEM_VIA_6522_REG_DDR + CLEM_VIA_6522_PORT_A:
        *data = via->data_dir[CLEM_VIA_6522_PORT_A];
        break;
    case CLEM_VIA_6522_REG_DATA + CLEM_VIA_6522_PORT_A:
        *data = via->data_in[CLEM_VIA_6522_PORT_A];
        break;
    case CLEM_VIA_6522_REG_DDR + CLEM_VIA_6522_PORT_B:
        *data = via->data_dir[CLEM_VIA_6522_PORT_B];
        break;
    case CLEM_VIA_6522_REG_DATA + CLEM_VIA_6522_PORT_B:
        //  See Section 2.1 of the W65C22 specification (and the Rockwell Port A+B
        //  section) on how IRB is read vs IRA. Bascially output pin values are
        //  read from ORB.  Latching is kinda fake here since we're running step
        //  by step vs concurrently.  I don't think this is problem - especially
        //  since the mockingboard doesn't really do VIA port input. :)
        *data = (via->data[CLEM_VIA_6522_PORT_B] & via->data_dir[CLEM_VIA_6522_PORT_B]) |
                (via->data_in[CLEM_VIA_6522_PORT_B] & ~via->data_dir[CLEM_VIA_6522_PORT_B]);
        break;
    case CLEM_VIA_6522_REG_TIMER1LL:
        *data = (uint8_t)(via->timer1[0] & 0x00ff);
        break;
    case CLEM_VIA_6522_REG_TIMER1CL:
        *data = (uint8_t)(via->timer1[1] & 0x00ff);
        if (!(flags & CLEM_OP_IO_NO_OP)) {
            via->ifr &= ~CLEM_VIA_6522_IER_TIMER1; // clear timer 1 interrupt
        }
        break;
    case CLEM_VIA_6522_REG_TIMER1LH:
        *data = (uint8_t)((via->timer1[0] & 0xff00) >> 8);
        break;
    case CLEM_VIA_6522_REG_TIMER1CH:
        *data = (uint8_t)((via->timer1[1] & 0xff00) >> 8);
        break;
    case CLEM_VIA_6522_REG_TIMER2CL:
        *data = (uint8_t)(via->timer2[1] & 0x00ff);
        if (!(flags & CLEM_OP_IO_NO_OP)) {
            via->ifr &= ~CLEM_VIA_6522_IER_TIMER2;
        }
        break;
    case CLEM_VIA_6522_REG_TIMER2CH:
        *data = (uint8_t)((via->timer2[1] & 0xff00) >> 8);
        break;
    case CLEM_VIA_6522_REG_SR:
        if (!(flags & CLEM_OP_IO_NO_OP)) {
            CLEM_UNIMPLEMENTED("6522 VIA SR read (%x)", addr);
        }
        break;
    case CLEM_VIA_6522_REG_PCR:
        if (!(flags & CLEM_OP_IO_NO_OP)) {
            CLEM_WARN("6522 VIA PCR read (%x)", addr);
        }
        break;
    case CLEM_VIA_6522_REG_ACR:
        *data = via->acr;
        break;
    case CLEM_VIA_6522_REG_IRQ_IER:
        *data = 0x80 | (via->ier & 0x7f);
        break;
    case CLEM_VIA_6522_REG_IRQ_IFR:
        // if interrupt disabled, do not return equivalent flag status
        *data = (_mmio_via_irq_active(via) ? 0x80 : 0x00) | (via->ifr & 0x7f);
        break;
    }
}

static void io_write(struct ClemensClock *clock, uint8_t data, uint8_t addr, uint8_t flags,
                     void *context) {
    ClemensMockingboardContext *board = (ClemensMockingboardContext *)context;
    struct ClemensVIA6522 *via;
    unsigned reg;

    if (!(flags & CLEM_OP_IO_DEVSEL))
        return;

    via = _mmio_via_addr_parse(board, addr, &reg);

    switch (reg) {
    case CLEM_VIA_6522_PORT_A_ALT:
    case CLEM_VIA_6522_REG_DDR + CLEM_VIA_6522_PORT_A:
        via->data_dir[CLEM_VIA_6522_PORT_A] = data;
        break;
    case CLEM_VIA_6522_REG_DATA + CLEM_VIA_6522_PORT_A:
        via->data[CLEM_VIA_6522_PORT_A] = data;
        break;
    case CLEM_VIA_6522_REG_DDR + CLEM_VIA_6522_PORT_B:
        via->data_dir[CLEM_VIA_6522_PORT_B] = data;
        break;
    case CLEM_VIA_6522_REG_DATA + CLEM_VIA_6522_PORT_B:
        via->data[CLEM_VIA_6522_PORT_B] = data;
        break;
    case CLEM_VIA_6522_REG_TIMER1LL:
    case CLEM_VIA_6522_REG_TIMER1CL:
        via->timer1[0] = (via->timer1[0] & 0xff00) | data;
        break;
    case CLEM_VIA_6522_REG_TIMER1LH:
        via->timer1[0] = (via->timer1[0] & 0x00ff) | ((uint16_t)(data) << 8);
        /* The 6522 datasheets conflict on this - the commodore 6522 datasheet
           (2-54) and mb-audit state the timer interrupt flag is cleared on writes
           to the high order latch - but the rockwell datasheet omits this fact. */
        via->ifr &= ~CLEM_VIA_6522_IER_TIMER1;
        break;
    case CLEM_VIA_6522_REG_TIMER1CH:
        via->timer1[0] = (via->timer1[0] & 0x00ff) | ((uint16_t)(data) << 8);
        via->ifr &= ~CLEM_VIA_6522_IER_TIMER1;
        via->timer1_status = kClemensVIA6522TimerStatus_LoadCounter;
        via->timer1_wraparound = false;
        break;
    case CLEM_VIA_6522_REG_TIMER2CL:
        via->timer2[0] = (via->timer2[0] & 0xff00) | data;
        break;
    case CLEM_VIA_6522_REG_TIMER2CH:
        // technically there is no timer 2 high byte latch, but since there are no
        // timer 2 latch registers, the contents of this latch doesn't matter as
        // the actual timer 2 counter is updated in io_sync
        via->timer2[0] = (via->timer2[0] & 0x00ff) | ((uint16_t)(data) << 8);
        via->ifr &= ~CLEM_VIA_6522_IER_TIMER2;
        via->timer2_status = kClemensVIA6522TimerStatus_LoadCounter;
        break;
    case CLEM_VIA_6522_REG_SR:
        CLEM_WARN("6522 VIA SR write (%x)", addr);
        break;
    case CLEM_VIA_6522_REG_PCR:
        CLEM_WARN("6522 VIA PCR write (%x)", addr);
        break;
    case CLEM_VIA_6522_REG_ACR:
        via->acr = data;
        break;
    case CLEM_VIA_6522_REG_IRQ_IER:
        // if disabling interrupts, IRQs will be cleared in io_sync()
        if (data & 0x80) {
            via->ier |= (data & 0x7f);
        } else {
            via->ier &= ~data;
        }
        break;
    case CLEM_VIA_6522_REG_IRQ_IFR:
        via->ifr &= ~(data & 0x7f);
        break;
    }
}

static struct ClemensSerializerRecord kAY3[] = {
    CLEM_SERIALIZER_RECORD_ARRAY(struct ClemensAY38913, kClemensSerializerTypeUInt16,
                                 channel_tone_period, 3, 0),
    CLEM_SERIALIZER_RECORD_UINT16(struct ClemensAY38913, envelope_period),
    CLEM_SERIALIZER_RECORD_ARRAY(struct ClemensAY38913, kClemensSerializerTypeUInt8,
                                 channel_amplitude, 3, 0),
    CLEM_SERIALIZER_RECORD_UINT8(struct ClemensAY38913, noise_period),
    CLEM_SERIALIZER_RECORD_UINT8(struct ClemensAY38913, enable),
    CLEM_SERIALIZER_RECORD_UINT8(struct ClemensAY38913, envelope_shape),
    CLEM_SERIALIZER_RECORD_ARRAY(struct ClemensAY38913, kClemensSerializerTypeUInt32, queue,
                                 CLEM_AY3_QUEUE_SIZE, 0),
    CLEM_SERIALIZER_RECORD_ARRAY(struct ClemensAY38913, kClemensSerializerTypeDuration, queue_time,
                                 CLEM_AY3_QUEUE_SIZE, 0),
    CLEM_SERIALIZER_RECORD_UINT32(struct ClemensAY38913, queue_tail),
    CLEM_SERIALIZER_RECORD_FLOAT(struct ClemensAY38913, clock_freq_hz),
    CLEM_SERIALIZER_RECORD_UINT8(struct ClemensAY38913, bus_control),
    CLEM_SERIALIZER_RECORD_UINT8(struct ClemensAY38913, reg_latch),
    CLEM_SERIALIZER_RECORD_ARRAY(struct ClemensAY38913, kClemensSerializerTypeUInt16,
                                 mixer_tone_period_reg, 3, 0),
    CLEM_SERIALIZER_RECORD_ARRAY(struct ClemensAY38913, kClemensSerializerTypeFloat,
                                 mixer_tone_half_period, 3, 0),
    CLEM_SERIALIZER_RECORD_ARRAY(struct ClemensAY38913, kClemensSerializerTypeFloat,
                                 mixer_tone_time, 3, 0),
    CLEM_SERIALIZER_RECORD_ARRAY(struct ClemensAY38913, kClemensSerializerTypeUInt32,
                                 mixer_tone_level, 3, 0),
    CLEM_SERIALIZER_RECORD_FLOAT(struct ClemensAY38913, mixer_noise_half_period),
    CLEM_SERIALIZER_RECORD_FLOAT(struct ClemensAY38913, mixer_noise_time),
    CLEM_SERIALIZER_RECORD_UINT32(struct ClemensAY38913, mixer_noise_level),
    CLEM_SERIALIZER_RECORD_UINT32(struct ClemensAY38913, noise_seed),
    CLEM_SERIALIZER_RECORD_ARRAY(struct ClemensAY38913, kClemensSerializerTypeUInt8, mixer_amp, 3,
                                 0),
    CLEM_SERIALIZER_RECORD_UINT8(struct ClemensAY38913, mixer_envelope_control),
    CLEM_SERIALIZER_RECORD_FLOAT(struct ClemensAY38913, mixer_envelope_time),
    CLEM_SERIALIZER_RECORD_UINT16(struct ClemensAY38913, mixer_envelope_period_reg),
    CLEM_SERIALIZER_RECORD_FLOAT(struct ClemensAY38913, mixer_envelope_period),
    CLEM_SERIALIZER_RECORD_EMPTY()};

static struct ClemensSerializerRecord kVIA[] = {
    CLEM_SERIALIZER_RECORD_ARRAY(struct ClemensVIA6522, kClemensSerializerTypeUInt8, data_dir, 2,
                                 0),
    CLEM_SERIALIZER_RECORD_ARRAY(struct ClemensVIA6522, kClemensSerializerTypeUInt8, data, 2, 0),
    CLEM_SERIALIZER_RECORD_ARRAY(struct ClemensVIA6522, kClemensSerializerTypeUInt8, data_in, 2, 0),
    CLEM_SERIALIZER_RECORD_ARRAY(struct ClemensVIA6522, kClemensSerializerTypeUInt16, timer1, 2, 0),
    CLEM_SERIALIZER_RECORD_ARRAY(struct ClemensVIA6522, kClemensSerializerTypeUInt16, timer2, 2, 0),
    CLEM_SERIALIZER_RECORD_UINT8(struct ClemensVIA6522, sr),
    CLEM_SERIALIZER_RECORD_UINT8(struct ClemensVIA6522, ier),
    CLEM_SERIALIZER_RECORD_UINT8(struct ClemensVIA6522, ifr),
    CLEM_SERIALIZER_RECORD_UINT8(struct ClemensVIA6522, acr),
    CLEM_SERIALIZER_RECORD_UINT8(struct ClemensVIA6522, pcr),
    CLEM_SERIALIZER_RECORD_INT32(struct ClemensVIA6522, timer1_status),
    CLEM_SERIALIZER_RECORD_INT32(struct ClemensVIA6522, timer2_status),
    CLEM_SERIALIZER_RECORD_BOOL(struct ClemensVIA6522, timer1_wraparound),
    CLEM_SERIALIZER_RECORD_EMPTY()};

static const char *io_name(void *context) { return "mockingboard_c"; }

static struct ClemensSerializerRecord kCard[] = {
    CLEM_SERIALIZER_RECORD_ARRAY_OBJECTS(ClemensMockingboardContext, via, 2, struct ClemensVIA6522,
                                         kVIA),
    CLEM_SERIALIZER_RECORD_ARRAY_OBJECTS(ClemensMockingboardContext, ay3, 2, struct ClemensAY38913,
                                         kAY3),
    CLEM_SERIALIZER_RECORD_ARRAY(ClemensMockingboardContext, kClemensSerializerTypeUInt8,
                                 via_ay3_bus, 2, 0),
    CLEM_SERIALIZER_RECORD_ARRAY(ClemensMockingboardContext, kClemensSerializerTypeUInt8,
                                 via_ay3_bus_control, 2, 0),
    CLEM_SERIALIZER_RECORD_DURATION(ClemensMockingboardContext, sync_time_budget),
    CLEM_SERIALIZER_RECORD_DURATION(ClemensMockingboardContext, ay3_render_slice_duration),
    CLEM_SERIALIZER_RECORD_CLOCK_OBJECT(ClemensMockingboardContext, last_clocks),
    CLEM_SERIALIZER_RECORD_EMPTY()};

void clem_card_mockingboard_initialize(ClemensCard *card) {
    card->context = (ClemensMockingboardContext *)calloc(1, sizeof(ClemensMockingboardContext));
    card->io_reset = &io_reset;
    card->io_sync = &io_sync;
    card->io_read = &io_read;
    card->io_write = &io_write;
    card->io_name = &io_name;
    card->io_dma = NULL;
}

void clem_card_mockingboard_uninitialize(ClemensCard *card) {
    if (card->context) {
        free(card->context);
    }
    memset(card, 0, sizeof(ClemensCard));
}

void clem_card_mockingboard_serialize(mpack_writer_t *writer, ClemensCard *card) {
    struct ClemensSerializerRecord root;
    memset(&root, 0, sizeof(root));
    root.type = kClemensSerializerTypeRoot;
    root.records = &kCard[0];
    clemens_serialize_object(writer, (uintptr_t)card->context, &root);
}

void clem_card_mockingboard_unserialize(mpack_reader_t *reader, ClemensCard *card,
                                        ClemensSerializerAllocateCb alloc_cb, void *context) {
    struct ClemensSerializerRecord root;
    memset(&root, 0, sizeof(root));
    root.type = kClemensSerializerTypeRoot;
    root.records = &kCard[0];
    clemens_unserialize_object(reader, (uintptr_t)card->context, &root, alloc_cb, context);
}

unsigned clem_card_ay3_render(ClemensCard *card, float *samples_out, unsigned sample_limit,
                              unsigned samples_per_frame, unsigned samples_per_second) {
    ClemensMockingboardContext *context = (ClemensMockingboardContext *)card->context;
    unsigned lcount = _ay3_render(&context->ay3[0], context->ay3_render_slice_duration, 0,
                                  samples_out, sample_limit, samples_per_frame, samples_per_second);
    unsigned rcount = _ay3_render(&context->ay3[1], context->ay3_render_slice_duration, 1,
                                  samples_out, sample_limit, samples_per_frame, samples_per_second);
    if (lcount < rcount) {
        for (; lcount < rcount; ++lcount) {
            samples_out[lcount << 1] = 0.0f;
        }

    } else {
        for (; rcount < lcount; ++rcount) {
            samples_out[(rcount << 1) + 1] = 0.0f;
        }
    }
    context->ay3_render_slice_duration = 0;
    return rcount;
}
