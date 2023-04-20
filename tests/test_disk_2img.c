//  Tests serialization of 2IMG to and from our intermediate from nibblized disk
//  format. Since the 2IMG APIs build off of the clem_disk encoding/decoding APIs,
//  these tests verify that the 2IMG input and output metadata remains
//  consistent.
//

#include "clem_2img.h"
#include "unity.h"
#include "utils.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

//#define CLEM_DISK_2IMG_TEST_SAVE_IMAGE_RESULTS

static uint8_t g_nib_data[CLEM_DISK_35_MAX_DATA_SIZE];

void setUp(void) {}

void tearDown(void) {}

void test_clem_2img_load_simple(void) {
    size_t image_sz;
    uint8_t *image_data = clem_test_load_disk_image("data/ProDOS 16v1_3.2mg", &image_sz);
    struct Clemens2IMGDisk disk;

    memset(&disk, 0, sizeof(disk));

    TEST_ASSERT_NOT_NULL_MESSAGE(image_data, "Failed to open disk image");
    TEST_ASSERT_TRUE(clem_2img_parse_header(&disk, image_data, image_data + image_sz));
    TEST_ASSERT_EQUAL_UINT32(disk.block_count, CLEM_DISK_35_DOUBLE_PRODOS_BLOCK_COUNT);
    TEST_ASSERT_EQUAL_UINT32(disk.format, CLEM_DISK_FORMAT_PRODOS);

    free(image_data);
}

void test_clem_2img_load_and_regenerate_image(void) {
    size_t image_sz, regen_image_sz;
    uint8_t *image_data = clem_test_load_disk_image("data/ProDOS 16v1_3.2mg", &image_sz);
    uint8_t *decoded_data;
    uint8_t *regen_image_data;
    struct Clemens2IMGDisk disk;
    struct ClemensNibbleDisk nib;

    memset(&disk, 0, sizeof(disk));

    TEST_ASSERT_NOT_NULL_MESSAGE(image_data, "Failed to open disk image");
    TEST_ASSERT_TRUE(clem_2img_parse_header(&disk, image_data, image_data + image_sz));
    TEST_ASSERT_EQUAL_UINT32(disk.block_count, CLEM_DISK_35_DOUBLE_PRODOS_BLOCK_COUNT);
    TEST_ASSERT_EQUAL_UINT32(disk.format, CLEM_DISK_FORMAT_PRODOS);

    memset(&nib, 0, sizeof(nib));

    nib.disk_type = CLEM_DISK_TYPE_3_5;
    nib.bits_data = g_nib_data;
    nib.bits_data_end = g_nib_data + CLEM_DISK_35_MAX_DATA_SIZE;
    nib.track_count = 160;
    disk.nib = &nib;

    TEST_ASSERT_TRUE(clem_2img_nibblize_data(&disk));

    image_sz = CLEM_DISK_35_DOUBLE_PRODOS_BLOCK_COUNT * 512;
    decoded_data = malloc(image_sz);
    TEST_ASSERT_TRUE(
        clem_2img_decode_nibblized_disk(&disk, decoded_data, decoded_data + image_sz, &nib));

#ifdef CLEM_DISK_2IMG_TEST_SAVE_IMAGE_RESULTS
    clem_test_save_disk_image("reference.bin", disk.data, disk.data_end - disk.data);
    clem_test_save_disk_image("actual.bin", decoded_data, image_sz);
#endif
    TEST_ASSERT_EQUAL_INT(0, memcmp(disk.data, decoded_data, image_sz));

    disk.data = decoded_data;
    disk.data_end = disk.data + image_sz;

    regen_image_sz = image_sz + 128;
    regen_image_data = malloc(regen_image_sz);

    regen_image_sz =
        clem_2img_build_image(&disk, regen_image_data, regen_image_data + regen_image_sz);
    TEST_ASSERT_EQUAL_UINT(image_sz + CLEM_2IMG_HEADER_BYTE_SIZE, regen_image_sz);

    free(regen_image_data);
    free(decoded_data);
    free(image_data);
}

void test_clem_2img_generate_image_from_dsk(void) {
    size_t image_sz;
    uint8_t *image_data = clem_test_load_disk_image("data/ProDOS_2_4_2.dsk", &image_sz);
    struct Clemens2IMGDisk disk;

    memset(&disk, 0, sizeof(disk));

    TEST_ASSERT_NOT_NULL_MESSAGE(image_data, "Failed to open disk image");
    TEST_ASSERT_TRUE(clem_2img_generate_header(&disk, CLEM_DISK_FORMAT_DOS, image_data,
                                               image_data + image_sz, 0));

    free(image_data);
}

void test_clem_2img_generate_image_from_po_800k(void) {
    size_t image_sz;
    uint8_t *image_data = clem_test_load_disk_image("data/System.Disk.po", &image_sz);
    struct Clemens2IMGDisk disk;

    memset(&disk, 0, sizeof(disk));

    TEST_ASSERT_NOT_NULL_MESSAGE(image_data, "Failed to open disk image");
    TEST_ASSERT_TRUE(clem_2img_generate_header(&disk, CLEM_DISK_FORMAT_PRODOS, image_data,
                                               image_data + image_sz, 0));
    TEST_ASSERT_EQUAL_UINT(disk.block_count, 1600);

    free(image_data);
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_clem_2img_load_simple);
    RUN_TEST(test_clem_2img_load_and_regenerate_image);
    RUN_TEST(test_clem_2img_generate_image_from_dsk);
    RUN_TEST(test_clem_2img_generate_image_from_po_800k);
    return UNITY_END();
}
