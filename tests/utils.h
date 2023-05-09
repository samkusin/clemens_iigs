#ifndef CLEM_TESTS_UTILS_H
#define CLEM_TESTS_UTILS_H

#include <stddef.h>
#include <stdint.h>

extern uint8_t *clem_test_load_disk_image(const char *path, size_t *image_sz);
extern void clem_test_save_disk_image(const char *path, const uint8_t *image, size_t image_sz);

#endif
