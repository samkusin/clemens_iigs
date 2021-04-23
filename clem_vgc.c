#include "clem_vgc.h"


void clem_vgc_init(struct ClemensVGC* vgc) {
    /* setup scanline maps for all of the different modes */
    struct ClemensScanline* line;
    unsigned offset;
    unsigned row, inner;

    vgc->vbl_counter = 0;

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

uint32_t clem_vgc_sync(
    struct ClemensVGC* vgc,
    uint32_t delta_us,
    uint32_t irq_line
) {

}
