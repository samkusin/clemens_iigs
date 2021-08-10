#include "clem_audio.hpp"

#define NOMIXMAX
#include <MMDeviceAPI.h>
#include <AudioClient.h>
#include <AudioPolicy.h>

#include <functiondiscoverykeys.h>
#include <stringapiset.h>

#include <cstdio>
#include <cstdint>
#include <cstdlib>
#include <cmath>
#include <cassert>

#include <Mmreg.h>

namespace {
  IMMDevice* findAudioDevice(IMMDeviceEnumerator* deviceEnumerator)
  {
    IMMDevice* device;
    deviceEnumerator->GetDefaultAudioEndpoint(EDataFlow::eRender, ERole::eConsole,
        &device);

    IPropertyStore* deviceProperties;
    device->OpenPropertyStore(STGM_READ, &deviceProperties);

    PROPVARIANT devicePropertyValue;
    PropVariantInit(&devicePropertyValue);
    deviceProperties->GetValue(PKEY_Device_FriendlyName, &devicePropertyValue);
    deviceProperties->Release();

    char deviceName[128];
    WideCharToMultiByte(CP_UTF8, 0, devicePropertyValue.pwszVal, -1,
                        deviceName, sizeof(deviceName), NULL, NULL);

    printf("Device Endpoint: %s\n", deviceName);
    return device;
  }

  IAudioClient* initAudioDevice(
    IMMDevice* device,
    ckaudio::DataFormat& dataFormat,
    unsigned desiredLatencyMS
  ) {
    IAudioClient* audioClient;
    HRESULT hr = device->Activate(__uuidof(IAudioClient), CLSCTX_INPROC_SERVER,
                                  NULL, (void **)&audioClient);
    if (FAILED(hr)) {
      printf("Unable to activate the default endpoint (%08x)\n", hr);
      return NULL;
    }

    auto bufferFormat = ckaudio::BufferFormat::Unknown;

    WAVEFORMATEX* endpointFormat;
    hr = audioClient->GetMixFormat(&endpointFormat);
    if (FAILED(hr)) {
      printf("Unable to acquire wave format (%08x)\n", hr);
      return NULL;
    }
    printf("Endpoint Mix Channels:      %u\n", endpointFormat->nChannels);
    printf("Endpoint Mix Frequency:     %u\n", endpointFormat->nSamplesPerSec);
    printf("Endpoint Mix BPS:           %u\n", endpointFormat->wBitsPerSample);
    printf("Endpoint Mix Block Align:   %u\n", endpointFormat->nBlockAlign);
    printf("Endpoint Mix Format Tag:    %u\n", endpointFormat->wFormatTag);

    if (endpointFormat->wFormatTag == WAVE_FORMAT_EXTENSIBLE) {
      WAVEFORMATEXTENSIBLE* extensibleFormat = (WAVEFORMATEXTENSIBLE*)endpointFormat;
      printf("Endpoint Mix Channel Mask:  %u\n", extensibleFormat->dwChannelMask);
      printf("Endpoint Mix Valid BPS:     %u\n", extensibleFormat->Samples.wValidBitsPerSample);
      if (extensibleFormat->SubFormat == KSDATAFORMAT_SUBTYPE_PCM) {
        bufferFormat = ckaudio::BufferFormat::PCM;
        printf("Endpoint Mix Subformat:     PCM\n");
      } else if (extensibleFormat->SubFormat == KSDATAFORMAT_SUBTYPE_IEEE_FLOAT) {
        bufferFormat = ckaudio::BufferFormat::Float;
        printf("Endpoint Mix Subformat:     float\n");
      } else {
        printf("Endpoint Mix Subformat:     unsupported\n");
      }
    } else if (endpointFormat->wFormatTag == WAVE_FORMAT_PCM) {
      bufferFormat = ckaudio::BufferFormat::PCM;
      printf("Endpoint Format:          PCM\n");
    } else if (endpointFormat->wFormatTag == WAVE_FORMAT_IEEE_FLOAT) {
      bufferFormat = ckaudio::BufferFormat::Float;
      printf("Endpoint Format:          float\n");
    } else {
      printf("Endpoint Format:          unsupported\n");
    }

    // WAVE_FORMAT_EXTENSIBLE = format trag
    printf("Audio client initializing...\n");
    hr = audioClient->Initialize(
        AUDCLNT_SHAREMODE_SHARED,
        AUDCLNT_STREAMFLAGS_EVENTCALLBACK | AUDCLNT_STREAMFLAGS_NOPERSIST,
        desiredLatencyMS * 10000,
        0,
        endpointFormat,
        NULL);

    if (FAILED(hr)) {
      audioClient->Release();
      return NULL;
    }

    dataFormat.frameFormat = bufferFormat;
    dataFormat.frameSize = endpointFormat->nBlockAlign;
    dataFormat.numChannels = endpointFormat->nChannels;
    dataFormat.frequency = endpointFormat->nSamplesPerSec;
    return audioClient;
  }

