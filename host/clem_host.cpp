
// TODO: cross-platform support

//  windows................grrrrr
#define NOMINMAX

#include "clem_host.hpp"
#include "clem_audio.hpp"
#include "clem_display.hpp"
#include "clem_program_trace.hpp"

#include "clem_host_platform.h"
#include "clem_debug.h"
#include "clem_drive.h"
#include "clem_mem.h"
#include "clem_mmio_defs.h"
#include "serializer.h"
#include "clem_vgc.h"

#include "fmt/format.h"
#include "imgui/imgui.h"

#include <algorithm>
#include <chrono>
#include <cctype>
#include <cstdio>
#include <cstdlib>

#include <fstream>

#include <inttypes.h>
#include <sys/stat.h>

namespace {
  constexpr unsigned kSlabMemorySize = 32 * 1024 * 1024;
  constexpr float kMinDebugHistoryHeight = 256;
  constexpr float kMinDebugHistoryScalar = 0.500f;
  constexpr float kMinDebugStatusHeight = 104;
  constexpr float kMinDebugStatusScalar = 0.180f;
  constexpr float kMinDebugTerminalHeight = 184;
  constexpr float kMinDebugTerminalScalar = 0.320f;

  constexpr float kMinConsoleWidth = 384;
  constexpr float kConsoleWidthScalar = 0.333f;

  constexpr unsigned kEmulationRunForever = 0x00ffffff;
  constexpr unsigned kEmulationRunTargetNone = 0xffffffff;

  struct FormatView {
    using BufferType = std::vector<char>;
    BufferType& buffer_;
    FormatView(BufferType& buffer): buffer_(buffer) {}
    template<typename... Args> void format(const char* formatStr, const Args&... args) {
      size_t sz = fmt::formatted_size(formatStr, args...);
      size_t start = buffer_.size();
      buffer_.resize(start + sz + 1);
      fmt::format_to_n(buffer_.data() + start, sz + 1, formatStr, args...);
    }
  };

  char clemens_is_mmio_card_rom(const ClemensMMIO& mmio, unsigned slot) {
    assert(slot > 0);
    if ((mmio.mmap_register & CLEM_MEM_IO_MMAP_CXROM) ||
        (mmio.mmap_register & (CLEM_MEM_IO_MMAP_C1ROM << (slot - 1)))
    ) {
      return 'C';
    } else {
      return 'I';
    }
  }

  unsigned calculateMaxDiskDataSize(unsigned int disk_type)
  {
    unsigned trackDataSize = 0;
    switch (disk_type) {
      case CLEM_DISK_TYPE_5_25:
        trackDataSize = 40 * 13 * 512;
        break;
      case CLEM_DISK_TYPE_3_5:
        trackDataSize = 160 * CLEM_DISK_35_CALC_BYTES_FROM_SECTORS(12);
        break;
    }
    assert(trackDataSize > 0);
    return trackDataSize;
  }


  double calculateTimeSpent(ClemensMachine* machine) {
    return (machine->clocks_spent / double(CLEM_CLOCKS_MEGA2_CYCLE)) *
      (1.0 / CLEM_MEGA2_CYCLES_PER_SECOND);
  }

} // namespace anon

#define CLEM_HOST_COUT FormatView(terminalOutput_)

void ClemensHost::Diagnostics::reset()
{
  audioFrames = 0;
  clocksSpent = 0;
  deltaTime = 0.0f;
  frameTime = 5.0f;
}


ClemensHost::ClemensHost() :
  machine_(),
  disks35_{},
  disks525_{},
  emulationRunTime_(0.0f),
  emulationSliceTimeLeft_(0.0f),
  emulationSliceDuration_(0.0f),
  emulationStepCount_(0),
  emulationStepCountSinceReset_(0),
  machineCyclesSpentDuringSample_(0),
  sampleDuration_(0.0f),
  emulationSpeedSampled_(0.0f),
  emulationRunTarget_(kEmulationRunTargetNone),
  emulatorHasKeyboardFocus_(false),
  cpuRegsSaved_ {},
  cpuPinsSaved_ {},
  cpu6502EmulationSaved_(true),
  widgetInputContext_(InputContext::None),
  widgetDebugContext_(DebugContext::IWM),
  display_(nullptr),
  adbKeyToggleMask_(0)
{
  ClemensTraceExecutedInstruction::initialize();

  void* slabMemory = malloc(kSlabMemorySize);
  slab_ = cinek::FixedStack(kSlabMemorySize, slabMemory);
  executedInstructions_.reserve(1024);
  memoryViewStatic_[0].HandlerContext = this;
  memoryViewStatic_[0].ReadFn = &ClemensHost::emulatorImGuiMemoryRead;
  memoryViewStatic_[0].WriteFn = &ClemensHost::emulatorImguiMemoryWrite;
  memoryViewStatic_[1].HandlerContext = this;
  memoryViewStatic_[1].ReadFn = &ClemensHost::emulatorImGuiMemoryRead;
  memoryViewStatic_[1].WriteFn = &ClemensHost::emulatorImguiMemoryWrite;

  for (auto& disk : disks525_) {
    auto maxDiskDataSize = calculateMaxDiskDataSize(CLEM_DISK_TYPE_5_25);
    disk.nib.disk_type = CLEM_DISK_TYPE_5_25;
    disk.nib.bits_data = (uint8_t*)malloc(maxDiskDataSize);
    disk.nib.bits_data_end = disk.nib.bits_data + maxDiskDataSize;
    disk.diskBrand = ClemensDisk::None;
  }

  for (auto& disk : disks35_) {
    auto maxDiskDataSize = calculateMaxDiskDataSize(CLEM_DISK_TYPE_3_5);
    disk.nib.disk_type = CLEM_DISK_TYPE_3_5;
    disk.nib.bits_data = (uint8_t*)malloc(maxDiskDataSize);
    disk.nib.bits_data_end = disk.nib.bits_data + maxDiskDataSize;
    disk.diskBrand = ClemensDisk::None;
  }

  displayProvider_ = std::make_unique<ClemensDisplayProvider>();
  display_ = std::make_unique<ClemensDisplay>(*displayProvider_);
  audio_ = std::make_unique<ClemensAudioDevice>();
  audio_->start();
}

ClemensHost::~ClemensHost()
{
  audio_->stop();

  for (auto& disk : disks525_) {
    free(disk.nib.bits_data);
  }
  for (auto& disk : disks35_) {
    free(disk.nib.bits_data);
  }

  void* slabMemory = slab_.getHead();
  if (slabMemory) {
    free(slabMemory);
  }
}

void ClemensHost::emulatorLog(
  int log_level,
  ClemensMachine* machine,
  const char* msg
) {
  static const char* levels[] = { "DEBUG", " INFO", " WARN", "UNIMP", "FATAL" };
  //  TODO: log level config
  if (log_level < CLEM_DEBUG_LOG_INFO) return;
  ClemensHost* host = reinterpret_cast<ClemensHost*>(machine->debug_user_ptr);
  fprintf(stdout, "[%s][%6.9lf]: ", levels[log_level], calculateTimeSpent(machine));
  fputs(msg, stdout);
  fputc('\n', stdout);
  if (log_level == CLEM_DEBUG_LOG_UNIMPL) {
    host->emulationBreak();
  }
}

uint8_t ClemensHost::emulatorImGuiMemoryRead(
  void* ctx,
  const uint8_t* data,
  size_t off
) {
  auto* self = reinterpret_cast<ClemensHost*>(ctx);
  uintptr_t dataPtr = (uintptr_t)data;
  uint8_t databank = uint8_t((dataPtr >> 16) & 0xff);
  uint16_t offset = uint16_t(dataPtr & 0xffff);
  uint8_t* realdata = (databank == 0xe0 || databank == 0xe1) ? (
    self->machine_.mega2_bank_map[databank & 0x1]) : (
      self->machine_.fpi_bank_map[databank]);
  uint8_t v;
  clem_read(&self->machine_, &v, uint16_t((offset + off) & 0xffff), databank,
            CLEM_MEM_FLAG_NULL);
  return v;
}

void ClemensHost::emulatorImguiMemoryWrite(
  void* ctx,
  uint8_t* data,
  size_t off,
  uint8_t d
) {
  auto* self = reinterpret_cast<ClemensHost*>(ctx);
  uintptr_t dataPtr = (uintptr_t)data;
  uint8_t databank = uint8_t((dataPtr >> 16) & 0xff);
  uint16_t offset = uint16_t(dataPtr & 0xffff);
  uint8_t* realdata = (databank == 0xe0 || databank == 0xe1) ? (
    self->machine_.mega2_bank_map[databank & 0x1]) : (
      self->machine_.fpi_bank_map[databank]);
  clem_write(&self->machine_, d, uint16_t((offset + off) & 0xffff), databank,
             CLEM_MEM_FLAG_NULL);
}

void ClemensHost::input(const ClemensInputEvent& input)
{
  if (isRunningEmulation() && emulatorHasKeyboardFocus_) {
    if (clemens_is_mmio_initialized(&machine_)) {
      clemens_input(&machine_, &input);
    } else if (machine_.mmio_bypass) {
      if (input.type == kClemensInputType_KeyDown) {
        simpleMachineIO_.eventKeybA2 = input.value;
        if (input.value == CLEM_ADB_KEY_LSHIFT || input.value == CLEM_ADB_KEY_RSHIFT) {
          simpleMachineIO_.modShift = true;
        }
      } else if (input.type == kClemensInputType_KeyUp) {
          if (input.value == CLEM_ADB_KEY_LSHIFT || input.value == CLEM_ADB_KEY_RSHIFT) {
            simpleMachineIO_.modShift = false;
          }
      }
    }
  }
}

