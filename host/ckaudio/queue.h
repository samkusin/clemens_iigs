#ifndef CINEK_AUDIO_QUEUE_H
#define CINEK_AUDIO_QUEUE_H

#include "ckaudio/types.h"

#ifdef __cplusplus
extern "C" {
#endif

struct CKAudioQueue {
    CKAudioBuffer *buffer;
    uint32_t frame_read_head;
    uint32_t frame_write_head;
};

struct CKAudioQueueWindow {
    CKAudioBuffer *buffer;
    uint32_t start;
    uint32_t count;
};

extern inline bool ckaudio_queue_empty(struct CKAudioQueue *queue) {
    return queue->frame_read_head == queue->frame_write_head;
}

extern inline void *ckaudio_buffer_get_frame_ptr(CKAudioBuffer *buffer,
                                                 uint32_t frame_index) {
    return (uint8_t *)buffer->data +
           buffer->data_format.frame_size * frame_index;
}

#define ckaudio_buffer_get_typed_frame(_buffer_, _type_, _idx_)                \
    (_type_ *)ckaudio_buffer_get_frame_ptr(_buffer_, _idx_)

CKAudioDataFormat *ckaudio_queue_init_data_format(CKAudioDataFormat *format,
                                                  uint32_t queue_size,
                                                  uint32_t element_size);
struct CKAudioQueue *ckaudio_audio_queue_init(struct CKAudioQueue *queue,
                                              CKAudioBuffer *buffer);

uint32_t ckaudio_queue_read_window(struct CKAudioQueueWindow *out_window,
                                   const struct CKAudioQueue *queue);

void ckaudio_queue_read(struct CKAudioQueue *queue, uint32_t frame_count);

uint32_t ckaudio_queue_write_window(struct CKAudioQueueWindow *out_window,
                                    const struct CKAudioQueue *queue);

void ckaudio_queue_write(struct CKAudioQueue *queue, uint32_t frame_count);

void ckaudio_queue_finish(struct CKAudioQueue *queue);

#ifdef __cplusplus
}
#endif

#endif