  struct EncodeBuffer {
    void* ptr;
    unsigned frameStart;
    unsigned frameEnd;
    unsigned frameSize;
  };


  unsigned encode_pcm_16_to_float_stereo(
    EncodeBuffer& output,
    const EncodeBuffer& input
  ) {
    unsigned inFrameIdx = input.frameStart;
    void* inptr = (void *)(uintptr_t(input.ptr) + input.frameStart * input.frameSize);
    void* outptr = (void *)(uintptr_t(output.ptr) + output.frameStart * output.frameSize);
    for (unsigned outFrameIdx = output.frameStart;
         outFrameIdx <= output.frameEnd && inFrameIdx <= input.frameEnd;
         ++outFrameIdx, ++inFrameIdx
    ) {
      uint16_t* in16 = (uint16_t*)inptr;
      float* outf32 = (float*)outptr;

      outf32[0] = (in16[0] / 32768.0f) - 1.0f;
      outf32[1] = (in16[1] / 32768.0f) - 1.0f;

      inptr = ((uint8_t*)inptr) + input.frameSize;
      outptr = ((uint8_t*)outptr) + output.frameSize;
    }
    return inFrameIdx - input.frameStart;
  }

} // namespace anon

//static unsigned sks_frame = 0;

DWORD __stdcall ckAudioRenderWorker(LPVOID context)
{
  auto* audio = reinterpret_cast<ClemensAudioDevice*>(context);

  //  TODO: It may just make sense to move all Core Audio logic, including
  //        enumeration here so that we don't require COM initialization on
  //        the controller side.
  //        This would require leveraging an event system to receive and
  //        broadcast commands/notifications.  This brings some cross-platform
  //        implications as well...
  //
  CoInitializeEx(NULL, COINIT_MULTITHREADED);

  HANDLE waitHandles[2] = {audio->shutdownEvent_, audio->readyEvent_};

  bool isActive = true;

  while (isActive) {
      DWORD waitResult = WaitForMultipleObjects(2, waitHandles, FALSE, INFINITE);
      switch (waitResult) {
        case WAIT_OBJECT_0:
          isActive = false;
          break;
        case WAIT_OBJECT_0 + 1:
           /* populate the already played frames in the audio buffer with new
             data from the shared controller/worker buffer
          */
          audio->render();
          break;
      }
  }

  CoUninitialize();

  return 0;
}

static void* allocateLocal(unsigned sz)
{
  return malloc(sz);
}

void freeLocal(void* data)
{
  free(data);
}


ClemensAudioDevice::ClemensAudioDevice() :
  audioDevice_(NULL),
  audioClient_(NULL),
  audioRenderClient_(NULL),
  audioThread_(INVALID_HANDLE_VALUE),
  shutdownEvent_(INVALID_HANDLE_VALUE),
  readyEvent_(INVALID_HANDLE_VALUE),
  audioEngineFrameCount_(0)
{
  IMMDeviceEnumerator *deviceEnumerator = NULL;
  HRESULT hr =  CoCreateInstance(__uuidof(MMDeviceEnumerator),
                         NULL,
                         CLSCTX_INPROC_SERVER,
                         IID_PPV_ARGS(&deviceEnumerator));
  if (FAILED(hr)) {
    printf("Failed to initialize device enumerator: %08x\n", hr);
    return;
  }

  desiredLatencyMS_ = 50;
  audioDevice_ = findAudioDevice(deviceEnumerator);
  audioClient_ = initAudioDevice(audioDevice_, dataFormat_, desiredLatencyMS_);
  if (audioClient_ == NULL) {
    return;
  }
  audioClient_->GetBufferSize(&audioEngineFrameCount_);
  printf("Audio client initialized with %u buffered frames\n", audioEngineFrameCount_);

  shutdownEvent_ = CreateEvent(NULL, FALSE, FALSE, "ckaudio_teardown");
  readyEvent_ = CreateEvent(NULL, FALSE, FALSE, "ckaudio_data");
  audioClient_->SetEventHandle(readyEvent_);

  //  TODO: reevaluate, one second seems a bit long...
  audioFrameLimit_ = dataFormat_.frequency;
  audioBuffer_ = (uint8_t*)allocateLocal(audioFrameLimit_ *dataFormat_.frameSize);
  audioReadHead_.store(0);
  audioWriteHead_.store(0);
}