void ClemensHost::frame(int width, int height, float deltaTime)
{
  bool emulationRan = false;
  if (isRunningEmulation()) {
    emulate(deltaTime);
    diagnostics_.deltaTime += deltaTime;
    emulationRan = true;
  }
  ClemensMonitor monitor = {};
  constexpr int kClemensScreenWidth = 720;
  constexpr int kClemensScreenHeight = 480;
  float screenUVs[2];

  if (clemens_is_mmio_initialized(&machine_)) {
    ClemensVideo video;
    clemens_get_monitor(&monitor, &machine_);

    display_->start(monitor, kClemensScreenWidth, kClemensScreenHeight);
    if (clemens_get_text_video(&video, &machine_)) {
      if (!(machine_.mmio.vgc.mode_flags & CLEM_VGC_80COLUMN_TEXT)) {
        display_->renderText40Col(
          video, machine_.mega2_bank_map[0],
          (machine_.mmio.vgc.mode_flags & CLEM_VGC_ALTCHARSET) ? true : false);
      } else {
        display_->renderText80Col(
          video, machine_.mega2_bank_map[0], machine_.mega2_bank_map[1],
          (machine_.mmio.vgc.mode_flags & CLEM_VGC_ALTCHARSET) ? true : false);
      }
    }
    if (clemens_get_graphics_video(&video, &machine_)) {
      if (video.format == kClemensVideoFormat_Double_Hires) {
        display_->renderDoubleHiresGraphics(video, machine_.mega2_bank_map[0],
                                            machine_.mega2_bank_map[1]);
      } else if (video.format == kClemensVideoFormat_Hires) {
        display_->renderHiresGraphics(video, machine_.mega2_bank_map[0]);
      } else if (machine_.mmio.vgc.mode_flags & CLEM_VGC_LORES) {
        display_->renderLoresGraphics(video, machine_.mega2_bank_map[0]);
      }
    }
    display_->finish(screenUVs);

    ClemensAudio audio;
    if (emulationRan && clemens_get_audio(&audio, &machine_)) {
      unsigned consumedFrames = audio_->queue(audio);
      clemens_audio_next_frame(&machine_, consumedFrames);
      diagnostics_.audioFrames += consumedFrames;
    }

    if (clem_host_get_caps_lock_state()) {
      adbKeyToggleMask_ |= CLEM_ADB_KEYB_TOGGLE_CAPS_LOCK;
    } else {
      adbKeyToggleMask_ &= ~CLEM_ADB_KEYB_TOGGLE_CAPS_LOCK;
    }
    clemens_input_key_toggle(&machine_, adbKeyToggleMask_);

    saveBRAM();
  } else if (clemens_is_initialized_simple(&machine_)) {
    /* simple machine video and input */

  }

  ImGui::SetNextWindowPos(ImVec2(512, 32), ImGuiCond_FirstUseEver);
  ImGui::SetNextWindowContentSize(ImVec2(kClemensScreenWidth, kClemensScreenHeight));
  ImGui::Begin("Display", nullptr, ImGuiWindowFlags_NoResize |
                                   ImGuiWindowFlags_NoCollapse |
                                   ImGuiWindowFlags_NoBringToFrontOnFocus);

  if (ImGui::IsWindowFocused()) {
    ImGui::SetKeyboardFocusHere(0);
    emulatorHasKeyboardFocus_ = true;
  } else {
    emulatorHasKeyboardFocus_ = false;
  }

  if (clemens_is_initialized(&machine_)) {
    ImVec4 tint_col = ImVec4(1.0f, 1.0f, 1.0f, 1.0f);   // No tint
    ImTextureID texId { (void *)((uintptr_t)display_->getScreenTarget().id) };
    ImVec2 p = ImGui::GetCursorScreenPos();
    ImVec2 display_uv(screenUVs[0], screenUVs[1]);
    ImGui::GetWindowDrawList()->AddImage(
      texId,
      p, ImVec2(p.x + kClemensScreenWidth, p.y + kClemensScreenHeight),
      ImVec2(0, 0), display_uv,
      ImGui::GetColorU32(tint_col));
  }

  ImGui::End();

  const struct ClemensCPURegs& cpuRegsNext = machine_.cpu.regs;
  const struct ClemensCPUPins& cpuPinsNext = machine_.cpu.pins;

  //  View
  ImVec2 windowSize;
  ImVec2 windowCursorPos;
  windowCursorPos.x = 0;
  windowCursorPos.y = 0;
  windowSize.x = std::max(kMinConsoleWidth, width * kConsoleWidthScalar);
  windowSize.y = std::max(kMinDebugHistoryHeight, height * kMinDebugHistoryScalar);
  windowSize.y -= ImGui::GetTextLineHeightWithSpacing();

  ImGui::SetNextWindowPos(windowCursorPos);
  ImGui::SetNextWindowSize(windowSize);
  ImGui::Begin("History", nullptr, ImGuiWindowFlags_NoResize |
                                   ImGuiWindowFlags_NoCollapse |
                                   ImGuiWindowFlags_NoBringToFrontOnFocus);

  for (auto& instruction : executedInstructions_) {
    ImGui::TextColored(ImVec4(0.75f, 0.75f, 0.75f, 1.0f), "(%u) %02X/%04X",
                       instruction.cycles_spent,
                       instruction.pc >> 16, instruction.pc & 0xffff);
    ImGui::SameLine();
    ImGui::TextColored(ImVec4(0.0f, 1.0f, 1.0f, 1.0f), "%s", instruction.opcode);
    ImGui::SameLine();
    ImGui::TextColored(ImVec4(0.0f, 1.0f, 0.5f, 1.0f), "%s", instruction.operand);
  }
  if (emulationRan) {
    ImGui::SetScrollHereY();
  }
  ImGui::End();

  windowCursorPos.y += windowSize.y;
  windowSize.y = ImGui::GetTextLineHeightWithSpacing();
  ImGui::SetNextWindowPos(windowCursorPos);
  ImGui::SetNextWindowSize(windowSize);
  ImGui::Begin("Panel", nullptr, ImGuiWindowFlags_NoTitleBar |
                                 ImGuiWindowFlags_NoScrollbar |
                                 ImGuiWindowFlags_NoResize |
                                 ImGuiWindowFlags_NoCollapse |
                                 ImGuiWindowFlags_NoBringToFrontOnFocus);
  {
    ImGui::BeginTable("leds", 3, 0);
    ImGui::TableNextColumn();
    ImGui::Text("S5"); ImGui::SameLine();
    doDriveBayLights(machine_.active_drives.slot5, 2,
        (machine_.mmio.dev_iwm.io_flags & CLEM_IWM_FLAG_DRIVE_2) ? 1 : 0,
        (machine_.mmio.dev_iwm.io_flags & CLEM_IWM_FLAG_DRIVE_35) != 0,
        (machine_.mmio.dev_iwm.io_flags & CLEM_IWM_FLAG_DRIVE_ON) != 0);
    ImGui::SameLine();
    ImGui::Text("S6"); ImGui::SameLine();
    doDriveBayLights(machine_.active_drives.slot6, 2,
        (machine_.mmio.dev_iwm.io_flags & CLEM_IWM_FLAG_DRIVE_2) ? 1 : 0,
        (machine_.mmio.dev_iwm.io_flags & CLEM_IWM_FLAG_DRIVE_35) == 0,
        (machine_.mmio.dev_iwm.io_flags & CLEM_IWM_FLAG_DRIVE_ON) != 0);
    ImGui::TableNextColumn();
    ImGui::Text("SPD"); ImGui::SameLine();
    if (clemens_is_initialized_simple(&machine_)) {
      ImGui::Text("%0.2f MHz", 1.023f * float(machine_.clocks_step_mega2) / machine_.clocks_step);
    }
    ImGui::TableNextColumn(); ImGui::Text("???");
    ImGui::EndTable();
  }
  ImGui::End();

  windowCursorPos.y += windowSize.y;
  windowSize.y = std::max(kMinDebugStatusHeight, height * kMinDebugStatusScalar);

  ImGui::SetNextWindowPos(windowCursorPos);
  ImGui::SetNextWindowSize(windowSize);
  ImGui::Begin("Status", nullptr, ImGuiWindowFlags_NoResize |
                                  ImGuiWindowFlags_NoCollapse |
                                  ImGuiWindowFlags_NoBringToFrontOnFocus);
  {
    //  N, V, M, B, D, I, Z, C
    //  N, V, M, X, D, I, Z, C
    //  PC, PBR, IR
    //  S, DBR, D
    //  A, X, Y
    //  Brk, Emu, MX
    //  Todo: cycles, clocks spent
    uint8_t cpuStatusChanged = cpuRegsNext.P ^ cpuRegsSaved_.P;
    bool selectedStatusBits[8] = {
      (cpuStatusChanged & kClemensCPUStatus_Negative) != 0,
      (cpuStatusChanged & kClemensCPUStatus_Overflow) != 0,
      (cpuStatusChanged & kClemensCPUStatus_MemoryAccumulator) != 0,
      (cpuStatusChanged & kClemensCPUStatus_Index) != 0,   // same mask as 6502 brk
      (cpuStatusChanged & kClemensCPUStatus_Decimal) != 0,
      (cpuStatusChanged & kClemensCPUStatus_IRQDisable) != 0,
      (cpuStatusChanged & kClemensCPUStatus_Zero) != 0,
      (cpuStatusChanged & kClemensCPUStatus_Carry) != 0
    };
    ImGui::BeginGroup();
    ImGui::BeginTable("cpu_status", 8, 0, ImVec2(windowSize.x * 0.5f, 0));
    ImGui::TableNextColumn(); ImGui::Text("N");
    ImGui::TableNextColumn(); ImGui::Text("V");
    if (machine_.cpu.pins.emulation) {
      ImGui::TableNextColumn(); ImGui::Text("-");
      ImGui::TableNextColumn(); ImGui::Text("-");
    } else {
      ImGui::TableNextColumn(); ImGui::Text("M");
      ImGui::TableNextColumn(); ImGui::Text("X");
    }
    ImGui::TableNextColumn(); ImGui::Text("D");
    ImGui::TableNextColumn(); ImGui::Text("I");
    ImGui::TableNextColumn(); ImGui::Text("Z");
    ImGui::TableNextColumn(); ImGui::Text("C");
    ImGui::TableNextRow();
    for (int i = 0; i < 8; ++i) {
      ImGui::TableNextColumn();
      ImGui::Selectable(cpuRegsNext.P & (1 << (7-i)) ? "1" : "0", selectedStatusBits[i]);
    }
    ImGui::EndTable();
    ImGui::SameLine();

    char label[16];
    ImGui::BeginTable("cpu_int", 6, 0);
    ImGui::TableNextColumn(); ImGui::Text("EMUL");
    ImGui::TableNextColumn(); ImGui::Text("RESB");
    ImGui::TableNextColumn(); ImGui::Text("RDYO");
    ImGui::TableNextColumn(); ImGui::Text("ADR");
    ImGui::TableNextColumn(); ImGui::Text("BANK");
    ImGui::TableNextColumn(); ImGui::Text("DATA");
    ImGui::TableNextRow();
    ImGui::TableNextColumn();
    ImGui::Selectable(cpuPinsNext.emulation ? "1" : "0",
      cpuPinsSaved_.emulation != cpuPinsNext.emulation);
    ImGui::TableNextColumn();
    ImGui::Selectable(cpuPinsNext.resbIn ? "1" : "0",
      cpuPinsSaved_.resbIn != cpuPinsNext.resbIn);
    ImGui::TableNextColumn();
    ImGui::Selectable(cpuPinsNext.readyOut ? "1" : "0",
      cpuPinsSaved_.readyOut != cpuPinsNext.readyOut);
    ImGui::TableNextColumn();
    snprintf(label, sizeof(label), "%04X", cpuPinsNext.adr);
    ImGui::Selectable(label, cpuPinsSaved_.adr != cpuPinsNext.adr);
    ImGui::TableNextColumn();
    snprintf(label, sizeof(label), "%02X", cpuPinsNext.bank);
    ImGui::Selectable(label, cpuPinsSaved_.bank != cpuPinsNext.bank);
    ImGui::TableNextColumn();
    snprintf(label, sizeof(label), "%02X", cpuPinsNext.data);
    ImGui::Selectable(label, cpuPinsSaved_.data != cpuPinsNext.data);
    ImGui::EndTable();
    ImGui::Separator();


    ImGui::BeginTable("cpu_regs", 3, 0, ImVec2(windowSize.x * 0.5f, 0));
    {
      ImGui::TableNextRow();
      snprintf(label, sizeof(label), "PC  = %04X", cpuRegsNext.PC);
      ImGui::TableNextColumn(); ImGui::Selectable(label, cpuRegsNext.PC != cpuRegsSaved_.PC);
      snprintf(label, sizeof(label), "S   = %04X", cpuRegsNext.S);
      ImGui::TableNextColumn(); ImGui::Selectable(label, cpuRegsNext.S != cpuRegsSaved_.S);
      snprintf(label, sizeof(label), "A   = %04X", cpuRegsNext.A);
      ImGui::TableNextColumn(); ImGui::Selectable(label, cpuRegsNext.A != cpuRegsSaved_.A);
      ImGui::TableNextRow();
      snprintf(label, sizeof(label), "PBR = %02X", cpuRegsNext.PBR);
      ImGui::TableNextColumn(); ImGui::Selectable(label, cpuRegsNext.PBR != cpuRegsSaved_.PBR);
      snprintf(label, sizeof(label), "DBR = %02X", cpuRegsNext.DBR);
      ImGui::TableNextColumn(); ImGui::Selectable(label, cpuRegsNext.DBR != cpuRegsSaved_.DBR);
      snprintf(label, sizeof(label), "X   = %04X", cpuRegsNext.X);
      ImGui::TableNextColumn(); ImGui::Selectable(label, cpuRegsNext.X != cpuRegsSaved_.X);
      ImGui::TableNextRow();
      snprintf(label, sizeof(label), "IR  = %02X", cpuRegsNext.IR);
      ImGui::TableNextColumn(); ImGui::Selectable(label, cpuRegsNext.IR != cpuRegsSaved_.IR);
      snprintf(label, sizeof(label), "D   = %04X", cpuRegsNext.D);
      ImGui::TableNextColumn(); ImGui::Selectable(label, cpuRegsNext.D != cpuRegsSaved_.D);
      snprintf(label, sizeof(label), "Y   = %04X", cpuRegsNext.Y);
      ImGui::TableNextColumn(); ImGui::Selectable(label, cpuRegsNext.Y != cpuRegsSaved_.Y);
    }
    ImGui::EndTable();
    ImGui::SameLine();
    ImGui::BeginTable("cpu_time", 2, 0, ImVec2(windowSize.x * 0.5f, 0));
    {
      ImGui::TableNextColumn(); ImGui::Text("Total Steps");
      ImGui::TableNextColumn(); ImGui::Text("%" PRIu64 "", emulationStepCountSinceReset_);
      ImGui::TableNextRow();
      ImGui::TableNextColumn(); ImGui::Text("Cycles/slice");
      ImGui::TableNextColumn(); ImGui::Text("%u", machine_.cpu.cycles_spent);
      ImGui::TableNextRow();
      ImGui::TableNextColumn(); ImGui::Text("Actual Speed");
      ImGui::TableNextColumn(); ImGui::Text("%.2lf mhz", emulationSpeedSampled_);
      ImGui::TableNextRow();
      ImGui::TableNextColumn(); ImGui::Text("Exec time");
      ImGui::TableNextColumn(); ImGui::Text("%.4f secs", emulationRunTime_);
      ImGui::TableNextColumn(); ImGui::Text("FPS");
      ImGui::TableNextColumn();
      if (emulationRan) {
        ImGui::Text("%.1f",  ImGui::GetIO().Framerate);
      } else {
        ImGui::Text("----");
      }
    }
    ImGui::EndTable();

    ImGui::EndGroup();
  }
  ImGui::End();

  windowCursorPos.y += windowSize.y;
  windowSize.y = std::max(kMinDebugTerminalHeight, height * kMinDebugTerminalScalar);
  ImGui::SetNextWindowPos(windowCursorPos);
  ImGui::SetNextWindowSize(windowSize);
  if (widgetInputContext_ == InputContext::TerminalKeyboardFocus) {
    // hacky - but unsure how else to keep my terminal in focus
    ImGui::SetNextWindowFocus();
  }
  ImGui::Begin("Terminal", nullptr, ImGuiWindowFlags_NoResize |
                                    ImGuiWindowFlags_NoCollapse );
  {
    char buffer[128] = "";
    ImGui::PushStyleColor(ImGuiCol_FrameBg, ImGui::GetStyleColorVec4(ImGuiCol_WindowBg));
    ImGui::Text(">");
    ImGui::SameLine();
    float xpos = ImGui::GetCursorPosX();
    ImGui::SetNextItemWidth(windowSize.x - xpos - ImGui::GetStyle().WindowPadding.x);

    if (widgetInputContext_ == InputContext::TerminalKeyboardFocus) {
      ImGui::SetKeyboardFocusHere(0);
      widgetInputContext_ = InputContext::None;
    }

    if (ImGui::InputText("", buffer, sizeof(buffer), ImGuiInputTextFlags_EnterReturnsTrue)) {
      terminalOutput_.clear();
      if (parseCommand(buffer)) {
        CLEM_HOST_COUT.format("Ok");
      } else {
        CLEM_HOST_COUT.format("Error");
      }
      widgetInputContext_ = InputContext::TerminalKeyboardFocus;
    }
    for (auto it = terminalOutput_.begin(); it != terminalOutput_.end();) {
      ImGui::Text(terminalOutput_.data() + (it - terminalOutput_.begin()));
      it = std::find(it, terminalOutput_.end(), 0);
      if (it != terminalOutput_.end()) ++it;
    }
    ImGui::SetItemDefaultFocus();
    ImGui::PopStyleColor();
  }
  ImGui::End();

  windowCursorPos.x += windowSize.x;

  ImVec2 memoryViewSize {windowSize.x, windowSize.y};
  ImVec2 memoryViewCursor {windowCursorPos};
  ImGui::SetNextWindowPos(memoryViewCursor);
  ImGui::SetNextWindowSize(memoryViewSize);

  ImGui::Begin("Context", nullptr, ImGuiWindowFlags_NoResize |
                                   ImGuiWindowFlags_NoCollapse |
                                   ImGuiWindowFlags_NoBringToFrontOnFocus);
  if (clemens_is_initialized_simple(&machine_)) {
    ImGui::BeginChild("context_memory", ImVec2(memoryViewSize.x, memoryViewSize.y/2));
    {
      memoryViewStatic_[0].ReadOnly = true;
      if (emulationRan) {
        memoryViewStatic_[0].GotoAddrAndHighlight(cpuPinsNext.adr, cpuPinsNext.adr + 1);
        memoryViewBank_[0] = cpuPinsNext.bank;
      }
      ImGui::BeginTable("context_memory", 1, 0,
                        ImVec2(memoryViewSize.x, memoryViewSize.y));
      ImGui::TableNextColumn();
      ImGui::InputScalar("Bank", ImGuiDataType_U8, &memoryViewBank_[0],
                        nullptr, nullptr, "%02X", ImGuiInputTextFlags_CharsHexadecimal);
      ImGui::TableNextRow();
      ImGui::TableNextColumn();
      uint8_t viewBank = memoryViewBank_[0];
      if (!isRunningEmulation() || isRunningEmulationStep()) {
        memoryViewStatic_[0].DrawContents((void*)
          (void *)((uintptr_t)viewBank << 16), CLEM_IIGS_BANK_SIZE);
      }
      ImGui::EndTable();
    }
    ImGui::EndChild();
    ImGui::Separator();
    if (widgetDebugContext_ == DebugContext::IWM) {
      doIWMContextWindow();
    } else if (widgetDebugContext_ == DebugContext::MemoryMaps) {
      doMemoryMapWindow();
    }
  }
  ImGui::End();
  memoryViewCursor.x += memoryViewSize.x;
  ImGui::SetNextWindowPos(memoryViewCursor);
  ImGui::SetNextWindowSize(memoryViewSize);
  ImGui::Begin("Memory 1", nullptr, ImGuiWindowFlags_NoResize |
                                    ImGuiWindowFlags_NoCollapse |
                                    ImGuiWindowFlags_NoBringToFrontOnFocus);
  if (clemens_is_initialized_simple(&machine_)) {
    ImGui::InputScalar("Bank", ImGuiDataType_U8, &memoryViewBank_[1],
                       nullptr, nullptr, "%02X", ImGuiInputTextFlags_CharsHexadecimal);
    uint8_t viewBank = memoryViewBank_[1];
    if (!isRunningEmulation() || isRunningEmulationStep()) {
      memoryViewStatic_[1].DrawContents((void*)
        (void *)((uintptr_t)viewBank << 16), CLEM_IIGS_BANK_SIZE);
    }
  }
  ImGui::End();

  if (diagnostics_.deltaTime >= diagnostics_.frameTime) {
    float scalar = 1.0f / diagnostics_.deltaTime;
    printf("diag_host: audio (%.01f/sec)\n"
           "diag_host: clocks (%.01f/sec)\n",
           diagnostics_.audioFrames * scalar,
           diagnostics_.clocksSpent * scalar);
    diagnostics_.reset();
  }

  if (machine_.mmio_bypass) {
    if (simpleMachineIO_.terminalOutIndex > 0) {
      printf(simpleMachineIO_.terminalOut);
      simpleMachineIO_.terminalOutIndex = 0;
    }
  }
}

