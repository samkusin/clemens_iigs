#ifndef CLEM_GAME_H
#define CLEM_GAME_H

#include "clem_types.h"

/**
 * Game Port Interface
 * These functions emulate the game-related peripherals (internal and external)
 */

#ifdef __cplusplus
extern "C" {
#endif

void clem_game_reset(struct ClemensDeviceGame* vgc);

uint8_t clem_game_read_switch(struct ClemensDeviceGame* vgc,
                              struct ClemensClock* clock,
                              uint8_t ioreg,
                              uint8_t flags);

void clem_game_write_switch(struct ClemensDeviceGame* vgc,
                            struct ClemensClock* clock,
                            uint8_t ioreg,
                            uint8_t value);


#ifdef __cplusplus
}
#endif

#endif
