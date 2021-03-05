#include "clem_types.h"
#include "clem_debug.h"
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

static inline void _clem_mmio_shadow_c035_set(
    struct ClemensMMIO* mmio,
    uint8_t value
) {
    CLEM_UNIMPLEMENTED("shadow c035 set");
}

static inline uint8_t _clem_mmio_speed_c036(struct ClemensMMIO* mmio) {
    return mmio->speed_c036;
}

static inline void _clem_mmio_speed_c036_set(
    struct ClemensMMIO* mmio,
    uint8_t value
) {
    CLEM_UNIMPLEMENTED("speed c036 set");
}

static inline uint8_t _clem_mmio_statereg_c068(struct ClemensMMIO* mmio) {
    CLEM_UNIMPLEMENTED("statereg c068");
    return 0;
}

static inline uint8_t _clem_mmio_statereg_c068_set(
    struct ClemensMMIO* mmio,
    uint8_t value
) {
    CLEM_UNIMPLEMENTED("statereg c068 set");
}

static uint8_t _clem_mmio_read(
    ClemensMachine* clem,
    uint16_t addr,
    uint32_t* clocks_spent
) {
    uint8_t result = 0x00;
    uint8_t ioreg;
    if (addr >= 0xC100) {
        //  TODO: MMIO slot ROM - it seems this needs to be treated differently
        return 0;
    }
    ioreg = (addr & 0xff);
    switch (ioreg) {
        case CLEM_MMIO_REG_NEWVIDEO:
            result = _clem_mmio_newvideo_c029(&clem->mmio);
            break;
        case CLEM_MMIO_REG_SHADOW:
            result = _clem_mmio_shadow_c035(&clem->mmio);
            break;
        case CLEM_MMIO_REG_SPEED:
            result = _clem_mmio_speed_c036(&clem->mmio);
            break;
        case CLEM_MMIO_REG_STATEREG:
            result = _clem_mmio_statereg_c068(&clem->mmio);
            break;
        default:
            CLEM_UNIMPLEMENTED("ioreg %u", ioreg);
            break;
    }

    return result;
}

static void _clem_mmio_write(
    ClemensMachine* clem,
    uint8_t data,
    uint16_t addr,
    uint32_t* clocks_spent
) {
    uint8_t ioreg;
    if (addr >= 0xC100) {
        //  TODO: MMIO slot ROM - it seems this needs to be treated differently
        return;
    }
    ioreg = (addr & 0xff);
    switch (ioreg) {
        case CLEM_MMIO_REG_NEWVIDEO:
            _clem_mmio_newvideo_c029_set(&clem->mmio, data);
            break;
        case CLEM_MMIO_REG_SHADOW:
            _clem_mmio_shadow_c035_set(&clem->mmio, data);
            break;
        case CLEM_MMIO_REG_SPEED:
            _clem_mmio_speed_c036_set(&clem->mmio, data);
            break;
        case CLEM_MMIO_REG_STATEREG:
            _clem_mmio_statereg_c068_set(&clem->mmio, data);
            break;
        default:
            CLEM_UNIMPLEMENTED("ioreg %u", ioreg);
            break;
    }
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
        uint8_t bank_actual;
        if (page->flags & CLEM_MMIO_PAGE_DIRECT) {
            bank_actual = bank;
        } else if (page->flags & CLEM_MMIO_PAGE_MAINAUX) {
            bank_actual = (bank & 0xfe) | (page->bank_read & 0x1);
        } else {
            bank_actual = page->bank_read;
        }
        bank_mem = _clem_get_memory_bank(clem, bank_actual, &clocks_spent);
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
    struct ClemensMMIOShadowMap* shadow_map = bank_page_map->shadow_map;
    struct ClemensMMIOPageInfo* page = &bank_page_map->pages[adr >> 8];
    uint32_t clocks_spent;
    uint16_t offset = ((uint16_t)page->write << 8) | (adr & 0xff);
    if (page->flags & CLEM_MMIO_PAGE_IOADDR) {
        _clem_mmio_write(clem, data, offset, &clocks_spent);
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
        bank_mem[offset] = data;
        if (shadow_map && shadow_map->pages[page->write]) {
            bank_mem = _clem_get_memory_bank(
                clem, (0xE0) | (bank_actual & 0x1), &clocks_spent);
            bank_mem[offset] = data;
        }
    }
    clem->cpu.pins.adr = adr;
    clem->cpu.pins.bank = bank;
    clem->cpu.pins.data = data;
    clem->clocks_spent += clocks_spent;
    ++clem->cpu.cycles_spent;
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
    if (remap_flags & CLEM_MMIO_MMAP_NIOLC) {
        remap_flags |= CLEM_MMIO_MMAP_LC;
        //  TODO: INTCXROM?
        for (page_idx = 0xC0; page_idx < 0xD0; ++page_idx) {
            page_B00 = &page_map_B00->pages[page_idx];
            page_B01 = &page_map_B01->pages[page_idx];
            _clem_mmio_create_page_mainaux_mapping(page_B00, page_idx, 0x00);
            _clem_mmio_create_page_mainaux_mapping(page_B01, page_idx, 0x01);
            if (!(memory_flags & CLEM_MMIO_MMAP_NIOLC)) {
                page_B00->flags |= CLEM_MMIO_PAGE_IOADDR;
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
                    page_B00->flags &= CLEM_MMIO_PAGE_WRITE_OK;
                    page_B01->flags &= CLEM_MMIO_PAGE_WRITE_OK;
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
                    page_B00->flags &= CLEM_MMIO_PAGE_WRITE_OK;
                    page_B01->flags &= CLEM_MMIO_PAGE_WRITE_OK;
                }
            }
        }
    }
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
    //  Banks E0 - C000-CFFF mapped as IO
    page_map = &mmio->mega2_main_page_map;
    page_map->shadow_map = NULL;
    for (page_idx = 0x00; page_idx < 0x100; ++page_idx) {
        _clem_mmio_create_page_direct_mapping(
            &page_map->pages[page_idx], page_idx);
    }
    for (page_idx = 0xC0; page_idx < 0xD0; ++page_idx) {
        page_map->pages[page_idx].flags &= ~CLEM_MMIO_PAGE_DIRECT;
        page_map->pages[page_idx].flags |= CLEM_MMIO_PAGE_IOADDR;
    }
    //  Banks E1 - C000-CFFF mapped as IO
    page_map = &mmio->mega2_aux_page_map;
    page_map->shadow_map = NULL;
    for (page_idx = 0x00; page_idx < 0x100; ++page_idx) {
        _clem_mmio_create_page_direct_mapping(
            &page_map->pages[page_idx], page_idx);
    }
    for (page_idx = 0xC0; page_idx < 0xD0; ++page_idx) {
        page_map->pages[page_idx].flags &= ~CLEM_MMIO_PAGE_DIRECT;
        page_map->pages[page_idx].flags |= CLEM_MMIO_PAGE_IOADDR;
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

    _clem_mmio_init_page_maps(mmio,
                              CLEM_MMIO_MMAP_WRLCRAM | CLEM_MMIO_MMAP_LCBANK2);

}
