#include "clem_backend.hpp"
#include "clem_front.hpp"
#include "clem_host_platform.h"

#include "fmt/format.h"
#include "imgui/imgui.h"

template<typename TBufferType>
struct FormatView {
  using BufferType = TBufferType;
  using StringType = typename BufferType::ValueType;
  using LevelType = typename StringType::Type;

  BufferType &buffer_;
  FormatView(BufferType &buffer) : buffer_(buffer) {}
  template <typename... Args>
  void format(LevelType type, const char *formatStr, const Args &...args) {
    size_t sz = fmt::formatted_size(formatStr, args...);
    auto& line = rotateBuffer();
    line.type = type;
    line.text.resize(sz);
    fmt::format_to_n(line.text.data(), sz, formatStr, args...);
  }
  void print(LevelType type, const char* text) {
    auto& line = rotateBuffer();
    line.type = type;
    line.text.assign(text);
  }
  void print(LevelType type, std::string_view text) {
    auto& line = rotateBuffer();
    line.type = type;
    line.text.assign(text);
  }
  void newline() {
    auto& line = rotateBuffer();
    line.type = LevelType::Info;
    line.text.clear();
  }
private:
  StringType& rotateBuffer() {
    if (buffer_.isFull()) {
      buffer_.pop();
    }
    StringType& tail = *buffer_.acquireTail();
    buffer_.push();
    return tail;
  }
};

#define CLEM_TERM_COUT FormatView<decltype(ClemensFrontend::terminalLines_)>(terminalLines_)

static constexpr size_t kFrameMemorySize = 8 * 1024 * 1024;

