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


void clem_rtc_reset(struct ClemensDeviceRTC* rtc,
                    clem_clocks_duration_t latency);
void clem_rtc_command(struct ClemensDeviceRTC* rtc, clem_clocks_time_t ts,
                      unsigned op);

void clem_adb_reset(struct ClemensDeviceADB* adb);
void clem_input_host_key(struct ClemensDeviceADB* adb);
void clem_adb_write_switch(struct ClemensDeviceADB* adb, uint8_t ioreg,
                           uint8_t value, uint8_t flags);
uint8_t clem_adb_read_switch(struct ClemensDeviceADB* adb, uint8_t ioreg,
                             uint8_t flags);


#ifdef __cplusplus
}
#endif

#endif
