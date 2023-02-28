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
static void _render_super_hires_320(const uint8_t *scan_row, unsigned scan_control,
                                    unsigned scan_cnt, uint8_t *out_row, unsigned out_x_limit) {
    unsigned scan_x = 0, out_x = 0;
    uint8_t palette_off = (scan_control & CLEM_VGC_SCANLINE_PALETTE_INDEX_MASK) << 4;
    uint8_t pixel;
    for (; scan_x < scan_cnt && out_x < out_x_limit; ++scan_x) {
        pixel = scan_row[scan_x] >> 4;
        out_row[out_x] = palette_off + pixel;
        out_x++;
        out_row[out_x] = palette_off + pixel;
        out_x++;
        pixel = scan_row[scan_x] & 0xf;
        out_row[out_x] = palette_off + pixel;
        out_x++;
        out_row[out_x] = palette_off + pixel;
        out_x++;
    }
}

static void _render_super_hires_320_fill(const uint8_t *scan_row, unsigned scan_control,
                                         unsigned scan_cnt, uint8_t *out_row,
                                         unsigned out_x_limit) {

    unsigned scan_x = 0, out_x = 0;
    uint8_t palette_off = (scan_control & CLEM_VGC_SCANLINE_PALETTE_INDEX_MASK) << 4;
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
        out_x++;
        out_row[out_x] = palette_off + pixel_a;
        out_x++;
        out_row[out_x] = palette_off + pixel_b;
        out_x++;
        out_row[out_x] = palette_off + pixel_a;
        out_x++;
    }
}

static void _render_super_hires_640(const uint8_t *scan_row, unsigned scan_control,
                                    unsigned scan_cnt, uint8_t *out_row, unsigned out_x_limit) {
    // See Ch 4, Table 4-21 IIgs HW Ref.
    // Palette offset cycles from +8, +12, +0, +4 offset into a palette row
    // starting at column 0, 1, 2, 3 and so forth.
    // Dithering will be performed by the host as this step doesn't output RGBA
    // values but palette indices (like in 320 mode)
    unsigned scan_x = 0, out_x = 0;
    uint8_t palette_off = (scan_control & CLEM_VGC_SCANLINE_PALETTE_INDEX_MASK) << 4;
    uint8_t pixel;
    for (; scan_x < scan_cnt && out_x < out_x_limit; ++scan_x) {
        pixel = scan_row[scan_x] >> 6;
        out_row[out_x] = (palette_off + 0x08) + pixel;
        out_x++;
        pixel = (scan_row[scan_x] >> 4) & 0x3;
        out_row[out_x] = (palette_off + 0x0c) + pixel;
        out_x++;
        pixel = (scan_row[scan_x] >> 2) & 0x3;
        out_row[out_x] = (palette_off + 0x00) + pixel;
        out_x++;
        pixel = scan_row[scan_x] & 0x3;
        out_row[out_x] = (palette_off + 0x04) + pixel;
        out_x++;
    }
}

static void _render_super_hires(const ClemensVideo *video, const uint8_t *memory, uint8_t *texture,
                                unsigned width, unsigned height, unsigned stride) {
    uint8_t *out_row = texture;
    unsigned scanline_end = video->scanline_start + video->scanline_count;
    unsigned scan_control;
    unsigned scan_y;
    for (scan_y = video->scanline_start; scan_y < scanline_end; ++scan_y) {
        scan_control = video->scanlines[scan_y].control;
        if (scan_control & CLEM_VGC_SCANLINE_CONTROL_640_MODE) {
            _render_super_hires_640(memory + video->scanlines[scan_y].offset,
                                    video->scanlines[scan_y].control, video->scanline_byte_cnt,
                                    out_row, width);
        } else if (scan_control & CLEM_VGC_SCANLINE_COLORFILL_MODE) {
            _render_super_hires_320_fill(memory + video->scanlines[scan_y].offset,
                                         video->scanlines[scan_y].control, video->scanline_byte_cnt,
                                         out_row, width);
        } else {
            _render_super_hires_320(memory + video->scanlines[scan_y].offset,
                                    video->scanlines[scan_y].control, video->scanline_byte_cnt,
                                    out_row, width);
        }
        out_row += stride;
        out_row += stride;
    }
}

