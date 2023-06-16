#ifndef CLEM_LOCALE_H
#define CLEM_LOCALE_H

#include "clem_mmio_defs.h"

#define CLEM_ADB_LOCALE_KEYB_US    0
#define CLEM_ADB_LOCALE_KEYB_COUNT 1

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Converts UTF8 code to an ISO 8859-1 compliant code given the input keyboard
 * type.
 *
 * @return a 16-bit word (high 8-bits are the ADB modifier, and low 8-bits are the
 *         keycode)
 */
uint16_t clem_iso_latin_1_to_adb_key_and_modifier(unsigned char ch, int keyb_type);

#ifdef __cplusplus
}
#endif
#endif
