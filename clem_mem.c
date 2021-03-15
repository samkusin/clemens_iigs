#include "clem_mem.h"
#include "clem_debug.h"
#include "clem_mmio_defs.h"

#include "clem_util.h"

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

static inline uint8_t _clem_mmio_newvideo_c029(struct ClemensMMIO* mmio) {
    return mmio->new_video_c029;
}

static inline void _clem_mmio_newvideo_c029_set(
    struct ClemensMMIO* mmio,
    uint8_t value
) {
    uint8_t setflags = mmio->new_video_c029 ^ value;
    if (setflags & kClemensMMIONewVideo_BANKLATCH_Inhibit) {
        if (!(value & kClemensMMIONewVideo_BANKLATCH_Inhibit)) {
            CLEM_UNIMPLEMENTED("ioreg %02x : %02x", CLEM_MMIO_REG_NEWVIDEO, value);
        }
        setflags ^= kClemensMMIONewVideo_BANKLATCH_Inhibit;
    }
    CLEM_ASSERT(setflags == 0);
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
    CLEM_UNIMPLEMENTED("shadow c035 set");
}

static inline uint8_t _clem_mmio_speed_c036(struct ClemensMMIO* mmio) {
    return mmio->speed_c036;
}

static void _clem_mmio_speed_c036_set(
    struct ClemensMMIO* mmio,
    uint8_t value
) {
    uint8_t setflags = mmio->speed_c036 ^ value;
    mmio->speed_c036 = value;
    if (!(mmio->speed_c036 & kClemensMMIOSpeed_PoweredOn)) {
        CLEM_WARN("SPEED C036 PowerOn set to zero");
    }
    mmio->speed_c036 |= kClemensMMIOSpeed_PoweredOn;
}

static inline uint8_t _clem_mmio_statereg_c068(struct ClemensMMIO* mmio) {
    //CLEM_UNIMPLEMENTED("statereg c068");
    return 0;
}

static uint8_t _clem_mmio_statereg_c068_set(
    struct ClemensMMIO* mmio,
    uint8_t value
) {
    CLEM_UNIMPLEMENTED("statereg c068 set");
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
    uint32_t* clocks_spent,
    uint8_t flags
) {
    struct ClemensMMIO* mmio = &clem->mmio;
    uint8_t result = 0x00;
    uint8_t ioreg = addr & 0xff;

    switch (ioreg) {
        case CLEM_MMIO_REG_LC_BANK_TEST:
            result = (mmio->mmap_register & CLEM_MMIO_MMAP_LCBANK2) ? 0x80 : 0x00;
            break;
        case CLEM_MMIO_REG_ROM_RAM_TEST:
            result = (mmio->mmap_register & CLEM_MMIO_MMAP_RDLCRAM) ? 0x80 : 0x00;
            break;
        case CLEM_MMIO_REG_READCXROM:
            result = (mmio->mmap_register & CLEM_MMIO_MMAP_CXROM) ? 0x01 : 00;
            break;
        case CLEM_MMIO_REG_RDALTZP:
            result = (mmio->mmap_register & CLEM_MMIO_MMAP_ALTZPLC) ? 0x80 : 0x00;
            break;
        case CLEM_MMIO_REG_READC3ROM:
            result = (mmio->mmap_register & CLEM_MMIO_MMAP_C3ROM) ? 0x01 : 00;
            break;
        case CLEM_MMIO_REG_NEWVIDEO:
            result = _clem_mmio_newvideo_c029(mmio);
            break;
        case CLEM_MMIO_REG_SHADOW:
            result = _clem_mmio_shadow_c035(mmio);
            break;
        case CLEM_MMIO_REG_SPEED:
            result = _clem_mmio_speed_c036(mmio);
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
        default:
            if (!(flags & CLEM_MMIO_READ_NO_OP)) {
                CLEM_UNIMPLEMENTED("ioreg %u", ioreg);
            }
            break;
    }

    *clocks_spent = !(flags & CLEM_MMIO_READ_NO_OP) ? clem->clocks_step_mega2 : 0;

    return result;
}

