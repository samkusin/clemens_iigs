#ifndef CLEM_SERIALIZER_H
#define CLEM_SERIALIZER_H

#include "clem_types.h"

#include "clem_mmio_types.h"

#include "external/mpack.h"

#define CLEM_SERIALIZER_INVALID_RECORD (UINT_MAX)

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
    kClemensSerializerTypeClockObject,
    kClemensSerializerTypeBlob,
    kClemensSerializerTypeArray,
    kClemensSerializerTypeObject,
    kClemensSerializerTypeCustom,
};

struct ClemensSerializerRecord {
    const char *name;
    enum ClemensSerializerType type;
    enum ClemensSerializerType array_type;
    unsigned offset;
    unsigned size;
    unsigned param;

    struct ClemensSerializerRecord *records;
};

#define CLEM_SERIALIZER_RECORD(_struct_, _type_, _name_)                                           \
    {                                                                                              \
#_name_, _type_, kClemensSerializerTypeEmpty, offsetof(_struct_, _name_), 0, 0, NULL       \
    }

#define CLEM_SERIALIZER_RECORD_PARAM(_struct_, _type_, _arr_type_, _name_, _size_, _param_,        \
                                     _records_)                                                    \
    {                                                                                              \
#_name_, _type_, _arr_type_, offsetof(_struct_, _name_), _size_, _param_, _records_        \
    }

#define CLEM_SERIALIZER_RECORD_ARRAY(_struct_, _type_, _name_, _size_, _param_)                    \
    CLEM_SERIALIZER_RECORD_PARAM(_struct_, kClemensSerializerTypeArray, _type_, _name_, _size_,    \
                                 _param_, NULL)

#define CLEM_SERIALIZER_RECORD_ARRAY_OBJECTS(_struct_, _name_, _size_, _object_type_, _records_)   \
    CLEM_SERIALIZER_RECORD_PARAM(_struct_, kClemensSerializerTypeArray,                            \
                                 kClemensSerializerTypeObject, _name_, _size_,                     \
                                 sizeof(_object_type_), _records_)

#define CLEM_SERIALIZER_RECORD_BOOL(_struct_, _name_)                                              \
    CLEM_SERIALIZER_RECORD(_struct_, kClemensSerializerTypeBool, _name_)

#define CLEM_SERIALIZER_RECORD_UINT8(_struct_, _name_)                                             \
    CLEM_SERIALIZER_RECORD(_struct_, kClemensSerializerTypeUInt8, _name_)

#define CLEM_SERIALIZER_RECORD_UINT16(_struct_, _name_)                                            \
    CLEM_SERIALIZER_RECORD(_struct_, kClemensSerializerTypeUInt16, _name_)

#define CLEM_SERIALIZER_RECORD_UINT32(_struct_, _name_)                                            \
    CLEM_SERIALIZER_RECORD(_struct_, kClemensSerializerTypeUInt32, _name_)

#define CLEM_SERIALIZER_RECORD_UINT64(_struct_, _name_)                                            \
    CLEM_SERIALIZER_RECORD(_struct_, kClemensSerializerTypeUInt64, _name_)

#define CLEM_SERIALIZER_RECORD_INT32(_struct_, _name_)                                             \
    CLEM_SERIALIZER_RECORD(_struct_, kClemensSerializerTypeInt32, _name_)

#define CLEM_SERIALIZER_RECORD_INT16(_struct_, _name_)                                             \
    CLEM_SERIALIZER_RECORD(_struct_, kClemensSerializerTypeInt16, _name_)

#define CLEM_SERIALIZER_RECORD_FLOAT(_struct_, _name_)                                             \
    CLEM_SERIALIZER_RECORD(_struct_, kClemensSerializerTypeFloat, _name_)

#define CLEM_SERIALIZER_RECORD_DURATION(_struct_, _name_)                                          \
    CLEM_SERIALIZER_RECORD(_struct_, kClemensSerializerTypeDuration, _name_)

