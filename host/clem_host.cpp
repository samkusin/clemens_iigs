#include "clem_host.hpp"


#include "imgui/imgui.h"

#include <algorithm>
#include <cctype>
#include <cstdio>
#include <cstdlib>

namespace {
  constexpr unsigned kSlabMemorySize = 32 * 1024 * 1024;
  constexpr float kMinDebugHistoryHeight = 256;
  constexpr float kMinDebugHistoryScalar = 0.500f;
  constexpr float kMinDebugStatusHeight = 96;
  constexpr float kMinDebugStatusScalar = 0.200f;
  constexpr float kMinDebugTerminalHeight = 160;
  constexpr float kMinDebugTerminalScalar = 0.300f;

  constexpr float kMinConsoleWidth = 384;
  constexpr float kMinConsoleHeight = kMinDebugHistoryHeight +
                                      kMinDebugStatusHeight +
                                      kMinDebugTerminalHeight;
  constexpr float kConsoleWidthScalar = 0.333f;

}

ClemensHost::ClemensHost() :
  machine_(),
  emulationStepCount_(0),
  cpuRegsSaved_ {},
  cpuPinsSaved_ {},
  cpu6502EmulationSaved_(true)
{
  void* slabMemory = malloc(kSlabMemorySize);
  slab_ = cinek::FixedStack(kSlabMemorySize, slabMemory);
  executedInstructions_.reserve(1024);
}

ClemensHost::~ClemensHost()
{
  void* slabMemory = slab_.getHead();
  if (slabMemory) {
    free(slabMemory);
  }
}

void ClemensHost::frame(int width, int height, float deltaTime)
{
  //  execution loop for clemens 65816
  if (emulationStepCount_ > 0) {
    cpuRegsSaved_ = machine_.cpu.regs;
    cpuPinsSaved_ = machine_.cpu.pins;
    cpu6502EmulationSaved_ = machine_.cpu.emulation;

    while (emulationStepCount_ > 0) {
      if (!machine_.cpu.pins.resbIn) {
        if (emulationStepCount_ == 1) {
          machine_.cpu.pins.resbIn = true;
        }
      }
      clemens_emulate(&machine_);
      --emulationStepCount_;
    }
  }

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
  ImGui::End();

  windowCursorPos.y += windowSize.y;
  windowSize.y = std::max(kMinDebugStatusHeight, height * kMinDebugStatusScalar);
  ImGui::SetNextWindowPos(windowCursorPos);
  ImGui::SetNextWindowSize(windowSize);
  ImGui::Begin("Status", nullptr, ImGuiWindowFlags_NoResize |
                                  ImGuiWindowFlags_NoCollapse |
                                  ImGuiWindowFlags_NoInputs |
                                  ImGuiWindowFlags_NoBringToFrontOnFocus);
  {
    //  N, V, M, B, D, I, Z, C
    //  N, V, M, X, D, I, Z, C
    //  PC, PBR, IR
    //  S, DBR, D
    //  A, X, Y
    //  Brk, Emu, MX
    //  Todo: cycles, clocks spent
    const struct ClemensCPURegs& cpuRegsNext = machine_.cpu.regs;
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
    if (machine_.cpu.emulation) {
      ImGui::TableNextColumn(); ImGui::Text("-");
      ImGui::TableNextColumn(); ImGui::Text("B");
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
    ImGui::Separator();

    char label[16];
    ImGui::BeginTable("cpu_regs", 3, 0, ImVec2(windowSize.x * 0.5f, 0));
    {
      ImGui::TableNextRow();
      snprintf(label, sizeof(label), "PC=%04X", cpuRegsNext.PC);
      ImGui::TableNextColumn(); ImGui::Selectable(label, cpuRegsNext.PC != cpuRegsSaved_.PC);
      snprintf(label, sizeof(label), "S=%04X", cpuRegsNext.S);
      ImGui::TableNextColumn(); ImGui::Selectable(label, cpuRegsNext.S != cpuRegsSaved_.S);
      snprintf(label, sizeof(label), "A=%04X", cpuRegsNext.A);
      ImGui::TableNextColumn(); ImGui::Selectable(label, cpuRegsNext.A != cpuRegsSaved_.A);
      ImGui::TableNextRow();
      snprintf(label, sizeof(label), "PBR=%02X", cpuRegsNext.PBR);
      ImGui::TableNextColumn(); ImGui::Selectable(label, cpuRegsNext.PBR != cpuRegsSaved_.PBR);
      snprintf(label, sizeof(label), "DBR=%02X", cpuRegsNext.DBR);
      ImGui::TableNextColumn(); ImGui::Selectable(label, cpuRegsNext.DBR != cpuRegsSaved_.DBR);
      snprintf(label, sizeof(label), "X=%04X", cpuRegsNext.X);
      ImGui::TableNextColumn(); ImGui::Selectable(label, cpuRegsNext.X != cpuRegsSaved_.X);
      ImGui::TableNextRow();
      snprintf(label, sizeof(label), "IR=%02X", cpuRegsNext.IR);
      ImGui::TableNextColumn(); ImGui::Selectable(label, cpuRegsNext.IR != cpuRegsSaved_.IR);
      snprintf(label, sizeof(label), "D=%04X", cpuRegsNext.D);
      ImGui::TableNextColumn(); ImGui::Selectable(label, cpuRegsNext.D != cpuRegsSaved_.D);
      snprintf(label, sizeof(label), "Y=%04X", cpuRegsNext.Y);
      ImGui::TableNextColumn(); ImGui::Selectable(label, cpuRegsNext.Y != cpuRegsSaved_.Y);
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
      parseCommand(buffer);
    }
    ImGui::SetItemDefaultFocus();
    ImGui::SetKeyboardFocusHere(-1);
    ImGui::PopStyleColor();
  }
  ImGui::End();
  //  debug console
  //    console text with input line
  //  processor status view
  //  memory view 0
  //  memory view 1

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
    resetMachine();
    return true;
  }
  return false;
}

bool ClemensHost::parseCommandStep(const char* line)
{
  const char* start = trimCommand(line);
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
}

void ClemensHost::stepMachine(int stepCount)
{
  emulationStepCount_ = std::max(0, stepCount);
}

void ClemensHost::emulatorOpcodePrint(
  struct ClemensInstruction* inst,
  const char* operand,
  void* this_ptr
) {
  auto* host = reinterpret_cast<ClemensHost*>(this_ptr);
  constexpr size_t kInstructionEdgeSize = 64;
  if (host->executedInstructions_.size() > 1024 + kInstructionEdgeSize) {
    auto it = host->executedInstructions_.begin();
    host->executedInstructions_.erase(it, it + kInstructionEdgeSize);
  }
  host->executedInstructions_.emplace_back();
  auto& instruction = host->executedInstructions_.back();
  strncpy(instruction.opcode, inst->desc->name, sizeof(instruction.opcode));
  instruction.pc = (uint32_t(inst->pbr) << 16) | inst->addr;
  strncpy(instruction.operand, operand, sizeof(instruction.operand));
}
