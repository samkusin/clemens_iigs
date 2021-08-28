#ifndef CLEM_SERIALIZER_H
#define CLEM_SERIALIZER_H

#include "clem_types.h"
#include "contrib/mpack.h"

#ifdef __cplusplus
extern "C" {
#endif

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
    mpack_reader_t* writer,
    ClemensMachine* machine);

/* The following APIs are provided for completeness, but they are typically
   not called directly by the application (called instead by the main machine
   APIs above.)
*/

/**
 * @brief
 *
 * @param writer
 * @param machine
 * @return mpack_writer_t*
 */
mpack_writer_t* clemens_serialize_cpu(
    mpack_writer_t* writer,
    struct Clemens65C816* cpu);

/**
 * @brief
 *
 * @param writer
 * @param mmio
 * @return mpack_writer_t*
 */
mpack_writer_t* clemens_serialize_mmio(
    mpack_writer_t* writer,
    struct ClemensMMIO* mmio);

/**
 * @brief
 *
 * @param writer
 * @param drives
 * @return mpack_writer_t*
 */
mpack_writer_t* clemens_serialize_drives(
    mpack_writer_t* writer,
    struct ClemensDriveBay* drives);

/**
 * @brief
 *
 * @param writer
 * @param cpu
 * @return mpack_writer_t*
 */
mpack_reader_t* clemens_unserialize_cpu(
    mpack_reader_t* writer,
    struct Clemens65C816* cpu);

/**
 * @brief
 *
 * @param writer
 * @param mmio
 * @return mpack_reader_t*
 */
mpack_reader_t* clemens_unserialize_mmio(
    mpack_reader_t* writer,
    struct ClemensMMIO* mmio);

/**
 * @brief
 *
 * @param writer
 * @param drives
 * @return mpack_reader_t*
 */
mpack_reader_t* clemens_unserialize_drives(
    mpack_reader_t* writer,
    struct ClemensDriveBay* drives);


#ifdef __cplusplus
}
#endif

#endif
