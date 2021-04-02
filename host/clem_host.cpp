#include "clem_host.hpp"
#include "clem_mem.h"

#include "fmt/format.h"
#include "imgui/imgui.h"

#include <algorithm>
#include <cctype>
#include <cstdio>
#include <cstdlib>

#include <inttypes.h>

namespace {
  constexpr unsigned kSlabMemorySize = 32 * 1024 * 1024;
  constexpr float kMinDebugHistoryHeight = 256;
  constexpr float kMinDebugHistoryScalar = 0.500f;
  constexpr float kMinDebugStatusHeight = 96;
  constexpr float kMinDebugStatusScalar = 0.175f;
  constexpr float kMinDebugTerminalHeight = 192;
  constexpr float kMinDebugTerminalScalar = 0.325f;

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
  emulationFrameLagTime_(0.0f),
  emulationRunTime_(0.0f),
  emulationStepCount_(0),
  emulationStepCountSinceReset_(0),
  clocksSpentOverflowFromLastFrame_(0),
  clocksSpentLastRun_(0),
  cpuRegsSaved_ {},
  cpuPinsSaved_ {},
  cpu6502EmulationSaved_(true)
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

}

void ClemensHost::frame(int width, int height, float deltaTime)
{
  bool emulationRan = false;
  if (emulationStepCount_ > 0 && clemens_is_initialized_simple(&machine_)) {
    emulate(deltaTime);
    emulationRan = true;
  }

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

  char windowTitle[64];
  if (emulationRan) {
    snprintf(windowTitle, sizeof(windowTitle), "Status (fps: %.2f)", ImGui::GetIO().Framerate);
  } else {
    snprintf(windowTitle, sizeof(windowTitle), "Status", ImGui::GetIO().Framerate);
  }
  ImGui::SetNextWindowPos(windowCursorPos);
  ImGui::SetNextWindowSize(windowSize);
  ImGui::Begin(windowTitle, nullptr, ImGuiWindowFlags_NoResize |
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
      double estimatedSpeed = 0.0;
      if (emulationRunTime_ > DBL_EPSILON) {
        estimatedSpeed = (clocksSpentLastRun_ * 1e-3) / (machine_.clocks_step_mega2 * 1e3);
        estimatedSpeed /= emulationRunTime_;
      }
      ImGui::TableNextColumn(); ImGui::Text("Total Steps");
      ImGui::TableNextColumn(); ImGui::Text("%" PRIu64 "", emulationStepCountSinceReset_);
      ImGui::TableNextRow();
      ImGui::TableNextColumn(); ImGui::Text("Cycles/slice");
      ImGui::TableNextColumn(); ImGui::Text("%u", machine_.cpu.cycles_spent);
      ImGui::TableNextRow();
      ImGui::TableNextColumn(); ImGui::Text("Actual Speed");
      if (estimatedSpeed < DBL_EPSILON) {
        ImGui::TableNextColumn(); ImGui::Text("N/A");
      } else {
        ImGui::TableNextColumn(); ImGui::Text("%.2lf mhz", estimatedSpeed);
      }
      ImGui::TableNextRow();
      ImGui::TableNextColumn(); ImGui::Text("Exec time");
      ImGui::TableNextColumn(); ImGui::Text("%.4f secs", emulationRunTime_);
    }
    ImGui::EndTable();

    ImGui::EndGroup();
  }
  ImGui::End();

  windowCursorPos.y += windowSize.y;
  windowSize.y = std::max(kMinDebugTerminalHeight, height * kMinDebugTerminalScalar);
  ImGui::SetNextWindowPos(windowCursorPos);
  ImGui::SetNextWindowSize(windowSize);
  ImGui::Begin("Terminal", nullptr, ImGuiWindowFlags_NoResize |
                                    ImGuiWindowFlags_NoCollapse );
  {
    bool terminalInFocus =  ImGui::IsWindowFocused();
    char buffer[128] = "";
    ImGui::PushStyleColor(ImGuiCol_FrameBg, ImGui::GetStyleColorVec4(ImGuiCol_WindowBg));
    ImGui::Text(">");
    ImGui::SameLine();
    float xpos = ImGui::GetCursorPosX();
    ImGui::SetNextItemWidth(windowSize.x - xpos - ImGui::GetStyle().WindowPadding.x);

    if (ImGui::InputText("", buffer, sizeof(buffer), ImGuiInputTextFlags_EnterReturnsTrue)) {
      terminalOutput_.clear();

      FormatView fv(terminalOutput_);
      if (parseCommand(buffer)) {
        fv.format("Ok.");
      } else {
        fv.format("Error.");
      }
      ImGui::SetKeyboardFocusHere(-1);
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
    memoryViewStatic_[0].DrawContents((void*)
      (void *)((uintptr_t)viewBank << 16), CLEM_IIGS_BANK_SIZE);
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
    memoryViewStatic_[1].DrawContents((void*)
      (void *)((uintptr_t)viewBank << 16), CLEM_IIGS_BANK_SIZE);
  }
  ImGui::End();
}

