#ifndef CLEM_VGC_H
#define CLEM_VGC_H

#include "clem_types.h"

#define CLEM_VGC_GRAPHICS_MODE     0x00000001
#define CLEM_VGC_MIXED_TEXT        0x00000002
#define CLEM_VGC_80COLUMN_TEXT     0x00000004
#define CLEM_VGC_LORES             0x00000008
#define CLEM_VGC_HIRES             0x00000010
#define CLEM_VGC_DBLHIRES          0x00000020
#define CLEM_VGC_SUPER_HIRES       0x00000040
#define CLEM_VGC_ALTCHARSET        0x00010000
#define CLEM_VGC_MONOCHROME        0x00020000
#define CLEM_VGC_PAL               0x00040000
#define CLEM_VGC_LANGUAGE          0x00080000

/**
 * Video Interface
 * These functions emulate the various peripherals (internal and external)
 */

#ifdef __cplusplus
extern "C" {
#endif


void clem_vgc_init(struct ClemensVGC* vgc);

uint32_t clem_vgc_sync(struct ClemensVGC* vgc, uint32_t delta_ns,
                       uint32_t irq_line);

void clem_vgc_set_mode(struct ClemensVGC* vgc, unsigned mode_flags);
void clem_vgc_clear_mode(struct ClemensVGC* vgc, unsigned mode_flags);
void clem_vgc_set_text_colors(struct ClemensVGC* vgc, unsigned fg_color,
                              unsigned bg_color);

void clem_vgc_set_region(struct ClemensVGC* vgc, uint8_t c02b_value);
uint8_t clem_vgc_get_region(struct ClemensVGC* vgc);

#ifdef __cplusplus
}
#endif

#endif
