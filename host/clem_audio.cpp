#include "clem_audio.hpp"
#include "sokol/sokol_audio.h"
#include <algorithm>
#include <cassert>
#include <cstdlib>
#include <cstring>

#include "fmt/core.h"

//  TODO: optimize when needed per platform

ClemensAudioDevice::ClemensAudioDevice()
    : queuedFrameBuffer_(nullptr), queuedFrameHead_(0), queuedFrameTail_(0), queuedFrameLimit_(0),
      queuedFrameStride_(0) {}

ClemensAudioDevice::~ClemensAudioDevice() { stop(); }

void ClemensAudioDevice::start() {
    saudio_desc audioDesc = {};
    audioDesc.sample_rate = 48000;
    audioDesc.num_channels = 2;
    audioDesc.buffer_frames = 2048;
    audioDesc.stream_userdata_cb = &ClemensAudioDevice::mixAudio;
    audioDesc.user_data = this;
    saudio_setup(audioDesc);

    queuedFrameHead_ = 0;
    queuedFrameTail_ = 0;
    if (saudio_isvalid()) {
        queuedFrameStride_ = 2 * sizeof(float);
        queuedFrameLimit_ = saudio_sample_rate() / 2;
        queuedFrameBuffer_ = new uint8_t[queuedFrameLimit_ * queuedFrameStride_];
        fmt::print("ClemensAudioDevice queuedFrameBuffer {} khz, frame limit = {}, stride = {}\n",
                   audioDesc.sample_rate / 1000.0f, queuedFrameLimit_, queuedFrameStride_);
    } else {
        queuedFrameStride_ = 0;
        queuedFrameLimit_ = 0;
        queuedFrameBuffer_ = nullptr;
    }
}

void ClemensAudioDevice::stop() {
    if (saudio_isvalid()) {
        saudio_shutdown();
        delete[] queuedFrameBuffer_;
        queuedFrameBuffer_ = nullptr;
    }
}

unsigned ClemensAudioDevice::getAudioFrequency() const { return saudio_sample_rate(); }
unsigned ClemensAudioDevice::getBufferStride() const { return queuedFrameStride_; }

unsigned ClemensAudioDevice::queue(ClemensAudio &audio, float /*deltaTime */) {
    if (!audio.frame_count || !queuedFrameBuffer_)
        return 0;

    //  the audio data layout of our queue must be the same as the data coming
    //  in from the emulated device.  conversion between formats occurs during
    //  the actual mix.
    assert(audio.frame_stride == queuedFrameStride_);

    //  update will mutex mixAudio, which is necessary when running the mixer
    //  callbacks in the core audio thread.
    unsigned audioInUsed = 0;
    {
        std::lock_guard<std::mutex> lk(queuedFrameMutex_);

        //  the input comes from a flat buffer, so ignore the buffer frame limit.
        //   TODO: Don't think we're going to support wraparound here, so look into better
        //         ways for the API to illustrate this.
        uint32_t audioInHead = audio.frame_start;
        uint32_t audioInEnd = audio.frame_total;
        uint32_t audioInTail = audio.frame_start + audio.frame_count;
        if (audioInTail > audioInEnd) {
            audioInTail -= audioInTail;
        }
        uint32_t audioInAvailable = audio.frame_count;
        uint32_t audioOutAvailable = queuedFrameLimit_ - queuedFrameTail_;

        assert(audioInTail >= audioInHead);

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
            audioOutAvailable = queuedFrameLimit_ - queuedFrameTail_;
        }
        if (audioInAvailable > audioOutAvailable) {
            //  copy only the amount we can to the output - so we don't have to
            //  bounds check against our queued output buffer when copying data.
            audioInAvailable = audioOutAvailable;
        }

        //  copy occurs from at most two windows in the input (re: ring buffer), to
        //  the single output window.
        uint8_t *audioOut = queuedFrameBuffer_ + queuedFrameTail_ * queuedFrameStride_;
        uint8_t *audioIn = audio.data + audioInHead * audio.frame_stride;

        uint32_t audioInCount = audioInTail - audioInHead;
        assert(audioInHead + audioInAvailable <= audioInEnd);
        /*
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
        */
        uint32_t framesOutCount = std::min(audioOutAvailable, audioInCount);
        // memset(audioOut, 0xea, audioInCount * audio.frame_stride);
        memcpy(audioOut, audioIn, framesOutCount * audio.frame_stride);
        queuedFrameTail_ += framesOutCount;
        audioInAvailable -= framesOutCount;
        audioInUsed += framesOutCount;
        if (audioInAvailable > 0) {
            fmt::print("ClemensAudioDevice: {} frames not queued for playback, output = {}\n",
                       audioInAvailable, framesOutCount);
        }
    }

    return audioInUsed;
}

void ClemensAudioDevice::mixClemensAudio(float *audioOut, int num_frames, int num_channels) {
    std::lock_guard<std::mutex> lk(queuedFrameMutex_);
    //  clemens generates an output mix == to the hardware mixer to avoid having
    //  to upsample here.
    uint32_t queuedAvailable = queuedFrameTail_ - queuedFrameHead_;
    if (queuedAvailable != 0) {
        auto frameLimit = std::min(queuedAvailable, (uint32_t)num_frames);
        const uint8_t *inData = queuedFrameBuffer_ + queuedFrameHead_ * queuedFrameStride_;
        float *frameOut = audioOut;

        for (uint32_t frameIndex = 0; frameIndex < frameLimit; ++frameIndex) {
            auto *frameIn = reinterpret_cast<const float *>(inData);
            inData += queuedFrameStride_;
            frameOut[0] = frameIn[0];
            frameOut[1] = frameIn[1];
            frameOut += num_channels;
        }
        queuedFrameHead_ += frameLimit;
    }
}

void ClemensAudioDevice::mixAudio(float *buffer, int num_frames, int num_channels,
                                  void *user_data) {
    auto *audio = reinterpret_cast<ClemensAudioDevice *>(user_data);
    audio->mixClemensAudio(buffer, num_frames, num_channels);
}
