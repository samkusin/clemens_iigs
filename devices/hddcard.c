#include "hddcard.h"
#include "clem_shared.h"
#include "clem_smartport.h"
#include "prodos_hdd32.h"
#include "slot7hdd_firmware.h"

#include "clem_debug.h"
#include "serializer.h"

#include <stdlib.h>
#include <string.h>

#define CLEM_CARD_HDD_STATE_IDLE      0x00
#define CLEM_CARD_HDD_STATE_COMMAND   0x01
#define CLEM_CARD_HDD_STATE_DMA_W     0x02
#define CLEM_CARD_HDD_STATE_DMA_R     0x04
#define CLEM_CARD_HDD_STATE_DMA       (CLEM_CARD_HDD_STATE_DMA_W + CLEM_CARD_HDD_STATE_DMA_R)
#define CLEM_CARD_HDD_STATE_FORMAT    0x08
#define CLEM_CARD_HDD_STATE_SMARTPORT 0x10

#define CLEM_CARD_HDD_DRIVE_LIMIT 2

#define CLEM_CARD_HDD_INDEX_NONE     0xff
#define CLEM_CARD_HDD_INDEX_SWITCHED 0xfe
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

#define CLEM_CARD_HDD_PRODOS_ERR_NONE          0x00
#define CLEM_CARD_HDD_PRODOS_ERR_IO            0x27
#define CLEM_CARD_HDD_PRODOS_ERR_NODEV         0x28
#define CLEM_CARD_HDD_PRODOS_ERR_WPROT         0x2b
#define CLEM_CARD_HDD_PRODOS_ERR_DISK_SWITCHED 0x2e

#define CLEM_CARD_HDD_CONTROL_FLAG_OK          0x00
#define CLEM_CARD_HDD_CONTROL_FLAG_ERROR       0x01
#define CLEM_CARD_HDD_CONTROL_FLAG_IN_PROGRESS 0x80

#define CLEM_CARD_HDD_COMMAND_STATUS 0x00
#define CLEM_CARD_HDD_COMMAND_READ   0x01
#define CLEM_CARD_HDD_COMMAND_WRITE  0x02
#define CLEM_CARD_HDD_COMMAND_FORMAT 0x03

#define CLEM_CARD_HDD_MISC_PRODOS_COMMAND_READY    0x06
#define CLEM_CARD_HDD_MISC_SMARTPORT_COMMAND_READY 0x80

struct ClemensHddCardContext {
    struct ClemensProdosHDD32 *hdd[CLEM_CARD_HDD_DRIVE_LIMIT];
    unsigned state;
    uint8_t cmd_num;
    uint8_t unit_num;
    uint8_t smartport_param_cnt;
    uint8_t smartport_param_byte;
    uint8_t write_prot[CLEM_CARD_HDD_DRIVE_LIMIT];
    uint8_t drive_index[CLEM_CARD_HDD_DRIVE_LIMIT];
    uint8_t drive_used[CLEM_CARD_HDD_DRIVE_LIMIT];
    uint16_t dma_addr;
    uint16_t dma_offset;
    uint16_t dma_size;
    uint32_t block_num;
    uint8_t results[4]; // used for returning results in IO_RESULT0/1, error code
    uint32_t smartport_outp_blocknum;
    uint16_t smartport_outp_ptr;
    uint8_t smartport_outp_code;
    uint8_t block_data[512];
};

static void inline _clem_card_hdd_fail_idle(struct ClemensHddCardContext *context, uint8_t err) {
    context->results[CLEM_CARD_HDD_RES_ERROR] = err;
    context->results[CLEM_CARD_HDD_RES_MISC] = 0xff;
    if (err != CLEM_CARD_HDD_PRODOS_ERR_NODEV && err != CLEM_CARD_HDD_PRODOS_ERR_NODEV) {
        CLEM_WARN("hddcard: device error %02X (state: %04X, command %02X )", err, context->state,
                  context->cmd_num);
    }
    context->state = CLEM_CARD_HDD_STATE_IDLE;
    context->cmd_num = CLEM_CARD_HDD_COMMAND_STATUS;
}

static void inline _clem_card_hdd_ok(struct ClemensHddCardContext *context) {
    context->results[CLEM_CARD_HDD_RES_ERROR] = CLEM_CARD_HDD_PRODOS_ERR_NONE;
    context->results[CLEM_CARD_HDD_RES_MISC] = 0x00;
    context->state = CLEM_CARD_HDD_STATE_IDLE;
    context->cmd_num = CLEM_CARD_HDD_COMMAND_STATUS;
}

