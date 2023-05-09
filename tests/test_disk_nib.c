//  Tests encoding to and decoding from a NIB using a trivial DOS and
//  ProDOS image as the basis.
//
//  To verify integrity with reference nibblized images, refer to the
//  WOZ tests

#include "unity.h"

#include "clem_disk.h"

#include <stdlib.h>
#include <string.h>

static uint8_t g_525_disk[140 * 1024];
static uint8_t g_35_disk[800 * 1024];
static uint8_t *g_nib_data = NULL;
static unsigned g_nib_size = 0;

//  https://codebase64.org/doku.php?id=base:small_fast_8-bit_prng
static uint8_t rand_next(uint8_t seed) {
    uint8_t a = seed;
    if (a) {
        a <<= 1;
        if (!a || !(seed & 0x80))
            return a;
    }
    a ^= 0x1d;
    return a;
}

void setUp(void) {}

void tearDown(void) {}

void suiteSetUp(void) {
    //  these disks contain sectors where the contents of each sector represents
    //  their logical sector number within the resident track
    unsigned i, j, k, sector;
    uint8_t *track_data;
    uint8_t byte;

    track_data = &g_525_disk[0];
    for (i = 0; i < CLEM_DISK_LIMIT_525_DISK_TRACKS; ++i) {
        for (sector = 0; sector < CLEM_DISK_525_NUM_SECTORS_PER_TRACK; ++sector) {
            byte = sector;
            for (k = 0; k < 256; ++k) {
                track_data[k] = byte;
                byte = rand_next(byte);
            }
            track_data += 256;
        }
    }
    track_data = &g_35_disk[0];
    for (i = 0; i < CLEM_DISK_35_NUM_REGIONS; ++i) {
        for (j = g_clem_track_start_per_region_35[i]; j < g_clem_track_start_per_region_35[i + 1];
             ++j) {
            for (sector = 0; sector < g_clem_max_sectors_per_region_35[i]; ++sector) {
                byte = sector;
                for (k = 0; k < 512; ++k) {
                    track_data[k] = byte;
                    byte = rand_next(byte);
                }
                track_data += 512;
            }
        }
    }

    g_nib_size = clem_disk_calculate_nib_storage_size(CLEM_DISK_TYPE_3_5);
    g_nib_data = malloc(g_nib_size);
}

void test_clem_track_35_encode_800k(void) {
    struct ClemensNibbleDisk nib;
    unsigned i, invalid_track_assignment_count;
    memset(&nib, 0, sizeof(nib));
    nib.disk_type = CLEM_DISK_TYPE_3_5;
    clem_nib_reset_tracks(&nib, 160, g_nib_data, g_nib_data + g_nib_size);
    TEST_ASSERT_TRUE(clem_disk_nib_encode_35(&nib, CLEM_DISK_FORMAT_PRODOS, true, &g_35_disk[0],
                                             &g_35_disk[0] + sizeof(g_35_disk)));

    //  validate double sided NIB created
    TEST_ASSERT_EQUAL_UINT(nib.track_count, 160U);
    invalid_track_assignment_count = 0;
    for (i = 0; i < nib.track_count; i++) {
        if (nib.meta_track_map[i] != i) {
            TEST_PRINTF("Track %u does not have a valid assignment", i);
            ++invalid_track_assignment_count;
        }
    }
    TEST_ASSERT_EQUAL_UINT(0, invalid_track_assignment_count);
}

void test_clem_track_35_encode_decode(void) {
    struct ClemensNibbleDisk nib;
    uint8_t decoded_35_disk[800 * 1024];
    unsigned i, block_not_equal_cnt;

    //  800K disk test
    memset(&nib, 0, sizeof(nib));
    nib.disk_type = CLEM_DISK_TYPE_3_5;
    clem_nib_reset_tracks(&nib, 160, g_nib_data, g_nib_data + g_nib_size);

    TEST_ASSERT_TRUE(clem_disk_nib_encode_35(&nib, CLEM_DISK_FORMAT_PRODOS, true, &g_35_disk[0],
                                             &g_35_disk[0] + sizeof(g_35_disk)));

    TEST_ASSERT_TRUE(clem_disk_nib_decode_35(&nib, CLEM_DISK_FORMAT_PRODOS, &decoded_35_disk[0],
                                             &decoded_35_disk[0] + sizeof(decoded_35_disk)));

    for (i = 0, block_not_equal_cnt = 0; i < CLEM_DISK_35_DOUBLE_PRODOS_BLOCK_COUNT; ++i) {
        int block_cmp = memcmp(&g_35_disk[i * 512], &decoded_35_disk[i * 512], 512);
        if (block_cmp != 0) {
            TEST_PRINTF("Block %u not equal", i);
            ++block_not_equal_cnt;
        }
    }

    TEST_ASSERT_EQUAL_INT(0, block_not_equal_cnt);
}

