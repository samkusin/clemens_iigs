/*
    Tests shadowed writes from 00,01 to E0,EI

    Cases:
        - Shadow register options (not including shadow-all-banks)
        - Write vs Read timings
        - Shadowed pages (text, graphics, iolc inhibit)
*/

#include "munit/munit.h"

#include "util.h"
#include "emulator.h"
#include "clem_mem.h"
#include "clem_mmio_defs.h"

static ClemensMachine g_test_machine;
static ClemensTestMemory g_test_memory;


static void test_check_fpi_mega2_bank(
    ClemensMachine* machine,
    unsigned check_type,
    uint8_t* original_buffer,
    uint8_t* check_buffer,
    uint16_t check_sz,
    uint16_t adr,
    uint8_t bank
) {
    unsigned i;
    memset(check_buffer, 0, check_sz);
    for (i = 0; i < check_sz; ++i) {
        clem_read(machine, check_buffer + i, adr + i, bank, CLEM_MEM_FLAG_DATA);
    }
    munit_assert_memory_equal(check_sz, original_buffer, check_buffer);
    if (check_type == CLEM_TEST_CHECK_EQUAL) {
        munit_assert_memory_equal(check_sz, check_buffer,
                                  &machine->mega2_bank_map[bank & 1][adr]);
    } else {
        munit_assert_memory_not_equal(check_sz, check_buffer,
                                      &machine->mega2_bank_map[bank & 1][adr]);
    }
}

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

static MunitResult test_shadow_on_reset(
    const MunitParameter params[],
    void* data
) {
    /* Validate flags are set correctly on startup/reset */
    ClemensMachine* machine = (ClemensMachine*)data;
    uint8_t value = 0;
    char long_data[4];
    uint8_t* bank_mem;

    /*  all video areas shadowed, and IOLC region not inhibited
        actual shadowed memory checks are not covered in this test
    */
    clem_read(machine, &value, CLEM_TEST_IOADDR(SHADOW), 0xe0, CLEM_MEM_FLAG_DATA);
    munit_assert_uint8(value, ==, 0x08);
    return MUNIT_OK;
}

static MunitResult test_shadow_txt_pages(
    const MunitParameter params[],
    void* data
) {
    /* Validate flags are set correctly on startup/reset */
    ClemensMachine* machine = (ClemensMachine*)data;
    uint8_t reg_c035 = 0;
    char long_data[8];
    char check_buffer[8];
    uint8_t* bank_mem;

    /*  TXT1,2 are shadowed by default */
    clem_read(machine, &reg_c035, CLEM_TEST_IOADDR(SHADOW), 0xe0, CLEM_MEM_FLAG_DATA);
    munit_assert_false(reg_c035 & 0x21);

    strncpy(long_data, "deadmeat", sizeof(long_data));
    test_write(machine, long_data, sizeof(long_data), 0x400, 0x00);
    test_check_fpi_mega2_bank(machine, CLEM_TEST_CHECK_EQUAL,
                              long_data, check_buffer, sizeof(check_buffer), 0x400, 0x00);

    strncpy(long_data, "catfoods", sizeof(long_data));
    test_write(machine, long_data, sizeof(long_data), 0x800, 0x00);
    test_check_fpi_mega2_bank(machine, CLEM_TEST_CHECK_EQUAL,
                              long_data, check_buffer, sizeof(check_buffer), 0x800, 0x00);

    return MUNIT_OK;
}

static MunitResult test_shadow_txt_pages_disable(
    const MunitParameter params[],
    void* data
) {
    /* Validate flags are set correctly on startup/reset */
    ClemensMachine* machine = (ClemensMachine*)data;
    uint8_t reg_c035 = 0;
    char long_data[8];
    char check_buffer[8];
    uint8_t* bank_mem;

    /*  TXT1,2 are shadowed by default */
    clem_read(machine, &reg_c035, CLEM_TEST_IOADDR(SHADOW), 0xe0, CLEM_MEM_FLAG_DATA);
    munit_assert_false(reg_c035 & 0x21);

    //  disable TXT1,2 shadow
    reg_c035 |= 0x21;
    clem_write(machine, reg_c035, CLEM_TEST_IOADDR(SHADOW), 0xe0, CLEM_MEM_FLAG_DATA);

    strncpy(long_data, "livemeat", sizeof(long_data));
    test_write(machine, long_data, sizeof(long_data), 0x400, 0x00);
    test_check_fpi_mega2_bank(machine, CLEM_TEST_CHECK_NOT_EQUAL,
                              long_data, check_buffer, sizeof(check_buffer), 0x400, 0x00);
    strncpy(long_data, "dogfoods", sizeof(long_data));
    test_write(machine, long_data, sizeof(long_data), 0x800, 0x00);
    test_check_fpi_mega2_bank(machine, CLEM_TEST_CHECK_NOT_EQUAL,
                              long_data, check_buffer, sizeof(check_buffer), 0x800, 0x00);

    return MUNIT_OK;
}


