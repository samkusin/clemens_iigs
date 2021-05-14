
#include "munit/munit.h"

#include "util.h"
#include "clem_dsk.h"

#include <stdio.h>

typedef struct {
    void* image;
    unsigned image_sz;
} ClemensTestContext;

ClemensTestContext g_test_context;

static void* test_dsk_fixture_setup(
    const MunitParameter params[],
    void* data
) {
    ClemensTestContext* ctx = &g_test_context;
    FILE* fp;
    memset(ctx, 0, sizeof(ClemensTestContext));

    fp = fopen("data/dos33_1983.dsk", "rb");
    munit_assert_not_null(fp);
    fseek(fp, 0, SEEK_END);
    ctx->image_sz = (unsigned)ftell(fp);
    fseek(fp, 0, SEEK_SET);
    ctx->image = munit_malloc(ctx->image_sz);
    fread(ctx->image, 1, ctx->image_sz, fp);
    fclose(fp);

    return &g_test_context;
}

static void test_dsk_fixture_teardown(
    void* data
) {
    ClemensTestContext* ctx = (ClemensTestContext*)data;
    if (ctx->image) {
        free(ctx->image);
    }
    memset(ctx, 0, sizeof(ClemensTestContext));
}

static MunitResult test_dsk_load_mimimal(
    const MunitParameter params[],
    void* data
) {
    ClemensTestContext* ctx = (ClemensTestContext*)data;

    clem_dsk_load(ctx->image, ctx->image_sz);

    return MUNIT_OK;
}

static MunitTest dsk_tests[] = {
    {
        (char*)"/dsk/load",
        test_dsk_load_mimimal,
        test_dsk_fixture_setup,
        test_dsk_fixture_teardown,
        MUNIT_TEST_OPTION_NONE,
        NULL
    },
    { NULL, NULL, NULL, NULL, MUNIT_TEST_OPTION_NONE, NULL }
};

static MunitSuite dsk_suite = {
    (char*)"",
    dsk_tests,
    NULL,
    1,
    MUNIT_SUITE_OPTION_NONE
};

int main(int argc, char* argv[]) {
  /* Finally, we'll actually run our test suite!  That second argument
   * is the user_data parameter which will be passed either to the
   * test or (if provided) the fixture setup function. */
  return munit_suite_main(&dsk_suite, NULL, argc, argv);
}