void ClemensHost::doIWMContextWindow()
{
  const ClemensDeviceIWM& iwm = machine_.mmio.dev_iwm;

  ImGui::BeginGroup();
  ImGui::BeginTable("IWM", 3, ImGuiTableFlags_Resizable);
  ImGui::TableSetupColumn("Drive", ImGuiTableColumnFlags_WidthStretch);
  ImGui::TableSetupColumn("IWM LSS     ", ImGuiTableColumnFlags_WidthFixed);
  ImGui::TableSetupColumn("IWM Pins    ", ImGuiTableColumnFlags_WidthFixed);
  ImGui::TableHeadersRow();
  ImGui::TableNextColumn();
  {
    const ClemensDrive* drive = NULL;
    int driveIdx = (iwm.io_flags & CLEM_IWM_FLAG_DRIVE_2) ? 1 :
                   (iwm.io_flags & CLEM_IWM_FLAG_DRIVE_1) ? 0 :
                   -1;
    if (iwm.io_flags & CLEM_IWM_FLAG_DRIVE_35) {
      drive = &machine_.active_drives.slot5[driveIdx];
    } else {
      drive = &machine_.active_drives.slot6[driveIdx];
    }
    ImGui::BeginTable("IWM_Drive", 2, 0);
    {
      ImGui::TableNextColumn(); ImGui::Text("Disk");
      ImGui::TableNextColumn();
      if (drive && driveIdx >= 0) {
        ImGui::Text("%s D%d %s", (iwm.io_flags & CLEM_IWM_FLAG_DRIVE_35) ? "3.5" : "5.25",
                    driveIdx,
                    (iwm.io_flags & CLEM_IWM_FLAG_DRIVE_ON) ? "on": "off");
      } else {
        ImGui::Text("N/A");
      }
      ImGui::TableNextRow();
      ImGui::TableNextColumn(); ImGui::Text("Track");
      ImGui::TableNextColumn(); ImGui::Text("%.2f", drive->qtr_track_index / 4.0f);
      ImGui::TableNextRow();
      ImGui::TableNextColumn(); ImGui::Text("Bits");
      ImGui::TableNextColumn();
      if (drive && driveIdx >=0 && drive->has_disk) {
        ImGui::Text("%x", drive->disk.track_bits_count[drive->real_track_index]);
      }
      ImGui::TableNextRow();
      ImGui::TableNextColumn(); ImGui::Text("Pos");
      ImGui::TableNextColumn();
      if (drive && driveIdx >= 0 && drive->has_disk) {
        ImGui::Text("%X (%u: %X)",
                    (drive->track_byte_index * 8) + (7 - drive->track_bit_shift),
                    drive->track_bit_shift, drive->track_byte_index);
      }
      ImGui::TableNextRow();
      ImGui::TableNextColumn(); ImGui::Text("Byte");
      ImGui::TableNextColumn();
      if (drive && driveIdx >= 0 && drive->has_disk && drive->real_track_index < 0xfe) {
        const uint8_t* data = drive->disk.bits_data + (
          drive->disk.track_byte_offset[drive->real_track_index]);
        ImGui::Text("%02X", data[drive->track_byte_index]);
      }
    }
    ImGui::EndTable();
  }
  ImGui::TableNextColumn();
  {
    ImGui::BeginTable("IWM_LSS", 2, 0);
    {
      ImGui::TableNextColumn(); ImGui::Text("Latch");
      ImGui::TableNextColumn();
      ImGui::Text("%02X", iwm.latch);
      ImGui::TableNextRow();
      ImGui::TableNextColumn(); ImGui::Text("State");
      ImGui::TableNextColumn();
      ImGui::Text("%02X", iwm.lss_state);
      ImGui::TableNextRow();
      ImGui::TableNextColumn(); ImGui::Text("Q6,Q7");
      ImGui::TableNextColumn();
      ImGui::Text("%u,%u ", iwm.q6_switch, iwm.q7_switch);
    }
    ImGui::EndTable();
  }
  ImGui::TableNextColumn();
  {
    ImGui::BeginTable("IWM_Pins", 2, 0);
    {
      ImGui::TableNextColumn(); ImGui::Text("PH0_3");
      ImGui::TableNextColumn();
      ImGui::Text("%u%u%u%u", (iwm.out_phase & 1) ? 1 : 0,
                              (iwm.out_phase & 2) ? 1 : 0,
                              (iwm.out_phase & 4) ? 1 : 0,
                              (iwm.out_phase & 8) ? 1 : 0);
      ImGui::TableNextRow();
      ImGui::TableNextColumn(); ImGui::Text("SENSE");
      ImGui::TableNextColumn();
      if (iwm.io_flags & CLEM_IWM_FLAG_WRPROTECT_SENSE) {
        ImGui::Text("1");
      } else {
        ImGui::Text("0");
      }
      ImGui::TableNextRow();
      ImGui::TableNextColumn(); ImGui::Text("RDDATA");
      ImGui::TableNextColumn();
      if (iwm.io_flags & CLEM_IWM_FLAG_READ_DATA) {
        ImGui::Text("1");
      } else {
        ImGui::Text("0");
      }
      ImGui::TableNextRow();
      ImGui::TableNextColumn(); ImGui::Text("WRREQ");
      ImGui::TableNextColumn();
      if (iwm.io_flags & CLEM_IWM_FLAG_WRITE_REQUEST) {
        ImGui::Text("1");
      } else {
        ImGui::Text("0");
      }
      ImGui::TableNextRow();
      ImGui::TableNextColumn(); ImGui::Text("ENABLE2");
      ImGui::TableNextColumn();
      if (iwm.enable2) {
        ImGui::Text("1");
      } else {
        ImGui::Text("0");
      }
    }
    ImGui::EndTable();
  }
  ImGui::EndTable();
  ImGui::EndGroup();
}

