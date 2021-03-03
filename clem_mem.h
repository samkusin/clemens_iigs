#include "clem_types.h"
#include "clem_mmio_defs.h"

/**
 * Memory Mapping Controller
 *
 * A major part of What makes a 65816 an 'Apple IIgs' machine.   The goals of
 * this module are to emulate accessing both FPI and the Mega2 memory.  From
 * this, the MMC controls read/write access to I/O registers that drive the
 * various 'machine' components (aka the main method of accessing devices
 * from machine instructions - Memory Mapped I/O)
 *
 * The Mega2 is particularly tricky due to this 'slow RAM' + shadowing methods
 * of access.  Specific state to determine what pages to access and where when
 * emulating 8-bit Apple II devices is particular tricky.
 *
 * This module admittedly covers a lot.  It must support 'slow' accesses to
 * Mega2 memory, shadowing, bank switching, I/O, etc.  Fortunately the I/O
 * registers and techniques here are well documented by 1980s Apple literature.
 *
 * Things to consider (a development TODO)
 * - The IIgs Technical Introduction is a good start for those unfamiliar with
 *   what's described above
 * - The IIgs Firmware Reference from 1987 gives some excellent background on
 *   what's going on under the hood on startup and how the components work
 *   together.  It's a good reference for certain I/O registers in $C0xx space
 * - The IIgs Hardware Reference is another source for what the $Cxxx pages
 *   are for, registers, and details about these components from a programming
 *   standpoint.  Much of this module uses this as a source
 * - Also some old //e technical docs - of which include even more details.
 *   Seems the earlier machines even have more technical documentation.
 *
 */

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

static void _clem_mmio_create_page_direct_mapping(
    struct ClemensMMIOPageInfo* page,
    uint8_t page_idx
) {
    page->read = page_idx;
    page->write = page_idx;
    page->flags =  CLEM_MMIO_PAGE_WRITE_OK | CLEM_MMIO_PAGE_DIRECT;
}

static uint8_t _clem_mmio_read(
    ClemensMachine* clem,
    uint16_t addr,
    uint32_t* clocks_spent
) {

    return 0;
}

static void _clem_mmio_write(
    ClemensMachine* clem,
    uint8_t data,
    uint16_t addr,
    uint32_t* clocks_spent
) {

}

static inline uint8_t* _clem_get_memory_bank(
    ClemensMachine* clem,
    uint8_t bank,
    uint32_t* clocks_spent
) {
    if (bank == 0xe0 || bank == 0xe1) {
        *clocks_spent = clem->clocks_step_mega2;
        return clem->mega2_bank_map[bank & 0x1];
    }
    *clocks_spent = clem->clocks_step;
    return clem->fpi_bank_map[bank];
}

static inline void _clem_read(
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
    if (page->flags & CLEM_MMIO_PAGE_IOADDR) {
        *data = _clem_mmio_read(clem, offset, &clocks_spent);
    } else {
        uint8_t* bank_mem;
        if (page->flags & CLEM_MMIO_PAGE_DIRECT) {
            bank_mem = _clem_get_memory_bank(clem, bank, &clocks_spent);
        } else {
            bank_mem = _clem_get_memory_bank(clem, page->bank_read, &clocks_spent);
        }
        *data = bank_mem[offset];
    }

    // TODO: account for slow/fast memory access
    clem->cpu.pins.adr = adr;
    clem->cpu.pins.bank = bank;
    clem->cpu.pins.data = *data;
    clem->clocks_spent += clocks_spent;
    ++clem->cpu.cycles_spent;
}

static inline void _clem_write(
    ClemensMachine* clem,
    uint8_t data,
    uint16_t adr,
    uint8_t bank
) {
    struct ClemensMMIOPageMap* bank_page_map = clem->mmio.bank_page_map[bank];
    struct ClemensMMIOPageInfo* page = &bank_page_map->pages[adr >> 8];
    uint32_t clocks_spent;
    uint16_t offset = ((uint16_t)page->write << 8) | (adr & 0xff);
    if (page->flags & CLEM_MMIO_PAGE_IOADDR) {
        _clem_mmio_write(clem, data, offset, &clocks_spent);
    } else {
        uint8_t* bank_mem;
        if (page->flags & CLEM_MMIO_PAGE_DIRECT) {
            bank_mem = _clem_get_memory_bank(clem, bank, &clocks_spent);
        } else {
            bank_mem = _clem_get_memory_bank(clem, page->bank_read, &clocks_spent);
        }
        bank_mem[offset] = data;
    }
    clem->cpu.pins.adr = adr;
    clem->cpu.pins.bank = bank;
    clem->cpu.pins.data = data;
    clem->clocks_spent += clocks_spent;
    ++clem->cpu.cycles_spent;
}

