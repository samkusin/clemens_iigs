#include "clem_front.hpp"
#include "clem_backend.hpp"
#include "clem_disk_utils.hpp"
#include "clem_host_platform.h"
#include "clem_import_disk.hpp"

#include "cinek/encode.h"
#include "fmt/format.h"
#include "imgui_filedialog/ImGuiFileDialog.h"

#include <charconv>
#include <filesystem>

//  TODO: implement all current commands documented in help
//  TODO: blank disk gui selection for disk set (selecting combo create will
//        enable another input widget, unselecting will gray out that widget.)
//  TODO: memory debug gui
//  TODO: memory dump command (non-gui)
//  TODO: instruction trace (improved)
//

template <typename TBufferType> struct FormatView {
  using BufferType = TBufferType;
  using StringType = typename BufferType::ValueType;
  using LevelType = typename StringType::Type;

  BufferType &buffer_;
  FormatView(BufferType &buffer) : buffer_(buffer) {}
  template <typename... Args>
  void format(LevelType type, const char *formatStr, const Args &...args) {
    size_t sz = fmt::formatted_size(formatStr, args...);
    auto &line = rotateBuffer();
    line.type = type;
    line.text.resize(sz);
    fmt::format_to_n(line.text.data(), sz, formatStr, args...);
  }
  void print(LevelType type, const char *text) {
    auto &line = rotateBuffer();
    line.type = type;
    line.text.assign(text);
  }
  void print(LevelType type, std::string_view text) {
    auto &line = rotateBuffer();
    line.type = type;
    line.text.assign(text);
  }
  void newline() {
    auto &line = rotateBuffer();
    line.type = LevelType::Info;
    line.text.clear();
  }

private:
  StringType &rotateBuffer() {
    if (buffer_.isFull()) {
      buffer_.pop();
    }
    StringType &tail = *buffer_.acquireTail();
    buffer_.push();
    return tail;
  }
};

#define CLEM_TERM_COUT FormatView<decltype(ClemensFrontend::terminalLines_)>(terminalLines_)

static constexpr size_t kFrameMemorySize = 8 * 1024 * 1024;
static constexpr size_t kLogMemorySize = 256 * 1024;

static std::string getCommandTypeName(ClemensBackendCommand::Type type) {
  switch (type) {
  case ClemensBackendCommand::AddBreakpoint:
    return "AddBreakpoint";
  case ClemensBackendCommand::Break:
    return "Break";
  case ClemensBackendCommand::DelBreakpoint:
    return "DelBreakpoint";
  case ClemensBackendCommand::EjectDisk:
    return "EjectDisk";
  case ClemensBackendCommand::InsertDisk:
    return "InsertDisk";
  case ClemensBackendCommand::ResetMachine:
    return "ResetMachine";
  case ClemensBackendCommand::RunMachine:
    return "RunMachine";
  case ClemensBackendCommand::SetHostUpdateFrequency:
    return "SetHostUpdateFrequency";
  case ClemensBackendCommand::Terminate:
    return "Terminate";
  default:
    return "Command";
  }
}

ClemensFrontend::ClemensFrontend(const cinek::ByteBuffer &systemFontLoBuffer,
                                 const cinek::ByteBuffer &systemFontHiBuffer)
    : displayProvider_(systemFontLoBuffer, systemFontHiBuffer), display_(displayProvider_),
      audio_(), frameWriteMemory_(kFrameMemorySize, malloc(kFrameMemorySize)),
      frameReadMemory_(kFrameMemorySize, malloc(kFrameMemorySize)),
      logMemory_(kLogMemorySize, malloc(kLogMemorySize)),
      frameSeqNo_(std::numeric_limits<decltype(frameSeqNo_)>::max()),
      frameLastSeqNo_(std::numeric_limits<decltype(frameLastSeqNo_)>::max()), lastFrameCPUPins_{},
      lastFrameCPURegs_{}, lastFrameIRQs_(0), lastFrameNMIs_(0), emulatorHasKeyboardFocus_(true),
      terminalMode_(TerminalMode::Command), guiMode_(GUIMode::Emulator),
      diskLibraryRootPath_{
          (std::filesystem::current_path() / std::filesystem::path(CLEM_HOST_LIBRARY_DIR))
              .string()},
      diskLibrary_(diskLibraryRootPath_, CLEM_DISK_TYPE_NONE, 256, 512), diskComboStateFlags_(0) {
  lastCommandState_.logNode = lastCommandState_.logNodeTail = nullptr;

  audio_.start();
  config_.type = ClemensBackend::Config::Type::Apple2GS;
  config_.audioSamplesPerSecond = audio_.getAudioFrequency();
  createBackend();

  CLEM_TERM_COUT.print(TerminalLine::Info, "Welcome to the Clemens IIgs Emulator");
}

ClemensFrontend::~ClemensFrontend() {
  backend_ = nullptr;
  audio_.stop();
  free(frameWriteMemory_.getHead());
  free(frameReadMemory_.getHead());
}

void ClemensFrontend::input(const ClemensInputEvent &input) {
  if (emulatorHasKeyboardFocus_) {
    backend_->inputEvent(input);
  }
}

void ClemensFrontend::createBackend() {
  backend_ = std::make_unique<ClemensBackend>(
      "gs_rom_3.rom", config_,
      std::bind(&ClemensFrontend::backendStateDelegate, this, std::placeholders::_1));
  backend_->setRefreshFrequency(60);
  backend_->reset();
  backend_->run();
  guiMode_ = GUIMode::Emulator;
}

