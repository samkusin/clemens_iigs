#include "context.h"

#define WIN32_LEAN_AND_MEAN
#include <Windows.h>

struct CKAudioMixerPlatform {
    CRITICAL_SECTION crit_sec;
};

struct CKAudioMixerPlatform *
ckaudio_mixer_platform_create(CKAudioContext *context) {
    struct CKAudioMixerPlatform *platform =
        ckaudio_allocator_typed_alloc(context, struct CKAudioMixerPlatform, 1);

    InitializeCriticalSection(&platform->crit_sec);

    return platform;
}

void ckaudio_mixer_platform_destroy(CKAudioContext *context,
                                    struct CKAudioMixerPlatform *platform) {
    DeleteCriticalSection(&platform->crit_sec);
    memset(context, 0, sizeof(*context));
    ckaudio_allocator_free(context, platform);
}

void ckaudio_mixer_platform_lock(struct CKAudioMixerPlatform *platform) {
    EnterCriticalSection(&platform->crit_sec);
}

void ckaudio_mixer_platform_unlock(struct CKAudioMixerPlatform *platform) {
    LeaveCriticalSection(&platform->crit_sec);
}