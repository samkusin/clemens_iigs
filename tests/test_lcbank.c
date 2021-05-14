#include "munit/munit.h"

#include "util.h"
#include "emulator.h"
#include "clem_mem.h"
#include "clem_mmio_defs.h"

/*
 Prerequisites:
 * An initialized ClemensMachine structure with a trivial ROM
 * MMIO has been initialized
 * Reset cycle has completed

 Approach:
 * Invoke clem_read/clem_write as needed to trigger the LCBANK2 softswitches
 * Perform a read or write action appropriate to the test
 * Verify read/write worked by checking raw memory

 */

static ClemensMachine g_test_machine;
static ClemensTestMemory g_test_memory;


static void* test_fixture_setup(
    const MunitParameter params[],
    void* data
) {
    uint16_t adr;
    uint8_t* bank;

    memset(&g_test_machine, 0, sizeof(g_test_machine));
    memset(&g_test_memory, 0, sizeof(g_test_memory));

    clem_test_init_machine_trivial_rom(&g_test_machine, &g_test_memory);

    g_test_machine.cpu.pins.resbIn = false;
    clemens_emulate(&g_test_machine);
    g_test_machine.cpu.pins.resbIn = true;
    clemens_emulate(&g_test_machine);
    munit_assert_int(g_test_machine.cpu.state_type, ==, kClemensCPUStateType_Execute);
    munit_assert_true(clemens_is_mmio_initialized(&g_test_machine));

    return &g_test_machine;
}

static void test_fixture_teardown(
    void* data
) {
    ClemensMachine* clem = (ClemensMachine*)data;
}

static MunitResult test_lcbank_on_reset_default(
    const MunitParameter params[],
    void* data
) {
    ClemensMachine* machine = (ClemensMachine*)data;
    uint8_t value;
    char long_data[4];
    uint8_t* bank_mem;

    clem_read(machine, &value, CLEM_TEST_IOADDR(LC_BANK_TEST), 0x00,
              CLEM_MEM_FLAG_DATA);
    munit_assert_true(value & 0x80);    // Bank 2
    clem_read(machine, &value, CLEM_TEST_IOADDR(ROM_RAM_TEST), 0x00,
              CLEM_MEM_FLAG_DATA);
    munit_assert_false(value & 0x80);   // ROM
    clem_read(machine, &value, CLEM_TEST_IOADDR(RDALTZP_TEST), 0x00,
              CLEM_MEM_FLAG_DATA);
    munit_assert_false(value & 0x80);   // Bank 0 RAM

    /* Confirm write enable in the LC space ($E000-FFFF) */
    strncpy(long_data, "dead", sizeof(long_data));
    bank_mem = machine->fpi_bank_map[0x00];
    test_write(machine, long_data, sizeof(long_data), 0xE000, 0x00);
    munit_assert_memory_equal(sizeof(long_data), long_data, &bank_mem[0xE000]);
    /* Confirm ROM read in the LC space */
    memset(long_data, 0, sizeof(long_data));
    bank_mem = machine->fpi_bank_map[0xff];
    test_read(machine, long_data, sizeof(long_data), 0xE000, 0x00);
    munit_assert_memory_equal(sizeof(long_data), long_data, &bank_mem[0xE000]);

    /* Confirm write enable in the LC Bank 2 sapce ($D000-$DFFF), which
       maps to FPI RAM [0x00][0xD000-0xDFFF] actual */
    strncpy(long_data, "beef", sizeof(long_data));
    bank_mem = machine->fpi_bank_map[0x00];
    test_write(machine, long_data, sizeof(long_data), 0xD000, 0x00);
    munit_assert_memory_equal(sizeof(long_data), long_data, &bank_mem[0xD000]);
    /* Confirm ROM read in the LC space */
    memset(long_data, 0, sizeof(long_data));
    bank_mem = machine->fpi_bank_map[0xff];
    test_read(machine, long_data, sizeof(long_data), 0xD000, 0x00);
    munit_assert_memory_equal(sizeof(long_data), long_data, &bank_mem[0xD000]);

    return MUNIT_OK;
}