void ClemensFrontend::backendStateDelegate(const ClemensBackendState &state) {
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

    //  copy over component state as needed
    frameWriteState_.vgcModeFlags = state.machine->mmio.vgc.mode_flags;
    frameWriteState_.irqs = state.machine->mmio.irq_line;
    frameWriteState_.nmis = state.machine->mmio.nmi_line;

    //  copy over memory banks as needed
    frameWriteMemory_.reset();
    frameWriteState_.bankE0 = (uint8_t *)frameWriteMemory_.allocate(CLEM_IIGS_BANK_SIZE);
    memcpy(frameWriteState_.bankE0, state.machine->mega2_bank_map[0], CLEM_IIGS_BANK_SIZE);
    frameWriteState_.bankE1 = (uint8_t *)frameWriteMemory_.allocate(CLEM_IIGS_BANK_SIZE);
    memcpy(frameWriteState_.bankE1, state.machine->mega2_bank_map[1], CLEM_IIGS_BANK_SIZE);

    auto audioBufferSize = int32_t(state.audio.frame_total * state.audio.frame_stride);
    frameWriteState_.audioBuffer = (uint8_t *)frameWriteMemory_.allocate(audioBufferSize);
    memcpy(frameWriteState_.audioBuffer, state.audio.data, audioBufferSize);

    const ClemensBackendDiskDriveState *driveState = state.diskDrives;
    for (auto &diskDrive : frameWriteState_.diskDrives) {
      diskDrive = *driveState;
      if (state.terminated.has_value() && *state.terminated) {
        config_.diskDriveStates[size_t(driveState - state.diskDrives)] = *driveState;
      }
      ++driveState;
    }

    frameWriteState_.breakpoints = frameWriteMemory_.allocateArray<ClemensBackendBreakpoint>(
        state.bpBufferEnd - state.bpBufferStart);
    frameWriteState_.breakpointCount = (unsigned)(state.bpBufferEnd - state.bpBufferStart);
    auto *bpDest = frameWriteState_.breakpoints;
    for (auto *bpCur = state.bpBufferStart; bpCur != state.bpBufferEnd; ++bpCur, ++bpDest) {
      *bpDest = *bpCur;
      if (state.bpHitIndex.has_value() && !lastCommandState_.hitBreakpoint.has_value()) {
        if (unsigned(bpCur - state.bpBufferStart) == *state.bpHitIndex) {
          lastCommandState_.hitBreakpoint = *state.bpHitIndex;
        }
      }
    }
    if (!lastCommandState_.commandFailed.has_value()) {
      lastCommandState_.commandFailed = state.commandFailed;
    }
    if (!lastCommandState_.commandType.has_value()) {
      lastCommandState_.commandType = state.commandType;
    }
    if (!lastCommandState_.terminated.has_value()) {
      lastCommandState_.terminated = state.terminated;
    }

    for (auto *logItem = state.logBufferStart; logItem != state.logBufferEnd; ++logItem) {
      LogOutputNode *logMemory = reinterpret_cast<LogOutputNode *>(
          logMemory_.allocate(sizeof(LogOutputNode) + CK_ALIGN_SIZE_TO_ARCH(logItem->text.size())));
      logMemory->logLevel = logItem->level;
      logMemory->sz = unsigned(logItem->text.size());
      logItem->text.copy(reinterpret_cast<char *>(logMemory) + sizeof(LogOutputNode),
                         std::string::npos);
      logMemory->next = nullptr;
      if (!lastCommandState_.logNode) {
        lastCommandState_.logNode = logMemory;
      } else {
        lastCommandState_.logNodeTail->next = logMemory;
      }
      lastCommandState_.logNodeTail = logMemory;
    }
  }
  framePublished_.notify_one();
}

