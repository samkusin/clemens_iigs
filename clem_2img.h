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

#define CLEM_2IMG_FORMAT_DOS    0U
#define CLEM_2IMG_FORMAT_PRODOS 1U
#define CLEM_2IMG_FORMAT_RAW    2U

/** Per spec, the header size preceding the disk data must be this length.  This value can be used
 * to allocate a backing buffer for a custom 2img file. */
#define CLEM_2IMG_HEADER_BYTE_SIZE 64

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
    uint32_t format;      /**< See CLEM_2IMG_FORMAT_XXX */
    uint32_t dos_volume;  /**< DOS Volume */
    uint32_t block_count; /**< Block count (ProDOS only) */
    char *creator_data;
    char *creator_data_end;
    char *comment;
    char *comment_end;
    uint8_t *data;
    uint8_t *data_end;
    uint8_t *image_buffer;        /**< Backing memory buffer owned by caller */
    uint32_t image_buffer_length; /**< Length of the original memory buffer */
    uint32_t image_data_offset;   /**< Offset to original track data */
    bool is_write_protected;      /**< Write protected image */
    bool is_nibblized;            /**< See the clem_2img_nibblize_data call */

    /* This is provided by the caller.  At the very least the nib->bits_data and
       nib->bits_data_end byte vector must be defined before calling
       clem_2img_nibblize_data.  The byte vector and metadata will be populated
       by said call.
    */
    struct ClemensNibbleDisk *nib;
};

struct ClemensNibEncoder {
    uint8_t *begin;
    uint8_t *end;
    unsigned bit_index;
    unsigned bit_index_end;
};

/**
 * @brief Obtains information from a 2IMG disk image used for processing
 *
 * This function will initialize a new Clemens2IMGDisk object.
 *
 * Calling this function is required before running the nibbilization pass via
 * clem_2img_nibblize_data.
 *
 * @param disk
 * @param image
 * @param image_end
 * @return true
 * @return false
 */
bool clem_2img_parse_header(struct Clemens2IMGDisk *disk, uint8_t *image, uint8_t *image_end);

/**
 * @brief Generates a 2IMG disk container from either ProDOS or DOS images.
 *
 * Once running a compliant disk image through this function successfully, it
 * should be possible to build a 2IMG file from the result.  It will be
 * possible to also call clem_2img_nibblize_data to retrieve the nibble format
 * data.
 *
 * @param disk
 * @param format See CLEM_2IMG_FORMAT_XXX
 * @param image Logical sectors based on the sector format
 * @param image_end End of the raw post-nibblized logical sector input buffer
 * @param image_data_offset Indicates where in the input image the disk data resides
 * @return true
 * @return false
 */
bool clem_2img_generate_header(struct Clemens2IMGDisk *disk, uint32_t format, uint8_t *image,
                               uint8_t *image_end, uint32_t image_data_offset);

/**
 * @brief Create a 2IMG disk buffer to serialize into a 2img file.
 *
 * @param disk The populated Clemens2IMGDisk struct
 * @param image The backing buffer
 * @param image_end
 * @return true
 * @return false
 */
bool clem_2img_build_image(struct Clemens2IMGDisk *disk, uint8_t *image, uint8_t *image_end);

/**
 * @brief Runs the nibbilization pass on the disk image.
 *
 * A ClemensNibbleDisk with an allocated bits buffer is required.  This function
 * will return false if the nibbilization pass fails if there's not enough
 * storage.
 *
 * @param disk A parsed disk image with an attached ClemensNibbleDisk buffer
 * @return true
 * @return false Nibbilization failed due to lack of space or invalid data from
 *      the source 2IMG (DSK, PO) data.
 */
bool clem_2img_nibblize_data(struct Clemens2IMGDisk *disk);

#ifdef __cplusplus
}
#endif

#endif