static bool inline _clem_card_hdd_is_smartport_cmd(struct ClemensHddCardContext *context) {
    return context->smartport_param_cnt != 0xff;
}

static uint8_t _clem_card_hdd_select_drive(struct ClemensHddCardContext *context) {
    CLEM_ASSERT(context->unit_num > 0);
    return _clem_card_hdd_is_smartport_cmd(context) ? context->unit_num - 1
                                                    : (context->unit_num >> 7);
}

static void _clem_card_hdd_command(struct ClemensHddCardContext *context) {
    uint16_t prodos_block_max;
    uint8_t drive_index = _clem_card_hdd_select_drive(context);
    if (!context->hdd[drive_index]) {
        _clem_card_hdd_fail_idle(context, CLEM_CARD_HDD_PRODOS_ERR_NODEV);
        return;
    }
    prodos_block_max = context->hdd[drive_index]->block_limit;
    if (context->hdd[drive_index]->block_limit >= 0x10000) {
        prodos_block_max = 0xffff;
    }
    switch (context->cmd_num) {
    case CLEM_CARD_HDD_COMMAND_STATUS:
        context->results[CLEM_CARD_HDD_RES_0] = (uint8_t)(prodos_block_max & 0xff);
        context->results[CLEM_CARD_HDD_RES_1] = (uint8_t)((prodos_block_max >> 8) & 0xff);
        _clem_card_hdd_ok(context);
        break;
    case CLEM_CARD_HDD_COMMAND_READ:
        context->dma_offset = 0;
        context->dma_size = 512;
        context->state = CLEM_CARD_HDD_STATE_DMA_W; // write to memory from reading disk
        context->results[CLEM_CARD_HDD_RES_0] = (*context->hdd[drive_index]->read_block)(
            context->hdd[drive_index]->user_context, 0, context->block_num, context->block_data);
        if (context->results[CLEM_CARD_HDD_RES_0] != CLEM_SMARTPORT_STATUS_CODE_OK) {
            _clem_card_hdd_fail_idle(context, CLEM_CARD_HDD_PRODOS_ERR_IO);
        }
        break;
    case CLEM_CARD_HDD_COMMAND_WRITE:
        context->dma_offset = 0;
        context->dma_size = 512;
        context->state = CLEM_CARD_HDD_STATE_DMA_R; // read from memory to write on disk
        if (context->block_num > prodos_block_max) {
            _clem_card_hdd_fail_idle(context, CLEM_CARD_HDD_PRODOS_ERR_IO);
        }
        break;
    case CLEM_CARD_HDD_COMMAND_FORMAT:
        // used as our block index until we hit block limit
        context->block_num = 0;
        // used to count current line (8 byte string) in 512 byte block
        context->dma_offset = 0;
        memset(context->block_data, 0, sizeof(context->block_data));
        context->state = CLEM_CARD_HDD_STATE_FORMAT;
        break;
    default:
        _clem_card_hdd_fail_idle(context, CLEM_CARD_HDD_PRODOS_ERR_IO);
        break;
    }
}

static uint8_t _clem_card_hdd_smartport_status_byte(struct ClemensHddCardContext *context) {
    uint8_t drive_index = _clem_card_hdd_select_drive(context);
    uint8_t tmp0 = 0xe8; // block device and read/write/format
    if (context->hdd[drive_index]) {
        tmp0 |= 0x10; // online
        if (context->write_prot[drive_index])
            tmp0 |= 0x04;
        // See tech note #25 on bit 0 = disk switched
        if (context->drive_index[drive_index] == CLEM_CARD_HDD_INDEX_SWITCHED) {
            tmp0 |= 0x01;
            context->drive_index[drive_index] = drive_index;
        }
    }
    return tmp0;
}

static uint16_t _clem_card_hdd_smartport_block_size(struct ClemensHddCardContext *context,
                                                    uint16_t dma_size) {
    unsigned prodos_block_max;
    uint8_t drive_index = _clem_card_hdd_select_drive(context);
    if (context->hdd[drive_index]) {
        prodos_block_max = context->hdd[drive_index]->block_limit;
    } else {
        prodos_block_max = 0;
    }
    context->block_data[dma_size++] = (uint8_t)(prodos_block_max & 0xff);
    context->block_data[dma_size++] = (uint8_t)((prodos_block_max >> 8) & 0xff);
    context->block_data[dma_size++] = (uint8_t)((prodos_block_max >> 16) & 0xff);
    return dma_size;
}

