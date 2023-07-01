#include "hddcard.h"
#include "prodos_hdd32.h"
#include "slot7hdd_firmware.h"

#include "clem_debug.h"
#include "serializer.h"

#include <stdlib.h>
#include <string.h>

#define CLEM_CARD_HDD_STATE_IDLE    0x00
#define CLEM_CARD_HDD_STATE_COMMAND 0x01
#define CLEM_CARD_HDD_STATE_DMA_W   0x02
#define CLEM_CARD_HDD_STATE_DMA_R   0x03

/*
+ 0x70 for slot 7
IO_CONTROL      equ  $C080          ; Write control, Read Handshake
IO_COMMAND      equ  $C081          ; Write command bytes
IO_RESULT0      equ  $C082          ; results for status = X
IO_RESULT1      equ  $C083          ; results for status = Y

Control and Command protocol
CTL: write $00 to start a command (this will cancel any current command)
CTL: read until bit 7 is lo (ready, idle)
CMD: write command bytes
CTL: when done, Write $80 to control fire the command
CTL: Read where bit 7 is hi for being in progress
                bit 0 is hi if error
CMD: Read the error code until CTL is set back $00
*/
#define CLEM_CARD_HDD_IO_CONTROL 0x00
#define CLEM_CARD_HDD_IO_COMMAND 0x01
#define CLEM_CARD_HDD_IO_RESULT0 0x02
#define CLEM_CARD_HDD_IO_RESULT1 0x03

#define CLEM_CARD_HDD_RES_ERROR 0x00
#define CLEM_CARD_HDD_RES_MISC  0x01
#define CLEM_CARD_HDD_RES_0     0x02
#define CLEM_CARD_HDD_RES_1     0x03

#define CLEM_CARD_HDD_PRODOS_ERR_NONE  0x00
#define CLEM_CARD_HDD_PRODOS_ERR_IO    0x27
#define CLEM_CARD_HDD_PRODOS_ERR_NODEV 0x28
#define CLEM_CARD_HDD_PRODOS_ERR_WPROT 0x2b

#define CLEM_CARD_HDD_CONTROL_FLAG_OK          0x00
#define CLEM_CARD_HDD_CONTROL_FLAG_ERROR       0x01
#define CLEM_CARD_HDD_CONTROL_FLAG_IN_PROGRESS 0x80

#define CLEM_CARD_HDD_COMMAND_STATUS 0x00
#define CLEM_CARD_HDD_COMMAND_READ   0x01
#define CLEM_CARD_HDD_COMMAND_WRITE  0x02
#define CLEM_CARD_HDD_COMMAND_FORMAT 0x03

struct ClemensHddCardContext {
    struct ClemensProdosHDD32 *hdd;
    unsigned state;
    uint8_t cmd_num;
    uint8_t unit_num;
    uint16_t block_num;
    uint16_t dma_addr;
    uint16_t dma_offset;
    uint8_t results[4]; // used for returning results in IO_RESULT0/1, error code
    uint8_t block_data[512];
};

static void inline _clem_card_hdd_fail_idle(struct ClemensHddCardContext *context, uint8_t err) {
    context->results[CLEM_CARD_HDD_RES_ERROR] = err;
    context->results[CLEM_CARD_HDD_RES_MISC] = 0xff;
    if (err != CLEM_CARD_HDD_PRODOS_ERR_NODEV && err != CLEM_CARD_HDD_PRODOS_ERR_NODEV) {
        CLEM_WARN("hddcard: device error %02X (state: %04X, command %02X )", context->state, err,
                  context->cmd_num);
    }
    context->state = CLEM_CARD_HDD_STATE_IDLE;
}

static void _clem_card_hdd_command(struct ClemensHddCardContext *context) {
    if (!context->hdd) {
        _clem_card_hdd_fail_idle(context, CLEM_CARD_HDD_PRODOS_ERR_NODEV);
        return;
    }
    switch (context->cmd_num) {
    case CLEM_CARD_HDD_COMMAND_STATUS:
        break;
    case CLEM_CARD_HDD_COMMAND_READ:
        break;
    case CLEM_CARD_HDD_COMMAND_WRITE:
        break;
    case CLEM_CARD_HDD_COMMAND_FORMAT:
        break;
    default:
        _clem_card_hdd_fail_idle(context, CLEM_CARD_HDD_PRODOS_ERR_IO);
        break;
    }
}

static const char *io_name(void *context) { return "hddcard"; }

