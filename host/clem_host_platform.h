#ifndef CLEMENS_HOST_PLATFORM_H
#define CLEMENS_HOST_PLATFORM_H

#include <stdbool.h>
#include <stdint.h>

#if defined(_WIN32)
#define CLEMENS_PLATFORM_WINDOWS 1
#define CLEMENS_PLATFORM_LINUX   0
#elif defined(__linux__)
#define CLEMENS_PLATFORM_WINDOWS 0
#define CLEMENS_PLATFORM_LINUX   1
#else
#define CLEMENS_PLATFORM_WINDOWS 0
#define CLEMENS_PLATFORM_LINUX   0
#endif

#define CLEM_HOST_JOYSTICK_LIMIT 4

#define CLEM_HOST_JOYSTICK_BUTTON_A 0x00000001
#define CLEM_HOST_JOYSTICK_BUTTON_B 0x00000002
#define CLEM_HOST_JOYSTICK_BUTTON_X 0x00000004
#define CLEM_HOST_JOYSTICK_BUTTON_Y 0x00000008

#define CLEM_HOST_JOYSTICK_AXIS_DELTA 1024

#define CLEM_HOST_JOYSTICK_PROVIDER_NONE    0
#define CLEM_HOST_JOYSTICK_PROVIDER_DEFAULT 1
#if defined(CLEMENS_PLATFORM_WINDOWS)
#define CLEM_HOST_JOYSTICK_PROVIDER_DINPUT 1
#define CLEM_HOST_JOYSTICK_PROVIDER_XINPUT 2
#elif defined(CLEMENS_PLATFORM_LINUX)
#define CLEM_HOST_JOYSTICK_PROVIDED_EVDEV 1
#endif
#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief
 *
 */
typedef struct {
    unsigned char data[16];
} ClemensHostUUID;

typedef struct {
    unsigned buttons;
    int16_t x[2];
    int16_t y[2];
    bool isConnected;
} ClemensHostJoystick;

/**
 * @brief Returns the current processor the local thread is running on
 *
 * Effectively getcpu() on Linux, and GetCurrentProcessorNumber() on Windows
 *
 * @return unsigned
 */
unsigned clem_host_get_processor_number();

/**
 * @brief Generates a UUID using the preferred OS method
 *
 * Under Windows, UuidCreate
 * Under Linux, libuuid
 *
 * @param uuid
 */
void clem_host_uuid_gen(ClemensHostUUID *uuid);

/**
 * @brief Initializes the joystick system
 *
 * @param provider OS specific (Windows: 0 = XInput, 1 = DirectInput)
 */
void clem_joystick_open_devices(unsigned provider);

/**
 * @brief Return joystick data for up to 4 devices
 *
 * @param joystick
 * @return unsigned
 */
unsigned clem_joystick_poll(ClemensHostJoystick *joysticks);

/**
 * @brief Terminates the active joystick system.
 *
 */
void clem_joystick_close_devices();

#ifdef __cplusplus
}
#endif

#endif
