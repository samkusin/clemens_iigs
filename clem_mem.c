#include "clem_mem.h"
#include "clem_debug.h"
#include "clem_drive.h"
#include "clem_mmio_defs.h"

#include "clem_util.h"
#include "clem_device.h"
#include "clem_vgc.h"

/**
 * Video Memory layout
 *
 *  High Level:
 *    FPI memory in banks $00 - $7F (practically up to 8MB RAM) and
 *      ROM ($F0-$FF) - runs at clock speed
 *    Mega 2 memory in banks $E0, $E1
 *      - memory accesses here are always at 1mhz (reads AND writes)
 *
 *    Shadowing keeps select pages from $00, $01 in sync with $e0, $e1
 *      - writes must occur at Mega 2 speed (1mhz)
 *      - reads for I/O shadowing occur at 1mhz (reads from $E0, E1)
 *      - reads for display shadowing occur at FPI speed (reading from $00,$01)
 *
 *
 *  Bank 00/01
 *    0400-07FF Text Page 1
 *    0800-0BFF Text Page 2
 *    2000-3FFF HGR Page 1
 *    4000-5FFF HGR Page 2
 *    * note there are quirks addressed in the "Alternate Display Mode" IIgs
 *      feature, which turns on shadowing for text page 2 (required for Apple II
 *      text page compatilbility)
 *
 *  Bank 00/01
 *    C000-CFFF IO + slot expansions (mirrored), shadowing from bank $e0
 *    D000-DFFF contains 2 banks of 4K RAM
 *    E000-FFFF contains 1 bank 12K RAM
 *
 *  Oddities
 *    C07X bank 0 contains code for interrupts, which relies on the shadowing
 *      to work a certain way.  Account for this when debugging/testing
 *      interrupts from the ROM
 *    Generally speaking, access in the $C000 page is slow, but certain FPI
 *      registers can be read/write fast, including interrupt rom at $C071-7F
 *    RAM refresh delays in FPI memory 8 when instructions/data accessed
 *      from RAM
 *
 *
 */


/**
 *  Memory R/W access
 *
 *  FPI ROM -   2.864mhz
 *  FPI RAM -   8% reduction from 2.8mhz (TPD?) - 2.6mhz approx
 *  Mega2 RAM - 1.023 mhz
 *
 *  - Map Bank:Address to its actual Bank:Address inside either its FPI or Mega2
 *    Memory
 *  - Shadowed reads outside of I/O are handled by reading the FPI memory
 *  - Shadowed writes outside of I/O are handled by writing to both FPI and
 *    Mega2
 *  - I/O is a special case
 *  - Softswitches alter the mapping of bank 00 reads/writes
 *  - For now, always allow address bit 17 to access auxillary memory where
 *    c029 bit 0 is on (TODO: handle off cases when they come up)
 *  - For now, assume Bank 00, 01 shadowing (bit 4 of c029 is off) until we
 *    need to run the ninja force demo for testing shadowing for all banks
 *
 *  - Bank 01, E1 access will override softswitch main/aux setting
 *  - Bank 00, E0 access will set the target bank bit 1 based on softswitch
 *    main/aux
 *    - 00, E0 special case page 00, 01, D0-DF, etc based on softswitches
 *  - Solution seems to have a page map that maps access to main or aux memory
 *    - Page map should include shadowing instructions for writes
 *    - There only needs to be three page maps - 00/E0, 01/E1, and the 1:1
 *      direct mapping version (or add a compare/branch each read/write)
 *    - Each bank has a page map template
 *      Each page has a 'target' (0 or 1 bank) and page (many will map 1:1)
 *      Each page has a shadow-bit to shadow writes to the Mega2 bank
 */

static void _clem_mmio_memory_map(struct ClemensMMIO* mmio,
                                  uint32_t memory_flags);
static void _clem_mmio_shadow_map(struct ClemensMMIO* mmio,
                                  uint32_t shadow_flags);

static inline void _clem_mem_cycle(
    ClemensMachine* clem,
    bool mega2_access
) {
    clem->clocks_spent += mega2_access ? clem->clocks_step_mega2 : (
        clem->clocks_step);
    ++clem->cpu.cycles_spent;
}

static void _clem_mmio_create_page_direct_mapping(
    struct ClemensMMIOPageInfo* page,
    uint8_t page_idx
) {
    page->read = page_idx;
    page->write = page_idx;
    page->flags =  CLEM_MMIO_PAGE_WRITE_OK | CLEM_MMIO_PAGE_DIRECT;
}

static void _clem_mmio_create_page_mainaux_mapping(
    struct ClemensMMIOPageInfo* page,
    uint8_t page_idx,
    uint8_t bank_idx
) {
    page->bank_read = bank_idx;
    page->bank_write = bank_idx;
    page->read = page_idx;
    page->write = page_idx;
    page->flags =  CLEM_MMIO_PAGE_WRITE_OK | CLEM_MMIO_PAGE_MAINAUX;
}

static void _clem_mmio_create_page_mapping(
    struct ClemensMMIOPageInfo* page,
    uint8_t page_idx,
    uint8_t bank_read_idx,
    uint8_t bank_write_idx
) {
    page->flags = CLEM_MMIO_PAGE_WRITE_OK;
    page->bank_read = bank_read_idx;
    page->read = page_idx;
    page->bank_write = bank_write_idx;
    page->write = page_idx;
}

static void _clem_mmio_clear_irq(
    struct ClemensMMIO* mmio,
    unsigned irq_flags
) {
    if (irq_flags & CLEM_IRQ_VGC_MASK) {
        mmio->vgc.irq_line &= ~(irq_flags & CLEM_IRQ_VGC_MASK);
        mmio->irq_line &= ~(irq_flags & CLEM_IRQ_VGC_MASK);
    }
    if (irq_flags & CLEM_IRQ_TIMER_MASK) {
        mmio->dev_timer.irq_line &= ~(irq_flags & CLEM_IRQ_TIMER_MASK);
        mmio->irq_line &= ~(irq_flags & CLEM_IRQ_TIMER_MASK);
    }
    if (irq_flags & CLEM_IRQ_ADB_MASK) {
        mmio->dev_adb.irq_line &= ~(irq_flags & CLEM_IRQ_ADB_MASK);
        mmio->irq_line &= ~(irq_flags & CLEM_IRQ_ADB_MASK);
    }
}

static inline uint8_t _clem_mmio_newvideo_c029(struct ClemensMMIO* mmio) {
    return mmio->new_video_c029;
}

static inline void _clem_mmio_newvideo_c029_set(
    struct ClemensMMIO* mmio,
    uint8_t value
) {
    uint8_t setflags = mmio->new_video_c029 ^ value;
    if (setflags & CLEM_MMIO_NEWVIDEO_BANKLATCH_INHIBIT) {
        if (!(value & CLEM_MMIO_NEWVIDEO_BANKLATCH_INHIBIT)) {
            CLEM_UNIMPLEMENTED("ioreg %02X : %02X", CLEM_MMIO_REG_NEWVIDEO, value);
        }
        setflags ^= CLEM_MMIO_NEWVIDEO_BANKLATCH_INHIBIT;
    }
    CLEM_ASSERT(setflags == 0);
}

static void _clem_mmio_slotrom_select_c02d(
    struct ClemensMMIO* mmio,
    uint8_t data
) {
    int i;
    unsigned slot_mask = CLEM_MMIO_MMAP_CROM & (
        ~(CLEM_MMIO_MMAP_CXROM | CLEM_MMIO_MMAP_C3ROM));
    unsigned mmap_register = mmio->mmap_register & ~slot_mask;
    slot_mask = 0;
    for (i = 1; i < 8; ++i) {
        if (i == 3) continue;
        slot_mask = CLEM_MMIO_MMAP_C1ROM << (i - 1);
        if (data & (1 << i)) {
            mmap_register |= slot_mask;
        } else {
            mmap_register &= ~slot_mask;
        }
    }
    _clem_mmio_memory_map(mmio, mmap_register);
}

static uint8_t _clem_mmio_slotromsel_c02d(struct ClemensMMIO* mmio) {
    uint8_t mask = 0;
    for (int i = 1; i < 8; ++i) {
        if (i == 3) continue;
        if (mmio->mmap_register & (CLEM_MMIO_MMAP_C1ROM << (i - 1))) {
            mask |= (1 << i);
        } else {
            mask &= ~(1 << i);
        }
    }
    return mask;
}

static inline uint8_t _clem_mmio_shadow_c035(struct ClemensMMIO* mmio) {
    uint8_t result = 0;
    if (mmio->mmap_register & CLEM_MMIO_MMAP_NSHADOW_TXT1) result |= 0x01;
    if (mmio->mmap_register & CLEM_MMIO_MMAP_NSHADOW_HGR1) result |= 0x02;
    if (mmio->mmap_register & CLEM_MMIO_MMAP_NSHADOW_HGR2) result |= 0x04;
    if (mmio->mmap_register & CLEM_MMIO_MMAP_NSHADOW_SHGR) result |= 0x08;
    if (mmio->mmap_register & CLEM_MMIO_MMAP_NSHADOW_AUX) result |= 0x10;
    if (mmio->mmap_register & CLEM_MMIO_MMAP_NSHADOW_TXT2) result |= 0x20;
    if (mmio->mmap_register & CLEM_MMIO_MMAP_NIOLC) result |= 0x40;
    return result;
}

static void _clem_mmio_shadow_c035_set(
    struct ClemensMMIO* mmio,
    uint8_t value
) {
    unsigned mmap = mmio->mmap_register;
    if (value & 0x01) mmap |= CLEM_MMIO_MMAP_NSHADOW_TXT1;
    else mmap &= ~CLEM_MMIO_MMAP_NSHADOW_TXT1;
    if (value & 0x02) mmap |= CLEM_MMIO_MMAP_NSHADOW_HGR1;
    else mmap &= ~CLEM_MMIO_MMAP_NSHADOW_HGR1;
    if (value & 0x04) mmap |= CLEM_MMIO_MMAP_NSHADOW_HGR2;
    else mmap &= ~CLEM_MMIO_MMAP_NSHADOW_HGR2;
    if (value & 0x08) mmap |= CLEM_MMIO_MMAP_NSHADOW_SHGR;
    else mmap &= ~CLEM_MMIO_MMAP_NSHADOW_SHGR;
    if (value & 0x10) mmap |= CLEM_MMIO_MMAP_NSHADOW_AUX;
    else mmap &= ~CLEM_MMIO_MMAP_NSHADOW_AUX;
    if (value & 0x20) mmap |= CLEM_MMIO_MMAP_NSHADOW_TXT2;
    else mmap &= ~CLEM_MMIO_MMAP_NSHADOW_TXT2;
    if (value & 0x40) mmap |= CLEM_MMIO_MMAP_NIOLC;
    else mmap &= ~CLEM_MMIO_MMAP_NIOLC;
    _clem_mmio_memory_map(mmio, mmap);
}

