#include "mockingboard.h"

#include <string.h>
#include <stdbool.h>

/*
  References:
  - AY-3-8910 Datasheet
  - 6522 MOS Datasheet
  - Programming to AY-3-8910 sound chip
    - https://www.vdsteenoven.com/aquarius/psgprog.html
  - Programming IO primer for the A2 Mockingboard
      https://www.apple2.org.za/gswv/a2zine/Docs/Mockingboard_MiniManual.html
  - Resources from
      https://wiki.reactivemicro.com/Mockingboard
*/

/**
 * @brief
 *
 * The PSG here is the AY-3-8910 chip
 *
 * For now, port_a_dir and port_b_dir should be 0xff, set by the emulated
 * application when initializing access to the Mockingboard
 */
struct ClemensVIA6522 {
  uint8_t port_a_dir;             /**< pins 0-7, on = output to PSG */
  uint8_t port_b_dir;             /**< pins 0-7, on = output to PSG */
};

/* The Mockingboard Device here is a 6 channel (2 chip) version
*/
typedef struct {
  struct ClemensVIA6522 via[2];
} ClemensMockingboardContext;


static ClemensMockingboardContext s_context;


static uint32_t io_sync(struct ClemensClock* clock, void* context) {
  return 0;
}

static void io_read(uint8_t* data, uint16_t adr, uint8_t flags, void* context) {

}

static void io_write(uint8_t data, uint16_t adr, uint8_t flags, void* context) {

}


void clem_card_mockingboard_initialize(ClemensCard* card) {
  card->context = &s_context;
  card->io_sync = &io_sync;
  card->io_read = &io_read;
  card->io_write = &io_write;
}

void clem_card_mockingboard_uninitialize(ClemensCard* card) {
  memset(card, 0, sizeof(ClemensCard));
}
