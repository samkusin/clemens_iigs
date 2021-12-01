#ifndef CINEK_AUDIO_TYPES_H
#define CINEK_AUDIO_TYPES_H

#include <stdbool.h>
#include <stdint.h>

#if defined(_WIN32)
#define CLEMENS_PLATFORM_WINDOWS 1
#else
#define CLEMENS_PLATFORM_WINDOWS 0
#endif

#define CKAUDIO_MIXER_TRACK_LIMIT 8
#define CKAUDIO_MIXER_STREAM_LIMIT 4

#define CKAUDIO_MIXER_ACTION_TYPE_NONE 0x00000000
#define CKAUDIO_MIXER_ACTION_TYPE_SINE_TONE 0x00000001
#define CKAUDIO_MIXER_ACTION_TYPE_SQUARE_TONE 0x00000002
#define CKAUDIO_MIXER_ACTION_TYPE_SAWTOOTH_TONE 0x00000003
#define CKAUDIO_MIXER_ACTION_TYPE_WAVEFORM 0x00000004
#define CKAUDIO_MIXER_ACTION_TYPE_STREAM 0x00000005

#define CKAUDIO_MIXER_ACTION_PARAM_FREQUENCY 0

#ifdef __cplusplus
extern "C"
{
#endif

    /**
 * @brief
 *
 */
    typedef struct
    {
        char data[8];
    } CKAudioTimePoint;

    enum CKAudioBufferFormat
    {
        kCKAudioBufferFormatUnknown,
        kCKAudioBufferFormatPCM,
        kCKAudioBufferFormatFloat,
        kCKAudioBufferFormatEvent,
        kCKAudioBufferFormatPoint
    };

    typedef struct
    {
        enum CKAudioBufferFormat buffer_format;
        uint32_t frame_size;
        uint32_t num_channels;
        uint32_t frequency;
    } CKAudioDataFormat;

    typedef struct
    {
        CKAudioDataFormat data_format;
        void *data;
        uint32_t frame_limit;
        int32_t ref_count;
    } CKAudioBuffer;

    typedef struct
    {
        void *(*allocate)(void *user_ctx, size_t amt);
        void (*free)(void *user_ctx, void *p);
        void *user_ctx;
    } CKAudioAllocator;

    typedef struct CKAudioMixerT CKAudioMixer;

    typedef uint32_t (*CKAudioReadyCallback)(CKAudioBuffer *buffer,
                                             CKAudioTimePoint *timepoint,
                                             void *ctx);


#ifdef __cplusplus
}
#endif

#endif