static uint8_t _clem_card_hdd_smartport_status(struct ClemensHddCardContext *context) {
    uint8_t err_code = CLEM_SMARTPORT_STATUS_CODE_OK;
    uint8_t drive_index;
    uint16_t dma_size = 0;

    context->dma_offset = 0;
    context->dma_addr = context->smartport_outp_ptr;

    switch (context->smartport_outp_code) {
    case 0x00: // device status bytes
        if (context->unit_num == 0) {
            //  SmartPort status
            //  See SmartPort tech note #2 for further definition of this call
            //  bit 6 of byte 1 should be 1 if there are no interrupts
            context->block_data[dma_size++] = CLEM_CARD_HDD_DRIVE_LIMIT;
            context->block_data[dma_size++] = 0x40; // no interrupts
            context->block_data[dma_size++] = 0;
            context->block_data[dma_size++] = 0;
            context->block_data[dma_size++] = 0;
            context->block_data[dma_size++] = 0;
            context->block_data[dma_size++] = 0;
            context->block_data[dma_size++] = 0;
        } else {
            context->block_data[dma_size++] = _clem_card_hdd_smartport_status_byte(context);
            dma_size = _clem_card_hdd_smartport_block_size(context, dma_size);
        }
        break;
    case 0x01: // DCB?
        break;
    case 0x03: // DIB
        if (context->unit_num != 0) {
            context->block_data[dma_size++] = _clem_card_hdd_smartport_status_byte(context);
            dma_size = _clem_card_hdd_smartport_block_size(context, dma_size);
            context->block_data[dma_size++] = 11;
            context->block_data[dma_size++] = 'C';
            context->block_data[dma_size++] = 'L';
            context->block_data[dma_size++] = 'E';
            context->block_data[dma_size++] = 'M';
            context->block_data[dma_size++] = 'H';
            context->block_data[dma_size++] = 'D';
            context->block_data[dma_size++] = 'D';
            context->block_data[dma_size++] = 'C';
            context->block_data[dma_size++] = 'A';
            context->block_data[dma_size++] = 'R';
            context->block_data[dma_size++] = 'D';
            context->block_data[dma_size++] = ' ';
            context->block_data[dma_size++] = ' ';
            context->block_data[dma_size++] = ' ';
            context->block_data[dma_size++] = ' ';
            context->block_data[dma_size++] = ' ';
            // TODO: interrupts?  firmware docs say this is only supported on the //c
            context->block_data[dma_size++] = 0x02; // HDD type
            context->block_data[dma_size++] = 0x40; // Removeable and disk switching
            context->block_data[dma_size++] = 0x01; // version 0.1?
            context->block_data[dma_size++] = 0x00; // version 0.1?
        } else {
            err_code = CLEM_SMARTPORT_STATUS_CODE_BAD_CTL;
        }
        break;
    default:
        err_code = CLEM_SMARTPORT_STATUS_CODE_BAD_CTL;
        break;
    }
    context->dma_size = dma_size;
    if (context->dma_size > 0) {
        context->state = CLEM_CARD_HDD_STATE_DMA_W;
    }
    return err_code;
}

