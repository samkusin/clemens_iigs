#ifndef CLEM_VGC_H
#define CLEM_VGC_H

#include "clem_types.h"

#define CLEM_VGC_GRAPHICS_MODE     0x00000001
#define CLEM_VGC_MIXED_TEXT        0x00000002
#define CLEM_VGC_80COLUMN_TEXT     0x00000004
#define CLEM_VGC_LORES             0x00000010
#define CLEM_VGC_HIRES             0x00000020
#define CLEM_VGC_RESOLUTION_MASK   0x000000F0
#define CLEM_VGC_SUPER_HIRES       0x00000100
#define CLEM_VGC_ALTCHARSET        0x00010000
#define CLEM_VGC_MONOCHROME        0x00020000
#define CLEM_VGC_PAL               0x00040000
#define CLEM_VGC_LANGUAGE          0x00080000
#define CLEM_VGC_ENABLE_VBL_IRQ    0x00100000
#define CLEM_VGC_DISABLE_AN3       0x00200000
#define CLEM_VGC_DBLHIRES_MASK     0x00200025
#define CLEM_VGC_INIT              0x80000000

/**
 * Video Interface
 * These functions emulate the various peripherals (internal and external)
 */

#ifdef __cplusplus
extern "C" {
#endif


void clem_vgc_reset(struct ClemensVGC* vgc);

void clem_vgc_sync(struct ClemensVGC* vgc, struct ClemensClock* clock);

void clem_vgc_scanline_enable_int(struct ClemensVGC* vgc, bool enable);

void clem_vgc_set_mode(struct ClemensVGC* vgc, unsigned mode_flags);
void clem_vgc_clear_mode(struct ClemensVGC* vgc, unsigned mode_flags);
void clem_vgc_set_text_colors(struct ClemensVGC* vgc, unsigned fg_color,
                              unsigned bg_color);

void clem_vgc_set_region(struct ClemensVGC* vgc, uint8_t c02b_value);
uint8_t clem_vgc_get_region(struct ClemensVGC* vgc);

uint8_t clem_vgc_read_switch(struct ClemensVGC* vgc,
                             struct ClemensClock* clock,
                             uint8_t ioreg,
                             uint8_t flags);

void clem_vgc_write_switch(struct ClemensVGC* vgc,
                          struct ClemensClock* clock,
                          uint8_t ioreg,
                          uint8_t value);

void clem_vgc_shgr_scanline(struct ClemensScanline* scanline,
                            uint8_t* dest, const uint8_t* src_bank);

#ifdef __cplusplus
}
#endif

#endif
