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
 * @brief
 *
 */
  void ckaudio_timepoint_init();

  void ckaudio_timepoint_make_null(CKAudioTimePoint *tp);

  bool ckaudio_timepoint_is_null(CKAudioTimePoint *tp);

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
