#include "munit/munit.h"

#include "util.h"
#include "emulator.h"

/* Just for testing purposes in this fixture */
#define CLEM_TEST_NUM_FPI_BANKS     4

static ClemensMachine g_test_machine;
static ClemensTestMemory g_test_memory;

static void* test_clem_fixture_setup(
    const MunitParameter params[],
    void* data
) {
    uint16_t adr;
    uint8_t* bank;

    memset(&g_test_machine, 0, sizeof(g_test_machine));
    memset(&g_test_memory, 0, sizeof(g_test_memory));

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
    int init_result = clem_test_init_machine_trivial_rom(
        machine, &g_test_memory);

    munit_assert_int(init_result, ==, 0);
    munit_assert_true(clemens_is_initialized(machine));
    munit_assert_true(clemens_is_initialized_simple(machine));

    /* MMIO will not be ready until a complete reset sequence */
    munit_assert_false(clemens_is_mmio_initialized(machine));

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
    int init_result =  clemens_init(
        machine,
        2800,
        1024,
        g_test_memory.g_test_rom,
        CLEM_IIGS_ROM3_SIZE - 1,
        g_test_memory.g_e0_ram, g_test_memory.g_e1_ram,
        g_test_memory.g_fpi_ram, CLEM_TEST_NUM_FPI_BANKS);

    munit_assert_int(init_result, ==, -1);

    /* This will test no ram
       TODO: add real error code defines
    */
    init_result = clemens_init(
        machine,
        2800,
        1024,
        g_test_memory.g_test_rom,
        CLEM_IIGS_ROM3_SIZE,
        NULL,
        NULL,
        NULL,
        0);

    munit_assert_int(init_result, ==, -2);

    return MUNIT_OK;
}


static MunitResult test_clem_emulate_minimal(
    const MunitParameter params[],
    void* data
) {
    /* Just test that the emulate cycle works - other tests will confirm the
       behavior and timing of instructions, interrupts, etc.
    */
    ClemensMachine* machine = (ClemensMachine*)data;
    int init_result = clem_test_init_machine_trivial_rom(
        machine, &g_test_memory);

    munit_assert_int(init_result, ==, 0);
    munit_assert_true(clemens_is_initialized(machine));
    munit_assert_true(clemens_is_initialized_simple(machine));

    /* MMIO will not be ready until a complete reset sequence */
    munit_assert_false(clemens_is_mmio_initialized(machine));

    /* TODO!!! DEBUG WHY THIS IS NOT WORKING? */
    machine->cpu.pins.resbIn = false;
    clemens_emulate(machine);
    machine->cpu.pins.resbIn = true;

    munit_assert_true(clemens_is_mmio_initialized(machine));

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
    {
        (char*)"/clem/emulate_minimal",
        test_clem_emulate_minimal,
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
