#include "clem_vgc.h"
#include "clem_debug.h"
#include "clem_mmio_defs.h"
#include "clem_shared.h"
#include "clem_util.h"

#include <inttypes.h>

/* References:

   Vertical/Horizontal Counters and general VBL timings
   http://www.1000bit.it/support/manuali/apple/technotes/iigs/tn.iigs.039.html

   VBL particulars:
   http://www.1000bit.it/support/manuali/apple/technotes/iigs/tn.iigs.040.html
*/

static inline void _clem_vgc_set_scanline_int(struct ClemensVGC *vgc, bool enable) {
    if (enable) {
        vgc->irq_line |= CLEM_IRQ_VGC_SCAN_LINE;
    } else {
        vgc->irq_line &= ~CLEM_IRQ_VGC_SCAN_LINE;
    }
}

static inline unsigned _clem_vgc_calc_h_counter(clem_clocks_duration_t duration,
                                                clem_clocks_duration_t ref_step) {
    return (duration / clem_calc_clocks_step_from_ns(980, ref_step)) & 0x7f; /* 7 bits */
}

static bool _clem_vgc_is_scanline_int_enabled(const uint8_t *mega2_e1, unsigned v_counter) {
    if (v_counter >= CLEM_VGC_FIRST_VISIBLE_SCANLINE_CNTR) {
        v_counter -= CLEM_VGC_FIRST_VISIBLE_SCANLINE_CNTR;
        if (v_counter < CLEM_VGC_SHGR_SCANLINE_COUNT) {
            return (mega2_e1[0x9d00 + v_counter] & CLEM_VGC_SCANLINE_CONTROL_INTERRUPT) != 0;
        }
    }
    return false;
}

static void _clem_vgc_scanline_build_rgb_palette(struct ClemensVGC *vgc, unsigned v_counter,
                                                 const uint8_t *mega2_e1) {
    uint16_t *rgb_dest;
    const uint8_t *rgb_src;
    unsigned palette, i;
    if (v_counter >= CLEM_VGC_FIRST_VISIBLE_SCANLINE_CNTR) {
        v_counter -= CLEM_VGC_FIRST_VISIBLE_SCANLINE_CNTR;
        if (v_counter >= CLEM_VGC_SHGR_SCANLINE_COUNT) {
            return;
        }
    }
    palette = mega2_e1[0x9d00 + v_counter] & CLEM_VGC_SCANLINE_PALETTE_INDEX_MASK;
    rgb_src = &mega2_e1[0x9e00 + palette * 32];
    rgb_dest = &vgc->shgr_palettes[16 * v_counter];
    for (i = 0; i < 16; ++i, rgb_src += 2, rgb_dest++) {
        *rgb_dest = ((uint16_t)(rgb_src[1]) << 8) | rgb_src[0];
    }
}

void clem_vgc_reset(struct ClemensVGC *vgc) {
    /* setup scanline maps for all of the different modes */
    ClemensVideo *video;
    struct ClemensScanline *line;
    unsigned offset;
    unsigned row, inner;

    vgc->mode_flags = CLEM_VGC_INIT;
    vgc->text_fg_color = CLEM_VGC_COLOR_WHITE;
    vgc->text_bg_color = CLEM_VGC_COLOR_MEDIUM_BLUE;
    vgc->scanline_irq_enable = false;
    vgc->vbl_started = false;
    vgc->vbl_counter = 0;

    /*  text page 1 $0400-$07FF, page 2 = $0800-$0BFF

        rows (0, 8, 16), (1, 9, 17), (2, 10, 18), etc.. increment by 40, for
        every row in the tuple.  When advancing to next tuple of rows, account
        for 8 byte 'hole' (so row 0 = +0, row 1 = +128, row 2 = +256)
    */
    line = &vgc->text_1_scanlines[0];
    offset = 0x400;
    for (row = 0; row < 8; ++row, ++line) {
        line[0].offset = offset;
        line[0].control = 0;
        line[8].offset = offset + 40;
        line[8].control = 0;
        line[16].offset = offset + 80;
        line[16].control = 0;
        offset += 128;
    }
    line = &vgc->text_2_scanlines[0];
    for (row = 0; row < 8; ++row, ++line) {
        line[0].offset = offset;
        line[0].control = 0;
        line[8].offset = offset + 40;
        line[8].control = 0;
        line[16].offset = offset + 80;
        line[16].control = 0;
        offset += 128;
    }
    /*  hgr page 1 $2000-$3FFF, page 2 = $4000-$5FFF

        line text but each "row" is 8 pixels high and offsets increment by 1024
        of course, each byte is 7 pixels + 1 palette bit, which keeps the
        familiar +40, +80 increments used in text scanline calculation.
    */
    line = &vgc->hgr_1_scanlines[0];
    offset = 0x2000;
    for (row = 0; row < 8; ++row) {
        line[0 + row * 8].offset = offset + row * 128;
        line[0 + row * 8].control = 0;
        line[64 + row * 8].offset = (offset + 0x28) + row * 128;
        line[64 + row * 8].control = 0;
        line[128 + row * 8].offset = (offset + 0x50) + row * 128;
        line[128 + row * 8].control = 0;
    }
    for (row = 0; row < 24; ++row) {
        for (inner = 1; inner < 8; ++inner) {
            line[row * 8 + inner].offset = line[row * 8 + (inner - 1)].offset + 0x400;
            line[row * 8 + inner].control = 0;
        }
    }
    line = &vgc->hgr_2_scanlines[0];
    offset = 0x4000;
    for (row = 0; row < 8; ++row) {
        line[0 + row * 8].offset = offset + row * 128;
        line[0 + row * 8].control = 0;
        line[64 + row * 8].offset = (offset + 0x28) + row * 128;
        line[64 + row * 8].control = 0;
        line[128 + row * 8].offset = (offset + 0x50) + row * 128;
        line[128 + row * 8].control = 0;
    }
    for (row = 0; row < 24; ++row) {
        for (inner = 1; inner < 8; ++inner) {
            line[row * 8 + inner].offset = line[row * 8 + (inner - 1)].offset + 0x400;
            line[row * 8 + inner].control = 0;
        }
    }
    //  linear mapping (otherwise is not supported on the IIgs I believe)
    line = &vgc->shgr_scanlines[0];
    offset = 0x2000;
    for (row = 0; row < CLEM_VGC_SHGR_SCANLINE_COUNT; ++row, ++line) {
        line->offset = offset;
        line->control = 0;
        offset += 160;
    }
    for (row = 0; row < CLEM_VGC_SHGR_SCANLINE_COUNT * 16; ++row, ++line) {
        vgc->shgr_palettes[row] = 0x0000;
    }
}