ClemensFrontend::ClemensFrontend(const cinek::ByteBuffer& systemFontLoBuffer,
                                 const cinek::ByteBuffer& systemFontHiBuffer) :
  displayProvider_(systemFontLoBuffer, systemFontHiBuffer),
  display_(displayProvider_),
  audio_(),
  frameWriteMemory_(kFrameMemorySize, malloc(kFrameMemorySize)),
  frameReadMemory_(kFrameMemorySize, malloc(kFrameMemorySize)),
  frameSeqNo_(std::numeric_limits<decltype(frameSeqNo_)>::max()),
  frameLastSeqNo_(std::numeric_limits<decltype(frameLastSeqNo_)>::max()),
  lastFrameCPUPins_{},
  lastFrameCPURegs_{},
  lastFrameIRQs_(0),
  lastFrameNMIs_(0),
  emulatorHasKeyboardFocus_(true),
  terminalMode_(TerminalMode::Command)
{
  audio_.start();
  ClemensBackend::Config backendConfig;
  backendConfig.type = ClemensBackend::Type::Apple2GS;
  backendConfig.audioSamplesPerSecond = audio_.getAudioFrequency();
  backend_ = std::make_unique<ClemensBackend>("gs_rom_3.rom", backendConfig,
    std::bind(&ClemensFrontend::backendStateDelegate, this, std::placeholders::_1));
  backend_->setRefreshFrequency(60);
  backend_->reset();
  backend_->run();

  CLEM_TERM_COUT.print(TerminalLine::Info, "Welcome to the Clemens IIgs Emulator");
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
  if (isNewFrame) {
    lastFrameCPURegs_ = frameReadState_.cpu.regs;
    lastFrameCPUPins_ = frameReadState_.cpu.pins;
    lastFrameIRQs_ = frameReadState_.irqs;
    lastFrameNMIs_ = frameReadState_.nmis;

    std::swap(frameWriteMemory_, frameReadMemory_);
    std::swap(frameWriteState_, frameReadState_);

    LogOutputNode* logNode = frameReadState_.logNode;
    while (logNode) {
      if (consoleLines_.isFull()) {
        consoleLines_.pop();
      }
      TerminalLine logLine;
      logLine.text.assign((char *)logNode + sizeof(LogOutputNode), logNode->sz);
      switch (logNode->logLevel) {
        case CLEM_DEBUG_LOG_DEBUG:
          logLine.type = TerminalLine::Debug;
          break;
        case CLEM_DEBUG_LOG_INFO:
          logLine.type = TerminalLine::Info;
          break;
        case CLEM_DEBUG_LOG_WARN:
          logLine.type = TerminalLine::Warn;
          break;
        case CLEM_DEBUG_LOG_FATAL:
        case CLEM_DEBUG_LOG_UNIMPL:
          logLine.type = TerminalLine::Error;
          break;
        default:
          logLine.type = TerminalLine::Info;
          break;
      }
      consoleLines_.push(std::move(logLine));
      logNode = logNode->next;
    }

    breakpoints_.clear();
    for (unsigned bpIndex = 0; bpIndex < frameReadState_.breakpointCount; ++bpIndex) {
      breakpoints_.emplace_back(frameReadState_.breakpoints[bpIndex]);
    }
    if (frameReadState_.commandFailed.has_value()) {
      if (*frameReadState_.commandFailed) {
        CLEM_TERM_COUT.print(TerminalLine::Error, "Failed.");
      } else {
        CLEM_TERM_COUT.print(TerminalLine::Info, "Ok.");
      }
    }
    if (frameReadState_.hitBreakpoint) {
      CLEM_TERM_COUT.format(TerminalLine::Info, "Breakpoint {} hit {:02X}/{:04X}.",
                            frameReadState_.hitBreakpoint - frameReadState_.breakpoints,
                            (frameReadState_.hitBreakpoint->address >> 16) & 0xff,
                            frameReadState_.hitBreakpoint->address & 0xffff);
    }
    frameLastSeqNo_ = frameSeqNo_;
  }
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

  const int kLineSpacing = ImGui::GetTextLineHeightWithSpacing();
  const int kTerminalViewHeight = std::max(kLineSpacing * 6, int(height * 0.33f));
  const int kMachineStateViewWidth = std::max(int(width * 0.33f), 480);
  const int kMonitorX = kMachineStateViewWidth;
  const int kMonitorViewWidth = width - kMonitorX;
  const int kMonitorViewHeight = height - kTerminalViewHeight;
  ImVec2 monitorSize(kMonitorViewWidth, kMonitorViewHeight);

  doMachineStateLayout(ImVec2(0, 0), ImVec2(kMachineStateViewWidth, height));
  doMachineViewLayout(ImVec2(kMonitorX, 0), monitorSize, screenUVs[0], screenUVs[1]);
  doMachineTerminalLayout(ImVec2(kMonitorX, height - kTerminalViewHeight),
                          ImVec2(width - kMonitorX, kTerminalViewHeight));

  backend_->publish();
}

static ImColor getModifiedColor(bool hi) {
  const ImColor kModifiedColor(255, 0, 255, hi ? 255 : 128);
  return kModifiedColor;
}

static ImColor getDefaultColor(bool hi) {
  const ImColor kDefaultColor(255, 255, 255, hi ? 255 : 128);
  return kDefaultColor;
}

#define CLEM_HOST_GUI_CPU_PINS_COLOR(_field_) \
  lastFrameCPUPins_._field_ != frameReadState_.cpu.pins._field_ ? \
    getModifiedColor(frameReadState_.cpu.pins._field_) : \
    getDefaultColor(frameReadState_.cpu.pins._field_)

#define CLEM_HOST_GUI_CPU_PINS_INV_COLOR(_field_) \
  lastFrameCPUPins_._field_ != frameReadState_.cpu.pins._field_ ? \
    getModifiedColor(!frameReadState_.cpu.pins._field_) : \
    getDefaultColor(!frameReadState_.cpu.pins._field_)

#define CLEM_HOST_GUI_CPU_FIELD_COLOR(_field_) \
  lastFrameCPURegs_._field_ != frameReadState_.cpu.regs._field_ ? \
    getModifiedColor(true) : getDefaultColor(true)

template<typename T>
static ImColor getStatusFieldColor(T a, T b, T statusMask) {
  return (a & statusMask) != (b & statusMask) ?
      getModifiedColor(b & statusMask) : getDefaultColor(b & statusMask);
}

