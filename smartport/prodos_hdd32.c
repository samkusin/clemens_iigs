#include "prodos_hdd32.h"
#include "clem_debug.h"
#include "serializer.h"

static uint8_t _do_reset(struct ClemensSmartPortDevice *context, unsigned delta_ns) {
    return CLEM_SMARTPORT_STATUS_CODE_OK;
}

static uint8_t _do_read_block(struct ClemensSmartPortDevice *context,
                              struct ClemensSmartPortPacket *packet, uint8_t unit_id,
                              unsigned delta_ns) {
    struct ClemensProdosHDD32 *hdd = (struct ClemensProdosHDD32 *)context->device_data;
    unsigned block_index = ((unsigned)(packet->contents[7]) << 16) |
                           ((unsigned)(packet->contents[6]) << 8) | packet->contents[7];
    uint8_t *buffer = packet->contents;
    uint8_t result = (*hdd->read_block)(hdd->user_context, 0, block_index, buffer);
    return result;
}
/** Write a block to the device and store into packet when response state is returned */
static uint8_t _do_write_block(struct ClemensSmartPortDevice *context,
                               struct ClemensSmartPortPacket *packet, uint8_t unit_id,
                               unsigned delta_ns) {
    struct ClemensProdosHDD32 *hdd = (struct ClemensProdosHDD32 *)context->device_data;
    return CLEM_SMARTPORT_STATUS_CODE_OFFLINE;
}

////////////////////////////////////////////////////////////////////////////////

void clem_smartport_prodos_hdd32_initialize(struct ClemensSmartPortDevice *device,
                                            struct ClemensProdosHDD32 *impl) {

    device->device_id = CLEM_SMARTPORT_DEVICE_ID_PRODOS_HDD32;
    device->device_data = impl;
    device->do_reset = &_do_reset;
    device->do_read_block = &_do_read_block;
    device->do_write_block = &_do_write_block;
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
