#include "utils.h"

#include <stdio.h>
#include <stdlib.h>

uint8_t *clem_test_load_disk_image(const char *path, size_t *image_sz) {
    uint8_t *data = NULL;
    FILE *fp = fopen(path, "rb");
    long size;
    if (!fp)
        return NULL;
    fseek(fp, 0, SEEK_END);
    size = ftell(fp);
    fseek(fp, 0, SEEK_SET);
    data = malloc(size);
    fread(data, size, 1, fp);
    fclose(fp);
    *image_sz = size;
    return data;
}

void clem_test_save_disk_image(const char *path, const uint8_t *image, size_t image_sz) {
    FILE *fp = fopen(path, "wb");
    long size;
    if (!fp)
        return;
    fwrite(image, image_sz, 1, fp);
    fclose(fp);
}
