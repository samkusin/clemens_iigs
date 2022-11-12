#ifndef CLEM_MEM_H
#define CLEM_MEM_H

#include "clem_types.h"

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

#ifdef __cplusplus
extern "C" {
#endif

void clem_mem_create_page_mapping(struct ClemensMemoryPageInfo *page, uint8_t page_idx,
                                  uint8_t bank_read_idx, uint8_t bank_write_idx);

void clem_read(ClemensMachine *clem, uint8_t *data, uint16_t adr, uint8_t bank, uint8_t flags);
void clem_write(ClemensMachine *clem, uint8_t data, uint16_t adr, uint8_t bank, uint8_t flags);

#ifdef __cplusplus
}
#endif

#endif