void ClemensFrontend::doMachineStateLayout(ImVec2 rootAnchor, ImVec2 rootSize) {
  ImGui::SetNextWindowPos(rootAnchor);
  ImGui::SetNextWindowSize(rootSize);
  ImGui::Begin("Status", nullptr,
               ImGuiWindowFlags_NoTitleBar |
               ImGuiWindowFlags_NoResize |
               ImGuiWindowFlags_NoCollapse |
               ImGuiWindowFlags_NoBringToFrontOnFocus |
               ImGuiWindowFlags_NoMove);

  ImGui::BeginTable("Diagnostics", 4);
  ImGui::TableNextColumn();
  ImGui::Text("CPU %02u", clem_host_get_processor_number());
  ImGui::TableNextColumn();
  ImGui::Text("GUI");
  ImGui::TableNextColumn();
  ImGui::Text("%0.1f", ImGui::GetIO().Framerate);
  ImGui::TableNextColumn();
  ImGui::Text("fps");
  ImGui::TableNextRow();
  ImGui::TableNextColumn();
  ImGui::Text("CPU %02u", frameReadState_.backendCPUID);
  ImGui::TableNextColumn();
  ImGui::Text("EMU");
  ImGui::TableNextColumn();
  ImGui::Text("%0.1f", frameReadState_.fps);
  ImGui::TableNextColumn();
  ImGui::Text("fps");
  ImGui::EndTable();

  ImGui::Separator();


  ImGui::BeginTable("Machine", 3);
  // Registers
  ImGui::TableSetupColumn("Registers", ImGuiTableColumnFlags_WidthStretch);
  // Signals
  ImGui::TableSetupColumn("Pins", ImGuiTableColumnFlags_WidthStretch);
  // I/O
  ImGui::TableSetupColumn("I/O", ImGuiTableColumnFlags_WidthStretch);
  ImGui::TableHeadersRow();
  //  Registers Column
  ImGui::TableNextColumn();
  ImGui::BeginTable("Registers", 1);
  ImGui::TableNextColumn();
  ImGui::TextColored(CLEM_HOST_GUI_CPU_FIELD_COLOR(PBR),
    "PBR  =%02X  ", frameReadState_.cpu.regs.PBR);
  ImGui::TableNextRow();
  ImGui::TableNextColumn();
  ImGui::TextColored(CLEM_HOST_GUI_CPU_FIELD_COLOR(PC),
    "PC   =%04X", frameReadState_.cpu.regs.PC);
  ImGui::TableNextRow();
  ImGui::TableNextColumn();
  ImGui::TextColored(CLEM_HOST_GUI_CPU_FIELD_COLOR(IR),
    "IR   =%02X  ", frameReadState_.cpu.regs.IR);
  ImGui::TableNextRow();
  ImGui::TableNextColumn();
  ImGui::TextColored(CLEM_HOST_GUI_CPU_FIELD_COLOR(DBR),
    "DBR  =%02X  ", frameReadState_.cpu.regs.DBR);
  ImGui::TableNextRow();
  ImGui::TableNextColumn();
  ImGui::TextColored(CLEM_HOST_GUI_CPU_FIELD_COLOR(S),
    "S    =%04X", frameReadState_.cpu.regs.S);
  ImGui::TableNextRow();
  ImGui::TableNextColumn();
  ImGui::TextColored(CLEM_HOST_GUI_CPU_FIELD_COLOR(D),
    "D    =%04X", frameReadState_.cpu.regs.D);
  ImGui::TableNextRow();
  ImGui::TableNextColumn();
  ImGui::TextColored(CLEM_HOST_GUI_CPU_FIELD_COLOR(A),
    "A    =%04X  ", frameReadState_.cpu.regs.A);
  ImGui::TableNextRow();
  ImGui::TableNextColumn();
  ImGui::TextColored(CLEM_HOST_GUI_CPU_FIELD_COLOR(X),
    "X    =%04X", frameReadState_.cpu.regs.X);
  ImGui::TableNextRow();
  ImGui::TableNextColumn();
  ImGui::TextColored(CLEM_HOST_GUI_CPU_FIELD_COLOR(Y),
    "Y    =%04X", frameReadState_.cpu.regs.Y);
  ImGui::TableNextRow();
  ImGui::TableNextColumn();
  ImGui::TextColored(
    getStatusFieldColor<uint8_t>(
      lastFrameCPURegs_.P, frameReadState_.cpu.regs.P, kClemensCPUStatus_Negative),
    "n");
  ImGui::SameLine(0.0f, 4.0f);
  ImGui::TextColored(
    getStatusFieldColor<uint8_t>(
      lastFrameCPURegs_.P, frameReadState_.cpu.regs.P, kClemensCPUStatus_Overflow),
    "v");
  ImGui::SameLine(0.0f, 4.0f);
  ImGui::TextColored(
    getStatusFieldColor<uint8_t>(
      lastFrameCPURegs_.P, frameReadState_.cpu.regs.P, kClemensCPUStatus_MemoryAccumulator),
    frameReadState_.cpu.pins.emulation ? " " : "m");
  ImGui::SameLine(0.0f, 4.0f);
  ImGui::TextColored(
    getStatusFieldColor<uint8_t>(
      lastFrameCPURegs_.P, frameReadState_.cpu.regs.P, kClemensCPUStatus_Index),
    frameReadState_.cpu.pins.emulation ? " " : "x");
  ImGui::SameLine(0.0f, 4.0f);
  ImGui::TextColored(
    getStatusFieldColor<uint8_t>(
      lastFrameCPURegs_.P, frameReadState_.cpu.regs.P, kClemensCPUStatus_Decimal),
    "d");
  ImGui::SameLine(0.0f, 4.0f);
  ImGui::TextColored(
    getStatusFieldColor<uint8_t>(
      lastFrameCPURegs_.P, frameReadState_.cpu.regs.P, kClemensCPUStatus_IRQDisable),
    "i");
  ImGui::SameLine(0.0f, 4.0f);
  ImGui::TextColored(
    getStatusFieldColor<uint8_t>(
      lastFrameCPURegs_.P, frameReadState_.cpu.regs.P, kClemensCPUStatus_Zero),
    "z");
  ImGui::SameLine(0.0f, 4.0f);
  ImGui::TextColored(
    getStatusFieldColor<uint8_t>(
      lastFrameCPURegs_.P, frameReadState_.cpu.regs.P, kClemensCPUStatus_Carry),
    "c");
  ImGui::EndTable();

  ImGui::TableNextColumn();
  ImGui::BeginTable("Pins", 1);
  ImGui::TableNextColumn();
  ImGui::TextColored(CLEM_HOST_GUI_CPU_PINS_COLOR(readyOut), "RDY");
  ImGui::TableNextRow();
  ImGui::TableNextColumn();
  ImGui::TextColored(CLEM_HOST_GUI_CPU_PINS_COLOR(resbIn), "RESB");
  ImGui::TableNextRow();
  ImGui::TableNextColumn();
  ImGui::TextColored(CLEM_HOST_GUI_CPU_PINS_COLOR(emulation), "E");
  ImGui::TableNextRow();
  ImGui::TableNextColumn();
  ImGui::TextColored(CLEM_HOST_GUI_CPU_PINS_INV_COLOR(irqbIn), "IRQ");
  ImGui::TableNextRow();
  ImGui::TableNextColumn();
  ImGui::TextColored(CLEM_HOST_GUI_CPU_PINS_INV_COLOR(nmibIn), "NMI");
  ImGui::EndTable();

  ImGui::TableNextColumn();
  ImGui::BeginTable("Mega2", 1);
  ImGui::EndTable();

  ImGui::EndTable();

  ImGui::Separator();

  ImGui::End();
}

