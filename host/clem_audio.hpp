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

  void start();
  void stop();
  unsigned queue(ClemensAudio &audio, float deltaTime);

private:
  static uint32_t mixAudio(CKAudioBuffer *audioBuffer, CKAudioTimePoint *timepoint, void *ctx);
  CKAudioMixer *mixer_;
  CKAudioDataFormat dataFormat_;

  std::vector<uint8_t> queuedFrames_;
};

#endif
