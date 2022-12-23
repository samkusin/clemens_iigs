#ifndef CLEM_MMIO_H
#define CLEM_MMIO_H

#include "clem_types.h"

#ifdef __cplusplus
extern "C" {
#endif

void clem_mmio_reset(ClemensMMIO *mmio, clem_clocks_duration_t mega2_clocks_step);

void clem_mmio_init(ClemensMMIO *mmio, struct ClemensDeviceDebugger *dev_debug,
                    struct ClemensMemoryPageMap **bank_page_map,
                    clem_clocks_duration_t mega2_clocks_step, void *slot_expansion_rom,
                    unsigned int fpi_ram_bank_count);

uint8_t clem_mmio_read(ClemensMMIO *mmio, struct ClemensTimeSpec *tspec, uint16_t addr,
                       uint8_t flags, bool *mega2_access);
void clem_mmio_write(ClemensMMIO *mmio, struct ClemensTimeSpec *tspec, uint8_t data, uint16_t addr,
                     uint8_t flags, bool *mega2_access);

void clem_mmio_restore(ClemensMMIO *mmio);

#ifdef __cplusplus
}
#endif

#endif