void ClemensFrontend::doMachineViewLayout(ImVec2 rootAnchor, ImVec2 rootSize,
                                          float screenU, float screenV) {
  ImGui::SetNextWindowPos(rootAnchor);
  ImGui::SetNextWindowSize(rootSize);
  ImGui::Begin("Display", nullptr,
                ImGuiWindowFlags_NoTitleBar |
                ImGuiWindowFlags_NoResize |
                ImGuiWindowFlags_NoCollapse |
                ImGuiWindowFlags_NoBringToFrontOnFocus |
                ImGuiWindowFlags_NoMove);
  ImVec4 tint_col = ImVec4(1.0f, 1.0f, 1.0f, 1.0f); // No tint
  ImTextureID texId{(void *)((uintptr_t)display_.getScreenTarget().id)};
  ImVec2 p = ImGui::GetCursorScreenPos();
  ImVec2 contentSize = ImGui::GetContentRegionAvail();
  ImVec2 monitorSize(contentSize.y * 1.5f, contentSize.y);
  ImVec2 monitorAnchor(p.x + (contentSize.x - monitorSize.x) * 0.5f, p.y);
  ImVec2 display_uv(screenU, screenV);
  ImGui::GetWindowDrawList()->AddImage(
      texId, ImVec2(monitorAnchor.x, monitorAnchor.y),
      ImVec2(monitorAnchor.x + monitorSize.x, monitorAnchor.y + monitorSize.y),
      ImVec2(0, 0), display_uv, ImGui::GetColorU32(tint_col));

  if (ImGui::IsWindowFocused()) {
    ImGui::SetKeyboardFocusHere(0);
    emulatorHasKeyboardFocus_ = true;
  } else {
    emulatorHasKeyboardFocus_ = false;
  }
  ImGui::End();
}

