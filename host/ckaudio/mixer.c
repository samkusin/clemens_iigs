#include "ckaudio/core.h"
#include "context.h"

#include "mathops.h"

/* first pass - hardcode a simple float PCM of a middle C note.

   preallocate all tracks
   add a SINE ACTION to a mixer track at the current timestamp
   update mixer tracks
    per track
        if it's time to end an action, remove from the track
        if it's time to begin an action, start it
        if action is active, render it

   render actions
    for now, actions are rendered directly to the write window



*/

#define CKAUDIO_MIXER_ACTION_TYPE_DIRTY 0x80000000

/* [in:channels] | [in:format_enum] | [out:channels] | [out:format_enum] */
#define CKAUDIO_MIXER_INT_PCM_1_TO_FLOAT_2 0x01010202
#define CKAUDIO_MIXER_INT_PCM_2_TO_FLOAT_2 0x02010202
#define CKAUDIO_MIXER_INT_FLOAT_1_TO_FLOAT_2 0x01020202
#define CKAUDIO_MIXER_INT_FLOAT_2_TO_FLOAT_2 0x02020202

uint32_t _ckaudio_mixer_get_out_in_xform_id(CKAudioDataFormat *out,
                                            CKAudioDataFormat *in) {
    unsigned tmp;
    tmp = (out->buffer_format & 0xff);
    tmp |= (out->num_channels & 0xff) << 8;
    tmp |= (in->buffer_format & 0xff) << 16;
    tmp |= (in->num_channels & 0xff) << 24;
    return tmp;
}

union CKAudioMixerParam {
    float f;
    uint32_t u32;
    int i32;
};

enum CKAudioMixerTrackState {
    kCKAudioMixerTrackStateBegin,
    kCKAudioMixerTrackStateAttack,
    kCKAudioMixerTrackStateDecay,
    kCKAudioMixerTrackStateSustain,
    kCKAudioMixerTrackStateRelease,
    kCKAudioMixerTrackStateEnd,
};

struct CKAudioMixerTrackAction {
    CKAudioBuffer *attached_buffer;
    uint32_t type;
    union CKAudioMixerParam params[8];
};

struct CKAudioMixerTrackLiveAction {
    struct CKAudioMixerTrackAction data;
    enum CKAudioMixerTrackState state;
    /* TODO: scratch should be an allocated buffer, default to 8 so it can be
       quickly retrieved from a preallocated action buffer, but also allow
       larger scratch buffers (16, 32, 64, etc.)
    */
    union CKAudioMixerParam scratch[8];
};

struct CKAudioMixerTrack {
    struct CKAudioMixerTrackLiveAction action[2];
    uint32_t volume;
};

struct CKAudioMixerStagingAction {
    struct CKAudioMixerTrackAction data;
    uint32_t volume;
    //  0x1 = action data dirty
    //  0x2 = volume dirty
    unsigned dirty;
};

typedef struct CKAudioMixerT {
    struct CKAudioMixerPlatform *platform;
    struct CKAudioMixerTrack *tracks;
    struct CKAudioMixerStagingAction *staging_actions;
    unsigned track_count;

    CKAudioTimePoint last_update_timepoint;
    uint64_t render_count;
} CKAudioMixer;

/*
#define CKAUDIO_MIXER_ACTION_LIMIT 16

struct CKAudioMixerAction *_ckaudio_int_allocate_action(CKAudioMixer *mixer) {
    struct CKAudioMixerAction *head = mixer->free_action_list;
    struct CKAudioMixerAction *node = head;
    if (node->next == head) {
        return NULL;
    }
    head->next = node->next;
    memset(node, 0, sizeof(*node));
    return node;
}
    int i;

    mixer->free_action_list = ckaudio_allocator_typed_calloc(
        &g_ckaudio_context, struct CKAudioMixerAction,
        CKAUDIO_MIXER_ACTION_LIMIT + 1);
    for (i = 0; i < CKAUDIO_MIXER_ACTION_LIMIT; ++i) {
        mixer->free_action_list[i].next = &mixer->free_action_list[i + 1];
    }

*/

static void _ckaudio_mixer_commit_staging_tracks(CKAudioMixer *mixer);

CKAudioMixer *ckaudio_mixer_create() {
    CKAudioMixer *mixer =
        ckaudio_allocator_typed_calloc(&g_ckaudio_context, CKAudioMixer, 1);
    int i;

    mixer->platform = ckaudio_mixer_platform_create(&g_ckaudio_context);
    mixer->track_count = CKAUDIO_MIXER_TRACK_LIMIT;
    mixer->tracks = ckaudio_allocator_typed_calloc(
        &g_ckaudio_context, struct CKAudioMixerTrack, mixer->track_count);
    mixer->staging_actions = ckaudio_allocator_typed_calloc(
        &g_ckaudio_context, struct CKAudioMixerStagingAction,
        mixer->track_count);
    for (i = 0; i < mixer->track_count; ++i) {
        mixer->staging_actions[i].data.type = CKAUDIO_MIXER_ACTION_TYPE_NONE;
        mixer->staging_actions[i].dirty = 1;
        mixer->staging_actions[i].volume = 50;
    }
    _ckaudio_mixer_commit_staging_tracks(mixer);

    return mixer;
}

