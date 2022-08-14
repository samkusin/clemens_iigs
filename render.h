#ifndef CLEM_RENDER_H
#define CLEM_RENDER_H

#include "clem_types.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Renders indexed color (16 colors x 16 palettes) into an 8-bit texture
 *
 * Note this method provides stride and x_step as a method to implement a form
 * of scaling the result by the host.  This function provides the reference
 * bitmap with 'holes' that the host can fill in with pixels to scale up the
 * image.  This is to support targets that want to generate a larger, blockier
 * screen result to perform various effects.
 *
 * If the output texture scale matches the emulated screen's dimensions (1:1),
 * then x_step == 1 and stride == byte width of the texture row.
 *
 * To scale 2x2, set x_step == 2 and stride == row_byte_size * 2, and so on.
 * The result will not fill in the pixels at (1, 0), (0, 1) and (1, 1) of the
 * output pixel - that will be left to the host implementation as described
 * above.
 *
 * @param video
 * @param memory
 * @param texture
 * @param width
 * @param height
 * @param stride
 * @param x_step
 */
void clemens_render_video(const ClemensVideo* video, const uint8_t* memory,
                          uint8_t* texture,
                          unsigned width, unsigned height, unsigned stride,
                          unsigned x_step);

#ifdef __cplusplus
}
#endif

#endif
