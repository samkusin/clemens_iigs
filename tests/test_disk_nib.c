//  Tests encoding to and decoding from a NIB using a trivial DOS and
//  ProDOS image as the basis.
//
//  To verify integrity with reference nibblized images, refer to the
//  WOZ tests

#include "unity.h"

#include "clem_disk.h"

static uint8_t g_525_disk[140 * 1024];
static uint8_t g_35_disk[800 * 1024];

void suiteSetUp(void) {
    //  these disks contain sectors where the contents of each sector represents
    //  their logical sector number within the resident track
    unsigned i, j, k, sector;
    uint8_t *track_data;

    track_data = &g_525_disk[0];
    for (i = 0; i < CLEM_DISK_LIMIT_525_DISK_TRACKS; ++i) {
        for (sector = 0; sector < CLEM_DISK_525_NUM_SECTORS_PER_TRACK; ++sector) {
            for (k = 0; k < 256; ++k) {
                track_data[k] = sector;
            }
            track_data += 256;
        }
    }

    track_data = &g_35_disk[0];
    for (i = 0; i < CLEM_DISK_35_NUM_REGIONS; ++i) {
        for (j = g_clem_track_start_per_region_35[i]; j < g_clem_track_start_per_region_35[i + 1];
             ++j) {
            for (sector = 0; sector < g_clem_max_sectors_per_region_35[i]; ++sector) {
                for (k = 0; k < 512; ++k) {
                    track_data[k] = sector;
                }
                track_data += 512;
            }
        }
    }
}

void test_clem_init(void) {}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_clem_init);
    return UNITY_END();
}