static MunitResult test_lcbank_bank1_writes(
    const MunitParameter params[],
    void* data
) {
    ClemensMachine* machine = (ClemensMachine*)data;
    uint8_t value;
    char long_data[4];
    uint8_t* bank_mem;

    clem_read(machine, &value, CLEM_TEST_IOADDR(ROM_RAM_TEST), 0x00,
              CLEM_MEM_FLAG_DATA);
    munit_assert_false(value & 0x80);   // ROM Reads

    //  Bank 2 to Bank 1
    clem_read(machine, &value, CLEM_TEST_IOADDR(LC_BANK_TEST), 0x00,
              CLEM_MEM_FLAG_DATA);
    munit_assert_true(value & 0x80);    // Bank 2
    clem_read(machine, &value, CLEM_TEST_IOADDR(LC1_ROM_WP), 0x00,
              CLEM_MEM_FLAG_DATA);
    clem_read(machine, &value, CLEM_TEST_IOADDR(LC_BANK_TEST), 0x00,
              CLEM_MEM_FLAG_DATA);
    munit_assert_false(value & 0x80);   // Bank 1

    /* Verify RAM in LC + Bank 1 is write protected */
    strncpy(long_data, "dead", sizeof(long_data));
    bank_mem = machine->fpi_bank_map[0x00];
    test_write(machine, long_data, sizeof(long_data), 0xE000, 0x00);
    munit_assert_memory_not_equal(sizeof(long_data), long_data, &bank_mem[0xE000]);
    /* Confirm write protect in the LC Bank 1 sapce ($D000-$DFFF), which
       maps to FPI RAM [0x00][0xC000-0xCFFF] actual */
    strncpy(long_data, "beef", sizeof(long_data));
    bank_mem = machine->fpi_bank_map[0x00];
    test_write(machine, long_data, sizeof(long_data), 0xD000, 0x00);
    munit_assert_memory_not_equal(sizeof(long_data), long_data, &bank_mem[0xC000]);

    /* Write enable and repeat the above */
    clem_read(machine, &value, CLEM_TEST_IOADDR(LC1_ROM_WE), 0x00,
              CLEM_MEM_FLAG_DATA);
    clem_read(machine, &value, CLEM_TEST_IOADDR(LC1_ROM_WE), 0x00,
              CLEM_MEM_FLAG_DATA);

    test_write(machine, long_data, sizeof(long_data), 0xD000, 0x00);
    munit_assert_memory_equal(sizeof(long_data), long_data, &bank_mem[0xC000]);

    return MUNIT_OK;
}

static MunitResult test_lcbank_bank1_reads(
    const MunitParameter params[],
    void* data
) {
    ClemensMachine* machine = (ClemensMachine*)data;
    uint8_t value;
    char long_data[4];
    uint8_t* bank_mem;

    clem_read(machine, &value, CLEM_TEST_IOADDR(ROM_RAM_TEST), 0x00,
              CLEM_MEM_FLAG_DATA);
    munit_assert_false(value & 0x80);   // ROM Reads

    //  Bank 2 to Bank 1
    clem_read(machine, &value, CLEM_TEST_IOADDR(LC_BANK_TEST), 0x00,
              CLEM_MEM_FLAG_DATA);
    munit_assert_true(value & 0x80);    // Bank 2
    clem_read(machine, &value, CLEM_TEST_IOADDR(LC1_RAM_WE), 0x00,
              CLEM_MEM_FLAG_DATA);
    clem_read(machine, &value, CLEM_TEST_IOADDR(LC1_RAM_WE), 0x00,
              CLEM_MEM_FLAG_DATA);
    clem_read(machine, &value, CLEM_TEST_IOADDR(LC_BANK_TEST), 0x00,
              CLEM_MEM_FLAG_DATA);
    munit_assert_false(value & 0x80);   // Bank 1

    /* Verify RAM in LC + Bank 1 is R/W enabled($D0=$DF pages), which
       maps to FPI RAM [0x00][0xC000-0xCFFF] actual */
    strncpy(long_data, "dead", sizeof(long_data));
    bank_mem = machine->fpi_bank_map[0x00];
    test_write(machine, long_data, sizeof(long_data), 0xD000, 0x00);
    munit_assert_memory_equal(sizeof(long_data), long_data, &bank_mem[0xC000]);
    memset(long_data, 0, sizeof(long_data));
    test_read(machine, long_data, sizeof(long_data), 0xD000, 0x00);
    munit_assert_memory_equal(sizeof(long_data), long_data, &bank_mem[0xC000]);

    /* Verify RAM in LC is R/W enabled in the LC sapce ($E0-$EF pages) */
    strncpy(long_data, "beef", sizeof(long_data));
    bank_mem = machine->fpi_bank_map[0x00];
    test_write(machine, long_data, sizeof(long_data), 0xE000, 0x00);
    munit_assert_memory_equal(sizeof(long_data), long_data, &bank_mem[0xE000]);
    memset(long_data, 0, sizeof(long_data));
    test_read(machine, long_data, sizeof(long_data), 0xE000, 0x00);
    munit_assert_memory_equal(sizeof(long_data), long_data, &bank_mem[0xE000]);

    /* ROM enable, and read from Bank 1 ROM */
    /* TODO: what does LC bank 1 ROM map to in FPI space?

    clem_read(machine, &value, CLEM_TEST_IOADDR(LC1_ROM_WE), 0x00,
              CLEM_MEM_FLAG_DATA);
    clem_read(machine, &value, CLEM_TEST_IOADDR(LC1_ROM_WE), 0x00,
              CLEM_MEM_FLAG_DATA);
    memset(long_data, 0, sizeof(long_data));
    test_read(machine, long_data, sizeof(long_data), 0xD000, 0x00);
    munit_assert_memory_equal(sizeof(long_data), long_data, &bank_mem[0xC000]);
    */
    return MUNIT_OK;
}

