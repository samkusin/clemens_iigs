#ifndef CLEM_HOST_AUDIO_H
#define CLEM_HOST_AUDIO_H

#include "clem_types.h"

#include <MMDeviceAPI.h>
#include <AudioClient.h>

namespace ckaudio {

  enum class BufferFormat {
    Unknown,
    PCM,
    Float
  };

  struct DataFormat {
    BufferFormat frameFormat;
    unsigned frameSize;
    unsigned numChannels;
    unsigned frequency;
  };

}

class ClemensAudio
{
public:
  ClemensAudio();
  ~ClemensAudio();

  unsigned getAudioFrequency() const;

  void start();
  void stop();

  //  expected data from the emulator rendered to the host's audio playback
  //  buffer.  the format coming from the emulator is 16-bit PCM stereo.
  //
  //  frames are the number of audio frames represented in the data.
  //  stride is the byte increment between frames (data[0], data[stride] are
  //    two frames of 16-bit PCM stereo data.)
  //  framesPerSecond is the mix frequency on the emulator - which should be
  //    equal to the host mix frequency for optimal use. besides if they are
  //    not equal, it's questionable if the host audio APIs support up or
  //    downsampling.
  //  data here is directly copied to the host's audio playback buffer - so
  //  this function should be called often enough to keep fluid playback
  //  (perhaps at 60hz)
  //
  //
  void render(const ClemensAudioMixBuffer& mixBuffer);

private:
  IMMDevice* audioDevice_;
  IAudioClient* audioClient_;
  IAudioRenderClient* audioRenderClient_;
  ckaudio::DataFormat dataFormat_;

  uint32_t desiredLatencyMS_;
  uint32_t audioEngineFrameCount_;
};


#endif