////////////////////////////////////////////////////////////////////////////////
//  HGR colors black, green/orange (odd), violet/blue (even), white
//    violet even ; green odd   (hcolor 2, 1)
//    orange even ; blue odd    (hcolor 5, 6)
//
//  row index comes from the CLEM_RENDER_HIRES_COLOR_TYPE_XXX constants below
static uint8_t indexFromHGRBitTable[4][2] = {
    {0, 4}, /* black */
    {2, 6}, /* even */
    {1, 5}, /* odd */
    {3, 7}  /* white */
};

//  Describes how to render a specific bit string for hires mode
//  Generally speaking when encountering certain bit strings, our renderer
//  can decide between black, white or color.
//  Color is determined by the X coordinate and Bit 7 of the current byte

// clang-format off
//  Selected color is always black
#define CLEM_RENDER_HIRES_COLOR_TYPE_BLACK   0x00
//  Selected color determined by the color of the preceding X position
#define CLEM_RENDER_HIRES_COLOR_TYPE_COLOR_0 0x01
//  Selected color is determined by the current at the current X position
#define CLEM_RENDER_HIRES_COLOR_TYPE_COLOR_1 0x02
//  Selected color is always white
#define CLEM_RENDER_HIRES_COLOR_TYPE_WHITE   0x03
//
//  There are 8 possible bit combinations which provide enough information to
//  select one of the three types described above.
//                                              +-This bit represents the current X
static uint8_t s_bitpixelToColorType[8] = { //  |
    CLEM_RENDER_HIRES_COLOR_TYPE_BLACK,     // 000
    CLEM_RENDER_HIRES_COLOR_TYPE_BLACK,     // 001
    CLEM_RENDER_HIRES_COLOR_TYPE_COLOR_1,   // 010
    CLEM_RENDER_HIRES_COLOR_TYPE_WHITE,     // 011
    CLEM_RENDER_HIRES_COLOR_TYPE_BLACK,     // 100
    CLEM_RENDER_HIRES_COLOR_TYPE_COLOR_0,   // 101
    CLEM_RENDER_HIRES_COLOR_TYPE_WHITE,     // 110
    CLEM_RENDER_HIRES_COLOR_TYPE_WHITE,     // 111
};
// clang-format on

static void a2hgrToIndexedColor2x2(uint8_t *pixout, uint8_t *pixout2, const uint8_t *scanline,
                                   int scanline_byte_cnt) {
    //  bits are pushed onto the shifter as we scan across the screen, so higher bits == past pixels
    unsigned x_pos = 0;
    int scanline_byte_index = 0;
    int remainder = 0;
    uint8_t scanline_byte = *scanline;
    uint8_t palette = scanline_byte >> 7;
    uint8_t shifter = scanline_byte & 0x1;
    uint8_t palette_shifter = palette;

    scanline_byte >>= 1;
    while (scanline_byte_index < scanline_byte_cnt || remainder > 0) {
        uint8_t pixel;
        uint8_t group;
        //  Ingest bit here - since we care only about bits 0-2, and bit 1 is the pixel
        //  at the current X
        palette_shifter <<= 1;
        palette_shifter |= palette;
        shifter <<= 1;
        shifter |= (scanline_byte & 0x1);
        shifter &= 0x7;
        scanline_byte >>= 1;
        //  determine color to plot from shifter, x_pos and palette
        //  next pixel.  the palette shifter bit 1 is the group color for the
        //  current x position (vs bit 0, which points to x + 1, and bit 2 to x - 1)
        pixel = s_bitpixelToColorType[shifter];
        group = (palette_shifter >> 1) & 0x1;
        switch (pixel) {
        case CLEM_RENDER_HIRES_COLOR_TYPE_BLACK:
            pixel = indexFromHGRBitTable[0][group];
            break;
        case CLEM_RENDER_HIRES_COLOR_TYPE_COLOR_0:
            pixel = indexFromHGRBitTable[1 + ((x_pos + 1) & 1)][group];
            break;
        case CLEM_RENDER_HIRES_COLOR_TYPE_COLOR_1:
            pixel = indexFromHGRBitTable[1 + (x_pos & 1)][group];
            break;
        case CLEM_RENDER_HIRES_COLOR_TYPE_WHITE:
            pixel = indexFromHGRBitTable[3][group];
            break;
        }

        //  draw it
        //  normalize hcolor 0 to 7 to 0-255 to be shader friendly
        pixel <<= 5;
        pixel += 16;
        *(pixout++) = pixel;
        *(pixout++) = pixel;
        *(pixout2++) = pixel;
        *(pixout2++) = pixel;
        if (remainder > 0)
            remainder--;
        x_pos++;
        if (((x_pos + 1) % 7) == 0) {
            scanline_byte_index++;
            if (scanline_byte_index < scanline_byte_cnt) {
                scanline_byte = scanline[scanline_byte_index];
                palette = scanline_byte >> 7;
            } else {
                remainder = 1;
            }
        }
    }
}

