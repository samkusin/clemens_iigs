#ifndef CLEM_HOST_FRONT_HPP
#define CLEM_HOST_FRONT_HPP

#include "cinek/buffer.hpp"
#include "cinek/circular_buffer.hpp"
#include "cinek/fixedstack.hpp"
#include "clem_audio.hpp"
#include "clem_disk_library.hpp"
#include "clem_display.hpp"
#include "clem_host_shared.hpp"
#include "imgui.h"
#include "imgui_memory_editor.h"

#include <array>
#include <condition_variable>
#include <memory>
#include <mutex>
#include <optional>
#include <vector>

struct ImFont;

class ClemensBackend;

class ClemensFrontend {
public:
  ClemensFrontend(const cinek::ByteBuffer &systemFontLoBuffer,
                  const cinek::ByteBuffer &systemFontHiBuffer);
  ~ClemensFrontend();

  //  application rendering hook
  void frame(int width, int height, float deltaTime);
  //  application input from OS
  void input(const ClemensInputEvent &input);

private:
  template <typename TBufferType> friend struct FormatView;

  void createBackend();

  //  the backend state delegate is run on a separate thread and notifies
  //  when a frame has been published
  void backendStateDelegate(const ClemensBackendState &state);

  void doMachineStateLayout(ImVec2 rootAnchor, ImVec2 rootSize);
  void doMachineDiagnosticsDisplay();
  void doMachineDiskDisplay();
  void doMachineDiskSelection(ClemensDriveType driveType);
  void doMachineDiskStatus(ClemensDriveType driveType);
  void doMachineCPUInfoDisplay();
  void doMachineViewLayout(ImVec2 rootAnchor, ImVec2 rootSize, float screenU, float screenV);
  void doMachineTerminalLayout(ImVec2 rootAnchor, ImVec2 rootSize);

  void layoutTerminalLines();
  void layoutConsoleLines();

  std::pair<std::string, bool> importDisks(std::string outputPath, std::string name,
                                           ClemensDriveType driveType,
                                           std::vector<std::string> imagePaths);

  void executeCommand(std::string_view command);
  void cmdHelp(std::string_view operand);
  void cmdBreak(std::string_view operand);
  void cmdRun(std::string_view operand);
  void cmdReboot(std::string_view operand);
  void cmdReset(std::string_view operand);
  void cmdDisk(std::string_view operand);
  void cmdStep(std::string_view operand);
  void cmdLog(std::string_view operand);
  void cmdDump(std::string_view operand);

private:
  ClemensDisplayProvider displayProvider_;

  ClemensDisplay display_;
  ClemensAudioDevice audio_;
  std::unique_ptr<ClemensBackend> backend_;

  //  These buffers are populated when the backend publishes the current
  //  emulator state.   Their contents are refreshed for every publish
  std::condition_variable framePublished_;
  std::mutex frameMutex_;
  uint64_t frameSeqNo_, frameLastSeqNo_;
  //  Toggle between frame memory buffers so that backendStateDelegate and frame
  //  write to and read from separate buffers, to minimize the time we need to
  //  keep the frame mutex between the two threads.
  struct LogOutputNode {
    int logLevel;
    unsigned sz; // size of log text following this struct in memory
    LogOutputNode *next;
  };

  struct LogInstructionNode {
    ClemensBackendExecutedInstruction* begin;
    ClemensBackendExecutedInstruction* end;
    LogInstructionNode* next;
  };

  struct IWMStatus {
    int qtr_track_index;
    unsigned track_byte_index;
    unsigned track_bit_shift;
    unsigned track_bit_length;
    uint8_t buffer[4];
    uint8_t data;
    uint8_t latch;
    uint8_t status;
    uint8_t ph03;
  };

  // This state comes in for any update to the emulator per frame.  As such
  // its possible to "lose" state if the emulator runs faster than the UI.
  // This is OK in most cases as the UI will only present this data per frame
  struct FrameState {
    uint8_t *bankE0 = nullptr;
    uint8_t *bankE1 = nullptr;
    uint8_t *memoryView = nullptr;
    uint8_t *audioBuffer = nullptr;
    uint8_t* ioPage = nullptr;
    LogOutputNode *logNode = nullptr;
    ClemensBackendBreakpoint *breakpoints = nullptr;
    unsigned breakpointCount = 0;
    int logLevel;

