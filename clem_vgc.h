#ifndef CLEM_VGC_H
#define CLEM_VGC_H

#include "clem_mmio_types.h"

/**
 * Video Interface
 * These functions emulate the various peripherals (internal and external)
 */

#ifdef __cplusplus
extern "C" {
#endif

void clem_vgc_reset(struct ClemensVGC *vgc);

void clem_vgc_sync(struct ClemensVGC *vgc, struct ClemensClock *clock, const uint8_t *mega2_bank0,
                   const uint8_t *mega2_bank1);

void clem_vgc_scanline_enable_int(struct ClemensVGC *vgc, bool enable);

void clem_vgc_set_mode(struct ClemensVGC *vgc, unsigned mode_flags);
void clem_vgc_clear_mode(struct ClemensVGC *vgc, unsigned mode_flags);
void clem_vgc_set_text_colors(struct ClemensVGC *vgc, unsigned fg_color, unsigned bg_color);

void clem_vgc_set_region(struct ClemensVGC *vgc, uint8_t c02b_value);
uint8_t clem_vgc_get_region(struct ClemensVGC *vgc);

uint8_t clem_vgc_read_switch(struct ClemensVGC *vgc, struct ClemensClock *clock, uint8_t ioreg,
                             uint8_t flags);

void clem_vgc_write_switch(struct ClemensVGC *vgc, struct ClemensClock *clock, uint8_t ioreg,
                           uint8_t value);

void clem_vgc_shgr_scanline(struct ClemensScanline *scanline, uint8_t *dest,
                            const uint8_t *src_bank);

void clem_vgc_calc_counters(struct ClemensVGC *vgc, struct ClemensClock *clock, unsigned *v_counter,
                            unsigned *h_counter);

#ifdef __cplusplus
}
#endif

#endif
