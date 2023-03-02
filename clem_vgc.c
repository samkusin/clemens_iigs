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

static clem_clocks_duration_t sks_vgc_sync_this_time = 0;
static clem_clocks_duration_t sks_vgc_sync_last_time = 0;
static unsigned sks_vgc_vcounter_last = 0;
static unsigned sks_vgc_frame_count = 0;

static inline void _clem_vgc_set_scanline_int(struct ClemensVGC *vgc, bool enable) {
    if (enable) {
        vgc->irq_line |= CLEM_IRQ_VGC_SCAN_LINE;
    } else {
        vgc->irq_line &= ~CLEM_IRQ_VGC_SCAN_LINE;
    }
}

static inline unsigned _clem_vgc_calc_v_counter(clem_clocks_duration_t duration,
                                                clem_clocks_duration_t ref_step) {

    unsigned v_counter = (unsigned)(((uint64_t)duration * CLEM_MEGA2_CYCLE_NS) /
                                    (CLEM_VGC_HORIZ_SCAN_TIME_NS * ref_step));

    return v_counter & 0x1ff; /* 9 bits */
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
    line = &vgc->shgr_scanlines[0];
    offset = 0x2000;
    for (row = 0; row < 200; ++row, ++line) {
        line->offset = offset;
        line->control = 0;
        offset += 160;
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
    clem_clocks_duration_t frame_duration;
    unsigned v_counter;

    if (vgc->mode_flags & CLEM_VGC_INIT) {
        vgc->ts_last_frame = clock->ts;
        vgc->ts_scanline_0 = clock->ts;
        vgc->dt_scanline = 0;
        vgc->mode_flags &= ~CLEM_VGC_INIT;
        frame_duration = 0;
        sks_vgc_sync_this_time = 0;
        sks_vgc_sync_last_time = 0;
        sks_vgc_frame_count = 0;
        fprintf(stdout, "vgc: sync settings { %.2f us/frame, %.2f fps }\n",
                CLEM_VGC_NTSC_SCAN_TIME_NS / 1000.0f, CLEM_VGC_NTSC_FRAMES_PER_SECOND);
        fflush(stdout);
    } else {
        frame_duration = clock->ts - vgc->ts_scanline_0;
        v_counter = _clem_vgc_calc_v_counter(frame_duration, clock->ref_step);

        vgc->dt_scanline += (clock->ts - vgc->ts_last_frame);
        if (vgc->dt_scanline >= CLEM_VGC_HORIZ_SCAN_DURATION(clock->ref_step)) {
            vgc->dt_scanline -= CLEM_VGC_HORIZ_SCAN_DURATION(clock->ref_step);
            if (vgc->scanline_irq_enable && (vgc->mode_flags & CLEM_VGC_SUPER_HIRES)) {
                if (_clem_vgc_is_scanline_int_enabled(mega2_bank1, v_counter)) {
                    _clem_vgc_set_scanline_int(vgc, true);
                }
            }
        }

        // TODO: vbl only once per VBL frame?
        if (v_counter >= CLEM_VGC_VBL_NTSC_LOWER_BOUND && !vgc->vbl_started) {
            if (vgc->mode_flags & CLEM_VGC_ENABLE_VBL_IRQ) {
                vgc->irq_line |= CLEM_IRQ_VGC_BLANK;
            }
            vgc->vbl_started = true;
        }
        if (frame_duration >= CLEM_VGC_NTSC_SCAN_DURATION(clock->ref_step)) {
            vgc->ts_scanline_0 += frame_duration;
            vgc->vbl_started = false;
            vgc->vbl_counter++;
            /*
            fprintf(stdout, "vgc: [%u][%u ns, dt = %u ns] v_counter = %u\n", sks_vgc_frame_count,
                    clem_calc_ns_step_from_clocks(sks_vgc_sync_this_time, clock->ref_step),
                    clem_calc_ns_step_from_clocks(sks_vgc_sync_this_time - sks_vgc_sync_last_time,
                                                  clock->ref_step),
                    v_counter);
            fflush(stdout);
            */
            sks_vgc_sync_last_time = sks_vgc_sync_this_time;
            sks_vgc_frame_count++;
        }
        sks_vgc_sync_this_time += (clock->ts - vgc->ts_last_frame);
    }

    if (sks_vgc_sync_this_time >= CLEM_MEGA2_CYCLES_PER_SECOND * clock->ref_step) {
        unsigned h_counter = _clem_vgc_calc_h_counter(vgc->dt_scanline, clock->ref_step);
        fprintf(stdout, "vgc: [%.2f (%u)][%u ns, dt = %u ns] {v: %u, h: %u}, vdiff: %d\n",
                sks_vgc_frame_count / CLEM_VGC_NTSC_FRAMES_PER_SECOND, sks_vgc_frame_count,
                clem_calc_ns_step_from_clocks(sks_vgc_sync_this_time, clock->ref_step),
                clem_calc_ns_step_from_clocks(sks_vgc_sync_this_time - sks_vgc_sync_last_time,
                                              clock->ref_step),
                v_counter, h_counter, (int)(v_counter - sks_vgc_vcounter_last));
        fflush(stdout);
        sks_vgc_vcounter_last = v_counter;
        sks_vgc_sync_last_time -= (CLEM_MEGA2_CYCLES_PER_SECOND * clock->ref_step);
        sks_vgc_sync_this_time -= (CLEM_MEGA2_CYCLES_PER_SECOND * clock->ref_step);
    }

    vgc->ts_last_frame = clock->ts;
}

uint8_t clem_vgc_read_switch(struct ClemensVGC *vgc, struct ClemensClock *clock, uint8_t ioreg,
                             uint8_t flags) {
    uint8_t result = 0x00;
    unsigned v_counter;
    unsigned h_counter;

    /* 65 cycles per horizontal scanline, 978 ns per horizontal count = 63.57us*/
    v_counter = _clem_vgc_calc_v_counter(clock->ts - vgc->ts_scanline_0, clock->ref_step);
    h_counter = _clem_vgc_calc_h_counter(vgc->dt_scanline, clock->ref_step);

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
