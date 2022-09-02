#include "clem_backend.hpp"
#include "clem_front.hpp"

#include "fmt/format.h"
#include "imgui/imgui.h"

static constexpr size_t kFrameMemorySize = 8 * 1024 * 1024;

ClemensFrontend::ClemensFrontend() :
  displayProvider_(),
  display_(displayProvider_),
  audio_(),
  frameWriteMemory_(kFrameMemorySize, malloc(kFrameMemorySize)),
  frameReadMemory_(kFrameMemorySize, malloc(kFrameMemorySize)),
  frameSeqNo_(std::numeric_limits<decltype(frameSeqNo_)>::max()),
  frameLastSeqNo_(std::numeric_limits<decltype(frameLastSeqNo_)>::max())
{

  audio_.start();
  ClemensBackend::Config backendConfig;
  backendConfig.type = ClemensBackend::Type::Apple2GS;
  backendConfig.audioSamplesPerSecond = audio_.getAudioFrequency();
  backend_ = std::make_unique<ClemensBackend>("gs_rom_3.rom", backendConfig,
    std::bind(&ClemensFrontend::backendStateDelegate, this, std::placeholders::_1));
  backend_->reset();
  backend_->run();
}

ClemensFrontend::~ClemensFrontend() {
  backend_ = nullptr;
  audio_.stop();
  free(frameWriteMemory_.getHead());
  free(frameReadMemory_.getHead());
}

void ClemensFrontend::frame(int width, int height, float deltaTime) {
  //  send commands to emulator thread
  //  get results from emulator thread
  //    video, audio, machine state, etc
  //
  //  developer_layout(width, height, deltaTime);
  bool isNewFrame = false;
  std::unique_lock<std::mutex> frameLock(frameMutex_);
  framePublished_.wait_for(frameLock, std::chrono::milliseconds::zero(),
    [this]() {
      return frameSeqNo_ != frameLastSeqNo_;
    });
  isNewFrame = frameLastSeqNo_ != frameSeqNo_;
  std::swap(frameWriteMemory_, frameReadMemory_);
  std::swap(frameWriteState_, frameReadState_);
  frameLastSeqNo_ = frameSeqNo_;

  frameLock.unlock();

  //  render video
  constexpr int kClemensScreenWidth = 720;
  constexpr int kClemensScreenHeight = 480;
  float screenUVs[2] { 0.0f, 0.0f };

  if (frameReadState_.mmioWasInitialized) {
    const uint8_t* e0mem = frameReadState_.bankE0;
    const uint8_t* e1mem = frameReadState_.bankE1;
    display_.start(frameReadState_.monitorFrame, kClemensScreenWidth, kClemensScreenHeight);
    if (frameReadState_.textFrame.format == kClemensVideoFormat_Text) {
      bool altCharSet = frameReadState_.vgcModeFlags & CLEM_VGC_ALTCHARSET;
      if (frameReadState_.vgcModeFlags & CLEM_VGC_80COLUMN_TEXT) {
        display_.renderText80Col(frameReadState_.textFrame, e0mem, e1mem, altCharSet);
      } else {
        display_.renderText40Col(frameReadState_.textFrame, e0mem, altCharSet);
      }
    }
    if (frameReadState_.graphicsFrame.format == kClemensVideoFormat_Double_Hires) {
      display_.renderDoubleHiresGraphics(frameReadState_.graphicsFrame, e0mem, e1mem);
    } else if (frameReadState_.graphicsFrame.format == kClemensVideoFormat_Hires) {
      display_.renderHiresGraphics(frameReadState_.graphicsFrame, e0mem);
    } else if (frameReadState_.graphicsFrame.format == kClemensVideoFormat_Super_Hires) {
      display_.renderSuperHiresGraphics(frameReadState_.graphicsFrame, e1mem);
    } else if (frameReadState_.vgcModeFlags & CLEM_VGC_LORES) {
      display_.renderLoresGraphics(frameReadState_.graphicsFrame, e0mem);
    }
    display_.finish(screenUVs);

    // render audio
    auto& audioFrame = frameReadState_.audioFrame;
    if (isNewFrame && audioFrame.frame_count > 0) {
      float *audio_frame_head = reinterpret_cast<float *>(
          audioFrame.data + audioFrame.frame_start * audioFrame.frame_stride);
      /*
      clem_card_ay3_render(&mockingboard_, audio_frame_head, audio.frame_count,
                           audio.frame_stride / sizeof(float),
                           audio_->getAudioFrequency());
      */
      unsigned consumedFrames = audio_.queue(audioFrame, deltaTime);
    }
  }

  ImGui::SetNextWindowPos(ImVec2(512, 32), ImGuiCond_FirstUseEver);
  ImGui::SetNextWindowContentSize(
      ImVec2(kClemensScreenWidth, kClemensScreenHeight));
  ImGui::Begin("Display", nullptr,
               ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoCollapse |
                   ImGuiWindowFlags_NoBringToFrontOnFocus);
  ImVec4 tint_col = ImVec4(1.0f, 1.0f, 1.0f, 1.0f); // No tint
  ImTextureID texId{(void *)((uintptr_t)display_.getScreenTarget().id)};
  ImVec2 p = ImGui::GetCursorScreenPos();
  ImVec2 display_uv(screenUVs[0], screenUVs[1]);
  ImGui::GetWindowDrawList()->AddImage(
      texId, p, ImVec2(p.x + kClemensScreenWidth, p.y + kClemensScreenHeight),
      ImVec2(0, 0), display_uv, ImGui::GetColorU32(tint_col));
  ImGui::End();

  ImGui::Text("Frontend: %0.2f fps", ImGui::GetIO().Framerate);
  ImGui::Text("Backend: %0.2f fps", frameReadState_.fps);

  backend_->publish();
}

void ClemensFrontend::input(const ClemensInputEvent &input){

}

void ClemensFrontend::backendStateDelegate(const ClemensBackendState& state) {
  {
    std::lock_guard<std::mutex> frameLock(frameMutex_);

    frameSeqNo_ = state.seqno;

    frameWriteState_.cpu = state.cpu;
    frameWriteState_.monitorFrame = state.monitor;
    frameWriteState_.textFrame = state.text;
    frameWriteState_.graphicsFrame = state.graphics;
    frameWriteState_.audioFrame = state.audio;
    frameWriteState_.fps = state.fps;
    frameWriteState_.mmioWasInitialized = state.mmio_was_initialized;

    //  copy over component state as needed
    frameWriteState_.vgcModeFlags = state.machine->mmio.vgc.mode_flags;

    //  copy over memory banks as needed
    frameWriteMemory_.reset();
    frameWriteState_.bankE0 = (uint8_t*)frameWriteMemory_.allocate(CLEM_IIGS_BANK_SIZE);
    memcpy(frameWriteState_.bankE0, state.machine->mega2_bank_map[0], CLEM_IIGS_BANK_SIZE);
    frameWriteState_.bankE1 = (uint8_t*)frameWriteMemory_.allocate(CLEM_IIGS_BANK_SIZE);
    memcpy(frameWriteState_.bankE1, state.machine->mega2_bank_map[1], CLEM_IIGS_BANK_SIZE);

    auto audioBufferSize = int32_t(state.audio.frame_total * state.audio.frame_stride);
    frameWriteState_.audioBuffer = (uint8_t*)frameWriteMemory_.allocate(audioBufferSize);
    memcpy(frameWriteState_.audioBuffer, state.audio.data, audioBufferSize);
  }
  framePublished_.notify_one();
}
