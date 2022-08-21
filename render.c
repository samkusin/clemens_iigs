#include "render.h"

/* 320x200 scanline mode renders 2x2 pixels to the buffer.
   640x200 scanline mode renders 1x2 pixels to the buffer.

   Each scanline has a control register that follows the IIgs scanline byte
   Dithering (in 640x200 mode) must occur in the host implementation (i.e.
   via shaders is likely the easiest and most versatile method.)  This
   implementation just generates indices into the palette table.

   Ch 4. IIgs Hardware Reference

 */

/* IN all of these functions, it's assumed out_x_limit is aligned with 4 pixels
 */
static void _render_super_hires_320(const uint8_t* scan_row,
                                    unsigned scan_control, unsigned scan_cnt,
                                    uint8_t* out_row, unsigned out_x_limit,
                                    unsigned x_step) {
  unsigned scan_x = 0, out_x = 0;
  uint8_t palette_off =
    (scan_control & CLEM_VGC_SCANLINE_PALETTE_INDEX_MASK) << 4;
  uint8_t pixel;
  for (; scan_x < scan_cnt && out_x < out_x_limit; ++scan_x) {
    pixel = scan_row[scan_x] >> 4;
    out_row[out_x] = palette_off + pixel;
    out_x += x_step;
    out_row[out_x] = palette_off + pixel;
    out_x += x_step;
    pixel = scan_row[scan_x] & 0xf;
    out_row[out_x] = palette_off + pixel;
    out_x += x_step;
    out_row[out_x] = palette_off + pixel;
    out_x += x_step;
  }
}

static void _render_super_hires_320_fill(const uint8_t* scan_row,
                                         unsigned scan_control, unsigned scan_cnt,
                                         uint8_t* out_row, unsigned out_x_limit,
                                         unsigned x_step) {

  unsigned scan_x = 0, out_x = 0;
  uint8_t palette_off =
    (scan_control & CLEM_VGC_SCANLINE_PALETTE_INDEX_MASK) << 4;
  // NOTE: HW ref says these values are undetermined if the first scan byte is
  //       zero.   Rather than emulating this undetermined behavior, set as 0
  //       which will mean palette index 0
  uint8_t pixel_a = 0, pixel_b = 0;
  while (scan_x < scan_cnt && out_x < out_x_limit) {
    if (scan_row[scan_x]) {
      pixel_a = scan_row[scan_x] >> 4;
      pixel_b = scan_row[scan_x] & 0xf;
    }
    out_row[out_x] = palette_off + pixel_a;
    out_x += x_step;
    out_row[out_x] = palette_off + pixel_a;
    out_x += x_step;
    out_row[out_x] = palette_off + pixel_b;
    out_x += x_step;
    out_row[out_x] = palette_off + pixel_a;
    out_x += x_step;
  }

}

static void _render_super_hires_640(const uint8_t* scan_row,
                                    unsigned scan_control, unsigned scan_cnt,
                                    uint8_t* out_row, unsigned out_x_limit,
                                    unsigned x_step) {
  // See Ch 4, Table 4-21 IIgs HW Ref.
  // Palette offset cycles from +8, +12, +0, +4 offset into a palette row
  // starting at column 0, 1, 2, 3 and so forth.
  // Dithering will be performed by the host as this step doesn't output RGBA
  // values but palette indices (like in 320 mode)
  unsigned scan_x = 0, out_x = 0;
  uint8_t palette_off =
    (scan_control & CLEM_VGC_SCANLINE_PALETTE_INDEX_MASK) << 4;
  uint8_t pixel;
  for (; scan_x < scan_cnt && out_x < out_x_limit; ++scan_x) {
    pixel = scan_row[scan_x] >> 6;
    out_row[out_x] = (palette_off + 0x08) + pixel;
    out_x += x_step;
    pixel = (scan_row[scan_x] >> 4) & 0x3;
    out_row[out_x] = (palette_off + 0x0c) + pixel;
    out_x += x_step;
    pixel = (scan_row[scan_x] >> 2) & 0x3;
    out_row[out_x] = (palette_off + 0x00) + pixel;
    out_x += x_step;
    pixel = scan_row[scan_x] & 0x3;
    out_row[out_x] = (palette_off + 0x04) + pixel;
    out_x += x_step;
  }
}

static void _render_super_hires(const ClemensVideo* video, const uint8_t* memory,
                                uint8_t* texture, unsigned width, unsigned height,
                                unsigned stride, unsigned x_step) {
  uint8_t* out_row = texture;
  unsigned scanline_end = video->scanline_start + video->scanline_count;
  unsigned scan_control;
  unsigned scan_y;
  for (scan_y = video->scanline_start; scan_y < scanline_end; ++scan_y) {
    scan_control = video->scanlines[scan_y].control;
    if (scan_control & CLEM_VGC_SCANLINE_CONTROL_640_MODE) {
      _render_super_hires_640(memory + video->scanlines[scan_y].offset,
                              video->scanlines[scan_y].control,
                              video->scanline_byte_cnt,
                              out_row, width, x_step);
    } else if (scan_control & CLEM_VGC_SCANLINE_COLORFILL_MODE) {
      _render_super_hires_320_fill(memory + video->scanlines[scan_y].offset,
                                   video->scanlines[scan_y].control,
                                   video->scanline_byte_cnt,
                                   out_row, width, x_step);
    } else {
      _render_super_hires_320(memory + video->scanlines[scan_y].offset,
                              video->scanlines[scan_y].control,
                              video->scanline_byte_cnt,
                              out_row, width, x_step);
    }
    out_row += stride;
  }
}

void clemens_render_video(const ClemensVideo* video, const uint8_t* memory,
                          uint8_t* texture, unsigned width, unsigned height,
                          unsigned stride,
                          unsigned x_step) {

  switch (video->format) {
    case kClemensVideoFormat_Super_Hires:
      _render_super_hires(video, memory, texture, width, height, stride, x_step);
      break;
    case kClemensVideoFormat_Double_Hires:
      break;
    case kClemensVideoFormat_Hires:
      break;
    case kClemensVideoFormat_Double_Lores:
      break;
    case kClemensVideoFormat_Lores:
      break;
  }

}