void _clem_mmio_shadow_pages(
    struct ClemensMMIOPageMap* page_map,
    unsigned start_page_idx,
    unsigned end_page_idx,
    bool shadowed
) {
    unsigned page_idx;
    for (page_idx = start_page_idx; page_idx < end_page_idx; ++page_idx) {
        struct ClemensMMIOPageInfo* page = &page_map->pages[page_idx];
        if (shadowed) page->flags |= CLEM_MMIO_PAGE_SHADOWED;
        else page->flags &= ~CLEM_MMIO_PAGE_SHADOWED;
    }
}

void _clem_mmio_shadow_txt1(struct ClemensMMIO* mmio) {
    struct ClemensMMIOPageMap* page_map = &mmio->fpi_main_page_map;
    bool inhibit_shadow = (mmio->shadowC035 & kClemensMMIOShadow_TXT1_Inhibit);
    unsigned page_idx;
    for (page_idx = 0x04; page_idx < 0x08; ++page_idx) {
        struct ClemensMMIOPageInfo* page = &page_map->pages[page_idx];
        if (inhibit_shadow) page->flags &= ~CLEM_MMIO_PAGE_SHADOWED;
        else page->flags |= CLEM_MMIO_PAGE_SHADOWED;
    }
    //  TODO: aux bank 0
    page_map = &mmio->fpi_aux_page_map;
    for (page_idx = 0x04; page_idx < 0x08; ++page_idx) {
        struct ClemensMMIOPageInfo* page = &page_map->pages[page_idx];
        if (inhibit_shadow) page->flags &= ~CLEM_MMIO_PAGE_SHADOWED;
        else page->flags |= CLEM_MMIO_PAGE_SHADOWED;
    }
}

void _clem_mmio_shadow_txt2(struct ClemensMMIO* mmio) {
    struct ClemensMMIOPageMap* page_map = &mmio->fpi_main_page_map;
    bool inhibit_shadow = (mmio->shadowC035 & kClemensMMIOShadow_TXT2_Inhibit);
    unsigned page_idx;
    for (page_idx = 0x08; page_idx < 0x0C; ++page_idx) {
        struct ClemensMMIOPageInfo* page = &page_map->pages[page_idx];
        if (inhibit_shadow) page->flags &= ~CLEM_MMIO_PAGE_SHADOWED;
        else page->flags |= CLEM_MMIO_PAGE_SHADOWED;
    }
    //  TODO: bank 0 aux
    page_map = &mmio->fpi_aux_page_map;
    for (page_idx = 0x08; page_idx < 0x0C; ++page_idx) {
        struct ClemensMMIOPageInfo* page = &page_map->pages[page_idx];
        if (inhibit_shadow) page->flags &= ~CLEM_MMIO_PAGE_SHADOWED;
        else page->flags |= CLEM_MMIO_PAGE_SHADOWED;
    }
}