static void _clem_mmio_speed_c036_set(
    ClemensMachine* clem,
    uint8_t value
) {
    uint8_t setflags = clem->mmio.speed_c036 ^ value;

    if (setflags & CLEM_MMIO_SPEED_FAST_ENABLED) {
        if (value & CLEM_MMIO_SPEED_FAST_ENABLED &&
            !clem->mmio.dev_iwm.disk_motor_on
        ) {
            //CLEM_LOG("C036: Fast Mode");
            clem->clocks_step = clem->clocks_step_fast;
        } else {
            //CLEM_LOG("C036: Slow Mode");
            clem->clocks_step = clem->clocks_step_mega2;
        }
    }
    if (setflags & CLEM_MMIO_SPEED_POWERED_ON) {
        if (value & CLEM_MMIO_SPEED_POWERED_ON) {
            CLEM_LOG("C036: Powered On SET");
        } else {
            CLEM_LOG("C036: Powered On CLEARED");
        }
    }
    if (setflags & CLEM_MMIO_SPEED_DISK_FLAGS) {
        CLEM_LOG("C036: Disk motor detect mask: %02X",
                 value & CLEM_MMIO_SPEED_DISK_FLAGS);
    }

    /* bit 5 should always be 0 */
    /* for ROM 3, bit 6 can be on or off - for ROM 1, must be off */
    clem->mmio.speed_c036 = (value & 0xdf);
}

static void _clem_mmio_mega2_inten_set(struct ClemensMMIO* mmio, uint8_t data) {
    if (data & 0xe0) {
        CLEM_WARN("clem_mmio: invalid inten set %02X", data);
    }
    if (data & 0x10) {
        mmio->dev_timer.flags |= CLEM_MMIO_TIMER_QSEC_ENABLED;
    } else {
        mmio->dev_timer.flags &= ~CLEM_MMIO_TIMER_QSEC_ENABLED;
        _clem_mmio_clear_irq(mmio, CLEM_IRQ_TIMER_QSEC);
    }
    if (data & 0x08) {
        clem_vgc_set_mode(&mmio->vgc, CLEM_VGC_ENABLE_VBL_IRQ);
    } else {
        clem_vgc_clear_mode(&mmio->vgc, CLEM_VGC_ENABLE_VBL_IRQ);
        _clem_mmio_clear_irq(mmio, CLEM_IRQ_VGC_BLANK);
    }
    if (data & 0x07) {
        CLEM_WARN("clem_mmio: mega2 mouse not impl - set %02X", data);
    }
}

static void _clem_mmio_mega2_clear_irq(
    struct ClemensMMIO* mmio,
    uint8_t data
) {
    if (data & 0x9f) {
        CLEM_WARN("clem_mmio: invalid clear flags for SCANINT %02X", data);
    }
    if (!(data & 0x40)) {
        _clem_mmio_clear_irq(mmio, CLEM_IRQ_TIMER_RTC_1SEC);
    }
    if (!(data & 0x20)) {
        //  TODO
    }
}

static uint8_t _clem_mmio_mega2_inten_get(struct ClemensMMIO* mmio) {
    uint8_t res = 0x00;
    if (mmio->dev_timer.flags & CLEM_MMIO_TIMER_QSEC_ENABLED) {
        res |= 0x10;
    }
    if (mmio->vgc.mode_flags & CLEM_VGC_ENABLE_VBL_IRQ) {
        res |= 0x08;
    }
    return res;
}


static uint8_t _clem_mmio_inttype_c046(
    struct ClemensMMIO* mmio
) {
    uint8_t result = mmio->irq_line ? CLEM_MMIO_INTTYPE_IRQ : 0;

    if (mmio->irq_line & CLEM_IRQ_TIMER_QSEC) {
        result |= CLEM_MMIO_INTTYPE_QSEC;
    }
    if (mmio->irq_line & CLEM_IRQ_VGC_BLANK) {
        result |= CLEM_MMIO_INTTYPE_VBL;
    }

    /* TODO: AN3, Mouse */

    /* TODO: other flags, mouse, VBL, */
    return result;
}

static void _clem_mmio_vgc_irq_c023_set(
    struct ClemensMMIO* mmio,
    uint8_t data
) {
    if (data & 0x4) {
        mmio->dev_timer.flags |= CLEM_MMIO_TIMER_1SEC_ENABLED;
    } else {
        mmio->dev_timer.flags &= ~CLEM_MMIO_TIMER_1SEC_ENABLED;
        _clem_mmio_clear_irq(mmio, CLEM_IRQ_TIMER_RTC_1SEC);
    }
    if (data & 0x2) {
        CLEM_UNIMPLEMENTED("VGC Scanline IRQ set");
    }
}

static uint8_t _clem_mmio_vgc_irq_c023_get(struct ClemensMMIO* mmio) {
    uint8_t res = 0x00;

    if (mmio->irq_line & (CLEM_IRQ_VGC_SCAN_LINE + CLEM_IRQ_TIMER_RTC_1SEC)) {
        res |= 0x80;
        if (mmio->irq_line & CLEM_IRQ_TIMER_RTC_1SEC) {
            res |= 0x40;
        }
        if (mmio->irq_line & CLEM_IRQ_VGC_SCAN_LINE) {
            res |= 0x20;
        }
    }
    if (mmio->dev_timer.flags & CLEM_MMIO_TIMER_1SEC_ENABLED) {
        res |= 0x04;
    }

    /* TODO: VGC SCAN LINE */
    return res;
}

/* For why we don't follow the HW Ref, see important changes documented for
   STATEREG here:
   http://www.1000bit.it/support/manuali/apple/technotes/iigs/tn.iigs.030.html
*/
static inline uint8_t _clem_mmio_statereg_c068(struct ClemensMMIO* mmio) {
     uint8_t value = 0x00;
    if (mmio->mmap_register & CLEM_MMIO_MMAP_ALTZPLC) {
        value |= 0x80;
    }
    /* TODO PAGE2 TEXT */

    if (mmio->mmap_register & CLEM_MMIO_MMAP_RAMRD) {
        value |= 0x20;
    }
    if (mmio->mmap_register & CLEM_MMIO_MMAP_RAMWRT) {
        value |= 0x10;
    }
    if (!(mmio->mmap_register & CLEM_MMIO_MMAP_RDLCRAM)) {
        value |= 0x08;
    }
    if (mmio->mmap_register & CLEM_MMIO_MMAP_LCBANK2) {
        value |= 0x04;
    }
    if (!(mmio->mmap_register & CLEM_MMIO_MMAP_CXROM)) {
        value |= 0x01;
    }
    return value;
}

static uint8_t _clem_mmio_statereg_c068_set(
    struct ClemensMMIO* mmio,
    uint8_t value
) {
    uint32_t mmap_register = mmio->mmap_register;
    /*  ALTZP  */
    if (value & 0x80) {
        mmap_register |= CLEM_MMIO_MMAP_ALTZPLC;
    } else {
        mmap_register &= ~CLEM_MMIO_MMAP_ALTZPLC;
    }
    /*  PAGE2 text - TODO when video options are fleshed out */
    if (value & 0x40) {
        CLEM_UNIMPLEMENTED("c068 PAGE2 Text");
    } else {

    }
    /*  RAMRD */
    if (value & 0x20) {
        mmap_register |= CLEM_MMIO_MMAP_RAMRD;
    } else {
        mmap_register &= ~CLEM_MMIO_MMAP_RAMRD;
    }
    /*  RAMWRT */
    if (value & 0x10) {
        mmap_register |= CLEM_MMIO_MMAP_RAMWRT;
    } else {
        mmap_register &= ~CLEM_MMIO_MMAP_RAMWRT;
    }
    /*  RDROM */
    if (value & 0x08) {
        mmap_register &= ~CLEM_MMIO_MMAP_RDLCRAM;
    } else {
        mmap_register |= CLEM_MMIO_MMAP_RDLCRAM;
    }
    /* LCBNK2 */
    if (value & 0x04) {
        mmap_register |= CLEM_MMIO_MMAP_LCBANK2;
    } else {
        mmap_register &= ~CLEM_MMIO_MMAP_LCBANK2;
    }
    /* ROMBANK always 0 */
    if (value & 0x02) {
        /* do not set */
        CLEM_WARN("c068 %02X not allowed", value);
    } else {
        /* only valid value */
    }
    /* INTCXROM (C3ROM?) */
    if (value & 0x01) {
        mmap_register &= ~CLEM_MMIO_MMAP_CXROM;
    } else {
        mmap_register |= CLEM_MMIO_MMAP_CXROM;
    }

    _clem_mmio_memory_map(mmio, mmap_register);

    return 0;
}

static uint8_t _clem_mmio_read_bank_select(
    struct ClemensMMIO* mmio,
    uint8_t ioreg,
    uint8_t flags
) {
    uint32_t memory_flags = mmio->mmap_register;
    switch (ioreg) {
        case CLEM_MMIO_REG_LC2_RAM_WP:
            memory_flags |= (CLEM_MMIO_MMAP_RDLCRAM + CLEM_MMIO_MMAP_LCBANK2);
            memory_flags &= ~CLEM_MMIO_MMAP_WRLCRAM;
            break;
        case CLEM_MMIO_REG_LC2_ROM_WE:
            if (mmio->flags_c08x & 0x1) {
                memory_flags |= (CLEM_MMIO_MMAP_WRLCRAM + CLEM_MMIO_MMAP_LCBANK2);
                memory_flags &= ~CLEM_MMIO_MMAP_RDLCRAM;
                mmio->flags_c08x &= ~0x1;
            } else {
                mmio->flags_c08x |= 0x1;
            }
            break;
        case CLEM_MMIO_REG_LC2_ROM_WP:
            memory_flags &= ~(CLEM_MMIO_MMAP_RDLCRAM + CLEM_MMIO_MMAP_WRLCRAM);
            memory_flags |= CLEM_MMIO_MMAP_LCBANK2;
            break;
        case CLEM_MMIO_REG_LC2_RAM_WE:
            if (mmio->flags_c08x & 0x2) {
                memory_flags |= (CLEM_MMIO_MMAP_RDLCRAM +
                                 CLEM_MMIO_MMAP_WRLCRAM +
                                 CLEM_MMIO_MMAP_LCBANK2);
                mmio->flags_c08x &= ~0x2;
            } else {
                mmio->flags_c08x |= 0x2;
            }
            break;
        case CLEM_MMIO_REG_LC1_RAM_WP:
            memory_flags &= ~(CLEM_MMIO_MMAP_LCBANK2 + CLEM_MMIO_MMAP_WRLCRAM);
            memory_flags |= CLEM_MMIO_MMAP_RDLCRAM;
            break;
        case CLEM_MMIO_REG_LC1_ROM_WE:
            if (mmio->flags_c08x & 0x4) {
                memory_flags &= ~(CLEM_MMIO_MMAP_RDLCRAM + CLEM_MMIO_MMAP_LCBANK2);
                memory_flags |= CLEM_MMIO_MMAP_WRLCRAM;
                mmio->flags_c08x &= ~0x4;
            } else {
                mmio->flags_c08x |= 0x4;
            }
            break;
        case CLEM_MMIO_REG_LC1_ROM_WP:
            memory_flags &= ~(CLEM_MMIO_MMAP_LCBANK2 + CLEM_MMIO_MMAP_WRLCRAM +
                              CLEM_MMIO_MMAP_RDLCRAM);
            break;
        case CLEM_MMIO_REG_LC1_RAM_WE:
            if (mmio->flags_c08x & 0x8) {
                memory_flags |= (CLEM_MMIO_MMAP_RDLCRAM + CLEM_MMIO_MMAP_WRLCRAM);
                memory_flags &= ~CLEM_MMIO_MMAP_LCBANK2;
                mmio->flags_c08x &= ~0x8;
            } else {
                mmio->flags_c08x |= 0x8;
            }
            break;
    }
    if (!(flags & CLEM_MMIO_READ_NO_OP) && memory_flags != mmio->mmap_register) {
        _clem_mmio_memory_map(mmio, memory_flags);
    }
    return 0;
}

