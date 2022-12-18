#ifndef CLEM_SMARTPORT_PRODOS_HDD32_H
#define CLEM_SMARTPORT_PRODOS_HDD32_H

#include "clem_shared.h"

typedef struct mpack_writer_t mpack_writer_t;
typedef struct mpack_reader_t mpack_reader_t;

#ifdef __cplusplus
extern "C" {
#endif

struct ClemensProdosHDD32 {
    void *user_context;
    unsigned drive_index;
    unsigned block_limit;
    unsigned (*read_block)(void *user_context, unsigned drive_index, unsigned block_index,
                           uint8_t *buffer);
    unsigned (*write_block)(void *user_context, unsigned drive_index, unsigned block_index,
                            const uint8_t *buffer);
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