static void _clem_card_hdd_smartport(struct ClemensHddCardContext *context) {
    uint16_t prodos_block_max;
    uint8_t drive_index;
    uint8_t err_code;

    switch (context->cmd_num) {
    case CLEM_SMARTPORT_COMMAND_STATUS:
        //CLEM_LOG("hddcard: smartport %02x STATUS %02x => %04x", context->unit_num,
        //         context->smartport_outp_code, context->smartport_outp_ptr);
        err_code = _clem_card_hdd_smartport_status(context);
        if (err_code != CLEM_SMARTPORT_STATUS_CODE_OK) {
            _clem_card_hdd_fail_idle(context, err_code);
        }
        break;
    case CLEM_SMARTPORT_COMMAND_READBLOCK:
        //CLEM_LOG("hddcard: smartport %02x READBLOCK %u => %04x", context->unit_num,
        //         context->smartport_outp_blocknum, context->smartport_outp_ptr);
        if (context->unit_num == 0) {
            _clem_card_hdd_fail_idle(context, CLEM_CARD_HDD_PRODOS_ERR_NODEV);
        } else {
            drive_index = _clem_card_hdd_select_drive(context);
            if (context->hdd[drive_index] != NULL) {
                context->dma_offset = 0;
                context->dma_size = 512;
                context->block_num = context->smartport_outp_blocknum;
                context->dma_addr = context->smartport_outp_ptr;
                context->state = CLEM_CARD_HDD_STATE_DMA_W; // write to memory from reading disk
                context->results[CLEM_CARD_HDD_RES_0] = (*context->hdd[drive_index]->read_block)(
                    context->hdd[drive_index]->user_context, 0, context->block_num,
                    context->block_data);
                if (context->results[CLEM_CARD_HDD_RES_0] != CLEM_SMARTPORT_STATUS_CODE_OK) {
                    _clem_card_hdd_fail_idle(context, context->results[CLEM_CARD_HDD_RES_0]);
                }
            } else {
                _clem_card_hdd_fail_idle(context, CLEM_CARD_HDD_PRODOS_ERR_NODEV);
            }
        }
        break;
    case CLEM_SMARTPORT_COMMAND_WRITEBLOCK:
        // CLEM_LOG("hddcard: smartport %02x WRITEBLOCK %u <= %04x", context->unit_num,
        //          context->smartport_outp_blocknum, context->smartport_outp_ptr);
        if (context->unit_num == 0) {
            _clem_card_hdd_fail_idle(context, CLEM_CARD_HDD_PRODOS_ERR_NODEV);
        } else {
            drive_index = _clem_card_hdd_select_drive(context);
            if (context->hdd[drive_index] != NULL) {
                prodos_block_max = context->hdd[drive_index]->block_limit;
                if (context->hdd[drive_index]->block_limit >= 0x10000) {
                    prodos_block_max = 0xffff;
                }
                context->dma_offset = 0;
                context->dma_size = 512;
                context->block_num = context->smartport_outp_blocknum;
                context->dma_addr = context->smartport_outp_ptr;
                context->state = CLEM_CARD_HDD_STATE_DMA_R; // read from memory to write on disk
                if (context->block_num > prodos_block_max) {
                    _clem_card_hdd_fail_idle(context, CLEM_SMARTPORT_STATUS_CODE_INVALID_BLOCK);
                }
            } else {
                _clem_card_hdd_fail_idle(context, CLEM_CARD_HDD_PRODOS_ERR_NODEV);
            }
        }
        break;
    case CLEM_SMARTPORT_COMMAND_CONTROL:
        CLEM_LOG("hddcard: smartport %02x CONTROL %u <= %04x", context->unit_num,
                 context->smartport_outp_code, context->smartport_outp_ptr);
        _clem_card_hdd_fail_idle(context, CLEM_SMARTPORT_STATUS_CODE_BUS_ERR);
        break;
    case CLEM_SMARTPORT_COMMAND_FORMAT:
        CLEM_LOG("hddcard: smartport %02x FORMAT", context->unit_num);
        _clem_card_hdd_fail_idle(context, CLEM_SMARTPORT_STATUS_CODE_BUS_ERR);
        break;
    case CLEM_SMARTPORT_COMMAND_INIT:
        CLEM_LOG("hddcard: smartport %02x INIT", context->unit_num);
        _clem_card_hdd_fail_idle(context, CLEM_SMARTPORT_STATUS_CODE_BUS_ERR);
        break;
    }
}

