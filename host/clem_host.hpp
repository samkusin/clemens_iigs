#ifndef CLEM_HOST_HPP
#define CLEM_HOST_HPP

#include "emulator.h"
#include "clem_woz.h"
#include "imgui/imgui.h"
#include "imgui/imgui_memory_editor.h"

#include "cinek/fixedstack.hpp"

#include "clem_host_utils.hpp"

#include <cstdint>
#include <vector>
#include <array>
#include <memory>
#include <string>

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

class ClemensAudioDevice;
class ClemensDisplay;
class ClemensDisplayProvider;
class ClemensProgramTrace;


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

  bool loadWOZDisk(const char* filename, struct ClemensWOZDisk* woz);

  bool parseWOZDisk(struct ClemensWOZDisk* woz, uint8_t* data, size_t dataSize);
  bool initWOZDisk(struct ClemensWOZDisk* woz);

private:

  void doIWMContextWindow();
  void doMemoryMapWindow();

  bool parseCommand(const char* buffer);
  bool parseCommandPower(const char* line);
  bool parseCommandReset(const char* line);
  bool parseCommandLoad(const char* line);
  bool parseCommandSave(const char* line);
  bool parseCommandDisk(const char* line);
  bool parseCommandDebugStatus(const char* line);
  bool parseCommandStep(const char* line);
  bool parseCommandBreak(const char* line);
  bool parseCommandListBreak(const char* line);
  bool parseCommandRemoveBreak(const char* line);
  bool parseCommandRun(const char* line);
  bool parseCommandLog(const char* line);
  bool parseCommandUnlog(const char* line);
  bool parseCommandDebugContext(const char* line);
  bool parseCommandSetValue(const char* line);
  bool parseCommandDump(const char* line);

  bool parseImmediateValue(unsigned& value, const char* line);
  bool parseImmediateString(std::string& value, const char* line);

  enum class MachineType {
    Apple2GS,
    Simple128K
  };

  bool createMachine(const char* filename, MachineType machineType);
  void destroyMachine();
  void resetMachine();
  bool saveState(const char* filename);
  bool loadState(const char* filename);
  void stepMachine(int stepCount);
  bool emulationRun(unsigned target);
  void emulationBreak();
  void resetDiagnostics();
  bool loadDisk(ClemensDriveType driveType, const char* filename);
  void loadDisks();
  void loadBRAM();
  void saveBRAM();


  void dumpMemory(unsigned bank, const char* filename);

  bool isRunningEmulation() const;
  bool isRunningEmulationStep() const;
  bool isRunningEmulationUntilBreak() const;
  bool hitBreakpoint();

  static void emulatorOpcodePrint(struct ClemensInstruction* inst,
                                  const char* operand,
                                  void* this_ptr);

  static uint8_t emulatorImGuiMemoryRead(void* ctx, const uint8_t* data, size_t off);
  static void emulatorImguiMemoryWrite(void* ctx, uint8_t* data, size_t off, uint8_t d);

  static uint8_t* unserializeAllocate(unsigned sz, void* context);

  static void emulatorLog(int log_level, ClemensMachine* machine, const char* msg);

private:
  ClemensMachine machine_;
  cinek::FixedStack slab_;

  ClemensMemoryPageMap simpleDirectPageMap_;

  std::array<struct ClemensWOZDisk, 2> disks35_;
  std::array<struct ClemensWOZDisk, 2> disks525_;
  std::array<std::string, 2> disks35Paths_;
  std::array<std::string, 2> disks525Paths_;

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

  bool emulatorHasKeyboardFocus_;

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

  enum class InputContext {
    None,
    TerminalKeyboardFocus
  };

  enum class DebugContext {
    IWM,
    MemoryMaps
  };

  InputContext widgetInputContext_;
  DebugContext widgetDebugContext_;

  std::unique_ptr<ClemensProgramTrace> programTrace_;

  std::vector<ClemensTraceExecutedInstruction> executedInstructions_;
  MemoryEditor memoryViewStatic_[2];
  uint8_t memoryViewBank_[2];

  std::vector<char> terminalOutput_;

  std::unique_ptr<ClemensDisplayProvider> displayProvider_;
  std::unique_ptr<ClemensDisplay> display_;

  std::unique_ptr<ClemensAudioDevice> audio_;
  std::vector<uint8_t> audioMixBuffer;

  unsigned adbKeyToggleMask_;

  struct Diagnostics {
    uint32_t audioFrames;
    clem_clocks_time_t clocksSpent;

    float deltaTime;        //  diagnostics current delta time from frame start
    float frameTime;        //  display diagnostics every frameTime seconds

    void reset();
  };
  Diagnostics diagnostics_;

  struct SimpleMachineIO {
    unsigned eventKeybA2 = 0;
    bool modShift = false;
    unsigned terminalOutIndex = 0;
    char terminalOut[256];
  };
  SimpleMachineIO simpleMachineIO_;
};


#endif