static uint8_t _clem_mmio_read(
    ClemensMachine* clem,
    uint16_t addr,
    uint8_t flags,
    bool* mega2_access
) {
    struct ClemensMMIO* mmio = &clem->mmio;
    struct ClemensClock ref_clock;
    uint8_t result = 0x00;
    uint8_t ioreg = addr & 0xff;
    bool is_noop = (flags & CLEM_MMIO_READ_NO_OP) != 0;

    if (!is_noop) {
        ++mmio->dev_debug.ioreg_read_ctr[ioreg];
    }

    if (!(flags & CLEM_MMIO_READ_NO_OP)) {
        /* disk motor speed registers */
        *mega2_access = true;
    }

    ref_clock.ts = clem->clocks_spent;
    ref_clock.ref_step = clem->clocks_step_mega2;

    switch (ioreg) {
        case CLEM_MMIO_REG_KEYB_READ:
        /* 01-0F are no-ops */
        case CLEM_MMIO_REG_ANYKEY_STROBE:
        case CLEM_MMIO_REG_ADB_MOUSE_DATA:
        case CLEM_MMIO_REG_ADB_MODKEY:
        case CLEM_MMIO_REG_ADB_CMD_DATA:
        case CLEM_MMIO_REG_ADB_STATUS:
            result = clem_adb_read_switch(&mmio->dev_adb, ioreg, flags);
            break;
        case CLEM_MMIO_REG_LC_BANK_TEST:
            result = (mmio->mmap_register & CLEM_MMIO_MMAP_LCBANK2) ? 0x80 : 0x00;
            break;
        case CLEM_MMIO_REG_ROM_RAM_TEST:
            result = (mmio->mmap_register & CLEM_MMIO_MMAP_RDLCRAM) ? 0x80 : 0x00;
            break;
        case CLEM_MMIO_REG_RAMRD_TEST:
            result = (mmio->mmap_register & CLEM_MMIO_MMAP_RAMRD) ? 0x80 : 0x00;
            break;
        case CLEM_MMIO_REG_RAMWRT_TEST:
            result = (mmio->mmap_register & CLEM_MMIO_MMAP_RAMWRT) ? 0x80 : 0x00;
            break;
        case CLEM_MMIO_REG_READCXROM:
            result = (mmio->mmap_register & CLEM_MMIO_MMAP_CXROM) ? 0x80 : 00;
            break;
        case CLEM_MMIO_REG_RDALTZP_TEST:
            result = (mmio->mmap_register & CLEM_MMIO_MMAP_ALTZPLC) ? 0x80 : 0x00;
            break;
        case CLEM_MMIO_REG_READC3ROM:
            result = (mmio->mmap_register & CLEM_MMIO_MMAP_C3ROM) ? 0x80 : 00;
            break;
        case CLEM_MMIO_REG_80COLSTORE_TEST:
            result = (mmio->mmap_register & CLEM_MMIO_MMAP_80COLSTORE) ? 0x80 : 00;
            break;
        case CLEM_MMIO_REG_VBLBAR:
            result = clem_vgc_read_switch(&mmio->vgc, &ref_clock, ioreg, flags);
            break;
        case CLEM_MMIO_REG_TXT_TEST:
            result = (mmio->vgc.mode_flags & CLEM_VGC_GRAPHICS_MODE) ? 0x00 : 0x80;
            break;
        case CLEM_MMIO_REG_MIXED_TEST:
            result = (mmio->vgc.mode_flags & CLEM_VGC_MIXED_TEXT) ? 0x80 : 0x80;
            break;
        case CLEM_MMIO_REG_TXTPAGE2_TEST:
            result = (mmio->mmap_register & CLEM_MMIO_MMAP_TXTPAGE2) ? 0x80 : 00;
            break;
        case CLEM_MMIO_REG_ALTCHARSET_TEST:
            result = (mmio->vgc.mode_flags & CLEM_VGC_ALTCHARSET) ? 0x80 : 00;
            break;
        case CLEM_MMIO_REG_HIRES_TEST:
            result = (mmio->vgc.mode_flags & CLEM_VGC_HIRES) ? 0x80 : 00;
            break;
        case CLEM_MMIO_REG_80COLUMN_TEST:
            result = (mmio->vgc.mode_flags & CLEM_VGC_80COLUMN_TEXT) ? 0x80 : 00;
            break;
        case CLEM_MMIO_REG_VGC_TEXT_COLOR:
            result = (uint8_t)(
                (mmio->vgc.text_fg_color << 4) | mmio->vgc.text_bg_color);
            break;
        case CLEM_MMIO_REG_VGC_IRQ_BYTE:
            result = _clem_mmio_vgc_irq_c023_get(mmio);
            break;
        case CLEM_MMIO_REG_NEWVIDEO:
            result = _clem_mmio_newvideo_c029(mmio);
            break;
        case CLEM_MMIO_REG_LANGSEL:
            result = clem_vgc_get_region(&mmio->vgc);
            break;
        case CLEM_MMIO_REG_SLOTROMSEL:
            result = _clem_mmio_slotromsel_c02d(mmio);
            break;
        case CLEM_MMIO_REG_SPKR:
            result = clem_sound_read_switch(&mmio->dev_audio, ioreg, flags);
            break;
        case CLEM_MMIO_REG_DISK_INTERFACE:
            result = clem_iwm_read_switch(&mmio->dev_iwm,
                                          &clem->active_drives,
                                          &ref_clock,
                                          ioreg,
                                          flags);
            break;
        case CLEM_MMIO_REG_RTC_SCANINT:
            break;
        case CLEM_MMIO_REG_SHADOW:
            result = _clem_mmio_shadow_c035(mmio);
            break;
        case CLEM_MMIO_REG_SPEED:
            result = mmio->speed_c036;
            break;
        case CLEM_MMIO_REG_RTC_CTL:
            if (!is_noop) {
                clem_rtc_command(&mmio->dev_rtc, clem->clocks_spent, CLEM_IO_READ);
            }
            result = mmio->dev_rtc.ctl_c034;
            break;
        case CLEM_MMIO_REG_RTC_DATA:
            result = mmio->dev_rtc.data_c033;
            break;
        case CLEM_MMIO_REG_SCC_B_CMD:
        case CLEM_MMIO_REG_SCC_A_CMD:
        case CLEM_MMIO_REG_SCC_B_DATA:
        case CLEM_MMIO_REG_SCC_A_DATA:
            result = clem_scc_read_switch(&mmio->dev_scc, ioreg, flags);
            break;
        case CLEM_MMIO_REG_AUDIO_CTL:
            result = clem_sound_read_switch(&mmio->dev_audio, ioreg, flags);
            break;
        case CLEM_MMIO_REG_AUDIO_DATA:
            result = clem_sound_read_switch(&mmio->dev_audio, ioreg, flags);
            break;
        case CLEM_MMIO_REG_AUDIO_ADRLO:
            result = clem_sound_read_switch(&mmio->dev_audio, ioreg, flags);
            break;
        case CLEM_MMIO_REG_AUDIO_ADRHI:
            result = clem_sound_read_switch(&mmio->dev_audio, ioreg, flags);
            break;
        case CLEM_MMIO_REG_MEGA2_INTEN:
            result = _clem_mmio_mega2_inten_get(mmio);
            break;
        case CLEM_MMIO_REG_DIAG_INTTYPE:
            result = _clem_mmio_inttype_c046(mmio);
            break;
        case CLEM_MMIO_REG_CLRVBLINT:
            if (!(flags & CLEM_MMIO_READ_NO_OP)) {
                _clem_mmio_clear_irq(mmio, CLEM_IRQ_TIMER_QSEC | CLEM_IRQ_VGC_BLANK);
            }
            break;
        case CLEM_MMIO_REG_TXTCLR:
            if (!(flags & CLEM_MMIO_READ_NO_OP)) {
                clem_vgc_set_mode(&mmio->vgc, CLEM_VGC_GRAPHICS_MODE);
            }
            break;
        case CLEM_MMIO_REG_TXTSET:
            if (!(flags & CLEM_MMIO_READ_NO_OP)) {
                clem_vgc_clear_mode(&mmio->vgc, CLEM_VGC_GRAPHICS_MODE);
            }
            break;
        case CLEM_MMIO_REG_MIXCLR:
            if (!(flags & CLEM_MMIO_READ_NO_OP)) {
                clem_vgc_clear_mode(&mmio->vgc, CLEM_VGC_MIXED_TEXT);
            }
            break;
        case CLEM_MMIO_REG_MIXSET:
            if (!(flags & CLEM_MMIO_READ_NO_OP)) {
                clem_vgc_set_mode(&mmio->vgc, CLEM_VGC_MIXED_TEXT);
            }
            break;
        case CLEM_MMIO_REG_TXTPAGE1:
            if (!(flags & CLEM_MMIO_READ_NO_OP)) {
                _clem_mmio_memory_map(
                    mmio, mmio->mmap_register & ~CLEM_MMIO_MMAP_TXTPAGE2);
            }
            break;
        case CLEM_MMIO_REG_TXTPAGE2:
            if (!(flags & CLEM_MMIO_READ_NO_OP)) {
                _clem_mmio_memory_map(
                    mmio, mmio->mmap_register | CLEM_MMIO_MMAP_TXTPAGE2);
            }
            break;
        case CLEM_MMIO_REG_LORES:
            /* implicitly clears hires */
            if (!(flags & CLEM_MMIO_READ_NO_OP)) {
                clem_vgc_set_mode(&mmio->vgc, CLEM_VGC_LORES);
            }
            break;
        case CLEM_MMIO_REG_HIRES:
            /* implicitly clears lores */
            if (!(flags & CLEM_MMIO_READ_NO_OP)) {
                clem_vgc_set_mode(&mmio->vgc, CLEM_VGC_HIRES);
            }
            break;
        case CLEM_MMIO_REG_AN0_OFF:
        case CLEM_MMIO_REG_AN0_ON:
        case CLEM_MMIO_REG_AN1_OFF:
        case CLEM_MMIO_REG_AN1_ON:
        case CLEM_MMIO_REG_AN2_OFF:
        case CLEM_MMIO_REG_AN2_ON:
        case CLEM_MMIO_REG_AN3_OFF:
        case CLEM_MMIO_REG_AN3_ON:
        case CLEM_MMIO_REG_BTN0:
        case CLEM_MMIO_REG_BTN1:
            result = clem_adb_read_switch(&mmio->dev_adb, ioreg, flags);
            break;
        case CLEM_MMIO_REG_STATEREG:
            result = _clem_mmio_statereg_c068(mmio);
            break;
        case CLEM_MMIO_REG_LC2_RAM_WP:
        case CLEM_MMIO_REG_LC2_ROM_WE:
        case CLEM_MMIO_REG_LC2_ROM_WP:
        case CLEM_MMIO_REG_LC2_RAM_WE:
        case CLEM_MMIO_REG_LC1_RAM_WP:
        case CLEM_MMIO_REG_LC1_ROM_WE:
        case CLEM_MMIO_REG_LC1_ROM_WP:
        case CLEM_MMIO_REG_LC1_RAM_WE:
            result = _clem_mmio_read_bank_select(mmio, ioreg, flags);
            break;
        case CLEM_MMIO_REG_IWM_PHASE0_LO:
        case CLEM_MMIO_REG_IWM_PHASE0_HI:
        case CLEM_MMIO_REG_IWM_PHASE1_LO:
        case CLEM_MMIO_REG_IWM_PHASE1_HI:
        case CLEM_MMIO_REG_IWM_PHASE2_LO:
        case CLEM_MMIO_REG_IWM_PHASE2_HI:
        case CLEM_MMIO_REG_IWM_PHASE3_LO:
        case CLEM_MMIO_REG_IWM_PHASE3_HI:
        case CLEM_MMIO_REG_IWM_DRIVE_DISABLE:
        case CLEM_MMIO_REG_IWM_DRIVE_ENABLE:
        case CLEM_MMIO_REG_IWM_DRIVE_0:
        case CLEM_MMIO_REG_IWM_DRIVE_1:
        case CLEM_MMIO_REG_IWM_Q6_LO:
        case CLEM_MMIO_REG_IWM_Q6_HI:
        case CLEM_MMIO_REG_IWM_Q7_LO:
        case CLEM_MMIO_REG_IWM_Q7_HI:
            result = clem_iwm_read_switch(&mmio->dev_iwm,
                                          &clem->active_drives,
                                          &ref_clock,
                                          ioreg,
                                          flags);
            break;
        default:
            if (ioreg >= 0x71 && ioreg < 0x80) {
                result = clem->fpi_bank_map[0xff][0xc000 | ioreg];
            } else if (!is_noop) {
                clem_debug_break(&mmio->dev_debug, &clem->cpu,
                                 CLEM_DEBUG_BREAK_UNIMPL_IOREAD, addr, 0x0000);
            }
            break;
    }


    return result;
}

