#ifndef CLEM_SMARTPORT_PRODOS_HDD32_H
#define CLEM_SMARTPORT_PRODOS_HDD32_H

#include "clem_shared.h"

#ifdef __cplusplus
extern "C" {
#endif

struct ClemensProdosHDD32 {
    unsigned foo;
};

void clem_smartport_prodos_hdd32_initialize(struct ClemensSmartPortUnit *unit,
                                            struct ClemensProdosHDD32 *device);
void clem_smartport_prodos_hdd32_uninitialize(struct ClemensSmartPortUnit *unit);

#ifdef __cplusplus
}
#endif
#endif
