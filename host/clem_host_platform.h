#ifndef CLEMENS_HOST_PLATFORM_H
#define CLEMENS_HOST_PLATFORM_H

#include <stdint.h>
#include <stdbool.h>

#if defined(_WIN32)
#define CLEMENS_PLATFORM_WINDOWS 1
#define CLEMENS_PLATFORM_LINUX 0
#elif defined(__linux__)
#define CLEMENS_PLATFORM_WINDOWS 0
#define CLEMENS_PLATFORM_LINUX 1
#else
#define CLEMENS_PLATFORM_WINDOWS 0
#define CLEMENS_PLATFORM_LINUX 0
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

#if CLEMENS_PLATFORM_LINUX
/**
 * @brief
 *
 * @param display A pointer to the Display opened via XOpenDisplay
 */
void clem_host_x11_init(const void* display);
#endif

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
void clem_host_uuid_gen(ClemensHostUUID* uuid);

#ifdef __cplusplus
}
#endif

#endif
