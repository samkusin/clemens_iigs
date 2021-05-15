
#include "munit/munit.h"

#include "util.h"
#include "clem_woz.h"

#include <stdio.h>

typedef struct {
    void* image;
    unsigned image_sz;
} ClemensTestContext;

ClemensTestContext g_test_context;

static void* test_woz_fixture_setup(
    const MunitParameter params[],
    void* data
) {
    ClemensTestContext* ctx = &g_test_context;
    FILE* fp;
    memset(ctx, 0, sizeof(ClemensTestContext));

    fp = fopen("data/dos_3_3_master.woz", "rb");
    munit_assert_not_null(fp);
    fseek(fp, 0, SEEK_END);
    ctx->image_sz = (unsigned)ftell(fp);
    fseek(fp, 0, SEEK_SET);
    ctx->image = munit_malloc(ctx->image_sz);
    fread(ctx->image, 1, ctx->image_sz, fp);
    fclose(fp);

    return &g_test_context;
}

static void* test_woz_fixture_setup_3_5(
    const MunitParameter params[],
    void* data
) {
    ClemensTestContext* ctx = &g_test_context;
    FILE* fp;
    memset(ctx, 0, sizeof(ClemensTestContext));

    fp = fopen("data/a2gs_system_disk_1_1.woz", "rb");
    munit_assert_not_null(fp);
    fseek(fp, 0, SEEK_END);
    ctx->image_sz = (unsigned)ftell(fp);
    fseek(fp, 0, SEEK_SET);
    ctx->image = munit_malloc(ctx->image_sz);
    fread(ctx->image, 1, ctx->image_sz, fp);
    fclose(fp);

    return &g_test_context;
}

static void test_woz_fixture_teardown(
    void* data
) {
    ClemensTestContext* ctx = (ClemensTestContext*)data;
    if (ctx->image) {
        free(ctx->image);
    }
    memset(ctx, 0, sizeof(ClemensTestContext));
}

static MunitResult test_woz_load_mimimal(
    const MunitParameter params[],
    void* data
) {
    ClemensTestContext* ctx = (ClemensTestContext*)data;
    struct ClemensWOZChunkHeader chunk_header;
    const uint8_t* buffer = ctx->image;
    size_t buffer_remain = ctx->image_sz;

    buffer = clem_woz_check_header(buffer, buffer_remain);
    munit_assert_not_null(buffer);
    buffer_remain = ctx->image_sz - (buffer - ctx->image);
    buffer = clem_woz_parse_chunk_header(&chunk_header, buffer, buffer_remain);
    munit_assert_not_null(buffer);
    munit_assert_uint(chunk_header.type, ==, CLEM_WOZ_CHUNK_INFO);

    return MUNIT_OK;
}

static MunitResult test_woz_load(
    const MunitParameter params[],
    void* data
) {
    ClemensTestContext* ctx = (ClemensTestContext*)data;
    struct ClemensWOZChunkHeader chunk_header;
    struct ClemensWOZDisk disk;
    const uint8_t* buffer = ctx->image;
    size_t buffer_remain = ctx->image_sz;
    unsigned idx, num_tracks, cnt;

    buffer = clem_woz_check_header(buffer, buffer_remain);
    munit_assert_not_null(buffer);
    buffer_remain = ctx->image_sz - (buffer - ctx->image);
    buffer = clem_woz_parse_chunk_header(&chunk_header, buffer, buffer_remain);
    munit_assert_not_null(buffer);
    munit_assert_uint(chunk_header.type, ==, CLEM_WOZ_CHUNK_INFO);

    memset(&disk, 0, sizeof(disk));
    buffer_remain = ctx->image_sz - (buffer - ctx->image);
    buffer = clem_woz_parse_info_chunk(
        &disk, &chunk_header, buffer, buffer_remain);
    munit_assert_not_null(buffer);
    munit_assert_uint(disk.disk_type, ==, CLEM_WOZ_DISK_5_25);
    munit_assert_uint(disk.boot_type, ==, CLEM_WOZ_BOOT_5_25_16);
    munit_assert_uint(disk.bit_timing_ns, !=, 0);

    /* discover the number of valid tracks (the highest map value != 255) + 1
     */
    buffer_remain = ctx->image_sz - (buffer - ctx->image);
    buffer = clem_woz_parse_chunk_header(&chunk_header, buffer, buffer_remain);
    munit_assert_not_null(buffer);
    munit_assert_uint(chunk_header.type, ==, CLEM_WOZ_CHUNK_TMAP);

    buffer_remain = ctx->image_sz - (buffer - ctx->image);
    buffer = clem_woz_parse_tmap_chunk(
        &disk, &chunk_header, buffer, buffer_remain);
    for (idx = 0, num_tracks = 0; idx < CLEM_WOZ_LIMIT_QTR_TRACKS; ++idx) {
        if (disk.meta_track_map[idx] == 255) continue;
        if (disk.meta_track_map[idx] + 1  > num_tracks) {
            num_tracks = disk.meta_track_map[idx] + 1;
        }
    }
    munit_assert_uint(num_tracks, >, 0);
    munit_assert_uint(num_tracks, ==, 35);

    cnt = num_tracks * disk.max_track_size_bytes;
    disk.bits_data = (uint8_t*)munit_malloc(cnt);
    disk.bits_data_end = disk.bits_data + cnt;

    /* allocate data for the tracks
     */
    buffer_remain = ctx->image_sz - (buffer - ctx->image);
    buffer = clem_woz_parse_chunk_header(&chunk_header, buffer, buffer_remain);
    munit_assert_not_null(buffer);
    munit_assert_uint(chunk_header.type, ==, CLEM_WOZ_CHUNK_TRKS);

    buffer_remain = ctx->image_sz - (buffer - ctx->image);
    buffer = clem_woz_parse_trks_chunk(
        &disk, &chunk_header, buffer, buffer_remain);

    for (idx = 0; idx < num_tracks; ++idx) {
        munit_assert_uint(disk.track_byte_count[idx], !=, 0);
        cnt = disk.track_bits_count[idx] / 8;
        if (disk.track_bits_count[idx] % 8) ++cnt;
        munit_assert_uint(disk.track_byte_count[idx], >=, cnt);
    }

    free(disk.bits_data);

    return MUNIT_OK;
}

