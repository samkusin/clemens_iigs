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

void clem_debug_reset(struct ClemensDeviceDebugger* dbg);
void clem_debug_call_stack(struct ClemensDeviceDebugger* dbg);
void clem_debug_counters(struct ClemensDeviceDebugger* dbg);

void clem_debug_break(struct ClemensDeviceDebugger* dbg,
                      struct Clemens65C816* cpu,
                      unsigned debug_reason,
                      unsigned param0,
                      unsigned param1);
void clem_debug_jsr(struct ClemensDeviceDebugger* dbg,
                    struct Clemens65C816* cpu,
                    uint16_t next_pc,
                    uint8_t pbr);
void clem_debug_rts(struct ClemensDeviceDebugger* dbg,
                    struct Clemens65C816* cpu,
                    uint16_t next_pc,
                    uint8_t pbr);


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
 * @param delta_us Microsecond increment since last sync
 * @param irq_line
 * @return uint32_t modified irq_line
 */
void clem_timer_sync(struct ClemensDeviceTimer* timer, uint32_t delta_us);

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
                           const struct ClemensInputEvent* input);
/**
 * @brief Executed frequently enough to emulate the GLU Microcontroller
 *
 * @param adb ADB device data (1 ms worth of cycles?)
 * @param delta_us Microsecond increment since last sync
 */
void clem_adb_glu_sync(struct ClemensDeviceADB* adb, uint32_t delta_us);

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

/**
 * @brief Resets the Sound state
 *
 * @param glu sound data
 */
void clem_sound_reset(struct ClemensDeviceAudio* glu);

/**
 * @brief Executed frequently enough to emulate the GLU Microcontroller
 *
 * @param glu device data (1 ms worth of cycles?)
 * @param delta_us Microsecond increment since last sync
 */
void clem_sound_glu_sync(struct ClemensDeviceAudio* glu, uint32_t delta_us);

/**
 * @brief Executed from the memory subsystem for MMIO
 *
 * @param glu GLU data
 * @param ioreg The MMIO register accessed
 * @param value The value written to the MMIO register
 */
void clem_sound_write_switch(struct ClemensDeviceAudio* glu, uint8_t ioreg,
                             uint8_t value);

/**
 * @brief Executed from the memory subsystem for MMIO
 *
 * @param glu GLU data
 * @param ioreg The MMIO register accessed
 * @param flags Access flags used to determine if the read is a no-op
 * @return uint8_t The value read from the MMIO register
 */
uint8_t clem_adb_read_switch(struct ClemensDeviceAudio* glu, uint8_t ioreg,
                             uint8_t flags);


#ifdef __cplusplus
}
#endif

#endif