void ClemensHost::doMemoryMapWindow()
{
  const ClemensMMIO& mmio = machine_.mmio;

  ImGui::BeginGroup();
  ImGui::BeginTable("MMIO_MemoryMaps", 3);
  ImGui::TableSetupColumn("64K");
  ImGui::TableSetupColumn("128K");
  ImGui::TableSetupColumn("Video");
  ImGui::TableHeadersRow();
  ImGui::TableNextColumn();
  {
    ImGui::BeginTable("MMIO_MEMORY", 2, 0);
    {
      ImGui::TableNextColumn(); ImGui::Text("MMAPR");
      ImGui::TableNextColumn();
      ImGui::Text("%08X", mmio.mmap_register);
      ImGui::TableNextRow();
      ImGui::TableNextColumn(); ImGui::Text("SLOTS");
      ImGui::TableNextColumn();
      ImGui::Text("%c%c%c%c%c%c%c", clemens_is_mmio_card_rom(mmio, 1),
                                    clemens_is_mmio_card_rom(mmio, 2),
                                    clemens_is_mmio_card_rom(mmio, 3),
                                    clemens_is_mmio_card_rom(mmio, 4),
                                    clemens_is_mmio_card_rom(mmio, 5),
                                    clemens_is_mmio_card_rom(mmio, 6),
                                    clemens_is_mmio_card_rom(mmio, 7));
      ImGui::TableNextRow();
      ImGui::TableNextColumn(); ImGui::Text("LCRDR");
      ImGui::TableNextColumn();
      if (mmio.mmap_register & CLEM_MEM_IO_MMAP_RDLCRAM) {
        ImGui::Text("RAM");
      } else {
        ImGui::Text("ROM");
      }
      ImGui::TableNextRow();
      ImGui::TableNextColumn(); ImGui::Text("LCWRI");
      ImGui::TableNextColumn();
      if (mmio.mmap_register & CLEM_MEM_IO_MMAP_WRLCRAM) {
        ImGui::Text("WRITE");
      } else {
        ImGui::Text("WPROT");
      }
      ImGui::TableNextRow();
      ImGui::TableNextColumn(); ImGui::Text("LCBNK");
      ImGui::TableNextColumn();
      if (mmio.mmap_register & CLEM_MEM_IO_MMAP_LCBANK2) {
        ImGui::Text("LC2");
      } else {
        ImGui::Text("LC1");
      }
    }
    ImGui::EndTable();
  }
  ImGui::TableNextColumn();
  {
    ImGui::BeginTable("MMIO_128K", 2, 0);
    {
      ImGui::TableNextColumn(); ImGui::Text("ALTZP");
      ImGui::TableNextColumn();
      if (mmio.mmap_register & CLEM_MEM_IO_MMAP_ALTZPLC) {
        ImGui::Text("AUX");
      } else {
        ImGui::Text("MAIN");
      }
      ImGui::TableNextColumn(); ImGui::Text("RAMRD");
      ImGui::TableNextColumn();
      if (mmio.mmap_register & CLEM_MEM_IO_MMAP_RAMRD) {
        ImGui::Text("AUX");
      } else {
        ImGui::Text("MAIN");
      }
      ImGui::TableNextColumn(); ImGui::Text("RAMWRT");
      ImGui::TableNextColumn();
      if (mmio.mmap_register & CLEM_MEM_IO_MMAP_RAMWRT) {
        ImGui::Text("AUX");
      } else {
        ImGui::Text("MAIN");
      }
    }
    ImGui::EndTable();
  }
  ImGui::TableNextColumn();
  {
    ImGui::BeginTable("MMIO_SHADOW", 2, 0);
    {
      ImGui::TableNextColumn(); ImGui::Text("A");
      ImGui::TableNextColumn();
      ImGui::Text("N/A");
      ImGui::TableNextRow();
      ImGui::TableNextColumn(); ImGui::Text("N");
      ImGui::TableNextColumn();
      ImGui::Text("N/A");
      ImGui::TableNextRow();
    }
    ImGui::EndTable();
  }
  ImGui::EndTable();
  ImGui::EndGroup();

}

void ClemensHost::doDriveBayLights(
  ClemensDrive* drives,
  int driveCount,
  int driveIndex,
  bool isEnabled,
  bool isRunning
) {
    const float kLineHeight = ImGui::GetTextLineHeightWithSpacing();
    const float kCircleRadius = ImGui::GetFontSize() * 0.5f;
    ClemensDrive* drive = drives;
    ImVec2 p0 = ImGui::GetCursorScreenPos();
    ImVec2 p = p0;
    for (int i = 0; i < driveCount; ++i) {
      ImColor color;
      if (isEnabled) {
        if (isRunning && driveIndex == i) {
          color = drive->has_disk ? 0xff0000ff : 0xff0000aa;
        } else {
          color = drive->has_disk ? 0x666666ff : 0x666666aa;
        }
      } else {
        color = drive->has_disk ? 0x333333ff : 0x333333aa;
      }
      ImGui::GetWindowDrawList()->AddCircleFilled(
        ImVec2(p.x + kCircleRadius, p.y + kCircleRadius), kCircleRadius, color);
      if (i != driveIndex) {
        ImGui::GetWindowDrawList()->AddCircle(
          ImVec2(p.x + kCircleRadius, p.y + kCircleRadius),
          kCircleRadius, ImColor(0x000000aa));
      }
      p.x += kCircleRadius * 2 + kCircleRadius * 0.25f;
    }
    ImGui::Dummy(ImVec2(p.x - p0.x, kLineHeight));
}

void ClemensHost::emulate(float deltaTime)
{
  //  execution loop for clemens 65816
  //    goal is to execute a 2.8 mhz machine, meaning execution of:
  //      2800 * 1e6 clocks per second
  //      1023 clocks per fast cycle
  //      2800 clocks per slow cycle
  //    attempt to run 2800*1e6 clocks worth of instructions per second

  cpuRegsSaved_ = machine_.cpu.regs;
  cpuPinsSaved_ = machine_.cpu.pins;
  cpu6502EmulationSaved_ = machine_.cpu.pins.emulation;

  bool is_machine_slow;
  const uint64_t kClocksPerSecond = clemens_clocks_per_second(
    &machine_, &is_machine_slow);
  const float kAdjustedDeltaTime = std::min(deltaTime, 0.1f);
  const uint64_t kClocksPerFrameDesired = kAdjustedDeltaTime * kClocksPerSecond;
  const uint64_t kClocksSpentInitial = machine_.clocks_spent;

  const time_t kEpoch1904To1970Seconds = 2082844800;
  auto epoch_time_1904 = (
    std::chrono::system_clock::to_time_t(std::chrono::system_clock::now()) +
      kEpoch1904To1970Seconds);

  clemens_rtc_set(&machine_, (unsigned)epoch_time_1904);

  machine_.cpu.cycles_spent = 0;
  while (emulationStepCount_ > 0 || isRunningEmulationUntilBreak()) {
    if (machine_.clocks_spent - kClocksSpentInitial >= kClocksPerFrameDesired) {
      // TODO: if we overflow, deduct from the budget for the next frame
      break;
    }
    if (emulationRunTarget_ <= 0x01000000) {
      if (machine_.cpu.regs.PC == (emulationRunTarget_ & 0xffff)) {
        if (machine_.cpu.regs.PBR == uint8_t(emulationRunTarget_ >> 16)) {
          emulationBreak();
          break;
        }
      }
    }
    if (!machine_.cpu.pins.resbIn) {
      if (emulationStepCount_ == 1) {
        machine_.cpu.pins.resbIn = true;
      }
    }
    clemens_emulate(&machine_);
    if (machine_.mmio_bypass) {
      //  execute simple machine I/O
      uint8_t* mainBank =  machine_.fpi_bank_map[0];
      if (simpleMachineIO_.eventKeybA2) {
        const uint8_t* keybToAscii = clemens_get_ascii_from_a2code(
          simpleMachineIO_.eventKeybA2);
        mainBank[0xff00] = keybToAscii[simpleMachineIO_.modShift ? 2 : 0];
        simpleMachineIO_.eventKeybA2 = 0;
      }
      if (mainBank[0xff01]) {
        if (simpleMachineIO_.terminalOutIndex < sizeof(simpleMachineIO_.terminalOut)) {
          simpleMachineIO_.terminalOut[simpleMachineIO_.terminalOutIndex++] = (
            mainBank[0xff01]);
          mainBank[0xff01] = 0;
        }
      }
    }
    if (!breakpoints_.empty()) {
      if (hitBreakpoint()) {
        emulationBreak();
        break;
      }
    }

    --emulationStepCount_;
    ++emulationStepCountSinceReset_;
  }
  diagnostics_.clocksSpent += (machine_.clocks_spent - kClocksSpentInitial);
  //printf("start==\n%u cycles executed\n", machine_.cpu.cycles_spent);
  machineCyclesSpentDuringSample_ += machine_.cpu.cycles_spent;
  sampleDuration_ += deltaTime;
  emulationSpeedSampled_ = machineCyclesSpentDuringSample_ / (1e6 * sampleDuration_);
  emulationRunTime_ += deltaTime;
}

