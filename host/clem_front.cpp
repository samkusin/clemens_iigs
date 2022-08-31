#include "clem_backend.hpp"
#include "clem_front.hpp"

ClemensFrontend::ClemensFrontend() :
  audio_()
{
  audio_.start();
  ClemensBackend::Config backendConfig;
  backendConfig.type = ClemensBackend::Type::Apple2GS;
  backendConfig.audioSamplesPerSecond = audio_.getAudioFrequency();
  backend_ = std::make_unique<ClemensBackend>("gs_rom_3.rom", backendConfig);
}

ClemensFrontend::~ClemensFrontend() {
  backend_ = nullptr;
  audio_.stop();
}

void ClemensFrontend::frame(int width, int height, float deltaTime) {
  //  send commands to emulator thread
  //  get results from emulator thread
  //    video, audio, machine state, etc
  //
  //  developer_layout(width, height, deltaTime);
}

void ClemensFrontend::input(const ClemensInputEvent &input){

}
