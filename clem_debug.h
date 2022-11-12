#ifndef CLEM_DEBUG_H
#define CLEM_DEBUG_H

/* TODO: something a little less reliant on clib */
#include <assert.h>
#include <stdio.h>

#define CLEM_ASSERT(_cond_)                                                                        \
    do {                                                                                           \
        assert(_cond_);                                                                            \
    } while (0)

#define CLEM_UNIMPLEMENTED(...)                                                                    \
    do {                                                                                           \
        clem_debug_log(CLEM_DEBUG_LOG_UNIMPL, __VA_ARGS__);                                        \
    } while (0)

#define CLEM_WARN(...)                                                                             \
    do {                                                                                           \
        clem_debug_log(CLEM_DEBUG_LOG_WARN, __VA_ARGS__);                                          \
    } while (0)

#define CLEM_LOG(...)                                                                              \
    do {                                                                                           \
        clem_debug_log(CLEM_DEBUG_LOG_INFO, __VA_ARGS__);                                          \
    } while (0)

#define CLEM_DEBUG(...)                                                                            \
    do {                                                                                           \
        clem_debug_log(CLEM_DEBUG_LOG_DEBUG, __VA_ARGS__);                                         \
    } while (0)

#ifdef __cplusplus
extern "C" {
#endif

typedef struct ClemensMachine ClemensMachine;

void clem_debug_reset(struct ClemensDeviceDebugger *dbg);
void clem_debug_break(struct ClemensDeviceDebugger *dbg, unsigned debug_reason, unsigned param0,
                      unsigned param1);

void clem_debug_context(ClemensMachine *context);

void clem_debug_log(int log_level, const char *fmt, ...);

char *clem_debug_acquire_trace(unsigned amt);
void clem_debug_trace_flush();

void clemens_debug_status_toolbox(ClemensMachine *context, unsigned id);

#ifdef __cplusplus
}
#endif

#endif