bool ClemensHost::hitBreakpoint() {
  for (auto it = breakpoints_.begin(); it != breakpoints_.end(); ++it) {
    uint16_t b_adr = (uint16_t)(it->addr & 0xffff);
    uint8_t b_bank = (uint8_t)(it->addr >> 16);
    bool b_adr_hit = (machine_.cpu.regs.PBR == b_bank &&
                      machine_.cpu.regs.PC == b_adr);
    bool b_data_hit = (machine_.cpu.regs.DBR == b_bank &&
                       machine_.cpu.pins.adr == b_adr);
    if (b_adr_hit) {
      switch (it->op) {
        case Breakpoint::PC:
          return true;
      }
    }
    if (b_data_hit) {
      switch (it->op) {
        case Breakpoint::Read:
          if (machine_.cpu.pins.rwbOut && machine_.cpu.pins.vdaOut) {
            return true;
          }
          break;
        case Breakpoint::Write:
          if (!machine_.cpu.pins.rwbOut && machine_.cpu.pins.vdaOut) {
            return true;
          }
          break;
      }
    }
  }
  return false;
}

static const char* trimCommand(const char* buffer)
{
  const char* start = buffer;
  while (*start && std::isspace(*start)) {
    ++start;
  }
  return *start ? start : nullptr;
}

template<typename Cb> static bool parseCommandToken(
  const char* line,
  Cb cb
) {
  const char* start = line;
  const char* cur = line;
  while (cur - line < 256) {
    char ch = *cur;
    if (!ch || std::isspace(ch)) {
      return cb(start, cur);
    }
    if (!ch) break;
    ++cur;
  }

  return true;
}

bool ClemensHost::parseCommand(const char* buffer)
{
  //  TODO: limit commands if we're mid-emulation (i.e. power, break, reset)
  //  '.power on|off'
  //  '.reset'
  //  'step <count=1>'
  //  'print <entry>'
  const char* start = trimCommand(buffer);
  if (!start) return true;
  return parseCommandToken(start,
    [this](const char* start, const char* end) {
      if (!strncasecmp(start, ".power", end - start)) {
        return parseCommandPower(end);
      } else if (!strncasecmp(start, ".reset", end - start)) {
        return parseCommandReset(end);
      } else if (!strncasecmp(start, ".status", end - start)) {
        return parseCommandDebugStatus(end);
      } else if (!strncasecmp(start, ".load", end - start)) {
        return parseCommandLoad(end);
      } else if (!strncasecmp(start, ".save", end - start)) {
        return parseCommandSave(end);
      } else if (!strncasecmp(start, ".disk", end - start)) {
        return parseCommandDisk(end);
      } else if (!strncasecmp(start, "step", end - start) ||
                 !strncasecmp(start, "s", end - start)) {
        return parseCommandStep(end);
      } else if (!strncasecmp(start, "stepover", end - start) ||
                 !strncasecmp(start, "so", end - start)) {
        return parseCommandStepOver(end);
      } else if (!strncasecmp(start, "run", end - start) ||
                 !strncasecmp(start, "r", end - start)) {
        return parseCommandRun(end);
      }  else if (!strncasecmp(start, "break", end - start) ||
                 !strncasecmp(start, "b", end - start)) {
        return parseCommandBreak(end);
      } else if (!strncasecmp(start, "lbreak", end-start) ||
               !strncasecmp(start, "lb", end - start)) {
        return parseCommandListBreak(end);
      } else if (!strncasecmp(start, "rbreak", end-start) ||
               !strncasecmp(start, "rb", end - start)) {
        return parseCommandRemoveBreak(end);
      } else if (!strncasecmp(start, "clear", end - start) ||
                 !strncasecmp(start, "c", end - start)) {
        // clear some UI states
        executedInstructions_.clear();
      } else if (!strncasecmp(start, "log", end - start)) {
        return parseCommandLog(end);
      } else if (!strncasecmp(start, "unlog", end - start)) {
        return parseCommandUnlog(end);
      } else if (!strncasecmp(start, "context", end - start)) {
        return parseCommandDebugContext(end);
      } else if (!strncasecmp(start, "set", end - start)) {
        return parseCommandSetValue(end);
      } else if (!strncasecmp(start, "dump", end - start)) {
        return parseCommandDump(end);
      }
      return false;
    });
}

bool ClemensHost::parseCommandPower(const char* line)
{
  const char* start = trimCommand(line);
  if (clemens_is_initialized_simple(&machine_)) {
    destroyMachine();
    return true;
  }

  if (!start) {
    //  assume toggle power
    return createMachine("gs_rom_3.rom", MachineType::Apple2GS);
  }

 return parseCommandToken(start,
    [this](const char* start, const char* end) {
      char machineName[256];
      memset(machineName, 0, sizeof(machineName));
      strncpy(machineName, start, std::min(sizeof(machineName) - 1, size_t(end - start)));
      return createMachine(machineName, MachineType::Simple128K);
    });

  return false;
}

bool ClemensHost::parseCommandReset(const char* line)
{
  const char* start = trimCommand(line);
  if (!start) {
    if (!clemens_is_initialized_simple(&machine_)) {
      CLEM_HOST_COUT.format("Machine not powered on.");
      return false;
    }
    resetMachine();
    return true;
  }
  return false;
}

bool ClemensHost::parseCommandLoad(const char* line)
{
  const char* start = trimCommand(line);
  if (!start) {
    return loadState("save.clemulate");
  }
  return false;
}

bool ClemensHost::parseCommandSave(const char* line)
{
  const char* start = trimCommand(line);
  if (!start) {
    return saveState("save.clemulate");
  }
  return false;
}

bool ClemensHost::parseCommandDisk(const char* line)
{
  const char* start = trimCommand(line);
  if (!start) {
    for (unsigned i = 0; i < disks35_.size(); ++i) {
      CLEM_HOST_COUT.format("S5.D{}: {}", i+1,
                            disks35_[i].path.empty() ? "<none>": disks35_[i].path);
    }
    for (unsigned i = 0; i < disks525_.size(); ++i) {
      CLEM_HOST_COUT.format("S6.D{}: {}", i+1,
                             disks525_[i].path.empty() ? "<none>": disks525_[i].path);
    }
    return true;
  }

  return parseCommandToken(start,
    [this](const char* start, const char* end) {
      char slot[32];
      strncpy(slot, start, std::min(sizeof(slot) - 1, size_t(end - start)));

      ClemensDriveType driveType = kClemensDrive_Invalid;
      if (slot[1] == '.') {
        if (slot[0] == '5') {
          if (slot[2] == '1') driveType = kClemensDrive_3_5_D1;
          else if (slot[2] == '2') driveType = kClemensDrive_3_5_D2;
        } else if (slot[0] == '6') {
          if (slot[2] == '1') driveType = kClemensDrive_5_25_D1;
          else if (slot[2] == '2') driveType = kClemensDrive_5_25_D2;
        }
      }
      if (driveType == kClemensDrive_Invalid) {
        CLEM_HOST_COUT.format("Command requires a <slot>.<drive> parameter");
        return false;
      }

      return loadDisk(driveType, end);
    });
}

bool ClemensHost::parseCommandDebugStatus(const char* line)
{
  const char* start = trimCommand(line);
  if (!start) {
    if (!clemens_is_initialized(&machine_)) {
      CLEM_HOST_COUT.format("Machine not powered on.");
      return false;
    }
    clemens_debug_status(&machine_);
    return true;
  }
  return false;
}


bool ClemensHost::parseCommandStep(const char* line)
{
  const char* start = trimCommand(line);
  if (!clemens_is_initialized(&machine_)) {
    CLEM_HOST_COUT.format("Machine not powered on.");
    return false;
  }
  emulationBreak();
  if (!start) {
    stepMachine(1);
    return true;
  }
  return parseCommandToken(start,
    [this](const char* start, const char* end) {
      char number[16];
      strncpy(number, start, std::min(sizeof(number) - 1, size_t(end - start)));
      number[15] = '\0';
      stepMachine(strtol(number, nullptr, 10));
      return true;
    });
}

bool ClemensHost::parseCommandStepOver(const char* line)
{
  const char* start = trimCommand(line);
  if (!clemens_is_initialized(&machine_)) {
    CLEM_HOST_COUT.format("Machine not powered on.");
    return false;
  }
  emulationBreak();
  if (!start) {
    //  run to next instruction after a JSR
    //  TODO: detect if next instruction is a JSR! or JSL!  otherwise just
    //        treat as a step.
    unsigned runto = (machine_.cpu.regs.PBR << 16) | (machine_.cpu.regs.PC + 3);
    return emulationRun(runto);
  }
  return false;
}

bool ClemensHost::parseCommandBreak(const char* line) {
  const char* start = trimCommand(line);
  if (!clemens_is_initialized(&machine_)) {
    CLEM_HOST_COUT.format("Machine not powered on.");
    return false;
  }
  if (!start) {
    emulationBreak();
    return true;
  }
  return parseCommandToken(start,
    [this](const char* start, const char* end) {
      char number[16];
      strncpy(number, start, std::min(sizeof(number) - 1, size_t(end - start)));
      number[15] = '\0';
      if (size_t(end - start) > 2) {
        unsigned addr;
        if (number[0] == 'r' && number[1] == '@') {
          addr = strtoul(&number[2], nullptr, 16);
          breakpoints_.emplace_back(Breakpoint { Breakpoint::Read, addr });
        } else if (number[0] == 'w' && number[1] == '@') {
          addr = strtoul(&number[2], nullptr, 16);
          breakpoints_.emplace_back(Breakpoint { Breakpoint::Write, addr });
        } else {
          addr = strtoul(&number[0], nullptr, 16);
          breakpoints_.emplace_back(Breakpoint { Breakpoint::PC, addr });
        }
        return true;
      }
      return false;
    });
}

