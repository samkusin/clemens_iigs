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
  emulationStepCount_(0)
{
  void* slabMemory = malloc(kSlabMemorySize);
  slab_ = cinek::FixedStack(kSlabMemorySize, slabMemory);
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

  while (emulationStepCount_ > 0) {
    if (!machine_.cpu.pins.resbIn) {
      if (emulationStepCount_ == 1) {
        machine_.cpu.pins.resbIn = true;
      }
    }
    clemens_emulate(&machine_);
    --emulationStepCount_;
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
                                   ImGuiWindowFlags_NoInputs |
                                   ImGuiWindowFlags_NoBringToFrontOnFocus);
  ImGui::End();

  windowCursorPos.y += windowSize.y;
  windowSize.y = std::max(kMinDebugStatusHeight, height * kMinDebugStatusScalar);
  ImGui::SetNextWindowPos(windowCursorPos);
  ImGui::SetNextWindowSize(windowSize);
  ImGui::Begin("Status", nullptr, ImGuiWindowFlags_NoResize |
                                  ImGuiWindowFlags_NoCollapse |
                                  ImGuiWindowFlags_NoInputs |
                                  ImGuiWindowFlags_NoBringToFrontOnFocus);
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
      } else if (!strncasecmp(start, "step", end - start)) {
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
  return false;
}

bool ClemensHost::parseCommandStep(const char* line)
{
  return false;
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