static void _clem_mmio_write(
    ClemensMachine* clem,
    uint8_t data,
    uint16_t addr,
    uint8_t mem_flags,
    bool *mega2_access
) {
    struct ClemensMMIO* mmio = &clem->mmio;
    struct ClemensClock ref_clock;
    bool is_noop = (mem_flags & CLEM_MMIO_READ_NO_OP) != 0;
    uint8_t ioreg;

    if (addr >= 0xC100) {
        //  TODO: MMIO slot ROM - it seems this needs to be treated differently
        return;
    }

    ioreg = (addr & 0xff);
    if (!is_noop) {
        ++mmio->dev_debug.ioreg_write_ctr[ioreg];
    }
    if (mem_flags != CLEM_MEM_FLAG_NULL) {
        *mega2_access = true;
    }

    ref_clock.ts = clem->clocks_spent;
    ref_clock.ref_step = clem->clocks_step_mega2;

    switch (ioreg) {
        case CLEM_MMIO_REG_80STOREOFF_WRITE:
            _clem_mmio_memory_map(mmio, mmio->mmap_register & ~CLEM_MMIO_MMAP_80COLSTORE);
            break;
        case CLEM_MMIO_REG_80STOREON_WRITE:
            _clem_mmio_memory_map(mmio, mmio->mmap_register | CLEM_MMIO_MMAP_80COLSTORE);
            break;
        case CLEM_MMIO_REG_RDMAINRAM:
            _clem_mmio_memory_map(mmio, mmio->mmap_register & ~CLEM_MMIO_MMAP_RAMRD);
            break;
        case CLEM_MMIO_REG_RDCARDRAM:
            _clem_mmio_memory_map(mmio, mmio->mmap_register | CLEM_MMIO_MMAP_RAMRD);
            break;
        case CLEM_MMIO_REG_WRMAINRAM:
            _clem_mmio_memory_map(mmio, mmio->mmap_register & ~CLEM_MMIO_MMAP_RAMWRT);
            break;
        case CLEM_MMIO_REG_WRCARDRAM:
            _clem_mmio_memory_map(mmio, mmio->mmap_register | CLEM_MMIO_MMAP_RAMWRT);
            break;
        case CLEM_MMIO_REG_SLOTCXROM:
            _clem_mmio_memory_map(mmio, mmio->mmap_register | CLEM_MMIO_MMAP_CXROM);
            break;
        case CLEM_MMIO_REG_INTCXROM:
            _clem_mmio_memory_map(mmio, mmio->mmap_register & ~CLEM_MMIO_MMAP_CXROM);
            break;
        case CLEM_MMIO_REG_STDZP:
            _clem_mmio_memory_map(mmio, mmio->mmap_register & ~CLEM_MMIO_MMAP_ALTZPLC);
            break;
        case CLEM_MMIO_REG_ALTZP:
            _clem_mmio_memory_map(mmio, mmio->mmap_register | CLEM_MMIO_MMAP_ALTZPLC);
            break;
        case CLEM_MMIO_REG_SLOTC3ROM:
            _clem_mmio_memory_map(mmio, mmio->mmap_register | CLEM_MMIO_MMAP_C3ROM);
            break;
        case CLEM_MMIO_REG_INTC3ROM:
            _clem_mmio_memory_map(mmio, mmio->mmap_register & ~CLEM_MMIO_MMAP_C3ROM);
            break;
        case CLEM_MMIO_REG_80COLUMN_OFF:
            clem_vgc_clear_mode(&mmio->vgc, CLEM_VGC_80COLUMN_TEXT);
            break;
        case CLEM_MMIO_REG_80COLUMN_ON:
            clem_vgc_set_mode(&mmio->vgc, CLEM_VGC_80COLUMN_TEXT);
            break;
        case CLEM_MMIO_REG_ALTCHARSET_OFF:
            clem_vgc_clear_mode(&mmio->vgc, CLEM_VGC_ALTCHARSET);
            break;
        case CLEM_MMIO_REG_ALTCHARSET_ON:
            clem_vgc_set_mode(&mmio->vgc, CLEM_VGC_ALTCHARSET);
            break;
        case CLEM_MMIO_REG_VGC_MONO:
            if (data & 0x80) {
                clem_vgc_set_mode(&mmio->vgc, CLEM_VGC_MONOCHROME);
            } else {
                clem_vgc_clear_mode(&mmio->vgc, CLEM_VGC_MONOCHROME);
            }
            break;
        case CLEM_MMIO_REG_VGC_TEXT_COLOR:
            clem_vgc_set_text_colors(&mmio->vgc, (data & 0xf0) >> 4, data & 0x0f);
            break;
        case CLEM_MMIO_REG_VGC_IRQ_BYTE:
            _clem_mmio_vgc_irq_c023_set(mmio, data);
            break;
        case CLEM_MMIO_REG_ANYKEY_STROBE:
        case CLEM_MMIO_REG_ADB_MOUSE_DATA:
        case CLEM_MMIO_REG_ADB_MODKEY:
        case CLEM_MMIO_REG_ADB_CMD_DATA:
        case CLEM_MMIO_REG_ADB_STATUS:
            clem_adb_write_switch(&mmio->dev_adb, ioreg, data);
            break;
        case CLEM_MMIO_REG_NEWVIDEO:
            _clem_mmio_newvideo_c029_set(mmio, data);
            break;
        case CLEM_MMIO_REG_LANGSEL:
            clem_vgc_set_region(&mmio->vgc, data);
            break;
        case CLEM_MMIO_REG_SLOTROMSEL:
            _clem_mmio_slotrom_select_c02d(mmio, data);
            break;
        case CLEM_MMIO_REG_SPKR:
            clem_sound_write_switch(&mmio->dev_audio, ioreg, data);
            break;
        case CLEM_MMIO_REG_DISK_INTERFACE:
            clem_iwm_write_switch(&mmio->dev_iwm,
                                  &clem->active_drives,
                                  &ref_clock,
                                  ioreg,
                                  data);
            break;
        case CLEM_MMIO_REG_RTC_SCANINT:
            _clem_mmio_mega2_clear_irq(mmio, data);
            break;
        case CLEM_MMIO_REG_RTC_CTL:
            mmio->dev_rtc.ctl_c034 = data;
            clem_rtc_command(&mmio->dev_rtc, clem->clocks_spent, CLEM_IO_WRITE);
            break;
        case CLEM_MMIO_REG_RTC_DATA:
            mmio->dev_rtc.data_c033 = data;
            break;
        case CLEM_MMIO_REG_SHADOW:
            _clem_mmio_shadow_c035_set(mmio, data);
            break;
        case CLEM_MMIO_REG_SPEED:
            _clem_mmio_speed_c036_set(clem, data);
            break;
        case CLEM_MMIO_REG_SCC_B_CMD:
        case CLEM_MMIO_REG_SCC_A_CMD:
        case CLEM_MMIO_REG_SCC_B_DATA:
        case CLEM_MMIO_REG_SCC_A_DATA:
            clem_scc_write_switch(&mmio->dev_scc, ioreg, data);
            break;
        case CLEM_MMIO_REG_AUDIO_CTL:
            clem_sound_write_switch(&mmio->dev_audio, ioreg, data);
            break;
        case CLEM_MMIO_REG_AUDIO_DATA:
            clem_sound_write_switch(&mmio->dev_audio, ioreg, data);
            break;
        case CLEM_MMIO_REG_AUDIO_ADRLO:
            clem_sound_write_switch(&mmio->dev_audio, ioreg, data);
            break;
        case CLEM_MMIO_REG_AUDIO_ADRHI:
            clem_sound_write_switch(&mmio->dev_audio, ioreg, data);
            break;
        case CLEM_MMIO_REG_MEGA2_INTEN:
            _clem_mmio_mega2_inten_set(mmio, data);
            break;
        case CLEM_MMIO_REG_CLRVBLINT:
            _clem_mmio_clear_irq(mmio, CLEM_IRQ_TIMER_QSEC | CLEM_IRQ_VGC_BLANK);
            break;
        case CLEM_MMIO_REG_TXTCLR:
            clem_vgc_set_mode(&mmio->vgc, CLEM_VGC_GRAPHICS_MODE);
            break;
        case CLEM_MMIO_REG_TXTSET:
            clem_vgc_clear_mode(&mmio->vgc, CLEM_VGC_GRAPHICS_MODE);
            break;
        case CLEM_MMIO_REG_MIXCLR:
            clem_vgc_clear_mode(&mmio->vgc, CLEM_VGC_MIXED_TEXT);
            break;
        case CLEM_MMIO_REG_MIXSET:
            clem_vgc_set_mode(&mmio->vgc, CLEM_VGC_MIXED_TEXT);
            break;
        case CLEM_MMIO_REG_TXTPAGE1:
            _clem_mmio_memory_map(mmio, mmio->mmap_register & ~CLEM_MMIO_MMAP_TXTPAGE2);
            break;
        case CLEM_MMIO_REG_TXTPAGE2:
            _clem_mmio_memory_map(mmio, mmio->mmap_register | CLEM_MMIO_MMAP_TXTPAGE2);
            break;
        case CLEM_MMIO_REG_LORES:
            /* implicitly clears hires */
            clem_vgc_set_mode(&mmio->vgc, CLEM_VGC_LORES);
            break;
        case CLEM_MMIO_REG_HIRES:
            /* implicitly clears lores */
            clem_vgc_set_mode(&mmio->vgc, CLEM_VGC_HIRES);
            break;
        case CLEM_MMIO_REG_AN0_OFF:
        case CLEM_MMIO_REG_AN0_ON:
        case CLEM_MMIO_REG_AN1_OFF:
        case CLEM_MMIO_REG_AN1_ON:
        case CLEM_MMIO_REG_AN2_OFF:
        case CLEM_MMIO_REG_AN2_ON:
        case CLEM_MMIO_REG_AN3_OFF:
        case CLEM_MMIO_REG_AN3_ON:
            clem_adb_write_switch(&mmio->dev_adb, ioreg, data);
            break;
        case CLEM_MMIO_REG_STATEREG:
            _clem_mmio_statereg_c068_set(mmio, data);
            break;
        case CLEM_MMIO_REG_IWM_PHASE0_LO:
        case CLEM_MMIO_REG_IWM_PHASE0_HI:
        case CLEM_MMIO_REG_IWM_PHASE1_LO:
        case CLEM_MMIO_REG_IWM_PHASE1_HI:
        case CLEM_MMIO_REG_IWM_PHASE2_LO:
        case CLEM_MMIO_REG_IWM_PHASE2_HI:
        case CLEM_MMIO_REG_IWM_PHASE3_LO:
        case CLEM_MMIO_REG_IWM_PHASE3_HI:
        case CLEM_MMIO_REG_IWM_DRIVE_DISABLE:
        case CLEM_MMIO_REG_IWM_DRIVE_ENABLE:
        case CLEM_MMIO_REG_IWM_DRIVE_0:
        case CLEM_MMIO_REG_IWM_DRIVE_1:
        case CLEM_MMIO_REG_IWM_Q6_LO:
        case CLEM_MMIO_REG_IWM_Q6_HI:
        case CLEM_MMIO_REG_IWM_Q7_LO:
        case CLEM_MMIO_REG_IWM_Q7_HI:
            clem_iwm_write_switch(&mmio->dev_iwm,
                                  &clem->active_drives,
                                  &ref_clock,
                                  ioreg,
                                  data);
            break;
        default:
            if (!is_noop) {
                clem_debug_break(&mmio->dev_debug, &clem->cpu,
                                 CLEM_DEBUG_BREAK_UNIMPL_IOWRITE, addr, data);
            }
            break;
    }
}

