#include "mockingboard.h"
#include "clem_debug.h"

#include <stdbool.h>
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

#define CLEM_VIA_6522_PORT_B 0x00
#define CLEM_VIA_6522_PORT_A 0x01
#define CLEM_VIA_6522_REG_DATA 0x00
#define CLEM_VIA_6522_REG_DDR 0x02
#define CLEM_VIA_6522_REG_TIMER1CL 0x04
#define CLEM_VIA_6522_REG_TIMER1CH 0x05
#define CLEM_VIA_6522_REG_TIMER1LL 0x06
#define CLEM_VIA_6522_REG_TIMER1LH 0x07
#define CLEM_VIA_6522_REG_TIMER2CL 0x08
#define CLEM_VIA_6522_REG_TIMER2CH 0x09
#define CLEM_VIA_6522_REG_SR 0x0a
#define CLEM_VIA_6522_REG_ACR 0x0b
#define CLEM_VIA_6522_REG_PCR 0x0c
#define CLEM_VIA_6522_REG_IRQ_IFR 0x0d
#define CLEM_VIA_6522_REG_IRQ_IER 0x0e
#define CLEM_VIA_6522_PORT_A_ALT 0x0f

#define CLEM_VIA_6522_TIMER1_ONESHOT 0x00
#define CLEM_VIA_6522_TIMER1_FREERUN 0x40
#define CLEM_VIA_6522_TIMER1_PB7 0x80

#define CLEM_VIA_6522_TIMER2_ONESHOT 0x00
#define CLEM_VIA_6522_TIMER2_PB6 0x20

#define CLEM_VIA_6522_IER_TIMER1 0x40
#define CLEM_VIA_6522_IER_TIMER2 0x20

#define CLEM_AY3_QUEUE_SIZE 256

#define CLEM_AY3_REG_A_TONE_PERIOD_FINE 0x00
#define CLEM_AY3_REG_A_TONE_PERIOD_COARSE 0x01
#define CLEM_AY3_REG_B_TONE_PERIOD_FINE 0x02
#define CLEM_AY3_REG_B_TONE_PERIOD_COARSE 0x03
#define CLEM_AY3_REG_C_TONE_PERIOD_FINE 0x04
#define CLEM_AY3_REG_C_TONE_PERIOD_COARSE 0x05
#define CLEM_AY3_REG_NOISE_PERIOD 0x06
#define CLEM_AY3_REG_ENABLE 0x07
#define CLEM_AY3_REG_A_AMPLITUDE 0x08
#define CLEM_AY3_REG_B_AMPLITUDE 0x09
#define CLEM_AY3_REG_C_AMPLITUDE 0x0a
#define CLEM_AY3_REG_ENVELOPE_COARSE 0x0b
#define CLEM_AY3_REG_ENVELOPE_FINE 0x0c
#define CLEM_AY3_REG_ENVELOPE_SHAPE 0x0d
#define CLEM_AY3_REG_IO_A 0x0e
#define CLEM_AY3_REG_IO_B 0x0f

// TODO: other interrupts

enum ClemensVIA6522TimerStatus {
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

  /* reference time step per tick (set at mega2 reference step) */
  clem_clocks_duration_t ref_step;
  /* bus counter to detect bdir changes */
  uint8_t bus_control;
  /* Current register ID latched for read/write */
  uint8_t reg_latch;
};

static void _ay3_reset(struct ClemensAY38913 *psg,
                       clem_clocks_duration_t ref_step) {
  memset(psg, 0, sizeof(*psg));
  psg->bus_control = 0x00;
  psg->ref_step = ref_step;
}