void ckaudio_mixer_destroy(CKAudioMixer *mixer) {
    ckaudio_allocator_free(&g_ckaudio_context, mixer->staging_actions);
    ckaudio_allocator_free(&g_ckaudio_context, mixer->tracks);
    ckaudio_allocator_free(&g_ckaudio_context, mixer->platform);
    ckaudio_allocator_free(&g_ckaudio_context, mixer);
}

void ckaudio_mixer_set_track_volume(CKAudioMixer *mixer, unsigned track_id,
                                    uint32_t volume) {
    struct CKAudioMixerStagingAction *action;
    if (track_id >= mixer->track_count) {
        return;
    }
    action = &mixer->staging_actions[track_id];
    action->dirty = 1;
    ck_op_u32_min(&action->volume, volume, 100);
}

uint32_t ckaudio_mixer_get_track_volume(CKAudioMixer *mixer,
                                        unsigned track_id) {
    if (track_id >= mixer->track_count) {
        return 0;
    }
    return mixer->staging_actions[track_id].volume;
}

void ckaudio_mixer_set_track_action(CKAudioMixer *mixer, unsigned track_id,
                                    uint32_t action_type) {
    struct CKAudioMixerStagingAction *action;
    if (track_id >= mixer->track_count) {
        return;
    }
    action = &mixer->staging_actions[track_id];
    action->data.type = action_type;
    action->dirty = 1;
}

uint32_t ckaudio_mixer_get_track_action(CKAudioMixer *mixer,
                                        unsigned track_id) {
    if (track_id >= mixer->track_count) {
        return 0;
    }
    return mixer->staging_actions[track_id].data.type;
}

void ckaudio_mixer_set_track_action_param(CKAudioMixer *mixer,
                                          unsigned track_id,
                                          uint32_t parameter_type,
                                          float value) {
    struct CKAudioMixerStagingAction *action;
    if (track_id >= mixer->track_count) {
        return;
    }
    action = &mixer->staging_actions[track_id];
    action->data.params[parameter_type].f = value;
    action->dirty = 1;
}

float ckaudio_mixer_get_track_action_param(CKAudioMixer *mixer,
                                           unsigned track_id,
                                           uint32_t parameter_type) {
    struct CKAudioMixerStagingAction *action;
    if (track_id >= mixer->track_count) {
        return 0.0f;
    }
    action = &mixer->staging_actions[track_id];
    return action->data.params[parameter_type].f;
}

void ckaudio_mixer_set_track_action_buffer(CKAudioMixer *mixer,
                                           unsigned track_id,
                                           CKAudioBuffer *buffer) {
    struct CKAudioMixerStagingAction *action;
    if (track_id >= mixer->track_count) {
        return;
    }
    action = &mixer->staging_actions[track_id];
    action->data.attached_buffer = buffer;
    ++action->data.attached_buffer->ref_count;
    action->dirty = 1;
}

static void _ckaudio_mixer_commit_staging_tracks(CKAudioMixer *mixer) {
    int i;
    for (i = 0; i < mixer->track_count; ++i) {
        struct CKAudioMixerStagingAction *action = &mixer->staging_actions[i];
        struct CKAudioMixerTrack *track = &mixer->tracks[i];
        if (action->dirty) {
            if (action->data.type != track->action[0].data.type) {
                /* new action */
                memcpy(&track->action[1], &track->action[0],
                       sizeof(struct CKAudioMixerTrackLiveAction));
                track->action[1].state = kCKAudioMixerTrackStateRelease;
                memcpy(&track->action[0].data, &action->data,
                       sizeof(struct CKAudioMixerTrackAction));
                memset(&track->action[0].scratch, 0,
                       sizeof(track->action[0].scratch));
                track->action[0].state = kCKAudioMixerTrackStateBegin;
            }
            track->volume = action->volume;
            memset(action, 0, sizeof(*action));
        }
    }
}

void ckaudio_mixer_begin_update(CKAudioMixer *mixer) {
    /* The ONE time the mixer locks its mutex on the application's thread.
       The app must call end_update as soon as possible to unlock the mix
       thread */
    ckaudio_mixer_platform_lock(mixer->platform);
    _ckaudio_mixer_commit_staging_tracks(mixer);
}

void ckaudio_mixer_end_update(CKAudioMixer *mixer) {
    ckaudio_mixer_platform_unlock(mixer->platform);
}