void clem_vgc_set_mode(struct ClemensVGC *vgc, unsigned mode_flags) {
    if (mode_flags & CLEM_VGC_RESOLUTION_MASK) {
        clem_vgc_clear_mode(vgc, CLEM_VGC_RESOLUTION_MASK);
    }
    vgc->mode_flags |= mode_flags;
}

void clem_vgc_clear_mode(struct ClemensVGC *vgc, unsigned mode_flags) {
    vgc->mode_flags &= ~mode_flags;
    if (mode_flags & CLEM_VGC_ENABLE_VBL_IRQ) {
        vgc->irq_line &= ~CLEM_IRQ_VGC_BLANK;
    }
}

void clem_vgc_set_text_colors(struct ClemensVGC *vgc, unsigned fg_color, unsigned bg_color) {
    vgc->text_fg_color = fg_color;
    vgc->text_bg_color = bg_color;
}

void clem_vgc_set_region(struct ClemensVGC *vgc, uint8_t c02b_value) {
    unsigned last_mode_flags = vgc->mode_flags;
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
    if ((last_mode_flags ^ vgc->mode_flags) & CLEM_VGC_PAL) {
        //  since we use vertical blanks as a time counter vs absolute clocks, need
        //  to reset the counter when changing the refresh rate - not the best
        //  solution but given how infrequent this use-case might be, this
        //  oddity shoudn't have any practical effect
        vgc->vbl_counter = 0;
    }
    vgc->text_language = (c02b_value & 0xe0) >> 5;
}

uint8_t clem_vgc_get_region(struct ClemensVGC *vgc) {
    uint8_t result = (vgc->mode_flags & CLEM_VGC_LANGUAGE) ? 0x08 : 0x00;
    if (vgc->mode_flags & CLEM_VGC_PAL)
        result |= 0x10;
    result |= (uint8_t)((vgc->text_language << 5) & 0xe0);
    return result;
}

void clem_vgc_scanline_enable_int(struct ClemensVGC *vgc, bool enable) {
    vgc->scanline_irq_enable = enable;
    if (!enable) {
        _clem_vgc_set_scanline_int(vgc, false);
    }
}

#define CLEM_VGC_HORIZ_SCAN_DURATION(_ref_step_)                                                   \
    (clem_calc_clocks_step_from_ns(CLEM_VGC_HORIZ_SCAN_TIME_NS, _ref_step_))

#define CLEM_VGC_NTSC_SCAN_DURATION(_ref_step_)                                                    \
    (clem_calc_clocks_step_from_ns_long(CLEM_VGC_NTSC_SCAN_TIME_NS, _ref_step_))

#define CLEM_VGC_NTSC_FRAMES_PER_SECOND (1e6f / (CLEM_VGC_NTSC_SCAN_TIME_NS / 1000.0f))

