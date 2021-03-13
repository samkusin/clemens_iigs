#include "munit/munit.h"

#include "emulator.h"

static ClemensMachine g_test_machine;

static void* test_clem_fixture_setup(
    const MunitParameter params[],
    void* data
) {
    memset(&g_test_machine, 0, sizeof(g_test_machine));
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