void ClemensFrontend::doMachineTerminalLayout(ImVec2 rootAnchor, ImVec2 rootSize) {
  char inputLine[128] = "";
  ImGui::SetNextWindowPos(rootAnchor);
  ImGui::SetNextWindowSize(rootSize);
  ImGui::Begin("Terminal", nullptr,
                ImGuiWindowFlags_NoTitleBar |
                ImGuiWindowFlags_NoResize |
                ImGuiWindowFlags_NoCollapse |
                ImGuiWindowFlags_NoMove);
  ImVec2 contentSize = ImGui::GetContentRegionAvail();
  contentSize.y -= (ImGui::GetFrameHeightWithSpacing() + ImGui::GetStyle().ItemSpacing.y);
  ImGui::BeginChild("TerminalView", contentSize);
  {
    if (terminalMode_ == TerminalMode::Command) {
      layoutTerminalLines();
    } else if (terminalMode_ == TerminalMode::Log) {
      layoutConsoleLines();
    }
  }
  ImGui::EndChild();
  ImGui::Separator();
  ImGui::PushStyleColor(ImGuiCol_FrameBg,
                          ImGui::GetStyleColorVec4(ImGuiCol_WindowBg));
  ImGui::Text("*");
  ImGui::SameLine();
  auto& style = ImGui::GetStyle();
  float xpos = ImGui::GetCursorPosX();
  float rightPaddingX = 3 * (ImGui::GetFont()->GetCharAdvance('A') +
                             style.FramePadding.x*2 + style.ItemSpacing.x);
  ImGui::SetNextItemWidth(rootSize.x - xpos - ImGui::GetStyle().WindowPadding.x - rightPaddingX);
  if (ImGui::InputText("", inputLine, sizeof(inputLine), ImGuiInputTextFlags_EnterReturnsTrue)) {
    auto* inputLineEnd = &inputLine[0] + strnlen(inputLine, sizeof(inputLine));
    if (inputLineEnd != &inputLine[0]) {
      const char* inputStart = &inputLine[0];
      const char* inputEnd = inputLineEnd - 1;
      for (; std::isspace(*inputStart) && inputStart < inputEnd; ++inputStart);
      for (; std::isspace(*inputEnd) && inputEnd > inputStart; --inputEnd);
      if (inputStart <= inputEnd) {
        std::string_view commandLine(inputStart, (inputEnd - inputStart) + 1);
        executeCommand(commandLine);
      }
    }
    ImGui::SetKeyboardFocusHere(-1);
  }
  ImGui::PopStyleColor();
  ImGui::SameLine();
  auto activeButtonColor = style.Colors[ImGuiCol_ButtonActive];
  auto buttonColor = style.Colors[ImGuiCol_Button];
  ImGui::PushStyleColor(ImGuiCol_Button,
    terminalMode_==TerminalMode::Command ? activeButtonColor : buttonColor);
  if (ImGui::Button("C")) {
    terminalMode_ = TerminalMode::Command;
  }
  ImGui::PopStyleColor();
  ImGui::SameLine();
  ImGui::PushStyleColor(ImGuiCol_Button,
    terminalMode_==TerminalMode::Log ? activeButtonColor : buttonColor);
  if (ImGui::Button("L")) {
    terminalMode_ = TerminalMode::Log;
  }
  ImGui::PopStyleColor();
  ImGui::SameLine();
  ImGui::PushStyleColor(ImGuiCol_Button,
    terminalMode_==TerminalMode::Execution ? activeButtonColor : buttonColor);
  if (ImGui::Button("E")) {
      terminalMode_ = TerminalMode::Execution;
  }
  ImGui::PopStyleColor();
  ImGui::End();
}

