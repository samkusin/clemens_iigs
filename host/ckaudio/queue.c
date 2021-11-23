#include "queue.h"

#include <string.h>

CKAudioDataFormat *ckaudio_queue_init_data_format(CKAudioDataFormat *format,
                                                  uint32_t queue_size,
                                                  uint32_t element_size) {
    format->frame_size = element_size;
    format->frequency = queue_size;
    format->buffer_format = kCKAudioBufferFormatEvent;
    format->num_channels = 1;
    return format;
}

struct CKAudioQueue *ckaudio_audio_queue_init(struct CKAudioQueue *queue,
                                              CKAudioBuffer *buffer) {
    queue->buffer = buffer;
    queue->frame_read_head = queue->frame_write_head = 0;
    return queue;
}

uint32_t ckaudio_queue_read_window(struct CKAudioQueueWindow *out_window,
                                   const struct CKAudioQueue *queue) {
    out_window->buffer = queue->buffer;
    out_window->start = queue->frame_read_head;
    out_window->count = queue->frame_write_head - queue->frame_read_head;
    return out_window->count;
}

void ckaudio_queue_read(struct CKAudioQueue *queue, uint32_t frame_count) {
    /* TODO: use the read/write count utility functions and then apply
        the results to the ring buffer pointers since the code is being
        duplicated
    */
    uint32_t window_size = queue->frame_write_head - queue->frame_read_head;
    if (frame_count > window_size) {
        frame_count = window_size;
    }
    queue->frame_read_head += frame_count;
}

uint32_t ckaudio_queue_write_window(struct CKAudioQueueWindow *out_window,
                                    const struct CKAudioQueue *queue) {
    out_window->buffer = queue->buffer;
    out_window->start = queue->frame_write_head;
    out_window->count = queue->buffer->frame_limit - queue->frame_write_head;
    return out_window->count;
}

void ckaudio_queue_write(struct CKAudioQueue *queue, uint32_t frame_count) {
    /* TODO: use the read/write count utility functions and then apply
        the results to the ring buffer pointers since the code is being
        duplicated
    */
    uint32_t window_size = queue->buffer->frame_limit - queue->frame_write_head;
    if (frame_count > window_size) {
        frame_count = window_size;
    }
    queue->frame_write_head += frame_count;
}

void ckaudio_queue_finish(struct CKAudioQueue *queue) {
    uint8_t *data = (uint8_t *)queue->buffer->data;
    uint32_t frame_size = queue->buffer->data_format.frame_size;
    uint32_t frame_count = queue->frame_write_head - queue->frame_read_head;
    if (queue->frame_read_head > 0) {
        if (queue->frame_read_head < queue->frame_write_head) {
            memmove(data, data + queue->frame_read_head * frame_size,
                    frame_count * frame_size);
        }
        queue->frame_write_head -= queue->frame_read_head;
        queue->frame_read_head = 0;
    }
}
