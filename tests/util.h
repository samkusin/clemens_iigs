#ifndef CLEM_TEST_UTIL_H
#define CLEM_TEST_UTIL_H

#include "clem_types.h"

#define CLEM_TEST_NUM_FPI_BANKS 4

typedef struct {
    uint8_t g_test_rom[CLEM_IIGS_ROM3_SIZE];
    uint8_t g_e0_ram[CLEM_IIGS_BANK_SIZE];
    uint8_t g_e1_ram[CLEM_IIGS_BANK_SIZE];
    uint8_t g_fpi_ram[CLEM_IIGS_BANK_SIZE * CLEM_TEST_NUM_FPI_BANKS];
} ClemensTestMemory;

extern int clem_test_init_machine(
    ClemensMachine* machine,
    ClemensTestMemory* memory,
    const char* rom_pathname);

/** Initialized machine with a trivial ROM */
extern int clem_test_init_machine_trivial_rom(
    ClemensMachine* machine,
    ClemensTestMemory* memory);

#endif
