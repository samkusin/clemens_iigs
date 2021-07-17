#include "clem_host.hpp"
#include "clem_display.hpp"
#include "clem_mem.h"
#include "clem_vgc.h"

#include "fmt/format.h"
#include "imgui/imgui.h"

#include <algorithm>
#include <chrono>
#include <cctype>
#include <cstdio>
#include <cstdlib>

#include <inttypes.h>

namespace {
  constexpr unsigned kSlabMemorySize = 32 * 1024 * 1024;
  constexpr float kMinDebugHistoryHeight = 256;
  constexpr float kMinDebugHistoryScalar = 0.500f;
  constexpr float kMinDebugStatusHeight = 104;
  constexpr float kMinDebugStatusScalar = 0.180f;
  constexpr float kMinDebugTerminalHeight = 184;
  constexpr float kMinDebugTerminalScalar = 0.320f;

  constexpr float kMinConsoleWidth = 384;
  constexpr float kMinConsoleHeight = kMinDebugHistoryHeight +
                                      kMinDebugStatusHeight +
                                      kMinDebugTerminalHeight;
  constexpr float kConsoleWidthScalar = 0.333f;

  struct FormatView {
    std::vector<char>& buffer_;
    FormatView(std::vector<char>& buffer): buffer_(buffer) {}
    template<typename... Args> void format(const char* formatStr, const Args&... args) {
      size_t sz = fmt::formatted_size(formatStr, args...);
      size_t start = buffer_.size();
      buffer_.resize(start + sz + 1);
      fmt::format_to_n(buffer_.data() + start, sz + 1, formatStr, args...);
    }
  };
} // namespace anon

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
  emulationRunTarget_(0xffffffff),
  emulatorHasKeyboardFocus_(false),
  cpuRegsSaved_ {},
  cpuPinsSaved_ {},
  cpu6502EmulationSaved_(true),
  widgetInputContext_(InputContext::None),
  display_(nullptr)
{
  void* slabMemory = malloc(kSlabMemorySize);
  slab_ = cinek::FixedStack(kSlabMemorySize, slabMemory);
  executedInstructions_.reserve(1024);
  memoryViewStatic_[0].HandlerContext = this;
  memoryViewStatic_[0].ReadFn = &ClemensHost::emulatorImGuiMemoryRead;
  memoryViewStatic_[0].WriteFn = &ClemensHost::emulatorImguiMemoryWrite;
  memoryViewStatic_[1].HandlerContext = this;
  memoryViewStatic_[1].ReadFn = &ClemensHost::emulatorImGuiMemoryRead;
  memoryViewStatic_[1].WriteFn = &ClemensHost::emulatorImguiMemoryWrite;

  //  TODO: move into UI
  FILE* fp = fopen("dos_3_3_master.woz", "rb");
  if (fp) {
    fseek(fp, 0, SEEK_END);
    long sz = ftell(fp);
    uint8_t* tmp = (uint8_t*)malloc(sz);
    fseek(fp, 0, SEEK_SET);
    fread(tmp, 1, sz, fp);
    fclose(fp);
    parseWOZDisk(&disks525_[0], tmp, sz);
    free(tmp);
  }

  displayProvider_ = std::make_unique<ClemensDisplayProvider>();
  display_ = std::make_unique<ClemensDisplay>(*displayProvider_);
}

ClemensHost::~ClemensHost()
{
  void* slabMemory = slab_.getHead();
  if (slabMemory) {
    free(slabMemory);
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
    clemens_input(&machine_, &input);
  }
}