static MunitResult test_woz_load_3_5(
    const MunitParameter params[],
    void* data
) {
    ClemensTestContext* ctx = (ClemensTestContext*)data;
    struct ClemensWOZChunkHeader chunk_header;
    struct ClemensWOZDisk disk;
    const uint8_t* buffer = ctx->image;
    size_t buffer_remain = ctx->image_sz;
    unsigned idx, num_tracks, cnt;

    buffer = clem_woz_check_header(buffer, buffer_remain);
    munit_assert_not_null(buffer);
    buffer_remain = ctx->image_sz - (buffer - ctx->image);
    buffer = clem_woz_parse_chunk_header(&chunk_header, buffer, buffer_remain);
    munit_assert_not_null(buffer);
    munit_assert_uint(chunk_header.type, ==, CLEM_WOZ_CHUNK_INFO);

    memset(&disk, 0, sizeof(disk));
    buffer_remain = ctx->image_sz - (buffer - ctx->image);
    buffer = clem_woz_parse_info_chunk(
        &disk, &chunk_header, buffer, buffer_remain);
    munit_assert_not_null(buffer);
    munit_assert_uint(disk.disk_type, ==, CLEM_WOZ_DISK_3_5);
    munit_assert_uint(disk.boot_type, ==, CLEM_WOZ_BOOT_UNDEFINED);
    munit_assert_uint(disk.bit_timing_ns, !=, 0);

    /* discover the number of valid tracks (the highest map value != 255) + 1
     */
    buffer_remain = ctx->image_sz - (buffer - ctx->image);
    buffer = clem_woz_parse_chunk_header(&chunk_header, buffer, buffer_remain);
    munit_assert_not_null(buffer);
    munit_assert_uint(chunk_header.type, ==, CLEM_WOZ_CHUNK_TMAP);

    buffer_remain = ctx->image_sz - (buffer - ctx->image);
    buffer = clem_woz_parse_tmap_chunk(
        &disk, &chunk_header, buffer, buffer_remain);
    for (idx = 0, num_tracks = 0; idx < CLEM_WOZ_LIMIT_QTR_TRACKS; ++idx) {
        if (disk.meta_track_map[idx] == 255) continue;
        if (disk.meta_track_map[idx] + 1  > num_tracks) {
            num_tracks = disk.meta_track_map[idx] + 1;
        }
    }
    munit_assert_uint(num_tracks, >, 0);
    munit_assert_true(disk.flags & CLEM_WOZ_IMAGE_DOUBLE_SIDED);
    munit_assert_uint(num_tracks, ==, 160);

    cnt = num_tracks * disk.max_track_size_bytes;
    disk.bits_data = (uint8_t*)munit_malloc(cnt);
    disk.bits_data_end = disk.bits_data + cnt;

    /* allocate data for the tracks
     */
    buffer_remain = ctx->image_sz - (buffer - ctx->image);
    buffer = clem_woz_parse_chunk_header(&chunk_header, buffer, buffer_remain);
    munit_assert_not_null(buffer);
    munit_assert_uint(chunk_header.type, ==, CLEM_WOZ_CHUNK_TRKS);

    buffer_remain = ctx->image_sz - (buffer - ctx->image);
    buffer = clem_woz_parse_trks_chunk(
        &disk, &chunk_header, buffer, buffer_remain);

    for (idx = 0; idx < num_tracks; ++idx) {
        munit_assert_uint(disk.track_byte_count[idx], !=, 0);
        cnt = disk.track_bits_count[idx] / 8;
        if (disk.track_bits_count[idx] % 8) ++cnt;
        munit_assert_uint(disk.track_byte_count[idx], >=, cnt);
    }

    free(disk.bits_data);

    return MUNIT_OK;
}

static MunitTest woz_tests[] = {
    {
        (char*)"/woz/minimal",
        test_woz_load_mimimal,
        test_woz_fixture_setup,
        test_woz_fixture_teardown,
        MUNIT_TEST_OPTION_NONE,
        NULL
    },
    {
        (char*)"/woz/load",
        test_woz_load,
        test_woz_fixture_setup,
        test_woz_fixture_teardown,
        MUNIT_TEST_OPTION_NONE,
        NULL
    },
    {
        (char*)"/woz/load35",
        test_woz_load_3_5,
        test_woz_fixture_setup_3_5,
        test_woz_fixture_teardown,
        MUNIT_TEST_OPTION_NONE,
        NULL
    },
    { NULL, NULL, NULL, NULL, MUNIT_TEST_OPTION_NONE, NULL }
};

static MunitSuite woz_suite = {
    (char*)"",
    woz_tests,
    NULL,
    1,
    MUNIT_SUITE_OPTION_NONE
};

int main(int argc, char* argv[]) {
  /* Finally, we'll actually run our test suite!  That second argument
   * is the user_data parameter which will be passed either to the
   * test or (if provided) the fixture setup function. */
  return munit_suite_main(&woz_suite, NULL, argc, argv);
}
