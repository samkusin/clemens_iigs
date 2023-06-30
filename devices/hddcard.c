#include "hddcard.h"

#include "prodos_hdd32.h"

#include <stdlib.h>

struct ClemensHddCardContext {
    struct ClemensProdosHDD32 *hdd;
};

static const char *io_name(void *context) { return "hddcard"; }

static void io_reset(struct ClemensClock *clock, void *context) {}

static uint32_t io_sync(struct ClemensClock *clock, void *context) { return 0; }

static void io_read(struct ClemensClock *clock, uint8_t *data, uint8_t addr, uint8_t flags,
                    void *context) {}

static void io_write(struct ClemensClock *clock, uint8_t data, uint8_t addr, uint8_t flags,
                     void *context) {}

void clem_card_hdd_initialize(ClemensCard *card) {
    struct ClemensHddCardContext *context =
        (struct ClemensHddCardContext *)calloc(1, sizeof(struct ClemensHddCardContext));
    context->hdd = NULL;

    card->context = context;
    card->io_reset = &io_reset;
    card->io_sync = &io_sync;
    card->io_read = &io_read;
    card->io_write = &io_write;
    card->io_name = &io_name;
    card->io_dma = NULL;
}

void clem_card_hdd_uninitialize(ClemensCard *card) {
    free(card->context);
    card->context = NULL;
    card->io_reset = NULL;
    card->io_sync = NULL;
    card->io_read = NULL;
    card->io_write = NULL;
    card->io_name = NULL;
    card->io_dma = NULL;
}

void clem_card_hdd_mount(ClemensCard *card, ClemensProdosHDD32 *hdd) {
    struct ClemensHddCardContext *context = (struct ClemensHddCardContext *)(card->context);
    context->hdd = hdd;
}

ClemensProdosHDD32 *clem_card_hdd_unmount(ClemensCard *card) {
    struct ClemensHddCardContext *context = (struct ClemensHddCardContext *)(card->context);
    return context->hdd;
}

void clem_card_hdd_serialize(mpack_writer_t *writer, ClemensCard *card) {}

void clem_card_hdd_unserialize(mpack_reader_t *reader, ClemensCard *card,
                               ClemensSerializerAllocateCb alloc_cb, void *context) {}
