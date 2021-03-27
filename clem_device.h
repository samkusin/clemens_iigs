#ifndef CLEM_DEVICE_H
#define CLEM_DEVICE_H

#include "clem_types.h"

/**
 * Device Interface
 * These functions emulate the various peripherals (internal and external)
 */

#ifdef __cplusplus
extern "C" {
#endif

void clem_rtc_reset(struct ClemensDeviceRTC* rtc);
void clem_rtc_command(struct ClemensDeviceRTC* rtc);

#ifdef __cplusplus
}
#endif

#endif
