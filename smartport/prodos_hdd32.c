#include "prodos_hdd32.h"
#include "clem_debug.h"
#include "serializer.h"

void clem_smartport_prodos_hdd32_initialize(struct ClemensSmartPortDevice *device,
                                            struct ClemensProdosHDD32 *impl) {}
void clem_smartport_prodos_hdd32_uninitialize(struct ClemensSmartPortDevice *device) {}

void clem_smartport_prodos_hdd32_serialize(mpack_writer_t *writer,
                                           struct ClemensSmartPortDevice *device) {}

void clem_smartport_prodos_hdd32_unserialize(mpack_reader_t *reader,
                                             struct ClemensSmartPortDevice *device,
                                             ClemensSerializerAllocateCb alloc_cb, void *context) {}