void ClemensHost::frame(int width, int height, float deltaTime)
{
  bool emulationRan = false;
  if (isRunningEmulation()) {
    emulate(deltaTime);
    emulationRan = true;
  }
  ClemensMonitor monitor = {};
  constexpr int kClemensScreenWidth = 720;
  constexpr int kClemensScreenHeight = 480;
  float screenUVs[2];

  if (clemens_is_initialized(&machine_)) {
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
      if (machine_.mmio.vgc.mode_flags & CLEM_VGC_HIRES) {
        display_->renderHiresGraphics(video, machine_.mega2_bank_map[0]);
      } else if (machine_.mmio.vgc.mode_flags & CLEM_VGC_LORES) {
        display_->renderLoresGraphics(video, machine_.mega2_bank_map[0]);
      }
    }
    display_->finish(screenUVs);
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

  ImGui::SetNextWindowPos(windowCursorPos);
  ImGui::SetNextWindowSize(windowSize);
  ImGui::Begin("History", nullptr, ImGuiWindowFlags_NoResize |
                                   ImGuiWindowFlags_NoCollapse |
                                   ImGuiWindowFlags_NoBringToFrontOnFocus);

  for (auto& instruction : executedInstructions_) {
    ImGui::TextColored(ImVec4(0.75f, 0.75f, 0.75f, 1.0f), "%02X/%04X",
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

      FormatView fv(terminalOutput_);
      if (parseCommand(buffer)) {
        fv.format("Ok.");
      } else {
        fv.format("Error.");
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
  if (clemens_is_mmio_initialized(&machine_)) {
    memoryViewStatic_[0].ReadOnly = true;
    if (emulationRan) {
      memoryViewStatic_[0].GotoAddrAndHighlight(cpuPinsNext.adr, cpuPinsNext.adr + 1);
      memoryViewBank_[0] = cpuPinsNext.bank;
    }
    ImGui::InputScalar("Bank", ImGuiDataType_U8, &memoryViewBank_[0],
                       nullptr, nullptr, "%02X", ImGuiInputTextFlags_CharsHexadecimal);
    uint8_t viewBank = memoryViewBank_[0];
    if (!isRunningEmulation() || isRunningEmulationStep()) {
      memoryViewStatic_[0].DrawContents((void*)
        (void *)((uintptr_t)viewBank << 16), CLEM_IIGS_BANK_SIZE);
    }
  }
  ImGui::End();
  memoryViewCursor.x += memoryViewSize.x;
  ImGui::SetNextWindowPos(memoryViewCursor);
  ImGui::SetNextWindowSize(memoryViewSize);
  ImGui::Begin("Memory 1", nullptr, ImGuiWindowFlags_NoResize |
                                    ImGuiWindowFlags_NoCollapse |
                                    ImGuiWindowFlags_NoBringToFrontOnFocus);
  if (clemens_is_mmio_initialized(&machine_)) {
    ImGui::InputScalar("Bank", ImGuiDataType_U8, &memoryViewBank_[1],
                       nullptr, nullptr, "%02X", ImGuiInputTextFlags_CharsHexadecimal);
    uint8_t viewBank = memoryViewBank_[1];
    if (!isRunningEmulation() || isRunningEmulationStep()) {
      memoryViewStatic_[1].DrawContents((void*)
        (void *)((uintptr_t)viewBank << 16), CLEM_IIGS_BANK_SIZE);
    }
  }
  ImGui::End();
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
  uint64_t clocksSpentInitial = machine_.clocks_spent;

  const time_t kEpoch1904To1970Seconds = 2082844800;
  auto epoch_time_1904 = (
    std::chrono::system_clock::to_time_t(std::chrono::system_clock::now()) +
      kEpoch1904To1970Seconds);

  clemens_rtc_set(&machine_, (unsigned)epoch_time_1904);


  machine_.cpu.cycles_spent = 0;
  while (emulationStepCount_ > 0 || emulationRunTarget_ != 0xffffffff) {
    if (machine_.clocks_spent - clocksSpentInitial >= kClocksPerFrameDesired) {
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
    if (!breakpoints_.empty()) {
      if (hitBreakpoint()) {
        emulationBreak();
        break;
      }
    }

    --emulationStepCount_;
    ++emulationStepCountSinceReset_;
  }
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
    bool b_adr_hit = (machine_.cpu.pins.bank == b_bank &&
                      machine_.cpu.pins.adr == b_adr);
    if (b_adr_hit) {
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
        case Breakpoint::PC:
          if (machine_.cpu.pins.vpaOut) {
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
      } else if (!strncasecmp(start, "step", end - start) ||
                 !strncasecmp(start, "s", end - start)) {
        return parseCommandStep(end);
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
      }
      return false;
    });
}

bool ClemensHost::parseCommandPower(const char* line)
{
  const char* start = trimCommand(line);
  if (!start) {
    //  assume toggle power
    if (!clemens_is_initialized(&machine_)) {
      createMachine();
    } else {
      destroyMachine();
    }
    return true;
  }
  return false;
}

bool ClemensHost::parseCommandReset(const char* line)
{
  const char* start = trimCommand(line);
  if (!start) {
    if (!clemens_is_initialized(&machine_)) {
      FormatView fv(terminalOutput_);
      fv.format("Machine not powered on.");
      return false;
    }
    resetMachine();
    return true;
  }
  return false;
}

bool ClemensHost::parseCommandDebugStatus(const char* line)
{
  const char* start = trimCommand(line);
  if (!start) {
    if (!clemens_is_initialized(&machine_)) {
      FormatView fv(terminalOutput_);
      fv.format("Machine not powered on.");
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
    FormatView fv(terminalOutput_);
    fv.format("Machine not powered on.");
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

bool ClemensHost::parseCommandBreak(const char* line) {
  const char* start = trimCommand(line);
  if (!clemens_is_initialized(&machine_)) {
    FormatView fv(terminalOutput_);
    fv.format("Machine not powered on.");
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
        unsigned addr = strtoul(&number[2], nullptr, 16);
        if (number[0] == 'r' && number[1] == '@') {
          breakpoints_.emplace_back(Breakpoint { Breakpoint::Read, addr });
        } else if (number[0] == 'w' && number[1] == '@') {
          breakpoints_.emplace_back(Breakpoint { Breakpoint::Write, addr });
        } else if (addr != 0) {
          breakpoints_.emplace_back(Breakpoint { Breakpoint::PC, addr });
        } else {
          return false;
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
  FormatView fv(terminalOutput_);
  if (!start) {
    breakpoints_.clear();
    fv.format("Breakpoints cleared");
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
    FormatView fv(terminalOutput_);
    fv.format("Machine not powered on.");
    return false;
  }
  if (!start) {
    emulationStepCount_ = 0;
    emulationRun(0x00ffffff);
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
    FormatView fv(terminalOutput_);
    fv.format("Machine not powered on.");
    return false;
  }
  if (!start) {
    FormatView fv(terminalOutput_);
    fv.format("usage: log <type>");
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
      return false;
    });
}

bool ClemensHost::parseCommandUnlog(const char* line)
{
  const char* start = trimCommand(line);
  if (!clemens_is_initialized(&machine_)) {
    FormatView fv(terminalOutput_);
    fv.format("Machine not powered on.");
    return false;
  }
  if (!start) {
    FormatView fv(terminalOutput_);
    fv.format("usage: unlog <type>");
    return false;
  }
  return parseCommandToken(start,
    [this](const char* start, const char* end) {
      char name[16];
      strncpy(name, start, std::min(sizeof(name) - 1, size_t(end - start)));
      name[end - start] = '\0';
      if (!strncasecmp(name, "iwm", sizeof(name))) {
        machine_.mmio.dev_iwm.enable_debug = false;
        return true;
      }
      return false;
    });
}

void ClemensHost::createMachine()
{
  if (clemens_is_initialized(&machine_)) {
    return;
  }
  slab_.reset();

  void* romMemory = NULL;
  unsigned romMemorySize = 0;
  FILE* fp = fopen("gs_rom_3.rom", "rb");
  if (fp) {
    fseek(fp, 0, SEEK_END);
    romMemorySize = (unsigned)ftell(fp);
    romMemory = slab_.allocate(romMemorySize);
    fseek(fp, 0, SEEK_SET);
    fread(romMemory, 1, romMemorySize, fp);
    fclose(fp);
  }
  unsigned fpiBankCount = CLEM_IIGS_FPI_MAIN_RAM_BANK_COUNT;
  // Apple II line (including Mega2) was 1.023mhz
  // A Mega2 cycle is equivalent to 1 full cycle of the 2.8mhz clock
  // TODO: allows 1.023, 2.8, 8mhz without any loss due to integer div
  const uint32_t kClocksPerFastCycle = CLEM_CLOCKS_FAST_CYCLE;
  const uint32_t kClocksPerSlowCycle = CLEM_CLOCKS_MEGA2_CYCLE;
  clemens_init(&machine_, kClocksPerSlowCycle, kClocksPerFastCycle,
               romMemory, romMemorySize,
               slab_.allocate(CLEM_IIGS_BANK_SIZE),
               slab_.allocate(CLEM_IIGS_BANK_SIZE),
               slab_.allocate(CLEM_IIGS_BANK_SIZE * fpiBankCount),
               slab_.allocate(256 * 7), // TODO make this a little less magic-numberish
               slab_.allocate(2048 * 7),
               fpiBankCount);

  clemens_opcode_callback(&machine_, &ClemensHost::emulatorOpcodePrint, this);

  clemens_assign_disk(&machine_, kClemensDrive_5_25_D1, &disks525_[0]);

  memoryViewBank_[0] = 0x00;
  memoryViewBank_[1] = 0x00;

  resetMachine();
}

void ClemensHost::destroyMachine()
{
  if (!clemens_is_initialized(&machine_)) {
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
  stepMachine(2);
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
}

bool ClemensHost::emulationRun(unsigned target) {
  emulationRunTarget_ = target;
  emulationStepCount_ = 0;
  if (emulationRunTarget_ > 0x00ffffff) {
    return false;
  }
  clemens_opcode_callback(&machine_, NULL, NULL);
  return true;
}

void ClemensHost::emulationBreak()
{
  emulationRunTarget_ = 0xffffffff;
  emulationStepCount_ = 0;
  clemens_opcode_callback(&machine_, &ClemensHost::emulatorOpcodePrint, this);
}

bool ClemensHost::isRunningEmulation() const
{
  return (emulationStepCount_ > 0 || emulationRunTarget_ != 0xffffffff);
}

bool ClemensHost::isRunningEmulationStep() const {
  return isRunningEmulation() && emulationStepCount_ > 0;
}

void ClemensHost::emulatorOpcodePrint(
  struct ClemensInstruction* inst,
  const char* operand,
  void* this_ptr
) {
  auto* host = reinterpret_cast<ClemensHost*>(this_ptr);
  constexpr size_t kInstructionEdgeSize = 64;
  if (host->executedInstructions_.size() > 128 + kInstructionEdgeSize) {
    auto it = host->executedInstructions_.begin();
    host->executedInstructions_.erase(it, it + kInstructionEdgeSize);
  }
  host->executedInstructions_.emplace_back();
  auto& instruction = host->executedInstructions_.back();
  strncpy(instruction.opcode, inst->desc->name, sizeof(instruction.opcode));
  instruction.pc = (uint32_t(inst->pbr) << 16) | inst->addr;
  strncpy(instruction.operand, operand, sizeof(instruction.operand));
}

bool ClemensHost::parseWOZDisk(
  struct ClemensWOZDisk* woz,
  uint8_t* data,
  size_t dataSize
) {
  const uint8_t* current = clem_woz_check_header(data, dataSize);
  const uint8_t* end = data + dataSize;
  if (!current) {
    return false;
  }

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
        if (woz->track_count > 0) {
          unsigned trackDataSize = woz->track_count * woz->max_track_size_bytes;
          if (woz->flags & CLEM_WOZ_IMAGE_DOUBLE_SIDED) {
            trackDataSize <<= 1;
          }
          woz->bits_data = (uint8_t*)malloc(trackDataSize);
          woz->bits_data_end = woz->bits_data + trackDataSize;
        }
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

  return true;
}
