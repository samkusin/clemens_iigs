#include "mockingboard.h"

#include <string.h>
#include <stdbool.h>

/*
  References:
  - Programming IO primer for the A2 Mockingboard
      https://www.apple2.org.za/gswv/a2zine/Docs/Mockingboard_MiniManual.html
  - Programming to AY-3-8910 sound chip
    - https://www.vdsteenoven.com/aquarius/psgprog.html
  - AY-3-8910 Datasheet
  - 6522 MOS Datasheet and WDC 65C22
  - Resources from
      https://wiki.reactivemicro.com/Mockingboard
*/

#define CLEM_VIA_6522_PORT_B        0x00
#define CLEM_VIA_6522_PORT_A        0x01
#define CLEM_VIA_6522_REG_DATA      0x00
#define CLEM_VIA_6522_REG_DDR       0x02
#define CLEM_VIA_6522_REG_TIMER1CL  0x04
#define CLEM_VIA_6522_REG_TIMER1CH  0x05
#define CLEM_VIA_6522_REG_TIMER1LL  0x06
#define CLEM_VIA_6522_REG_TIMER1LH  0x07
#define CLEM_VIA_6522_REG_TIMER2CL  0x08
#define CLEM_VIA_6522_REG_TIMER2CH  0x09
#define CLEM_VIA_6522_REG_SR        0x0a
#define CLEM_VIA_6522_REG_ACR       0x0b
#define CLEM_VIA_6522_REG_PCR       0x0c
#define CLEM_VIA_6522_REG_IRQ_IFR   0x0d
#define CLEM_VIA_6522_REG_IRQ_IER   0x0e

#define CLEM_VIA_6522_LATCH_DATA_B  0x00
#define CLEM_VIA_6522_LATCH_DATA_A  0x01
#define CLEM_VIA_6522_LATCH_TIL     0x02
#define CLEM_VIA_6522_LATCH_TIH     0x03
#define CLEM_VIA_6522_LATCH_T2L     0x04


/**
 * @brief
 *
 * The PSG here is the AY-3-8910 chip
 *
 * For now, port_a_dir and port_b_dir should be 0xff, set by the emulated
 * application when initializing access to the Mockingboard
 */
struct ClemensVIA6522 {
    uint8_t port_dir[2];                /**< pins 0-7, on = output to PSG */
    uint8_t port_data[2];               /**< pins 0-7, A, B  */
    uint16_t t1_timer;                  /**< t1 multi-mode timer counter */
    uint16_t t2_timer;                  /**< t2 one-shot timer counter */
    uint8_t sr;                         /**< SR (shift register) */
    uint8_t ier;                        /**< interrupt enable flags */
    uint8_t ifr;                        /**< interrupt flags */
    uint8_t acr;                        /**< auxillary control register */
    uint8_t pcr;                        /**< peripheral control register */

    uint8_t latch[5];
};

/* The Mockingboard Device here is a 6 channel (2 chip) version

   Below describes the AY-3-8910 implementation

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


*/
typedef struct {
  struct ClemensVIA6522 via[2];
  clem_clocks_time_t last_clocks_time;  /* 1mhz (MEGAII) clock */
} ClemensMockingboardContext;

static ClemensMockingboardContext s_context;


static inline struct ClemensVIA6522* _mmio_via_addr_parse(uint8_t ioreg, unsigned* port, unsigned* reg) {
    *port = (ioreg & 0x1) ^ 1;      /* flip A, B so A = 0, B = 1 */
    *reg = (ioreg & 0xe);           /* 0 = ORx/IRxg, 2 = DDRx, etc */
    return &s_context.via[(ioreg & 0x80) >> 7];     /* chip select */
}

void _clem_via_update_timers(struct ClemensVIA6522* via) {

}


/* io_read and io_write sets the port/control values on the 6522

   io_sync:
    * performs the 6522 <-> AY-3-8910 operations to control the synthesizer
    * the 6522 specific operations (mainly IRQ/timer related)
*/

static void io_reset(struct ClemensClock* clock, void* context) {
    memset(&s_context, 0, sizeof(s_context));
    s_context.last_clocks_time = clock->ts;
}

static uint32_t io_sync(struct ClemensClock* clock, void* context) {
    clem_clocks_duration_t dt_clocks = clock->ts - s_context.last_clocks_time;
    clem_clocks_duration_t dt_spent = 0;

    while (dt_spent < dt_clocks ) {
        _clem_via_update_timers(&s_context.via[0]);
        _clem_via_update_timers(&s_context.via[1]);
        dt_spent += clock->ref_step;
    }

    s_context.last_clocks_time = clock->ts;

    return (s_context.via[0].ifr || s_context.via[1].ifr) ? CLEM_CARD_IRQ : 0;
}

