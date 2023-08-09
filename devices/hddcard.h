#ifndef CLEM_CARD_HDDCARD_H
#define CLEM_CARD_HDDCARD_H

#include "clem_shared.h"

typedef struct mpack_writer_t mpack_writer_t;
typedef struct mpack_reader_t mpack_reader_t;

typedef struct ClemensProdosHDD32 ClemensProdosHDD32;

#define CLEM_CARD_HDD_STATUS_DRIVE_ON           1
#define CLEM_CARD_HDD_STATUS_DRIVE_WRITE_PROT   2

#define CLEM_CARD_HDD_INDEX_NONE 0xff

#ifdef __cplusplus
extern "C" {
#endif

void clem_card_hdd_initialize(ClemensCard *card);
void clem_card_hdd_uninitialize(ClemensCard *card);
void clem_card_hdd_mount(ClemensCard* card, ClemensProdosHDD32* hdd, uint8_t drive_index);

ClemensProdosHDD32* clem_card_hdd_unmount(ClemensCard* card, uint8_t drive_index);
void clem_card_hdd_serialize(mpack_writer_t* writer, ClemensCard* card);
void clem_card_hdd_unserialize(mpack_reader_t* reader, ClemensCard* card,
                                        ClemensSerializerAllocateCb alloc_cb,
                                        void* context);
unsigned clem_card_hdd_get_status(ClemensCard *card, uint8_t drive_index);
void clem_card_hdd_lock(ClemensCard* card, uint8_t lock, uint8_t drive_index);
ClemensProdosHDD32* clem_card_get_mount(ClemensCard* card, uint8_t drive_index);
uint8_t clem_card_hdd_drive_index_has_image(ClemensCard* card, uint8_t drive_index);

#ifdef __cplusplus
}
#endif

#endif