void ClemensFrontend::layoutTerminalLines() {
  for (size_t index = 0; index < terminalLines_.size(); ++index) {
    auto& line = terminalLines_.at(index);
    switch (line.type) {
      case TerminalLine::Debug:
        ImGui::PushStyleColor(ImGuiCol_Text, (ImU32)ImColor(192, 192, 192, 255));
        break;
      case TerminalLine::Warn:
        ImGui::PushStyleColor(ImGuiCol_Text, (ImU32)ImColor(255, 255, 0, 255));
        break;
      case TerminalLine::Error:
        ImGui::PushStyleColor(ImGuiCol_Text, (ImU32)ImColor(255, 0, 192, 255));
        break;
      case TerminalLine::Command:
        ImGui::PushStyleColor(ImGuiCol_Text, (ImU32)ImColor(0, 255, 255, 255));
        break;
    }
    ImGui::TextUnformatted(terminalLines_.at(index).text.c_str());
    if (line.type != TerminalLine::Info) {
      ImGui::PopStyleColor();
    }
    ImGui::SetScrollHereY();
  }
}

void ClemensFrontend::layoutConsoleLines() {
  for (size_t index = 0; index < consoleLines_.size(); ++index) {
    auto& line = consoleLines_.at(index);
    switch (line.type) {
      case TerminalLine::Debug:
        ImGui::PushStyleColor(ImGuiCol_Text, (ImU32)ImColor(192, 192, 192, 255));
        break;
      case TerminalLine::Warn:
        ImGui::PushStyleColor(ImGuiCol_Text, (ImU32)ImColor(255, 255, 0, 255));
        break;
      case TerminalLine::Error:
        ImGui::PushStyleColor(ImGuiCol_Text, (ImU32)ImColor(255, 0, 192, 255));
        break;
      case TerminalLine::Command:
        ImGui::PushStyleColor(ImGuiCol_Text, (ImU32)ImColor(0, 255, 255, 255));
        break;
    }
    ImGui::TextUnformatted(consoleLines_.at(index).text.c_str());
    if (line.type != TerminalLine::Info) {
      ImGui::PopStyleColor();
    }
    ImGui::SetScrollHereY();
  }
}

void ClemensFrontend::input(const ClemensInputEvent &input){
  if (emulatorHasKeyboardFocus_) {
    backend_->inputEvent(input);
  }
}

