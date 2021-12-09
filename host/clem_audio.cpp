#include "clem_audio.hpp"

#include <algorithm>
#include <cassert>
#include <cstdlib>

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
  queuedFrameLimit_ = dataFormat_.frequency / 2;
  queuedFrameHead_ = 0;
  queuedFrameTail_ = 0;
  queuedPreroll_ = 0;
  queuedFrameBuffer_ = new uint8_t[queuedFrameLimit_ * queuedFrameStride_];

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
  if (!audio.frame_count) return 0;

  //  the audio data layout of our queue must be the same as the data coming
  //  in from the emulated device.  conversion between formats occurs during
  //  the actual mix.
  assert( audio.frame_stride == queuedFrameStride_);

  //  update will mutex mixAudio, which is necessary when running the mixer
  //  callbacks in the core audio thread.
  ckaudio_lock();
  ckaudio_mixer_update(mixer_);

  //  the input comes from a ring buffer
  uint32_t audioInHead = audio.frame_start;
  uint32_t audioInTail = (audio.frame_start + audio.frame_count) % audio.frame_total;
  uint32_t audioInEnd = audio.frame_total;
  uint32_t audioInAvailable = audio.frame_count;
  uint32_t audioOutAvailable = queuedFrameLimit_ - queuedFrameTail_;
  uint32_t audioInUsed = 0;

  if (audioInAvailable > audioOutAvailable && queuedFrameHead_ > 0) {
    //  adjust audioOut queue so that all frames *already* mixed are removed
    //  which will free up some space
    if (queuedFrameHead_ != queuedFrameTail_) {
      memmove(queuedFrameBuffer_,
              queuedFrameBuffer_ + (queuedFrameHead_ * queuedFrameStride_),
              (queuedFrameTail_ - queuedFrameHead_) * queuedFrameStride_);
      queuedFrameTail_ -= queuedFrameHead_;
    } else {
      queuedFrameTail_ = 0;
    }
    queuedFrameHead_ = 0;
    audioOutAvailable = queuedFrameTail_ - queuedFrameLimit_;
  }
  if (audioInAvailable > audioOutAvailable)  {
    //  copy only the amount we can to the output - so we don't have to
    //  bounds check against our queued output buffer when copying data.
    audioInAvailable = audioOutAvailable;
  }

  //  copy occurs from at most two windows in the input (re: ring buffer), to
  //  the single output window.
  uint8_t* audioOut = queuedFrameBuffer_ + queuedFrameTail_ * queuedFrameStride_;
  uint8_t* audioIn = audio.data + audioInHead * audio.frame_stride;

  uint32_t audioInCount;
  if (audioInHead + audioInAvailable > audioInEnd) {
    audioInCount = audioInEnd - audioInHead;
    memcpy(audioOut, audioIn, audioInCount * audio.frame_stride);
    queuedFrameTail_ += audioInCount;
    audioOut = queuedFrameBuffer_ + queuedFrameTail_ * queuedFrameStride_;
    audioIn = audio.data;
    audioInAvailable -= audioInCount;
    audioInUsed += audioInCount;
    audioInHead = 0;
  }
  audioInCount = std::min(audioInAvailable, audioInTail - audioInHead);

  memcpy(audioOut, audioIn, audioInCount * audio.frame_stride);
  queuedFrameTail_ += audioInCount;
  audioInAvailable -= audioInCount;
  audioInUsed += audioInCount;
  assert(audioInAvailable == 0);

  //printf("{%u:%u  %u:%u} ", audio.frame_count, audioInUsed, queuedFrameHead_, queuedFrameTail_);

  ckaudio_unlock();

  return audioInUsed;
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
  if (audio->queuedPreroll_ < 4800 /*audio->dataFormat_.frequency / 8 */) {
    audio->queuedPreroll_ = audio->queuedFrameTail_ - audio->queuedFrameHead_;
    return 0;
  }
  CKAudioBuffer streamBuffer;
  streamBuffer.data = audio->queuedFrameBuffer_ + audio->queuedFrameHead_ * audio->queuedFrameStride_;
  streamBuffer.frame_limit = audio->queuedFrameTail_ - audio->queuedFrameHead_;
  streamBuffer.ref_count = 0;
  streamBuffer.data_format.buffer_format = kCKAudioBufferFormatPCM;
  streamBuffer.data_format.frame_size = audio->queuedFrameStride_;
  streamBuffer.data_format.num_channels = 2;
  streamBuffer.data_format.frequency = audio->dataFormat_.frequency;
  //if (audioBuffer->frame_limit > streamBuffer.frame_limit) {
  //  printf("ST!");
  //}
  uint32_t renderedCount = ckaudio_mixer_render_waveform(audioBuffer, &streamBuffer, 100);
  audio->queuedFrameHead_ += renderedCount;
  return audioBuffer->frame_limit;
}
