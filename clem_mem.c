#include "clem_mem.h"
#include "clem_debug.h"
#include "clem_util.h"

static inline void _clem_mem_cycle(ClemensMachine *clem, bool mega2_access) {
    // TODO: trying to avoid keeping track of two clock counters (PHI2 and PHI0)
    //       for now, using a single PHI2 and sync to a PHI0 cycle if accessing
    //       the mega2.
    if (mega2_access) {
        // SYNC? seems to slow the system down entirely when I/O is accessed (even speaker audio)
        // clem->tspec.clocks_spent += (clem->tspec.clocks_spent % clem->tspec.clocks_step_mega2);
        clem->tspec.clocks_spent += clem->tspec.clocks_step_mega2;
    } else {
        clem->tspec.clocks_spent += clem->tspec.clocks_step;
    }
    ++clem->cpu.cycles_spent;
}

void clem_mem_create_page_mapping(struct ClemensMemoryPageInfo *page, uint8_t page_idx,
                                  uint8_t bank_read_idx, uint8_t bank_write_idx) {
    page->flags = CLEM_MEM_PAGE_WRITEOK_FLAG;
    page->bank_read = bank_read_idx;
    page->read = page_idx;
    page->bank_write = bank_write_idx;
    page->write = page_idx;
}

void clem_read(ClemensMachine *clem, uint8_t *data, uint16_t adr, uint8_t bank, uint8_t flags) {
    struct ClemensMemoryPageMap *bank_page_map = clem->mem.bank_page_map[bank];
    struct ClemensMemoryPageInfo *page = &bank_page_map->pages[adr >> 8];
    uint16_t offset = ((uint16_t)page->read << 8) | (adr & 0xff);
    bool read_only = (flags == CLEM_MEM_FLAG_NULL);
    bool mega2_access = false;
    bool io_access = false;

    // TODO: store off if read_reg has a read_count of 1 here
    //       reset it automatically if true at the end of this function
    if (page->flags & CLEM_MEM_IO_MEMORY_MASK) {
        unsigned slot_idx;
        io_access = true;
        if (page->flags & CLEM_MEM_PAGE_IOADDR_FLAG) {
            if (offset >= 0xc071 && offset < 0xc080) {
                *data = clem->mem.fpi_bank_map[0xff][offset];
                io_access = false;
            } else {
                *data = (*clem->mem.mmio_read)(&clem->mem, &clem->tspec, offset,
                                               read_only ? CLEM_OP_IO_NO_OP : 0, &mega2_access);
            }
        } else if (page->flags & CLEM_MEM_PAGE_CARDMEM_FLAG) {
            *data = (*clem->mem.mmio_read)(
                &clem->mem, &clem->tspec, ((uint16_t)page->read << 8) | (adr & 0xff),
                (read_only ? CLEM_OP_IO_NO_OP : 0) | CLEM_OP_IO_CARD, &mega2_access);
        } else {
            CLEM_ASSERT(false);
        }

    } else if (!(page->flags & CLEM_MEM_PAGE_TYPE_MASK) ||
               (page->flags & CLEM_MEM_PAGE_BANK_MASK)) {
        uint8_t *bank_mem;
        uint8_t bank_actual;
        if (page->flags & CLEM_MEM_PAGE_DIRECT_FLAG) {
            bank_actual = bank;
        } else if (page->flags & CLEM_MEM_PAGE_MAINAUX_FLAG) {
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
        clem->cpu.pins.ioOut = io_access;
        _clem_mem_cycle(clem, mega2_access);
    }
}

void clem_write(ClemensMachine *clem, uint8_t data, uint16_t adr, uint8_t bank, uint8_t mem_flags) {
    struct ClemensMemoryPageMap *bank_page_map = clem->mem.bank_page_map[bank];
    struct ClemensMemoryShadowMap *shadow_map = bank_page_map->shadow_map;
    struct ClemensMemoryPageInfo *page = &bank_page_map->pages[adr >> 8];
    uint16_t offset = ((uint16_t)page->write << 8) | (adr & 0xff);
    uint8_t flags = mem_flags == CLEM_MEM_FLAG_NULL ? CLEM_OP_IO_NO_OP : 0;
    bool mega2_access = false;
    bool io_access = false;

    if (page->flags & CLEM_MEM_IO_MEMORY_MASK) {
        unsigned slot_idx;
        if (page->flags & CLEM_MEM_PAGE_IOADDR_FLAG) {
            if (page->flags & CLEM_MEM_PAGE_WRITEOK_FLAG) {
                (*clem->mem.mmio_write)(&clem->mem, &clem->tspec, data, offset, flags,
                                        &mega2_access);
            } else {
                mega2_access = true;
            }
        } else if (page->flags & CLEM_MEM_PAGE_CARDMEM_FLAG) {
            (*clem->mem.mmio_write)(&clem->mem, &clem->tspec, data,
                                    ((uint16_t)page->write << 8) | (adr & 0xff),
                                    flags | CLEM_OP_IO_CARD, &mega2_access);
        } else {
            CLEM_ASSERT(false);
        }
        io_access = true;
    } else if (!(page->flags & CLEM_MEM_PAGE_TYPE_MASK) ||
               (page->flags & CLEM_MEM_PAGE_BANK_MASK)) {
        uint8_t *bank_mem;
        uint8_t bank_actual;

        if (page->flags & CLEM_MEM_PAGE_DIRECT_FLAG) {
            bank_actual = bank;
        } else if (page->flags & CLEM_MEM_PAGE_MAINAUX_FLAG) {
            bank_actual = (bank & 0xfe) | (page->bank_write & 0x1);
        } else {
            bank_actual = page->bank_write;
        }
        bank_mem = _clem_get_memory_bank(clem, bank_actual, &mega2_access);
        if (page->flags & CLEM_MEM_PAGE_WRITEOK_FLAG) {
            bank_mem[offset] = data;
        }
        if (shadow_map && shadow_map->pages[page->write]) {
            bank_mem = _clem_get_memory_bank(clem, (0xE0) | (bank_actual & 0x1), &mega2_access);
            if (page->flags & CLEM_MEM_PAGE_WRITEOK_FLAG) {
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
        clem->cpu.pins.ioOut = io_access;
        _clem_mem_cycle(clem, mega2_access);
    }
}
