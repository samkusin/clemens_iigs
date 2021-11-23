#include "ckaudio/core.h"
#include "context.h"

#include <string.h>

void *ckaudio_allocator_calloc(CKAudioContext *ctx, size_t cnt, size_t amt)
{
    void *data = ctx->allocator.allocate(ctx->allocator.user_ctx, cnt * amt);
    memset(data, 0, cnt * amt);
    return data;
}

CKAudioBuffer *ckaudio_buffer_init(CKAudioBuffer *buffer, uint32_t duration_ms,
                                   const CKAudioDataFormat *data_format)
{
    unsigned frame_size = 0;
    unsigned frame_limit = 0;

    memset(buffer, 0, sizeof(*buffer));

    switch (data_format->buffer_format)
    {
    case kCKAudioBufferFormatPCM:
        buffer->data_format.frequency = data_format->frequency;
        buffer->data_format.num_channels = data_format->num_channels;
        frame_size = sizeof(uint16_t);
        frame_limit = (data_format->frequency * duration_ms) / 1000;
        break;
    case kCKAudioBufferFormatFloat:
        buffer->data_format.frequency = data_format->frequency;
        buffer->data_format.num_channels = data_format->num_channels;
        frame_size = sizeof(float);
        frame_limit = (data_format->frequency * duration_ms) / 1000;
        break;
    case kCKAudioBufferFormatEvent:
        buffer->data_format.num_channels = 1;
        frame_size = sizeof(struct CKAudioEvent);
        frame_limit = data_format->frequency;
        break;
    default:
        return NULL;
    }

    buffer->data =
        ckaudio_allocator_alloc(&g_ckaudio_context, frame_limit * frame_size);
    buffer->data_format.buffer_format = data_format->buffer_format;
    buffer->data_format.frame_size =
        frame_size * buffer->data_format.num_channels;
    buffer->frame_limit = frame_limit;

    return buffer;
}

CKAudioBuffer *ckaudio_buffer_create(uint32_t duration_ms,
                                     const CKAudioDataFormat *data_format)
{
    CKAudioBuffer buffer;
    if (!ckaudio_buffer_init(&buffer, duration_ms, data_format))
    {
        return NULL;
    }

    return (CKAudioBuffer *)memcpy(
        ckaudio_allocator_typed_alloc(&g_ckaudio_context, CKAudioBuffer, 1),
        &buffer, sizeof(CKAudioBuffer));
}

void ckaudio_buffer_release(CKAudioBuffer *buffer)
{
    if (!buffer)
    {
        return;
    }
    if (buffer->ref_count > 0)
    {
        --buffer->ref_count;
    }
    if (buffer->ref_count == 0 && buffer->data)
    {
        ckaudio_allocator_free(&g_ckaudio_context, buffer->data);
    }
    memset(buffer, 0, sizeof(*buffer));
}
