#include "clem_audio.hpp"

#include <MMDeviceAPI.h>
#include <AudioClient.h>
#include <AudioPolicy.h>

#include <functiondiscoverykeys.h>
#include <stringapiset.h>

#include <cstdio>
#include <cstdint>
#include <cmath>

#include <Mmreg.h>

namespace {
  IMMDevice* findAudioDevice()
  {
    IMMDeviceEnumerator *deviceEnumerator = NULL;
    HRESULT hr =  CoCreateInstance(__uuidof(MMDeviceEnumerator),
                          NULL,
                          CLSCTX_INPROC_SERVER,
                          IID_PPV_ARGS(&deviceEnumerator));
    if (FAILED(hr)) {
      printf("Failed to initialize device enumerator: %08x\n", hr);
      return NULL;
    }

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

    deviceProperties->Release();
    deviceEnumerator->Release();

    return device;
  }

  IAudioClient* initAudioDevice(IMMDevice* device, ckaudio::DataFormat& dataFormat)
  {
    IAudioClient* audioClient;
    HRESULT hr = device->Activate(__uuidof(IAudioClient), CLSCTX_INPROC_SERVER,
                                  NULL, (void **)&audioClient);
    if (FAILED(hr)) {
      printf("Unable to activate the default endpoint (%08x)\n", hr);
      return NULL;
    }

    auto bufferFormat = ckaudio::BufferFormat::Unknown;
    unsigned desiredLatencyMS = 50;

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
        AUDCLNT_STREAMFLAGS_NOPERSIST,
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


}


ClemensAudio::ClemensAudio() :
  audioDevice_(NULL),
  audioClient_(NULL),
  audioRenderClient_(NULL),
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

  audioDevice_ = findAudioDevice();
  audioClient_ = initAudioDevice(audioDevice_, dataFormat_);
  if (audioClient_) {
    audioClient_->GetBufferSize(&audioEngineFrameCount_);
    printf("Audio client initialized with %u buffered frames\n", audioEngineFrameCount_);
  }
}

ClemensAudio::~ClemensAudio()
{
  stop();

  if (audioClient_) {
    audioClient_->Release();
  }
  if (audioDevice_) {
    audioDevice_->Release();
  }
}

void ClemensAudio::start()
{
  if (audioClient_) {
    HRESULT hr = audioClient_->GetService(IID_PPV_ARGS(&audioRenderClient_));
    if (FAILED(hr)) {
      printf("Failed to create audio render client: %08x\n", hr);
    }
  }
}

void ClemensAudio::stop()
{
  if (audioClient_) {
    audioRenderClient_->Release();
    audioRenderClient_ = NULL;
  }

}


unsigned ClemensAudio::getAudioFrequency() const {
  return dataFormat_.frequency;
}

void ClemensAudio::render(const ClemensAudioMixBuffer& mix_buffer)
{

}
