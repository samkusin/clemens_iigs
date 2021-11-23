#include "clem_audio.hpp"

#include <algorithm>
#include <cstdlib>

static void *allocateLocal(void *user_ctx, size_t sz) { return malloc(sz); }

static void freeLocal(void *user_ctx, void *data) { free(data); }

ClemensAudioDevice::ClemensAudioDevice() :
  mixer_(NULL) {

  CKAudioAllocator allocator;
  allocator.allocate = &allocateLocal;
  allocator.free = &freeLocal;
  allocator.user_ctx = this;
  ckaudio_init(&allocator);

  //  kind of magic - we really aren't going to need a half second buffer
  queuedFrames_.reserve(8 * 24000);
}

ClemensAudioDevice::~ClemensAudioDevice() {
  if (mixer_ != nullptr) {
    stop();
  }
  ckaudio_term();
}

void ClemensAudioDevice::start() {
  mixer_ = ckaudio_mixer_create();
  ckaudio_start(&ClemensAudioDevice::mixAudio, this);
  //  TODO: updating our device format should go into an event handler that
  //  traps events from the CKAudioCore/Mixer (i.e what happens if a device
  //  is switched while the system is started.
  //  THAT IS A TODO for the CKAudio System
  ckaudio_get_data_format(&dataFormat_);
}

void ClemensAudioDevice::stop() {
  ckaudio_stop();
  ckaudio_mixer_destroy(mixer_);
  mixer_ = nullptr;
}

unsigned ClemensAudioDevice::getAudioFrequency() const { return dataFormat_.frequency; }

unsigned ClemensAudioDevice::queue(ClemensAudio &audio, float deltaTime) {
  //  TODO, get frequency from emulator as a param to queue or in ClemensAudio (probably that)
  uint32_t framesToWrite = std::min(audio.frame_count, (uint32_t)(deltaTime * 48000));

  ckaudio_mixer_begin_update(mixer_);

  std::copy(audio.data + audio.frame_start * audio.frame_stride,
            audio.data + (audio.frame_start + framesToWrite) * audio.frame_stride,
            std::back_inserter(queuedFrames_));

  ckaudio_mixer_end_update(mixer_);
  return audio.frame_count;
}

uint32_t ClemensAudioDevice::mixAudio(CKAudioBuffer *audioBuffer, CKAudioTimePoint *timepoint,
                                      void *ctx) {
  auto *audio = reinterpret_cast<ClemensAudioDevice *>(ctx);
  const uint8_t* rawIn = audio->queuedFrames_.data();
  uint8_t *rawOut = (uint8_t *)(audioBuffer->data);
  uint32_t frameCount = ckaudio_mixer_render(audio->mixer_, audioBuffer, timepoint);
  uint32_t queuedFramesRendered = 0;
  uint32_t queuedFramesLimit = std::min(uint32_t(audio->queuedFrames_.size()), frameCount);

  //  this is pretty crude - what if the input buffer has a different frequency?
  //  need to implement proper streaming support into the mixer, so the mixer can
  //  ask for data, and the mixer will convert it accordingly into its own
  //  hardware format and sample rate.
  for (; queuedFramesRendered < queuedFramesLimit; ++queuedFramesRendered) {
    float* frameOut = (float *)rawOut;
    const uint16_t* frameIn = (const uint16_t*)rawIn;
    frameOut[0] += (frameIn[0] / 32767.0f) - 1.0f;
    frameOut[1] += (frameIn[1] / 32767.0f) - 1.0f;
    rawOut += audioBuffer->data_format.frame_size;
    rawIn += 8;
  }

  audio->queuedFrames_.erase(
    audio->queuedFrames_.begin(),
    audio->queuedFrames_.begin() + queuedFramesLimit);

  return frameCount;
}