bool ClemensHost::parseCommandListBreak(const char* line) {
  const char* start = trimCommand(line);
  FormatView fv(terminalOutput_);
  if (breakpoints_.empty()) {
    fv.format("No breakpoints.");
  } else {
    int i = 0;
    for (auto& breakpoint : breakpoints_) {
      fv.format("{0}: {1} @ {2:x}", i,
        breakpoint.op == Breakpoint::PC ? "PC" :
        breakpoint.op == Breakpoint::Read ? "Rd" :
        breakpoint.op == Breakpoint::Write ? "Wr" : "??",
        breakpoint.addr);
    }
  }

  return true;
}

bool ClemensHost::parseCommandRemoveBreak(const char* line) {
  const char* start = trimCommand(line);
  if (!start) {
    breakpoints_.clear();
    CLEM_HOST_COUT.format("Breakpoints cleared");
    return true;
  }
  return parseCommandToken(start,
    [this](const char* start, const char* end) {
      char number[16];
      strncpy(number, start, std::min(sizeof(number) - 1, size_t(end - start)));
      number[15] = '\0';
      //  TODO
      return false;
    });
}

bool ClemensHost::parseCommandRun(const char* line)
{
  const char* start = trimCommand(line);
  if (!clemens_is_initialized(&machine_)) {
    CLEM_HOST_COUT.format("Machine not powered on.");
    return false;
  }
  if (!start) {
    emulationStepCount_ = 0;
    emulationRun(kEmulationRunForever);
    return true;
  }
  return parseCommandToken(start,
    [this](const char* start, const char* end) {
      char number[16];
      strncpy(number, start, std::min(sizeof(number) - 1, size_t(end - start)));
      number[15] = '\0';
      return emulationRun(strtoul(number, nullptr, 16));
    });
}

bool ClemensHost::parseCommandLog(const char* line)
{
  const char* start = trimCommand(line);
  if (!clemens_is_initialized(&machine_)) {
    CLEM_HOST_COUT.format("Machine not powered on.");
    return false;
  }
  if (!start) {
    CLEM_HOST_COUT.format("usage: log <type>");
    return false;
  }
  return parseCommandToken(start,
    [this](const char* start, const char* end) {
      char name[16];
      strncpy(name, start, std::min(sizeof(name) - 1, size_t(end - start)));
      name[end - start] = '\0';
      if (!strncasecmp(name, "iwm", sizeof(name))) {
        machine_.mmio.dev_iwm.enable_debug = true;
        return true;
      }
      if (!strncasecmp(name, "opcode", sizeof(name))) {
        machine_.debug_flags |= (
          kClemensDebugFlag_StdoutOpcode | kClemensDebugFlag_DebugLogOpcode);
        return true;
      }
      if (!strncasecmp(name, "trace", sizeof(name))) {
        if (programTrace_) {
          CLEM_HOST_COUT.format("trace already active");
          return false;
        }
        programTrace_ = std::make_unique<ClemensProgramTrace>();
        clemens_opcode_callback(&machine_, &ClemensHost::emulatorOpcodePrint, this);
        return true;
      }
      return false;
    });
}

bool ClemensHost::parseCommandUnlog(const char* line)
{
  const char* start = trimCommand(line);
  if (!clemens_is_initialized(&machine_)) {
    CLEM_HOST_COUT.format("Machine not powered on.");
    return false;
  }
  if (!start) {
    machine_.mmio.dev_iwm.enable_debug = false;
    machine_.debug_flags &= ~(
       kClemensDebugFlag_StdoutOpcode | kClemensDebugFlag_DebugLogOpcode);
    clem_debug_log_flush();
    return true;
  }
  return parseCommandToken(start,
    [this](const char* start, const char* end) {
      char name[16];
      strncpy(name, start, std::min(sizeof(name) - 1, size_t(end - start)));
      name[end - start] = '\0';
      if (!strncasecmp(name, "iwm", sizeof(name))) {
        machine_.mmio.dev_iwm.enable_debug = false;
        clem_debug_log_flush();
        return true;
      }
      if (!strncasecmp(name, "opcode", sizeof(name))) {
        machine_.debug_flags &= ~(
          kClemensDebugFlag_StdoutOpcode | kClemensDebugFlag_DebugLogOpcode);
        clem_debug_log_flush();
        return true;
      }
      if (!strncasecmp(name, "trace", sizeof(name))) {
        if (!programTrace_) {
          CLEM_HOST_COUT.format("trace not active");
          return false;
        }
        programTrace_->exportTrace("trace.out");
        if (isRunningEmulationUntilBreak()) {
          // don't log
          clemens_opcode_callback(&machine_, NULL, this);
        }
        programTrace_ = nullptr;
        return true;
      }
      return false;
    });
}

bool ClemensHost::parseCommandDebugContext(const char* line)
{
  const char* start = trimCommand(line);
  if (!start) {
    CLEM_HOST_COUT.format("usage: context <iwm|mmap>");
    return false;
  }
  return parseCommandToken(start,
    [this](const char* start, const char* end) {
      char name[16];
      strncpy(name, start, std::min(sizeof(name) - 1, size_t(end - start)));
      name[end - start] = '\0';
      if (!strncasecmp(name, "iwm", sizeof(name))) {
        widgetDebugContext_ = DebugContext::IWM;
        return true;
      }
      if (!strncasecmp(name, "mmap", sizeof(name))) {
        widgetDebugContext_ = DebugContext::MemoryMaps;
        return true;
      }
      return false;
    });
}

bool ClemensHost::parseCommandSetValue(const char* line)
{
  const char* start = trimCommand(line);
  if (!start) {
    CLEM_HOST_COUT.format("usage: set <a|x|y|pc> <value>");
    return false;
  }
  return parseCommandToken(start,
    [this](const char* start, const char* end) {
      unsigned value;
      if (!parseImmediateValue(value, end)) {
        return false;
      }

      char name[16];
      strncpy(name, start, std::min(sizeof(name) - 1, size_t(end - start)));
      name[end - start] = '\0';
      if (!strncasecmp(name, "a", sizeof(name))) {
        machine_.cpu.regs.A = (uint16_t)(value & 0xffff);
        return true;
      } else if (!strncasecmp(name, "x", sizeof(name))) {
        machine_.cpu.regs.X = (uint16_t)(value & 0xffff);
        return true;
      } else if (!strncasecmp(name, "y", sizeof(name))) {
        machine_.cpu.regs.Y = (uint16_t)(value & 0xffff);
        return true;
      } else if (!strncasecmp(name, "pc", sizeof(name))) {
        machine_.cpu.regs.PC = (uint16_t)(value & 0xffff);
        return true;
      }
      return false;
    });
}

bool ClemensHost::parseCommandDump(const char* line)
{
  const char* start = trimCommand(line);
  if (!start) {
    CLEM_HOST_COUT.format("usage: dump <bank> <name>");
    return false;
  }
  return parseCommandToken(start,
    [this](const char* start, const char* end) {
      char name[16];
      strncpy(name, start, std::min(sizeof(name) - 1, size_t(end - start)));
      name[end - start] = '\0';
      unsigned bank = (unsigned)strtoul(name, NULL, 16);
      if (bank >= 256) return false;

      dumpMemory(bank, "");
      return true;
    });
}

bool ClemensHost::parseImmediateValue(unsigned& value, const char* line)
{
  const char* start = trimCommand(line);
  if (!start) {
    CLEM_HOST_COUT.format("requires an immediate value");
    return false;
  }
  return parseCommandToken(start,
    [this, &value](const char* start, const char* end) {
      char name[16];
      strncpy(name, start, std::min(sizeof(name) - 1, size_t(end - start)));
      name[end - start] = '\0';
      if (name[0] == '$') {
        value = (unsigned)(strtoul(name + 1, NULL, 16));
      } else {
        value = (unsigned)(strtoul(name, NULL, 10));
      }
      return true;
    });
}

bool ClemensHost::parseImmediateString(std::string& value, const char* line)
{
  const char* start = trimCommand(line);
  if (!start) {
    CLEM_HOST_COUT.format("requires an immediate value");
    return false;
  }
  return parseCommandToken(start,
    [this, &value](const char* start, const char* end) {
      char name[256];
      strncpy(name, start, std::min(sizeof(name) - 1, size_t(end - start)));
      name[end - start] = '\0';
      value.assign(name);
      return true;
    });
}

bool ClemensHost::createMachine(const char* filename, MachineType machineType)
{
  bool success = false;
  if (clemens_is_initialized_simple(&machine_)) {
    return success;
  }
  slab_.reset();

 // Apple II line (including Mega2) was 1.023mhz
  // A Mega2 cycle is equivalent to 1 full cycle of the 2.8mhz clock
  // TODO: allows 1.023, 2.8, 8mhz without any loss due to integer div
  const uint32_t kClocksPerFastCycle = CLEM_CLOCKS_FAST_CYCLE;
  const uint32_t kClocksPerSlowCycle = CLEM_CLOCKS_MEGA2_CYCLE;

  std::ifstream romFileStream(filename, std::ios::binary | std::ios::ate);
  void* romMemory = NULL;
  unsigned romMemorySize = 0;

  if (romFileStream.is_open()) {
    romMemorySize = unsigned(romFileStream.tellg());
    romMemory = slab_.allocate(romMemorySize);
    romFileStream.seekg(0, std::ios::beg);
    romFileStream.read((char*)romMemory, romMemorySize);
    romFileStream.close();
  }
  if (!romMemory) {
    CLEM_HOST_COUT.format("{} not found", filename);
    return false;
  }

  machine_.logger_fn = &ClemensHost::emulatorLog;

  switch (machineType) {
    case MachineType::Apple2GS: {
      const unsigned fpiBankCount = CLEM_IIGS_FPI_MAIN_RAM_BANK_COUNT;
      clemens_init(&machine_, kClocksPerSlowCycle, kClocksPerFastCycle,
                  romMemory, romMemorySize,
                  slab_.allocate(CLEM_IIGS_BANK_SIZE),
                  slab_.allocate(CLEM_IIGS_BANK_SIZE),
                  slab_.allocate(CLEM_IIGS_BANK_SIZE * fpiBankCount),
                  slab_.allocate(2048 * 7),
                  fpiBankCount);

      loadBRAM();

      ClemensAudioMixBuffer mix_buffer;
      mix_buffer.frames_per_second = audio_->getAudioFrequency();
      mix_buffer.frame_count = mix_buffer.frames_per_second / 20;
      mix_buffer.stride = 8;    //   2 channels, 16-bits per channel
      mix_buffer.data = (uint8_t*)(
        slab_.allocate(mix_buffer.frame_count * mix_buffer.stride));
      clemens_assign_audio_mix_buffer(&machine_, &mix_buffer);
      loadDisks();
      success = true;
    } break;
    case MachineType::Simple128K: {
      const unsigned fpiBankCount = 2;
      clemens_simple_init(&machine_,  kClocksPerSlowCycle, kClocksPerFastCycle,
                          slab_.allocate(CLEM_IIGS_BANK_SIZE * fpiBankCount),
                          fpiBankCount);

      auto* page_map = &simpleDirectPageMap_;
      page_map->shadow_map = NULL;
      for (unsigned pageIdx = 0x00; pageIdx < 0x100; ++pageIdx) {
        clemens_create_page_mapping(&page_map->pages[pageIdx], pageIdx, 0x00, 0x00);
        page_map->pages[pageIdx].flags |= CLEM_MEM_PAGE_DIRECT_FLAG;
      }
      for (unsigned bankIdx = 0; bankIdx < fpiBankCount; ++bankIdx) {
        machine_.bank_page_map[bankIdx] = page_map;
      }

      //  load in the hex image for our machine
      auto* hexMemory = reinterpret_cast<char*>(romMemory);
      success = clemens_load_hex(
        &machine_, hexMemory, hexMemory + romMemorySize, 0x00);
      if (!success) {
        CLEM_HOST_COUT.format("Failed to ingest hex data from {}", filename);
      }

      simpleMachineIO_ = SimpleMachineIO {};
    } break;
  }

  clemens_opcode_callback(&machine_, &ClemensHost::emulatorOpcodePrint, this);

  memoryViewBank_[0] = 0x00;
  memoryViewBank_[1] = 0x00;

  resetMachine();
  return success;
}