static MunitResult test_lcbank_bank2_writes(
    const MunitParameter params[],
    void* data
) {
    ClemensMachine* machine = (ClemensMachine*)data;
    uint8_t value;
    char long_data[4];
    uint8_t* bank_mem;

    clem_read(machine, &value, CLEM_TEST_IOADDR(ROM_RAM_TEST), 0x00,
              CLEM_MEM_FLAG_DATA);
    munit_assert_false(value & 0x80);   // ROM Reads

    //  Bank 2
    clem_read(machine, &value, CLEM_TEST_IOADDR(LC2_ROM_WP), 0x00,
              CLEM_MEM_FLAG_DATA);
    clem_read(machine, &value, CLEM_TEST_IOADDR(LC_BANK_TEST), 0x00,
              CLEM_MEM_FLAG_DATA);
    munit_assert_true(value & 0x80);   // Bank 2

    /* Verify RAM in LC + Bank 2 is write protected */
    strncpy(long_data, "dead", sizeof(long_data));
    bank_mem = machine->fpi_bank_map[0x00];
    test_write(machine, long_data, sizeof(long_data), 0xE000, 0x00);
    munit_assert_memory_not_equal(sizeof(long_data), long_data, &bank_mem[0xE000]);
    /* Confirm write protect in the LC Bank 2 sapce ($D000-$DFFF), which
       maps to FPI RAM [0x00][0xD0-0xDF pages] actual */
    strncpy(long_data, "beef", sizeof(long_data));
    bank_mem = machine->fpi_bank_map[0x00];
    test_write(machine, long_data, sizeof(long_data), 0xD000, 0x00);
    munit_assert_memory_not_equal(sizeof(long_data), long_data, &bank_mem[0xD000]);

    /* Write enable and repeat the above */
    clem_read(machine, &value, CLEM_TEST_IOADDR(LC2_ROM_WE), 0x00,
              CLEM_MEM_FLAG_DATA);
    clem_read(machine, &value, CLEM_TEST_IOADDR(LC2_ROM_WE), 0x00,
              CLEM_MEM_FLAG_DATA);
    test_write(machine, long_data, sizeof(long_data), 0xD000, 0x00);
    munit_assert_memory_equal(sizeof(long_data), long_data, &bank_mem[0xD000]);

    return MUNIT_OK;
}