static void io_read(uint8_t* data, uint16_t addr, uint8_t flags, void* context) {
    unsigned port, reg;
    struct ClemensVIA6522* via =
        _mmio_via_addr_parse((uint8_t)(addr & 0xff), &port, &reg);

    switch (reg) {
        case CLEM_VIA_6522_REG_DDR:
            *data = via->port_dir[port];
            break;
        case CLEM_VIA_6522_REG_DATA:
            *data = via->port_data[port] & (~via->port_dir[port]);
            break;
        default:
            reg = reg | port;       /* parse the remaining register options */
            break;
    }
    switch (reg) {
        case CLEM_VIA_6522_REG_TIMER1CL:
            /* W65C22 - 2.5 Timer 1 Operation Read */
            *data = (uint8_t)(via->t1_timer & 0xff);
            via->ifr &= ~(1 << 6);
            break;
        case CLEM_VIA_6522_REG_TIMER1CH:
            /* W65C22 - 2.5 Timer 1 Operation Read */
            *data = (uint8_t)(via->t1_timer >> 8);
            break;
        case CLEM_VIA_6522_REG_TIMER1LL:
            /* W65C22 - 2.5 Timer 1 Operation Read */
            *data = via->latch[CLEM_VIA_6522_LATCH_TIL];
            break;
        case CLEM_VIA_6522_REG_TIMER1LH:
            /* W65C22 - 2.5 Timer 1 Operation Read */
            *data = via->latch[CLEM_VIA_6522_LATCH_TIH];
            break;
        case CLEM_VIA_6522_REG_TIMER2CL:
            *data = (uint8_t)(via->t2_timer & 0xff);
            via->ifr &= ~(1 << 5);
            break;
        case CLEM_VIA_6522_REG_TIMER2CH:
            *data = (uint8_t)(via->t2_timer >> 8);
            break;
        case CLEM_VIA_6522_REG_SR:
            *data = via->sr;
            break;
        case CLEM_VIA_6522_REG_ACR:
            *data = via->acr;
            break;
        case CLEM_VIA_6522_REG_PCR:
            *data = via->pcr;
            break;
        case CLEM_VIA_6522_REG_IRQ_IFR:
            /* W65C22 2.14 */
            *data = via->ifr;
            break;
        case CLEM_VIA_6522_REG_IRQ_IER:
            /* W65C22 2.14 */
            *data = via->ier | 0x80;
            break;
    }
}

static void io_write(uint8_t data, uint16_t addr, uint8_t flags, void* context) {
    unsigned port, reg;
     struct ClemensVIA6522* via =
        _mmio_via_addr_parse((uint8_t)(addr & 0xff), &port, &reg);

    switch (reg) {
        case CLEM_VIA_6522_REG_DDR:
            via->port_dir[port] = data;
            break;
        case CLEM_VIA_6522_REG_DATA:
            via->latch[CLEM_VIA_6522_LATCH_DATA_A + port] = (uint8_t)data;
            break;
        default:
            reg = reg | port;       /* parse the remaining register options */
            break;
    }
    switch (reg) {
        case CLEM_VIA_6522_REG_TIMER1CL:
            /* W65C22 - 2.5 Timer 1 Operation Write - latch xfer to register
               on high byte load */
            via->latch[CLEM_VIA_6522_LATCH_TIL] = data;
            break;
        case CLEM_VIA_6522_REG_TIMER1CH:
            /* W65C22 - 2.5 Timer 1 Operation Write, IFR reset */
            via->latch[CLEM_VIA_6522_LATCH_TIH] = data;
            via->t1_timer =
                ((uint16_t)(via->latch[CLEM_VIA_6522_LATCH_TIH]) << 8) |
                    via->latch[CLEM_VIA_6522_LATCH_TIL];
            via->ifr &= ~(1 << 6);
            break;
        case CLEM_VIA_6522_REG_TIMER1LL:
            /* W65C22 - 2.5 Timer 1 Operation Write */
            via->latch[CLEM_VIA_6522_LATCH_TIL] = data;
            break;
        case CLEM_VIA_6522_REG_TIMER1LH:
            /* W65C22 - 2.5 Timer 1 Operation Write - no latch xfer, ifr
               reset */
            via->latch[CLEM_VIA_6522_LATCH_TIH] = data;
            via->ifr &= ~(1 << 6);
            break;
        case CLEM_VIA_6522_REG_TIMER2CL:
            via->latch[CLEM_VIA_6522_LATCH_T2L] = data;
            break;
        case CLEM_VIA_6522_REG_TIMER2CH:
            via->t2_timer =
                ((uint16_t)(data) << 8) | via->latch[CLEM_VIA_6522_LATCH_T2L];
            via->ifr &= ~(1 << 5);
            break;
        case CLEM_VIA_6522_REG_SR:
            via->sr = data;
            break;
        case CLEM_VIA_6522_REG_ACR:
            via->acr = data;
            break;
        case CLEM_VIA_6522_REG_PCR:
            via->pcr = data;
            break;
        case CLEM_VIA_6522_REG_IRQ_IFR:
            /* W65C22 2.14 */
            via->ifr = data & 0x80;     /* IFR7 is cleared in sync() */
            break;
        case CLEM_VIA_6522_REG_IRQ_IER:
            /* W65C22 2.14 */
            if (data & 0x80) {
                via->ier |= (data & 0x7f);
            } else {
                via->ier &= ~(data & 0x7f);
            }
            break;
    }
}


void clem_card_mockingboard_initialize(ClemensCard* card) {
  card->context = &s_context;
  card->io_reset = &io_reset;
  card->io_sync = &io_sync;
  card->io_read = &io_read;
  card->io_write = &io_write;
}

void clem_card_mockingboard_uninitialize(ClemensCard* card) {
  memset(card, 0, sizeof(ClemensCard));
}