void ClemensHost::emulate(float deltaTime)
{
  //  execution loop for clemens 65816
  //    goal is to execute at least 2.8mhz / FixedFrameRate cycles per second
  //    practially this means:
  //      2800 clocks * 1e6 / FixedFrameRate for the 2.8mhz IIgs
  constexpr float kFixedFrameDeltaTime = 1/30.0f;
  bool is_machine_slow;
  const uint64_t kClocksPerSecond = clemens_clocks_per_second(
    &machine_, &is_machine_slow);
  const uint64_t kClocksPerFrameDesired = kFixedFrameDeltaTime * kClocksPerSecond;

  cpuRegsSaved_ = machine_.cpu.regs;
  cpuPinsSaved_ = machine_.cpu.pins;
  cpu6502EmulationSaved_ = machine_.cpu.pins.emulation;
  machine_.cpu.cycles_spent = 0;

  emulationFrameLagTime_ += deltaTime;
  while (emulationFrameLagTime_ >= kFixedFrameDeltaTime) {
    uint64_t clocksSpentDuringRun = clocksSpentOverflowFromLastFrame_;
    uint64_t clocksSpentInitial = machine_.clocks_spent;
    while (emulationStepCount_ > 0) {
      if (clocksSpentDuringRun >= kClocksPerFrameDesired) {
        clocksSpentOverflowFromLastFrame_ = clocksSpentDuringRun - kClocksPerFrameDesired;
        break;
      }
      if (!machine_.cpu.pins.resbIn) {
        if (emulationStepCount_ == 1) {
          machine_.cpu.pins.resbIn = true;
        }
      }
      clemens_emulate(&machine_);
      --emulationStepCount_;
      ++emulationStepCountSinceReset_;
      clocksSpentDuringRun = machine_.clocks_spent - clocksSpentInitial;
    }
    clocksSpentLastRun_ += clocksSpentDuringRun;
    emulationFrameLagTime_ -= kFixedFrameDeltaTime;
    printf("lag = %10.4f\n", emulationFrameLagTime_);
  }
  emulationRunTime_ += deltaTime;
  printf("dtm = %10.4f\n", deltaTime);
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
      if (!cb(start, cur)) return false;
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
      } else if (!strncasecmp(start, "step", end - start) ||
                 !strncasecmp(start, "s", end - start)) {
        return parseCommandStep(end);
      } else if (!strncasecmp(start, "run", end - start) ||
                 !strncasecmp(start, "r", end - start)) {
        return parseCommandRun(end);
      } else if (!strncasecmp(start, "clear", end - start) ||
                 !strncasecmp(start, "c", end - start)) {
        // clear some UI states
        executedInstructions_.clear();
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

bool ClemensHost::parseCommandStep(const char* line)
{
  const char* start = trimCommand(line);
  if (!clemens_is_initialized(&machine_)) {
    FormatView fv(terminalOutput_);
    fv.format("Machine not powered on.");
    return false;
  }
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

bool ClemensHost::parseCommandRun(const char* line)
{
  const char* start = trimCommand(line);
  if (!clemens_is_initialized(&machine_)) {
    FormatView fv(terminalOutput_);
    fv.format("Machine not powered on.");
    return false;
  }
  if (!start) {
    stepMachine(1000000);
    return true;
  }
  return parseCommandToken(start,
    [this](const char* start, const char* end) {
      char number[16];
      strncpy(number, start, std::min(sizeof(number) - 1, size_t(end - start)));
      number[15] = '\0';
      //stepMachine(strtol(number, nullptr, 10));
      return true;
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
  unsigned fpiBankCount = 16;         // 1MB RAM
  // was the IIgs really 2.864mhz vs 2.80mhz?
  // Apple II line (including Mega2) was 1.023mhz
  const uint32_t kClocksPerFastCycle = 1023;
  const uint32_t kClocksPerSlowCycle = 2864;
  clemens_init(&machine_, kClocksPerSlowCycle, kClocksPerFastCycle,
               romMemory, romMemorySize,
               slab_.allocate(CLEM_IIGS_BANK_SIZE),
               slab_.allocate(CLEM_IIGS_BANK_SIZE),
               slab_.allocate(CLEM_IIGS_BANK_SIZE * fpiBankCount),
               fpiBankCount);

  clemens_opcode_callback(&machine_, &ClemensHost::emulatorOpcodePrint, this);

  memoryViewBank_[0] = 0x00;
  memoryViewBank_[1] = 0x00;

  resetMachine();
}

void ClemensHost::destroyMachine()
{
  if (!clemens_is_initialized(&machine_)) {
    return;
  }
  memset(&machine_, 0, sizeof(ClemensMachine));
}

void ClemensHost::resetMachine()
{
  //  low signal indicates reset
  //  step 1: reset start, pull up resbIn
  //  step 2: reset end, issue interrupt
  machine_.cpu.pins.resbIn = false;   // low signal indicates reset
  emulationStepCount_ = 2;
  emulationStepCountSinceReset_ = 0;
}

void ClemensHost::stepMachine(int stepCount)
{
  emulationStepCount_ = std::max(0, stepCount);
  emulationFrameLagTime_ = 0.0f;
  emulationRunTime_ = 0.0f;
  clocksSpentOverflowFromLastFrame_ = 0;
  clocksSpentLastRun_ = 0;
}

void ClemensHost::emulatorOpcodePrint(
  struct ClemensInstruction* inst,
  const char* operand,
  void* this_ptr
) {
  auto* host = reinterpret_cast<ClemensHost*>(this_ptr);
  constexpr size_t kInstructionEdgeSize = 64;
  if (host->executedInstructions_.size() > 256 + kInstructionEdgeSize) {
    auto it = host->executedInstructions_.begin();
    host->executedInstructions_.erase(it, it + kInstructionEdgeSize);
  }
  host->executedInstructions_.emplace_back();
  auto& instruction = host->executedInstructions_.back();
  strncpy(instruction.opcode, inst->desc->name, sizeof(instruction.opcode));
  instruction.pc = (uint32_t(inst->pbr) << 16) | inst->addr;
  strncpy(instruction.operand, operand, sizeof(instruction.operand));
}