static MunitResult test_lcbank_bank2_reads(
    const MunitParameter params[],
    void* data
) {
    ClemensMachine* machine = (ClemensMachine*)data;
    uint8_t value;
    char long_data[4];
    uint8_t* bank_mem;

    clem_read(machine, &value, CLEM_TEST_IOADDR(ROM_RAM_TEST), 0x00,
              CLEM_MEM_FLAG_DATA);
    munit_assert_false(value & 0x80);   // ROM Reads

    //  Bank 2 Write Protect, Read Enable
    clem_read(machine, &value, CLEM_TEST_IOADDR(LC2_RAM_WP), 0x00,
              CLEM_MEM_FLAG_DATA);
    clem_read(machine, &value, CLEM_TEST_IOADDR(LC_BANK_TEST), 0x00,
              CLEM_MEM_FLAG_DATA);
    munit_assert_true(value & 0x80);   // Bank 2

    /* Verify RAM in LC + Bank 2 is R/W enabled($D0-$DF pages), which
       maps to FPI RAM [0x00][0xC0-0xCF pages] actual */
    bank_mem = machine->fpi_bank_map[0x00];
    strncpy(long_data, "dead", sizeof(long_data));
    test_write(machine, long_data, sizeof(long_data), 0xD000, 0x00);
    munit_assert_memory_not_equal(sizeof(long_data), long_data, &bank_mem[0xD000]);

    strncpy(long_data, "beef", sizeof(long_data));
    test_write(machine, long_data, sizeof(long_data), 0xE000, 0x00);
    munit_assert_memory_not_equal(sizeof(long_data), long_data, &bank_mem[0xE000]);

    /* Bank 2 Write Enable */
    clem_read(machine, &value, CLEM_TEST_IOADDR(LC2_RAM_WE), 0x00,
              CLEM_MEM_FLAG_DATA);
    clem_read(machine, &value, CLEM_TEST_IOADDR(LC2_RAM_WE), 0x00,
              CLEM_MEM_FLAG_DATA);

    strncpy(long_data, "dead", sizeof(long_data));
    test_write(machine, long_data, sizeof(long_data), 0xD000, 0x00);
    munit_assert_memory_equal(sizeof(long_data), long_data, &bank_mem[0xD000]);
    memset(long_data, 0, sizeof(long_data));
    test_read(machine, long_data, sizeof(long_data), 0xD000, 0x00);
    munit_assert_memory_equal(sizeof(long_data), long_data, &bank_mem[0xD000]);
    strncpy(long_data, "beef", sizeof(long_data));
    test_write(machine, long_data, sizeof(long_data), 0xE000, 0x00);
    munit_assert_memory_equal(sizeof(long_data), long_data, &bank_mem[0xE000]);
    memset(long_data, 0, sizeof(long_data));
    test_read(machine, long_data, sizeof(long_data), 0xE000, 0x00);
    munit_assert_memory_equal(sizeof(long_data), long_data, &bank_mem[0xE000]);

    /* ROM enable, and read from LC + Bank 2 ROM */
    clem_read(machine, &value, CLEM_TEST_IOADDR(LC2_ROM_WE), 0x00,
              CLEM_MEM_FLAG_DATA);
    clem_read(machine, &value, CLEM_TEST_IOADDR(LC2_ROM_WE), 0x00,
              CLEM_MEM_FLAG_DATA);
    bank_mem = machine->fpi_bank_map[0xff];
    memset(long_data, 0, sizeof(long_data));
    test_read(machine, long_data, sizeof(long_data), 0xD000, 0x00);
    munit_assert_memory_equal(sizeof(long_data), long_data, &bank_mem[0xD000]);
    munit_assert_uint8(long_data[0], ==, CLEM_TEST_TRIVIAL_ROM_MARK_D000);
    munit_assert_uint8(long_data[1], ==, CLEM_TEST_TRIVIAL_ROM_MARK_D001);
    munit_assert_uint8(long_data[2], ==, CLEM_TEST_TRIVIAL_ROM_MARK_D002);
    munit_assert_uint8(long_data[3], ==, CLEM_TEST_TRIVIAL_ROM_MARK_D003);

    memset(long_data, 0, sizeof(long_data));
    test_read(machine, long_data, sizeof(long_data), 0xE000, 0x00);
    munit_assert_memory_equal(sizeof(long_data), long_data, &bank_mem[0xE000]);
    munit_assert_uint8(long_data[0], ==, CLEM_TEST_TRIVIAL_ROM_MARK_E000);
    munit_assert_uint8(long_data[1], ==, CLEM_TEST_TRIVIAL_ROM_MARK_E001);
    munit_assert_uint8(long_data[2], ==, CLEM_TEST_TRIVIAL_ROM_MARK_E002);
    munit_assert_uint8(long_data[3], ==, CLEM_TEST_TRIVIAL_ROM_MARK_E003);

    return MUNIT_OK;
}