void _clem_mmio_shadow_hgr_multi(struct ClemensMMIO* mmio) {
    /* handle hgr page 0,1 in main, aux
       handle shgr in aux, negotiating the auxillary page shadowing for hgr as
        well
    */
    struct ClemensMMIOPageMap* page_map_0 = &mmio->fpi_main_page_map;
    struct ClemensMMIOPageMap* page_map_1 = &mmio->fpi_aux_page_map;
    bool inhibit_hgr1 = (mmio->shadowC035 & kClemensMMIOShadow_HGR1_Inhibit);
    bool inhibit_hgr2 = (mmio->shadowC035 & kClemensMMIOShadow_HGR2_Inhibit);
    bool inhibit_shgr = (mmio->shadowC035 & kClemensMMIOShadow_SHGR_Inhibit);
    bool inhibit_aux_hgr = (mmio->shadowC035 & kClemensMMIOShadow_AUXHGR_Inhibit);
    unsigned page_idx;

    //  HGR1
    for (page_idx = 0x20; page_idx < 0x40; ++page_idx) {
        struct ClemensMMIOPageInfo* page = &page_map_0->pages[page_idx];
        if (inhibit_hgr1) page->flags &= ~CLEM_MMIO_PAGE_SHADOWED;
        else page->flags |= CLEM_MMIO_PAGE_SHADOWED;
    }
    //  TODO: bank 0 aux access
    for (page_idx = 0x20; page_idx < 0x40; ++page_idx) {
        struct ClemensMMIOPageInfo* page = &page_map_1->pages[page_idx];
        if (inhibit_hgr1 || inhibit_shgr) {
            page->flags &= ~CLEM_MMIO_PAGE_SHADOWED;
        } else if (!inhibit_aux_hgr || !inhibit_shgr) {
            page->flags |= CLEM_MMIO_PAGE_SHADOWED;
        }
    }
    //  HGR2
    for (page_idx = 0x40; page_idx < 0x60; ++page_idx) {
        struct ClemensMMIOPageInfo* page = &page_map_0->pages[page_idx];
        if (inhibit_hgr1) page->flags &= ~CLEM_MMIO_PAGE_SHADOWED;
        else page->flags |= CLEM_MMIO_PAGE_SHADOWED;
    }
    //  TODO: bank 0 aux access
    if (!(mmio->shadowC035 & kClemensMMIOShadow_AUXHGR_Inhibit)) {
        for (page_idx = 0x40; page_idx < 0x60; ++page_idx) {
            struct ClemensMMIOPageInfo* page = &page_map_1->pages[page_idx];
            if (inhibit_hgr1) {
                page->flags &= ~CLEM_MMIO_PAGE_SHADOWED;
            } else if (!inhibit_aux_hgr || !inhibit_shgr) {
                page->flags |= CLEM_MMIO_PAGE_SHADOWED;
            }
        }
    }
    //  SHGR (upper pages only, since 0x20-0x60 is handled during the HGR1/2
    //        pass)
    //  TODO: bank 0 aux access
    for (page_idx = 0x60; page_idx < 0xA0; ++page_idx) {
        struct ClemensMMIOPageInfo* page = &page_map_1->pages[page_idx];
        if (inhibit_shgr) page->flags &= ~CLEM_MMIO_PAGE_SHADOWED;
        else page->flags |= CLEM_MMIO_PAGE_SHADOWED;
    }
}

void _clem_mmio_shadow_iolc(struct ClemensMMIO* mmio) {

}

/*
void _clem_mmio_shadow_iolc_page_maps(struct ClemensMMIO* mmio) {
    if (inhibit) {
        //
        for (page_idx = 0xC0; page_idx < 0xCF; ++page_idx) {
            page = &page_map->pages[page_idx];
            _clem_mmio_create_page_direct_mapping(page, page_idx);
            page->bank_read = 0xe0;
            page->bank_write = 0xe0;
            page->flags |= CLEM_MMIO_PAGE_IOADDR;
            page->flags &= ~CLEM_MMIO_PAGE_DIRECT;
        }
        for (page_idx = 0xD0; page_idx < 0xDF; ++page_idx) {
            page = &page_map->pages[page_idx];
            _clem_mmio_create_page_direct_mapping(page, page_idx);
            page->bank_read = 0xff;
            page->bank_write = 0x00;
            page->flags &= ~CLEM_MMIO_PAGE_DIRECT;
        }
        for (page_idx = 0xE0; page_idx < 0x100; ++page_idx) {
            page = &page_map->pages[page_idx];
            _clem_mmio_create_page_direct_mapping(page, page_idx);
            page->bank_read = 0xff;
            page->bank_write = 0x00;
            page->flags &= ~CLEM_MMIO_PAGE_DIRECT;
        }
    } else {
        for (page_idx = 0xC0; page_idx < 0xCF; ++page_idx) {
            page = &page_map->pages[page_idx];
            _clem_mmio_create_page_direct_mapping(page, page_idx);
            page->bank_read = 0xe0;
            page->bank_write = 0xe0;
            page->flags |= CLEM_MMIO_PAGE_IOADDR;
            page->flags &= ~CLEM_MMIO_PAGE_DIRECT;
        }
        for (page_idx = 0xD0; page_idx < 0xDF; ++page_idx) {
            page = &page_map->pages[page_idx];
            _clem_mmio_create_page_direct_mapping(page, page_idx);
            page->bank_read = 0xff;
            page->bank_write = 0x00;
            page->flags &= ~CLEM_MMIO_PAGE_DIRECT;
        }
        for (page_idx = 0xE0; page_idx < 0x100; ++page_idx) {
            page = &page_map->pages[page_idx];
            _clem_mmio_create_page_direct_mapping(page, page_idx);
            page->bank_read = 0xff;
            page->bank_write = 0x00;
            page->flags &= ~CLEM_MMIO_PAGE_DIRECT;
        }
    }
}
*/

