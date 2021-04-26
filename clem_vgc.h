#ifndef CLEM_VGC_H
#define CLEM_VGC_H

#include "clem_types.h"

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

#ifdef __cplusplus
}
#endif

#endif
