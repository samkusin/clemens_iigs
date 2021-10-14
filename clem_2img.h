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

#include "clem_disk.h"

#define CLEM_2IMG_FORMAT_DOS            0
#define CLEM_2IMG_FORMAT_PRODOS         1
#define CLEM_2IMG_FORMAT_RAW            2

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief The 2IMG disk object
 *
 * Note all data pointers will refer to locations in the backing memory buffer
 * allocated by the caller.  This allows callers to allocate the memory for
 * the whole disk on load time and keep it until it's finished.
 */
struct Clemens2IMGDisk {
    char creator[4];
    uint16_t version;
    uint32_t format;            /**< See CLEM_2IMG_FORMAT_XXX */
    uint32_t dos_volume;        /**< DOS Volume */
    uint32_t block_count;       /**< Block count (ProDOS only) */
    char* creator_data;
    char* creator_data_end;
    char* comment;
    char* comment_end;
    uint8_t* data;
    uint8_t* data_end;
    uint8_t* image_buffer;      /**< Backing memory buffer owned by caller */
    uint32_t image_data_offset; /**< Offset to original track data */
    uint32_t image_data_length; /**< Length of the original track data */
    bool is_write_protected;    /**< Write protected image */
    bool is_nibblized;          /**< See the clem_2img_nibblize_data call */

    struct ClemensNibbleDisk* nib;
};


bool clem_2img_parse_header(struct Clemens2IMGDisk* disk,
                            uint8_t* image,
                            uint8_t* image_end);

size_t clem_2img_calculate_nibble_data_size(struct Clemens2IMGDisk* disk);


bool clem_2img_nibblize_data(struct Clemens2IMGDisk* disk,
                               uint8_t* data_start,
                               uint8_t* data_end);


#ifdef __cplusplus
}
#endif

#endif