static void _render_hires(const ClemensVideo *video, const uint8_t *memory, uint8_t *texture,
                          unsigned width, unsigned height, unsigned stride) {
    //  draw the graphics data with the incredible A2 hires color rules in mind
    //  and scale in software the pixels to 2x2 so they conform to our output
    //  texture size (which is 4x the size of a 280x192 screen)
    for (int i = 0; i < video->scanline_count; ++i) {
        int row = i + video->scanline_start;
        const uint8_t *scanline = memory + video->scanlines[row].offset;
        uint8_t *pixout = texture + i * 2 * stride;
        a2hgrToIndexedColor2x2(pixout, pixout + stride, scanline, video->scanline_byte_cnt);
    }
}

////////////////////////////////////////////////////////////////////////////////

//  Interesting...
//    References: Patent - US4786893A
//    "Method and apparatus for generating RGB color signals from composite
//      digital video signal"
//
//    https://patents.google.com/patent/US4786893A/en?oq=US4786893
//
//    Patent seems to refer to Apple II composite signals converted to RGB
//    using the sliding bit window as referred to in the Hardware Reference.
//    It's likely this method is used in the IIgs - and so it's good enough for
//    a baseline (doesn't fix IIgs Double Hires issues related to artifacting
//    that allows better quality for NTSC hardware... which is another issue)
//
//  Notes:
//    The "Prior Art Method" described in the patent matches my first naive
//    implementation (4 bits per effective pixel = the color.)  The problem
//    with this is that data is streamed serially to the controller vs on a per
//    nibble basis.  This becomes an issue when transitioning between colors
//    and the 4-bit color isn't aligned on the nibble.
//
//  Concept:
//    Implement a version of the "Present Invention" from the patent
//    - Given the most recent bit from the bitstream
//    - if the result indicates a color pattern change, then render the
//      original color until the color pattern change occurs
//
//  Details:
//    Bit stream: incoming from pixin
//    Shift register:
//        history (most recent 4 bits being relevant)
//    Barrel shifter:
//        the original color at the start of the 4-bit string
//        (this can be simplified in software as just a stored-off value)
//    Color Change Test:
//        If Shift Register Bit 3 != incoming bit, then color change
//    Plot:
//        If Color Change, Select Latch Color
//        Else Select Barrel Shifted Color (Current)
//        Set Latch color to selected color
//    Latched Color:
//        Initially Zero
//
//  This is a literal translation of the patent's Fig. 4 - which works pretty
//  well to emulate the IIgs implementation.   This could be optimized via
//  lookup tables.
//
//  TODO: optimize using lookup tables
//

