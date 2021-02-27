#ifndef CLEMENS_HOST_PLATFORM_H
#define CLEMENS_HOST_PLATFORM_H

#include <stdint.h>

#if defined(_WIN32)
#define CLEMENS_PLATFORM_WINDOWS 1
#else
#define CLEMENS_PLATFORM_WINDOWS 0
#endif

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief
 *
 */
typedef struct {
  char data[8];
} ClemensHostTimePoint;

/**
 * @brief
 *
 */
void clem_host_timepoint_init();

/**
 * @brief
 *
 * @param tp
 */
void clem_host_timepoint_now(ClemensHostTimePoint* tp);
/**
 * @brief
 *
 * @param t1
 * @param t0
 * @return float
 */
float clem_host_timepoint_deltaf(ClemensHostTimePoint* t1,
                                 ClemensHostTimePoint* t0);
/**
 * @brief
 *
 * @param t1
 * @param t0
 * @return double
 */
double clem_host_timepoint_deltad(ClemensHostTimePoint* t1,
                                 ClemensHostTimePoint* t0);
#ifdef __cplusplus
}
#endif

#endif