static void _clem_mmio_shadow_map(
    struct ClemensMMIO* mmio,
    uint32_t shadow_flags
) {
    /* Sets up which pages are shadowed on banks 00, 01.  Flags tested inside
       _clem_write determine if the write operation actual performs the copy to
       E0, E1
    */
    unsigned remap_flags = mmio->mmap_register ^ shadow_flags;
    unsigned page_idx;
    bool inhibit_hgr_bank_01 = (shadow_flags & CLEM_MMIO_MMAP_NSHADOW_AUX) != 0;
    bool inhibit_shgr_bank_01 = (shadow_flags & CLEM_MMIO_MMAP_NSHADOW_SHGR) != 0;

    //  TXT 1
    if (remap_flags & CLEM_MMIO_MMAP_NSHADOW_TXT1) {
        for (page_idx = 0x04; page_idx < 0x08; ++page_idx) {
            uint8_t v = (shadow_flags & CLEM_MMIO_MMAP_NSHADOW_TXT1) ? 0 : 1;
            mmio->fpi_mega2_main_shadow_map.pages[page_idx] = v;
            mmio->fpi_mega2_aux_shadow_map.pages[page_idx] = v;
        }
    }
    //  TXT 2
    if (remap_flags & CLEM_MMIO_MMAP_NSHADOW_TXT2) {
        for (page_idx = 0x08; page_idx < 0x0C; ++page_idx) {
            uint8_t v = (shadow_flags & CLEM_MMIO_MMAP_NSHADOW_TXT2) ? 0 : 1;
            mmio->fpi_mega2_main_shadow_map.pages[page_idx] = v;
            mmio->fpi_mega2_aux_shadow_map.pages[page_idx] = v;
        }
    }
    //  HGR1
    if ((remap_flags & CLEM_MMIO_MMAP_NSHADOW_HGR1) ||
        (remap_flags & CLEM_MMIO_MMAP_NSHADOW_AUX) ||
        (remap_flags & CLEM_MMIO_MMAP_NSHADOW_SHGR)
    ) {
        for (page_idx = 0x20; page_idx < 0x40; ++page_idx) {
            uint8_t v0 = (shadow_flags & CLEM_MMIO_MMAP_NSHADOW_HGR1) ? 0 : 1;
            uint8_t v1 = (v0 && !inhibit_hgr_bank_01) ? 1 : 0;
            if (!inhibit_shgr_bank_01 && !v1) v1 = 1;
            mmio->fpi_mega2_main_shadow_map.pages[page_idx] = v0;
            mmio->fpi_mega2_aux_shadow_map.pages[page_idx] = v1;
        }
    }
    if ((remap_flags & CLEM_MMIO_MMAP_NSHADOW_HGR2) ||
        (remap_flags & CLEM_MMIO_MMAP_NSHADOW_AUX) ||
        (remap_flags & CLEM_MMIO_MMAP_NSHADOW_SHGR)
    ) {
        for (page_idx = 0x40; page_idx < 0x60; ++page_idx) {
            uint8_t v0 = (shadow_flags & CLEM_MMIO_MMAP_NSHADOW_HGR2) ? 0 : 1;
            uint8_t v1 = (v0 && !inhibit_hgr_bank_01) ? 1 : 0;
            if (!inhibit_shgr_bank_01 && !v1) v1 = 1;
            mmio->fpi_mega2_main_shadow_map.pages[page_idx] = v0;
            mmio->fpi_mega2_aux_shadow_map.pages[page_idx] = v1;
        }
    }
    if (remap_flags & CLEM_MMIO_MMAP_NSHADOW_SHGR) {
        for (page_idx = 0x60; page_idx < 0xA0; ++page_idx) {
            uint8_t v1 = inhibit_shgr_bank_01 ? 0 : 1;
            mmio->fpi_mega2_aux_shadow_map.pages[page_idx] = v1;
        }
    }
}