static void io_reset(struct ClemensClock *clock, void *ctxptr) {
    struct ClemensHddCardContext *context = (struct ClemensHddCardContext *)(ctxptr);
    memset(context, 0, sizeof(struct ClemensHddCardContext));
}

static uint32_t io_sync(struct ClemensClock *clock, void *context) { return 0; }

static void io_read(struct ClemensClock *clock, uint8_t *data, uint8_t addr, uint8_t flags,
                    void *ctxptr) {
    struct ClemensHddCardContext *context = (struct ClemensHddCardContext *)(ctxptr);
    if (flags & CLEM_OP_IO_DEVSEL) {
        *data = slot7hdd_firmware_bytes[addr];
    } else if (!(addr & 0xf0)) {
        // I/O line c0x0 - c0xf
        addr &= 0x0f;
        switch (addr) {
        case CLEM_CARD_HDD_IO_CONTROL:
            if (context->state == CLEM_CARD_HDD_STATE_IDLE) {
                *data = CLEM_CARD_HDD_CONTROL_FLAG_OK;
                if (context->results[CLEM_CARD_HDD_RES_MISC] == 0xff)
                    *data |= CLEM_CARD_HDD_CONTROL_FLAG_ERROR;
            } else {
                *data = CLEM_CARD_HDD_CONTROL_FLAG_IN_PROGRESS;
            }
            break;
        case CLEM_CARD_HDD_IO_COMMAND:
            *data = context->results[CLEM_CARD_HDD_RES_ERROR];
            break;
        case CLEM_CARD_HDD_IO_RESULT0:
            *data = context->results[CLEM_CARD_HDD_RES_0];
            break;
        case CLEM_CARD_HDD_IO_RESULT1:
            *data = context->results[CLEM_CARD_HDD_RES_1];
            break;
        }
    }
}

static void io_write(struct ClemensClock *clock, uint8_t data, uint8_t addr, uint8_t flags,
                     void *ctxptr) {
    struct ClemensHddCardContext *context = (struct ClemensHddCardContext *)(ctxptr);
    if (!(flags & CLEM_OP_IO_DEVSEL) && !(addr & 0xf0)) {
        // I/O line c0x0 - c0xf
        addr &= 0x0f;
        switch (addr) {
        case CLEM_CARD_HDD_IO_CONTROL:
            if (context->state == CLEM_CARD_HDD_STATE_IDLE) {
                context->state = CLEM_CARD_HDD_STATE_COMMAND;
                context->cmd_num = CLEM_CARD_HDD_COMMAND_STATUS;
                context->unit_num = 0x00;
                context->results[CLEM_CARD_HDD_RES_MISC] = 0x00;
            }
            if (context->state == CLEM_CARD_HDD_STATE_COMMAND) {
                if (context->results[CLEM_CARD_HDD_RES_MISC] == 0x06) {
                    //  fire!
                    _clem_card_hdd_command(context);
                } else {
                    //  command not well formed
                    _clem_card_hdd_fail_idle(context, CLEM_CARD_HDD_PRODOS_ERR_IO);
                }
            } else {
                //  can interrupt operation if a command is in progress
                context->state = CLEM_CARD_HDD_STATE_IDLE;
            }
            break;
        case CLEM_CARD_HDD_IO_COMMAND:
            if (context->state == CLEM_CARD_HDD_STATE_COMMAND) {
                //  set up DMA if applicable for the fire command
                switch (context->results[CLEM_CARD_HDD_RES_MISC]) {
                case 0: // command byte
                    context->cmd_num = data;
                    break;
                case 1: // unit
                    context->unit_num = data;
                    break;
                case 2: // buffer ptr lo
                    context->dma_addr &= 0xff00;
                    context->dma_addr |= data;
                    break;
                case 3: // buffer ptr hi
                    context->dma_addr &= 0x00ff;
                    context->dma_addr |= ((uint16_t)(data) << 8);
                    break;
                case 4: // block number lo
                    context->block_num &= 0xff00;
                    context->block_num |= data;
                    break;
                case 5: // block number hi
                    context->block_num &= 0x00ff;
                    context->block_num |= ((uint16_t)(data) << 8);
                    break;
                default:
                    CLEM_WARN("hddcard: command overflow (%02X)", data);
                    break;
                }
                ++context->results[CLEM_CARD_HDD_RES_MISC];
            }
            break;
        case CLEM_CARD_HDD_IO_RESULT0: // nop
        case CLEM_CARD_HDD_IO_RESULT1: // nop
            break;
        }
    }
}

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