static void _clem_card_hdd_setup_smartport(struct ClemensHddCardContext *context, uint8_t data) {
    switch (context->cmd_num) {
    case CLEM_SMARTPORT_COMMAND_STATUS:
        if (context->smartport_param_byte == 0) {
            context->smartport_outp_ptr = data;
        } else if (context->smartport_param_byte == 1) {
            context->smartport_outp_ptr |= ((uint16_t)(data) << 8);
            context->smartport_param_cnt--;
        } else if (context->smartport_param_byte == 2) {
            context->smartport_outp_code = data;
            context->smartport_param_cnt--;
        }
        break;
    case CLEM_SMARTPORT_COMMAND_READBLOCK:
        if (context->smartport_param_byte == 0) {
            context->smartport_outp_ptr = data;
        } else if (context->smartport_param_byte == 1) {
            context->smartport_outp_ptr |= ((uint16_t)(data) << 8);
            context->smartport_param_cnt--;
        } else if (context->smartport_param_byte == 2) {
            context->smartport_outp_blocknum = data;
        } else if (context->smartport_param_byte == 3) {
            context->smartport_outp_blocknum |= ((uint16_t)(data) << 8);
        } else if (context->smartport_param_byte == 4) {
            context->smartport_outp_blocknum |= ((uint32_t)(data) << 16);
            context->smartport_param_cnt--;
        }
        break;
    case CLEM_SMARTPORT_COMMAND_WRITEBLOCK:
        if (context->smartport_param_byte == 0) {
            context->smartport_outp_ptr = data;
        } else if (context->smartport_param_byte == 1) {
            context->smartport_outp_ptr |= ((uint16_t)(data) << 8);
            context->smartport_param_cnt--;
        } else if (context->smartport_param_byte == 2) {
            context->smartport_outp_blocknum = data;
        } else if (context->smartport_param_byte == 3) {
            context->smartport_outp_blocknum |= ((uint16_t)(data) << 8);
        } else if (context->smartport_param_byte == 4) {
            context->smartport_outp_blocknum |= ((uint32_t)(data) << 16);
            context->smartport_param_cnt--;
        }
        break;
    case CLEM_SMARTPORT_COMMAND_FORMAT:
        CLEM_ASSERT(false); // there are no args
        break;
    case CLEM_SMARTPORT_COMMAND_CONTROL:
        if (context->smartport_param_byte == 0) {
            context->smartport_outp_ptr = data;
        } else if (context->smartport_param_byte == 1) {
            context->smartport_outp_ptr |= ((uint16_t)(data) << 8);
            context->smartport_param_cnt--;
        } else if (context->smartport_param_byte == 2) {
            context->smartport_outp_code = data;
            context->smartport_param_cnt--;
        }
        break;
    case CLEM_SMARTPORT_COMMAND_INIT:
        CLEM_ASSERT(false); // there are no args
        break;
    }
    context->smartport_param_byte++;
}

static const char *io_name(void *context) { return "hddcard"; }

static void io_reset(struct ClemensClock *clock, void *ctxptr) {
    struct ClemensHddCardContext *context = (struct ClemensHddCardContext *)(ctxptr);

    //  HDD mount NOT reset (hdd, write_prot, drive_index)
    context->state = CLEM_CARD_HDD_STATE_IDLE;
    context->cmd_num = 0;
    context->unit_num = 0;
    context->dma_addr = 0;
    context->dma_offset = 0;
    context->dma_size = 0;
    context->block_num = 0;
    context->smartport_param_byte = 0;
    context->smartport_param_cnt = 0;
    memset(context->results, 0, sizeof(context->results));
}

static uint32_t io_sync(struct ClemensClock *clock, void *ctxptr) {
    struct ClemensHddCardContext *context = (struct ClemensHddCardContext *)(ctxptr);
    unsigned i;
    uint8_t drive_index;
    if (context->state != CLEM_CARD_HDD_STATE_FORMAT) {
        return (context->state & CLEM_CARD_HDD_STATE_DMA) ? CLEM_CARD_DMA : 0;
    }
    drive_index = _clem_card_hdd_select_drive(context);
    if (!context->hdd[drive_index]) {
        _clem_card_hdd_fail_idle(context, CLEM_CARD_HDD_PRODOS_ERR_NODEV);
    } else if (context->block_num < context->hdd[drive_index]->block_limit) {
        if (++context->dma_offset >= 64) {
            //  TODO: this seems  like an unnecesary delay (64 machine cycles per block?)
            context->dma_offset = 0;
            context->results[CLEM_CARD_HDD_RES_0] = (*context->hdd[drive_index]->write_block)(
                context->hdd[drive_index]->user_context, 0, context->block_num,
                context->block_data);
            if (context->results[CLEM_CARD_HDD_RES_0] != CLEM_SMARTPORT_STATUS_CODE_OK) {
                _clem_card_hdd_fail_idle(context, CLEM_CARD_HDD_PRODOS_ERR_IO);
            }
            ++context->block_num;
        }
    } else {
        _clem_card_hdd_ok(context);
    }
    return 0;
}