/*  Banks 02-7F and FC-FF almost always keep the same memory mapping.
    Banks 00, 01, E0, E1 commonly experience the most remappings:

    00: Switch ZP+Stack, IOB, LCB1, LCB2, LC/ROM0
        Shadow TXT1, TXT2, HGR1, HGR2
    01: Switch ZP+Stack, IOB, LCB1, LCB2, LC/ROM0
        Shadow TXT1, TXT2, HGR1, HGR2, SHGR

    Strategy is to apply //e softswitches first, and then apply shadow (iigs)
    switches (iolc inhibit)

*/
static void _clem_mmio_memory_map(
    struct ClemensMMIO* mmio,
    uint32_t memory_flags
) {
    struct ClemensMMIOPageMap* page_map_B00;
    struct ClemensMMIOPageMap* page_map_B01;
    struct ClemensMMIOPageMap* page_map_BE0;
    struct ClemensMMIOPageMap* page_map_BE1;
    struct ClemensMMIOPageInfo* page_B00;
    struct ClemensMMIOPageInfo* page_B01;
    struct ClemensMMIOPageInfo* page_BE0;
    struct ClemensMMIOPageInfo* page_BE1;
    unsigned remap_flags = mmio->mmap_register ^ memory_flags;
    unsigned page_idx;

    page_map_B00 = &mmio->fpi_main_page_map;
    page_map_B01 = &mmio->fpi_aux_page_map;
    page_map_BE0 = &mmio->mega2_main_page_map;
    page_map_BE1 = &mmio->mega2_aux_page_map;

    //  ALTZPLC is a main bank-only softswitch.  As a result 01, E0, E1 bank
    //      maps for page 0, 1 remain unchanged
    if (remap_flags & CLEM_MMIO_MMAP_ALTZPLC) {
        //  TODO: do LC mappings also change?  //e docs state that soft switches
        //        should be explicitly set again when switching banks, but
        //        looking at other emulators (yes... this is driving me crazy
        //        imply otherwise.  when testing with real software, determine
        //        which requirement is true.)
        remap_flags |= CLEM_MMIO_MMAP_LC;
        for (page_idx = 0x00; page_idx < 0x02; ++page_idx) {
            page_B00 = &page_map_B00->pages[page_idx];
            if (memory_flags & CLEM_MMIO_MMAP_ALTZPLC) {
                page_B00->bank_read = 0x01;
                page_B00->bank_write = 0x01;
            } else {
                page_B00->bank_read = 0x00;
                page_B00->bank_write = 0x00;
            }
        }
    }

    //  TODO: 80COLSTORE and TXTPAGE2 will override RAMRD, RAMWRT and so
    //        we check the 80COLSTORE flags before RAMRD/WRT
    //  Shadowing is always applied after the write to 00/01, so the remapping
    //      here should automatically be shdaowed to the appropriate e0/e1
    //      area for display.
    if (remap_flags & CLEM_MMIO_MMAP_OLDVIDEO) {
        for (page_idx = 0x04; page_idx < 0x08; ++page_idx) {
            page_B00 = &page_map_B00->pages[page_idx];
            if (memory_flags & CLEM_MMIO_MMAP_80COLSTORE) {
                page_B00->bank_read = (
                    (memory_flags & CLEM_MMIO_MMAP_TXTPAGE2) ? 0x01 : 00);
                page_B00->bank_write =(
                    (memory_flags & CLEM_MMIO_MMAP_TXTPAGE2) ? 0x01 : 00);
            } else {
                page_B00->bank_read =(
                    (memory_flags & CLEM_MMIO_MMAP_RAMRD) ? 0x01 : 00);
                page_B00->bank_write =(
                    (memory_flags & CLEM_MMIO_MMAP_RAMWRT) ? 0x01 : 00);
            }
        }
        for (page_idx = 0x20; page_idx < 0x40; ++page_idx) {
            page_B00 = &page_map_B00->pages[page_idx];
            if (memory_flags & (CLEM_MMIO_MMAP_80COLSTORE + CLEM_MMIO_MMAP_HIRES)) {
                page_B00->bank_read =(
                    (memory_flags & CLEM_MMIO_MMAP_TXTPAGE2) ? 0x01 : 00);
                page_B00->bank_write =(
                    (memory_flags & CLEM_MMIO_MMAP_TXTPAGE2) ? 0x01 : 00);
            } else {
                page_B00->bank_read =(
                    (memory_flags & CLEM_MMIO_MMAP_RAMRD) ? 0x01 : 00);
                page_B00->bank_write =(
                    (memory_flags & CLEM_MMIO_MMAP_RAMWRT) ? 0x01 : 00);
            }
        }
    }

    //  RAMRD/RAMWRT minus the page 1 Apple //e video regions
    if (remap_flags & (CLEM_MMIO_MMAP_RAMRD + CLEM_MMIO_MMAP_RAMWRT)) {
        remap_flags |= CLEM_MMIO_MMAP_NSHADOW;
        for (page_idx = 0x02; page_idx < 0x03; ++page_idx) {
            page_B00 = &page_map_B00->pages[page_idx];
            page_B00->bank_read =(
                (memory_flags & CLEM_MMIO_MMAP_RAMRD) ? 0x01 : 00);
            page_B00->bank_write =(
                (memory_flags & CLEM_MMIO_MMAP_RAMWRT) ? 0x01 : 00);
        }
        for (page_idx = 0x08; page_idx < 0x20; ++page_idx) {
            page_B00 = &page_map_B00->pages[page_idx];
            page_B00->bank_read =(
                (memory_flags & CLEM_MMIO_MMAP_RAMRD) ? 0x01 : 00);
            page_B00->bank_write =(
                (memory_flags & CLEM_MMIO_MMAP_RAMWRT) ? 0x01 : 00);
        }
        for (page_idx = 0x40; page_idx < 0xC0; ++page_idx) {
            page_B00 = &page_map_B00->pages[page_idx];
            page_B00->bank_read =(
                (memory_flags & CLEM_MMIO_MMAP_RAMRD) ? 0x01 : 00);
            page_B00->bank_write =(
                (memory_flags & CLEM_MMIO_MMAP_RAMWRT) ? 0x01 : 00);
        }
    }

    //  Shadowing
    if (remap_flags & CLEM_MMIO_MMAP_NSHADOW) {
        _clem_mmio_shadow_map(mmio, memory_flags & CLEM_MMIO_MMAP_NSHADOW);
    }

    //  I/O space mapping
    //  IOLC switch changed, which requires remapping the entire language card
    //  region + the I/O region (for FPI memory - Mega2 doesn't deal with
    //  shadowing or LC ROM mapping)
    if (remap_flags & (CLEM_MMIO_MMAP_NIOLC + CLEM_MMIO_MMAP_CROM)) {
        if (remap_flags & CLEM_MMIO_MMAP_NIOLC) {
            remap_flags |= CLEM_MMIO_MMAP_LC;
        }

        page_B00 = &page_map_B00->pages[0xC0];
        page_B01 = &page_map_B01->pages[0xC0];
        _clem_mmio_create_page_mainaux_mapping(page_B00, 0xC0, 0x00);
        _clem_mmio_create_page_mainaux_mapping(page_B01, 0xC0, 0x01);

        if (memory_flags & CLEM_MMIO_MMAP_NIOLC) {
            page_B00->flags &= ~CLEM_MMIO_PAGE_IOADDR;
            page_B01->flags &= ~CLEM_MMIO_PAGE_IOADDR;
            for (page_idx = 0xC1; page_idx < 0xD0; ++page_idx) {
                page_B00 = &page_map_B00->pages[page_idx];
                _clem_mmio_create_page_mainaux_mapping(page_B00, page_idx, 0x00);
                page_B00->flags |= CLEM_MMIO_PAGE_WRITE_OK;
                page_B01 = &page_map_B01->pages[page_idx];
                _clem_mmio_create_page_mainaux_mapping(page_B01, page_idx, 0x01);
                page_B01->flags |= CLEM_MMIO_PAGE_WRITE_OK;
            }
        } else {
            page_B00->flags |= CLEM_MMIO_PAGE_IOADDR;
            page_B01->flags |= CLEM_MMIO_PAGE_IOADDR;
            for (page_idx = 0xC1; page_idx < 0xC8; ++page_idx) {
                unsigned slot_idx = ((page_idx - 1) & 0xf) >> 4;
                /* INTCXROM from IIgs specific status reg takes precedence */
                bool intcx_page = !(memory_flags & CLEM_MMIO_MMAP_CXROM) || !(
                    memory_flags & (CLEM_MMIO_MMAP_C1ROM << slot_idx));

                // TODO: peripheral ROM and slot 3 switch
                page_B00 = &page_map_B00->pages[page_idx];
                page_B01 = &page_map_B01->pages[page_idx];
                if (intcx_page) {
                    _clem_mmio_create_page_mapping(page_B00, page_idx, 0xff, 0x00);
                    _clem_mmio_create_page_mapping(page_B01, page_idx, 0xff, 0x01);
                } else {
                    _clem_mmio_create_page_mapping(page_B00, slot_idx, 0x00, 0x00);
                    _clem_mmio_create_page_mapping(page_B01, slot_idx, 0x00, 0x00);
                    page_B00->flags |= CLEM_MMIO_PAGE_CARDMEM;
                    page_B01->flags |= CLEM_MMIO_PAGE_CARDMEM;
                }
                page_B00->flags &= ~CLEM_MMIO_PAGE_WRITE_OK;
                page_B01->flags &= ~CLEM_MMIO_PAGE_WRITE_OK;
            }
            for (page_idx = 0xC8; page_idx < 0xD0; ++page_idx) {
                bool intcx_page = !(memory_flags & CLEM_MMIO_MMAP_CXROM) ||
                    mmio->card_expansion_rom_index < 0;
                page_B00 = &page_map_B00->pages[page_idx];
                page_B01 = &page_map_B01->pages[page_idx];
                if (intcx_page) {
                    /* internal ROM */
                    _clem_mmio_create_page_mapping(page_B00, page_idx, 0xff, 0x00);
                    _clem_mmio_create_page_mapping(page_B01, page_idx, 0xff, 0x01);
                } else {
                    _clem_mmio_create_page_mapping(page_B00, page_idx - 0xc8, 0xcc, 0xcc);
                    _clem_mmio_create_page_mapping(page_B01, page_idx - 0xc8, 0xcc, 0xcc);
                    page_B00->flags |= CLEM_MMIO_PAGE_CARDMEM;
                    page_B01->flags |= CLEM_MMIO_PAGE_CARDMEM;
                }
                page_B00->flags &= ~CLEM_MMIO_PAGE_WRITE_OK;
                page_B01->flags &= ~CLEM_MMIO_PAGE_WRITE_OK;
            }
        }
        //  e0, e1 isn't affected by shadowing
        if (remap_flags & CLEM_MMIO_MMAP_CROM) {
            for (page_idx = 0xC1; page_idx < 0xC8; ++page_idx) {
                unsigned slot_idx = ((page_idx - 1) & 0xf) >> 4;
                bool intcx_page = !(memory_flags & CLEM_MMIO_MMAP_CXROM) || !(
                    memory_flags & (CLEM_MMIO_MMAP_C1ROM << slot_idx));
                page_BE0 = &page_map_BE0->pages[page_idx];
                page_BE1 = &page_map_BE1->pages[page_idx];
                if (intcx_page) {
                    _clem_mmio_create_page_mapping(page_BE0, page_idx, 0xff, 0xe0);
                    _clem_mmio_create_page_mapping(page_BE1, page_idx, 0xff, 0xe1);
                } else {
                    _clem_mmio_create_page_mapping(page_BE0, slot_idx, 0x00, 0x00);
                    _clem_mmio_create_page_mapping(page_BE1, slot_idx, 0x00, 0x00);
                    page_B00->flags |= CLEM_MMIO_PAGE_CARDMEM;
                    page_B01->flags |= CLEM_MMIO_PAGE_CARDMEM;
                }
                page_BE0->flags &= ~CLEM_MMIO_PAGE_WRITE_OK;
                page_BE1->flags &= ~CLEM_MMIO_PAGE_WRITE_OK;

            }
            for (page_idx = 0xC8; page_idx < 0xD0; ++page_idx) {
                bool intcx_page = !(memory_flags & CLEM_MMIO_MMAP_CXROM) ||
                    mmio->card_expansion_rom_index < 0;
                page_BE0 = &page_map_BE0->pages[page_idx];
                page_BE1 = &page_map_BE1->pages[page_idx];
                if (intcx_page) {
                    /* internal ROM */
                    _clem_mmio_create_page_mapping(page_BE0, page_idx, 0xff, 0xe0);
                    _clem_mmio_create_page_mapping(page_BE1, page_idx, 0xff, 0xe1);
                } else {
                    _clem_mmio_create_page_mapping(page_BE0, page_idx-0xc8, 0xcc, 0xcc);
                    _clem_mmio_create_page_mapping(page_BE1, page_idx-0xc8, 0xcc, 0xcc);
                    page_BE0->flags |= CLEM_MMIO_PAGE_CARDMEM;
                    page_BE1->flags |= CLEM_MMIO_PAGE_CARDMEM;
                }
                page_BE0->flags &= ~CLEM_MMIO_PAGE_WRITE_OK;
                page_BE1->flags &= ~CLEM_MMIO_PAGE_WRITE_OK;
            }
        }
    }

    //  Language Card softswitches - ROM/RAM/IOLC for Bank 00/01,
    //                               RAM for Bank E0/E1
    if (remap_flags & CLEM_MMIO_MMAP_LC) {
        bool is_rom_bank_0x = !(memory_flags & CLEM_MMIO_MMAP_NIOLC) &&
                              !(memory_flags & CLEM_MMIO_MMAP_RDLCRAM);
        for (page_idx = 0xD0; page_idx < 0xE0; ++page_idx) {
            page_B00 = &page_map_B00->pages[page_idx];
            page_B01 = &page_map_B01->pages[page_idx];
            page_BE0 = &page_map_BE0->pages[page_idx];
            page_BE1 = &page_map_BE1->pages[page_idx];
            if (memory_flags & CLEM_MMIO_MMAP_ALTZPLC) {
                page_B00->bank_read = is_rom_bank_0x ? 0xff : 0x01;
                page_B00->bank_write = 0x01;
            } else {
                page_B00->bank_read = is_rom_bank_0x ? 0xff : 0x00;
                page_B00->bank_write = 0x00;
            }
            page_B01->bank_read = is_rom_bank_0x ? 0xff : 0x01;
            page_B01->bank_write = 0x01;
            if (is_rom_bank_0x) {
                page_B00->flags &= ~CLEM_MMIO_PAGE_MAINAUX;
                page_B01->flags &= ~CLEM_MMIO_PAGE_MAINAUX;
            } else {
                page_B00->flags |= CLEM_MMIO_PAGE_MAINAUX;
                page_B01->flags |= CLEM_MMIO_PAGE_MAINAUX;
            }
            //  Bank 00, 01 IOLC
            if (memory_flags & (CLEM_MMIO_MMAP_NIOLC + CLEM_MMIO_MMAP_LCBANK2)) {
                page_B00->read = page_idx;
                page_B00->write = page_idx;
                page_B01->read = page_idx;
                page_B01->write = page_idx;
            } else if (!(memory_flags & CLEM_MMIO_MMAP_LCBANK2)) {
                //  LC bank 1 = 0xC000-0xCFFF
                page_B00->read = 0xC0 + (page_idx - 0xD0);
                page_B00->write = 0xC0 + (page_idx - 0xD0);
                page_B01->read = 0xC0 + (page_idx - 0xD0);
                page_B01->write = 0xC0 + (page_idx - 0xD0);
            }
            if (memory_flags & CLEM_MMIO_MMAP_LCBANK2) {
                page_BE0->read = page_idx;
                page_BE0->write = page_idx;
                page_BE1->read = page_idx;
                page_BE1->write = page_idx;
            } else {
                //  LC bank 1 = 0xC000-0xCFFF
                page_BE0->read = 0xC0 + (page_idx - 0xD0);
                page_BE0->write = 0xC0 + (page_idx - 0xD0);
                page_BE1->read = 0xC0 + (page_idx - 0xD0);
                page_BE1->write = 0xC0 + (page_idx - 0xD0);
            }
            if (memory_flags & CLEM_MMIO_MMAP_NIOLC) {
                //  diabled LC - region treated as writable RAM
                page_B00->flags |= CLEM_MMIO_PAGE_WRITE_OK;
                page_B01->flags |= CLEM_MMIO_PAGE_WRITE_OK;
            } else {
                if (memory_flags & CLEM_MMIO_MMAP_WRLCRAM) {
                    page_B00->flags |= CLEM_MMIO_PAGE_WRITE_OK;
                    page_B01->flags |= CLEM_MMIO_PAGE_WRITE_OK;
                } else {
                    page_B00->flags &= ~CLEM_MMIO_PAGE_WRITE_OK;
                    page_B01->flags &= ~CLEM_MMIO_PAGE_WRITE_OK;
                }
            }
        }
        for (page_idx = 0xE0; page_idx < 0x100; ++page_idx) {
            page_B00 = &page_map_B00->pages[page_idx];
            page_B01 = &page_map_B01->pages[page_idx];
            page_BE0 = &page_map_BE0->pages[page_idx];
            page_BE1 = &page_map_BE1->pages[page_idx];
            if (memory_flags & CLEM_MMIO_MMAP_ALTZPLC) {
                page_B00->bank_read = is_rom_bank_0x ? 0xff : 0x01;
                page_B00->bank_write = 0x01;
            } else {
                page_B00->bank_read = is_rom_bank_0x ? 0xff : 0x00;
                page_B00->bank_write = 0x00;
            }
            page_B01->bank_read = is_rom_bank_0x ? 0xff : 0x01;
            page_B01->bank_write = 0x01;

            if (is_rom_bank_0x) {
                page_B00->flags &= ~CLEM_MMIO_PAGE_MAINAUX;
                page_B01->flags &= ~CLEM_MMIO_PAGE_MAINAUX;
            } else {
                page_B00->flags |= CLEM_MMIO_PAGE_MAINAUX;
                page_B01->flags |= CLEM_MMIO_PAGE_MAINAUX;
            }
            //  Bank 00, 01 IOLC
            page_B00->read = page_idx;
            page_B00->write = page_idx;
            page_B01->read = page_idx;
            page_B01->write = page_idx;
            page_BE0->read = page_idx;
            page_BE0->write = page_idx;
            page_BE1->read = page_idx;
            page_BE1->write = page_idx;
            if (memory_flags & CLEM_MMIO_MMAP_NIOLC) {
                //  diabled LC - region treated as writable RAM
                page_B00->flags |= CLEM_MMIO_PAGE_WRITE_OK;
                page_B01->flags |= CLEM_MMIO_PAGE_WRITE_OK;
            } else {
                if (memory_flags & CLEM_MMIO_MMAP_WRLCRAM) {
                    page_B00->flags |= CLEM_MMIO_PAGE_WRITE_OK;
                    page_B01->flags |= CLEM_MMIO_PAGE_WRITE_OK;
                } else {
                    page_B00->flags &= ~CLEM_MMIO_PAGE_WRITE_OK;
                    page_B01->flags &= ~CLEM_MMIO_PAGE_WRITE_OK;
                }
            }
        }
    }

    mmio->mmap_register = memory_flags;
}

