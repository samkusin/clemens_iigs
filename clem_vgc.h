#ifndef CLEM_DEVICE_H
#define CLEM_DEVICE_H

#include "clem_types.h"

/**
 * Video Interface
 * These functions emulate the various peripherals (internal and external)
 */

#ifdef __cplusplus
extern "C" {
#endif


void clem_vgc_init(struct ClemensVGC* vgc);

uint32_t clem_vgc_sync(struct ClemensVGC* vgc, uint32_t delta_us,
                       uint32_t irq_line)

#ifdef __cplusplus
}
#endif

#endif