//  scanlines[2] = {aux_memory,main_memory} interleaved for the scanline.
//
static void a2dhgrToIndexedRGB1x2(uint8_t *pixout0, uint8_t *pixout1, const uint8_t *scanlines[2],
                                  int scanline_byte_cnt) {
    int pixin_byte_index = 0;
    int clock_ctr = 0; // 1 bit per clock cycle
    uint8_t pixin_byte = *scanlines[0];
    unsigned shifter = 0x00;
    unsigned barrel = 0x00;
    unsigned latch = 0x00;
    unsigned latch_counter = 0;
    unsigned tail_counter = 0;

    scanline_byte_cnt <<= 1; // account for both scanlines/40+40
    while (pixin_byte_index < scanline_byte_cnt || tail_counter > 0) {
        unsigned barrel_rotate = clock_ctr % 4;
        bool pixin_bit = (pixin_byte & 0x1);
        bool shifter_hi_bit = (shifter & 0x8) >> 3;
        bool changed = pixin_bit != shifter_hi_bit;
        uint8_t pixout;
        uint8_t next_latch;
        //  barrel rotate in an attempt to retain the original nibble for comparison
        //  with the incoming shift register pattern
        barrel = shifter >> barrel_rotate;
        barrel |= (shifter << (4 - barrel_rotate));
        barrel &= 0xf;

        if (latch_counter == 0 || barrel == 0xf || barrel == 0x0) {
            latch = barrel;
        }

        //  output indexed RGB color
        // scales the 4-bit color to 8-bit.  this works well with color maps define 16-pixels per
        // color horizontal so UVs can be scaled appropriately from 0 to 1 without rounding or
        // worries abound text bleed.  The + 8 makes this resolution 5-bit (xxxx1000) where xxxx is
        // the latch
        if (clock_ctr >= 4) {
            pixout = (latch << 4) + 8;
            *(pixout0++) = pixout;
            *(pixout1++) = pixout;
        }

        if (shifter == 0 || shifter == 0xf) {
            latch_counter = 3;
        } else if (latch_counter > 0) {
            latch_counter--;
        } else {
            if (changed) {
                latch_counter = 3 - barrel_rotate;
            }
        }

        //  next clock
        clock_ctr++;
        if (tail_counter > 0) {
            tail_counter--;
        }

        //  apply bit to shift register
        shifter <<= 1;
        shifter |= (pixin_bit ? 1 : 0);
        shifter &= 0xf;
        //  advance to next byte in the video stream
        pixin_byte >>= 1;
        if ((clock_ctr % 7) == 0) {
            if (pixin_byte_index < scanline_byte_cnt) {
                ++scanlines[pixin_byte_index % 2];
                ++pixin_byte_index;
                pixin_byte = *scanlines[pixin_byte_index % 2];
            }
            if (pixin_byte_index == scanline_byte_cnt) {
                tail_counter = 4;
            }
        }
    }
}

static void _render_double_hires(const ClemensVideo *video, const uint8_t *main, const uint8_t *aux,
                                 uint8_t *texture, unsigned width, unsigned height,
                                 unsigned stride) {
    //  An oversimplication of double hires reads that the 'effective' resolution
    //  is 4 pixels per color (so 140x192 - let's say a color is a 'block' of 4
    //  pixels.   Since a block is a 4-bit pattern representing actual pixels on
    //  the screen, adjacent blocks to the current block of interest will effect
    //  this block.   To best handle the 'bit per pixel' method of rendering,
    //  where the pixel color is determine by past state, our plotter will
    //  'slide' along the bit array.  At some point the plotter will decide what
    //  color to render at an earlier point in the array and proceed ahead.
    //
    for (int y = 0; y < video->scanline_count; ++y) {
        int row = y + video->scanline_start;
        const uint8_t *pixsources[2] = {aux + video->scanlines[row].offset,
                                        main + video->scanlines[row].offset};
        uint8_t *pixout = texture + y * 2 * stride;
        a2dhgrToIndexedRGB1x2(pixout, pixout + stride, pixsources, video->scanline_byte_cnt);
    }
}

////////////////////////////////////////////////////////////////////////////////

void clemens_render_graphics(const ClemensVideo *video, const uint8_t *memory, const uint8_t *aux,
                             uint8_t *texture, unsigned width, unsigned height, unsigned stride) {

    switch (video->format) {
    case kClemensVideoFormat_None:
        break;
    case kClemensVideoFormat_Super_Hires:
        _render_super_hires(video, memory, texture, width, height, stride);
        break;
    case kClemensVideoFormat_Double_Hires:
        _render_double_hires(video, memory, aux, texture, width, height, stride);
        break;
    case kClemensVideoFormat_Hires:
        _render_hires(video, memory, texture, width, height, stride);
        break;
    case kClemensVideoFormat_Lores:
    case kClemensVideoFormat_Double_Lores:
    case kClemensVideoFormat_Text:
        //  This is currently handled as drawing characters in the host implementation
        break;
    }
}

#if defined(CLEM_RENDER_SAMPLE)

#endif
