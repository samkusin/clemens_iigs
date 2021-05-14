/**
 * @file clem_dsk.h
 * @author your name (you@domain.com)
 * @brief Apple IIgs disk image utilities
 * @version 0.1
 * @date 2021-05-13
 *
 * @copyright Copyright (c) 2021
 *
 */

#ifndef CLEM_DISK_H
#define CLEM_DISK_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

int clem_dsk_load(uint8_t* data, unsigned data_sz);

#ifdef __cplusplus
}
#endif

#endif