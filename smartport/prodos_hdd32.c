#include "prodos_hdd32.h"
#include "clem_debug.h"
#include "serializer.h"

static uint8_t _do_reset(struct ClemensSmartPortDevice *context, unsigned delta_ns) {
    return CLEM_SMARTPORT_STATUS_CODE_OK;
}

static uint8_t _do_read_block(struct ClemensSmartPortDevice *context,
                              struct ClemensSmartPortPacket *packet, unsigned block_index,
                              unsigned delta_ns) {
    struct ClemensProdosHDD32 *hdd = (struct ClemensProdosHDD32 *)context->device_data;
    uint8_t *buffer = packet->contents;
    uint8_t result;
    hdd->current_block_index = block_index;
    result =
        (*hdd->read_block)(hdd->user_context, hdd->drive_index, hdd->current_block_index, buffer);
    if (result == CLEM_SMARTPORT_STATUS_CODE_OK) {
        /* Data returned, contents will be 512 bytes */
        packet->contents_length = 512;
    }
    return result;
}
/** Write a block to the device and store into packet when response state is returned */
static uint8_t _do_write_block(struct ClemensSmartPortDevice *context,
                               struct ClemensSmartPortPacket *packet, unsigned block_index,
                               unsigned delta_ns) {
    struct ClemensProdosHDD32 *hdd = (struct ClemensProdosHDD32 *)context->device_data;
    uint8_t result = CLEM_SMARTPORT_STATUS_CODE_OFFLINE;
    uint8_t *buffer = packet->contents;

    if (packet->type == kClemensSmartPortPacketType_Command) {
        hdd->current_block_index = block_index;
        result = CLEM_SMARTPORT_STATUS_CODE_OK;
    } else {
        result = (*hdd->write_block)(hdd->user_context, hdd->drive_index, hdd->current_block_index,
                                     buffer);
        if (result == CLEM_SMARTPORT_STATUS_CODE_OK) {
            packet->contents_length = 512;
        }
    }

    return result;
}

static uint8_t _do_status(struct ClemensSmartPortDevice *context,
                          struct ClemensSmartPortPacket *packet, uint8_t status,
                          unsigned delta_ns) {
    struct ClemensProdosHDD32 *hdd = (struct ClemensProdosHDD32 *)context->device_data;
    uint16_t clen = 0;
    uint8_t result = CLEM_SMARTPORT_STATUS_CODE_OK;

    switch (status) {
    case 0x00:                           /* 4 byte device status */
        packet->contents[clen++] = 0xf8; /* block, r/w, online, formattable */
        packet->contents[clen++] = (uint8_t)((hdd->block_limit) & 0xff);
        packet->contents[clen++] = (uint8_t)((hdd->block_limit >> 8) & 0xff);
        packet->contents[clen++] = (uint8_t)((hdd->block_limit >> 16) & 0xff);
        packet->contents_length = clen;
        break;
    case 0x01: /* Control Information Block */
        result = CLEM_SMARTPORT_STATUS_CODE_BAD_CTL;
    case 0x03: /* DIB */
        packet->contents[clen++] = 0xf8;
        packet->contents[clen++] = (uint8_t)((hdd->block_limit) & 0xff);
        packet->contents[clen++] = (uint8_t)((hdd->block_limit >> 8) & 0xff);
        packet->contents[clen++] = (uint8_t)((hdd->block_limit >> 16) & 0xff);
        packet->contents[clen++] = 12;
        packet->contents[clen++] = 'C';
        packet->contents[clen++] = 'L';
        packet->contents[clen++] = 'E';
        packet->contents[clen++] = 'M';
        packet->contents[clen++] = 'H';
        packet->contents[clen++] = 'D';
        packet->contents[clen++] = 'D';
        packet->contents[clen++] = '0';
        packet->contents[clen++] = '4';
        packet->contents[clen++] = '_';
        packet->contents[clen++] = 'S';
        packet->contents[clen++] = 'P';
        packet->contents[clen++] = ' ';
        packet->contents[clen++] = ' ';
        packet->contents[clen++] = ' ';
        packet->contents[clen++] = ' ';
        packet->contents[clen++] = 0x02;
        packet->contents[clen++] = 0x20; /* this is a very basic hard drive?*/
        packet->contents[clen++] = 0x01; /* firmware version */
        packet->contents[clen++] = 0x00;
        packet->contents_length = clen;
        break;
    }
    return result;
}

////////////////////////////////////////////////////////////////////////////////

void clem_smartport_prodos_hdd32_initialize(struct ClemensSmartPortDevice *device,
                                            struct ClemensProdosHDD32 *impl) {

    device->device_id = CLEM_SMARTPORT_DEVICE_ID_PRODOS_HDD32;
    device->device_data = impl;
    device->do_reset = &_do_reset;
    device->do_read_block = &_do_read_block;
    device->do_write_block = &_do_write_block;
    device->do_status = &_do_status;
    device->do_format = NULL;
}

void clem_smartport_prodos_hdd32_uninitialize(struct ClemensSmartPortDevice *device) {
    struct ClemensProdosHDD32 *hdd = (struct ClemensProdosHDD32 *)device->device_data;
    if (hdd->flush) {
        (*hdd->flush)(hdd->user_context, hdd->drive_index);
    }
}

void clem_smartport_prodos_hdd32_serialize(mpack_writer_t *writer,
                                           struct ClemensSmartPortDevice *device) {
    struct ClemensProdosHDD32 *hdd = (struct ClemensProdosHDD32 *)device->device_data;
}

void clem_smartport_prodos_hdd32_unserialize(mpack_reader_t *reader,
                                             struct ClemensSmartPortDevice *device,
                                             ClemensSerializerAllocateCb alloc_cb, void *context) {
    struct ClemensProdosHDD32 *hdd = (struct ClemensProdosHDD32 *)device->device_data;
}
