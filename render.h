#ifndef CLEM_RENDER_H
#define CLEM_RENDER_H

#include "clem_types.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Renders indexed color into an 8-bit texture
 *
 * Note to support all graphics rendering modes, the output texture should be
 * at least 640 x 400 texels.  Hires and Super hires 320 pixels are scaled 2x2
 * to fill out the texture.  Double hires and 640 mode render pixels at 1x2.
 *
 * @param video
 * @param memory
 * @param aux
 * @param texture
 * @param width
 * @param height
 * @param stride
 */
void clemens_render_graphics(const ClemensVideo *video, const uint8_t *memory, const uint8_t *aux,
                             uint8_t *texture, unsigned width, unsigned height, unsigned stride);

#ifdef __cplusplus
}
#endif

#endif
