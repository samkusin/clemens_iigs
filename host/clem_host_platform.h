#ifndef CLEMENS_HOST_PLATFORM_H
#define CLEMENS_HOST_PLATFORM_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#if defined(_WIN32)

#define CLEMENS_PLATFORM_WINDOWS
//  A larger value to cover more edge cases but likely not all
#define CLEMENS_PATH_MAX 1024

#elif defined(__linux__)

#define CLEMENS_PLATFORM_LINUX
//  A larger value to cover PATH_MAX but perhaps not all usecases
#define CLEMENS_PATH_MAX 4096

#elif defined(__APPLE__)
#include <TargetConditionals.h>

#if TARGET_OS_MAC
#define CLEMENS_PLATFORM_MACOS
#endif

//  A larger value to cover PATH_MAX but perhaps not all usecases
#define CLEMENS_PATH_MAX 4096

#endif

#define CLEM_HOST_JOYSTICK_LIMIT 4

#define CLEM_HOST_JOYSTICK_BUTTON_A 0x00000001
#define CLEM_HOST_JOYSTICK_BUTTON_B 0x00000002
#define CLEM_HOST_JOYSTICK_BUTTON_X 0x00000004
#define CLEM_HOST_JOYSTICK_BUTTON_Y 0x00000008

#define CLEM_HOST_JOYSTICK_AXIS_DELTA 1023

#if defined(CLEMENS_PLATFORM_WINDOWS)
#define CLEM_HOST_JOYSTICK_PROVIDER_DINPUT  "dinput"
#define CLEM_HOST_JOYSTICK_PROVIDER_XINPUT  "xinput"
#define CLEM_HOST_JOYSTICK_PROVIDER_DEFAULT CLEM_HOST_JOYSTICK_PROVIDER_DINPUT
#elif defined(CLEMENS_PLATFORM_LINUX)
#define CLEM_HOST_JOYSTICK_PROVIDER_DEFAULT ""
#elif defined(CLEMENS_PLATFORM_MACOS)
#define CLEM_HOST_JOYSTICK_PROVIDER_GAMECONTROLLER "gamecontroller"
#define CLEM_HOST_JOYSTICK_PROVIDER_HIDIOKIT       "hid-iokit"
#define CLEM_HOST_JOYSTICK_PROVIDER_DEFAULT        CLEM_HOST_JOYSTICK_PROVIDER_GAMECONTROLLER
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
 * @brief Starts any platform specific systems
 *
 * This may equate to a no-op on some platforms, but gives our application
 * a change to perform any platform specific initialization not handled by
 * the cross-platform application framework used.
 */
void clem_host_platform_init(void);

/**
 * @brief Reverses clem_host_platform_init()
 *
 */
void clem_host_platform_terminate(void);

/**
 * @brief Returns the current processor the local thread is running on
 *
 * Effectively getcpu() on Linux, and GetCurrentProcessorNumber() on Windows
 *
 * @return unsigned
 */
unsigned clem_host_get_processor_number(void);

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
 * @brief Get the process executable full path name
 *
 * @param outpath C string buffer
 * @param outpath_size  Size of the *whole* outpath buffer (including null term)
 *                      If the value is not large enough then a value will be returned here
 * @return char* NULL if the pathname length is greater than outpath_size
 */
char *get_process_executable_path(char *outpath, size_t *outpath_size);

/**
 * @brief Get the local user data directory qualified with identifiers
 *
 * @param outpath
 * @param outpath_size
 * @param company_name
 * @param app_name
 * @return char*
 */
char *get_local_user_data_directory(char *outpath, size_t outpath_size, const char *company_name,
                                    const char *app_name);

/**
 * @brief Initializes the joystick system
 *
 * @param provider OS specific (see CLEM_HOST_JOYSTICK_PROVIDER_xxx IDs)
 */
void clem_joystick_open_devices(const char *provider);

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
void clem_joystick_close_devices(void);

#ifdef __cplusplus
}
#endif

#endif
