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
    uint32_t format;      /**< See CLEM_DISK_FORMAT_XXX */
    uint32_t dos_volume;  /**< DOS Volume */
    uint32_t block_count; /**< Block count (ProDOS only) */
    const uint8_t *data;  /**< The disk data within the 2IMG image */
    const uint8_t *data_end;
    const char *creator_data; /**< Guaranteed to exist after data_end */
    const char *creator_data_end;
    const char *comment;
    const char *comment_end;
    const uint8_t *image_buffer;  /**< Backing memory buffer owned by caller */
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
bool clem_2img_parse_header(struct Clemens2IMGDisk *disk, const uint8_t *image,
                            const uint8_t *image_end);

/**
 * @brief Generates a 2IMG disk container from either ProDOS or DOS images.
 *
 * Once running a compliant disk image through this function successfully, it
 * should be possible to build a 2IMG file from the result.  It will be
 * possible to also call clem_2img_nibblize_data to retrieve the nibble format
 * data.
 *
 * @param disk
 * @param format See CLEM_DISK_FORMAT_XXX
 * @param image Logical sectors based on the sector format
 * @param image_end End of the raw post-nibblized logical sector input buffer
 * @param image_data_offset Indicates where in the input image the disk data resides
 * @return true
 * @return false
 */
bool clem_2img_generate_header(struct Clemens2IMGDisk *disk, uint32_t format, const uint8_t *image,
                               const uint8_t *image_end, uint32_t image_data_offset);

/**
 * @brief Create a 2IMG disk buffer to serialize into a 2img file.
 *
 * @param disk The populated Clemens2IMGDisk struct
 * @param image The backing buffer
 * @param image_end
 * @return Image size or 0 if build failed
 */
unsigned clem_2img_build_image(struct Clemens2IMGDisk *disk, uint8_t *image, uint8_t *image_end);

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

/**
 * @brief Encodes the nibbilized data into bytes that conform to the disk format
 *
 * Encodes nibbles into GCR encoded data bytes for DOS or ProDOS compliant images.
 * This will not work correctly for corrupted images
 *
 * If corrupted is set to true, assume the data is corrupted and offer the user
 * an option to save the disk as a WOZ image to avoid potential data loss.
 *
 * @param disk
 * @param data_start
 * @param data_end
 * @param nib
 * @param corrupted
 * @return unsigned Amount of bytes encoded or 0 on error
 */
unsigned clem_2img_decode_nibblized_disk(struct Clemens2IMGDisk *disk, uint8_t *data_start,
                                         uint8_t *data_end, const struct ClemensNibbleDisk *nib,
                                         bool *corrupted);

#ifdef __cplusplus
}
#endif

#endif