////////////////////////////////////////////////////////////////////////////////

static unsigned _ckaudio_mixer_track_silence(float dt, CKAudioBuffer *out) {
    unsigned frame_idx;
    unsigned frame_size = out->data_format.frame_size;
    uint8_t *raw = (uint8_t *)out->data;
    float *frame;
    float dt_op = 0.0f;
    float dt_per_sample = 1.0f / out->data_format.frequency;

    for (frame_idx = 0; frame_idx < out->frame_limit && dt_op < dt;
         ++frame_idx) {
        frame = (float *)raw;
        frame[0] = 0.0f;
        frame[1] = 0.0f;
        dt_op += dt_per_sample;
        raw += frame_size;
    }
    return frame_idx;
}

static void
_ckaudio_mixer_track_begin_action(struct CKAudioMixerTrackLiveAction *action,
                                  CKAudioBuffer *out) {
    switch (action->data.type) {
    case CKAUDIO_MIXER_ACTION_TYPE_SINE_TONE:
        action->scratch[0].f = 0.0f;
        break;
    case CKAUDIO_MIXER_ACTION_TYPE_SQUARE_TONE:
        /* for now force 3 waveforms */
        action->scratch[0].f = 0.0f;
        action->scratch[1].f = 0.0f;
        action->scratch[2].f = 0.0f;
        action->scratch[4].f =
            (action->data.params[CKAUDIO_MIXER_ACTION_PARAM_FREQUENCY].f *
             CK_PI_2);
        action->scratch[5].f = action->scratch[4].f * 3;
        action->scratch[6].f = action->scratch[4].f * 5;
        break;
    case CKAUDIO_MIXER_ACTION_TYPE_WAVEFORM:
        if (action->data.attached_buffer) {
            action->scratch[0].u32 = _ckaudio_mixer_get_out_in_xform_id(
                &out->data_format, &action->data.attached_buffer->data_format);
        }
        break;
    }

    action->state = kCKAudioMixerTrackStateSustain;
}

static void
_ckaudio_mixer_track_square_tone(struct CKAudioMixerTrackLiveAction *action,
                                 float dt, float volume, CKAudioBuffer *out) {
    uint8_t *raw = (uint8_t *)(out->data);
    unsigned frame_idx;
    unsigned frame_size = out->data_format.frame_size;
    float dt_theta[3];
    float dt_op = 0.0f;
    float dt_per_sample = 1.0f / out->data_format.frequency;
    float v[3];
    float vol[3];

    vol[0] = volume;
    vol[1] = volume * 0.33333333f;
    vol[2] = volume * 0.2f;

    dt_theta[0] = action->scratch[4].f / out->data_format.frequency;
    dt_theta[1] = action->scratch[5].f / out->data_format.frequency;
    dt_theta[2] = action->scratch[6].f / out->data_format.frequency;

    for (frame_idx = 0; frame_idx < out->frame_limit && dt_op < dt;
         ++frame_idx) {
        float *frame = (float *)raw;
        ck_op_sinf(&v[0], action->scratch[0].f);
        ck_op_sinf(&v[1], action->scratch[1].f);
        ck_op_sinf(&v[2], action->scratch[2].f);
        frame[0] += (v[0] * vol[0]) + (v[1] * vol[1]) + (v[2] * vol[2]);
        frame[1] += (v[0] * vol[0]) + (v[1] * vol[1]) + (v[2] * vol[2]);
        action->scratch[0].f += dt_theta[0];
        action->scratch[1].f += dt_theta[1];
        action->scratch[2].f += dt_theta[2];
        if (action->scratch[0].f >= CK_PI_2) {
            action->scratch[0].f -= CK_PI_2;
        }
        if (action->scratch[1].f >= CK_PI_2) {
            action->scratch[1].f -= CK_PI_2;
        }
        if (action->scratch[2].f >= CK_PI_2) {
            action->scratch[2].f -= CK_PI_2;
        }
        dt_op += dt_per_sample;
        raw += frame_size;
    }
}

static void
_ckaudio_mixer_track_sine_tone(struct CKAudioMixerTrackLiveAction *action,
                               float dt, float volume, CKAudioBuffer *out) {
    unsigned frame_idx;
    unsigned frame_size = out->data_format.frame_size;
    uint8_t *raw = (uint8_t *)(out->data);
    float *frame;
    float tone_theta = action->scratch[0].f;
    /* TODO: precalc */
    float dt_theta =
        (action->data.params[CKAUDIO_MIXER_ACTION_PARAM_FREQUENCY].f *
         CK_PI_2) /
        out->data_format.frequency;
    float dt_op = 0.0f;
    float dt_per_sample = 1.0f / out->data_format.frequency;
    float v;

    for (frame_idx = 0; frame_idx < out->frame_limit && dt_op < dt;
         ++frame_idx) {
        frame = (float *)raw;
        ck_op_sinf(&v, tone_theta);
        frame[0] += (v * volume);
        frame[1] += (v * volume);
        tone_theta += dt_theta;
        if (tone_theta >= CK_PI_2) {
            tone_theta -= CK_PI_2;
        }
        dt_op += dt_per_sample;
        raw += frame_size;
    }
    action->scratch[0].f = tone_theta;
}

