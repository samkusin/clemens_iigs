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

/**
 * @brief Resets the timer counters and interrupt flags
 *
 * @param timer
 */
void clem_timer_reset(struct ClemensDeviceTimer* timer);

/**
 * @brief Issues interrupts for the 1 sec and 0.266667 timer
 *
 * @param timer
 * @param delta_ms Millisecond increment since last sync
 * @param irq_line
 * @return uint32_t modified irq_line
 */
uint32_t clem_timer_sync(struct ClemensDeviceTimer* timer, uint32_t delta_ms,
                         uint32_t irq_line);

/**
 * @brief Resets the ADB state
 *
 * @param adb ADB data
 */
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
 * @param delta_ms Millisecond increment since last sync
 * @param irq_line
 * @return uint32_t modified irq_line*
 */
uint32_t clem_adb_glu_sync(struct ClemensDeviceADB* adb, uint32_t delta_ms,
                           uint32_t irq_line);

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