ClemensAudioDevice::~ClemensAudioDevice()
{
  stop();

  if (audioClient_) {
    audioClient_->Release();

    CloseHandle(shutdownEvent_);
    CloseHandle(readyEvent_);

    freeLocal(audioBuffer_);
  }
  if (audioDevice_) {
    audioDevice_->Release();
  }
}

void ClemensAudioDevice::start()
{
  if (audioClient_) {

    HRESULT hr = audioClient_->GetService(IID_PPV_ARGS(&audioRenderClient_));
    if (FAILED(hr)) {
      printf("Failed to create audio render client: %08x\n", hr);
    }
    // preroll silence
    BYTE *data;
    hr = audioRenderClient_->GetBuffer(audioEngineFrameCount_, &data);
    if (FAILED(hr)) {
      printf("GetBuffer() failed (hr: %08x)", hr);
      return;
    }
    hr = audioRenderClient_->ReleaseBuffer(
      audioEngineFrameCount_, AUDCLNT_BUFFERFLAGS_SILENT);
    if (FAILED(hr)) {
      printf("ReleaseBuffer() failed (hr: %08x)", hr);
      return;
    }

    audioThread_ = CreateThread(NULL, 0, ckAudioRenderWorker, this, 0, NULL);
    audioClient_->Start();
  }
}

void ClemensAudioDevice::stop()
{
  if (audioClient_) {
    audioClient_->Stop();

    SetEvent(shutdownEvent_);
    WaitForSingleObject(audioThread_, INFINITE);
    CloseHandle(audioThread_);

    audioRenderClient_->Release();
    audioRenderClient_ = NULL;
  }

}


unsigned ClemensAudioDevice::getAudioFrequency() const {
  return dataFormat_.frequency;
}

void ClemensAudioDevice::queue(const ClemensAudio& source)
{
  if (!audioClient_) return;

  if (source.frame_count == 0) return;

  //  TODO: populate buffer

  //  The input buffer is a simple data, data_size buffer to copy data from
  //  The input buffer data format is 16-bit PCM stereo
  //  The input buffer is consumed immediately - if the buffer cannot fit in
  //    the available write space to the audio buffer, the remainder will be
  //    dropped (input clipped)

  //  The audio system has two points of data entry and exit:
  //    Data is added via the queue()
  //    Data is played via the thread()
  //    Leverages a shared ring buffer

  //  The shared ring buffer has two heads
  //    read head
  //    write head
  //    an empty buffer is read = write
  //    a full buffer is read = write + 1 sample
  //    frames in the shared buffer are the same format as the hardware
  //      playback format

  //  encodeAudio(output, input, format)
  //    output available
  //    input available
  //
  //

  //  check if there's space to write - we only modify write in this block and
  //  do not mind that read can advance at any time
  uint32_t currentReadHead = audioReadHead_.load();
  uint32_t currentWriteHead = audioWriteHead_.load();

  EncodeBuffer input;
  input.ptr = source.data;
  input.frameSize = source.frame_stride;

  EncodeBuffer output;
  output.ptr = audioBuffer_;
  output.frameSize = dataFormat_.frameSize;

  uint32_t inputHead = source.frame_start;
  uint32_t inputTail = (source.frame_start + source.frame_count) % source.frame_total;
  uint32_t inputRemaining = source.frame_count;

  //printf("SKS: W0: %u\n", currentWriteHead);

  while (inputHead != inputTail) {
    input.frameStart = inputHead;
    assert(inputRemaining > 0 && inputRemaining <= source.frame_count);
    if (input.frameStart + inputRemaining >= source.frame_total) {
      input.frameEnd = source.frame_total - 1;
    } else {
      input.frameEnd = (input.frameStart + inputRemaining) - 1;
    }
    output.frameStart = currentWriteHead;
    if (currentReadHead > currentWriteHead) {
      output.frameEnd = currentReadHead - 1;
    } else {
      output.frameEnd = audioFrameLimit_ - 1;
    }

    uint32_t writtenFrames = encode_pcm_16_to_float_stereo(output, input);

    assert(inputRemaining >= writtenFrames);
    inputHead += writtenFrames;
    inputRemaining -= writtenFrames;
    if (inputHead >= source.frame_total) {
      inputHead = 0;
    }
    currentWriteHead += writtenFrames;
    if (currentWriteHead >= audioFrameLimit_) {
      currentWriteHead = 0;
    }
  }
  //printf("SKS: W1: %u\n", currentWriteHead);
  audioWriteHead_.store(currentWriteHead);
}

