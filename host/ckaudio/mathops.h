#ifndef CINEK_MATH_OPS_H
#define CINEK_MATH_OPS_H

#include <stdint.h>

#define CK_PI_2 6.28318531f

#ifdef __cplusplus
extern "C" {
#endif

extern inline void ck_op_u32_min(uint32_t *vo, uint32_t v0, uint32_t v1) {
    *vo = v0 < v1 ? v0 : v1;
}

extern inline void ck_op_u32_max(uint32_t *vo, uint32_t v0, uint32_t v1) {
    *vo = v0 > v1 ? v0 : v1;
}

extern void ck_op_sinf(float *vo, float radians);

extern inline void ck_op_pcm_unsigned_to_float(float *vo, uint16_t v0) {
    *vo = v0 / 32767.0f - 1.0f;
}

#ifdef __cplusplus
}
#endif

#endif