void ClemensFrontend::backendStateDelegate(const ClemensBackendState& state) {
  {
    std::lock_guard<std::mutex> frameLock(frameMutex_);

    frameSeqNo_ = state.seqno;

    frameWriteState_.cpu = state.machine->cpu;
    frameWriteState_.monitorFrame = state.monitor;
    frameWriteState_.textFrame = state.text;
    frameWriteState_.graphicsFrame = state.graphics;
    frameWriteState_.audioFrame = state.audio;
    frameWriteState_.backendCPUID = state.hostCPUID;
    frameWriteState_.fps = state.fps;
    frameWriteState_.mmioWasInitialized = state.mmio_was_initialized;
    frameWriteState_.commandFailed = state.commandFailed;

    //  copy over component state as needed
    frameWriteState_.vgcModeFlags = state.machine->mmio.vgc.mode_flags;
    frameWriteState_.irqs = state.machine->mmio.irq_line;
    frameWriteState_.nmis = state.machine->mmio.nmi_line;

    //  copy over memory banks as needed
    frameWriteMemory_.reset();
    frameWriteState_.bankE0 = (uint8_t*)frameWriteMemory_.allocate(CLEM_IIGS_BANK_SIZE);
    memcpy(frameWriteState_.bankE0, state.machine->mega2_bank_map[0], CLEM_IIGS_BANK_SIZE);
    frameWriteState_.bankE1 = (uint8_t*)frameWriteMemory_.allocate(CLEM_IIGS_BANK_SIZE);
    memcpy(frameWriteState_.bankE1, state.machine->mega2_bank_map[1], CLEM_IIGS_BANK_SIZE);

    auto audioBufferSize = int32_t(state.audio.frame_total * state.audio.frame_stride);
    frameWriteState_.audioBuffer = (uint8_t*)frameWriteMemory_.allocate(audioBufferSize);
    memcpy(frameWriteState_.audioBuffer, state.audio.data, audioBufferSize);

    frameWriteState_.logNode = nullptr;
    LogOutputNode* prev = nullptr;
    for (auto* logItem = state.logBufferStart; logItem != state.logBufferEnd; ++logItem) {
      LogOutputNode* logMemory = reinterpret_cast<LogOutputNode*>(frameWriteMemory_.allocate(
        sizeof(LogOutputNode) + CK_ALIGN_SIZE_TO_ARCH(logItem->text.size())));
      logMemory->logLevel = logItem->level;
      logMemory->sz = unsigned(logItem->text.size());
      logItem->text.copy(reinterpret_cast<char*>(logMemory) + sizeof(LogOutputNode),
                         std::string::npos);
      logMemory->next = nullptr;
      if (!frameWriteState_.logNode) frameWriteState_.logNode = logMemory;
      else prev->next = logMemory;
      prev = logMemory;
    }

    frameWriteState_.breakpoints =
      frameWriteMemory_.allocateArray<ClemensBackendBreakpoint>(
        state.bpBufferEnd - state.bpBufferStart);
    frameWriteState_.breakpointCount = (unsigned)(state.bpBufferEnd - state.bpBufferStart);
    frameWriteState_.hitBreakpoint = nullptr;
    auto* bpDest = frameWriteState_.breakpoints;
    for (auto* bpCur = state.bpBufferStart;
         bpCur != state.bpBufferEnd; ++bpCur, ++bpDest) {
      *bpDest = *bpCur;
      if (bpCur == state.bpHit) {
        frameWriteState_.hitBreakpoint = bpDest;
      }
    }
  }
  framePublished_.notify_one();
}

void ClemensFrontend::executeCommand(std::string_view command) {
  CLEM_TERM_COUT.format(TerminalLine::Command, "* {}", command);
  auto sep = command.find(' ');
  auto action = command.substr(0, sep);
  auto operand = sep != std::string_view::npos ? command.substr(sep + 1)
                                               : std::string_view();
  if (action == "help" || action == "?") {
    cmdHelp(operand);
  } else if (action == "run" || action == "r") {
    cmdRun(operand);
  } else if (action == "break" || action == "b") {
    cmdBreak(operand);
  } else if (action == "list" || action == "l") {
    cmdList(operand);
  } else if (!action.empty()) {
    CLEM_TERM_COUT.print(TerminalLine::Error, "Unrecognized command!");
  }
}