static uint32_t io_dma(uint8_t *data_bank, uint16_t *adr, uint8_t is_adr_bus, void *ctxptr) {
    // MISC = 0; is_adr_bus = true; *data_bank = 0x00 (bank); adr = dma_offset; MISC++
    // MISC = 1; is_adr_bus = false; *data_bank = data; dma_offset++;
    // if dma_offset == 512 switch to IDLE, MISC = 0
    // return 0 for DMA read
    // return 1 for DMA write
    struct ClemensHddCardContext *context = (struct ClemensHddCardContext *)(ctxptr);
    uint32_t out;
    if (context->dma_offset >= (uint16_t)sizeof(context->block_data)) {
        return 0; // default to read, as reading is a non-destructive event
    }
    out = (context->state == CLEM_CARD_HDD_STATE_DMA_W) ? 1 : 0;
    if (is_adr_bus) {
        *data_bank = 0x00;
        *adr = context->dma_addr + context->dma_offset;
    } else {
        if (context->state == CLEM_CARD_HDD_STATE_DMA_W) {
            *data_bank = context->block_data[context->dma_offset++];
        } else {
            context->block_data[context->dma_offset++] = *data_bank;
        }
    }
    if (context->dma_offset == context->dma_size) {
        if (context->cmd_num == CLEM_CARD_HDD_COMMAND_WRITE) {
            // commit block
            uint8_t drive_index = _clem_card_hdd_select_drive(context);
            if (context->write_prot[drive_index]) {
                _clem_card_hdd_fail_idle(context, CLEM_CARD_HDD_PRODOS_ERR_WPROT);
            } else {
                context->results[CLEM_CARD_HDD_RES_0] = (*context->hdd[drive_index]->write_block)(
                    context->hdd[drive_index]->user_context, 0, context->block_num,
                    context->block_data);
                if (context->results[CLEM_CARD_HDD_RES_0] != CLEM_SMARTPORT_STATUS_CODE_OK) {
                    _clem_card_hdd_fail_idle(context, CLEM_CARD_HDD_PRODOS_ERR_IO);
                } else {
                    if (_clem_card_hdd_is_smartport_cmd(context)) {
                        context->results[CLEM_CARD_HDD_RES_0] =
                            (uint8_t)(context->dma_offset & 0xff);
                        context->results[CLEM_CARD_HDD_RES_1] =
                            (uint8_t)((context->dma_offset >> 8) & 0xff);
                    }
                    _clem_card_hdd_ok(context);
                }
            }
        } else {
            if (_clem_card_hdd_is_smartport_cmd(context)) {
                context->results[CLEM_CARD_HDD_RES_0] = (uint8_t)(context->dma_offset & 0xff);
                context->results[CLEM_CARD_HDD_RES_1] =
                    (uint8_t)((context->dma_offset >> 8) & 0xff);
            }
            _clem_card_hdd_ok(context);
        }
    }
    return out;
}

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
            } else if ((context->state != CLEM_CARD_HDD_STATE_COMMAND &&
                        context->state != CLEM_CARD_HDD_STATE_SMARTPORT) ||
                       context->results[CLEM_CARD_HDD_RES_MISC] > 0x00) {
                *data = CLEM_CARD_HDD_CONTROL_FLAG_IN_PROGRESS;
                // smartport mode will
                if (context->state == CLEM_CARD_HDD_STATE_SMARTPORT &&
                    context->results[CLEM_CARD_HDD_RES_MISC] >=
                        CLEM_CARD_HDD_MISC_SMARTPORT_COMMAND_READY) {
                    *data = CLEM_CARD_HDD_CONTROL_FLAG_OK;
                } else if (context->state == CLEM_CARD_HDD_STATE_COMMAND &&
                           context->results[CLEM_CARD_HDD_RES_MISC] >=
                               CLEM_CARD_HDD_MISC_PRODOS_COMMAND_READY) {
                    *data = CLEM_CARD_HDD_CONTROL_FLAG_OK;
                }
            }
            break;
        case CLEM_CARD_HDD_IO_COMMAND:
            *data = context->results[CLEM_CARD_HDD_RES_ERROR];
            if (!(flags & CLEM_OP_IO_NO_OP)) {
                context->results[CLEM_CARD_HDD_RES_ERROR] = CLEM_CARD_HDD_PRODOS_ERR_NONE;
            }
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
                context->state =
                    !data ? CLEM_CARD_HDD_STATE_COMMAND : CLEM_CARD_HDD_STATE_SMARTPORT;
                context->cmd_num = CLEM_CARD_HDD_COMMAND_STATUS;
                context->unit_num = 0x00;
                context->smartport_param_cnt = 0xff;
                context->smartport_param_byte = 0x00;
                context->results[CLEM_CARD_HDD_RES_MISC] = 0x00;
            } else if (context->state == CLEM_CARD_HDD_STATE_COMMAND) {
                //  firmware sets data == 0x80, but since this is a state machine
                //  we can just handle writes
                if (context->results[CLEM_CARD_HDD_RES_MISC] ==
                    CLEM_CARD_HDD_MISC_PRODOS_COMMAND_READY) {
                    //  fire!
                    _clem_card_hdd_command(context);
                } else {
                    //  command not well formed
                    _clem_card_hdd_fail_idle(context, CLEM_CARD_HDD_PRODOS_ERR_IO);
                }
            } else if (context->state == CLEM_CARD_HDD_STATE_SMARTPORT) {
                if (context->results[CLEM_CARD_HDD_RES_MISC] >=
                    CLEM_CARD_HDD_MISC_SMARTPORT_COMMAND_READY) {
                    //  fire!
                    _clem_card_hdd_smartport(context);
                } else {
                    //  command not well formed
                    _clem_card_hdd_fail_idle(context, CLEM_SMARTPORT_STATUS_CODE_BUS_ERR);
                }

            } else {
                //  can interrupt operation if a command is in progress
                _clem_card_hdd_ok(context);
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
            } else if (context->state == CLEM_CARD_HDD_STATE_SMARTPORT) {
                switch (context->results[CLEM_CARD_HDD_RES_MISC]) {
                case 0: //  command
                    context->cmd_num = data;
                    break;
                case 1: //  parameter count
                    context->smartport_param_cnt = data;
                    break;
                case 2: //  unit number
                    context->unit_num = data;
                    context->smartport_param_cnt--;
                    break;
                default:
                    _clem_card_hdd_setup_smartport(context, data);
                    break;
                }
                if (context->smartport_param_cnt > 0) {
                    ++context->results[CLEM_CARD_HDD_RES_MISC];
                } else {
                    context->results[CLEM_CARD_HDD_RES_MISC] =
                        CLEM_CARD_HDD_MISC_SMARTPORT_COMMAND_READY;
                }
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
    unsigned i;
    for (i = 0; i < CLEM_CARD_HDD_DRIVE_LIMIT; i++) {
        context->drive_index[i] = CLEM_CARD_HDD_INDEX_NONE;
    }
    card->context = context;
    card->io_reset = &io_reset;
    card->io_sync = &io_sync;
    card->io_read = &io_read;
    card->io_write = &io_write;
    card->io_name = &io_name;
    card->io_dma = &io_dma;
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

void clem_card_hdd_mount(ClemensCard *card, ClemensProdosHDD32 *hdd, uint8_t drive_index) {
    struct ClemensHddCardContext *context = (struct ClemensHddCardContext *)(card->context);
    if (hdd) {
        context->hdd[drive_index - 1] = hdd;
        // context->drive_index[drive_index - 1] = CLEM_CARD_HDD_INDEX_SWITCHED;
        if (context->drive_index[drive_index - 1] != CLEM_CARD_HDD_INDEX_SWITCHED) {
            context->drive_index[drive_index - 1] = drive_index;
        }
        context->drive_used[drive_index - 1] = 1;
    }
}

ClemensProdosHDD32 *clem_card_hdd_unmount(ClemensCard *card, uint8_t drive_index) {
    struct ClemensHddCardContext *context = (struct ClemensHddCardContext *)(card->context);
    struct ClemensProdosHDD32 *hdd = context->hdd[drive_index - 1];
    context->drive_index[drive_index - 1] = CLEM_CARD_HDD_INDEX_SWITCHED;
    context->hdd[drive_index - 1] = NULL;
    context->drive_used[drive_index - 1] = 0;
    return hdd;
}

unsigned clem_card_hdd_get_status(ClemensCard *card, uint8_t drive_index) {
    struct ClemensHddCardContext *context = (struct ClemensHddCardContext *)(card->context);
    unsigned status = 0;
    if (context->cmd_num == CLEM_CARD_HDD_COMMAND_READ ||
        context->cmd_num == CLEM_CARD_HDD_COMMAND_WRITE) {
        uint8_t cmd_drive_index = _clem_card_hdd_is_smartport_cmd(context);
        if (cmd_drive_index == drive_index - 1) {
            status |= CLEM_CARD_HDD_STATUS_DRIVE_ON;
        }
    }
    return status;
}

void clem_card_hdd_lock(ClemensCard *card, uint8_t lock, uint8_t drive_index) {
    struct ClemensHddCardContext *context = (struct ClemensHddCardContext *)(card->context);
    //  note, this will take effect on the next block write
    context->write_prot[drive_index - 1] = lock ? 1 : 0;
}

uint8_t clem_card_hdd_drive_index_has_image(ClemensCard *card, uint8_t drive_index) {
    struct ClemensHddCardContext *context = (struct ClemensHddCardContext *)(card->context);
    return context->drive_used[drive_index - 1];
}

static struct ClemensSerializerRecord kCard[] = {
    // hdd is fixed up after the initial load by the owning system
    CLEM_SERIALIZER_RECORD_UINT32(struct ClemensHddCardContext, state),
    CLEM_SERIALIZER_RECORD_UINT8(struct ClemensHddCardContext, cmd_num),
    CLEM_SERIALIZER_RECORD_UINT8(struct ClemensHddCardContext, unit_num),
    CLEM_SERIALIZER_RECORD_UINT8(struct ClemensHddCardContext, smartport_param_cnt),
    CLEM_SERIALIZER_RECORD_UINT8(struct ClemensHddCardContext, smartport_param_byte),
    CLEM_SERIALIZER_RECORD_ARRAY(struct ClemensHddCardContext, kClemensSerializerTypeUInt8,
                                 write_prot, CLEM_CARD_HDD_DRIVE_LIMIT, 0),
    CLEM_SERIALIZER_RECORD_ARRAY(struct ClemensHddCardContext, kClemensSerializerTypeUInt8,
                                 drive_index, CLEM_CARD_HDD_DRIVE_LIMIT, 0),
    CLEM_SERIALIZER_RECORD_ARRAY(struct ClemensHddCardContext, kClemensSerializerTypeUInt8,
                                 drive_used, CLEM_CARD_HDD_DRIVE_LIMIT, 0),
    CLEM_SERIALIZER_RECORD_UINT16(struct ClemensHddCardContext, dma_addr),
    CLEM_SERIALIZER_RECORD_UINT16(struct ClemensHddCardContext, dma_offset),
    CLEM_SERIALIZER_RECORD_UINT16(struct ClemensHddCardContext, dma_size),
    CLEM_SERIALIZER_RECORD_UINT32(struct ClemensHddCardContext, block_num),
    CLEM_SERIALIZER_RECORD_ARRAY(struct ClemensHddCardContext, kClemensSerializerTypeUInt8, results,
                                 4, 0),
    CLEM_SERIALIZER_RECORD_UINT32(struct ClemensHddCardContext, smartport_outp_blocknum),
    CLEM_SERIALIZER_RECORD_UINT16(struct ClemensHddCardContext, smartport_outp_ptr),
    CLEM_SERIALIZER_RECORD_UINT8(struct ClemensHddCardContext, smartport_outp_code),
    CLEM_SERIALIZER_RECORD_ARRAY(struct ClemensHddCardContext, kClemensSerializerTypeUInt8,
                                 block_data, 512, 0),
    CLEM_SERIALIZER_RECORD_EMPTY()};

void clem_card_hdd_serialize(mpack_writer_t *writer, ClemensCard *card) {
    struct ClemensSerializerRecord root;
    memset(&root, 0, sizeof(root));
    root.type = kClemensSerializerTypeRoot;
    root.records = &kCard[0];
    clemens_serialize_object(writer, (uintptr_t)card->context, &root);
}

void clem_card_hdd_unserialize(mpack_reader_t *reader, ClemensCard *card,
                               ClemensSerializerAllocateCb alloc_cb, void *context) {
    struct ClemensSerializerRecord root;
    memset(&root, 0, sizeof(root));
    root.type = kClemensSerializerTypeRoot;
    root.records = &kCard[0];
    clemens_unserialize_object(reader, (uintptr_t)card->context, &root, alloc_cb, context);
}
