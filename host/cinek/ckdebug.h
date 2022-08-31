/**
 * \file    debug.h
 *
 *
 * \note    Created by Samir Sinha on 1/5/13.
 *          Copyright (c) 2013 Cinekine. All rights reserved.
 */

#ifndef CINEK_CKDEBUG_H
#define CINEK_CKDEBUG_H

/* PORTABILITY NOTE: Not including cktypes.h if possible to minimize dependencies.
   Try to mimic ckdefs.h if we need preprocessor ifdef checks for including one
   header vs. another.
*/
#include <stdarg.h>
#include <stddef.h>

#ifndef NDEBUG
    #ifndef CK_DEBUG_LOGGING
    /**
     * \def CK_DEBUG_LOGGING
     * Enables debug logging.
     */
    #define CK_DEBUG_LOGGING 1
    #endif
    #ifndef CK_DEBUG_ASSERT
    /**
     * \def CK_DEBUG_ASSERT
     * Enables the CK_ASSERT macro.
     */
    #define CK_DEBUG_ASSERT 1
    #endif
#endif

/** Logging level constants. */
typedef enum cinek_log_level_t
{
    kCinekLogLevel_First,
    kCinekLogLevel_Trace = kCinekLogLevel_First,
    kCinekLogLevel_Debug,
    kCinekLogLevel_Info,
    kCinekLogLevel_Warn,
    kCinekLogLevel_Error,
    kCinekLogLevel_Count
}
cinek_log_level;

/** Callbacks used for logging. */
typedef struct cinek_log_callbacks_t
{
    /** An array of pointers to functions displaying log messages for each log level. */
    void    (*logger[kCinekLogLevel_Count])(void* ctx, const char* sourceId, const char* fmt, va_list args);
    /** Raw logger */
    void    (*rawLogger)(void* ctx, cinek_log_level level, const char* sourceId, const char* msg);
    /** A request to flush the logging system. */
    void    (*flush)(void* ctx);
}
cinek_log_callbacks;


#if CK_DEBUG_LOGGING

#ifdef __cplusplus
extern "C" {
#endif

/** Starts the logging system, with an option to specify a custom log provider
 *  for debugging cinek systems.
 *
 *  @param  callbacks       Application defined logging callbacks.
 *  @param  context         Supplied for applications to pass a context to their
 *                          custom callback.
 */
void cinek_debug_log_start(
        const cinek_log_callbacks* callbacks,
        void* context
    );

/** Sends a flush to any custom log provider. */
void cinek_debug_log_flush(void);

/** Outputs a message to the CineK raw logger.
 *
 *  @param  level           The logging level.
 *  @param  sourceId        The message source (see kCinekLogLevel_XXXX constants.)
 *  @param  msg             Message string as is
 */
void cinek_debug_log_raw(
        cinek_log_level level,
        const char* sourceId,
        const char* msg
    );

/** Outputs a message to the CineK logger.
 *
 *  @param  level           The logging level.
 *  @param  sourceId        The message source (see kCinekLogLevel_XXXX constants.)
 *  @param  fmt             Message string with printf style formatting.
 *  @param  ...             Variable arguments matching the format.
 */
void cinek_debug_log(
        cinek_log_level level,
        const char* sourceId,
        const char* fmt,
        ...
    );

/** Outputs a message to the CineK logger given an argument list.
 *
 *  @param  level           The logging level.
 *  @param  sourceId        The message source (see kCinekLogLevel_XXXX constants.)
 *  @param  fmt             Message string with Pprintf style formatting.
 *  @param  args            List of arguments matching the format.
 */
void cinek_debug_log_args(
        cinek_log_level level,
        const char* sourceId,
        const char* fmt,
        va_list args
    );

#ifdef __cplusplus
}
#endif

#define CK_LOG_TRACE(_source_, ...) cinek_debug_log(kCinekLogLevel_Trace, _source_, __VA_ARGS__)
#define CK_LOG_DEBUG(_source_, ...) cinek_debug_log(kCinekLogLevel_Debug, _source_, __VA_ARGS__)
#define CK_LOG_INFO(_source_, ...) cinek_debug_log(kCinekLogLevel_Info, _source_, __VA_ARGS__)
#define CK_LOG_WARN(_source_, ...) cinek_debug_log(kCinekLogLevel_Warn, _source_, __VA_ARGS__)
#define CK_LOG_ERROR(_source_, ...) cinek_debug_log(kCinekLogLevel_Error, _source_, __VA_ARGS__)

#else  /* CK_DEBUG_LOGGING */

#define cinek_debug_log_start(_callbacks_, _context_)
#define cinek_debug_log_flush()
#define cinek_debug_log(...)
#define cinek_debug_break()

#define CK_LOG_TRACE(_source_, ...)
#define CK_LOG_DEBUG(_source_, ...)
#define CK_LOG_INFO(_source_, ...)
#define CK_LOG_WARN(_source_, ...)
#define CK_LOG_ERROR(_source_, ...)

#endif  /* CK_DEBUG_LOGGING */

#if CK_DEBUG_ASSERT

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Triggers a break using the defined break handler (see cinek_debug_set_break_handler.)
 */
void cinek_debug_break(void);

#ifdef __cplusplus
}
#endif

/**
 * \def CK_ASSERT(_cond_)
 * Assert macro verifying the _cond_ expression is true.
 */
#ifndef CK_ASSERT
#define CK_ASSERT(_cond_) do {  \
    if (!(_cond_))                  \
    {                               \
        CK_LOG_ERROR( "assert", #_cond_ );    \
        cinek_debug_break();        \
    }                               \
} while(0)
#endif

#ifndef CK_ASSERT_RETURN
#define CK_ASSERT_RETURN(_cond_) do { \
    if (!(_cond_))                  \
    {                               \
        CK_LOG_ERROR( "assert", #_cond_ );    \
        cinek_debug_break();        \
        return;                     \
    }                               \
} while(0)
#endif

#ifndef CK_ASSERT_RETURN_VALUE
#define CK_ASSERT_RETURN_VALUE(_cond_, _val_) do { \
    if (!(_cond_))                  \
    {                               \
        CK_LOG_ERROR( "assert", #_cond_ );    \
        cinek_debug_break();        \
        return (_val_);             \
    }                               \
} while(0)
#endif


#else

#ifndef CK_ASSERT
#define CK_ASSERT(_cond_)
#endif

#ifndef CK_ASSERT_RETURN
#define CK_ASSERT_RETURN(_cond_) if (!(_cond_)) return
#endif

#ifndef CK_ASSERT_RETURN_VALUE
#define CK_ASSERT_RETURN_VALUE(_cond_,_val_) if (!(_cond_)) return (_val_)
#endif

#endif

#endif  /* CINEK_CKDEBUG_H */