static MunitResult test_lcbank_altzp_stdzp(
    const MunitParameter params[],
    void* data
) {
    ClemensMachine* machine = (ClemensMachine*)data;
    uint8_t value;
    char long_data[4];
    uint8_t* bank_mem_00;
    uint8_t* bank_mem_01;
    uint8_t* bank_mem_ff;

    /* Altzp first - then switch to stdzp */
    clem_write(machine, 0x01, CLEM_TEST_IOADDR(ALTZP), 0x00, CLEM_MEM_FLAG_DATA);
    clem_read(machine, &value, CLEM_TEST_IOADDR(RDALTZP_TEST), 0x00,
              CLEM_MEM_FLAG_DATA);
    munit_assert_true(value & 0x80);   // Aux bank

    /* Verify write and read to page 0, 1 */
    strncpy(long_data, "food", sizeof(long_data));
    bank_mem_00 = machine->fpi_bank_map[0x00];
    bank_mem_01 = machine->fpi_bank_map[0x01];
    test_write(machine, long_data, sizeof(long_data), 0x0000, 0x00);
    munit_assert_memory_not_equal(sizeof(long_data), long_data, &bank_mem_00[0x0000]);
    munit_assert_memory_equal(sizeof(long_data), long_data, &bank_mem_01[0x0000]);
    test_write(machine, long_data, sizeof(long_data), 0x0100, 0x00);
    munit_assert_memory_not_equal(sizeof(long_data), long_data, &bank_mem_00[0x0100]);
    munit_assert_memory_equal(sizeof(long_data), long_data, &bank_mem_01[0x0100]);
    memset(long_data, 0, sizeof(long_data));
    test_read(machine, long_data, sizeof(long_data), 0x0100, 0x00);
    munit_assert_memory_equal(sizeof(long_data), long_data, &bank_mem_01[0x0100]);

    /* Not RAMRD, so writes to main bank memory*/
    strncpy(long_data, "food", sizeof(long_data));
    test_write(machine, long_data, sizeof(long_data), 0x1000, 0x00);
    munit_assert_memory_equal(sizeof(long_data), long_data, &bank_mem_00[0x1000]);
    munit_assert_memory_not_equal(sizeof(long_data), long_data, &bank_mem_01[0x1000]);

    /* Test Read and Write to aux lc  (bank 01) */
    clem_read(machine, &value, CLEM_TEST_IOADDR(LC_BANK_TEST), 0x00,
              CLEM_MEM_FLAG_DATA);
    munit_assert_true(value & 0x80);    // Bank 2 Selected
    clem_read(machine, &value, CLEM_TEST_IOADDR(ROM_RAM_TEST), 0x00,
              CLEM_MEM_FLAG_DATA);
    munit_assert_false(value & 0x80);   // ROM read

    /* Assumption here is bank 02 ROM Read/RAM WE */
    /* Confirm ROM read in the LC space should be same as STDZP */
    memset(long_data, 0, sizeof(long_data));
    bank_mem_ff = machine->fpi_bank_map[0xff];
    test_read(machine, long_data, sizeof(long_data), 0xD000, 0x00);
    munit_assert_memory_equal(sizeof(long_data), long_data, &bank_mem_ff[0xD000]);

    strncpy(long_data, "beet", sizeof(long_data));
    test_write(machine, long_data, sizeof(long_data), 0xD000, 0x00);
    munit_assert_memory_equal(sizeof(long_data), long_data, &bank_mem_01[0xD000]);
    memset(long_data, 0, sizeof(long_data));
    test_read(machine, long_data, sizeof(long_data), 0xD000, 0x00);
    munit_assert_memory_not_equal(sizeof(long_data), long_data, &bank_mem_01[0xD000]);

    /* Now the RAM read should work */
    clem_read(machine, &value, CLEM_TEST_IOADDR(LC2_RAM_WE), 0x00,
              CLEM_MEM_FLAG_DATA);
    clem_read(machine, &value, CLEM_TEST_IOADDR(LC2_RAM_WE), 0x00,
              CLEM_MEM_FLAG_DATA);

    memset(long_data, 0, sizeof(long_data));
    test_read(machine, long_data, sizeof(long_data), 0xD000, 0x00);
    munit_assert_memory_equal(sizeof(long_data), long_data, &bank_mem_01[0xD000]);

    /* Switch to StdZP */
    clem_write(machine, 0x01, CLEM_TEST_IOADDR(STDZP), 0x00, CLEM_MEM_FLAG_DATA);
    clem_read(machine, &value, CLEM_TEST_IOADDR(RDALTZP_TEST), 0x00,
              CLEM_MEM_FLAG_DATA);
    munit_assert_false(value & 0x80);   // Std/Main bank

    strncpy(long_data, "wozi", sizeof(long_data));
    test_write(machine, long_data, sizeof(long_data), 0x0000, 0x00);
    munit_assert_memory_equal(sizeof(long_data), long_data, &bank_mem_00[0x0000]);
    munit_assert_memory_not_equal(sizeof(long_data), long_data, &bank_mem_01[0x0000]);
    test_write(machine, long_data, sizeof(long_data), 0x0100, 0x00);
    munit_assert_memory_equal(sizeof(long_data), long_data, &bank_mem_00[0x0100]);
    munit_assert_memory_not_equal(sizeof(long_data), long_data, &bank_mem_01[0x0100]);
    memset(long_data, 0, sizeof(long_data));
    test_read(machine, long_data, sizeof(long_data), 0x0100, 0x00);
    munit_assert_memory_equal(sizeof(long_data), long_data, &bank_mem_00[0x0100]);

    strncpy(long_data, "twai", sizeof(long_data));
    test_write(machine, long_data, sizeof(long_data), 0xD000, 0x00);
    munit_assert_memory_equal(sizeof(long_data), long_data, &bank_mem_00[0xD000]);
    memset(long_data, 0, sizeof(long_data));
    test_read(machine, long_data, sizeof(long_data), 0xD000, 0x00);
    munit_assert_memory_equal(sizeof(long_data), long_data, &bank_mem_00[0xD000]);

    return MUNIT_OK;
}

