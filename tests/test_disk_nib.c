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
    memset(&nib, 0, sizeof(nib));
    nib.disk_type = CLEM_DISK_TYPE_3_5;
    clem_nib_reset_tracks(&nib, 160, g_nib_data, g_nib_data + g_nib_size);
    TEST_ASSERT_TRUE(clem_disk_nib_encode_35(&nib, CLEM_DISK_FORMAT_PRODOS, true, &g_35_disk[0],
                                             &g_35_disk[0] + sizeof(g_35_disk)));
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

void test_clem_track_525_encode_decode(void) {}

int main(void) {
    suiteSetUp();
    UNITY_BEGIN();
    RUN_TEST(test_clem_track_35_encode_800k);
    RUN_TEST(test_clem_track_35_encode_decode);
    RUN_TEST(test_clem_track_525_encode_decode);
    return UNITY_END();
}