static MunitResult test_shadow_hgr_pages(
    const MunitParameter params[],
    void* data
) {
    /* Validate flags are set correctly on startup/reset */
    ClemensMachine* machine = (ClemensMachine*)data;
    uint8_t reg_c035 = 0;
    char long_data[8];
    char check_buffer[8];
    uint8_t* bank_mem;

    /*  HGR1,2 + AUX are shadowed by default */
    clem_read(machine, &reg_c035, CLEM_TEST_IOADDR(SHADOW), 0xe0, CLEM_MEM_FLAG_DATA);
    munit_assert_false(reg_c035 & 0x16);

    strncpy(long_data, "deadmeat", sizeof(long_data));
    test_write(machine, long_data, sizeof(long_data), 0x2000, 0x00);
    test_write(machine, long_data, sizeof(long_data), 0x2100, 0x01);
    test_check_fpi_mega2_bank(machine, CLEM_TEST_CHECK_EQUAL,
                              long_data, check_buffer, sizeof(check_buffer), 0x2000, 0x00);
    test_check_fpi_mega2_bank(machine, CLEM_TEST_CHECK_EQUAL,
                              long_data, check_buffer, sizeof(check_buffer), 0x2100, 0x01);

    strncpy(long_data, "catfoods", sizeof(long_data));
    test_write(machine, long_data, sizeof(long_data), 0x4000, 0x00);
    test_write(machine, long_data, sizeof(long_data), 0x4100, 0x01);
    test_check_fpi_mega2_bank(machine, CLEM_TEST_CHECK_EQUAL,
                              long_data, check_buffer, sizeof(check_buffer), 0x4000, 0x00);
    test_check_fpi_mega2_bank(machine, CLEM_TEST_CHECK_EQUAL,
                              long_data, check_buffer, sizeof(check_buffer), 0x4100, 0x01);

    //  disable AUX page shadow
    reg_c035 |= 0x10;
    clem_write(machine, reg_c035, CLEM_TEST_IOADDR(SHADOW), 0xe0, CLEM_MEM_FLAG_DATA);

    strncpy(long_data, "zombifys", sizeof(long_data));
    test_write(machine, long_data, sizeof(long_data), 0x3000, 0x00);
    test_write(machine, long_data, sizeof(long_data), 0x3100, 0x01);
    test_check_fpi_mega2_bank(machine, CLEM_TEST_CHECK_EQUAL,
                              long_data, check_buffer, sizeof(check_buffer), 0x3000, 0x00);
    test_check_fpi_mega2_bank(machine, CLEM_TEST_CHECK_NOT_EQUAL,
                              long_data, check_buffer, sizeof(check_buffer), 0x3100, 0x01);
    strncpy(long_data, "ratfoods", sizeof(long_data));
    test_write(machine, long_data, sizeof(long_data), 0x5000, 0x00);
    test_write(machine, long_data, sizeof(long_data), 0x5100, 0x01);
    test_check_fpi_mega2_bank(machine, CLEM_TEST_CHECK_EQUAL,
                              long_data, check_buffer, sizeof(check_buffer), 0x5000, 0x00);
    test_check_fpi_mega2_bank(machine, CLEM_TEST_CHECK_NOT_EQUAL,
                              long_data, check_buffer, sizeof(check_buffer), 0x5100, 0x01);

    return MUNIT_OK;
}

static MunitTest clem_tests[] = {
    {
        (char*)"/shadow/boot", test_shadow_on_reset,
        test_fixture_setup,
        test_fixture_teardown,
        MUNIT_TEST_OPTION_NONE,
        NULL
    },
    {
        (char*)"/shadow/txt01", test_shadow_txt_pages,
        test_fixture_setup,
        test_fixture_teardown,
        MUNIT_TEST_OPTION_NONE,
        NULL
    },
    {
        (char*)"/shadow/txt01_disable", test_shadow_txt_pages_disable,
        test_fixture_setup,
        test_fixture_teardown,
        MUNIT_TEST_OPTION_NONE,
        NULL
    },
    {
        (char*)"/shadow/hgr", test_shadow_hgr_pages,
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
