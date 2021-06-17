#include "clem_vgc.h"
#include "clem_util.h"

/* References:

   Vertical/Horizontal Counters and general VBL timings
   http://www.1000bit.it/support/manuali/apple/technotes/iigs/tn.iigs.039.html

   VBL particulars:
   http://www.1000bit.it/support/manuali/apple/technotes/iigs/tn.iigs.040.html
*/

#define CLEM_VGC_VSYNC_TIME_NS  (1e9/60)

void clem_vgc_init(struct ClemensVGC* vgc) {
    /* setup scanline maps for all of the different modes */
    ClemensVideo* video;
    struct ClemensScanline* line;
    unsigned offset;
    unsigned row, inner;

    vgc->timer_ns = 0;
    vgc->vbl_counter = 0;

    vgc->text_fg_color = CLEM_VGC_COLOR_WHITE;
    vgc->text_bg_color = CLEM_VGC_COLOR_MEDIUM_BLUE;

    /*  text page 1 $0400-$07FF, page 2 = $0800-$0BFF

        rows (0, 8, 16), (1, 9, 17), (2, 10, 18), etc.. increment by 40, for
        every row in the tuple.  When advancing to next tuple of rows, account
        for 8 byte 'hole' (so row 0 = +0, row 1 = +128, row 2 = +256)
    */
    line = &vgc->text_1_scanlines[0];
    offset = 0x400;
    for (row = 0; row < 8; ++row, ++line) {
        line[0].offset = offset;            line[0].meta = 0;
        line[8].offset = offset + 40;       line[8].meta = 0;
        line[16].offset = offset + 80;      line[16].meta = 0;
        offset += 128;
    }
    line = &vgc->text_2_scanlines[0];
    for (row = 0; row < 8; ++row, ++line) {
        line[0].offset = offset;            line[0].meta = 0;
        line[8].offset = offset + 40;       line[8].meta = 0;
        line[16].offset = offset + 80;      line[16].meta = 0;
        offset += 128;
    }
    /*  hgr page 1 $2000-$3FFF, page 2 = $4000-$5FFF

        line text but each "row" is 8 pixels high and offsets increment by 1024
        of course, each byte is 7 pixels + 1 palette bit, which keeps the
        familiar +40, +80 increments used in text scanline calculation.
    */
    line = &vgc->hgr_1_scanlines[0];
    offset = 0x2000;
    for (row = 0; row < 8; ++row, ++line) {
        for (inner = 0; inner < 8; ++inner) {
            line[0+inner].offset = offset + inner*1024;
            line[0+inner].meta = 0;
        }
        for (inner = 0; inner < 8; ++inner) {
            line[64+inner].offset = offset + 40 + inner*1024;
            line[64+inner].meta = 0;
        }
        for (inner = 0; inner < 8; ++inner) {
            line[128+inner].offset = offset + 80 + inner*1024;
            line[128+inner].meta = 0;
        }
        offset += 128;
    }
    line = &vgc->hgr_2_scanlines[0];
    for (row = 0; row < 8; ++row, ++line) {
        for (inner = 0; inner < 8; ++inner) {
            line[0+inner].offset = offset + inner*1024;
            line[0+inner].meta = 0;
        }
        for (inner = 0; inner < 8; ++inner) {
            line[64+inner].offset = offset + 40 + inner*1024;
            line[64+inner].meta = 0;
        }
        for (inner = 0; inner < 8; ++inner) {
            line[128+inner].offset = offset + 80 + inner*1024;
            line[128+inner].meta = 0;
        }
        offset += 128;
    }
    line = &vgc->shgr_scanlines[0];
    offset = 0x2000;
    for (row = 0; row < 200; ++row, ++line) {
        line->offset = offset;
        line->meta = 0;         /* this is the scanline control register */
        offset += 160;
    }
}

void clem_vgc_set_mode(struct ClemensVGC* vgc, unsigned mode_flags) {
    if (mode_flags & CLEM_VGC_RESOLUTION_MASK) {
        mode_flags &= ~CLEM_VGC_RESOLUTION_MASK;
    }
    vgc->mode_flags |= mode_flags;
}

void clem_vgc_clear_mode(struct ClemensVGC* vgc, unsigned mode_flags) {
    vgc->mode_flags &= ~mode_flags;
}

void clem_vgc_set_text_colors(
    struct ClemensVGC* vgc,
    unsigned fg_color,
    unsigned bg_color
) {
    vgc->text_fg_color = fg_color;
    vgc->text_bg_color = bg_color;
}

void clem_vgc_set_region(struct ClemensVGC* vgc, uint8_t c02b_value) {
    if (c02b_value & 0x08) {
        clem_vgc_set_mode(vgc, CLEM_VGC_LANGUAGE);
    } else {
        clem_vgc_clear_mode(vgc, CLEM_VGC_LANGUAGE);
    }
    if (c02b_value & 0x10) {
        clem_vgc_set_mode(vgc, CLEM_VGC_PAL);
    } else {
        clem_vgc_clear_mode(vgc, CLEM_VGC_PAL);
    }
    vgc->text_language = (c02b_value & 0xe0) >> 5;
}

uint8_t clem_vgc_get_region(struct ClemensVGC* vgc) {
    uint8_t result = (vgc->mode_flags & CLEM_VGC_LANGUAGE) ? 0x08 : 0x00;
    if (vgc->mode_flags & CLEM_VGC_PAL) result |= 0x10;
    result |= (uint8_t)((vgc->text_language << 5) & 0xe0);
    return result;
}



void clem_vgc_sync(
    struct ClemensVGC* vgc,
    struct ClemensClock* clock
) {
    /* TODO: OMG -
        my calculation of delta_clocks/ref_step is not going to work
        because of resolution issues (2 fast cycles/ 1 mega cycle == 0)
        this is likely part of the problems with the IWM timing

        Solution is using an accumulator of clocks that is incremented until
        we can 'exhaust it'.  Continue this over every frame.

    */
    unsigned delta_ns = _clem_calc_ns_step_from_clocks(
        clock->ts - vgc->last_clocks_ts,  clock->ref_step);
    vgc->timer_ns += delta_ns;

    while (vgc->timer_ns >= CLEM_VGC_VSYNC_TIME_NS) {

        ++vgc->vbl_counter;
        vgc->timer_ns -= CLEM_VGC_VSYNC_TIME_NS;
    }
    vgc->last_clocks_ts = clock->ts;
}