    std::array<ClemensBackendDiskDriveState, kClemensDrive_Count> diskDrives;

    Clemens65C816 cpu;
    ClemensMonitor monitorFrame;
    ClemensVideo textFrame;
    ClemensVideo graphicsFrame;
    ClemensAudio audioFrame;

    IWMStatus iwm;

    uint32_t vgcModeFlags;

    uint32_t irqs, nmis;

    uint8_t memoryViewBank = 0;

    unsigned backendCPUID;
    float fps;
    bool mmioWasInitialized = false;
  };

  //  This state sticks around until processed by the UI frame - a hacky solution
  //  to the problem mentioned with FrameState.  In some cases we want to know
  //  when an event happened (command failed, termination, breakpoint hit, etc)
  struct LastCommandState {
    std::optional<bool> terminated;
    std::optional<bool> commandFailed;
    std::optional<ClemensBackendCommand::Type> commandType;
    std::optional<unsigned> hitBreakpoint;
    LogOutputNode *logNode = nullptr;
    LogOutputNode *logNodeTail = nullptr;
    LogInstructionNode* logInstructionNode = nullptr;
    LogInstructionNode* logInstructionNodeTail = nullptr;
  };

  ClemensBackendConfig config_;

  cinek::FixedStack frameWriteMemory_;
  cinek::FixedStack frameReadMemory_;
  cinek::FixedStack logMemory_;
  FrameState frameWriteState_;
  FrameState frameReadState_;
  LastCommandState lastCommandState_;

  ClemensCPUPins lastFrameCPUPins_;
  ClemensCPURegs lastFrameCPURegs_;
  IWMStatus lastFrameIWM_;
  uint32_t lastFrameIRQs_, lastFrameNMIs_;
  uint8_t lastFrameIORegs_[256];
  bool emulatorHasKeyboardFocus_;

  struct TerminalLine {
    enum Type { Debug, Info, Warn, Error, Command, Opcode };
    std::string text;
    Type type;
  };
  template <size_t N> using TerminalBuffer = cinek::CircularBuffer<TerminalLine, N>;
  TerminalBuffer<128> terminalLines_;
  TerminalBuffer<512> consoleLines_;

  enum class TerminalMode { Command, Log, Execution };
  TerminalMode terminalMode_;

  std::vector<ClemensBackendBreakpoint> breakpoints_;

  std::string diskLibraryRootPath_;
  ClemensDiskLibrary diskLibrary_;

  unsigned diskComboStateFlags_; // if opened, flag == 1 else 0

private:
  void doMachineDebugMemoryDisplay();
  void doMachineDebugCoreIODisplay();
  void doMachineDebugVideoIODisplay();
  void doMachineDebugDiskIODisplay();

  enum class DebugIOMode {
    Core
  };
  DebugIOMode debugIOMode_;

  MemoryEditor debugMemoryEditor_;

  static ImU8 imguiMemoryEditorRead(const ImU8* mem_ptr, size_t off);
  static void imguiMemoryEditorWrite(ImU8* mem_ptr, size_t off, ImU8 value);

private:
  //  UI State Specific Flows
  void doModalOperations(int width, int height);
  void doImportDiskSetFlowStart(int width, int height);
  void doImportDiskSetReplaceOld(int width, int height);
  void doImportDiskSet(int width, int height);
  void doNewBlankDisk(int widht, int height);

  enum class GUIMode {
    Emulator,
    ImportDiskModal,
    BlankDiskModal,
    ImportDiskSetFlow,
    ImportDiskSetReplaceOld,
    ImportDiskSet,
    NewBlankDisk,
    RebootEmulator
  };
  GUIMode guiMode_;
  ClemensDriveType importDriveType_;
  std::string importDiskSetName_;
  std::string importDiskSetPath_;
  std::vector<std::string> importDiskFiles_;
  std::string messageModalString_;
};

#endif
