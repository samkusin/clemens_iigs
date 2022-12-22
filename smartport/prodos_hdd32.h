#ifndef CLEM_SMARTPORT_PRODOS_HDD32_H
#define CLEM_SMARTPORT_PRODOS_HDD32_H

#include "clem_shared.h"
#include "clem_smartport.h"

typedef struct mpack_writer_t mpack_writer_t;
typedef struct mpack_reader_t mpack_reader_t;

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Defines a shallow implementation of a simple Hard Drive interface
 */
struct ClemensProdosHDD32 {
    void *user_context;
    unsigned drive_index;
    unsigned block_limit;
    unsigned current_block_index;

    /**
     * @brief Callback to read a block of data from the host resident drive
     *
     */
    uint8_t (*read_block)(void *user_context, unsigned drive_index, unsigned block_index,
                          uint8_t *buffer);
    /**
     * @brief Call to write data to the host resident drive
     *
     */
    uint8_t (*write_block)(void *user_context, unsigned drive_index, unsigned block_index,
                           const uint8_t *buffer);
    /** Flush the host resideent device (write all contents, etc - This is optional depending on
     *  how read_block and write_block were implemented.*/
    uint8_t (*flush)(void *user_context, unsigned drive_index);
};

void clem_smartport_prodos_hdd32_initialize(struct ClemensSmartPortDevice *device,
                                            struct ClemensProdosHDD32 *impl);
void clem_smartport_prodos_hdd32_uninitialize(struct ClemensSmartPortDevice *device);
void clem_smartport_prodos_hdd32_serialize(mpack_writer_t *writer,
                                           struct ClemensSmartPortDevice *device);
void clem_smartport_prodos_hdd32_unserialize(mpack_reader_t *reader,
                                             struct ClemensSmartPortDevice *device,
                                             ClemensSerializerAllocateCb alloc_cb, void *context);

#ifdef __cplusplus
}
#endif
#endif
