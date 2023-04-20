//  Tests serialization of WOZ files to and from our intermediate from nibblized
//  disk format.  Since the 2IMG APIs build off of the clem_disk encoding/decoding
//  APIs, these tests verify that the 2IMG input and output metadata remains
//  consistent.
//

#include "clem_disk.h"
#include "clem_woz.h"
#include "unity.h"
#include "utils.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static uint8_t g_nib_data[CLEM_DISK_35_MAX_DATA_SIZE];

void setUp(void) {}

void tearDown(void) {}

void test_clem_woz_parse_info(void) {
    size_t image_sz;
    uint8_t *image_data = clem_test_load_disk_image("data/dos_3_3_master.woz", &image_sz);
    const uint8_t *image_data_end;
    struct ClemensWOZDisk disk;
    struct ClemensWOZChunkHeader header;
    const uint8_t *woz_data;

    TEST_ASSERT_NOT_NULL_MESSAGE(image_data, "Failed to open disk image");

    memset(&disk, 0, sizeof(disk));
    memset(&header, 0, sizeof(header));

    image_data_end = image_data + image_sz;
    woz_data = clem_woz_check_header(image_data, image_sz);
    TEST_ASSERT_NOT_NULL(woz_data);
    woz_data = clem_woz_parse_chunk_header(&header, woz_data, image_data_end - woz_data);
    TEST_ASSERT_NOT_NULL(woz_data);
    woz_data = clem_woz_parse_info_chunk(&disk, &header, woz_data, image_data_end - woz_data);
    TEST_ASSERT_NOT_NULL(woz_data);
    TEST_ASSERT_EQUAL_UINT(CLEM_DISK_TYPE_5_25, disk.disk_type);
    TEST_ASSERT_EQUAL_UINT(CLEM_WOZ_BOOT_5_25_16, disk.boot_type);
    free(image_data);
}

void test_clem_woz_load_simple(void) {
    size_t image_sz;
    uint8_t *image_data = clem_test_load_disk_image("data/dos_3_3_master.woz", &image_sz);
    struct ClemensWOZDisk disk;
    struct ClemensNibbleDisk nib;
    int errc;

    TEST_ASSERT_NOT_NULL_MESSAGE(image_data, "Failed to open disk image");

    memset(&disk, 0, sizeof(disk));
    memset(&nib, 0, sizeof(nib));

    nib.bits_data = g_nib_data;
    nib.bits_data_end = g_nib_data + CLEM_DISK_35_MAX_DATA_SIZE;

    disk.nib = &nib;

    TEST_ASSERT_NOT_NULL(clem_woz_unserialize(&disk, image_data, image_sz, 2, &errc));
    TEST_ASSERT_EQUAL_UINT(35, nib.track_count);
    TEST_ASSERT_EQUAL_UINT(4000, nib.bit_timing_ns);
    TEST_ASSERT_EQUAL_UINT(CLEM_DISK_TYPE_5_25, nib.disk_type);

    free(image_data);
}

void test_clem_woz_load_and_regenerate_image(void) {
    size_t image_sz, regen_image_sz;
    uint8_t *image_data = clem_test_load_disk_image("data/dos_3_3_master.woz", &image_sz);
    uint8_t *regen_image_data;
    struct ClemensWOZDisk disk;
    struct ClemensWOZDisk disk_regen;
    struct ClemensNibbleDisk nib;
    int errc;

    TEST_ASSERT_NOT_NULL_MESSAGE(image_data, "Failed to open disk image");

    memset(&disk, 0, sizeof(disk));
    memset(&nib, 0, sizeof(nib));

    nib.disk_type = CLEM_DISK_TYPE_3_5;
    nib.bits_data = g_nib_data;
    nib.bits_data_end = g_nib_data + CLEM_DISK_35_MAX_DATA_SIZE;
    disk.nib = &nib;

    TEST_ASSERT_NOT_NULL(clem_woz_unserialize(&disk, image_data, image_sz, 2, &errc));
    regen_image_sz = image_sz * 2;
    regen_image_data = malloc(regen_image_sz);

    TEST_ASSERT_NOT_NULL(clem_woz_serialize(&disk, regen_image_data, &regen_image_sz));
    TEST_ASSERT_NOT_EQUAL_size_t(0, regen_image_sz);

    memset(&disk_regen, 0, sizeof(disk_regen));
    memset(&nib, 0, sizeof(nib));
    nib.disk_type = CLEM_DISK_TYPE_3_5;
    nib.bits_data = g_nib_data;
    nib.bits_data_end = g_nib_data + CLEM_DISK_35_MAX_DATA_SIZE;
    disk_regen.nib = &nib;

    TEST_ASSERT_NOT_NULL(
        clem_woz_unserialize(&disk_regen, regen_image_data, regen_image_sz, 2, &errc));
    TEST_ASSERT_EQUAL_UINT(CLEM_DISK_TYPE_5_25, disk_regen.disk_type);
    TEST_ASSERT_EQUAL_UINT(CLEM_WOZ_BOOT_5_25_16, disk_regen.boot_type);
    TEST_ASSERT_EQUAL_UINT(35, nib.track_count);
    TEST_ASSERT_EQUAL_UINT(4000, nib.bit_timing_ns);
    TEST_ASSERT_EQUAL_UINT(CLEM_DISK_TYPE_5_25, nib.disk_type);

    free(regen_image_data);
    free(image_data);
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_clem_woz_parse_info);
    RUN_TEST(test_clem_woz_load_simple);
    RUN_TEST(test_clem_woz_load_and_regenerate_image);
    return UNITY_END();
}
