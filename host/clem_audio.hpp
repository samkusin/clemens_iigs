#ifndef CLEM_HOST_AUDIO_H
#define CLEM_HOST_AUDIO_H

#include "ckaudio/core.h"
#include "clem_types.h"

#include <vector>


class ClemensAudioDevice {
public:
  ClemensAudioDevice();
  ~ClemensAudioDevice();

  unsigned getAudioFrequency() const;
  unsigned getBufferStride() const;

  void start();
  void stop();
  unsigned queue(ClemensAudio &audio, float deltaTime);

private:
  static uint32_t mixAudio(CKAudioBuffer *audioBuffer, CKAudioTimePoint *timepoint, void *ctx);
  static uint32_t mixClemensAudio(CKAudioBuffer *audioBuffer, CKAudioTimePoint *timepoint, void *ctx);
  CKAudioMixer *mixer_;
  CKAudioDataFormat dataFormat_;

  uint8_t* queuedFrameBuffer_;
  uint32_t queuedFrameHead_;
  uint32_t queuedFrameTail_;
  uint32_t queuedFrameLimit_;
  uint32_t queuedFrameStride_;
  uint32_t queuedPreroll_;

  float tone_theta_;
};

#endif
