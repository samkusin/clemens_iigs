#ifndef CLEM_CARD_HDDCARD_H
#define CLEM_CARD_HDDCARD_H

#include "clem_shared.h"

typedef struct mpack_writer_t mpack_writer_t;
typedef struct mpack_reader_t mpack_reader_t;

typedef struct ClemensProdosHDD32 ClemensProdosHDD32;

#ifdef __cplusplus
extern "C" {
#endif

void clem_card_hdd_initialize(ClemensCard *card);
void clem_card_hdd_uninitialize(ClemensCard *card);
void clem_card_hdd_mount(ClemensCard* card, ClemensProdosHDD32* hdd);
ClemensProdosHDD32* clem_card_hdd_unmount(ClemensCard* card);
void clem_card_hdd_serialize(mpack_writer_t* writer, ClemensCard* card);
void clem_card_hdd_unserialize(mpack_reader_t* reader, ClemensCard* card,
                                        ClemensSerializerAllocateCb alloc_cb,
                                        void* context);
#ifdef __cplusplus
}
#endif

#endif
