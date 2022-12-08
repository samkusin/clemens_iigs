#ifndef CLEM_SMARTPORT_H
#define CLEM_SMARTPORT_H

#include "clem_shared.h"
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

bool clem_smartport_do_reset(struct ClemensSmartPortUnit *unit, unsigned unit_count,
                             unsigned *io_flags, unsigned *out_phase, unsigned delta_ns);

bool clem_smartport_do_enable(struct ClemensSmartPortUnit *unit, unsigned unit_count,
                              unsigned *io_flags, unsigned *out_phase, unsigned delta_ns);

#ifdef __cplusplus
}
#endif

#endif
