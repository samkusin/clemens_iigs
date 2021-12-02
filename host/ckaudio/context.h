#ifndef CINEK_AUDIO_INTERNAL_CONTEXT_H
#define CINEK_AUDIO_INTERNAL_CONTEXT_H

#include "ckaudio/types.h"

#ifdef __cplusplus
extern "C" {
#endif

struct CKAudioWorker;
struct CKAudioMixerPlatform;

struct CKAudioEvent {
    char data[16];
};

typedef struct {
    char text[256];
} CKAudioMessage;

typedef struct CKAudioContextT {
    CKAudioAllocator allocator;

    uint64_t mix_timestamp_ms;
    bool failure;

    struct CKAudioWorker *worker;
} CKAudioContext;

extern CKAudioContext g_ckaudio_context;

extern inline bool ckaudio_buffer_valid(CKAudioBuffer *buffer) {
    return buffer->data != NULL && buffer->frame_limit > 0 &&
           buffer->data_format.buffer_format != kCKAudioBufferFormatUnknown &&
           buffer->data_format.frame_size != 0;
}

extern inline void *ckaudio_allocator_alloc(CKAudioContext *ctx, size_t amt) {
    return ctx->allocator.allocate(ctx->allocator.user_ctx, amt);
}

void *ckaudio_allocator_calloc(CKAudioContext *ctx, size_t cnt, size_t amt);

#define ckaudio_allocator_typed_alloc(_ctx_, _type_, _cnt_)                    \
    (_type_ *)ckaudio_allocator_alloc(_ctx_, sizeof(_type_) * (_cnt_))

#define ckaudio_allocator_typed_calloc(_ctx_, _type_, _cnt_)                   \
    (_type_ *)ckaudio_allocator_calloc(_ctx_, sizeof(_type_), (_cnt_))

extern inline void ckaudio_allocator_free(CKAudioContext *ctx, void *p) {
    ctx->allocator.free(ctx->allocator.user_ctx, p);
}

/**
 * @brief Allows data init of a buffer struct (so it can be used internally)
 *
 * Called implicitly by ckaudio_buffer_create.
 *
 * @param buffer
 * @param duration_ms
 * @param data_format
 * @return CKAudioBuffer*
 */
CKAudioBuffer *ckaudio_buffer_init(CKAudioBuffer *buffer, uint32_t duration_ms,
                                   const CKAudioDataFormat *data_format);

/**
 * @brief Releases the buffer data created via ckaudio_buffer_init
 *
 * This will not free the CKAudioBuffer object pointer if created via
 * ckaudio_buffer_create.  Use ckaudio_allocator_free to do this AFTER calling
 * release.
 *
 * @param buffer
 */
void ckaudio_buffer_release(CKAudioBuffer *buffer);

#ifdef __cplusplus
}
#endif

#endif