void _clem_mmio_init_page_maps(struct ClemensMMIO* mmio) {
    struct ClemensMMIOPageMap* page_map;
    struct ClemensMMIOPageInfo* page;
    unsigned page_idx;
    unsigned bank_idx;

    //  Bank 00, 01 as RAM
    page_map = &mmio->fpi_main_page_map;
    for (page_idx = 0x00; page_idx < 0xFF; ++page_idx) {
        _clem_mmio_create_page_direct_mapping(
            &page_map->pages[page_idx], page_idx);
    }
    page_map = &mmio->fpi_aux_page_map;
    for (page_idx = 0x00; page_idx < 0xFF; ++page_idx) {
        _clem_mmio_create_page_direct_mapping(
            &page_map->pages[page_idx], page_idx);
    }
    //  Banks 02-7F typically
    page_map = &mmio->fpi_direct_page_map;
    for (page_idx = 0x00; page_idx < 0x100; ++page_idx) {
        _clem_mmio_create_page_direct_mapping(
            &page_map->pages[page_idx], page_idx);
    }
    //  Banks E0
    page_map = &mmio->mega2_main_page_map;
    for (page_idx = 0x00; page_idx < 0x100; ++page_idx) {
        _clem_mmio_create_page_direct_mapping(
            &page_map->pages[page_idx], page_idx);
    }
    //  Banks E1
    page_map = &mmio->mega2_aux_page_map;
    for (page_idx = 0x00; page_idx < 0x100; ++page_idx) {
        _clem_mmio_create_page_direct_mapping(
            &page_map->pages[page_idx], page_idx);
    }
    //  Banks FC-FF ROM access is read-only of course.
    page_map = &mmio->fpi_rom_page_map;
    for (page_idx = 0x00; page_idx < 0x100; ++page_idx) {
        page = &page_map->pages[page_idx];
        _clem_mmio_create_page_direct_mapping(page, page_idx);
        page->flags &= ~CLEM_MMIO_PAGE_WRITE_OK;
    }

    //  set up the default page mappings
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

    //  apply softswitches which affect page mappings for banks 00,01,E0,E1
    //  shadow regions for 00, 01
    _clem_mmio_shadow_txt1(mmio);
    _clem_mmio_shadow_txt2(mmio);
    _clem_mmio_shadow_hgr_multi(mmio);
    _clem_mmio_shadow_iolc(mmio);

    //  IOLC for 00, 01
    //
}
/*

    // TODO: apply all I/O register settings now - these should be the
    //       defaults
    if (!(mmio->shadowC035 & kClemensMMIOShadow_TXT1_Inhibit)) {
        for (page_idx = 0x04; page_idx < 0x08; ++page_idx) {
            page = &page_map->pages[page_idx];
            page->flags |= CLEM_MMIO_PAGE_SHADOWED;
        }
    }
    if (!(mmio->shadowC035 & kClemensMMIOShadow_TXT2_Inhibit)) {
        for (page_idx = 0x08; page_idx < 0x0C; ++page_idx) {
            page = &page_map->pages[page_idx];
            page->flags |= CLEM_MMIO_PAGE_SHADOWED;
        }
    }
    if (!(mmio->shadowC035 & kClemensMMIOShadow_HGR1_Inhibit)) {
        for (page_idx = 0x20; page_idx < 0x40; ++page_idx) {
            page = &page_map->pages[page_idx];
            page->flags |= CLEM_MMIO_PAGE_SHADOWED;
        }
    }
    if (!(mmio->shadowC035 & kClemensMMIOShadow_HGR2_Inhibit)) {
        for (page_idx = 0x40; page_idx < 0x60; ++page_idx) {
            page = &page_map->pages[page_idx];
            page->flags |= CLEM_MMIO_PAGE_SHADOWED;
        }
    }

    _clem_mmio_shadow_iolc_page_map(
        page_map, 0x00, mmio->shadowC035 & kClemensMMIOShadow_IOLC_Inhibit);
}
*/

void _clem_mmio_init(struct ClemensMMIO* mmio) {
    //  Mega2 shadowing enabled for all regions
    //  Fast CPU mode
    //  TODO: support enabling bank latch if we ever need to as this would be
    //        the likely value at reset (bit set to 0 vs 1)
    mmio->newVideoC029 = kClemensMMIONewVideo_BANKLATCH_Inhibit;
    mmio->shadowC035 = 0x00;
    mmio->speedC036 = kClemensMMIOSpeed_FAST_Enable;

    _clem_mmio_init_page_maps(mmio);
}
