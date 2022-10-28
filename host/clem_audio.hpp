#ifndef CLEM_HOST_AUDIO_H
#define CLEM_HOST_AUDIO_H

#include "clem_types.h"

#include <vector>
#include <mutex>


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
  static void mixAudio(float* buffer, int num_frames, int num_channels, void* user_data);

  void mixClemensAudio(float* buffer, int num_frames, int num_channels);

  uint8_t* queuedFrameBuffer_;
  uint32_t queuedFrameHead_;
  uint32_t queuedFrameTail_;
  uint32_t queuedFrameLimit_;
  uint32_t queuedFrameStride_;

  std::mutex queuedFrameMutex_;
};

#endif