void test_clem_track_525_encode(void) {
    struct ClemensNibbleDisk nib;
    unsigned i, track_idx, track_mismatch_count, track_gap_not_found_cnt;
    memset(&nib, 0, sizeof(nib));
    nib.disk_type = CLEM_DISK_TYPE_5_25;
    clem_nib_reset_tracks(&nib, 35, g_nib_data, g_nib_data + g_nib_size);
    TEST_ASSERT_TRUE(clem_disk_nib_encode_525(&nib, CLEM_DISK_FORMAT_PRODOS, true, &g_525_disk[0],
                                              &g_525_disk[0] + sizeof(g_525_disk)));

    //  validate double sided NIB created
    TEST_ASSERT_EQUAL_UINT(nib.track_count, 35U);
    track_mismatch_count = 0; // if the track index doesn't match what's expected in the meta list
    track_idx = 0;
    for (i = 0; i < CLEM_DISK_LIMIT_QTR_TRACKS && track_idx < nib.track_count;
         i += 4, track_idx++) {
        bool track_mismatch = false;
        if (track_idx > 0) {
            track_mismatch = track_mismatch || (nib.meta_track_map[i - 1] != track_idx);
        }
        track_mismatch = track_mismatch || (nib.meta_track_map[i] != track_idx);
        if (track_idx < nib.track_count - 1) {
            track_mismatch = track_mismatch || (nib.meta_track_map[i + 1] != track_idx);
        }
        if (track_mismatch) {
            TEST_PRINTF("Track %u does not have a valid assignment on the qtr track list (%u)",
                        track_idx, i);
            track_mismatch_count++;
        }
    }
    track_gap_not_found_cnt = 0; // counts the unassigned tracks in the meta list (in 5.25" disks
                                 // there should be some at qtr_track 2, 6, 10, etc.)
    for (i = 0; i < nib.track_count; i++) {
        if (nib.meta_track_map[i * 4 + 2] != 0xff)
            track_gap_not_found_cnt++;
    }
    //  tracks 36 - 40 should be unassigned
    for (i = nib.track_count * 4; i < 160; ++i) {
        if (nib.meta_track_map[i] != 0xff)
            track_gap_not_found_cnt++;
    }
    TEST_ASSERT_EQUAL_UINT(0, track_gap_not_found_cnt);
}

void test_clem_track_525_encode_decode(void) {
    struct ClemensNibbleDisk nib;
    uint8_t decoded_disk[140 * 1024];
    unsigned i, block_not_equal_cnt;

    //  800K disk test
    memset(&nib, 0, sizeof(nib));
    nib.disk_type = CLEM_DISK_TYPE_5_25;
    clem_nib_reset_tracks(&nib, 35, g_nib_data, g_nib_data + g_nib_size);

    TEST_ASSERT_TRUE(clem_disk_nib_encode_525(&nib, CLEM_DISK_FORMAT_PRODOS, true, &g_525_disk[0],
                                              &g_525_disk[0] + sizeof(g_525_disk)));

    TEST_ASSERT_TRUE(clem_disk_nib_decode_525(&nib, CLEM_DISK_FORMAT_PRODOS, &decoded_disk[0],
                                              &decoded_disk[0] + sizeof(decoded_disk)));

    for (i = 0, block_not_equal_cnt = 0; i < CLEM_DISK_525_PRODOS_BLOCK_COUNT * 2; ++i) {
        int block_cmp = memcmp(&g_525_disk[i * 256], &decoded_disk[i * 256], 256);
        if (block_cmp != 0) {
            TEST_PRINTF("Sector %u not equal", i);
            ++block_not_equal_cnt;
        }
    }

    TEST_ASSERT_EQUAL_INT(0, block_not_equal_cnt);
}

void test_clem_track_525_encode_decode_dos(void) {
    struct ClemensNibbleDisk nib;
    uint8_t decoded_disk[140 * 1024];
    unsigned i, block_not_equal_cnt;

    //  800K disk test
    memset(&nib, 0, sizeof(nib));
    nib.disk_type = CLEM_DISK_TYPE_5_25;
    clem_nib_reset_tracks(&nib, 35, g_nib_data, g_nib_data + g_nib_size);

    TEST_ASSERT_TRUE(clem_disk_nib_encode_525(&nib, CLEM_DISK_FORMAT_DOS, true, &g_525_disk[0],
                                              &g_525_disk[0] + sizeof(g_525_disk)));

    TEST_ASSERT_TRUE(clem_disk_nib_decode_525(&nib, CLEM_DISK_FORMAT_DOS, &decoded_disk[0],
                                              &decoded_disk[0] + sizeof(decoded_disk)));

    for (i = 0, block_not_equal_cnt = 0; i < CLEM_DISK_525_PRODOS_BLOCK_COUNT * 2; ++i) {
        int block_cmp = memcmp(&g_525_disk[i * 256], &decoded_disk[i * 256], 256);
        if (block_cmp != 0) {
            TEST_PRINTF("Sector %u not equal", i);
            ++block_not_equal_cnt;
        }
    }

    TEST_ASSERT_EQUAL_INT(0, block_not_equal_cnt);
}

int main(void) {
    suiteSetUp();
    UNITY_BEGIN();
    RUN_TEST(test_clem_track_35_encode_800k);
    RUN_TEST(test_clem_track_35_encode_decode);
    RUN_TEST(test_clem_track_525_encode);
    RUN_TEST(test_clem_track_525_encode_decode);
    RUN_TEST(test_clem_track_525_encode_decode_dos);
    return UNITY_END();
}
