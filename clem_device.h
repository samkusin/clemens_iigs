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

/**
 * @brief This method is provided for testing and custom implementations.
 *
 * Normally this is executed from clemens_input to capture input events from
 * the host application.  This is the 'Device' side of the ADB architecture.
 *
 * @param adb ADB device data
 * @param input The input event from the host OS/application
 */
void clem_adb_device_input(struct ClemensDeviceADB* adb,
                           struct ClemensInputEvent* input);
/**
 * @brief Executed frequently enough to emulate the GLU Microcontroller
 *
 * @param adb ADB device data (1 ms worth of cycles?)
 */
void clem_adb_glu(struct ClemensDeviceADB* adb);

/**
 * @brief Executed from the memory subsystem for MMIO
 *
 * @param adb ADB data
 * @param ioreg The MMIO register accessed
 * @param value The value written to the MMIO register
 */
void clem_adb_write_switch(struct ClemensDeviceADB* adb, uint8_t ioreg,
                           uint8_t value);

/**
 * @brief Executed from the memory subsystem for MMIO
 *
 * @param adb ADB data
 * @param ioreg The MMIO register accessed
 * @param flags Access flags used to determine if the read is a no-op
 * @return uint8_t The value read from the MMIO register
 */
uint8_t clem_adb_read_switch(struct ClemensDeviceADB* adb, uint8_t ioreg,
                             uint8_t flags);


#ifdef __cplusplus
}
#endif

#endif
