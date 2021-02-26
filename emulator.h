#ifndef CLEM_EMULATOR_H
#define CLEM_EMULATOR_H

#include "clem_types.h"

#ifdef __cplusplus
extern "C" {
#endif

int clemens_init(
    ClemensMachine* machine,
    uint32_t speed_factor,
    uint32_t clocks_step,
    void* rom,
    size_t romSize
);

void emulate(ClemensMachine* clem);

#ifdef __cplusplus
}
#endif

#endif
