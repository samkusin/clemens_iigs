#include "clem_audio.hpp"
#include <algorithm>
#include <cassert>
#include <cstdlib>
#include <cstring>

static void *allocateLocal(void */*user_ctx*/, size_t sz) { return malloc(sz); }

static void freeLocal(void * /*user_ctx*/, void *data) { free(data); }

//  TODO: optimize when needed per platform

ClemensAudioDevice::ClemensAudioDevice()
    : queuedFrameBuffer_(nullptr), queuedFrameHead_(0), queuedFrameTail_(0),
      queuedFrameLimit_(0), queuedFrameStride_(0) {

  CKAudioAllocator allocator;
  allocator.allocate = &allocateLocal;
  allocator.free = &freeLocal;
  allocator.user_ctx = this;
  ckaudio_init(&allocator);
}

ClemensAudioDevice::~ClemensAudioDevice() { ckaudio_term(); }

void ClemensAudioDevice::start() {
  ckaudio_timepoint_make_null(&lastTimepoint_);
  ckaudio_start(&ClemensAudioDevice::mixAudio, this);

  //  TODO: updating our device format should go into an event handler that
  //  traps events from the CKAudioCore/Mixer (i.e what happens if a device
  //  is switched while the system is started.
  //  THAT IS A TODO for the CKAudio System
  ckaudio_get_data_format(&dataFormat_);

  //  data is 2 channel 32-bit float
  queuedFrameStride_ = 2 * sizeof(float);
  queuedFrameLimit_ = dataFormat_.frequency / 2;
  queuedFrameHead_ = 0;
  queuedFrameTail_ = 0;
  queuedFrameBuffer_ = new uint8_t[queuedFrameLimit_ * queuedFrameStride_];
}

void ClemensAudioDevice::stop() {
  ckaudio_stop();
  delete[] queuedFrameBuffer_;
  queuedFrameBuffer_ = nullptr;
}

unsigned ClemensAudioDevice::getAudioFrequency() const {
  return dataFormat_.frequency;
}
unsigned ClemensAudioDevice::getBufferStride() const {
  return queuedFrameStride_;
}

unsigned ClemensAudioDevice::queue(ClemensAudio &audio, float /*deltaTime */) {
  if (!audio.frame_count)
    return 0;

  //  the audio data layout of our queue must be the same as the data coming
  //  in from the emulated device.  conversion between formats occurs during
  //  the actual mix.
  assert(audio.frame_stride == queuedFrameStride_);

  //  update will mutex mixAudio, which is necessary when running the mixer
  //  callbacks in the core audio thread.
  ckaudio_lock();

  //  the input comes from a ring buffer
  uint32_t audioInHead = audio.frame_start;
  uint32_t audioInTail =
      (audio.frame_start + audio.frame_count) % audio.frame_total;
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
  if (audioInAvailable > audioOutAvailable) {
    //  copy only the amount we can to the output - so we don't have to
    //  bounds check against our queued output buffer when copying data.
    audioInAvailable = audioOutAvailable;
  }

  //  copy occurs from at most two windows in the input (re: ring buffer), to
  //  the single output window.
  uint8_t *audioOut =
      queuedFrameBuffer_ + queuedFrameTail_ * queuedFrameStride_;
  uint8_t *audioIn = audio.data + audioInHead * audio.frame_stride;

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

  // printf("{%u:%u  %u:%u} ", audio.frame_count, audioInUsed, queuedFrameHead_,
  // queuedFrameTail_);

  ckaudio_unlock();

  return audioInUsed;
}

uint32_t ClemensAudioDevice::mixClemensAudio(CKAudioBuffer *outBuffer,
                                             uint32_t outFramesAvailable) {
  //  clemens generates an output mix == to the hardware mixer to avoid having
  //  to upsample here.
  if (outBuffer->data_format.frequency != dataFormat_.frequency) {
    return 0;
  }
  uint32_t queuedAvailable = queuedFrameTail_ - queuedFrameHead_;
  if (queuedAvailable == 0) {
    return 0;
  }
  auto frameLimit = std::min(queuedAvailable, outFramesAvailable);
  auto *outData = reinterpret_cast<uint8_t *>(outBuffer->data);
  const auto *inData =
      queuedFrameBuffer_ + queuedFrameHead_ * queuedFrameStride_;
  switch (outBuffer->data_format.buffer_format) {
  case kCKAudioBufferFormatFloat:
    for (uint32_t frameIndex = 0; frameIndex < frameLimit; ++frameIndex) {
      auto *pcmFrame = reinterpret_cast<const float *>(inData);
      auto *f32Frame = reinterpret_cast<float *>(outData);
      f32Frame[0] = pcmFrame[0];
      f32Frame[1] = pcmFrame[1];
      inData += queuedFrameStride_;
      outData += outBuffer->data_format.frame_size;
    }
    break;
  default:
    frameLimit = 0;
    break;
  }
  queuedFrameHead_ += frameLimit;
  return frameLimit;
}

uint32_t ClemensAudioDevice::mixAudio(CKAudioBuffer *audioBuffer,
                                      CKAudioTimePoint */*timepoint */, void *ctx) {
  auto *audio = reinterpret_cast<ClemensAudioDevice *>(ctx);

  uint32_t frameIndex =
      audio->mixClemensAudio(audioBuffer, audioBuffer->frame_limit);
  /*
  switch (audioBuffer->data_format.buffer_format) {
    case kCKAudioBufferFormatFloat:
      if (audioBuffer->data_format.num_channels == 2) {
        frameIndex = mixSilenceF32Stereo(audioBuffer, frameIndex);
      }
      break;
    default:
      break;
  }
  */

  return frameIndex;
}