unsigned _ay3_render(struct ClemensAY38913 *psg,
                     clem_clocks_duration_t duration, unsigned channel,
                     float *out, unsigned out_limit, unsigned samples_per_frame,
                     unsigned samples_per_second) {

  float render_window_secs =
      clem_calc_ns_step_from_clocks(duration, CLEM_CLOCKS_MEGA2_CYCLE) * 1e-9f;
  float sample_dt = 1.0f / samples_per_second;
  unsigned sample_count = 0;
  clem_clocks_duration_t render_dt = clem_calc_clocks_step_from_ns(
    sample_dt * 1e9f, CLEM_CLOCKS_MEGA2_CYCLE);
  clem_clocks_duration_t render_ts = 0;
  float render_t;

  for (render_t = 0.0f;
       render_t < render_window_secs && sample_count < out_limit;
       render_t += sample_dt, out += samples_per_frame) {
    float mag = out[channel];
    mag += 0.0f;
    if (mag > 1.0f)
      mag = 1.0f;
    else if (mag < -1.0f)
      mag = -1.0f;
    out[channel] = mag;
    render_ts += render_dt;
    sample_count++;
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
static void _ay3_update(struct ClemensAY38913 *psg, uint8_t *bus,
                        uint8_t *bus_control,
                        clem_clocks_duration_t render_slice_dt) {
  uint8_t bc1 = *bus_control & 0x1;
  uint8_t bdir = *bus_control & 0x2;
  uint8_t reset_b = *bus_control & 0x4;
  uint32_t queue_event = 0;
  if (*bus_control != psg->bus_control) {
    return;
  }
  if (!reset_b) {
    _ay3_reset(psg, psg->ref_step);
    return;
  }

  // CLEM_LOG("AY3: reset_b=%c bdir=%c bc1=%c", reset_b ? '1' : '0',
  //          bdir ? '1' : '0', bc1 ? '1' : '0');

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
    queue_event = 0x80000000 | ((uint16_t)psg->reg_latch << 8) | *bus;
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

static ClemensMockingboardContext s_context;

static inline struct ClemensVIA6522 *_mmio_via_addr_parse(uint8_t ioreg,
                                                          unsigned *reg) {
  *reg = (ioreg & 0xf);                       /* 0 = ORx/IRxg, 2 = DDRx, etc */
  return &s_context.via[(ioreg & 0x80) >> 7]; /* chip select */
}

static inline bool _mmio_via_irq_active(struct ClemensVIA6522 *via) {
  uint8_t tmp = (via->ier & via->ifr) & 0x7f;
  return tmp != 0;
}

/* The 6522 VIA update deals mainly with timer state updates
 */

void _clem_via_update_state(struct ClemensVIA6522 *via, uint8_t *port_a,
                            uint8_t *port_b) {
  uint8_t timer1_mode = via->acr & 0xc0;
  uint8_t timer2_mode = via->acr & 0x20;

  via->data_in[CLEM_VIA_6522_PORT_A] &= via->data_dir[CLEM_VIA_6522_PORT_A];
  via->data_in[CLEM_VIA_6522_PORT_A] |=
      (*port_a & ~via->data_dir[CLEM_VIA_6522_PORT_A]);
  *port_a &= ~via->data_dir[CLEM_VIA_6522_PORT_A];
  *port_a |=
      (via->data[CLEM_VIA_6522_PORT_A] & via->data_dir[CLEM_VIA_6522_PORT_A]);

  via->data_in[CLEM_VIA_6522_PORT_B] &= via->data_dir[CLEM_VIA_6522_PORT_B];
  via->data_in[CLEM_VIA_6522_PORT_B] |=
      (*port_b & ~via->data_dir[CLEM_VIA_6522_PORT_B]);
  *port_b &= ~via->data_dir[CLEM_VIA_6522_PORT_B];
  *port_b |=
      (via->data[CLEM_VIA_6522_PORT_B] & via->data_dir[CLEM_VIA_6522_PORT_B]);

  // PB7 toggling not supported (unneeded)

  // Timer 1 operation:
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
  } else {
    --via->timer1[1];
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
  if (via->timer2_status == kClemensVIA6522TimerStatus_LoadCounter) {
    via->timer2[1] = via->timer2[0];
    via->timer2_status = kClemensVIA6522TimerStatus_Active;
  } else {
    --via->timer2[1];
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
  board->via_ay3_bus_control[0] = 0x00;
  board->via_ay3_bus_control[1] = 0x00;
  board->ay3_render_slice_duration = 0;
  board->sync_time_budget = 0;
}

static uint32_t io_sync(struct ClemensClock *clock, void *context) {
  ClemensMockingboardContext *board = (ClemensMockingboardContext *)context;
  clem_clocks_duration_t dt_clocks = clock->ts - s_context.last_clocks.ts;

  board->sync_time_budget += dt_clocks;

  while (board->sync_time_budget > clock->ref_step) {
    _clem_via_update_state(&board->via[0], &board->via_ay3_bus[0],
                           &board->via_ay3_bus_control[0]);
    _ay3_update(&board->ay3[0], &board->via_ay3_bus[0],
                &board->via_ay3_bus_control[0],
                board->ay3_render_slice_duration);
    _clem_via_update_state(&board->via[1], &board->via_ay3_bus[1],
                           &board->via_ay3_bus_control[1]);
    _ay3_update(&board->ay3[1], &board->via_ay3_bus[1],
                &board->via_ay3_bus_control[1],
                board->ay3_render_slice_duration);
    board->sync_time_budget -= clock->ref_step;
    board->ay3_render_slice_duration += clock->ref_step;
  }

  memcpy(&board->last_clocks, clock, sizeof(board->last_clocks));

  return (_mmio_via_irq_active(&board->via[0]) ||
          _mmio_via_irq_active(&board->via[1]))
             ? CLEM_CARD_IRQ
             : 0;
}

static void io_read(struct ClemensClock *clock, uint8_t *data, uint8_t addr,
                    uint8_t flags, void *context) {
  unsigned reg;
  struct ClemensVIA6522 *via;

  if (!(flags & CLEM_OP_IO_DEVSEL)) {
    *data = 0;
    return;
  }

  via = _mmio_via_addr_parse(addr, &reg);

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
    *data = (via->data[CLEM_VIA_6522_PORT_B] &
             via->data_dir[CLEM_VIA_6522_PORT_B]) |
            (via->data_in[CLEM_VIA_6522_PORT_B] &
             ~via->data_dir[CLEM_VIA_6522_PORT_B]);
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

static void io_write(struct ClemensClock *clock, uint8_t data, uint8_t addr,
                     uint8_t flags, void *context) {
  struct ClemensVIA6522 *via;
  unsigned reg;

  if (!(flags & CLEM_OP_IO_DEVSEL))
    return;

  via = _mmio_via_addr_parse(addr, &reg);

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

void clem_card_mockingboard_initialize(ClemensCard *card) {
  card->context = &s_context;
  card->io_reset = &io_reset;
  card->io_sync = &io_sync;
  card->io_read = &io_read;
  card->io_write = &io_write;
}

void clem_card_mockingboard_uninitialize(ClemensCard *card) {
  memset(card, 0, sizeof(ClemensCard));
}

unsigned clem_card_ay3_render(ClemensCard *card, float *samples_out,
                              unsigned sample_limit, unsigned samples_per_frame,
                              unsigned samples_per_second) {
  ClemensMockingboardContext *context =
      (ClemensMockingboardContext *)card->context;
  unsigned lcount = _ay3_render(
      &context->ay3[0], context->ay3_render_slice_duration, 0, samples_out,
      sample_limit, samples_per_frame, samples_per_second);
  unsigned rcount = _ay3_render(
      &context->ay3[1], context->ay3_render_slice_duration, 1, samples_out,
      sample_limit, samples_per_frame, samples_per_second);
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
