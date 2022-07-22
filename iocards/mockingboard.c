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
  uint16_t channel_tone_period[3];
  uint16_t envelope_period;
  uint8_t channel_amplitude[3];
  uint8_t noise_period;
  uint8_t enable;
  uint8_t envelope_shape;

  /* instruction queue */
  uint32_t queue[CLEM_AY3_QUEUE_SIZE];
  clem_clocks_time_t queue_ts[CLEM_AY3_QUEUE_SIZE];
  clem_clocks_duration_t ref_step;
  uint32_t queue_tail;

  /* bus counter to detect bdir changes */
  uint8_t bus_control;
};

static void _ay3_reset(struct ClemensAY38913 *psg, struct ClemensClock *clock) {
  memset(psg, 0, sizeof(*psg));
  psg->bus_control = 0x00;
  psg->ref_step = clock->ref_step;
}

/*
    Queues commands for audio rendering via clem_ay3_render(...).  Fortunately
    the AY3 here doesn't deal with port output - just taking commands.
    For debugging and possible register reads, we keep a record of current
    register values as well.
 */
static void _ay3_update(struct ClemensAY38913 *psg, uint8_t *bus,
                        uint8_t *bus_control, clem_clocks_duration_t dt_step) {
  uint8_t bctl = *bus_control & 0x1;
  uint8_t bdir = *bus_control & 0x2;
  /* This maps to the b_reset pin on a AY3-8913 and implies that it'll always
     equal 1 unless we're resetting the AY3.  */
  uint8_t bc2 = *bus_control & 0x4;

  if (*bus_control == psg->bus_control) {
    return;
  }

  CLEM_LOG("AY3: bc2=%c bctl=%c bdir=%c", bc2 ? '1' : '0', bctl ? '1' : '0',
           bdir ? '1' : '0');

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

 6522 -> AY3 communication
    a.) Instigated by register ORA, ORB writes
    b.) 6522.PortA -> AY3 Bus
    c.) 6522.PortB[0:2] -> AY3 Bus Control

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
  struct ClemensClock last_clocks;
} ClemensMockingboardContext;

static ClemensMockingboardContext s_context;