static void _clem_mmio_write(
    ClemensMachine* clem,
    uint8_t data,
    uint16_t addr,
    uint32_t* clocks_spent,
    uint8_t flags
) {
    struct ClemensMMIO* mmio = &clem->mmio;
    uint8_t ioreg;
    if (addr >= 0xC100) {
        //  TODO: MMIO slot ROM - it seems this needs to be treated differently
        return;
    }
    ioreg = (addr & 0xff);
    switch (ioreg) {
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
        case CLEM_MMIO_REG_NEWVIDEO:
            _clem_mmio_newvideo_c029_set(mmio, data);
            break;
        case CLEM_MMIO_REG_SHADOW:
            _clem_mmio_shadow_c035_set(mmio, data);
            break;
        case CLEM_MMIO_REG_SPEED:
            _clem_mmio_speed_c036_set(mmio, data);
            break;
        case CLEM_MMIO_REG_STATEREG:
            _clem_mmio_statereg_c068_set(mmio, data);
            break;
        default:
            if (flags != CLEM_MEM_FLAG_NULL) {
                CLEM_UNIMPLEMENTED("ioreg %u", ioreg);
            }
            break;
    }
    if (flags != CLEM_MEM_FLAG_NULL) {
        *clocks_spent = clem->clocks_step_mega2;
    } else {
        *clocks_spent = 0;
    }
}

