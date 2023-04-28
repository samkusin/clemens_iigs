#include "clem_mmio.h"
#include "clem_debug.h"
#include "clem_drive.h"
#include "clem_mem.h"
#include "clem_mmio_defs.h"

#include "clem_device.h"
#include "clem_mmio_types.h"
#include "clem_types.h"
#include "clem_util.h"
#include "clem_vgc.h"

#include <string.h>

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

static void _clem_mmio_memory_map(ClemensMMIO *mmio, uint32_t memory_flags);
static void _clem_mmio_shadow_map(ClemensMMIO *mmio, uint32_t shadow_flags);

static void _clem_mmio_create_page_direct_mapping(struct ClemensMemoryPageInfo *page,
                                                  uint8_t page_idx) {
    page->read = page_idx;
    page->write = page_idx;
    page->flags = CLEM_MEM_PAGE_WRITEOK_FLAG | CLEM_MEM_PAGE_DIRECT_FLAG;
}

static void _clem_mmio_create_page_mainaux_mapping(struct ClemensMemoryPageInfo *page,
                                                   uint8_t page_idx, uint8_t bank_idx) {
    page->bank_read = bank_idx;
    page->bank_write = bank_idx;
    page->read = page_idx;
    page->write = page_idx;
    page->flags = CLEM_MEM_PAGE_WRITEOK_FLAG | CLEM_MEM_PAGE_MAINAUX_FLAG;
}