void ClemensHost::destroyMachine()
{
  if (!clemens_is_initialized_simple(&machine_)) {
    return;
  }
  emulationBreak();
  memset(&machine_, 0, sizeof(ClemensMachine));
}

void ClemensHost::resetMachine()
{
  //  low signal indicates reset
  //  step 1: reset start, pull up resbIn
  //  step 2: reset end, issue interrupt
  machine_.cpu.pins.resbIn = false;   // low signal indicates reset
  emulationStepCountSinceReset_ = 0;
  diagnostics_.reset();
  stepMachine(2);
}

bool ClemensHost::saveState(const char* filename)
{
  if (!clemens_is_initialized_simple(&machine_)) {
    return false;
  }
  mpack_writer_t writer;
  //  this save buffer is probably, unnecessarily big - but it's just used for
  //  saves and freed
  //
  //  {
  //    machine state
  //    bram blob
  //    disk[ { woz/2img, path }]
  //  }
  //

  mpack_writer_init_filename(&writer, filename);
  mpack_build_map(&writer);
  mpack_write_cstr(&writer, "machine");
  clemens_serialize_machine(&writer, &machine_);
  mpack_write_cstr(&writer, "bram");
  mpack_write_bin(&writer, (char *)clemens_rtc_get_bram(&machine_, NULL),
                  CLEM_RTC_BRAM_SIZE);
  mpack_write_cstr(&writer, "disks");
  mpack_build_array(&writer);
  saveDiskMetadata(&writer, disks525_[0]);
  saveDiskMetadata(&writer, disks525_[1]);
  saveDiskMetadata(&writer, disks35_[0]);
  saveDiskMetadata(&writer, disks35_[1]);
  mpack_complete_array(&writer);
  mpack_complete_map(&writer);
  mpack_writer_destroy(&writer);
  return true;
}

uint8_t* ClemensHost::unserializeAllocate(unsigned sz, void* context)
{
  auto* host = reinterpret_cast<ClemensHost*>(context);
  return (uint8_t*)host->slab_.allocate(sz);
}


bool ClemensHost::loadState(const char* filename)
{
  if (!clemens_is_initialized_simple(&machine_)) {
    //  power on and load state
  }
  char str[256];
  mpack_reader_t reader;
  mpack_reader_init_filename(&reader, filename);
  mpack_expect_map(&reader);
  mpack_expect_cstr(&reader, str, sizeof(str));
  if (!clemens_unserialize_machine(
          &reader,
          &machine_,
          &ClemensHost::unserializeAllocate,
          this)
  ) {
    // power off the machine
    mpack_reader_destroy(&reader);
    return false;
  }
  mpack_expect_cstr(&reader, str, sizeof(str));
  if (mpack_expect_bin(&reader) == CLEM_RTC_BRAM_SIZE) {
    mpack_read_bytes(&reader, (char *)machine_.mmio.dev_rtc.bram,
                     CLEM_RTC_BRAM_SIZE);
  }
  mpack_done_bin(&reader);
  clemens_rtc_set_bram_dirty(&machine_);

  //  load woz filenames - the actual images have already been
  //  unserialized inside clemens_unserialize_machine
  mpack_expect_cstr(&reader, str, sizeof(str));
  mpack_expect_array(&reader);

  loadDiskMetadata(&reader, disks525_[0]);
  loadDiskMetadata(&reader, disks525_[1]);
  loadDiskMetadata(&reader, disks35_[0]);
  loadDiskMetadata(&reader, disks35_[1]);

  mpack_done_array(&reader);
  mpack_done_map(&reader);
  mpack_reader_destroy(&reader);

  saveBRAM();

  return true;
}

void ClemensHost::saveBRAM()
{
  bool isDirty = false;
  const uint8_t* bram = clemens_rtc_get_bram(&machine_, &isDirty);
  if (!isDirty) return;

  std::ofstream bramFile("clem.bram", std::ios::binary);
  if (bramFile.is_open()) {
    bramFile.write((char *)machine_.mmio.dev_rtc.bram, CLEM_RTC_BRAM_SIZE);
  } else {
    //  TODO: display error?
  }
}

void ClemensHost::loadBRAM()
{
  std::ifstream bramFile("clem.bram", std::ios::binary);
  if (bramFile.is_open()) {
    bramFile.read((char *)machine_.mmio.dev_rtc.bram, CLEM_RTC_BRAM_SIZE);
  } else {
    //  TODO: display warning?
  }
}

void ClemensHost::saveDiskMetadata(mpack_writer_t* writer, const ClemensDisk& disk)
{
  mpack_build_map(writer);
  mpack_write_cstr(writer, "path");
  mpack_write_cstr(writer, disk.path.c_str());
  mpack_write_cstr(writer, "brand");
  switch (disk.diskBrand) {
    case ClemensDisk::None:
      mpack_write_str(writer, "none", 4);
      break;
    case ClemensDisk::WOZ:
      mpack_write_str(writer, "woz2", 4);
      mpack_write_cstr(writer, "disk_type");
      mpack_write_u32(writer, disk.dataWOZ.disk_type);
      mpack_write_cstr(writer, "boot_type");
      mpack_write_u32(writer, disk.dataWOZ.boot_type);
      mpack_write_cstr(writer, "flags");
      mpack_write_u32(writer, disk.dataWOZ.flags);
      mpack_write_cstr(writer, "required_ram_kb");
      mpack_write_u32(writer, disk.dataWOZ.required_ram_kb);
      mpack_write_cstr(writer, "max_track_size_bytes");
      mpack_write_u32(writer, disk.dataWOZ.max_track_size_bytes);
      mpack_write_cstr(writer, "creator");
      mpack_write_str(writer, disk.dataWOZ.creator, sizeof(disk.dataWOZ.creator));
      break;
    case ClemensDisk::IMG2:
      mpack_write_str(writer, "2img", 4);
      mpack_write_cstr(writer, "creator");
      mpack_write_str(writer, disk.data2IMG.creator, 4);
      mpack_write_cstr(writer, "version");
      mpack_write_u32(writer, disk.data2IMG.version);
      mpack_write_cstr(writer, "format");
      mpack_write_u32(writer, disk.data2IMG.format);
      mpack_write_cstr(writer, "dos_volume");
      mpack_write_u32(writer, disk.data2IMG.dos_volume);
      mpack_write_cstr(writer, "block_count");
      mpack_write_u32(writer, disk.data2IMG.block_count);
      mpack_write_cstr(writer, "comment");
      if (disk.data2IMG.comment) {
        mpack_write_str(writer, disk.data2IMG.comment,
                        disk.data2IMG.comment_end - disk.data2IMG.comment);
      } else {
        mpack_write_nil(writer);
      }
      mpack_write_cstr(writer, "creator_data");
      if (disk.data2IMG.creator_data) {
        mpack_write_bin(writer, (char *)disk.data2IMG.creator_data,
                        disk.data2IMG.creator_data_end - disk.data2IMG.creator_data);
      } else {
        mpack_write_nil(writer);
      }
      break;
  }
  mpack_complete_map(writer);
}

void ClemensHost::loadDiskMetadata(mpack_reader_t* reader, ClemensDisk& disk)
{
  char value[1024];
  mpack_expect_map(reader);
  mpack_expect_cstr_match(reader, "path");
  mpack_expect_cstr(reader, value, sizeof(value));
  disk.path = value;
  mpack_expect_cstr_match(reader, "brand");
  mpack_expect_str_buf(reader, value, 4);
  if (!strncasecmp(value, "none", 4)) {
    disk.diskBrand = ClemensDisk::None;
  } else if (!strncasecmp(value, "woz2", 4)) {
    disk.diskBrand = ClemensDisk::WOZ;
  } else if (!strncasecmp(value, "2img", 4)) {
    disk.diskBrand = ClemensDisk::IMG2;
  } else {
    disk.diskBrand = ClemensDisk::None;
  }
  switch (disk.diskBrand) {
    case ClemensDisk::None:
      break;
    case ClemensDisk::WOZ:
      mpack_expect_cstr_match(reader, "disk_type");
      disk.dataWOZ.disk_type = mpack_expect_u32(reader);
      mpack_expect_cstr_match(reader, "boot_type");
      disk.dataWOZ.boot_type = mpack_expect_u32(reader);
      mpack_expect_cstr_match(reader, "flags");
      disk.dataWOZ.flags = mpack_expect_u32(reader);
      mpack_expect_cstr_match(reader, "required_ram_kb");
      disk.dataWOZ.required_ram_kb = mpack_expect_u32(reader);
      mpack_expect_cstr_match(reader, "max_track_size_bytes");
      disk.dataWOZ.max_track_size_bytes = mpack_expect_u32(reader);
      mpack_expect_cstr_match(reader, "creator");
      mpack_expect_str_buf(reader, disk.dataWOZ.creator, sizeof(disk.dataWOZ.creator));
      break;
    case ClemensDisk::IMG2:
      break;
  }

  mpack_done_map(reader);
}

void ClemensHost::stepMachine(int stepCount)
{
  emulationStepCount_ = std::max(0, stepCount);
  emulationRunTime_ = 0.0f;
  emulationSliceDuration_ = 1/30.0f;
  emulationSliceTimeLeft_ = 0.0f;
  emulationSpeedSampled_ = 0.0f;
  machineCyclesSpentDuringSample_ = 0;
  sampleDuration_ = 0.0f;
  clemens_opcode_callback(&machine_, &ClemensHost::emulatorOpcodePrint, this);
}

bool ClemensHost::emulationRun(unsigned target) {
  emulationRunTarget_ = target;
  emulationStepCount_ = 0;
  if (emulationRunTarget_ > 0x00ffffff) {
    return false;
  }
  if (!programTrace_) {
    clemens_opcode_callback(&machine_, NULL, this);
  }
  return true;
}

void ClemensHost::emulationBreak()
{
  emulationRunTarget_ = kEmulationRunTargetNone;
  emulationStepCount_ = 0;
}

bool ClemensHost::isRunningEmulation() const
{
  return (emulationStepCount_ > 0 || isRunningEmulationUntilBreak());
}

bool ClemensHost::isRunningEmulationStep() const
{
  return isRunningEmulation() && emulationStepCount_ > 0;
}

bool ClemensHost::isRunningEmulationUntilBreak() const {
  return emulationRunTarget_ != kEmulationRunTargetNone;
}