void _clem_mmio_init_page_maps(
    struct ClemensMMIO* mmio,
    uint32_t memory_flags
) {
    struct ClemensMMIOPageMap* page_map;
    struct ClemensMMIOPageInfo* page;
    unsigned page_idx;
    unsigned bank_idx;

    //  Bank 00, 01 as RAM
    //  TODO need to mask bank for main and aux page maps

    page_map = &mmio->empty_page_map;
    page_map->shadow_map = NULL;
    for (page_idx = 0x00; page_idx < 0x100; ++page_idx) {
        /* using a non-valid IIgs bank here that's not writable. */
        page = &page_map->pages[page_idx];
        _clem_mmio_create_page_mapping(
            page,
            page_idx,
            CLEM_IIGS_EMPTY_RAM_BANK,
            CLEM_IIGS_EMPTY_RAM_BANK);
        page->flags &= ~CLEM_MMIO_PAGE_WRITE_OK;
    }

    page_map = &mmio->fpi_main_page_map;
    page_map->shadow_map = &mmio->fpi_mega2_main_shadow_map;
    for (page_idx = 0x00; page_idx < 0x100; ++page_idx) {
        _clem_mmio_create_page_mainaux_mapping(
            &page_map->pages[page_idx], page_idx, 0x00);
    }
    page_map = &mmio->fpi_aux_page_map;
    page_map->shadow_map = &mmio->fpi_mega2_aux_shadow_map;
    for (page_idx = 0x00; page_idx < 0x100; ++page_idx) {
        _clem_mmio_create_page_mainaux_mapping(
            &page_map->pages[page_idx], page_idx, 0x01);
    }
    //  Banks 02-7f typically (if expanded memory is available)
    page_map = &mmio->fpi_direct_page_map;
    page_map->shadow_map = NULL;
    for (page_idx = 0x00; page_idx < 0x100; ++page_idx) {
        _clem_mmio_create_page_direct_mapping(
            &page_map->pages[page_idx], page_idx);
    }
    //  Banks E0 - C000-CFFF mapped as IO, Internal ROM
    page_map = &mmio->mega2_main_page_map;
    page_map->shadow_map = NULL;
    for (page_idx = 0x00; page_idx < 0x100; ++page_idx) {
        _clem_mmio_create_page_direct_mapping(
            &page_map->pages[page_idx], page_idx);
    }
    page_map->pages[0xC0].flags &= ~CLEM_MMIO_PAGE_DIRECT;
    page_map->pages[0xC0].flags |= CLEM_MMIO_PAGE_IOADDR;
    for (page_idx = 0xC1; page_idx < 0xD0; ++page_idx) {
        page = &page_map->pages[page_idx];
        _clem_mmio_create_page_mapping(page, page_idx, 0xff, 0xe0);
        page->flags &= ~CLEM_MMIO_PAGE_WRITE_OK;
    }
    //  Banks E1 - C000-CFFF mapped as IO, Internal ROM
    page_map = &mmio->mega2_aux_page_map;
    page_map->shadow_map = NULL;
    for (page_idx = 0x00; page_idx < 0x100; ++page_idx) {
        _clem_mmio_create_page_direct_mapping(
            &page_map->pages[page_idx], page_idx);
    }
    page_map->pages[0xC0].flags &= ~CLEM_MMIO_PAGE_DIRECT;
    page_map->pages[0xC0].flags |= CLEM_MMIO_PAGE_IOADDR;
    for (page_idx = 0xC1; page_idx < 0xD0; ++page_idx) {
        page = &page_map->pages[page_idx];
        _clem_mmio_create_page_mapping(page, page_idx, 0xff, 0xe1);
        page->flags &= ~CLEM_MMIO_PAGE_WRITE_OK;
    }
    //  Banks FC-FF ROM access is read-only of course.
    page_map = &mmio->fpi_rom_page_map;
    page_map->shadow_map = NULL;
    for (page_idx = 0x00; page_idx < 0x100; ++page_idx) {
        page = &page_map->pages[page_idx];
        _clem_mmio_create_page_direct_mapping(page, page_idx);
        page->flags &= ~CLEM_MMIO_PAGE_WRITE_OK;
    }

    //  set up the default page mappings
    memset(&mmio->bank_page_map, 0, sizeof(mmio->bank_page_map));
    mmio->bank_page_map[0x00] = &mmio->fpi_main_page_map;
    mmio->bank_page_map[0x01] = &mmio->fpi_aux_page_map;

    for (bank_idx = 0x02; bank_idx < CLEM_IIGS_FPI_MAIN_RAM_BANK_COUNT; ++bank_idx) {
        mmio->bank_page_map[bank_idx] = &mmio->fpi_direct_page_map;
    }
    /* TODO: handle expansion RAM */
    for (bank_idx = CLEM_IIGS_FPI_MAIN_RAM_BANK_COUNT; bank_idx < 0x80; ++bank_idx) {
        mmio->bank_page_map[bank_idx] = &mmio->empty_page_map;
    }
    /* Handles unavailable banks beyond the 0x80 bank IIgs hard RAM limit */
    for (bank_idx = 0x80; bank_idx < 0xF0; ++bank_idx) {
        mmio->bank_page_map[bank_idx] = &mmio->empty_page_map;
    }
    /* Mega II banks */
    mmio->bank_page_map[0xE0] = &mmio->mega2_main_page_map;
    mmio->bank_page_map[0xE1] = &mmio->mega2_aux_page_map;
    /* TODO: handle expansion ROM and 128K firmware ROM 01*/
    for (bank_idx = 0xF0; bank_idx < 0xFC; ++bank_idx) {
        mmio->bank_page_map[bank_idx] = &mmio->empty_page_map;
    }
    for (bank_idx = 0xFC; bank_idx < 0x100; ++bank_idx) {
        mmio->bank_page_map[bank_idx] = &mmio->fpi_rom_page_map;
    }

    memset(&mmio->fpi_mega2_main_shadow_map, 0,
           sizeof(mmio->fpi_mega2_main_shadow_map));
    memset(&mmio->fpi_mega2_aux_shadow_map, 0,
           sizeof(mmio->fpi_mega2_aux_shadow_map));

    /* brute force initialization of all page maps to ensure every option
       is executed on startup */
    mmio->mmap_register = 0xffffffff;
    _clem_mmio_memory_map(mmio, 0x0000000000);
    _clem_mmio_memory_map(mmio, memory_flags);
}