static void _clem_mmio_clear_irq(ClemensMMIO *mmio, unsigned irq_flags) {
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

static inline uint8_t _clem_mmio_newvideo_c029(ClemensMMIO *mmio) { return mmio->new_video_c029; }

static inline void _clem_mmio_newvideo_c029_set(ClemensMMIO *mmio, uint8_t value) {
    uint8_t setflags = mmio->new_video_c029 ^ value;
    if (setflags & CLEM_MMIO_NEWVIDEO_BANKLATCH_INHIBIT) {
        if (!(value & CLEM_MMIO_NEWVIDEO_BANKLATCH_INHIBIT)) {
            CLEM_UNIMPLEMENTED("ioreg %02X : %02X", CLEM_MMIO_REG_NEWVIDEO, value);
        }
        setflags ^= CLEM_MMIO_NEWVIDEO_BANKLATCH_INHIBIT;
    }
    if (setflags & CLEM_MMIO_NEWVIDEO_SUPERHIRES_ENABLE) {
        if (value & CLEM_MMIO_NEWVIDEO_SUPERHIRES_ENABLE) {
            clem_vgc_set_mode(&mmio->vgc, CLEM_VGC_SUPER_HIRES);
        } else {
            clem_vgc_clear_mode(&mmio->vgc, CLEM_VGC_SUPER_HIRES);
        }
        CLEM_LOG("clem_mem: c029 super hires = %u",
                 (value & CLEM_MMIO_NEWVIDEO_SUPERHIRES_ENABLE) != 0);
        setflags ^= CLEM_MMIO_NEWVIDEO_SUPERHIRES_ENABLE;
    }
    /* TODO: what happens if this is set with super hires turned off,
             this behvaior is assumed when in super-hires mode by
             implementation
    */
    if (setflags & CLEM_MMIO_NEWVIDEO_LINEARIZE_MEMORY) {
        CLEM_LOG("clem_mem: c029 linearize 0x2000-0x9fff bank 01 = %u",
                 (value & CLEM_MMIO_NEWVIDEO_LINEARIZE_MEMORY) != 0);
        setflags ^= CLEM_MMIO_NEWVIDEO_LINEARIZE_MEMORY;
    }
    CLEM_ASSERT(setflags == 0);
    mmio->new_video_c029 = value & ~0x1e; /* bits 1-4 are not used */
}

static void _clem_mmio_slotrom_select_c02d(ClemensMMIO *mmio, uint8_t data) {
    int i;
    unsigned slot_mask =
        CLEM_MEM_IO_MMAP_CROM & (~(CLEM_MEM_IO_MMAP_CXROM | CLEM_MEM_IO_MMAP_C3ROM));
    unsigned mmap_register = mmio->mmap_register & ~slot_mask;
    slot_mask = 0;
    for (i = 1; i < 8; ++i) {
        if (i == 3)
            continue;
        slot_mask = CLEM_MEM_IO_MMAP_C1ROM << (i - 1);
        if (data & (1 << i)) {
            mmap_register |= slot_mask;
        } else {
            mmap_register &= ~slot_mask;
        }
    }

    _clem_mmio_memory_map(mmio, mmap_register);
}

static uint8_t _clem_mmio_slotromsel_c02d(ClemensMMIO *mmio) {
    uint8_t mask = 0;
    for (int i = 1; i < 8; ++i) {
        if (i == 3)
            continue;
        if (mmio->mmap_register & (CLEM_MEM_IO_MMAP_C1ROM << (i - 1))) {
            mask |= (1 << i);
        } else {
            mask &= ~(1 << i);
        }
    }
    return mask;
}

static inline uint8_t _clem_mmio_shadow_c035(ClemensMMIO *mmio) {
    uint8_t result = 0;
    if (mmio->mmap_register & CLEM_MEM_IO_MMAP_NSHADOW_TXT1)
        result |= 0x01;
    if (mmio->mmap_register & CLEM_MEM_IO_MMAP_NSHADOW_HGR1)
        result |= 0x02;
    if (mmio->mmap_register & CLEM_MEM_IO_MMAP_NSHADOW_HGR2)
        result |= 0x04;
    if (mmio->mmap_register & CLEM_MEM_IO_MMAP_NSHADOW_SHGR)
        result |= 0x08;
    if (mmio->mmap_register & CLEM_MEM_IO_MMAP_NSHADOW_AUX)
        result |= 0x10;
    if (mmio->mmap_register & CLEM_MEM_IO_MMAP_NSHADOW_TXT2)
        result |= 0x20;
    if (mmio->mmap_register & CLEM_MEM_IO_MMAP_NIOLC)
        result |= 0x40;
    return result;
}

static void _clem_mmio_shadow_c035_set(ClemensMMIO *mmio, uint8_t value) {
    unsigned mmap = mmio->mmap_register;
    if (value & 0x01)
        mmap |= CLEM_MEM_IO_MMAP_NSHADOW_TXT1;
    else
        mmap &= ~CLEM_MEM_IO_MMAP_NSHADOW_TXT1;
    if (value & 0x02)
        mmap |= CLEM_MEM_IO_MMAP_NSHADOW_HGR1;
    else
        mmap &= ~CLEM_MEM_IO_MMAP_NSHADOW_HGR1;
    if (value & 0x04)
        mmap |= CLEM_MEM_IO_MMAP_NSHADOW_HGR2;
    else
        mmap &= ~CLEM_MEM_IO_MMAP_NSHADOW_HGR2;
    if (value & 0x08)
        mmap |= CLEM_MEM_IO_MMAP_NSHADOW_SHGR;
    else
        mmap &= ~CLEM_MEM_IO_MMAP_NSHADOW_SHGR;
    if (value & 0x10)
        mmap |= CLEM_MEM_IO_MMAP_NSHADOW_AUX;
    else
        mmap &= ~CLEM_MEM_IO_MMAP_NSHADOW_AUX;
    if (value & 0x20)
        mmap |= CLEM_MEM_IO_MMAP_NSHADOW_TXT2;
    else
        mmap &= ~CLEM_MEM_IO_MMAP_NSHADOW_TXT2;
    if (value & 0x40)
        mmap |= CLEM_MEM_IO_MMAP_NIOLC;
    else
        mmap &= ~CLEM_MEM_IO_MMAP_NIOLC;
    _clem_mmio_memory_map(mmio, mmap);
}

static void _clem_mmio_speed_c036_set(ClemensMMIO *mmio, struct ClemensTimeSpec *tspec,
                                      uint8_t value) {
    uint8_t setflags = mmio->speed_c036 ^ value;

    if (setflags & CLEM_MMIO_SPEED_FAST_ENABLED) {
        if (value & CLEM_MMIO_SPEED_FAST_ENABLED && !mmio->dev_iwm.disk_motor_on) {
            //    CLEM_LOG("C036: Fast Mode");
            tspec->clocks_step = tspec->clocks_step_fast;
        } else {
            //    CLEM_LOG("C036: Slow Mode");
            tspec->clocks_step = CLEM_CLOCKS_PHI0_CYCLE;
        }
    }
    if (setflags & CLEM_MMIO_SPEED_POWERED_ON) {
        if (value & CLEM_MMIO_SPEED_POWERED_ON) {
            CLEM_LOG("C036: Powered On SET");
        } else {
            CLEM_LOG("C036: Powered On CLEARED");
        }
    }
    /*
    if (setflags & CLEM_MMIO_SPEED_DISK_FLAGS) {
        CLEM_LOG("C036: Disk motor detect mask: %02X",
                 value & CLEM_MMIO_SPEED_DISK_FLAGS);
    }
    */

    /* bit 5 should always be 0 */
    /* for ROM 3, bit 6 can be on or off - for ROM 1, must be off */
    mmio->speed_c036 = (value & 0xdf);
}

static void _clem_mmio_mega2_inten_set(ClemensMMIO *mmio, uint8_t data) {
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

static uint8_t _clem_mmio_mega2_inten_get(ClemensMMIO *mmio) {
    uint8_t res = 0x00;
    if (mmio->dev_timer.flags & CLEM_MMIO_TIMER_QSEC_ENABLED) {
        res |= 0x10;
    }
    if (mmio->vgc.mode_flags & CLEM_VGC_ENABLE_VBL_IRQ) {
        res |= 0x08;
    }
    return res;
}

static uint8_t _clem_mmio_inttype_c046(ClemensMMIO *mmio) {
    uint8_t result = 0x0; // mmio->irq_line ? CLEM_MMIO_INTTYPE_IRQ : 0;

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

static void _clem_mmio_vgc_irq_c023_set(ClemensMMIO *mmio, uint8_t data) {
    if (data & 0x4) {
        mmio->dev_timer.flags |= CLEM_MMIO_TIMER_1SEC_ENABLED;
    } else {
        mmio->dev_timer.flags &= ~CLEM_MMIO_TIMER_1SEC_ENABLED;
        _clem_mmio_clear_irq(mmio, CLEM_IRQ_TIMER_RTC_1SEC);
    }
    if (data & 0x2) {
        clem_vgc_scanline_enable_int(&mmio->vgc, true);
    } else {
        clem_vgc_scanline_enable_int(&mmio->vgc, false);
    }
}

static uint8_t _clem_mmio_vgc_irq_c023_get(ClemensMMIO *mmio) {
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
    if (mmio->vgc.scanline_irq_enable) {
        res |= 0x02;
    }
    return res;
}

/* For why we don't follow the HW Ref, see important changes documented for
   STATEREG here:
   http://www.1000bit.it/support/manuali/apple/technotes/iigs/tn.iigs.030.html
*/
static inline uint8_t _clem_mmio_statereg_c068(ClemensMMIO *mmio) {
    uint8_t value = 0x00;
    if (mmio->mmap_register & CLEM_MEM_IO_MMAP_ALTZPLC) {
        value |= 0x80;
    }
    if (mmio->mmap_register & CLEM_MEM_IO_MMAP_TXTPAGE2) {
        value |= 0x40;
    }
    if (mmio->mmap_register & CLEM_MEM_IO_MMAP_RAMRD) {
        value |= 0x20;
    }
    if (mmio->mmap_register & CLEM_MEM_IO_MMAP_RAMWRT) {
        value |= 0x10;
    }
    if (!(mmio->mmap_register & CLEM_MEM_IO_MMAP_RDLCRAM)) {
        value |= 0x08;
    }
    if (mmio->mmap_register & CLEM_MEM_IO_MMAP_LCBANK2) {
        value |= 0x04;
    }
    if (!(mmio->mmap_register & CLEM_MEM_IO_MMAP_CXROM)) {
        value |= 0x01;
    }
    return value;
}

static uint8_t _clem_mmio_statereg_c068_set(ClemensMMIO *mmio, uint8_t value) {
    uint32_t mmap_register = mmio->mmap_register;
    /*  ALTZP  */
    if (value & 0x80) {
        mmap_register |= CLEM_MEM_IO_MMAP_ALTZPLC;
    } else {
        mmap_register &= ~CLEM_MEM_IO_MMAP_ALTZPLC;
    }
    /*  PAGE2 text - TODO when video options are fleshed out */
    if (value & 0x40) {
        mmap_register |= CLEM_MEM_IO_MMAP_TXTPAGE2;
    } else {
        mmap_register &= ~CLEM_MEM_IO_MMAP_TXTPAGE2;
    }
    /*  RAMRD */
    if (value & 0x20) {
        mmap_register |= CLEM_MEM_IO_MMAP_RAMRD;
    } else {
        mmap_register &= ~CLEM_MEM_IO_MMAP_RAMRD;
    }
    /*  RAMWRT */
    if (value & 0x10) {
        mmap_register |= CLEM_MEM_IO_MMAP_RAMWRT;
    } else {
        mmap_register &= ~CLEM_MEM_IO_MMAP_RAMWRT;
    }
    /*  RDROM */
    if (value & 0x08) {
        mmap_register &= ~CLEM_MEM_IO_MMAP_RDLCRAM;
    } else {
        mmap_register |= CLEM_MEM_IO_MMAP_RDLCRAM;
    }
    /* LCBNK2 */
    if (value & 0x04) {
        mmap_register |= CLEM_MEM_IO_MMAP_LCBANK2;
    } else {
        mmap_register &= ~CLEM_MEM_IO_MMAP_LCBANK2;
    }
    /* ROMBANK always 0 */
    if (value & 0x02) {
        /* do not set */
        CLEM_WARN("c068 %02X not allowed", value);
    } else {
        /* only valid value */
    }
    /* INTCXROM */
    if (value & 0x01) {
        mmap_register &= ~CLEM_MEM_IO_MMAP_CXROM;
    } else {
        mmap_register |= CLEM_MEM_IO_MMAP_CXROM;
    }

    _clem_mmio_memory_map(mmio, mmap_register);

    return 0;
}

static void _clem_mmio_rw_bank_select(ClemensMMIO *mmio, uint16_t address) {
    uint32_t memory_flags = mmio->mmap_register;
    uint16_t last_data_address = mmio->last_data_address & 0xffff;
    uint8_t ioreg = (address & 0xff);

    /* odd address access will enable ram writes first before their other ops
       which handles applications that perform single reads on the odd
       addressed softswitches after a prior write-enable double read switch.
       this seems to jive with the documentation on these softswitches, which
       assumes that the dual write is to perform */
    switch (ioreg) {
    case CLEM_MMIO_REG_LC2_RAM_WP:
    case CLEM_MMIO_REG_LC2_RAM_WP2:
        memory_flags |= (CLEM_MEM_IO_MMAP_RDLCRAM + CLEM_MEM_IO_MMAP_LCBANK2);
        if (last_data_address == address) {
            memory_flags &= ~CLEM_MEM_IO_MMAP_WRLCRAM;
        }
        break;
    case CLEM_MMIO_REG_LC2_ROM_WE:
    case CLEM_MMIO_REG_LC2_ROM_WE2:
        memory_flags |= CLEM_MEM_IO_MMAP_LCBANK2;
        memory_flags &= ~CLEM_MEM_IO_MMAP_RDLCRAM;
        if (last_data_address == address) {
            memory_flags |= CLEM_MEM_IO_MMAP_WRLCRAM;
        }
        break;
    case CLEM_MMIO_REG_LC2_ROM_WP:
    case CLEM_MMIO_REG_LC2_ROM_WP2:
        memory_flags &= ~(CLEM_MEM_IO_MMAP_RDLCRAM);
        memory_flags |= CLEM_MEM_IO_MMAP_LCBANK2;
        if (last_data_address == address) {
            memory_flags &= ~CLEM_MEM_IO_MMAP_WRLCRAM;
        }
        break;
    case CLEM_MMIO_REG_LC2_RAM_WE:
    case CLEM_MMIO_REG_LC2_RAM_WE2:
        memory_flags |= (CLEM_MEM_IO_MMAP_RDLCRAM + CLEM_MEM_IO_MMAP_LCBANK2);
        if (last_data_address == address) {
            memory_flags |= CLEM_MEM_IO_MMAP_WRLCRAM;
        }
        break;
    case CLEM_MMIO_REG_LC1_RAM_WP:
    case CLEM_MMIO_REG_LC1_RAM_WP2:
        memory_flags &= ~(CLEM_MEM_IO_MMAP_LCBANK2);
        memory_flags |= CLEM_MEM_IO_MMAP_RDLCRAM;
        if (last_data_address == address) {
            memory_flags &= ~CLEM_MEM_IO_MMAP_WRLCRAM;
        }
        break;
    case CLEM_MMIO_REG_LC1_ROM_WE:
    case CLEM_MMIO_REG_LC1_ROM_WE2:
        memory_flags &= ~(CLEM_MEM_IO_MMAP_RDLCRAM + CLEM_MEM_IO_MMAP_LCBANK2);
        if (last_data_address == address) {
            memory_flags |= CLEM_MEM_IO_MMAP_WRLCRAM;
        }
        break;
    case CLEM_MMIO_REG_LC1_ROM_WP:
    case CLEM_MMIO_REG_LC1_ROM_WP2:
        memory_flags &= ~(CLEM_MEM_IO_MMAP_LCBANK2 + CLEM_MEM_IO_MMAP_RDLCRAM);
        if (last_data_address == address) {
            memory_flags &= ~CLEM_MEM_IO_MMAP_WRLCRAM;
        }
        break;
    case CLEM_MMIO_REG_LC1_RAM_WE:
    case CLEM_MMIO_REG_LC1_RAM_WE2:
        memory_flags |= CLEM_MEM_IO_MMAP_RDLCRAM;
        memory_flags &= ~CLEM_MEM_IO_MMAP_LCBANK2;
        if (last_data_address == address) {
            memory_flags |= CLEM_MEM_IO_MMAP_WRLCRAM;
        }
        break;
    }
    if (memory_flags != mmio->mmap_register) {
        _clem_mmio_memory_map(mmio, memory_flags);
    }
}

static uint8_t _clem_mmio_card_io_read(ClemensCard *card, struct ClemensClock *clock, uint8_t addr,
                                       uint8_t flags) {
    uint8_t result = 0;
    if (card) {
        if (!(flags & CLEM_OP_IO_NO_OP)) {
            (*card->io_sync)(clock, card->context);
        }
        (*card->io_read)(clock, &result, addr, flags, card->context);
    }
    return result;
}

static uint8_t _clem_mmio_floating_bus(ClemensMMIO *mmio, struct ClemensTimeSpec *tspec) {
    //  The floating bus is basically data that is read from video memory given no other
    //  source (I/O, FPI RAM).   This occurs on the 2nd half of a 1mhz cycle in hardware.
    //  Here, the clem_mmio_read() function will selectively pick up this data if reading
    //  an I/O register that acts as a switch but doesn't return data.
    //
    //  Its unknown if Super-hires counts. (edit) The floating bus  emulation here
    //  works like it did on the Apple II
    //
    //  http://www.deater.net/weave/vmwprod/megademo/vapor_lock.html
    //
    //  TODO: The implementation here likely isn't accurate as pointed out on
    //        the Apple 2 slack channel for the Mega II (i.e. what happens with SHR mode
    //        as I believe that mode overrides any Apple II video mode switches.)
    //        A good test would be to try the vapor lock tests on a real GS and
    //        witness if they work.  That still doesn't answer the SHR mode
    //        question.

    struct ClemensClock clock;
    struct ClemensScanline *scanline;
    enum ClemensVideoFormat video_type;
    unsigned h_counter, v_counter;

    clock.ts = tspec->clocks_spent;
    clock.ref_step = CLEM_CLOCKS_PHI0_CYCLE;
    clem_vgc_calc_counters(&mmio->vgc, &clock, &v_counter, &h_counter);

    if (v_counter >= CLEM_VGC_HGR_SCANLINE_COUNT) {
        // bus has no values during a blank - no real way to illustrate this beyond
        // returning 0
        return 0;
    }
    if (h_counter < 25) {
        //  HBLANK no video data
        return 0;
    }
    h_counter -= 25; // point to start of visible data on line
    //  video_type will direct us to scanline type where:
    //      lores or text use text scanlines
    //      hires uses hires scanlines
    if (mmio->vgc.mode_flags & CLEM_VGC_HIRES) {
        video_type = kClemensVideoFormat_Hires;
        if (mmio->vgc.mode_flags & CLEM_VGC_MIXED_TEXT) {
            if (v_counter >= 160) {
                video_type = kClemensVideoFormat_Text;
            }
        }
    } else {
        video_type = kClemensVideoFormat_Text;
    }
    //  read from PAGE1, PAGE2?
    if (video_type == kClemensVideoFormat_Text) {
        // 80 column only supports page 1
        // TODO: re-read from main memory?  or point to aux if in 80-column mode?
        v_counter >>= 3;
        if ((mmio->mmap_register & CLEM_MEM_IO_MMAP_TXTPAGE2) &&
            !(mmio->mmap_register & CLEM_MEM_IO_MMAP_80COLSTORE)) {
            scanline = mmio->vgc.text_2_scanlines;
        } else {
            scanline = mmio->vgc.text_1_scanlines;
        }
    } else {
        if (mmio->mmap_register & CLEM_MEM_IO_MMAP_TXTPAGE2) {
            scanline = mmio->vgc.hgr_2_scanlines;
        } else {
            scanline = mmio->vgc.hgr_1_scanlines;
        }
    }
    scanline = &scanline[v_counter];
    return mmio->e0_bank[scanline->offset + h_counter];
}

uint8_t clem_mmio_read(ClemensMMIO *mmio, struct ClemensTimeSpec *tspec, uint16_t addr,
                       uint8_t flags, bool *mega2_access) {
    struct ClemensClock ref_clock;
    uint8_t result = 0x00;
    uint8_t ioreg = addr & 0xff;
    bool is_noop = (flags & CLEM_OP_IO_NO_OP) != 0;

    //  SHADOW, CYA, DMA are fast
    //  SLOT, STATE are fast (read-only)
    *mega2_access = true;

    ref_clock.ts = tspec->clocks_spent;
    ref_clock.ref_step = *mega2_access ? CLEM_CLOCKS_PHI0_CYCLE : tspec->clocks_step;

    if (flags & CLEM_OP_IO_CARD) {
        uint8_t slot_idx;

        if (addr == 0xCFFF) {
            /* TODO: CFFF access */
        } else if (addr < 0xCFFF && addr >= 0xC800) {
            slot_idx = (uint8_t)(mmio->card_expansion_rom_index & 0xff);
            if (slot_idx > 0 && slot_idx <= 7) {
                result = mmio->card_slot_expansion_memory[slot_idx - 1][addr - 0xc800];
            }
        } else if (addr >= 0xC100) {
            slot_idx = (uint8_t)(addr >> 8) - 0xc0 - 1;
            if (mmio->card_slot[slot_idx]) {
                result = _clem_mmio_card_io_read(mmio->card_slot[slot_idx], &ref_clock, ioreg,
                                                 flags | CLEM_OP_IO_DEVSEL);
            }
        }

        return result;
    }

    switch (ioreg) {
    case CLEM_MMIO_REG_KEYB_READ:
    case CLEM_MMIO_REG_KEYB_READ + 1:
    case CLEM_MMIO_REG_KEYB_READ + 2:
    case CLEM_MMIO_REG_KEYB_READ + 3:
    case CLEM_MMIO_REG_KEYB_READ + 4:
    case CLEM_MMIO_REG_KEYB_READ + 5:
    case CLEM_MMIO_REG_KEYB_READ + 6:
    case CLEM_MMIO_REG_KEYB_READ + 7:
    case CLEM_MMIO_REG_KEYB_READ + 8:
    case CLEM_MMIO_REG_KEYB_READ + 9:
    case CLEM_MMIO_REG_KEYB_READ + 10:
    case CLEM_MMIO_REG_KEYB_READ + 11:
    case CLEM_MMIO_REG_KEYB_READ + 12:
    case CLEM_MMIO_REG_KEYB_READ + 13:
    case CLEM_MMIO_REG_KEYB_READ + 14:
    case CLEM_MMIO_REG_KEYB_READ + 15:
    case CLEM_MMIO_REG_ANYKEY_STROBE:
        result = clem_adb_read_mega2_switch(&mmio->dev_adb, ioreg, flags);
        break;
    case CLEM_MMIO_REG_ADB_MOUSE_DATA:
    case CLEM_MMIO_REG_ADB_MODKEY:
    case CLEM_MMIO_REG_ADB_CMD_DATA:
    case CLEM_MMIO_REG_ADB_STATUS:
        result = clem_adb_read_switch(&mmio->dev_adb, ioreg, flags);
        break;
    case CLEM_MMIO_REG_LC_BANK_TEST:
        result = (mmio->mmap_register & CLEM_MEM_IO_MMAP_LCBANK2) ? 0x80 : 0x00;
        break;
    case CLEM_MMIO_REG_ROM_RAM_TEST:
        result = (mmio->mmap_register & CLEM_MEM_IO_MMAP_RDLCRAM) ? 0x80 : 0x00;
        break;
    case CLEM_MMIO_REG_RAMRD_TEST:
        result = (mmio->mmap_register & CLEM_MEM_IO_MMAP_RAMRD) ? 0x80 : 0x00;
        break;
    case CLEM_MMIO_REG_RAMWRT_TEST:
        result = (mmio->mmap_register & CLEM_MEM_IO_MMAP_RAMWRT) ? 0x80 : 0x00;
        break;
    case CLEM_MMIO_REG_READCXROM:
        result = !(mmio->mmap_register & CLEM_MEM_IO_MMAP_CXROM) ? 0x80 : 0x00;
        break;
    case CLEM_MMIO_REG_RDALTZP_TEST:
        result = (mmio->mmap_register & CLEM_MEM_IO_MMAP_ALTZPLC) ? 0x80 : 0x00;
        break;
    case CLEM_MMIO_REG_READC3ROM:
        result = (mmio->mmap_register & CLEM_MEM_IO_MMAP_C3ROM) ? 0x80 : 0x00;
        break;
    case CLEM_MMIO_REG_80COLSTORE_TEST:
        result = (mmio->mmap_register & CLEM_MEM_IO_MMAP_80COLSTORE) ? 0x80 : 0x00;
        break;
    case CLEM_MMIO_REG_VBLBAR:
    case CLEM_MMIO_REG_VGC_VERTCNT:
    case CLEM_MMIO_REG_VGC_HORIZCNT:
        result = clem_vgc_read_switch(&mmio->vgc, &ref_clock, ioreg, flags);
        break;
    case CLEM_MMIO_REG_TXT_TEST:
        result = (mmio->vgc.mode_flags & CLEM_VGC_GRAPHICS_MODE) ? 0x00 : 0x80;
        break;
    case CLEM_MMIO_REG_MIXED_TEST:
        result = (mmio->vgc.mode_flags & CLEM_VGC_MIXED_TEXT) ? 0x80 : 0x00;
        break;
    case CLEM_MMIO_REG_TXTPAGE2_TEST:
        result = (mmio->mmap_register & CLEM_MEM_IO_MMAP_TXTPAGE2) ? 0x80 : 0x00;
        break;
    case CLEM_MMIO_REG_ALTCHARSET_TEST:
        result = (mmio->vgc.mode_flags & CLEM_VGC_ALTCHARSET) ? 0x80 : 0x00;
        break;
    case CLEM_MMIO_REG_HIRES_TEST:
        result = (mmio->vgc.mode_flags & CLEM_VGC_HIRES) ? 0x80 : 0x00;
        break;
    case CLEM_MMIO_REG_80COLUMN_TEST:
        result = (mmio->vgc.mode_flags & CLEM_VGC_80COLUMN_TEXT) ? 0x80 : 0x00;
        break;
    case CLEM_MMIO_REG_CASSETTE_PORT_NOP:
        result = _clem_mmio_floating_bus(mmio, tspec);
        break;
    case CLEM_MMIO_REG_VGC_TEXT_COLOR:
        result = (uint8_t)((mmio->vgc.text_fg_color << 4) | mmio->vgc.text_bg_color);
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
        *mega2_access = false;
        break;
    case CLEM_MMIO_REG_SPKR:
        clem_sound_read_switch(&mmio->dev_audio, ioreg, flags);
        result = _clem_mmio_floating_bus(mmio, tspec);
        break;
    case CLEM_MMIO_REG_DISK_INTERFACE:
        result = clem_iwm_read_switch(&mmio->dev_iwm, &mmio->active_drives, tspec, ioreg, flags);
        break;
    case CLEM_MMIO_REG_RTC_VGC_SCANINT:
        result = clem_vgc_read_switch(&mmio->vgc, &ref_clock, ioreg, flags);
        break;
    case CLEM_MMIO_REG_SHADOW:
        result = _clem_mmio_shadow_c035(mmio);
        *mega2_access = false;
        break;
    case CLEM_MMIO_REG_SPEED:
        result = mmio->speed_c036;
        *mega2_access = false;
        break;
    // TODO: DMA?  fast access as well
    case CLEM_MMIO_REG_RTC_CTL:
        if (!is_noop) {
            clem_rtc_command(&mmio->dev_rtc, tspec->clocks_spent, CLEM_IO_READ);
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
    case CLEM_MMIO_REG_MEGA2_MOUSE_DX:
    case CLEM_MMIO_REG_MEGA2_MOUSE_DY:
        result = clem_adb_read_mega2_switch(&mmio->dev_adb, ioreg, flags);
        break;
    case CLEM_MMIO_REG_DIAG_INTTYPE:
        result = _clem_mmio_inttype_c046(mmio);
        break;
    case CLEM_MMIO_REG_CLRVBLINT:
        if (!(flags & CLEM_OP_IO_NO_OP)) {
            _clem_mmio_clear_irq(mmio, CLEM_IRQ_TIMER_QSEC | CLEM_IRQ_VGC_BLANK);
        }
        break;
    case CLEM_MMIO_REG_EMULATOR:
        if (mmio->emulator_detect == CLEM_MMIO_EMULATOR_DETECT_START) {
            result = CLEM_EMULATOR_ID;
            mmio->emulator_detect = CLEM_MMIO_EMULATOR_DETECT_VERSION;
        } else if (mmio->emulator_detect == CLEM_MMIO_EMULATOR_DETECT_VERSION) {
            result = CLEM_EMULATOR_VER;
            mmio->emulator_detect = CLEM_MMIO_EMULATOR_DETECT_IDLE;
        }
        break;
    case CLEM_MMIO_REG_TXTCLR:
        if (!(flags & CLEM_OP_IO_NO_OP)) {
            clem_vgc_set_mode(&mmio->vgc, CLEM_VGC_GRAPHICS_MODE);
        }
        result = _clem_mmio_floating_bus(mmio, tspec);
        break;
    case CLEM_MMIO_REG_TXTSET:
        if (!(flags & CLEM_OP_IO_NO_OP)) {
            clem_vgc_clear_mode(&mmio->vgc, CLEM_VGC_GRAPHICS_MODE);
        }
        result = _clem_mmio_floating_bus(mmio, tspec);
        break;
    case CLEM_MMIO_REG_MIXCLR:
        if (!(flags & CLEM_OP_IO_NO_OP)) {
            clem_vgc_clear_mode(&mmio->vgc, CLEM_VGC_MIXED_TEXT);
        }
        result = _clem_mmio_floating_bus(mmio, tspec);
        break;
    case CLEM_MMIO_REG_MIXSET:
        if (!(flags & CLEM_OP_IO_NO_OP)) {
            clem_vgc_set_mode(&mmio->vgc, CLEM_VGC_MIXED_TEXT);
        }
        result = _clem_mmio_floating_bus(mmio, tspec);
        break;
    case CLEM_MMIO_REG_TXTPAGE1:
        if (!(flags & CLEM_OP_IO_NO_OP)) {
            _clem_mmio_memory_map(mmio, mmio->mmap_register & ~CLEM_MEM_IO_MMAP_TXTPAGE2);
        }
        result = _clem_mmio_floating_bus(mmio, tspec);
        break;
    case CLEM_MMIO_REG_TXTPAGE2:
        if (!(flags & CLEM_OP_IO_NO_OP)) {
            _clem_mmio_memory_map(mmio, mmio->mmap_register | CLEM_MEM_IO_MMAP_TXTPAGE2);
        }
        result = _clem_mmio_floating_bus(mmio, tspec);
        break;
    case CLEM_MMIO_REG_LORES:
        /* implicitly clears hires */
        if (!(flags & CLEM_OP_IO_NO_OP)) {
            clem_vgc_set_mode(&mmio->vgc, CLEM_VGC_LORES);
        }
        result = _clem_mmio_floating_bus(mmio, tspec);
        break;
    case CLEM_MMIO_REG_HIRES:
        /* implicitly clears lores */
        if (!(flags & CLEM_OP_IO_NO_OP)) {
            clem_vgc_set_mode(&mmio->vgc, CLEM_VGC_HIRES);
        }
        result = _clem_mmio_floating_bus(mmio, tspec);
        break;
    case CLEM_MMIO_REG_AN0_OFF:
    case CLEM_MMIO_REG_AN0_ON:
    case CLEM_MMIO_REG_AN1_OFF:
    case CLEM_MMIO_REG_AN1_ON:
    case CLEM_MMIO_REG_AN2_OFF:
    case CLEM_MMIO_REG_AN2_ON:
    case CLEM_MMIO_REG_SW0:
    case CLEM_MMIO_REG_SW1:
        result = clem_adb_read_switch(&mmio->dev_adb, ioreg, flags);
        break;
    case CLEM_MMIO_REG_AN3_OFF:
    case CLEM_MMIO_REG_AN3_ON:
        /* AN3 used for double hires graphics */
        if (!is_noop) {
            if (ioreg == CLEM_MMIO_REG_AN3_ON) {
                clem_vgc_clear_mode(&mmio->vgc, CLEM_VGC_DISABLE_AN3);
            } else {
                clem_vgc_set_mode(&mmio->vgc, CLEM_VGC_DISABLE_AN3);
            }
        }
        result = clem_adb_read_switch(&mmio->dev_adb, ioreg, flags);
        break;
    case CLEM_MMIO_REG_PADDL0:
    case CLEM_MMIO_REG_PADDL1:
    case CLEM_MMIO_REG_PADDL2:
    case CLEM_MMIO_REG_PADDL3:
    case CLEM_MMIO_REG_PTRIG:
        /* note c071 - 7f are reserved for ROM access - used for the
           BRK interrupt */
        result = clem_adb_read_switch(&mmio->dev_adb, ioreg, flags);
        break;
    case CLEM_MMIO_REG_STATEREG:
        result = _clem_mmio_statereg_c068(mmio);
        *mega2_access = false;
        break;
    case CLEM_MMIO_REG_LC2_RAM_WP:
    case CLEM_MMIO_REG_LC2_RAM_WP2:
    case CLEM_MMIO_REG_LC2_ROM_WE:
    case CLEM_MMIO_REG_LC2_ROM_WE2:
    case CLEM_MMIO_REG_LC2_ROM_WP:
    case CLEM_MMIO_REG_LC2_ROM_WP2:
    case CLEM_MMIO_REG_LC2_RAM_WE:
    case CLEM_MMIO_REG_LC2_RAM_WE2:
    case CLEM_MMIO_REG_LC1_RAM_WP:
    case CLEM_MMIO_REG_LC1_RAM_WP2:
    case CLEM_MMIO_REG_LC1_ROM_WE:
    case CLEM_MMIO_REG_LC1_ROM_WE2:
    case CLEM_MMIO_REG_LC1_ROM_WP:
    case CLEM_MMIO_REG_LC1_ROM_WP2:
    case CLEM_MMIO_REG_LC1_RAM_WE:
    case CLEM_MMIO_REG_LC1_RAM_WE2:
        if (!(flags & CLEM_OP_IO_NO_OP)) {
            _clem_mmio_rw_bank_select(mmio, addr);
        }
        result = _clem_mmio_floating_bus(mmio, tspec);
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
        result = clem_iwm_read_switch(&mmio->dev_iwm, &mmio->active_drives, tspec, ioreg, flags);
        break;
    default:
        if (ioreg >= 0x90) {
            result = _clem_mmio_card_io_read(mmio->card_slot[(ioreg - 0x90) >> 4], &ref_clock,
                                             ioreg & 0xf, flags);
        } else if (!is_noop) {
            clem_debug_break(mmio->dev_debug, CLEM_DEBUG_BREAK_UNIMPL_IOREAD, addr, 0x0000);
        }
        break;
    }

    return result;
}

static void _clem_mmio_card_io_write(ClemensCard *card, struct ClemensClock *clock, uint8_t data,
                                     uint8_t addr, uint8_t flags) {
    if (card) {
        if (!(flags & CLEM_OP_IO_NO_OP)) {
            (*card->io_sync)(clock, card->context);
        }
        (*card->io_write)(clock, data, addr, flags, card->context);
    }
}

void clem_mmio_write(ClemensMMIO *mmio, struct ClemensTimeSpec *tspec, uint8_t data, uint16_t addr,
                     uint8_t flags, bool *mega2_access) {
    struct ClemensClock ref_clock;
    bool is_noop = (flags & CLEM_OP_IO_NO_OP) != 0;
    uint8_t ioreg = (addr & 0xff);

    //  SHADOW, SPEED and DMA are fast
    *mega2_access = true;

    ref_clock.ts = tspec->clocks_spent;
    ref_clock.ref_step = *mega2_access ? CLEM_CLOCKS_PHI0_CYCLE : tspec->clocks_step;

    if ((flags & CLEM_OP_IO_CARD) && addr >= 0xC100) {
        uint8_t slot_idx = (uint8_t)(addr >> 8) - 0xc0 - 1;
        if (mmio->card_slot[slot_idx]) {
            _clem_mmio_card_io_write(mmio->card_slot[slot_idx], &ref_clock, data, ioreg,
                                     flags | CLEM_OP_IO_DEVSEL);
        }
        return;
    }

    switch (ioreg) {
    case CLEM_MMIO_REG_80STOREOFF_WRITE:
        _clem_mmio_memory_map(mmio, mmio->mmap_register & ~CLEM_MEM_IO_MMAP_80COLSTORE);
        break;
    case CLEM_MMIO_REG_80STOREON_WRITE:
        _clem_mmio_memory_map(mmio, mmio->mmap_register | CLEM_MEM_IO_MMAP_80COLSTORE);
        break;
    case CLEM_MMIO_REG_RDMAINRAM:
        _clem_mmio_memory_map(mmio, mmio->mmap_register & ~CLEM_MEM_IO_MMAP_RAMRD);
        break;
    case CLEM_MMIO_REG_RDCARDRAM:
        _clem_mmio_memory_map(mmio, mmio->mmap_register | CLEM_MEM_IO_MMAP_RAMRD);
        break;
    case CLEM_MMIO_REG_WRMAINRAM:
        _clem_mmio_memory_map(mmio, mmio->mmap_register & ~CLEM_MEM_IO_MMAP_RAMWRT);
        break;
    case CLEM_MMIO_REG_WRCARDRAM:
        _clem_mmio_memory_map(mmio, mmio->mmap_register | CLEM_MEM_IO_MMAP_RAMWRT);
        break;
    case CLEM_MMIO_REG_SLOTCXROM:
        _clem_mmio_memory_map(mmio, mmio->mmap_register | CLEM_MEM_IO_MMAP_CXROM);
        break;
    case CLEM_MMIO_REG_INTCXROM:
        _clem_mmio_memory_map(mmio, mmio->mmap_register & ~CLEM_MEM_IO_MMAP_CXROM);
        break;
    case CLEM_MMIO_REG_STDZP:
        _clem_mmio_memory_map(mmio, mmio->mmap_register & ~CLEM_MEM_IO_MMAP_ALTZPLC);
        break;
    case CLEM_MMIO_REG_ALTZP:
        _clem_mmio_memory_map(mmio, mmio->mmap_register | CLEM_MEM_IO_MMAP_ALTZPLC);
        break;
    case CLEM_MMIO_REG_SLOTC3ROM:
        _clem_mmio_memory_map(mmio, mmio->mmap_register | CLEM_MEM_IO_MMAP_C3ROM);
        break;
    case CLEM_MMIO_REG_INTC3ROM:
        _clem_mmio_memory_map(mmio, mmio->mmap_register & ~CLEM_MEM_IO_MMAP_C3ROM);
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
        clem_iwm_write_switch(&mmio->dev_iwm, &mmio->active_drives, tspec, ioreg, data);
        break;
    case CLEM_MMIO_REG_RTC_VGC_SCANINT:
        if (!(data & 0x40)) {
            _clem_mmio_clear_irq(mmio, CLEM_IRQ_TIMER_RTC_1SEC);
        }
        clem_vgc_write_switch(&mmio->vgc, &ref_clock, ioreg, data & ~0x40);
        break;
    case CLEM_MMIO_REG_RTC_CTL:
        mmio->dev_rtc.ctl_c034 = data;
        clem_rtc_command(&mmio->dev_rtc, tspec->clocks_spent, CLEM_IO_WRITE);
        break;
    case CLEM_MMIO_REG_RTC_DATA:
        mmio->dev_rtc.data_c033 = data;
        break;
    case CLEM_MMIO_REG_SHADOW:
        _clem_mmio_shadow_c035_set(mmio, data);
        *mega2_access = false;
        break;
    case CLEM_MMIO_REG_SPEED:
        _clem_mmio_speed_c036_set(mmio, tspec, data);
        *mega2_access = false;
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
    case CLEM_MMIO_REG_EMULATOR:
        mmio->emulator_detect = CLEM_MMIO_EMULATOR_DETECT_START;
        break;
        ;
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
        _clem_mmio_memory_map(mmio, mmio->mmap_register & ~CLEM_MEM_IO_MMAP_TXTPAGE2);
        break;
    case CLEM_MMIO_REG_TXTPAGE2:
        _clem_mmio_memory_map(mmio, mmio->mmap_register | CLEM_MEM_IO_MMAP_TXTPAGE2);
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
    case CLEM_MMIO_REG_PADDL0:
    case CLEM_MMIO_REG_PADDL1:
    case CLEM_MMIO_REG_PADDL2:
    case CLEM_MMIO_REG_PADDL3:
    case CLEM_MMIO_REG_PTRIG:
    case CLEM_MMIO_REG_PTRIG + 0x1:
    case CLEM_MMIO_REG_PTRIG + 0x2:
    case CLEM_MMIO_REG_PTRIG + 0x3:
    case CLEM_MMIO_REG_C074_TRANSWARP:
    case CLEM_MMIO_REG_PTRIG + 0x5:
    case CLEM_MMIO_REG_PTRIG + 0x6:
    case CLEM_MMIO_REG_PTRIG + 0x7:
    case CLEM_MMIO_REG_PTRIG + 0x8:
    case CLEM_MMIO_REG_PTRIG + 0x9:
    case CLEM_MMIO_REG_PTRIG + 0xa:
    case CLEM_MMIO_REG_PTRIG + 0xb:
    case CLEM_MMIO_REG_PTRIG + 0xc:
    case CLEM_MMIO_REG_PTRIG + 0xd:
    case CLEM_MMIO_REG_PTRIG + 0xe:
    case CLEM_MMIO_REG_PTRIG + 0xf:
        clem_adb_write_switch(&mmio->dev_adb, ioreg, data);
        break;
    case CLEM_MMIO_REG_AN3_OFF:
    case CLEM_MMIO_REG_AN3_ON:
        if (ioreg == CLEM_MMIO_REG_AN3_ON) {
            clem_vgc_clear_mode(&mmio->vgc, CLEM_VGC_DISABLE_AN3);
        } else {
            clem_vgc_set_mode(&mmio->vgc, CLEM_VGC_DISABLE_AN3);
        }
        clem_adb_write_switch(&mmio->dev_adb, ioreg, data);
        break;
    case CLEM_MMIO_REG_LC2_RAM_WP:
    case CLEM_MMIO_REG_LC2_RAM_WP2:
    case CLEM_MMIO_REG_LC2_ROM_WE:
    case CLEM_MMIO_REG_LC2_ROM_WE2:
    case CLEM_MMIO_REG_LC2_ROM_WP:
    case CLEM_MMIO_REG_LC2_ROM_WP2:
    case CLEM_MMIO_REG_LC2_RAM_WE:
    case CLEM_MMIO_REG_LC2_RAM_WE2:
    case CLEM_MMIO_REG_LC1_RAM_WP:
    case CLEM_MMIO_REG_LC1_RAM_WP2:
    case CLEM_MMIO_REG_LC1_ROM_WE:
    case CLEM_MMIO_REG_LC1_ROM_WE2:
    case CLEM_MMIO_REG_LC1_ROM_WP:
    case CLEM_MMIO_REG_LC1_ROM_WP2:
    case CLEM_MMIO_REG_LC1_RAM_WE:
    case CLEM_MMIO_REG_LC1_RAM_WE2:
        _clem_mmio_rw_bank_select(mmio, addr);
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
        clem_iwm_write_switch(&mmio->dev_iwm, &mmio->active_drives, tspec, ioreg, data);
        break;
    default:
        if (ioreg >= 0x80) {
            _clem_mmio_card_io_write(mmio->card_slot[(ioreg - 0x90) >> 4], &ref_clock, data,
                                     ioreg & 0xf, flags);
        }
        if (!is_noop) {
            clem_debug_break(mmio->dev_debug, CLEM_DEBUG_BREAK_UNIMPL_IOWRITE, addr, data);
        }
        break;
    }
}

static void _clem_mmio_shadow_map(ClemensMMIO *mmio, uint32_t shadow_flags) {
    /* Sets up which pages are shadowed on banks 00, 01.  Flags tested inside
       _clem_write determine if the write operation actual performs the copy to
       E0, E1
    */
    unsigned remap_flags = mmio->mmap_register ^ shadow_flags;
    unsigned page_idx;
    bool inhibit_hgr_bank_01 = (shadow_flags & CLEM_MEM_IO_MMAP_NSHADOW_AUX) != 0;
    bool inhibit_shgr_bank_01 = (shadow_flags & CLEM_MEM_IO_MMAP_NSHADOW_SHGR) != 0;

    //  TXT 1
    if (remap_flags & CLEM_MEM_IO_MMAP_NSHADOW_TXT1) {
        for (page_idx = 0x04; page_idx < 0x08; ++page_idx) {
            uint8_t v = (shadow_flags & CLEM_MEM_IO_MMAP_NSHADOW_TXT1) ? 0 : 1;
            mmio->fpi_mega2_main_shadow_map.pages[page_idx] = v;
            mmio->fpi_mega2_aux_shadow_map.pages[page_idx] = v;
        }
    }
    //  TXT 2
    if (remap_flags & CLEM_MEM_IO_MMAP_NSHADOW_TXT2) {
        for (page_idx = 0x08; page_idx < 0x0C; ++page_idx) {
            uint8_t v = (shadow_flags & CLEM_MEM_IO_MMAP_NSHADOW_TXT2) ? 0 : 1;
            mmio->fpi_mega2_main_shadow_map.pages[page_idx] = v;
            mmio->fpi_mega2_aux_shadow_map.pages[page_idx] = v;
        }
    }
    //  HGR1
    if ((remap_flags & CLEM_MEM_IO_MMAP_NSHADOW_HGR1) ||
        (remap_flags & CLEM_MEM_IO_MMAP_NSHADOW_AUX) ||
        (remap_flags & CLEM_MEM_IO_MMAP_NSHADOW_SHGR)) {
        for (page_idx = 0x20; page_idx < 0x40; ++page_idx) {
            uint8_t v0 = (shadow_flags & CLEM_MEM_IO_MMAP_NSHADOW_HGR1) ? 0 : 1;
            uint8_t v1 = (v0 && !inhibit_hgr_bank_01) ? 1 : 0;
            if (!inhibit_shgr_bank_01 && !v1)
                v1 = 1;
            mmio->fpi_mega2_main_shadow_map.pages[page_idx] = v0;
            mmio->fpi_mega2_aux_shadow_map.pages[page_idx] = v1;
        }
    }
    if ((remap_flags & CLEM_MEM_IO_MMAP_NSHADOW_HGR2) ||
        (remap_flags & CLEM_MEM_IO_MMAP_NSHADOW_AUX) ||
        (remap_flags & CLEM_MEM_IO_MMAP_NSHADOW_SHGR)) {
        for (page_idx = 0x40; page_idx < 0x60; ++page_idx) {
            uint8_t v0 = (shadow_flags & CLEM_MEM_IO_MMAP_NSHADOW_HGR2) ? 0 : 1;
            uint8_t v1 = (v0 && !inhibit_hgr_bank_01) ? 1 : 0;
            if (!inhibit_shgr_bank_01 && !v1)
                v1 = 1;
            mmio->fpi_mega2_main_shadow_map.pages[page_idx] = v0;
            mmio->fpi_mega2_aux_shadow_map.pages[page_idx] = v1;
        }
    }
    if (remap_flags & CLEM_MEM_IO_MMAP_NSHADOW_SHGR) {
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
static void _clem_mmio_memory_map(ClemensMMIO *mmio, uint32_t memory_flags) {
    struct ClemensMemoryPageMap *page_map_B00;
    struct ClemensMemoryPageMap *page_map_B01;
    struct ClemensMemoryPageMap *page_map_BE0;
    struct ClemensMemoryPageMap *page_map_BE1;
    struct ClemensMemoryPageInfo *page_B00;
    struct ClemensMemoryPageInfo *page_B01;
    struct ClemensMemoryPageInfo *page_BE0;
    struct ClemensMemoryPageInfo *page_BE1;
    unsigned remap_flags = mmio->mmap_register ^ memory_flags;
    unsigned page_idx;

    page_map_B00 = &mmio->fpi_main_page_map;
    page_map_B01 = &mmio->fpi_aux_page_map;
    page_map_BE0 = &mmio->mega2_main_page_map;
    page_map_BE1 = &mmio->mega2_aux_page_map;

    //  ALTZPLC is a main bank-only softswitch.  As a result 01, E0, E1 bank
    //      maps for page 0, 1 remain unchanged
    if (remap_flags & CLEM_MEM_IO_MMAP_ALTZPLC) {
        //  TODO: do LC mappings also change?  //e docs state that soft switches
        //        should be explicitly set again when switching banks, but
        //        looking at other emulators (yes... this is driving me crazy
        //        imply otherwise.  when testing with real software, determine
        //        which requirement is true.)
        remap_flags |= CLEM_MEM_IO_MMAP_LC;
        for (page_idx = 0x00; page_idx < 0x02; ++page_idx) {
            page_B00 = &page_map_B00->pages[page_idx];
            if (memory_flags & CLEM_MEM_IO_MMAP_ALTZPLC) {
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
    if (remap_flags & CLEM_MEM_IO_MMAP_OLDVIDEO) {
        if (memory_flags & CLEM_MEM_IO_MMAP_80COLSTORE) {
            for (page_idx = 0x04; page_idx < 0x08; ++page_idx) {
                page_B00 = &page_map_B00->pages[page_idx];
                page_B00->bank_read = ((memory_flags & CLEM_MEM_IO_MMAP_TXTPAGE2) ? 0x01 : 00);
                page_B00->bank_write = ((memory_flags & CLEM_MEM_IO_MMAP_TXTPAGE2) ? 0x01 : 00);
            }
            for (page_idx = 0x20; page_idx < 0x40; ++page_idx) {
                page_B00 = &page_map_B00->pages[page_idx];
                if (memory_flags & CLEM_MEM_IO_MMAP_HIRES) {
                    page_B00->bank_read = ((memory_flags & CLEM_MEM_IO_MMAP_TXTPAGE2) ? 0x01 : 00);
                    page_B00->bank_write = ((memory_flags & CLEM_MEM_IO_MMAP_TXTPAGE2) ? 0x01 : 00);
                } else {
                    page_B00->bank_read = ((memory_flags & CLEM_MEM_IO_MMAP_RAMRD) ? 0x01 : 00);
                    page_B00->bank_write = ((memory_flags & CLEM_MEM_IO_MMAP_RAMWRT) ? 0x01 : 00);
                }
            }
        } else {
            for (page_idx = 0x04; page_idx < 0x08; ++page_idx) {
                page_B00 = &page_map_B00->pages[page_idx];
                page_B00->bank_read = ((memory_flags & CLEM_MEM_IO_MMAP_RAMRD) ? 0x01 : 00);
                page_B00->bank_write = ((memory_flags & CLEM_MEM_IO_MMAP_RAMWRT) ? 0x01 : 00);
            }
            for (page_idx = 0x20; page_idx < 0x40; ++page_idx) {
                page_B00 = &page_map_B00->pages[page_idx];
                page_B00->bank_read = ((memory_flags & CLEM_MEM_IO_MMAP_RAMRD) ? 0x01 : 00);
                page_B00->bank_write = ((memory_flags & CLEM_MEM_IO_MMAP_RAMWRT) ? 0x01 : 00);
            }
        }
    }

    //  RAMRD/RAMWRT minus the page 1 Apple //e video regions
    if (remap_flags & (CLEM_MEM_IO_MMAP_RAMRD + CLEM_MEM_IO_MMAP_RAMWRT)) {
        remap_flags |= CLEM_MEM_IO_MMAP_NSHADOW;
        for (page_idx = 0x02; page_idx < 0x04; ++page_idx) {
            page_B00 = &page_map_B00->pages[page_idx];
            page_B00->bank_read = ((memory_flags & CLEM_MEM_IO_MMAP_RAMRD) ? 0x01 : 00);
            page_B00->bank_write = ((memory_flags & CLEM_MEM_IO_MMAP_RAMWRT) ? 0x01 : 00);
        }
        for (page_idx = 0x08; page_idx < 0x20; ++page_idx) {
            page_B00 = &page_map_B00->pages[page_idx];
            page_B00->bank_read = ((memory_flags & CLEM_MEM_IO_MMAP_RAMRD) ? 0x01 : 00);
            page_B00->bank_write = ((memory_flags & CLEM_MEM_IO_MMAP_RAMWRT) ? 0x01 : 00);
        }
        for (page_idx = 0x40; page_idx < 0xC0; ++page_idx) {
            page_B00 = &page_map_B00->pages[page_idx];
            page_B00->bank_read = ((memory_flags & CLEM_MEM_IO_MMAP_RAMRD) ? 0x01 : 00);
            page_B00->bank_write = ((memory_flags & CLEM_MEM_IO_MMAP_RAMWRT) ? 0x01 : 00);
        }
    }

    //  Shadowing
    if (remap_flags & CLEM_MEM_IO_MMAP_NSHADOW) {
        _clem_mmio_shadow_map(mmio, memory_flags & CLEM_MEM_IO_MMAP_NSHADOW);
    }

    //  I/O space mapping
    //  IOLC switch changed, which requires remapping the entire language card
    //  region + the I/O region (for FPI memory - Mega2 doesn't deal with
    //  shadowing or LC ROM mapping)
    if (remap_flags & (CLEM_MEM_IO_MMAP_NIOLC + CLEM_MEM_IO_MMAP_CROM)) {
        if (remap_flags & CLEM_MEM_IO_MMAP_NIOLC) {
            remap_flags |= CLEM_MEM_IO_MMAP_LC;
        }

        page_B00 = &page_map_B00->pages[0xC0];
        page_B01 = &page_map_B01->pages[0xC0];
        _clem_mmio_create_page_mainaux_mapping(page_B00, 0xC0, 0x00);
        _clem_mmio_create_page_mainaux_mapping(page_B01, 0xC0, 0x01);

        if (memory_flags & CLEM_MEM_IO_MMAP_NIOLC) {
            page_B00->flags &= ~CLEM_MEM_PAGE_IOADDR_FLAG;
            page_B01->flags &= ~CLEM_MEM_PAGE_IOADDR_FLAG;
            for (page_idx = 0xC1; page_idx < 0xD0; ++page_idx) {
                page_B00 = &page_map_B00->pages[page_idx];
                _clem_mmio_create_page_mainaux_mapping(page_B00, page_idx, 0x00);
                page_B00->flags |= CLEM_MEM_PAGE_WRITEOK_FLAG;
                page_B01 = &page_map_B01->pages[page_idx];
                _clem_mmio_create_page_mainaux_mapping(page_B01, page_idx, 0x01);
                page_B01->flags |= CLEM_MEM_PAGE_WRITEOK_FLAG;
            }
        } else {
            page_B00->flags |= CLEM_MEM_PAGE_IOADDR_FLAG;
            page_B01->flags |= CLEM_MEM_PAGE_IOADDR_FLAG;
            for (page_idx = 0xC1; page_idx < 0xC8; ++page_idx) {
                unsigned slot_idx = ((page_idx - 1) & 0xf);
                bool intcx_page;
                if (page_idx == 0xC3) {
                    intcx_page = !(memory_flags & CLEM_MEM_IO_MMAP_C3ROM);
                } else {
                    intcx_page = !(memory_flags & CLEM_MEM_IO_MMAP_CXROM) ||
                                 !(memory_flags & (CLEM_MEM_IO_MMAP_C1ROM << slot_idx));
                }

                page_B00 = &page_map_B00->pages[page_idx];
                page_B01 = &page_map_B01->pages[page_idx];
                if (intcx_page) {
                    clem_mem_create_page_mapping(page_B00, page_idx, 0xff, 0x00);
                    clem_mem_create_page_mapping(page_B01, page_idx, 0xff, 0x01);
                    page_B00->flags &= ~CLEM_MEM_PAGE_CARDMEM_FLAG;
                    page_B01->flags &= ~CLEM_MEM_PAGE_CARDMEM_FLAG;
                } else {
                    clem_mem_create_page_mapping(page_B00, page_idx, 0x00, 0x00);
                    clem_mem_create_page_mapping(page_B01, page_idx, 0x00, 0x00);
                    page_B00->flags |= CLEM_MEM_PAGE_CARDMEM_FLAG;
                    page_B01->flags |= CLEM_MEM_PAGE_CARDMEM_FLAG;
                }
                page_B00->flags &= ~CLEM_MEM_PAGE_WRITEOK_FLAG;
                page_B01->flags &= ~CLEM_MEM_PAGE_WRITEOK_FLAG;
            }
            for (page_idx = 0xC8; page_idx < 0xD0; ++page_idx) {
                bool intcx_page =
                    !(memory_flags & CLEM_MEM_IO_MMAP_CXROM) || mmio->card_expansion_rom_index < 0;
                page_B00 = &page_map_B00->pages[page_idx];
                page_B01 = &page_map_B01->pages[page_idx];
                if (intcx_page) {
                    /* internal ROM */
                    clem_mem_create_page_mapping(page_B00, page_idx, 0xff, 0x00);
                    clem_mem_create_page_mapping(page_B01, page_idx, 0xff, 0x01);
                    page_B00->flags &= ~CLEM_MEM_PAGE_CARDMEM_FLAG;
                    page_B01->flags &= ~CLEM_MEM_PAGE_CARDMEM_FLAG;
                } else {
                    clem_mem_create_page_mapping(page_B00, page_idx - 0xc8, 0xcc, 0xcc);
                    clem_mem_create_page_mapping(page_B01, page_idx - 0xc8, 0xcc, 0xcc);
                    page_B00->flags |= CLEM_MEM_PAGE_CARDMEM_FLAG;
                    page_B01->flags |= CLEM_MEM_PAGE_CARDMEM_FLAG;
                }
                page_B00->flags &= ~CLEM_MEM_PAGE_WRITEOK_FLAG;
                page_B01->flags &= ~CLEM_MEM_PAGE_WRITEOK_FLAG;
            }
        }
        //  e0, e1 isn't affected by shadowing
        if (remap_flags & CLEM_MEM_IO_MMAP_CROM) {
            for (page_idx = 0xC1; page_idx < 0xC8; ++page_idx) {
                unsigned slot_idx = ((page_idx - 1) & 0xf);
                bool intcx_page;
                if (page_idx == 0xC3) {
                    intcx_page = !(memory_flags & CLEM_MEM_IO_MMAP_C3ROM);
                } else {
                    intcx_page = !(memory_flags & CLEM_MEM_IO_MMAP_CXROM) ||
                                 !(memory_flags & (CLEM_MEM_IO_MMAP_C1ROM << slot_idx));
                }
                page_BE0 = &page_map_BE0->pages[page_idx];
                page_BE1 = &page_map_BE1->pages[page_idx];
                if (intcx_page) {
                    clem_mem_create_page_mapping(page_BE0, page_idx, 0xff, 0xe0);
                    clem_mem_create_page_mapping(page_BE1, page_idx, 0xff, 0xe1);
                    page_BE0->flags &= ~CLEM_MEM_PAGE_CARDMEM_FLAG;
                    page_BE1->flags &= ~CLEM_MEM_PAGE_CARDMEM_FLAG;
                } else {
                    clem_mem_create_page_mapping(page_BE0, page_idx, 0x00, 0x00);
                    clem_mem_create_page_mapping(page_BE1, page_idx, 0x00, 0x00);
                    page_BE0->flags |= CLEM_MEM_PAGE_CARDMEM_FLAG;
                    page_BE1->flags |= CLEM_MEM_PAGE_CARDMEM_FLAG;
                }
                page_BE0->flags &= ~CLEM_MEM_PAGE_WRITEOK_FLAG;
                page_BE1->flags &= ~CLEM_MEM_PAGE_WRITEOK_FLAG;
            }
            for (page_idx = 0xC8; page_idx < 0xD0; ++page_idx) {
                bool intcx_page =
                    !(memory_flags & CLEM_MEM_IO_MMAP_CXROM) || mmio->card_expansion_rom_index < 0;
                page_BE0 = &page_map_BE0->pages[page_idx];
                page_BE1 = &page_map_BE1->pages[page_idx];
                if (intcx_page) {
                    /* internal ROM */
                    clem_mem_create_page_mapping(page_BE0, page_idx, 0xff, 0xe0);
                    clem_mem_create_page_mapping(page_BE1, page_idx, 0xff, 0xe1);
                    page_BE0->flags &= ~CLEM_MEM_PAGE_CARDMEM_FLAG;
                    page_BE1->flags &= ~CLEM_MEM_PAGE_CARDMEM_FLAG;
                } else {
                    clem_mem_create_page_mapping(page_BE0, page_idx - 0xc8, 0xcc, 0xcc);
                    clem_mem_create_page_mapping(page_BE1, page_idx - 0xc8, 0xcc, 0xcc);
                    page_BE0->flags |= CLEM_MEM_PAGE_CARDMEM_FLAG;
                    page_BE1->flags |= CLEM_MEM_PAGE_CARDMEM_FLAG;
                }
                page_BE0->flags &= ~CLEM_MEM_PAGE_WRITEOK_FLAG;
                page_BE1->flags &= ~CLEM_MEM_PAGE_WRITEOK_FLAG;
            }
        }
    }

    //  Language Card softswitches - ROM/RAM/IOLC for Bank 00/01,
    //                               RAM for Bank E0/E1
    if (remap_flags & CLEM_MEM_IO_MMAP_LC) {
        bool is_rom_bank_0x =
            !(memory_flags & CLEM_MEM_IO_MMAP_NIOLC) && !(memory_flags & CLEM_MEM_IO_MMAP_RDLCRAM);
        for (page_idx = 0xD0; page_idx < 0xE0; ++page_idx) {
            page_B00 = &page_map_B00->pages[page_idx];
            page_B01 = &page_map_B01->pages[page_idx];
            page_BE0 = &page_map_BE0->pages[page_idx];
            page_BE1 = &page_map_BE1->pages[page_idx];
            if (memory_flags & CLEM_MEM_IO_MMAP_ALTZPLC) {
                page_B00->bank_read = is_rom_bank_0x ? 0xff : 0x01;
                page_B00->bank_write = 0x01;
            } else {
                page_B00->bank_read = is_rom_bank_0x ? 0xff : 0x00;
                page_B00->bank_write = 0x00;
            }
            page_B01->bank_read = is_rom_bank_0x ? 0xff : 0x01;
            page_B01->bank_write = 0x01;
            if (is_rom_bank_0x) {
                page_B00->flags &= ~CLEM_MEM_PAGE_MAINAUX_FLAG;
                page_B01->flags &= ~CLEM_MEM_PAGE_MAINAUX_FLAG;
            } else {
                page_B00->flags |= CLEM_MEM_PAGE_MAINAUX_FLAG;
                page_B01->flags |= CLEM_MEM_PAGE_MAINAUX_FLAG;
            }
            //  Bank 00, 01 IOLC
            if (memory_flags & (CLEM_MEM_IO_MMAP_NIOLC + CLEM_MEM_IO_MMAP_LCBANK2)) {
                page_B00->read = page_idx;
                page_B00->write = page_idx;
                page_B01->read = page_idx;
                page_B01->write = page_idx;
            } else if (!(memory_flags & CLEM_MEM_IO_MMAP_LCBANK2)) {
                //  LC bank 1 = 0xC000-0xCFFF
                page_B00->read = 0xC0 + (page_idx - 0xD0);
                page_B00->write = 0xC0 + (page_idx - 0xD0);
                page_B01->read = 0xC0 + (page_idx - 0xD0);
                page_B01->write = 0xC0 + (page_idx - 0xD0);
            }
            if (memory_flags & CLEM_MEM_IO_MMAP_LCBANK2) {
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
            if (memory_flags & CLEM_MEM_IO_MMAP_NIOLC) {
                //  diabled LC - region treated as writable RAM
                page_B00->flags |= CLEM_MEM_PAGE_WRITEOK_FLAG;
                page_B01->flags |= CLEM_MEM_PAGE_WRITEOK_FLAG;
            } else {
                if (memory_flags & CLEM_MEM_IO_MMAP_WRLCRAM) {
                    page_B00->flags |= CLEM_MEM_PAGE_WRITEOK_FLAG;
                    page_B01->flags |= CLEM_MEM_PAGE_WRITEOK_FLAG;
                } else {
                    page_B00->flags &= ~CLEM_MEM_PAGE_WRITEOK_FLAG;
                    page_B01->flags &= ~CLEM_MEM_PAGE_WRITEOK_FLAG;
                }
            }
        }
        for (page_idx = 0xE0; page_idx < 0x100; ++page_idx) {
            page_B00 = &page_map_B00->pages[page_idx];
            page_B01 = &page_map_B01->pages[page_idx];
            page_BE0 = &page_map_BE0->pages[page_idx];
            page_BE1 = &page_map_BE1->pages[page_idx];
            if (memory_flags & CLEM_MEM_IO_MMAP_ALTZPLC) {
                page_B00->bank_read = is_rom_bank_0x ? 0xff : 0x01;
                page_B00->bank_write = 0x01;
            } else {
                page_B00->bank_read = is_rom_bank_0x ? 0xff : 0x00;
                page_B00->bank_write = 0x00;
            }
            page_B01->bank_read = is_rom_bank_0x ? 0xff : 0x01;
            page_B01->bank_write = 0x01;

            if (is_rom_bank_0x) {
                page_B00->flags &= ~CLEM_MEM_PAGE_MAINAUX_FLAG;
                page_B01->flags &= ~CLEM_MEM_PAGE_MAINAUX_FLAG;
            } else {
                page_B00->flags |= CLEM_MEM_PAGE_MAINAUX_FLAG;
                page_B01->flags |= CLEM_MEM_PAGE_MAINAUX_FLAG;
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
            if (memory_flags & CLEM_MEM_IO_MMAP_NIOLC) {
                //  diabled LC - region treated as writable RAM
                page_B00->flags |= CLEM_MEM_PAGE_WRITEOK_FLAG;
                page_B01->flags |= CLEM_MEM_PAGE_WRITEOK_FLAG;
            } else {
                if (memory_flags & CLEM_MEM_IO_MMAP_WRLCRAM) {
                    page_B00->flags |= CLEM_MEM_PAGE_WRITEOK_FLAG;
                    page_B01->flags |= CLEM_MEM_PAGE_WRITEOK_FLAG;
                } else {
                    page_B00->flags &= ~CLEM_MEM_PAGE_WRITEOK_FLAG;
                    page_B01->flags &= ~CLEM_MEM_PAGE_WRITEOK_FLAG;
                }
            }
        }
    }

    mmio->mmap_register = memory_flags;
}

void _clem_mmio_restore_mappings(ClemensMMIO *mmio) {
    uint32_t memory_flags = mmio->mmap_register;
    mmio->mmap_register = 0xffffffff;
    _clem_mmio_memory_map(mmio, 0x0000000000);
    _clem_mmio_memory_map(mmio, memory_flags);
}

void _clem_mmio_init_page_maps(ClemensMMIO *mmio, struct ClemensMemoryPageMap **bank_page_map,
                               uint8_t *e0_bank, uint8_t *e1_bank, uint32_t memory_flags) {
    struct ClemensMemoryPageMap *page_map;
    struct ClemensMemoryPageInfo *page;
    unsigned page_idx;
    unsigned bank_idx;

    //  Bank 00, 01 as RAM
    //  TODO need to mask bank for main and aux page maps
    mmio->e0_bank = e0_bank;
    mmio->e1_bank = e1_bank;
    mmio->bank_page_map = bank_page_map;

    page_map = &mmio->empty_page_map;
    page_map->shadow_map = NULL;
    for (page_idx = 0x00; page_idx < 0x100; ++page_idx) {
        /* using a non-valid IIgs bank here that's not writable. */
        page = &page_map->pages[page_idx];
        clem_mem_create_page_mapping(page, page_idx, CLEM_IIGS_EMPTY_RAM_BANK,
                                     CLEM_IIGS_EMPTY_RAM_BANK);
        page->flags &= ~CLEM_MEM_PAGE_WRITEOK_FLAG;
    }

    page_map = &mmio->fpi_main_page_map;
    page_map->shadow_map = &mmio->fpi_mega2_main_shadow_map;
    for (page_idx = 0x00; page_idx < 0x100; ++page_idx) {
        _clem_mmio_create_page_mainaux_mapping(&page_map->pages[page_idx], page_idx, 0x00);
    }
    page_map = &mmio->fpi_aux_page_map;
    page_map->shadow_map = &mmio->fpi_mega2_aux_shadow_map;
    for (page_idx = 0x00; page_idx < 0x100; ++page_idx) {
        _clem_mmio_create_page_mainaux_mapping(&page_map->pages[page_idx], page_idx, 0x01);
    }
    //  Banks 02-7f typically (if expanded memory is available)
    page_map = &mmio->fpi_direct_page_map;
    page_map->shadow_map = NULL;
    for (page_idx = 0x00; page_idx < 0x100; ++page_idx) {
        _clem_mmio_create_page_direct_mapping(&page_map->pages[page_idx], page_idx);
    }
    //  Banks E0 - C000-CFFF mapped as IO, Internal ROM
    page_map = &mmio->mega2_main_page_map;
    page_map->shadow_map = NULL;
    for (page_idx = 0x00; page_idx < 0x100; ++page_idx) {
        _clem_mmio_create_page_direct_mapping(&page_map->pages[page_idx], page_idx);
    }
    page_map->pages[0xC0].flags &= ~CLEM_MEM_PAGE_DIRECT_FLAG;
    page_map->pages[0xC0].flags |= CLEM_MEM_PAGE_IOADDR_FLAG;
    for (page_idx = 0xC1; page_idx < 0xD0; ++page_idx) {
        page = &page_map->pages[page_idx];
        clem_mem_create_page_mapping(page, page_idx, 0xff, 0xe0);
        page->flags &= ~CLEM_MEM_PAGE_WRITEOK_FLAG;
    }
    //  Banks E1 - C000-CFFF mapped as IO, Internal ROM
    page_map = &mmio->mega2_aux_page_map;
    page_map->shadow_map = NULL;
    for (page_idx = 0x00; page_idx < 0x100; ++page_idx) {
        _clem_mmio_create_page_direct_mapping(&page_map->pages[page_idx], page_idx);
    }
    page_map->pages[0xC0].flags &= ~CLEM_MEM_PAGE_DIRECT_FLAG;
    page_map->pages[0xC0].flags |= CLEM_MEM_PAGE_IOADDR_FLAG;
    for (page_idx = 0xC1; page_idx < 0xD0; ++page_idx) {
        page = &page_map->pages[page_idx];
        clem_mem_create_page_mapping(page, page_idx, 0xff, 0xe1);
        page->flags &= ~CLEM_MEM_PAGE_WRITEOK_FLAG;
    }
    //  Banks FC-FF ROM access is read-only of course.
    page_map = &mmio->fpi_rom_page_map;
    page_map->shadow_map = NULL;
    for (page_idx = 0x00; page_idx < 0x100; ++page_idx) {
        page = &page_map->pages[page_idx];
        _clem_mmio_create_page_direct_mapping(page, page_idx);
        page->flags &= ~CLEM_MEM_PAGE_WRITEOK_FLAG;
    }

    //  set up the default page mappings
    mmio->bank_page_map[0x00] = &mmio->fpi_main_page_map;
    mmio->bank_page_map[0x01] = &mmio->fpi_aux_page_map;

    for (bank_idx = 0x02; bank_idx < mmio->fpi_ram_bank_count; ++bank_idx) {
        mmio->bank_page_map[bank_idx] = &mmio->fpi_direct_page_map;
    }
    /* TODO: handle expansion RAM */
    for (bank_idx = mmio->fpi_rom_bank_count; bank_idx < 0x80; ++bank_idx) {
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
    for (bank_idx = 0xF0; bank_idx < 0x100; ++bank_idx) {
        mmio->bank_page_map[bank_idx] = &mmio->empty_page_map;
    }
    for (bank_idx = 0x100 - mmio->fpi_rom_bank_count; bank_idx < 0x100; ++bank_idx) {
        mmio->bank_page_map[bank_idx] = &mmio->fpi_rom_page_map;
    }

    memset(&mmio->fpi_mega2_main_shadow_map, 0, sizeof(mmio->fpi_mega2_main_shadow_map));
    memset(&mmio->fpi_mega2_aux_shadow_map, 0, sizeof(mmio->fpi_mega2_aux_shadow_map));

    /* brute force initialization of all page maps to ensure every option
       is executed on startup */
    mmio->mmap_register = memory_flags;
    _clem_mmio_restore_mappings(mmio);
}

void clem_mmio_restore(ClemensMMIO *mmio, struct ClemensMemoryPageMap **bank_page_map,
                       uint8_t *e0_bank, uint8_t *e1_bank) {
    _clem_mmio_init_page_maps(mmio, bank_page_map, e0_bank, e1_bank, mmio->mmap_register);
}

void clem_mmio_reset(ClemensMMIO *mmio, struct ClemensTimeSpec *tspec) {
    clem_timer_reset(&mmio->dev_timer);
    clem_rtc_reset(&mmio->dev_rtc, CLEM_CLOCKS_PHI0_CYCLE);
    clem_adb_reset(&mmio->dev_adb);
    clem_sound_reset(&mmio->dev_audio);
    clem_vgc_reset(&mmio->vgc);
    clem_iwm_reset(&mmio->dev_iwm, tspec);
    clem_scc_reset(&mmio->dev_scc);
}

void clem_mmio_init(ClemensMMIO *mmio, struct ClemensDeviceDebugger *dev_debug,
                    struct ClemensMemoryPageMap **bank_page_map, void *slot_expansion_rom,
                    unsigned int fpi_ram_bank_count, unsigned int fpi_rom_bank_count,
                    uint8_t *e0_bank, uint8_t *e1_bank, struct ClemensTimeSpec *tspec) {
    int idx;
    //  Memory map starts out without shadowing, but our call to
    //  init_page_maps will initialize the memory map on IIgs reset
    //  Fast CPU mode
    //  TODO: support enabling bank latch if we ever need to as this would be
    //        the likely value at reset (bit set to 0 vs 1)
    mmio->dev_debug = dev_debug;
    mmio->new_video_c029 = CLEM_MMIO_NEWVIDEO_BANKLATCH_INHIBIT;
    //  TODO: ROM 01 will not use bit 6 and expect it to be cleared
    mmio->speed_c036 = CLEM_MMIO_SPEED_FAST_ENABLED | CLEM_MMIO_SPEED_POWERED_ON;
    mmio->mega2_cycles = 0;
    mmio->last_data_address = 0xffffffff;
    mmio->emulator_detect = CLEM_MMIO_EMULATOR_DETECT_IDLE;
    mmio->card_expansion_rom_index = -1;
    mmio->fpi_ram_bank_count = fpi_ram_bank_count;
    mmio->fpi_rom_bank_count = fpi_rom_bank_count;

    //  TODO: look into making mega2 memory solely reside inside mmio to avoid this
    //        external dependency.
    for (idx = 0; idx < CLEM_CARD_SLOT_COUNT; ++idx) {
        mmio->card_slot[idx] = NULL;
        mmio->card_slot_expansion_memory[idx] = (((uint8_t *)slot_expansion_rom) + (idx * 2048));
    }
    mmio->bank_page_map = bank_page_map;

    //  initial settings for memory map on reset/initr
    _clem_mmio_init_page_maps(mmio, bank_page_map, e0_bank, e1_bank,
                              CLEM_MEM_IO_MMAP_NSHADOW_SHGR | CLEM_MEM_IO_MMAP_WRLCRAM |
                                  CLEM_MEM_IO_MMAP_LCBANK2);

    clem_mmio_reset(mmio, tspec);
}