static inline struct ClemensVIA6522 *_mmio_via_addr_parse(uint8_t ioreg,
                                                          unsigned *reg) {
  *reg = (ioreg & 0xf);                       /* 0 = ORx/IRxg, 2 = DDRx, etc */
  return &s_context.via[(ioreg & 0x80) >> 7]; /* chip select */
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

  // Timer 1 vs 2 subtle difference in one-shot mode, on timer 1 wraparound,
  // the counter is reloaded from the timer 1 latches.  For timer 2, this is
  // not the case.
  //
  // PB7 toggling not supported (unneeded)
  if (via->timer1_status == kClemensVIA6522TimerStatus_LoadCounter) {
    via->timer1[1] = via->timer1[0];
    via->timer1_status = kClemensVIA6522TimerStatus_Active;
  } else {
    --via->timer1[1];
    if (via->timer1[1] == 0xffff) {
      // Here is the counter reload from latch per wraparound
      via->timer1[1] = via->timer1[0];
    }
  }
  if (via->timer1[1] == 0) {
    if (via->timer1_status == kClemensVIA6522TimerStatus_Active) {
      if (via->ier & CLEM_VIA_6522_IER_TIMER1) {
        via->ifr |= CLEM_VIA_6522_IER_TIMER1;
      }
      if ((timer1_mode & 0x40) == CLEM_VIA_6522_TIMER1_ONESHOT) {
        via->timer1_status = kClemensVIA6522TimerStatus_Inactive;
      }
    }
  }

  // PB6 pulse updated counter not supported (timer 2 pulse mode)
  if (via->timer2_status == kClemensVIA6522TimerStatus_LoadCounter) {
    via->timer2[1] = via->timer2[0];
    via->timer2_status = kClemensVIA6522TimerStatus_Active;
  } else {
    if ((timer2_mode & 0x20) == CLEM_VIA_6522_TIMER2_ONESHOT) {
      --via->timer2[1];
    }
  }
  if (via->timer2[1] == 0) {
    if (via->timer2_status == kClemensVIA6522TimerStatus_Active) {
      if (via->ier & CLEM_VIA_6522_IER_TIMER2) {
        via->ifr |= CLEM_VIA_6522_IER_TIMER2;
      }
      if ((timer2_mode & 0x20) == CLEM_VIA_6522_TIMER2_ONESHOT) {
        via->timer1_status = kClemensVIA6522TimerStatus_Inactive;
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
  _ay3_reset(&board->ay3[0], clock);
  _ay3_reset(&board->ay3[1], clock);
  memcpy(&board->last_clocks, clock, sizeof(board->last_clocks));
  board->via_ay3_bus[0] = 0x00;
  board->via_ay3_bus_control[0] = 0x00;
  board->via_ay3_bus_control[1] = 0x00;
}

static uint32_t io_sync(struct ClemensClock *clock, void *context) {
  ClemensMockingboardContext *board = (ClemensMockingboardContext *)context;
  clem_clocks_duration_t dt_clocks = clock->ts - s_context.last_clocks.ts;
  clem_clocks_duration_t dt_spent = 0;

  while (dt_spent < dt_clocks) {
    _clem_via_update_state(&board->via[0], &board->via_ay3_bus[0],
                           &board->via_ay3_bus_control[0]);
    _ay3_update(&board->ay3[0], &board->via_ay3_bus[0],
                &board->via_ay3_bus_control[0], clock->ref_step);
    _clem_via_update_state(&board->via[1], &board->via_ay3_bus[1],
                           &board->via_ay3_bus_control[1]);
    _ay3_update(&board->ay3[1], &board->via_ay3_bus[1],
                &board->via_ay3_bus_control[1], clock->ref_step);
    dt_spent += clock->ref_step;
  }

  memcpy(&board->last_clocks, clock, sizeof(board->last_clocks));

  return (board->via[0].ifr || board->via[1].ifr) ? CLEM_CARD_IRQ : 0;
}

static void io_read(uint8_t *data, uint8_t addr, uint8_t flags,
                    void *context) {
  unsigned reg;
  uint8_t tmp;
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
    via->ifr &= ~CLEM_VIA_6522_IER_TIMER1; // clear timer 1 interrupt
    break;
  case CLEM_VIA_6522_REG_TIMER1LH:
    *data = (uint8_t)((via->timer1[0] & 0xff00) >> 8);
    break;
  case CLEM_VIA_6522_REG_TIMER1CH:
    *data = (uint8_t)((via->timer1[1] & 0xff00) >> 8);
    break;
  case CLEM_VIA_6522_REG_TIMER2CL:
    *data = (uint8_t)(via->timer2[1] & 0x00ff);
    via->ifr &= ~CLEM_VIA_6522_IER_TIMER2;
    break;
  case CLEM_VIA_6522_REG_TIMER2CH:
    *data = (uint8_t)((via->timer2[1] & 0xff00) >> 8);
    break;
  case CLEM_VIA_6522_REG_SR:
    CLEM_UNIMPLEMENTED("6522 VIA SR read (%x)", addr);
    break;
  case CLEM_VIA_6522_REG_PCR:
    CLEM_UNIMPLEMENTED("6522 VIA PCR read (%x)", addr);
    break;
  case CLEM_VIA_6522_REG_ACR:
    *data = via->acr;
    break;
  case CLEM_VIA_6522_REG_IRQ_IER:
    *data = 0x80 | (via->ier & 0x7f);
    break;
  case CLEM_VIA_6522_REG_IRQ_IFR:
    // if interrupt disabled, do not return equivalent flag status
    tmp = (via->ier & via->ifr) & 0x7f;
    *data = (tmp ? 0x80 : 0x00) | tmp;
    break;
  }
}

static void io_write(uint8_t data, uint8_t addr, uint8_t flags,
                     void *context) {
  unsigned reg;
  struct ClemensVIA6522 *via;

  if (!(flags & CLEM_OP_IO_DEVSEL)) return;

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
    via->timer1[0] = (via->timer1[0] & ~0xff00) | data;
    break;
  case CLEM_VIA_6522_REG_TIMER1LH:
    via->timer1[0] = (via->timer1[0] & ~0x00ff) | (uint16_t)(data << 8);
    break;
  case CLEM_VIA_6522_REG_TIMER1CH:
    via->timer1[0] = (via->timer1[0] & ~0x00ff) | (uint16_t)(data << 8);
    via->ifr &= ~CLEM_VIA_6522_IER_TIMER1; // clear interrupt on write
    via->timer1_status = kClemensVIA6522TimerStatus_LoadCounter;
    break;
  case CLEM_VIA_6522_REG_TIMER2CL:
    via->timer2[0] = (via->timer2[0] & 0xff00) | data;
    break;
  case CLEM_VIA_6522_REG_TIMER2CH:
    // technically there is no timer 2 high byte latch, but since there are no
    // timer 2 latch registers, the contents of this latch doesn't matter as
    // the actual timer 2 counter is updated in io_sync
    via->timer2[0] = (via->timer2[0] & ~0x00ff) | (uint16_t)(data << 8);
    via->ifr &= ~CLEM_VIA_6522_IER_TIMER2;
    via->timer2_status = kClemensVIA6522TimerStatus_LoadCounter;
    break;
  case CLEM_VIA_6522_REG_SR:
    CLEM_UNIMPLEMENTED("6522 VIA SR write (%x)", addr);
    break;
  case CLEM_VIA_6522_REG_PCR:
    CLEM_UNIMPLEMENTED("6522 VIA PCR write (%x)", addr);
    break;
  case CLEM_VIA_6522_REG_ACR:
    via->acr = data;
    break;
  case CLEM_VIA_6522_REG_IRQ_IER:
    // if disabling interrupts, IRQs will be cleared in io_sync()
    if (data & 0x80) {
      via->ier |= data;
    } else {
      via->ier &= ~data;
    }
    break;
  case CLEM_VIA_6522_REG_IRQ_IFR:
    if (data & 0x80) {
      // clears interrupts set - the IRQs are determined in io_sync (bit 7)
      via->ifr &= ~(data & 0x7f);
    }
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
