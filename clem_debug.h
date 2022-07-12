#ifndef CLEM_DEBUG_H
#define CLEM_DEBUG_H

/* TODO: something a little less reliant on clib */
#include <assert.h>
#include <stdio.h>

#define CLEM_ASSERT(_cond_)                                                    \
  do {                                                                         \
    assert(_cond_);                                                            \
  } while (0)

#define CLEM_UNIMPLEMENTED(_fmt_, ...)                                         \
  do {                                                                         \
    clem_debug_log(CLEM_DEBUG_LOG_UNIMPL, _fmt_, __VA_ARGS__);                 \
  } while (0)

#define CLEM_WARN(_fmt_, ...)                                                  \
  do {                                                                         \
    clem_debug_log(CLEM_DEBUG_LOG_WARN, _fmt_, __VA_ARGS__);                   \
  } while (0)

#define CLEM_LOG(_fmt_, ...)                                                   \
  do {                                                                         \
    clem_debug_log(CLEM_DEBUG_LOG_INFO, _fmt_, __VA_ARGS__);                   \
  } while (0)

#define CLEM_DEBUG(_fmt_, ...)                                                 \
  do {                                                                         \
    clem_debug_log(CLEM_DEBUG_LOG_DEBUG, _fmt_, __VA_ARGS__);                  \
  } while (0)

#ifdef __cplusplus
extern "C" {
#endif

typedef struct ClemensMachine ClemensMachine;

void clem_debug_context(ClemensMachine *context);

void clem_debug_log(int log_level, const char *fmt, ...);

char *clem_debug_acquire_trace(unsigned amt);
void clem_debug_trace_flush();

void clem_debug_iwm_start(ClemensMachine *context);
void clem_debug_iwm_stop(ClemensMachine *context);

#ifdef __cplusplus
}
#endif

#endif