void ClemensFrontend::cmdHelp(std::string_view operand) {
  CLEM_TERM_COUT.print(TerminalLine::Info, "reset                       - soft reset of the machine");
  CLEM_TERM_COUT.print(TerminalLine::Info, "reboot                      - hard reset of the machine");
  CLEM_TERM_COUT.print(TerminalLine::Info, "disk                        - disk information");
  CLEM_TERM_COUT.print(TerminalLine::Info, "disk <s5|6>,<d1|2>,<image>  - insert disk");
  CLEM_TERM_COUT.print(TerminalLine::Info, "disk <s5|6>,<d1|2>          - disk at slot s, drive d");
  CLEM_TERM_COUT.print(TerminalLine::Info, "eject <s5|6>,<d1|2>         - eject disk");
  CLEM_TERM_COUT.print(TerminalLine::Info, "r]un                        - execute emulator until break");
  CLEM_TERM_COUT.print(TerminalLine::Info, "b]reak                      - break execution at current PC");
  CLEM_TERM_COUT.print(TerminalLine::Info, "b]reak <address>            - break execution at address");
  CLEM_TERM_COUT.print(TerminalLine::Info, "b]reak r:<address>          - break on data read from address");
  CLEM_TERM_COUT.print(TerminalLine::Info, "b]reak w:<address>          - break on write to address");
  CLEM_TERM_COUT.print(TerminalLine::Info, "l]ist                       - list all breakpoints");
  CLEM_TERM_COUT.newline();
}

void ClemensFrontend::cmdBreak(std::string_view operand) {
  //  parse [r|w]<address>
  ClemensBackendBreakpoint breakpoint {ClemensBackendBreakpoint::Undefined};
  auto sepPos = operand.find(':');
  if (sepPos != std::string_view::npos) {
    auto typeStr = operand.substr(0, sepPos);
    if (typeStr.size() == 1) {
      if (typeStr[0] == 'r') {
        breakpoint.type = ClemensBackendBreakpoint::DataRead;
      } else if (typeStr[0] == 'w') {
        breakpoint.type = ClemensBackendBreakpoint::Write;
      }
    }
    if (breakpoint.type == ClemensBackendBreakpoint::Undefined) {
      CLEM_TERM_COUT.format(TerminalLine::Error,
                            "Breakpoint type {} is invalid.", typeStr);
      return;
    }
    operand = operand.substr(sepPos + 1);
    if (operand.empty()) {
      CLEM_TERM_COUT.format(TerminalLine::Error,
                            "Breakpoint type {} is invalid.", typeStr);
      return;
    }
  } else {
    breakpoint.type = ClemensBackendBreakpoint::Execute;
  }

  char address[16];
  auto bankSepPos = operand.find('//');
  if (bankSepPos == std::string_view::npos) {
    if (operand.size() >= 2) {
      snprintf(address, sizeof(address), "%02X", frameReadState_.cpu.regs.PBR);
      operand.copy(address + 2, 4, 2);
    } else if (!operand.empty()) {
      CLEM_TERM_COUT.format(TerminalLine::Error, "Address {} is invalid.", operand);
      return;
    }
  } else if (bankSepPos == 2 && operand.size() > bankSepPos) {
    operand.copy(address, bankSepPos, 0);
    operand.copy(address + bankSepPos, operand.size() - (bankSepPos + 1), bankSepPos + 1);
  } else {
    CLEM_TERM_COUT.format(TerminalLine::Error, "Address {} is invalid.", operand);
    return;
  }
  if (operand.empty()) {
    backend_->breakExecution();
  } else {
    address[6] = '\0';
    char* addressEnd = NULL;
    breakpoint.address = std::strtoul(address, &addressEnd, 16);
    if (addressEnd != address + 6) {
      CLEM_TERM_COUT.format(TerminalLine::Error,
                            "Address format is invalid read from '{}'", operand);
      return;
    }
    backend_->addBreakpoint(breakpoint);
  }
}

void ClemensFrontend::cmdList(std::string_view operand) {
  //  TODO: granular listing based on operand
  if (breakpoints_.empty()) {
    CLEM_TERM_COUT.print(TerminalLine::Info, "No breakpoints defined.");
    return;
  }
  for (size_t i = 0; i < breakpoints_.size(); ++i) {
    auto& bp = breakpoints_[i];
    CLEM_TERM_COUT.format(TerminalLine::Info, "bp #{}: {:02X}/{:04X}", i,
                          (bp.address >> 16) & 0xff, bp.address & 0xffff);
  }
}

void ClemensFrontend::cmdRun(std::string_view operand) {
  backend_->run();
}
