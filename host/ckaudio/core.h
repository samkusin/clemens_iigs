#ifndef CINEK_AUDIO_CORE_H
#define CINEK_AUDIO_CORE_H

#include "ckaudio/types.h"

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C"
{
#endif

  void ckaudio_init(CKAudioAllocator *allocator);

  const char *ckaudio_get_message();

  void ckaudio_start(CKAudioReadyCallback audio_ready_cb, void *cb_ctx);

  void ckaudio_stop();

  void ckaudio_term();

  void ckaudio_lock();

  void ckaudio_unlock();

  void ckaudio_get_data_format(CKAudioDataFormat *out);

  /**
 * @brief Creates a buffer managed by the audio mixer/core.
 *
 * The audio engine is responsible for the lifecycle of this buffer and will
 * release it once no longer used.
 *
 * @param buffer
 * @param duration_ms
 * @param data_format
 * @return CKAudioBuffer*
 */
  CKAudioBuffer *ckaudio_buffer_create(uint32_t duration_ms,
                                       const CKAudioDataFormat *data_format);
  /**
 * @brief Creates the audio mixer engine
 *
 * @return CKAudioMixer*
 */
  CKAudioMixer *ckaudio_mixer_create();
  /**
 * @brief
 *
 * @param mixer
 */
  void ckaudio_mixer_destroy(CKAudioMixer *mixer);

  /**
 * @brief
 *
 * @param mixer
 */
  void ckaudio_mixer_update(CKAudioMixer *mixer);
  /**
 * @brief Renders mixed audio data into a buffer.
 *
 * This can be used to feed the audio core for realtime playback
 *
 * @param mixer
 * @param buffer
 * @param timepoint
 * @return uint32_t
 */
  uint32_t ckaudio_mixer_render(CKAudioMixer *mixer, CKAudioBuffer *buffer,
                                CKAudioTimePoint *timepoint);
  /**
 * @brief Set the track volume used by actions when rendering audio
 *
 * @param mixer
 * @param volume 0-100 where 100 is full amplitude
 */
  void ckaudio_mixer_set_track_volume(CKAudioMixer *mixer, unsigned track_id,
                                      uint32_t volume);
  /**
 * @brief
 *
 * @param mixer
 * @param track_id
 * @return uint32_t
 */
  uint32_t ckaudio_mixer_get_track_volume(CKAudioMixer *mixer, unsigned track_id);
  /**
 * @brief
 *
 * @param mixer
 * @param track_id
 * @param action_type
 */
  void ckaudio_mixer_set_track_action(CKAudioMixer *mixer, unsigned track_id,
                                      uint32_t action_type);
  /**
 * @brief
 *
 * @param mixer
 * @param track_id
 * @return uint32_t
 */
  uint32_t ckaudio_mixer_get_track_action(CKAudioMixer *mixer, unsigned track_id);
  /**
 * @brief
 *
 * @param mixer
 * @param track_id
 * @param parameter_type
 * @param value
 */
  void ckaudio_mixer_set_track_action_param(CKAudioMixer *mixer,
                                            unsigned track_id,
                                            uint32_t parameter_type, float value);
  /**
 * @brief
 *
 * @param mixer
 * @param track_id
 * @param parameter_type
 * @return float
 */
  float ckaudio_mixer_get_track_action_param(CKAudioMixer *mixer,
                                             unsigned track_id,
                                             uint32_t parameter_type);
  /**
   * @brief
   *
   * @param mixer
   * @param callback_index
   * @param callback
   */
  void ckaudio_mixer_set_track_callback(CKAudioMixer *mixer,
                                        unsigned track_id,
                                        CKAudioReadyCallback callback,
                                        void* cb_ctx);
  /**
 * @brief Attach a buffer to the action
 *
 * If the buffer was attached with a refernce count other than 0, then the
 * caller owns the buffer.  Otherwise ownership is passed to mixer, which will
 * issue the destroy buffer event once the action is finished.
 *
 * In either case, once passed to the mixer, the caller cannot modify the
 * buffer until the mixer is stopped, or the buffer refcount has returned to the
 * value it was initialzed at (i.e. the mixer is done with it.)  This is why
 * there is no getter (the application should keep a pointer to it.)
 *
 * TODO: Mixer events may allow for modification as needed.
 *
 * @param mixer
 * @param track_id
 * @param buffer
 */
  void ckaudio_mixer_set_track_action_buffer(CKAudioMixer *mixer,
                                             unsigned track_id,
                                             CKAudioBuffer *buffer);
  /**
   * @brief Render audio from one buffer (and format) to another
   *
   * @param out
   * @param in
   * @param volume
   * @return uint32_t Number of frames rendered from input
   */
  uint32_t ckaudio_mixer_render_waveform(CKAudioBuffer* out, CKAudioBuffer* in,
                                         uint32_t volume);
  /**
 * @brief
 *
 */
  void ckaudio_timepoint_init();

  /**
 * @brief
 *
 * @param tp
 */
  void ckaudio_timepoint_now(CKAudioTimePoint *tp);

  /**
 * @brief
 *
 * @param t1
 * @param t0
 * @return float
 */
  float ckaudio_timepoint_deltaf(CKAudioTimePoint *t1, CKAudioTimePoint *t0);

  /**
 * @brief
 *
 * @param t1
 * @param t0
 * @return double
 */
  double ckaudio_timepoint_deltad(CKAudioTimePoint *t1, CKAudioTimePoint *t0);

  /**
 * @brief
 *
 * @param t
 * @return float
 */
  float ckaudio_timepoint_stepf(CKAudioTimePoint *t);

  /**
 * @brief
 *
 * @param t1
 * @param t0
 * @return double
 */
  double ckaudio_timepoint_stepd(CKAudioTimePoint *t);

#ifdef __cplusplus
}
#endif

#endif
