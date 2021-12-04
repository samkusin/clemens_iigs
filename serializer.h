#ifndef CLEM_SERIALIZER_H
#define CLEM_SERIALIZER_H

#include "clem_types.h"
#include "external/mpack.h"

#define CLEM_SERIALIZER_INVALID_RECORD      (UINT_MAX)

#ifdef __cplusplus
extern "C" {
#endif

enum ClemensSerializerType {
    kClemensSerializerTypeEmpty,
    kClemensSerializerTypeRoot,
    kClemensSerializerTypeBool,
    kClemensSerializerTypeUInt8,
    kClemensSerializerTypeUInt16,
    kClemensSerializerTypeUInt32,
    kClemensSerializerTypeUInt64,
    kClemensSerializerTypeInt16,
    kClemensSerializerTypeInt32,
    kClemensSerializerTypeFloat,
    kClemensSerializerTypeDuration,
    kClemensSerializerTypeClocks,
    kClemensSerializerTypeBlob,
    kClemensSerializerTypeArray,
    kClemensSerializerTypeObject,
    kClemensSerializerTypeCustom,
};

struct ClemensSerializerRecord {
    const char* name;
    enum ClemensSerializerType type;
    enum ClemensSerializerType array_type;
    unsigned offset;
    unsigned size;
    unsigned param;

    struct ClemensSerializerRecord* records;
};


typedef uint8_t*(*ClemensSerializerAllocateCb)(unsigned, void*);

/**
 * @brief
 *
 * @param machine
 * @param writer
 * @return mpack_writer_t*
 */
mpack_writer_t* clemens_serialize_machine(
    mpack_writer_t* writer,
    ClemensMachine* machine);

/**
 * @brief
 *
 * @param machine
 * @param writer
 * @return mpack_reader_t*
 */
mpack_reader_t* clemens_unserialize_machine(
    mpack_reader_t* reader,
    ClemensMachine* machine,
    ClemensSerializerAllocateCb alloc_cb,
    void* context);

/* The following APIs are provided for completeness, but they are typically
   not called directly by the application (called instead by the main machine
   APIs above.)
*/
unsigned clemens_serialize_object(
    mpack_writer_t* writer,
    uintptr_t data_adr,
    const struct ClemensSerializerRecord* record);

unsigned clemens_serialize_array(
    mpack_writer_t* writer,
    uintptr_t data_adr,
    const struct ClemensSerializerRecord* record);

unsigned clemens_serialize_record(
    mpack_writer_t* writer,
    uintptr_t data_adr,
    const struct ClemensSerializerRecord* record);

unsigned clemens_unserialize_object(
    mpack_reader_t* reader,
    uintptr_t data_adr,
    const struct ClemensSerializerRecord* record,
    ClemensSerializerAllocateCb alloc_cb,
    void* context);

unsigned clemens_unserialize_array(
    mpack_reader_t* reader,
    uintptr_t data_adr,
    const struct ClemensSerializerRecord* record,
    ClemensSerializerAllocateCb alloc_cb,
    void* context);

unsigned clemens_unserialize_record(
    mpack_reader_t* reader,
    uintptr_t data_adr,
    const struct ClemensSerializerRecord* record,
    ClemensSerializerAllocateCb alloc_cb,
    void* context);

#ifdef __cplusplus
}
#endif

#endif
