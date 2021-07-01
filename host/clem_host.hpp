#ifndef CLEM_HOST_HPP
#define CLEM_HOST_HPP

#include "emulator.h"
#include "clem_woz.h"
#include "imgui/imgui.h"
#include "imgui/imgui_memory_editor.h"

#include "cinek/fixedstack.hpp"

#include <cstdint>
#include <vector>
#include <array>
#include <memory>

struct ClemensHostInputEvent
{
  enum Type {
    KeyDown,
    KeyUp
  };
  union {
    unsigned adb_keycode;
  };
};

class ClemensDisplay;
class ClemensDisplayProvider;


class ClemensHost
{
public:
  ClemensHost();
  ~ClemensHost();

  void frame(int width, int height, float deltaTime);

  void input(const ClemensInputEvent& input);

  void setDisplayImage(ImTextureID texId);

private:
  void emulate(float deltaTime);

  bool parseWOZDisk(struct ClemensWOZDisk* woz, uint8_t* data, size_t dataSize);

private:

  bool parseCommand(const char* buffer);
  bool parseCommandPower(const char* line);
  bool parseCommandReset(const char* line);
  bool parseCommandDebugStatus(const char* line);
  bool parseCommandStep(const char* line);
  bool parseCommandBreak(const char* line);
  bool parseCommandListBreak(const char* line);
  bool parseCommandRemoveBreak(const char* line);
  bool parseCommandRun(const char* line);

  void createMachine();
  void destroyMachine();
  void resetMachine();
  void stepMachine(int stepCount);
  bool emulationRun(unsigned target);
  void emulationBreak();

  bool isRunningEmulation() const;
  bool isRunningEmulationStep() const;
  bool hitBreakpoint();

  static void emulatorOpcodePrint(struct ClemensInstruction* inst,
                                  const char* operand,
                                  void* this_ptr);

  static uint8_t emulatorImGuiMemoryRead(void* ctx, const uint8_t* data, size_t off);
  static void emulatorImguiMemoryWrite(void* ctx, uint8_t* data, size_t off, uint8_t d);

private:
  ClemensMachine machine_;
  cinek::FixedStack slab_;

  std::array<struct ClemensWOZDisk, 2> disks35_;
  std::array<struct ClemensWOZDisk, 2> disks525_;

  float emulationRunTime_;
  float emulationSliceTimeLeft_;
  float emulationSliceDuration_;

  int emulationStepCount_;
  uint64_t emulationStepCountSinceReset_;
  uint64_t machineCyclesSpentDuringSample_;
  float sampleDuration_;
  float emulationSpeedSampled_;
  //  0x80000000, run indefinitely, emulationStepCount ignored
  //  0xffffffff, run target inactive (uses emulationStepCount)
  //  0x00xxxxxx, run PC target (if never hit, runs indefinitely)
  uint32_t emulationRunTarget_;

  struct Breakpoint {
    enum Op {
      Read,
      Write,
      PC
    };
    Op op;
    uint32_t addr;
  };

  std::vector<Breakpoint> breakpoints_;

  struct ClemensCPURegs cpuRegsSaved_;
  struct ClemensCPUPins cpuPinsSaved_;
  bool cpu6502EmulationSaved_;

  struct ExecutedInstruction {
    uint32_t pc;
    char opcode[4];
    char operand[24];
  };

  enum class InputContext {
    None,
    TerminalKeyboardFocus
  };

  InputContext widgetInputContext_;

  std::vector<ExecutedInstruction> executedInstructions_;
  MemoryEditor memoryViewStatic_[2];
  uint8_t memoryViewBank_[2];

  std::vector<char> terminalOutput_;

  std::unique_ptr<ClemensDisplayProvider> displayProvider_;
  std::unique_ptr<ClemensDisplay> display_;
};


#endif