void ClemensFrontend::frame(int width, int height, float deltaTime) {
  //  send commands to emulator thread
  //  get results from emulator thread
  //    video, audio, machine state, etc
  //
  //  developer_layout(width, height, deltaTime);
  bool isNewFrame = false;
  bool isBackendTerminated = false;
  std::unique_lock<std::mutex> frameLock(frameMutex_);
  framePublished_.wait_for(frameLock, std::chrono::milliseconds::zero(),
                           [this]() { return frameSeqNo_ != frameLastSeqNo_; });
  isNewFrame = frameLastSeqNo_ != frameSeqNo_;
  if (isNewFrame) {
    lastFrameCPURegs_ = frameReadState_.cpu.regs;
    lastFrameCPUPins_ = frameReadState_.cpu.pins;
    lastFrameIRQs_ = frameReadState_.irqs;
    lastFrameNMIs_ = frameReadState_.nmis;

    std::swap(frameWriteMemory_, frameReadMemory_);
    std::swap(frameWriteState_, frameReadState_);

    LogOutputNode *logNode = lastCommandState_.logNode;
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
    lastCommandState_.logNode = lastCommandState_.logNodeTail = nullptr;
    logMemory_.reset();

    breakpoints_.clear();
    for (unsigned bpIndex = 0; bpIndex < frameReadState_.breakpointCount; ++bpIndex) {
      breakpoints_.emplace_back(frameReadState_.breakpoints[bpIndex]);
    }
    if (lastCommandState_.commandFailed.has_value()) {
      if (*lastCommandState_.commandFailed) {
        CLEM_TERM_COUT.format(TerminalLine::Error, "{} Failed.",
                              getCommandTypeName(*lastCommandState_.commandType));
      } else {
        CLEM_TERM_COUT.print(TerminalLine::Info, "Ok.");
      }
      lastCommandState_.commandFailed = std::nullopt;
      lastCommandState_.commandType = std::nullopt;
    }
    if (lastCommandState_.hitBreakpoint.has_value()) {
      unsigned bpIndex = *lastCommandState_.hitBreakpoint;
      CLEM_TERM_COUT.format(TerminalLine::Info, "Breakpoint {} hit {:02X}/{:04X}.", bpIndex,
                            (breakpoints_[bpIndex].address >> 16) & 0xff,
                            breakpoints_[bpIndex].address & 0xffff);
      lastCommandState_.hitBreakpoint = std::nullopt;
    }
    if (lastCommandState_.terminated.has_value()) {
      isBackendTerminated = *lastCommandState_.terminated;
      lastCommandState_.terminated = std::nullopt;
    }
    frameLastSeqNo_ = frameSeqNo_;
  }
  frameLock.unlock();

  //  render video
  constexpr int kClemensScreenWidth = 720;
  constexpr int kClemensScreenHeight = 480;
  float screenUVs[2]{0.0f, 0.0f};

  if (frameReadState_.mmioWasInitialized) {
    const uint8_t *e0mem = frameReadState_.bankE0;
    const uint8_t *e1mem = frameReadState_.bankE1;
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
    auto &audioFrame = frameReadState_.audioFrame;
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

  switch (guiMode_) {
  case GUIMode::ImportDiskModal:
  case GUIMode::BlankDiskModal:
    doModalOperations(width, height);
    break;
  case GUIMode::ImportDiskSetFlow:
    doImportDiskSetFlowStart(width, height);
    break;
  case GUIMode::ImportDiskSetReplaceOld:
    doImportDiskSetReplaceOld(width, height);
    break;
  case GUIMode::ImportDiskSet:
    doImportDiskSet(width, height);
    break;
  case GUIMode::NewBlankDisk:
    doNewBlankDisk(width, height);
    break;
  case GUIMode::RebootEmulator:
    if (isBackendTerminated) {
      backend_ = nullptr;
      createBackend();
    }
    break;
  default:
    break;
  }

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

template <typename T> static ImColor getStatusFieldColor(T a, T b, T statusMask) {
  return (a & statusMask) != (b & statusMask) ? getModifiedColor(b & statusMask)
                                              : getDefaultColor(b & statusMask);
}

void ClemensFrontend::doMachineStateLayout(ImVec2 rootAnchor, ImVec2 rootSize) {
  ImGui::SetNextWindowPos(rootAnchor);
  ImGui::SetNextWindowSize(rootSize);
  ImGui::Begin("Status", nullptr,
               ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize |
                   ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoBringToFrontOnFocus |
                   ImGuiWindowFlags_NoMove);
  doMachineDiagnosticsDisplay();
  ImGui::Separator();
  doMachineDiskDisplay();
  ImGui::Separator();
  doMachineCPUInfoDisplay();
  ImGui::Separator();

  ImGui::End();
}

void ClemensFrontend::doMachineDiagnosticsDisplay() {
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
}

void ClemensFrontend::doMachineDiskDisplay() {
  ImGui::BeginTable("DiskSelect", 2);
  ImGui::TableSetupColumn(
      "WP", ImGuiTableColumnFlags_WidthFixed,
      (ImGui::GetStyle().FramePadding.x + ImGui::GetFont()->GetCharAdvance('W')) * 2);
  ImGui::TableSetupColumn("Image", ImGuiTableColumnFlags_WidthStretch);
  ImGui::TableHeadersRow();
  ImGui::TableNextColumn();
  doMachineDiskStatus(kClemensDrive_3_5_D1);
  ImGui::TableNextColumn();
  doMachineDiskSelection(kClemensDrive_3_5_D1);
  ImGui::TableNextRow();
  ImGui::TableNextColumn();
  doMachineDiskStatus(kClemensDrive_3_5_D2);
  ImGui::TableNextColumn();
  doMachineDiskSelection(kClemensDrive_3_5_D2);
  ImGui::TableNextRow();
  ImGui::TableNextColumn();
  doMachineDiskStatus(kClemensDrive_5_25_D1);
  ImGui::TableNextColumn();
  doMachineDiskSelection(kClemensDrive_5_25_D1);
  ImGui::TableNextRow();
  ImGui::TableNextColumn();
  doMachineDiskStatus(kClemensDrive_5_25_D2);
  ImGui::TableNextColumn();
  doMachineDiskSelection(kClemensDrive_5_25_D2);
  ImGui::EndTable();
}

static const char *sDriveName[] = {"Slot5 D1", "Slot5 D2", "Slot6 D1", "Slot6 D2"};
static const char *sDriveDescription[] = {"3.5 inch 800K", "3.5 inch 800K", "5.25 inch 140K",
                                          "5.25 inch 140K"};

void ClemensFrontend::doMachineDiskSelection(ClemensDriveType driveType) {
  const ClemensBackendDiskDriveState &drive = frameReadState_.diskDrives[driveType];
  //  2 states: empty, has disk
  //    options if empty: <blank disk>, <import image>, image 0, image 1, ...
  //    options if full: <eject>
  const char *imageName;
  if (drive.isEjecting) {
    imageName = "Ejecting...";
  } else if (drive.imagePath.empty()) {
    imageName = "- Empty";
  } else {
    imageName = drive.imagePath.c_str();
  }
  if (ImGui::BeginCombo(sDriveName[driveType], imageName, ImGuiComboFlags_NoArrowButton)) {
    if (!(diskComboStateFlags_ & (1 << driveType))) {
      if (driveType == kClemensDrive_3_5_D1 || driveType == kClemensDrive_3_5_D2) {
        diskLibrary_.reset(diskLibraryRootPath_, CLEM_DISK_TYPE_3_5);
      } else if (driveType == kClemensDrive_5_25_D1 || driveType == kClemensDrive_5_25_D2) {
        diskLibrary_.reset(diskLibraryRootPath_, CLEM_DISK_TYPE_5_25);
      } else {
        diskLibrary_.reset(diskLibraryRootPath_, CLEM_DISK_TYPE_NONE);
      }
      diskComboStateFlags_ |= (1 << driveType);
    } else {
      diskLibrary_.update();
    }
    if (!drive.imagePath.empty() && !drive.isEjecting && ImGui::Selectable("<eject>")) {
      backend_->ejectDisk(driveType);
    }
    if (drive.imagePath.empty()) {
      if (ImGui::Selectable("<insert blank disk>")) {
        guiMode_ = GUIMode::BlankDiskModal;
        importDriveType_ = driveType;
      }
      if (ImGui::Selectable("<import master>")) {
        guiMode_ = GUIMode::ImportDiskModal;
        importDriveType_ = driveType;
      }
      ImGui::Separator();
      std::filesystem::path selectedPath;
      diskLibrary_.iterate([&selectedPath](const ClemensDiskLibrary::DiskEntry &entry) {
        auto relativePath = entry.location.parent_path().filename() / entry.location.stem();
        if (ImGui::Selectable(relativePath.string().c_str())) {
          selectedPath = entry.location.string();
        }
      });
      if (!selectedPath.empty()) {
        backend_->insertDisk(driveType, selectedPath.string());
      }
      ImGui::Separator();
    }
    ImGui::EndCombo();
  } else {
    if (diskComboStateFlags_ & (1 << driveType)) {
      diskComboStateFlags_ &= ~(1 << driveType);
    }
  }
}

void ClemensFrontend::doMachineDiskStatus(ClemensDriveType driveType) {
  const ClemensBackendDiskDriveState &drive = frameReadState_.diskDrives[driveType];
  if (drive.imagePath.empty())
    return;

  bool wp = drive.isWriteProtected;
  if (ImGui::Checkbox("", &wp)) {
    backend_->writeProtectDisk(driveType, wp);
  }
}

#define CLEM_HOST_GUI_CPU_PINS_COLOR(_field_)                                                      \
  lastFrameCPUPins_._field_ != frameReadState_.cpu.pins._field_                                    \
      ? getModifiedColor(frameReadState_.cpu.pins._field_)                                         \
      : getDefaultColor(frameReadState_.cpu.pins._field_)

#define CLEM_HOST_GUI_CPU_PINS_INV_COLOR(_field_)                                                  \
  lastFrameCPUPins_._field_ != frameReadState_.cpu.pins._field_                                    \
      ? getModifiedColor(!frameReadState_.cpu.pins._field_)                                        \
      : getDefaultColor(!frameReadState_.cpu.pins._field_)

#define CLEM_HOST_GUI_CPU_FIELD_COLOR(_field_)                                                     \
  lastFrameCPURegs_._field_ != frameReadState_.cpu.regs._field_ ? getModifiedColor(true)           \
                                                                : getDefaultColor(true)

void ClemensFrontend::doMachineCPUInfoDisplay() {
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
  ImGui::TextColored(CLEM_HOST_GUI_CPU_FIELD_COLOR(PBR), "PBR  =%02X  ",
                     frameReadState_.cpu.regs.PBR);
  ImGui::TableNextRow();
  ImGui::TableNextColumn();
  ImGui::TextColored(CLEM_HOST_GUI_CPU_FIELD_COLOR(PC), "PC   =%04X", frameReadState_.cpu.regs.PC);
  ImGui::TableNextRow();
  ImGui::TableNextColumn();
  ImGui::TextColored(CLEM_HOST_GUI_CPU_FIELD_COLOR(IR), "IR   =%02X  ",
                     frameReadState_.cpu.regs.IR);
  ImGui::TableNextRow();
  ImGui::TableNextColumn();
  ImGui::TextColored(CLEM_HOST_GUI_CPU_FIELD_COLOR(DBR), "DBR  =%02X  ",
                     frameReadState_.cpu.regs.DBR);
  ImGui::TableNextRow();
  ImGui::TableNextColumn();
  ImGui::TextColored(CLEM_HOST_GUI_CPU_FIELD_COLOR(S), "S    =%04X", frameReadState_.cpu.regs.S);
  ImGui::TableNextRow();
  ImGui::TableNextColumn();
  ImGui::TextColored(CLEM_HOST_GUI_CPU_FIELD_COLOR(D), "D    =%04X", frameReadState_.cpu.regs.D);
  ImGui::TableNextRow();
  ImGui::TableNextColumn();
  ImGui::TextColored(CLEM_HOST_GUI_CPU_FIELD_COLOR(A), "A    =%04X  ", frameReadState_.cpu.regs.A);
  ImGui::TableNextRow();
  ImGui::TableNextColumn();
  ImGui::TextColored(CLEM_HOST_GUI_CPU_FIELD_COLOR(X), "X    =%04X", frameReadState_.cpu.regs.X);
  ImGui::TableNextRow();
  ImGui::TableNextColumn();
  ImGui::TextColored(CLEM_HOST_GUI_CPU_FIELD_COLOR(Y), "Y    =%04X", frameReadState_.cpu.regs.Y);
  ImGui::TableNextRow();
  ImGui::TableNextColumn();
  ImGui::TextColored(getStatusFieldColor<uint8_t>(lastFrameCPURegs_.P, frameReadState_.cpu.regs.P,
                                                  kClemensCPUStatus_Negative),
                     "n");
  ImGui::SameLine(0.0f, 4.0f);
  ImGui::TextColored(getStatusFieldColor<uint8_t>(lastFrameCPURegs_.P, frameReadState_.cpu.regs.P,
                                                  kClemensCPUStatus_Overflow),
                     "v");
  ImGui::SameLine(0.0f, 4.0f);
  ImGui::TextColored(getStatusFieldColor<uint8_t>(lastFrameCPURegs_.P, frameReadState_.cpu.regs.P,
                                                  kClemensCPUStatus_MemoryAccumulator),
                     frameReadState_.cpu.pins.emulation ? " " : "m");
  ImGui::SameLine(0.0f, 4.0f);
  ImGui::TextColored(getStatusFieldColor<uint8_t>(lastFrameCPURegs_.P, frameReadState_.cpu.regs.P,
                                                  kClemensCPUStatus_Index),
                     frameReadState_.cpu.pins.emulation ? " " : "x");
  ImGui::SameLine(0.0f, 4.0f);
  ImGui::TextColored(getStatusFieldColor<uint8_t>(lastFrameCPURegs_.P, frameReadState_.cpu.regs.P,
                                                  kClemensCPUStatus_Decimal),
                     "d");
  ImGui::SameLine(0.0f, 4.0f);
  ImGui::TextColored(getStatusFieldColor<uint8_t>(lastFrameCPURegs_.P, frameReadState_.cpu.regs.P,
                                                  kClemensCPUStatus_IRQDisable),
                     "i");
  ImGui::SameLine(0.0f, 4.0f);
  ImGui::TextColored(getStatusFieldColor<uint8_t>(lastFrameCPURegs_.P, frameReadState_.cpu.regs.P,
                                                  kClemensCPUStatus_Zero),
                     "z");
  ImGui::SameLine(0.0f, 4.0f);
  ImGui::TextColored(getStatusFieldColor<uint8_t>(lastFrameCPURegs_.P, frameReadState_.cpu.regs.P,
                                                  kClemensCPUStatus_Carry),
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
}

void ClemensFrontend::doMachineViewLayout(ImVec2 rootAnchor, ImVec2 rootSize, float screenU,
                                          float screenV) {
  ImGui::SetNextWindowPos(rootAnchor);
  ImGui::SetNextWindowSize(rootSize);
  ImGui::Begin("Display", nullptr,
               ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize |
                   ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoBringToFrontOnFocus |
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
      ImVec2(monitorAnchor.x + monitorSize.x, monitorAnchor.y + monitorSize.y), ImVec2(0, 0),
      display_uv, ImGui::GetColorU32(tint_col));

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
               ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize |
                   ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoMove);
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
  ImGui::PushStyleColor(ImGuiCol_FrameBg, ImGui::GetStyleColorVec4(ImGuiCol_WindowBg));
  ImGui::Text("*");
  ImGui::SameLine();
  auto &style = ImGui::GetStyle();
  float xpos = ImGui::GetCursorPosX();
  float rightPaddingX =
      3 * (ImGui::GetFont()->GetCharAdvance('A') + style.FramePadding.x * 2 + style.ItemSpacing.x);
  ImGui::SetNextItemWidth(rootSize.x - xpos - ImGui::GetStyle().WindowPadding.x - rightPaddingX);
  if (ImGui::InputText("##", inputLine, sizeof(inputLine), ImGuiInputTextFlags_EnterReturnsTrue)) {
    auto *inputLineEnd = &inputLine[0] + strnlen(inputLine, sizeof(inputLine));
    if (inputLineEnd != &inputLine[0]) {
      const char *inputStart = &inputLine[0];
      const char *inputEnd = inputLineEnd - 1;
      for (; std::isspace(*inputStart) && inputStart < inputEnd; ++inputStart)
        ;
      for (; std::isspace(*inputEnd) && inputEnd > inputStart; --inputEnd)
        ;
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
                        terminalMode_ == TerminalMode::Command ? activeButtonColor : buttonColor);
  if (ImGui::Button("C")) {
    terminalMode_ = TerminalMode::Command;
  }
  ImGui::PopStyleColor();
  ImGui::SameLine();
  ImGui::PushStyleColor(ImGuiCol_Button,
                        terminalMode_ == TerminalMode::Log ? activeButtonColor : buttonColor);
  if (ImGui::Button("L")) {
    terminalMode_ = TerminalMode::Log;
  }
  ImGui::PopStyleColor();
  ImGui::SameLine();
  ImGui::PushStyleColor(ImGuiCol_Button,
                        terminalMode_ == TerminalMode::Execution ? activeButtonColor : buttonColor);
  if (ImGui::Button("E")) {
    terminalMode_ = TerminalMode::Execution;
  }
  ImGui::PopStyleColor();
  ImGui::End();
}

void ClemensFrontend::layoutTerminalLines() {
  for (size_t index = 0; index < terminalLines_.size(); ++index) {
    auto &line = terminalLines_.at(index);
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
    auto &line = consoleLines_.at(index);
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

void ClemensFrontend::doModalOperations(int width, int height) {
  if (!ImGuiFileDialog::Instance()->WasOpenedThisFrame()) {
    if (guiMode_ == GUIMode::ImportDiskModal) {
      //  File GUI dialog
      const char *filters =
          "Disk image files (*.dsk *.do *.po *.2mg *.woz){.dsk,.do,.po,.2mg,.woz}";

      ImGuiFileDialog::Instance()->OpenDialog("choose_disk_images", "Choose Disk Image", filters,
                                              ".", "", 16, (void *)((intptr_t)importDriveType_),
                                              ImGuiFileDialogFlags_Modal);
    }
  }
  if (ImGuiFileDialog::Instance()->Display("choose_disk_images", ImGuiWindowFlags_NoCollapse,
                                           ImVec2(1024, 512), ImVec2(width, height))) {
    if (ImGuiFileDialog::Instance()->IsOk()) {
      auto selection = ImGuiFileDialog::Instance()->GetSelection();
      auto filepath = ImGuiFileDialog::Instance()->GetCurrentPath();
      auto driveType = (ClemensDriveType)(reinterpret_cast<intptr_t>(
          ImGuiFileDialog::Instance()->GetUserDatas()));
      importDiskFiles_.clear();
      for (auto &e : selection) {
        importDiskFiles_.emplace_back(e.second);
      }
      guiMode_ = GUIMode::ImportDiskSetFlow;
      ImGui::OpenPopup("Import Master Disk Set");
    } else {
      guiMode_ = GUIMode::Emulator;
    }

    ImGuiFileDialog::Instance()->Close();
  }
  if (!ImGui::IsPopupOpen("Enter Disk Set Name")) {
    if (guiMode_ == GUIMode::BlankDiskModal) {
      ImGui::OpenPopup("Enter Disk Set Name");
    }
  }
  ImVec2 center = ImGui::GetMainViewport()->GetCenter();
  ImGui::SetNextWindowPos(center, ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
  ImGui::SetNextWindowSize(
      ImVec2(std::max(720.0f, width * 0.33f), 7 * ImGui::GetTextLineHeightWithSpacing()));
  if (ImGui::BeginPopupModal("Enter Disk Set Name", NULL, 0)) {
    ImGui::Spacing();
    char collectionName[64] = "";
    char imageName[64] = "";

    //  TODO: seems the user has to enter text and press enter always to confirm
    //        adding an OK button seems that the right thing to do, but there's
    //        AFAIK no way to force InputText() to return true without some
    //       'next frame return true' hack flag.  Investigate but no need to
    //        waste cycles on figuring out this minor UI convenience for now.
    ImGui::BeginTable("Disk Label Entry", 2);
    ImGui::TableSetupColumn("", ImGuiTableColumnFlags_WidthFixed,
                            ImGui::CalcTextSize("Disk Name").x +
                                ImGui::GetStyle().ColumnsMinSpacing);
    ImGui::TableSetupColumn("", ImGuiTableColumnFlags_WidthStretch);
    ImGui::TableNextRow();
    ImGui::TableNextColumn();
    ImGui::TextUnformatted("Disk Set");
    ImGui::TableNextColumn();
    if (ImGui::InputText("##1", collectionName, sizeof(collectionName),
                         ImGuiInputTextFlags_EnterReturnsTrue)) {
      importDiskSetPath_ = collectionName;
    }
    ImGui::TableNextRow();
    ImGui::TableNextColumn();
    ImGui::TextUnformatted("Disk Name");
    ImGui::TableNextColumn();
    if (ImGui::InputText("##2", imageName, sizeof(imageName),
                         ImGuiInputTextFlags_EnterReturnsTrue)) {
      importDiskSetName_ = imageName;
    }
    ImGui::EndTable();
    if (ImGui::Button("Ok")) {
      if (!importDiskSetPath_.empty() && !std::filesystem::path(importDiskSetPath_).empty() &&
          !importDiskSetName_.empty()) {
        importDiskSetPath_ =
            (std::filesystem::path(diskLibraryRootPath_) / importDiskSetPath_).string();
        guiMode_ = GUIMode::NewBlankDisk;
      } else {
        guiMode_ = GUIMode::Emulator;
      }
      ImGui::CloseCurrentPopup();
    }
    ImGui::SameLine();
    if (ImGui::Button("Cancel")) {
      guiMode_ = GUIMode::Emulator;
      ImGui::CloseCurrentPopup();
    }
    ImGui::Spacing();
    ImGui::EndPopup();
  }
}

void ClemensFrontend::doImportDiskSetFlowStart(int width, int height) {
  auto guiModeLast = guiMode_;
  ImVec2 center = ImGui::GetMainViewport()->GetCenter();
  ImGui::SetNextWindowPos(center, ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
  if (ImGui::BeginPopupModal("Import Master Disk Set", NULL, ImGuiWindowFlags_AlwaysAutoResize)) {
    ImGui::Spacing();
    ImGui::Text("Enter the name of the imported disk set collection.");
    char collectionName[64] = "";
    ImGui::SetNextItemWidth(ImGui::GetWindowContentRegionWidth() -
                            ImGui::GetStyle().WindowPadding.x);
    //  TODO: seems the user has to enter text and press enter always to confirm
    //        adding an OK button seems that the right thing to do, but there's
    //        AFAIK no way to force InputText() to return true without some
    //       'next frame return true' hack flag.  Investigate but no need to
    //        waste cycles on figuring out this minor UI convenience for now.
    ImGui::SetItemDefaultFocus();
    bool doImport = false;
    if (ImGui::InputText("##", collectionName, sizeof(collectionName),
                         ImGuiInputTextFlags_EnterReturnsTrue)) {
      importDiskSetName_ = collectionName;
      doImport = true;
    }
    if (ImGui::Button("Ok")) {
      doImport = true;
    }
    if (doImport) {
      if (!importDiskSetName_.empty() && !std::filesystem::path(importDiskSetName_).empty()) {
        importDiskSetPath_ =
            (std::filesystem::path(diskLibraryRootPath_) / importDiskSetName_).string();
        if (std::filesystem::exists(importDiskSetPath_)) {
          guiMode_ = GUIMode::ImportDiskSetReplaceOld;
        } else {
          guiMode_ = GUIMode::ImportDiskSet;
        }
      } else {
        guiMode_ = GUIMode::Emulator;
      }
      ImGui::CloseCurrentPopup();
    }
    ImGui::SameLine();
    if (ImGui::Button("Cancel")) {
      guiMode_ = GUIMode::Emulator;
      ImGui::CloseCurrentPopup();
    }
    ImGui::Spacing();
    ImGui::EndPopup();
  }
  if (guiModeLast != guiMode_) {
    if (guiMode_ == GUIMode::ImportDiskSetReplaceOld) {
      ImGui::OpenPopup("Replace Disk Set");
    } else if (guiMode_ == GUIMode::ImportDiskSet) {
      auto result = importDisks(importDiskSetPath_, importDiskSetName_, importDriveType_,
                                std::move(importDiskFiles_));
      if (!result.second) {
        messageModalString_ = std::move(result.first);
        ImGui::OpenPopup("Import Disk Set Error");
      } else {
        messageModalString_ = fmt::format("Import {} completed.", importDiskSetName_);
        ImGui::OpenPopup("Confirmation");
      }
    }
  }
}

void ClemensFrontend::doImportDiskSetReplaceOld(int width, int height) {
  auto guiModeLast = guiMode_;
  ImVec2 center = ImGui::GetMainViewport()->GetCenter();
  ImGui::SetNextWindowPos(center, ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
  ImGui::SetNextWindowSize(
      ImVec2(std::max((float)width * 0.40f, 720.0f), std::max((float)height * 0.25f, 360.0f)),
      ImGuiCond_Appearing);
  bool failure = false;
  if (ImGui::BeginPopupModal("Replace Disk Set", NULL)) {
    ImGui::Spacing();
    ImGui::Spacing();
    ImGui::TextWrapped("%s already exists.", importDiskSetPath_.c_str());
    ImGui::Spacing();
    ImGui::Text("Replace?");
    ImGui::Spacing();
    ImGui::Spacing();

    auto cursorPos = ImGui::GetCursorPos();
    auto contentRegionAvail = ImGui::GetContentRegionAvail();

    ImGui::SetCursorPos(ImVec2(
        cursorPos.x, cursorPos.y + contentRegionAvail.y -
                         (ImGui::GetStyle().FramePadding.y * 2 + ImGui::GetTextLineHeight())));
    if (ImGui::Button("Yes")) {
      std::error_code errorCode;
      ImGui::CloseCurrentPopup();
      guiMode_ = GUIMode::ImportDiskSet;
      if (std::filesystem::remove_all(importDiskSetPath_, errorCode) < 0) {
        failure = true;
      }
    }
    ImGui::SameLine();
    if (ImGui::Button("No")) {
      ImGui::CloseCurrentPopup();
      guiMode_ = GUIMode::Emulator;
    }
    ImGui::EndPopup();
  }

  if (guiModeLast != guiMode_) {
    if (guiMode_ == GUIMode::ImportDiskSet) {
      if (failure) {
        ImGui::OpenPopup("Import Disk Set Error");
        messageModalString_ = "Cancelled Dist Set import.";
      } else {
        auto result = importDisks(importDiskSetPath_, importDiskSetName_, importDriveType_,
                                  std::move(importDiskFiles_));
        if (!result.second) {
          ImGui::OpenPopup("Import Disk Set Error");
          messageModalString_ = std::move(result.first);
        } else {
          messageModalString_ = fmt::format("Import {} completed.", importDiskSetName_);
          ImGui::OpenPopup("Confirmation");
        }
      }
    }
  }
}

void ClemensFrontend::doImportDiskSet(int width, int height) {
  ImVec2 center = ImGui::GetMainViewport()->GetCenter();
  ImGui::SetNextWindowPos(center, ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
  ImGui::SetNextWindowSize(
      ImVec2(std::max((float)width * 0.40f, 720.0f), std::max((float)height * 0.25f, 360.0f)),
      ImGuiCond_Appearing);
  if (ImGui::BeginPopupModal("Import Disk Set Error", NULL)) {
    auto cursorPos = ImGui::GetCursorPos();
    auto contentRegionAvail = ImGui::GetContentRegionAvail();
    ImGui::Spacing();
    ImGui::Spacing();
    ImGui::TextWrapped(messageModalString_.c_str());
    ImGui::Spacing();
    ImGui::Spacing();
    ImGui::SetCursorPos(ImVec2(
        cursorPos.x, cursorPos.y + contentRegionAvail.y -
                         (ImGui::GetStyle().FramePadding.y * 2 + ImGui::GetTextLineHeight())));
    if (ImGui::Button("Ok")) {
      ImGui::CloseCurrentPopup();
      guiMode_ = GUIMode::Emulator;
    }
    ImGui::EndPopup();
  }
  ImGui::SetNextWindowPos(center, ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
  ImGui::SetNextWindowSize(
      ImVec2(std::max((float)width * 0.40f, 720.0f), std::max((float)height * 0.20f, 280.0f)),
      ImGuiCond_Appearing);
  if (ImGui::BeginPopupModal("Confirmation", NULL)) {
    auto cursorPos = ImGui::GetCursorPos();
    auto contentRegionAvail = ImGui::GetContentRegionAvail();
    ImGui::Spacing();
    ImGui::Spacing();
    ImGui::TextWrapped(messageModalString_.c_str());
    ImGui::Spacing();
    ImGui::SetCursorPos(ImVec2(
        cursorPos.x, cursorPos.y + contentRegionAvail.y -
                         (ImGui::GetStyle().FramePadding.y * 2 + ImGui::GetTextLineHeight())));
    if (ImGui::Button("Ok")) {
      ImGui::CloseCurrentPopup();
      guiMode_ = GUIMode::Emulator;
    }
    ImGui::EndPopup();
  }
}

void ClemensFrontend::doNewBlankDisk(int width, int height) {
  // importDiskSetPath_ is the path of the disk set
  // importDiskSetName_ is the name of the blank dist
  // Must create or use the existing path pointed to in importDiskSetPath
  // location = importDiskSetPath_ / importDiskSetName_ + .woz
  // If a file exists (@location), then
  //  error out
  // Generate local disk set/name path for:
  //   auto relativePath = location.parent_path().filename() / location.stem();
  //  issue backend::InsertNewDisk()
}

std::pair<std::string, bool> ClemensFrontend::importDisks(std::string outputPath,
                                                          std::string collectionName,
                                                          ClemensDriveType driveType,
                                                          std::vector<std::string> imagePaths) {
  //  parse file extension for supported types:
  //    WOZ
  //    2MG
  //    DSK
  //    DO
  //    PO
  //  only succeed if each image works with the desired drive type
  ClemensDiskImporter importer(driveType, imagePaths.size());
  for (auto &imagePath : imagePaths) {
    ClemensWOZDisk *disk = importer.add(imagePath);
    if (!disk) {
      return std::make_pair(fmt::format("Failed to import disk image {} for drive format {}",
                                        imagePath, sDriveDescription[driveType]),
                            false);
    }
    switch (driveType) {
    case kClemensDrive_3_5_D1:
    case kClemensDrive_3_5_D2:
      if (disk->nib->disk_type != CLEM_DISK_TYPE_3_5) {
        return std::make_pair(
            fmt::format("Disk image {} with type 3.5 doesn't match drive with required format {}",
                        imagePath, sDriveDescription[driveType]),
            false);
      }
      break;
    case kClemensDrive_5_25_D1:
    case kClemensDrive_5_25_D2:
      if (disk->nib->disk_type != CLEM_DISK_TYPE_5_25) {
        return std::make_pair(
            fmt::format("Disk image {} with type 5.25 doesn't match drive with required format {}",
                        imagePath, sDriveDescription[driveType]),
            false);
      }
      break;
    }
  }
  if (!std::filesystem::create_directories(outputPath)) {
    return std::make_pair(fmt::format("Unable to create directory {}", outputPath), false);
  }
  if (!importer.build(outputPath)) {
    //  TODO: mare information please!
    return std::make_pair(
        fmt::format("Import build step failed for drive type {}", sDriveDescription[driveType]),
        false);
  }

  return std::make_pair(outputPath, true);
}

static std::string_view trimToken(const std::string_view& token, size_t off = 0,
                                  size_t length=std::string_view::npos) {
  return token.substr(off, length);
}

static bool parseBool(const std::string_view& token, bool& result) {
  if (token == "on" || token == "true") {
    result = true;
    return true;
  } else if (token == "off" || token == "false") {
    result = false;
    return false;
  }
  int v = 0;
  if (std::from_chars(token.data(), token.data() + token.size(), v).ec == std::errc{}) {
    result = v != 0;
    return true;
  }
  return false;
}

void ClemensFrontend::executeCommand(std::string_view command) {
  CLEM_TERM_COUT.format(TerminalLine::Command, "* {}", command);
  auto sep = command.find(' ');
  auto action = trimToken(command, 0, sep);
  auto operand = sep != std::string_view::npos ? trimToken(command, sep + 1) : std::string_view();
  if (action == "help" || action == "?") {
    cmdHelp(operand);
  } else if (action == "run" || action == "r") {
    cmdRun(operand);
  } else if (action == "break" || action == "b") {
    cmdBreak(operand);
  } else if (action == "list" || action == "l") {
    cmdList(operand);
  } else if (action == "reboot") {
    cmdReboot(operand);
  } else if (action == "reset") {
    cmdReset(operand);
  } else if (action == "disk") {
    cmdDisk(operand);
  } else if (!action.empty()) {
    CLEM_TERM_COUT.print(TerminalLine::Error, "Unrecognized command!");
  }
}

void ClemensFrontend::cmdHelp(std::string_view operand) {
  CLEM_TERM_COUT.print(TerminalLine::Info,
                       "reset                       - soft reset of the machine");
  CLEM_TERM_COUT.print(TerminalLine::Info,
                       "reboot                      - hard reset of the machine");
  CLEM_TERM_COUT.print(TerminalLine::Info, "disk                        - disk information");
  CLEM_TERM_COUT.print(TerminalLine::Info, "disk <drive>,file=<image>   - insert disk");
  CLEM_TERM_COUT.print(TerminalLine::Info, "disk <drive>,wprot=<off|on> - write protect");
  CLEM_TERM_COUT.print(TerminalLine::Info, "disk <drive>,eject          - eject disk");
  CLEM_TERM_COUT.print(TerminalLine::Info,
                       "r]un                        - execute emulator until break");
  CLEM_TERM_COUT.print(TerminalLine::Info,
                       "b]reak                      - break execution at current PC");
  CLEM_TERM_COUT.print(TerminalLine::Info,
                       "b]reak <address>            - break execution at address");
  CLEM_TERM_COUT.print(TerminalLine::Info,
                       "b]reak r:<address>          - break on data read from address");
  CLEM_TERM_COUT.print(TerminalLine::Info,
                       "b]reak w:<address>          - break on write to address");
  CLEM_TERM_COUT.print(TerminalLine::Info, "l]ist                       - list all breakpoints");
  CLEM_TERM_COUT.newline();
}

void ClemensFrontend::cmdBreak(std::string_view operand) {
  //  parse [r|w]<address>
  ClemensBackendBreakpoint breakpoint{ClemensBackendBreakpoint::Undefined};
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
      CLEM_TERM_COUT.format(TerminalLine::Error, "Breakpoint type {} is invalid.", typeStr);
      return;
    }
    operand = operand.substr(sepPos + 1);
    if (operand.empty()) {
      CLEM_TERM_COUT.format(TerminalLine::Error, "Breakpoint type {} is invalid.", typeStr);
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
    char *addressEnd = NULL;
    breakpoint.address = std::strtoul(address, &addressEnd, 16);
    if (addressEnd != address + 6) {
      CLEM_TERM_COUT.format(TerminalLine::Error, "Address format is invalid read from '{}'",
                            operand);
      return;
    }
    backend_->addBreakpoint(breakpoint);
  }
}

void ClemensFrontend::cmdList(std::string_view /*operand*/) {
  //  TODO: granular listing based on operand
  static const char *bpType[] = {"unknown", "execute", "data-read", "write"};
  if (breakpoints_.empty()) {
    CLEM_TERM_COUT.print(TerminalLine::Info, "No breakpoints defined.");
    return;
  }
  for (size_t i = 0; i < breakpoints_.size(); ++i) {
    auto &bp = breakpoints_[i];
    CLEM_TERM_COUT.format(TerminalLine::Info, "bp #{}: {:02X}/{:04X} {}", i,
                          (bp.address >> 16) & 0xff, bp.address & 0xffff, bpType[bp.type]);
  }
}

void ClemensFrontend::cmdRun(std::string_view /*operand*/) { backend_->run(); }

void ClemensFrontend::cmdReboot(std::string_view /*operand*/) {
  guiMode_ = GUIMode::RebootEmulator;
  backend_->terminate();
}

void ClemensFrontend::cmdReset(std::string_view /*operand*/) { backend_->reset(); }

void ClemensFrontend::cmdDisk(std::string_view operand) {
  // disk
  // disk <drive>,file=<image>
  // disk <drive>,wprot=off|on
  if (operand.empty()) {
    for (auto it = frameReadState_.diskDrives.begin(); it != frameReadState_.diskDrives.end();
         ++it) {
      auto driveType = static_cast<ClemensDriveType>(it - frameReadState_.diskDrives.begin());
    CLEM_TERM_COUT.format(TerminalLine::Info, "{} {}: {}", it->isWriteProtected ? "wp" : "  ",
                            ClemensDiskUtilities::getDriveName(driveType),
                            it->imagePath.empty() ? "<none>" : it->imagePath);
    }
    return;
  }
  auto sepPos = operand.find(',');
  auto driveType = ClemensDiskUtilities::getDriveType(trimToken(operand, 0, sepPos));
  if (driveType == kClemensDrive_Invalid) {
    CLEM_TERM_COUT.format(TerminalLine::Error, "Invalid drive name {} specified.", operand);
    return;
  }
  auto& driveInfo = frameReadState_.diskDrives[driveType];
  std::string_view diskOpExpr;
  if (sepPos == std::string_view::npos || (diskOpExpr = trimToken(operand, sepPos+1)).empty()) {

    CLEM_TERM_COUT.format(TerminalLine::Info, "{} {}: {}", driveInfo.isWriteProtected ? "wp" : "  ",
                            ClemensDiskUtilities::getDriveName(driveType),
                            driveInfo.imagePath.empty() ? "<none>" : driveInfo.imagePath);
    return;
  }

  bool validOp = true;
  bool validValue = true;
  sepPos = diskOpExpr.find('=');
  auto diskOpType = trimToken(diskOpExpr, 0, sepPos);
  std::string_view diskOpValue;
  if (sepPos == std::string_view::npos) {
    if (diskOpType == "eject") {
      backend_->ejectDisk(driveType);
    } else {
      validOp = false;
    }
  } else {
    diskOpValue = trimToken(diskOpExpr, sepPos+1);
    if (diskOpType == "file") {
      backend_->insertDisk(driveType, std::string(diskOpValue));
    } else if (diskOpType == "wprot") {
      bool wprot;
      if (parseBool(diskOpValue, wprot)) {
        backend_->writeProtectDisk(driveType, wprot);
      } else {
        validValue = false;
      }
    } else {
      validOp = false;
    }
  }
  if (!validValue) {
    CLEM_TERM_COUT.format(TerminalLine::Error, "Invalid value {} in expression.", diskOpValue);
    return;
  } else if (!validOp) {
    CLEM_TERM_COUT.format(TerminalLine::Error, "Invalid or unsupported operation {}.", diskOpExpr);
    return;
  }
}
