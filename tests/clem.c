#include "munit/munit.h"

#include "emulator.h"

/* Just for testing purposes in this fixture */
#define CLEM_TEST_NUM_FPI_BANKS     4

static ClemensMachine g_test_machine;

static uint8_t g_test_rom[CLEM_IIGS_ROM3_SIZE];
static uint8_t g_e0_ram[CLEM_IIGS_BANK_SIZE];
static uint8_t g_e1_ram[CLEM_IIGS_BANK_SIZE];
static uint8_t g_fpi_ram[CLEM_IIGS_BANK_SIZE * CLEM_TEST_NUM_FPI_BANKS];


static void* test_clem_fixture_setup(
    const MunitParameter params[],
    void* data
) {
    uint16_t adr;
    uint8_t* bank;

    memset(&g_test_machine, 0, sizeof(g_test_machine));
    memset(g_test_rom, 0, sizeof(g_test_rom));

    bank = &g_test_rom[CLEM_IIGS_BANK_SIZE * 3];
    adr = (CLEM_6502_RESET_VECTOR_HI_ADDR << 8) |
          CLEM_6502_RESET_VECTOR_LO_ADDR;
    bank[adr] = 0x00;
    bank[adr + 1] = 0xfe;
    adr = 0xfe00;
    bank[adr] = CLEM_OPC_NOP;
    bank[adr + 1] = CLEM_OPC_STP;

    return &g_test_machine;
}

static void test_clem_fixture_teardown(
    void* data
) {
    ClemensMachine* clem = (ClemensMachine*)data;
}

static MunitResult test_clem_is_initialized_false(
    const MunitParameter params[],
    void* data
) {
    ClemensMachine* machine = (ClemensMachine*)data;
    munit_assert_false(clemens_is_initialized_simple(machine));
    munit_assert_false(clemens_is_initialized(machine));
    munit_assert_false(clemens_is_mmio_initialized(machine));

    return MUNIT_OK;
}

static MunitResult test_clem_initialize_minimal(
    const MunitParameter params[],
    void* data
) {
    ClemensMachine* machine = (ClemensMachine*)data;
    int init_result = clemens_init(
        machine,
        2800,
        1024,
        g_test_rom,
        CLEM_IIGS_ROM3_SIZE,
        g_e0_ram,
        g_e1_ram,
        g_fpi_ram,
        CLEM_TEST_NUM_FPI_BANKS);

    munit_assert_int(init_result, ==, 0);

    return MUNIT_OK;
}

static MunitResult test_clem_initialize_failure(
    const MunitParameter params[],
    void* data
) {
    /* This will test if input is ROM3 compliant (roughly), though...
       TODO: Support ROM 01
    */
    ClemensMachine* machine = (ClemensMachine*)data;
    int init_result = clemens_init(
        machine,
        2800,
        1024,
        g_test_rom,
        CLEM_IIGS_ROM3_SIZE - 1,
        g_e0_ram,
        g_e1_ram,
        g_fpi_ram,
        CLEM_TEST_NUM_FPI_BANKS);

    munit_assert_int(init_result, ==, -1);

    /* This will test no ram
       TODO: add real error code defines
    */
    init_result = clemens_init(
        machine,
        2800,
        1024,
        g_test_rom,
        CLEM_IIGS_ROM3_SIZE,
        NULL,
        NULL,
        NULL,
        0);

    munit_assert_int(init_result, ==, -2);

    return MUNIT_OK;
}

static MunitTest clem_tests[] = {
    {
        (char*)"/clem/uninit",
        test_clem_is_initialized_false,
        test_clem_fixture_setup,
        test_clem_fixture_teardown,
        MUNIT_TEST_OPTION_NONE,
        NULL
    },
    {
        (char*)"/clem/init_minimal",
        test_clem_initialize_minimal,
        test_clem_fixture_setup,
        test_clem_fixture_teardown,
        MUNIT_TEST_OPTION_NONE,
        NULL
    },
    {
        (char*)"/clem/init_fail",
        test_clem_initialize_failure,
        test_clem_fixture_setup,
        test_clem_fixture_teardown,
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