static MunitTest clem_tests[] = {
    {
        (char*)"/lcbank/default", test_lcbank_on_reset_default,
        test_fixture_setup,
        test_fixture_teardown,
        MUNIT_TEST_OPTION_NONE,
        NULL
    },
    {
        (char*)"/lcbank/b1writes", test_lcbank_bank1_writes,
        test_fixture_setup,
        test_fixture_teardown,
        MUNIT_TEST_OPTION_NONE,
        NULL
    },
    {
        (char*)"/lcbank/b1reads", test_lcbank_bank1_reads,
        test_fixture_setup,
        test_fixture_teardown,
        MUNIT_TEST_OPTION_NONE,
        NULL
    },
    {
        (char*)"/lcbank/b2writes", test_lcbank_bank2_writes,
        test_fixture_setup,
        test_fixture_teardown,
        MUNIT_TEST_OPTION_NONE,
        NULL
    },
    {
        (char*)"/lcbank/b2reads", test_lcbank_bank2_reads,
        test_fixture_setup,
        test_fixture_teardown,
        MUNIT_TEST_OPTION_NONE,
        NULL
    },
    {
        (char*)"/lcbank/altzp_stdzp", test_lcbank_altzp_stdzp,
        test_fixture_setup,
        test_fixture_teardown,
        MUNIT_TEST_OPTION_NONE,
        NULL
    },
    { NULL, NULL, NULL, NULL, MUNIT_TEST_OPTION_NONE, NULL }
};

static MunitSuite clem_suite = {
    (char*)"",
    clem_tests,
    NULL,
    1,
    MUNIT_SUITE_OPTION_NONE
};

int main(int argc, char* argv[]) {
  /* Finally, we'll actually run our test suite!  That second argument
   * is the user_data parameter which will be passed either to the
   * test or (if provided) the fixture setup function. */
  return munit_suite_main(&clem_suite, NULL, argc, argv);
}
