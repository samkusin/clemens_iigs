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


void _clem_init_page_maps(struct ClemensMMIO* mmio) {
    /* Initializes the all of our page map instances.  Each instance maps to
        one or more banks in the MMIO page-map bank table.

        In most cases, the majority of banks use the 'direct' page map.  Banks
        00, 01 use the fpi_main/aux maps, and E0, E1 use the mega2_main/aux
        maps.
    */
    /*
        TODO: This assumption may change to support the extremely edge case of
        shadowing all RAM banks.  In that case, we may need to support two
        more page map types (even and odd banks that are not bank 0,1?)

    */

    /* Use case:
        Bank 00: Apple //e style, Mega2 shadowing
        Page  00-BF direct map
              C0-CF I/O map
              E0-FF Read ROM, Write RAM
              D0-DF Read ROM, Write RAM Bank 2
                (D000-DFFF RAM actual vs Bank 1 which is RAM C000-CFFF)

        Bank 01: Auxillary address bit 17 -> bank 01, E1 shadowing
                 Bypasses softswitches if bank latch bit 0 from NEWVIDEO is off
        Page  00-BF direct map
              C0-CF I/O map
              D0-FF Read and Write RAM, Bank 2
    */
    struct ClemensMMIOPageMap* page_map = &mmio->fpi_direct_page_map;
    unsigned page_idx;
    unsigned bank_idx;

    //  Banks 02-7F typically
    for (page_idx = 0x00; page_idx < 0x100; ++page_idx) {
        _clem_mmio_create_page_direct_mapping(
            &page_map->pages[page_idx], page_idx);
    }

    //  Banks FC-FF ROM access is read-only of course.
    page_map = &mmio->fpi_rom_page_map;
    for (page_idx = 0x00; page_idx < 0x100; ++page_idx) {
        _clem_mmio_create_page_direct_mapping(
            &page_map->pages[page_idx], page_idx);
        page_map->pages[page_idx].flags &= ~CLEM_MMIO_PAGE_WRITE_OK;
    }

    //  Bank 00
    page_map = &mmio->fpi_main_page_map;
    for (page_idx = 0x00; page_idx < 0xC0; ++page_idx) {
        _clem_mmio_create_page_direct_mapping(
            &page_map->pages[page_idx], page_idx);
    }
    for (page_idx = 0xC0; page_idx < 0xCF; ++page_idx) {
        _clem_mmio_create_page_direct_mapping(
            &page_map->pages[page_idx], page_idx);
        page_map->pages[page_idx].bank_read = 0xe0;
        page_map->pages[page_idx].bank_write = 0xe0;
        page_map->pages[page_idx].flags |= CLEM_MMIO_PAGE_IOADDR;
        page_map->pages[page_idx].flags &= ~CLEM_MMIO_PAGE_DIRECT;
    }
    for (page_idx = 0xD0; page_idx < 0xDF; ++page_idx) {
        _clem_mmio_create_page_direct_mapping(
            &page_map->pages[page_idx], page_idx);
        page_map->pages[page_idx].bank_read = 0xff;
        page_map->pages[page_idx].bank_write = 0x00;
        page_map->pages[page_idx].flags &= ~CLEM_MMIO_PAGE_DIRECT;
    }
    for (page_idx = 0xE0; page_idx < 0x100; ++page_idx) {
        _clem_mmio_create_page_direct_mapping(
            &page_map->pages[page_idx], page_idx);
        page_map->pages[page_idx].bank_read = 0xff;
        page_map->pages[page_idx].bank_write = 0x00;
        page_map->pages[page_idx].flags &= ~CLEM_MMIO_PAGE_DIRECT;
    }



    //  set up the default page mappings
    mmio->bank_page_map[0x00] = &mmio->fpi_main_page_map;
    for (bank_idx = 0x02; bank_idx < 0x80; ++bank_idx) {
        mmio->bank_page_map[bank_idx] = &mmio->fpi_direct_page_map;
    }
    for (bank_idx = 0xFC; bank_idx < 0x100; ++bank_idx) {
        mmio->bank_page_map[bank_idx] = &mmio->fpi_rom_page_map;
    }
    mmio->bank_page_map[0x00] = &mmio->fpi_main_page_map;
}
