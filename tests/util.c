#include "util.h"
#include "emulator.h"

#include <stdio.h>
#include <string.h>


int clem_test_init_machine(
    ClemensMachine* machine,
    ClemensTestMemory* memory,
    const char* rom_pathname
) {
    FILE* fp = fopen(rom_pathname, "rb");
    unsigned sz;
    if (!fp) {
        return 1;
    }
    fseek(fp, 0, SEEK_END);
    sz = (unsigned)ftell(fp);
    if (sz > sizeof(memory->g_test_rom)) {
        return 1;
    }
    if ((sz % CLEM_IIGS_BANK_SIZE) != 0) {
        return 1;
    }
    fseek(fp, 0, SEEK_SET);
    fread(memory->g_test_rom + sizeof(memory->g_test_rom) - sz, 1, sz, fp);

    return clemens_init(machine,
                        CLEM_TEST_SLOW_CYCLE_CLOCK_COUNT,
                        CLEM_TEST_CYCLE_CLOCK_COUNT,
                        memory->g_test_rom, sizeof(memory->g_test_rom),
                        memory->g_e0_ram, memory->g_e1_ram,
                        memory->g_fpi_ram, CLEM_TEST_NUM_FPI_BANKS);
}

int clem_test_init_machine_trivial_rom(
    ClemensMachine* machine,
    ClemensTestMemory* memory
) {
    uint8_t* bank;
    uint16_t adr;

    bank = &memory->g_test_rom[CLEM_IIGS_BANK_SIZE * 3];
    adr = (CLEM_6502_RESET_VECTOR_HI_ADDR << 8) |
          CLEM_6502_RESET_VECTOR_LO_ADDR;
    bank[adr] = 0x00;
    bank[adr + 1] = 0xfe;
    adr = 0xfe00;
    bank[adr] = CLEM_OPC_NOP;
    bank[adr + 1] = CLEM_OPC_STP;
    adr = 0xe000;
    bank[adr] = CLEM_TEST_TRIVIAL_ROM_MARK_E000;
    bank[adr + 1] = CLEM_TEST_TRIVIAL_ROM_MARK_E001;
    bank[adr + 2] = CLEM_TEST_TRIVIAL_ROM_MARK_E002;
    bank[adr + 3] = CLEM_TEST_TRIVIAL_ROM_MARK_E003;
    adr = 0xd000;
    bank[adr] = CLEM_TEST_TRIVIAL_ROM_MARK_D000;
    bank[adr + 1] = CLEM_TEST_TRIVIAL_ROM_MARK_D001;
    bank[adr + 2] = CLEM_TEST_TRIVIAL_ROM_MARK_D002;
    bank[adr + 3] = CLEM_TEST_TRIVIAL_ROM_MARK_D003;

    return clemens_init(machine,
                        CLEM_TEST_SLOW_CYCLE_CLOCK_COUNT,
                        CLEM_TEST_CYCLE_CLOCK_COUNT,
                        memory->g_test_rom, sizeof(memory->g_test_rom),
                        memory->g_e0_ram, memory->g_e1_ram,
                        memory->g_fpi_ram, CLEM_TEST_NUM_FPI_BANKS);
}