void ClemensAudioDevice::render()
{
  UINT32 queuedFrameCount;
  HRESULT hr = audioClient_->GetCurrentPadding(&queuedFrameCount);
  if (FAILED(hr)) {
    return;
  }
  UINT32 availableFrames = audioEngineFrameCount_ - queuedFrameCount;
  UINT32 framesWritten = 0;
  uint32_t currentReadHead = audioReadHead_.load();
  uint32_t currentWriteHead = audioWriteHead_.load();
  BYTE* data = NULL;
  //printf("SKS: R0: %u\n", currentReadHead);
  while (availableFrames > 0 && currentReadHead != currentWriteHead) {
    //  This operation is much simpler than the queue() method, since
    //  data formats are guaranteed to be the same - so a direct copy to the
    //  target buffer.  Also we know how many frames can copy over, and we
    //  poll the number of frames we are able to write.
    //
    uint32_t readFrames;
    if (currentReadHead <= currentWriteHead) {
      readFrames = currentWriteHead - currentReadHead;
    } else {
      readFrames = audioFrameLimit_ - currentReadHead;
    }

    if (data == NULL) {
      //printf("sks A: %u (%u)\n", sks_frame, availableFrames);
      hr = audioRenderClient_->GetBuffer(availableFrames, &data);
      if (FAILED(hr)) {
          printf("FAILED! (%08x)\n", hr);
    break;
      }
    }

    //  TODO availableFrames needs to be decremented if this loop has already
    //  copied data, since available frame count has one down after the copy
    //  but we need to fix the termination logic below...
    unsigned framesToCopy;
    if (readFrames < availableFrames) {
      framesToCopy = readFrames;
    } else {
      framesToCopy = availableFrames;
    }
    //memset(data, 0, framesToCopy * dataFormat_.frameSize);

    memcpy(data, audioBuffer_ + currentReadHead * dataFormat_.frameSize,
            framesToCopy * dataFormat_.frameSize);

    //printf("sks B: %u (%u), %u\n", sks_frame, framesToCopy, currentReadHead <= currentWriteHead ? 0 : 1);

    currentReadHead += framesToCopy;
    if (currentReadHead >= audioFrameLimit_) {
      currentReadHead = 0;
    }
    framesWritten += framesToCopy;
    data += framesToCopy * dataFormat_.frameSize;
    availableFrames -= framesToCopy;
    if (availableFrames == 0 || currentReadHead == currentWriteHead) {
      audioRenderClient_->ReleaseBuffer(framesWritten, 0);
      data = NULL;
      //printf("sks C: %u (%u)\n", sks_frame, framesWritten);
      //++sks_frame;

      framesWritten = 0;
      hr = audioClient_->GetCurrentPadding(&queuedFrameCount);
      if (FAILED(hr)) {
          printf("FAILED2!\n");
      }
      availableFrames = audioEngineFrameCount_ - queuedFrameCount;
    }
  } // end while

  //printf("SKS: R1: %u\n", currentReadHead);
  audioReadHead_.store(currentReadHead);
}