void ClemensHost::emulatorOpcodePrint(
  struct ClemensInstruction* inst,
  const char* operand,
  void* this_ptr
) {
  auto* host = reinterpret_cast<ClemensHost*>(this_ptr);

  if (!host->isRunningEmulationUntilBreak()) {
    //  don't display run history while running continously
    constexpr size_t kInstructionEdgeSize = 64;
    if (host->executedInstructions_.size() > 128 + kInstructionEdgeSize) {
      auto it = host->executedInstructions_.begin();
      host->executedInstructions_.erase(it, it + kInstructionEdgeSize);
    }
    host->executedInstructions_.emplace_back();
    auto& instruction = host->executedInstructions_.back();
    instruction.fromInstruction(*inst, operand);
  }
  if (host->programTrace_) {
    host->programTrace_->addExecutedInstruction(*inst, operand, host->machine_);
  }
}

void ClemensHost::dumpMemory(unsigned bank, const char* filename)
{
  if (!clemens_is_initialized_simple(&machine_)) {
    CLEM_HOST_COUT.format("Machine not powered on");
    return;
  }

  std::string dumpFilePath;
  if (!filename || !filename[0]) {
    dumpFilePath = fmt::format("memory_{:02X}.txt", bank);
  }

  std::ofstream dumpFile(dumpFilePath, std::ios::binary);
  constexpr unsigned kHexByteCountPerLine = 64;
  constexpr unsigned kByteCountPerLine = 6 + kHexByteCountPerLine * 2;
  char* hexDump = new char[kByteCountPerLine + 1];
  unsigned adr = 0x0000;

  while (adr < 0x10000) {
    snprintf(hexDump, kByteCountPerLine + 1, "%04X: ", adr);
    clemens_out_hex_data_body(
      &machine_, hexDump + 6, kHexByteCountPerLine * 2, bank, adr);
    hexDump[kByteCountPerLine] = '\n';
    dumpFile.write(hexDump, kByteCountPerLine + 1);
    adr += 0x40;
  }
  delete[] hexDump;
}

///////////////////////////////////////////////////////////////////////////////
// Disk Management

void ClemensHost::loadDisks()
{
  clemens_assign_disk(&machine_, kClemensDrive_5_25_D1, &disks525_[0].nib);
  clemens_assign_disk(&machine_, kClemensDrive_5_25_D2, &disks525_[1].nib);
  clemens_assign_disk(&machine_, kClemensDrive_3_5_D1, &disks35_[0].nib);
  clemens_assign_disk(&machine_, kClemensDrive_3_5_D2, &disks35_[1].nib);
}

bool ClemensHost::loadDisk(ClemensDriveType driveType, const char* filename)
{
  ClemensDisk* disk = nullptr;
  unsigned diskType = CLEM_DISK_TYPE_NONE;
  bool doubleSided = false;
  int driveIndex = -1;

  switch (driveType) {
    case kClemensDrive_5_25_D1:
      disk = &disks525_[0];
      diskType = CLEM_DISK_TYPE_5_25;
      driveIndex = 0;
      break;
    case kClemensDrive_5_25_D2:
      disk = &disks525_[1];
      diskType = CLEM_DISK_TYPE_5_25;
      driveIndex = 1;
      break;
    case kClemensDrive_3_5_D1:
      disk = &disks35_[0];
      diskType = CLEM_DISK_TYPE_3_5;
      driveIndex = 0;
      doubleSided = true;
      break;
    case kClemensDrive_3_5_D2:
      disk = &disks35_[1];
      diskType = CLEM_DISK_TYPE_3_5;
      driveIndex = 1;
      doubleSided = true;
      break;
  }
  if (!disk) return false;

  while (std::isspace(*filename)) ++filename;

  disk->diskBrand = ClemensDisk::None;

  std::string filename_ext = filename;
  auto filename_ext_pos = filename_ext.find_last_of('.');
  if (filename_ext_pos != std::string::npos) {
    if (filename_ext.compare(filename_ext_pos + 1, std::string::npos,
                             "woz") == 0) {
      disk->diskBrand = ClemensDisk::WOZ;
    } else if (filename_ext.compare(filename_ext_pos + 1, std::string::npos,
                                    "2mg") == 0) {
      disk->diskBrand = ClemensDisk::IMG2;
    }
  }

  struct stat fileStat;
  bool isOk = false;
  if (stat(filename, &fileStat) != 0) {
    disk->nib.disk_type = diskType;
    disk->nib.is_double_sided = doubleSided;
    isOk = createBlankDisk(&disk->nib);
  } else {
    switch (disk->diskBrand) {
      case ClemensDisk::WOZ:
        disk->dataWOZ.nib = &disk->nib;
        isOk = loadWOZDisk(filename, &disk->dataWOZ, driveType);
        break;
      case ClemensDisk::IMG2:
        disk->data2IMG.nib =  &disk->nib;
        isOk = load2IMGDisk(filename, &disk->data2IMG, driveType);
        break;
    }
  }
  if (isOk) {
    clemens_assign_disk(&machine_, driveType, &disk->nib);
    disk->path = filename;
  } else {
    clemens_eject_disk(&machine_, driveType);
    disk->path.clear();
  }

  return isOk;
}


bool ClemensHost::loadWOZDisk(
  const char* filename,
  struct ClemensWOZDisk* woz,
  ClemensDriveType driveType
) {
  std::ifstream wozFile(filename, std::ios::binary | std::ios::ate);
  if (!wozFile.is_open()) {
    return false;
  }
  unsigned dataSize = unsigned(wozFile.tellg());
  uint8_t* data = new uint8_t[dataSize];
  wozFile.seekg(0, std::ios::beg);
  wozFile.read((char *)data, dataSize);
  wozFile.close();

  const uint8_t* current = clem_woz_check_header(data, dataSize);
  const uint8_t* end = data + dataSize;

  struct ClemensWOZChunkHeader chunkHeader;

  while ((
    current = clem_woz_parse_chunk_header(
                  &chunkHeader,
                  current,
                  end - current)
    ) != nullptr
  ) {
    switch (chunkHeader.type) {
      case CLEM_WOZ_CHUNK_INFO:
        current = clem_woz_parse_info_chunk(
          woz, &chunkHeader, current, chunkHeader.data_size);
        break;
      case CLEM_WOZ_CHUNK_TMAP:
        current = clem_woz_parse_tmap_chunk(
          woz, &chunkHeader, current, chunkHeader.data_size);
        break;
      case CLEM_WOZ_CHUNK_TRKS:
        current = clem_woz_parse_trks_chunk(
          woz, &chunkHeader, current, chunkHeader.data_size);
        break;
      case CLEM_WOZ_CHUNK_WRIT:
        break;
      case CLEM_WOZ_CHUNK_META:
        current = clem_woz_parse_meta_chunk(
          woz, &chunkHeader, current, chunkHeader.data_size);
        // skip for now
        break;
      default:
        break;
    }
  }

  if (woz->flags & CLEM_WOZ_IMAGE_WRITE_PROTECT) {
    printf("WOZ is write protected\n");
  } else {
    printf("WOZ is NOT write protected\n");
  }
  for (unsigned i = 0; i < woz->nib->track_count; ++i) {
    printf("WOZ Track %u: %u bits\n", i, woz->nib->track_bits_count[i]);
  }

  delete[] data;

  return true;
}

bool ClemensHost::createBlankDisk(struct ClemensNibbleDisk* disk)
{
  //  This method creates a very basic nibblized disk with many assumptions.
  //  Use real hardware if you want to experiment with copy protection, etc and
  //  generate the image using other tools.
  //
  unsigned max_track_size_bytes, track_byte_offset;

  auto trackDataSize = disk->bits_data_end - disk->bits_data;
  memset(disk->bits_data, 0, trackDataSize);

  switch (disk->disk_type) {
    case CLEM_DISK_TYPE_5_25:
      disk->bit_timing_ns = 4000;
      disk->track_count = 35;

      max_track_size_bytes = 0x0d * 512;
      for (unsigned i = 0; i < CLEM_DISK_LIMIT_QTR_TRACKS; ++i) {
        if ((i % 4) == 0 || (i % 4) == 1) {
          disk->meta_track_map[i] = (i / 4);
        } else {
          disk->meta_track_map[i] = 0xff;
        }
      }
      track_byte_offset = 0;
      for (unsigned i = 0; i < disk->track_count; ++i) {
        disk->track_byte_offset[i] = track_byte_offset;
        disk->track_byte_count[i] = max_track_size_bytes;
        disk->track_bits_count[i] = CLEM_DISK_BLANK_TRACK_BIT_LENGTH_525;
        disk->track_initialized[i] = 0;
        track_byte_offset += max_track_size_bytes;
      }
      break;
    case CLEM_DISK_TYPE_3_5:
      disk->bit_timing_ns = 2000;
      disk->track_count = disk->is_double_sided ? 160 : 80;
      for (unsigned i = 0; i < CLEM_DISK_LIMIT_QTR_TRACKS; ++i) {
        if (disk->is_double_sided) {
          disk->meta_track_map[i] = i;
        } else if ((i % 2) == 0) {
          disk->meta_track_map[i] = (i / 2);
        } else {
          disk->meta_track_map[i] = 0xff;
        }
      }
      track_byte_offset = 0;
      for (unsigned region_index = 0; region_index < 5; ++region_index) {
        unsigned bits_cnt = CLEM_DISK_35_CALC_BYTES_FROM_SECTORS(
          g_clem_max_sectors_per_region_35[region_index]) * 8;
        max_track_size_bytes = bits_cnt / 8;
        for (unsigned i = g_clem_track_start_per_region_35[region_index];
                      i < g_clem_track_start_per_region_35[region_index + 1];
        ) {
          unsigned track_index = i;
          if (!disk->is_double_sided) {
            track_index /= 2;
            i += 2;
          } else {
            i += 1;
          }
          disk->track_byte_offset[track_index] = track_byte_offset;
          disk->track_byte_count[track_index] = max_track_size_bytes;
          disk->track_bits_count[track_index] = bits_cnt;
          disk->track_initialized[track_index] = 0;
          track_byte_offset += max_track_size_bytes;
        }
      }
      break;
    default:
      return false;
  }
  return true;
}

void ClemensHost::release2IMGDisk(struct Clemens2IMGDisk* disk)
{
  if (disk->image_buffer) {
    free(disk->image_buffer);
    disk->image_buffer = NULL;
  }
  memset(disk, 0, sizeof(*disk));
}

bool ClemensHost::load2IMGDisk(
  const char* filename,
  struct Clemens2IMGDisk* disk,
  ClemensDriveType driveType
) {
  ClemensNibbleDisk* nib = disk->nib;
  release2IMGDisk(disk);
  disk->nib = nib;

  std::ifstream dskFile(filename, std::ios::binary | std::ios::ate);
  if (dskFile.is_open()) {
    unsigned sz = unsigned(dskFile.tellg());
    uint8_t* image_buffer = (uint8_t*)malloc(sz);
    dskFile.seekg(0, std::ios::beg);
    dskFile.read((char *)image_buffer, sz);
    dskFile.close();
    if (clem_2img_parse_header(disk, image_buffer, image_buffer + sz)) {
      //  output parsed metadata in log
      if (clem_2img_nibblize_data(disk)) {
        return true;
      }
      printf("load2IMGDisk: nibbilization pass failed.\n");
    }
  }

  return false;
}
