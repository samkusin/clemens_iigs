#include "clem_audio.hpp"

#include <algorithm>
#include <cstdlib>
#include <cmath>

static void *allocateLocal(void *user_ctx, size_t sz) { return malloc(sz); }

static void freeLocal(void *user_ctx, void *data) { free(data); }

ClemensAudioDevice::ClemensAudioDevice() :
  mixer_(NULL),
  queuedFrameBuffer_(nullptr),
  queuedFrameHead_(0),
  queuedFrameTail_(0),
  queuedFrameLimit_(0),
  queuedFrameStride_(0) {

  CKAudioAllocator allocator;
  allocator.allocate = &allocateLocal;
  allocator.free = &freeLocal;
  allocator.user_ctx = this;
  ckaudio_init(&allocator);
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

  //  data will always be 16-bit PCM stereo
  queuedFrameStride_ = 4;
  queuedFrameLimit_ = dataFormat_.frequency * 5;
  queuedFrameHead_ = 0;
  queuedFrameTail_ = 0;
  queuedPreroll_ = 0;
  queuedFrameBuffer_ = new uint8_t[queuedFrameLimit_ * queuedFrameStride_];
  tone_theta_ = 0.0f;

  ckaudio_mixer_set_track_action(mixer_, 0, CKAUDIO_MIXER_ACTION_TYPE_STREAM);
  ckaudio_mixer_set_track_volume(mixer_, 0, 75);
  ckaudio_mixer_set_track_callback(mixer_, 0, &ClemensAudioDevice::mixClemensAudio, this);
}

void ClemensAudioDevice::stop() {
  ckaudio_stop();
  if (mixer_) {
    ckaudio_mixer_destroy(mixer_);
    delete[] queuedFrameBuffer_;
    queuedFrameBuffer_ = nullptr;
    mixer_ = nullptr;
  }
}

unsigned ClemensAudioDevice::getAudioFrequency() const { return dataFormat_.frequency; }
unsigned ClemensAudioDevice::getBufferStride() const { return queuedFrameStride_; }

unsigned ClemensAudioDevice::queue(ClemensAudio &audio, float deltaTime) {
  //  update will mutex mixAudio and the streaming callbacks
  unsigned consumedCount = audio.frame_count;
  ckaudio_lock();
  ckaudio_mixer_update(mixer_);
  {
    unsigned availFramesCount = queuedFrameLimit_ - queuedFrameTail_;
    if (consumedCount > availFramesCount) {
      consumedCount = availFramesCount;
    }

    if (queuedFrameHead_ > 0) {
      if (queuedFrameHead_ != queuedFrameTail_) {
        // free up space used (up to the head)
        memmove(queuedFrameBuffer_,
                queuedFrameBuffer_ + queuedFrameHead_ * queuedFrameStride_,
                queuedFrameStride_ * (queuedFrameTail_ - queuedFrameHead_));
        queuedFrameTail_ = queuedFrameHead_;
      } else {
        queuedFrameTail_ = 0;
      }
      queuedFrameHead_ = 0;
    }
    if (consumedCount > 0) {
      memcpy(queuedFrameBuffer_ + queuedFrameTail_ * queuedFrameStride_,
            audio.data + audio.frame_start * audio.frame_stride,
            consumedCount * queuedFrameStride_);
      queuedFrameTail_ += consumedCount;
    }
  }
  ckaudio_unlock();
  return consumedCount;
}

uint32_t ClemensAudioDevice::mixAudio(CKAudioBuffer *audioBuffer, CKAudioTimePoint *timepoint,
                                      void *ctx) {
  auto *audio = reinterpret_cast<ClemensAudioDevice *>(ctx);
  uint32_t frameCount = ckaudio_mixer_render(audio->mixer_, audioBuffer, timepoint);
  return frameCount;
}

uint32_t ClemensAudioDevice::mixClemensAudio(CKAudioBuffer *audioBuffer,
                                             CKAudioTimePoint *timepoint,
                                             void *ctx) {
  auto *audio = reinterpret_cast<ClemensAudioDevice *>(ctx);
  if (audio->queuedPreroll_ < 4800) {
    audio->queuedPreroll_ = audio->queuedFrameTail_ - audio->queuedFrameHead_;
    return 0;
  }
  constexpr float kPi2 = 6.28318531f;
  const float dt_theta = (300.0f *  kPi2)/audioBuffer->data_format.frequency;
  uint8_t* rawOut = (uint8_t*)audioBuffer->data;
  for (uint32_t i = 0; i < audioBuffer->frame_limit; ++i) {
    float* out = (float*)rawOut;
    out[0] = sinf(audio->tone_theta_) * 0.75f;
    out[1] = out[0];
    rawOut += audioBuffer->data_format.frame_size;
    audio->tone_theta_ += dt_theta;
    if (audio->tone_theta_ >= kPi2) {
      audio->tone_theta_ -= kPi2;
    }
  }
  /*
  CKAudioBuffer streamBuffer;
  streamBuffer.data = audio->queuedFrameBuffer_ + audio->queuedFrameHead_ * audio->queuedFrameStride_;
  streamBuffer.frame_limit = audio->queuedFrameTail_ - audio->queuedFrameHead_;
  streamBuffer.ref_count = 0;
  streamBuffer.data_format.buffer_format = kCKAudioBufferFormatPCM;
  streamBuffer.data_format.frame_size = audio->queuedFrameStride_;
  streamBuffer.data_format.num_channels = 2;
  streamBuffer.data_format.frequency = audio->dataFormat_.frequency;
  if (audioBuffer->frame_limit > streamBuffer.frame_limit) {
    printf("ST!");
  }
  audio->queuedFrameHead_ += ckaudio_mixer_render_waveform(audioBuffer, &streamBuffer, 100);
  return std::min(audioBuffer->frame_limit, streamBuffer.frame_limit);
*/
  return audioBuffer->frame_limit;
}
