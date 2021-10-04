/**
 * @file clem_2img.h
 * @author your name (you@domain.com)
 * @brief Apple IIgs 2img disk image utilities
 * @version 0.1
 * @date 2021-10-06
 *
 * @copyright Copyright (c) 2021
 *
 */

#ifndef CLEM_2IMG_DISK_H
#define CLEM_2IMG_DISK_H

#include <stdint.h>
#include <stdbool.h>

#define CLEM_2IMG_FORMAT_DOS            0
#define CLEM_2IMG_FORMAT_PRODOS         1
#define CLEM_2IMG_FORMAT_RAW            2

#ifdef __cplusplus
extern "C" {
#endif


struct Clemens2IMGDisk {
    char creator[4];
    uint16_t version;
    uint32_t format;            /**< See CLEM_2IMG_FORMAT_XXX */
    uint32_t dos_volume;        /**< DOS Volume */
    uint32_t block_count;       /**< Block count (ProDOS only) */
    bool is_write_protected;    /**< Write protected image */
    uint8_t* creator_data;
    uint8_t* creator_data_end;
    char* comment;
    char* comment_end;
    uint8_t* data;
    uint8_t* data_end;
};

bool clem_2img_parse_header(struct Clemens2IMGDisk* disk,
                            uint8_t* data,
                            uint8_t* data_end);

#ifdef __cplusplus
}
#endif

#endif