void _clem_mmio_reset(
    struct ClemensMMIO* mmio,
    clem_clocks_duration_t mega2_clocks_step
) {
    clem_debug_reset(&mmio->dev_debug);
    clem_timer_reset(&mmio->dev_timer);
    clem_rtc_reset(&mmio->dev_rtc, mega2_clocks_step);
    clem_adb_reset(&mmio->dev_adb);
    clem_sound_reset(&mmio->dev_audio);
    clem_vgc_init(&mmio->vgc);
    clem_iwm_reset(&mmio->dev_iwm);
    clem_scc_reset(&mmio->dev_scc);
}

void _clem_mmio_init(
    struct ClemensMMIO* mmio,
    clem_clocks_duration_t mega2_clocks_step
) {
    //  Memory map starts out without shadowing, but our call to
    //  init_page_maps will initialize the memory map on IIgs reset
    //  Fast CPU mode
    //  TODO: support enabling bank latch if we ever need to as this would be
    //        the likely value at reset (bit set to 0 vs 1)
    mmio->new_video_c029 = CLEM_MMIO_NEWVIDEO_BANKLATCH_INHIBIT;
    //  TODO: ROM 01 will not use bit 6 and expect it to be cleared
    mmio->speed_c036 = CLEM_MMIO_SPEED_FAST_ENABLED |
                       CLEM_MMIO_SPEED_POWERED_ON;
    mmio->flags_c08x = 0;
    mmio->mega2_cycles = 0;
    mmio->card_expansion_rom_index = -1;

    _clem_mmio_init_page_maps(mmio,
                              CLEM_MMIO_MMAP_NSHADOW_SHGR |
                              CLEM_MMIO_MMAP_WRLCRAM |
                              CLEM_MMIO_MMAP_LCBANK2);

    _clem_mmio_reset(mmio, mega2_clocks_step);
}


void clem_read(
    ClemensMachine* clem,
    uint8_t* data,
    uint16_t adr,
    uint8_t bank,
    uint8_t flags
) {
    struct ClemensMMIOPageMap* bank_page_map = clem->mmio.bank_page_map[bank];
    struct ClemensMMIOPageInfo* page = &bank_page_map->pages[adr >> 8];
    uint16_t offset = ((uint16_t)page->read << 8) | (adr & 0xff);
    bool read_only = (flags == CLEM_MEM_FLAG_NULL);
    bool mega2_access = false;

    // TODO: store off if read_reg has a read_count of 1 here
    //       reset it automatically if true at the end of this function
    if (page->flags & CLEM_MMIO_IO_MEMORY) {
        if (page->flags & CLEM_MMIO_PAGE_IOADDR) {
            *data = _clem_mmio_read(
                clem, offset, read_only ? CLEM_MMIO_READ_NO_OP : 0, &mega2_access);
        } else if (page->flags & CLEM_MMIO_PAGE_CARDMEM) {
            if (page->bank_read == 0x00) {
                *data = clem->card_slot_memory[page->read][offset & 0xff];
            } else if (clem->mmio.card_expansion_rom_index >= 0) {
                *data = clem->card_slot_expansion_memory[(
                    clem->mmio.card_expansion_rom_index)][offset];
            } else {
                /* TODO: read from 'phantom' memory? */
                *data = 0x00;
                //CLEM_ASSERT(false);
            }
            /* assuming reads from card memory via MEGA2 are slow */
            mega2_access = true;
        } else {
            CLEM_ASSERT(false);
        }
    } else if (!(page->flags & CLEM_MMIO_PAGE_TYPE_MASK) ||
               (page->flags & CLEM_MMIO_BANK_MEMORY)
    ) {
        uint8_t* bank_mem;
        uint8_t bank_actual;
        if (page->flags & CLEM_MMIO_PAGE_DIRECT) {
            bank_actual = bank;
        } else if (page->flags & CLEM_MMIO_PAGE_MAINAUX) {
            bank_actual = (bank & 0xfe) | (page->bank_read & 0x1);
        } else {
            bank_actual = page->bank_read;
        }
        bank_mem = _clem_get_memory_bank(clem, bank_actual, &mega2_access);

        //  TODO: when reading from e0/e1 banks, is it always slow?
        //          internal ROM, peripheral?
        if (bank_actual == 0xe0 || bank_actual == 0xe1) {
            mega2_access = true;
        }
        *data = bank_mem[offset];
    } else {
        CLEM_ASSERT(false);
    }

    if (!read_only) {
        // TODO: account for slow/fast memory access
        clem->cpu.pins.adr = adr;
        clem->cpu.pins.bank = bank;
        clem->cpu.pins.data = *data;
        clem->cpu.pins.vpaOut = (flags & CLEM_MEM_FLAG_PROGRAM) != 0;
        clem->cpu.pins.vdaOut = (flags & CLEM_MEM_FLAG_DATA) != 0;
        clem->cpu.pins.rwbOut = true;
        _clem_mem_cycle(clem, mega2_access);
    }
}

void clem_write(
    ClemensMachine* clem,
    uint8_t data,
    uint16_t adr,
    uint8_t bank,
    uint8_t mem_flags
) {
    struct ClemensMMIOPageMap* bank_page_map = clem->mmio.bank_page_map[bank];
    struct ClemensMMIOShadowMap* shadow_map = bank_page_map->shadow_map;
    struct ClemensMMIOPageInfo* page = &bank_page_map->pages[adr >> 8];
    uint16_t offset = ((uint16_t)page->write << 8) | (adr & 0xff);
    bool mega2_access = false;

    if (bank == 0x00 && adr >= 0x2000 && adr <= 0x2001) {
        CLEM_LOG("%04X: %02X", adr, data);
    }

    if (page->flags & CLEM_MMIO_IO_MEMORY) {
        if (page->flags & CLEM_MMIO_PAGE_IOADDR) {
            if (page->flags & CLEM_MMIO_PAGE_WRITE_OK) {
                _clem_mmio_write(clem, data, offset, mem_flags, &mega2_access);
            } else {
                mega2_access = true;
            }
        } else if (page->flags & CLEM_MMIO_PAGE_CARDMEM) {
            //  Always ROM?
            CLEM_ASSERT(false);
        } else {
            CLEM_ASSERT(false);
        }
    } else if (!(page->flags & CLEM_MMIO_PAGE_TYPE_MASK) ||
                (page->flags & CLEM_MMIO_BANK_MEMORY)
    ) {
        uint8_t* bank_mem;
        uint8_t bank_actual;

        if (page->flags & CLEM_MMIO_PAGE_DIRECT) {
            bank_actual = bank;
        } else if (page->flags & CLEM_MMIO_PAGE_MAINAUX) {
            bank_actual = (bank & 0xfe) | (page->bank_write & 0x1);
        } else {
            bank_actual = page->bank_write;
        }
        bank_mem = _clem_get_memory_bank(clem, bank_actual, &mega2_access);
        if (page->flags & CLEM_MMIO_PAGE_WRITE_OK) {
            bank_mem[offset] = data;
        }
        if (shadow_map && shadow_map->pages[page->write]) {
            bank_mem = _clem_get_memory_bank(
                clem, (0xE0) | (bank_actual & 0x1), &mega2_access);
            if (page->flags & CLEM_MMIO_PAGE_WRITE_OK) {
                bank_mem[offset] = data;
            }
        }
        if (bank_actual == 0xe0 || bank_actual == 0xe1) {
            mega2_access = true;
        }
    } else {
        CLEM_ASSERT(false);
    }
    if (mem_flags != CLEM_MEM_FLAG_NULL) {
        clem->cpu.pins.adr = adr;
        clem->cpu.pins.bank = bank;
        clem->cpu.pins.data = data;
        clem->cpu.pins.vpaOut = false;
        clem->cpu.pins.vdaOut = (mem_flags & CLEM_MEM_FLAG_DATA) != 0;
        clem->cpu.pins.rwbOut = false;
        _clem_mem_cycle(clem, mega2_access);
    }
}
