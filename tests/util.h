#ifndef CLEM_TEST_UTIL_H
#define CLEM_TEST_UTIL_H

#include "clem_types.h"

#define CLEM_TEST_NUM_FPI_BANKS             4
#define CLEM_TEST_SLOW_CYCLE_CLOCK_COUNT    2800
#define CLEM_TEST_CYCLE_CLOCK_COUNT         1023

#define CLEM_TEST_LITERAL_NAME(a, b) a ## b
#define CLEM_TEST_IOADDR(_reg_) (0xC000 | CLEM_TEST_LITERAL_NAME(CLEM_MMIO_REG_, _reg_))

/* IIgs */
#define CLEM_TEST_TRIVIAL_ROM_MARK_E000     0x49
#define CLEM_TEST_TRIVIAL_ROM_MARK_E001     0x49
#define CLEM_TEST_TRIVIAL_ROM_MARK_E002     0x67
#define CLEM_TEST_TRIVIAL_ROM_MARK_E003     0x73

/* clem */
#define CLEM_TEST_TRIVIAL_ROM_MARK_D000     0x63
#define CLEM_TEST_TRIVIAL_ROM_MARK_D001     0x6C
#define CLEM_TEST_TRIVIAL_ROM_MARK_D002     0x65
#define CLEM_TEST_TRIVIAL_ROM_MARK_D003     0x6D

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