#define CLEM_SERIALIZER_RECORD_CLOCKS(_struct_, _name_)                                            \
    CLEM_SERIALIZER_RECORD(_struct_, kClemensSerializerTypeClocks, _name_)

#define CLEM_SERIALIZER_RECORD_CLOCK_OBJECT(_struct_, _name_)                                      \
    CLEM_SERIALIZER_RECORD(_struct_, kClemensSerializerTypeClockObject, _name_)

#define CLEM_SERIALIZER_RECORD_BLOB(_struct_, _name_, _size_)                                      \
    CLEM_SERIALIZER_RECORD_PARAM(_struct_, kClemensSerializerTypeBlob,                             \
                                 kClemensSerializerTypeEmpty, _name_, _size_, 0, NULL)

#define CLEM_SERIALIZER_RECORD_OBJECT(_struct_, _name_, _object_type_, _records_)                  \
    CLEM_SERIALIZER_RECORD_PARAM(_struct_, kClemensSerializerTypeObject,                           \
                                 kClemensSerializerTypeEmpty, _name_, sizeof(_object_type_), 0,    \
                                 _records_)

#define CLEM_SERIALIZER_RECORD_CUSTOM(_struct_, _name_, _object_type_, _record_id_)                \
    CLEM_SERIALIZER_RECORD_PARAM(_struct_, kClemensSerializerTypeCustom,                           \
                                 kClemensSerializerTypeEmpty, _name_, sizeof(_object_type_),       \
                                 _record_id_, NULL)

#define CLEM_SERIALIZER_RECORD_EMPTY()                                                             \
    { NULL, kClemensSerializerTypeEmpty, kClemensSerializerTypeEmpty, 0, 0, 0, NULL }

/**
 * @brief
 *
 * @param machine
 * @param writer
 * @return mpack_writer_t*
 */
mpack_writer_t *clemens_serialize_machine(mpack_writer_t *writer, ClemensMachine *machine);

/**
 * @brief
 *
 * @param machine
 * @param writer
 * @return mpack_reader_t*
 */
mpack_reader_t *clemens_unserialize_machine(mpack_reader_t *reader, ClemensMachine *machine,
                                            ClemensSerializerAllocateCb alloc_cb, void *context);

/**
 * @brief
 *
 * @param writer
 * @param mmio
 * @return mpack_writer_t*
 */
mpack_writer_t *clemens_serialize_mmio(mpack_writer_t *writer, ClemensMMIO *mmio);

/**
 * @brief
 *
 * @param reader
 * @param mmio
 * @param alloc_cb
 * @param context
 * @return mpack_reader_t*
 */
mpack_reader_t *clemens_unserialize_mmio(mpack_reader_t *reader, ClemensMMIO *mmio,
                                         ClemensSerializerAllocateCb alloc_cb, void *context);

/* The following APIs are provided for completeness, but they are typically
   not called directly by the application (called instead by the main machine
   APIs above.)
*/
unsigned clemens_serialize_object(mpack_writer_t *writer, uintptr_t data_adr,
                                  const struct ClemensSerializerRecord *record);

unsigned clemens_serialize_array(mpack_writer_t *writer, uintptr_t data_adr,
                                 const struct ClemensSerializerRecord *record);

unsigned clemens_serialize_record(mpack_writer_t *writer, uintptr_t data_adr,
                                  const struct ClemensSerializerRecord *record);

unsigned clemens_unserialize_object(mpack_reader_t *reader, uintptr_t data_adr,
                                    const struct ClemensSerializerRecord *record,
                                    ClemensSerializerAllocateCb alloc_cb, void *context);

unsigned clemens_unserialize_array(mpack_reader_t *reader, uintptr_t data_adr,
                                   const struct ClemensSerializerRecord *record,
                                   ClemensSerializerAllocateCb alloc_cb, void *context);

unsigned clemens_unserialize_record(mpack_reader_t *reader, uintptr_t data_adr,
                                    const struct ClemensSerializerRecord *record,
                                    ClemensSerializerAllocateCb alloc_cb, void *context);

#ifdef __cplusplus
}
#endif

#endif