void clem_vgc_sync(struct ClemensVGC *vgc, struct ClemensClock *clock, const uint8_t *mega2_bank0,
                   const uint8_t *mega2_bank1) {
    /* TODO: VBL detection depends on NTSC/PAL switch
                @ specific time within a scan until end of scan
             Calculate v_counter and h_counter on demand given:
                NTSC/PAL mode
                Timer/cycle
             Trigger IRQ (vgc->irq_line) if inside VBL region
             And of course, super hi-res
    */
    clem_clocks_duration_t scanline_duration = CLEM_VGC_HORIZ_SCAN_DURATION(clock->ref_step);
    unsigned scanline_limit = CLEM_VGC_NTSC_SCANLINE_COUNT;
    unsigned delta_tmp;

    if (vgc->mode_flags & CLEM_VGC_INIT) {
        vgc->ts_last_frame = clock->ts;
        vgc->dt_scanline = 0;
        vgc->mode_flags &= ~CLEM_VGC_INIT;
    } else {
        vgc->dt_scanline += (clock->ts - vgc->ts_last_frame);
        while (vgc->dt_scanline >= scanline_duration) {
            if (vgc->scanline_irq_enable && (vgc->mode_flags & CLEM_VGC_SUPER_HIRES)) {
                if (_clem_vgc_is_scanline_int_enabled(mega2_bank1, vgc->v_counter)) {
                    _clem_vgc_set_scanline_int(vgc, true);
                }
            }
            _clem_vgc_scanline_build_rgb_palette(vgc, vgc->v_counter, mega2_bank1);
            vgc->dt_scanline -= scanline_duration;
            vgc->v_counter++;
        }

        // TODO: vbl only once per VBL frame?
        if (vgc->v_counter >= CLEM_VGC_VBL_NTSC_LOWER_BOUND && !vgc->vbl_started) {
            if (vgc->mode_flags & CLEM_VGC_ENABLE_VBL_IRQ) {
                vgc->irq_line |= CLEM_IRQ_VGC_BLANK;
            }
            vgc->vbl_started = true;
        }
        if (vgc->v_counter >= scanline_limit) {
            vgc->v_counter -= scanline_limit;
            vgc->vbl_started = false;
            vgc->vbl_counter++;
        }
    }

    vgc->ts_last_frame = clock->ts;
}

void clem_vgc_calc_counters(struct ClemensVGC *vgc, struct ClemensClock *clock, unsigned *v_counter,
                            unsigned *h_counter) {
    /* 65 cycles per horizontal scanline, 978 ns per horizontal count = 63.57us
       and include wraparound since this is called during a read from I/O register switch and some
       time might have passed before the last call to sync(). */
    clem_clocks_duration_t scanline_duration = vgc->dt_scanline + (clock->ts - vgc->ts_last_frame);
    unsigned v_counter_delta = scanline_duration / CLEM_VGC_HORIZ_SCAN_DURATION(clock->ref_step);
    scanline_duration -= v_counter_delta * CLEM_VGC_HORIZ_SCAN_DURATION(clock->ref_step);
    *v_counter = vgc->v_counter + v_counter_delta;
    *h_counter = _clem_vgc_calc_h_counter(scanline_duration, clock->ref_step);
}

uint8_t clem_vgc_read_switch(struct ClemensVGC *vgc, struct ClemensClock *clock, uint8_t ioreg,
                             uint8_t flags) {
    uint8_t result = 0x00;
    unsigned v_counter;
    unsigned h_counter;

    clem_vgc_calc_counters(vgc, clock, &v_counter, &h_counter);

    switch (ioreg) {
    case CLEM_MMIO_REG_VBLBAR:
        /* IIgs sets bit 7 if scanline is within vertical blank region */
        if (v_counter >= CLEM_VGC_VBL_NTSC_LOWER_BOUND) {
            result |= 0x80;
        }
        break;
    case CLEM_MMIO_REG_VGC_VERTCNT:
        result = (uint8_t)(((v_counter + 0xFA) >> 1) & 0xff);
        break;
    case CLEM_MMIO_REG_VGC_HORIZCNT:
        if (h_counter < 1) {
            result = 0x00;
        } else {
            result = 0x3F + h_counter;
        }
        result |= ((v_counter + 0xFA) & 1) << 7;
        break;
    }
    return result;
}

void clem_vgc_write_switch(struct ClemensVGC *vgc, struct ClemensClock *clock, uint8_t ioreg,
                           uint8_t value) {
    switch (ioreg) {
    case CLEM_MMIO_REG_RTC_VGC_SCANINT:
        if (!(value & 0x20)) {
            _clem_vgc_set_scanline_int(vgc, false);
        }
        break;
    default:
        CLEM_UNIMPLEMENTED("vgc: write %02x : %02x ", ioreg, value);
        break;
    }
}