void _clem_mmio_shadow_map(
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

    //  TXT 1
    if (remap_flags & CLEM_MMIO_MMAP_NSHADOW_TXT1) {
        for (page_idx = 0x04; page_idx < 0x08; ++page_idx) {
            uint8_t v = (shadow_flags & CLEM_MMIO_MMAP_NSHADOW_TXT1) ? 1 : 0;
            mmio->fpi_mega2_main_shadow_map.pages[page_idx] = v;
            mmio->fpi_mega2_aux_shadow_map.pages[page_idx] = v;
        }
    }
    //  TXT 2
    if (remap_flags & CLEM_MMIO_MMAP_NSHADOW_TXT2) {
        for (page_idx = 0x08; page_idx < 0x0C; ++page_idx) {
            uint8_t v = (shadow_flags & CLEM_MMIO_MMAP_NSHADOW_TXT2) ? 1 : 0;
            mmio->fpi_mega2_main_shadow_map.pages[page_idx] = v;
            mmio->fpi_mega2_aux_shadow_map.pages[page_idx] = v;
        }
    }
    //  HGR1
    if (remap_flags & CLEM_MMIO_MMAP_NSHADOW_HGR1) {
        for (page_idx = 0x20; page_idx < 0x40; ++page_idx) {
            uint8_t v = (shadow_flags & CLEM_MMIO_MMAP_NSHADOW_HGR1) ? 1 : 0;
            mmio->fpi_mega2_main_shadow_map.pages[page_idx] = v;
            mmio->fpi_mega2_aux_shadow_map.pages[page_idx] = (
                (v && !inhibit_hgr_bank_01) ? 1 : 0);
        }
    }
    if (remap_flags & CLEM_MMIO_MMAP_NSHADOW_HGR1) {
        for (page_idx = 0x40; page_idx < 0x60; ++page_idx) {
            uint8_t v = (shadow_flags & CLEM_MMIO_MMAP_NSHADOW_HGR2) ? 1 : 0;
            mmio->fpi_mega2_main_shadow_map.pages[page_idx] = v;
            mmio->fpi_mega2_aux_shadow_map.pages[page_idx] = (
                (v && !inhibit_hgr_bank_01) ? 1 : 0);
        }
    }
    if (remap_flags & CLEM_MMIO_MMAP_NSHADOW_SHGR) {
        for (page_idx = 0x60; page_idx < 0xA0; ++page_idx) {
            uint8_t v = (shadow_flags & CLEM_MMIO_MMAP_NSHADOW_SHGR) ? 1 : 0;
            mmio->fpi_mega2_aux_shadow_map.pages[page_idx] = v;
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

    //  RAMRD/RAMWRT
    if (remap_flags & (CLEM_MMIO_MMAP_RAMRD + CLEM_MMIO_MMAP_RAMWRT)) {
        remap_flags |= CLEM_MMIO_MMAP_NSHADOW;
        for (page_idx = 0x02; page_idx < 0xC0; ++page_idx) {
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
    if (remap_flags & (CLEM_MMIO_MMAP_NIOLC +
                       CLEM_MMIO_MMAP_CXROM +
                       CLEM_MMIO_MMAP_C3ROM)
    ) {
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
            for (page_idx = 0xC1; page_idx < 0xD0; ++page_idx) {
                // TODO: peripheral ROM and slot 3 switch
                page_B00 = &page_map_B00->pages[page_idx];
                page_B01 = &page_map_B01->pages[page_idx];
                if (page_idx == 0xC3) {
                    _clem_mmio_create_page_mapping(page_B00, page_idx, 0xff, 0xe0);
                    _clem_mmio_create_page_mapping(page_B01, page_idx, 0xff, 0xe1);
                    page_B00->flags &= ~CLEM_MMIO_PAGE_WRITE_OK;
                    page_B01->flags &= ~CLEM_MMIO_PAGE_WRITE_OK;
                } else {
                    _clem_mmio_create_page_mapping(page_B00, page_idx, 0xff, 0xe0);
                    _clem_mmio_create_page_mapping(page_B01, page_idx, 0xff, 0xe1);
                    page_B00->flags &= ~CLEM_MMIO_PAGE_WRITE_OK;
                    page_B01->flags &= ~CLEM_MMIO_PAGE_WRITE_OK;
                }
            }
        }
        //  e0, e1 isn't affected by shadowing
        if (remap_flags & (CLEM_MMIO_MMAP_C3ROM + CLEM_MMIO_MMAP_CXROM)) {
            for (page_idx = 0xC1; page_idx < 0xD0; ++page_idx) {
                // TODO: peripheral ROM and slot 3 switch
                page_BE0 = &page_map_BE0->pages[page_idx];
                page_BE1 = &page_map_BE1->pages[page_idx];

                if (page_idx == 0xC3) {
                    _clem_mmio_create_page_mapping(page_BE0, page_idx, 0xff, 0xe0);
                    _clem_mmio_create_page_mapping(page_BE1, page_idx, 0xff, 0xe1);
                    page_BE0->flags &= ~CLEM_MMIO_PAGE_WRITE_OK;
                    page_BE1->flags &= ~CLEM_MMIO_PAGE_WRITE_OK;
                } else {
                    _clem_mmio_create_page_mapping(page_BE0, page_idx, 0xff, 0xe0);
                    _clem_mmio_create_page_mapping(page_BE1, page_idx, 0xff, 0xe1);
                    page_BE0->flags &= ~CLEM_MMIO_PAGE_WRITE_OK;
                    page_BE1->flags &= ~CLEM_MMIO_PAGE_WRITE_OK;
                }
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
    //  Banks 02-7F typically
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
    for (bank_idx = 0x02; bank_idx < 0x80; ++bank_idx) {
        mmio->bank_page_map[bank_idx] = &mmio->fpi_direct_page_map;
    }
    mmio->bank_page_map[0xE0] = &mmio->mega2_main_page_map;
    mmio->bank_page_map[0xE1] = &mmio->mega2_aux_page_map;
    for (bank_idx = 0xFC; bank_idx < 0x100; ++bank_idx) {
        mmio->bank_page_map[bank_idx] = &mmio->fpi_rom_page_map;
    }

    memset(&mmio->fpi_mega2_main_shadow_map, 0,
           sizeof(mmio->fpi_mega2_main_shadow_map));
    memset(&mmio->fpi_mega2_aux_shadow_map, 0,
           sizeof(mmio->fpi_mega2_aux_shadow_map));

    _clem_mmio_memory_map(mmio, memory_flags);
}

void _clem_mmio_init(struct ClemensMMIO* mmio) {
    //  Memory map starts out without shadowing, but our call to
    //  init_page_maps will initialize the memory map on IIgs reset
    //  Fast CPU mode
    //  TODO: support enabling bank latch if we ever need to as this would be
    //        the likely value at reset (bit set to 0 vs 1)
    mmio->new_video_c029 = kClemensMMIONewVideo_BANKLATCH_Inhibit;
    mmio->speed_c036 = kClemensMMIOSpeed_FAST_Enable;
    mmio->mmap_register = CLEM_MMIO_MMAP_NSHADOW | CLEM_MMIO_MMAP_NIOLC;
    mmio->flags_c08x = 0;

    _clem_mmio_init_page_maps(mmio,
                              CLEM_MMIO_MMAP_WRLCRAM | CLEM_MMIO_MMAP_LCBANK2);

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
    uint32_t clocks_spent;
    uint16_t offset = ((uint16_t)page->read << 8) | (adr & 0xff);
    bool read_only = (flags == CLEM_MEM_FLAG_NULL);

    // TODO: store off if read_reg has a read_count of 1 here
    //       reset it automatically if true at the end of this function
    if (page->flags & CLEM_MMIO_PAGE_IOADDR) {
        if (offset >= 0xC100) {
            *data = 0;
        } else {
            *data = _clem_mmio_read(clem, offset, &clocks_spent,
                read_only ? CLEM_MMIO_READ_NO_OP : 0);
        }
    } else {
        uint8_t* bank_mem;
        uint8_t bank_actual;
        if (page->flags & CLEM_MMIO_PAGE_DIRECT) {
            bank_actual = bank;
        } else if (page->flags & CLEM_MMIO_PAGE_MAINAUX) {
            bank_actual = (bank & 0xfe) | (page->bank_read & 0x1);
        } else {
            bank_actual = page->bank_read;
        }
        bank_mem = _clem_get_memory_bank(clem, bank_actual, &clocks_spent);

        //  TODO: when reading from e0/e1 banks, is it always slow?
        //          internal ROM, peripheral?
        if (bank_actual == 0xe0 || bank_actual == 0xe1) {
            clocks_spent = clem->clocks_step_mega2;
        }
        *data = bank_mem[offset];
    }

    if (!read_only) {
        // TODO: account for slow/fast memory access
        clem->cpu.pins.adr = adr;
        clem->cpu.pins.bank = bank;
        clem->cpu.pins.data = *data;
        clem->clocks_spent += clocks_spent;
        ++clem->cpu.cycles_spent;
    }
}

void clem_write(
    ClemensMachine* clem,
    uint8_t data,
    uint16_t adr,
    uint8_t bank,
    uint8_t flags
) {
    struct ClemensMMIOPageMap* bank_page_map = clem->mmio.bank_page_map[bank];
    struct ClemensMMIOShadowMap* shadow_map = bank_page_map->shadow_map;
    struct ClemensMMIOPageInfo* page = &bank_page_map->pages[adr >> 8];
    uint32_t clocks_spent;
    uint16_t offset = ((uint16_t)page->write << 8) | (adr & 0xff);
    if (page->flags & CLEM_MMIO_PAGE_IOADDR) {
        //  TODO: bring clocks_spent out of _clem_mmio_write (and other
        //          utility methods - just keep inside clem_read/clem_write)
        if (page->flags & CLEM_MMIO_PAGE_WRITE_OK) {
            _clem_mmio_write(clem, data, offset, &clocks_spent, flags);
        } else {
            clocks_spent = clem->clocks_step_mega2;
        }
    } else {
        uint8_t* bank_mem;
        uint8_t bank_actual;
        if (page->flags & CLEM_MMIO_PAGE_DIRECT) {
            bank_actual = bank;
        } else if (page->flags & CLEM_MMIO_PAGE_MAINAUX) {
            bank_actual = (bank & 0xfe) | (page->bank_write & 0x1);
        } else {
            bank_actual = page->bank_write;
        }
        bank_mem = _clem_get_memory_bank(clem, bank_actual, &clocks_spent);
        if (page->flags & CLEM_MMIO_PAGE_WRITE_OK) {
            bank_mem[offset] = data;
        }
        if (shadow_map && shadow_map->pages[page->write]) {
            bank_mem = _clem_get_memory_bank(
                clem, (0xE0) | (bank_actual & 0x1), &clocks_spent);
            if (page->flags & CLEM_MMIO_PAGE_WRITE_OK) {
                bank_mem[offset] = data;
            }
        }

        if (bank == 0xe0 || bank == 0xe1) {
            clocks_spent = clem->clocks_step_mega2;
        }
    }
    if (flags != CLEM_MEM_FLAG_NULL) {
        clem->cpu.pins.adr = adr;
        clem->cpu.pins.bank = bank;
        clem->cpu.pins.data = data;
        clem->clocks_spent += clocks_spent;
        ++clem->cpu.cycles_spent;
    }
}