static void _ckaudio_mixer_track_waveform_pcm_2_float_2(
    struct CKAudioMixerTrackLiveAction *action, float dt, float volume,
    CKAudioBuffer *out) {
    unsigned frame_idx;
    unsigned frame_size = out->data_format.frame_size;
    float in_frame_ctr;
    float in_frame_delta;
    unsigned in_frame_size =
        action->data.attached_buffer->data_format.frame_size;
    uint8_t *raw_dest = (uint8_t *)(out->data);
    uint8_t *raw_src = (uint8_t *)(action->data.attached_buffer->data);
    float *frame;
    uint16_t *frame_src;
    float v;

    in_frame_delta =
        ((float)action->data.attached_buffer->data_format.frequency /
         out->data_format.frequency);

    in_frame_ctr = 0.0f;
    for (frame_idx = 0; frame_idx < out->frame_limit; ++frame_idx) {
        frame = (float *)raw_dest;
        frame_src =
            (uint16_t *)(raw_src + (unsigned)in_frame_ctr * in_frame_size);

        ck_op_pcm_unsigned_to_float(&v, frame_src[0]);
        frame[0] += (v * volume);
        ck_op_pcm_unsigned_to_float(&v, frame_src[1]);
        frame[1] += (v * volume);

        in_frame_ctr += in_frame_delta;
        raw_dest += frame_size;
    }
}

static void
_ckaudio_mixer_track_waveform(struct CKAudioMixerTrackLiveAction *action,
                              float dt, float volume, CKAudioBuffer *out) {

    switch (action->scratch[0].u32) {
    case CKAUDIO_MIXER_INT_PCM_1_TO_FLOAT_2:
        break;
    case CKAUDIO_MIXER_INT_PCM_2_TO_FLOAT_2:
        _ckaudio_mixer_track_waveform_pcm_2_float_2(action, dt, volume, out);
        break;
    case CKAUDIO_MIXER_INT_FLOAT_1_TO_FLOAT_2:
        break;
    case CKAUDIO_MIXER_INT_FLOAT_2_TO_FLOAT_2:
        break;
    }
}

static void _ckaudio_mixer_track_action(struct CKAudioMixerTrack *track,
                                        unsigned action_index, float dt,
                                        CKAudioBuffer *out) {
    struct CKAudioMixerTrackLiveAction *action = &track->action[action_index];

    if (action->state == kCKAudioMixerTrackStateBegin) {
        _ckaudio_mixer_track_begin_action(action, out);
    }
    if (action->state == kCKAudioMixerTrackStateSustain) {
        /* TODO: attack, decay and release */
        switch (action->data.type) {
        case CKAUDIO_MIXER_ACTION_TYPE_SINE_TONE:
            _ckaudio_mixer_track_sine_tone(action, dt, track->volume * 0.01f,
                                           out);
            break;
        case CKAUDIO_MIXER_ACTION_TYPE_SQUARE_TONE:
            _ckaudio_mixer_track_square_tone(action, dt, track->volume * 0.01f,
                                             out);
            break;
        case CKAUDIO_MIXER_ACTION_TYPE_WAVEFORM:
            if (action->data.attached_buffer) {
                _ckaudio_mixer_track_waveform(action, dt, track->volume * 0.01f,
                                              out);
            }
            break;

        default:
            break;
        }
    }
}

uint32_t ckaudio_mixer_render(CKAudioMixer *mixer, CKAudioBuffer *out,
                              CKAudioTimePoint *timepoint) {
    uint32_t render_cnt;
    struct CKAudioMixerTrack *track;
    float dt;
    int track_id;

    ckaudio_mixer_platform_lock(mixer->platform);

    dt =
        mixer->render_count > 0
            ? ckaudio_timepoint_deltaf(timepoint, &mixer->last_update_timepoint)
            : 0.0f;

    render_cnt = _ckaudio_mixer_track_silence(dt, out);

    for (track_id = 0; track_id < mixer->track_count; ++track_id) {
        track = &mixer->tracks[track_id];
        _ckaudio_mixer_track_action(track, 0, dt, out);
        _ckaudio_mixer_track_action(track, 1, dt, out);
    }
    memcpy(&mixer->last_update_timepoint, timepoint,
           sizeof(mixer->last_update_timepoint));
    ++mixer->render_count;
    ckaudio_mixer_platform_unlock(mixer->platform);
    return render_cnt;
}
