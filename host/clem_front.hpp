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

  struct FrameAppInterop {
    bool mouseLock;
  };

  //  application rendering hook
  void frame(int width, int height, double deltaTime, FrameAppInterop& interop);
  //  application input from OS
  void input(ClemensInputEvent input);

private:
  template <typename TBufferType> friend struct FormatView;

  std::unique_ptr<ClemensBackend> createBackend();

  //  the backend state delegate is run on a separate thread and notifies
  //  when a frame has been published
  void backendStateDelegate(const ClemensBackendState &state);
  void copyState(const ClemensBackendState& state);

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
  void cmdTrace(std::string_view operand);
  std::string cmdMessageFromBackend(std::string_view operand, const ClemensMachine* machine);
  bool cmdMessageLocal(std::string_view operand);
  void cmdSave(std::string_view operand);
  void cmdLoad(std::string_view operand);
  void cmdGet(std::string_view operand);
  void cmdADBMouse(std::string_view operand);

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
  static const uint64_t kFrameSeqNoInvalid;

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

  struct DOCStatus {
    /** PCM output (floating point per channel) */
    float voice[16];

    uint8_t reg[256];             /**< DOC register values */
    unsigned acc[32];             /**< Oscillator running accumulator */
    uint16_t ptr[32];             /**< Stored pointer from last cycle */
    uint8_t osc_flags[32];        /**< IRQ flagged */

    void copyFrom(const ClemensDeviceEnsoniq& doc);
  };

  // This state comes in for any update to the emulator per frame.  As such
  // its possible to "lose" state if the emulator runs faster than the UI.
  // This is OK in most cases as the UI will only present this data per frame
  struct FrameState {
    uint8_t *bankE0 = nullptr;
    uint8_t *bankE1 = nullptr;
    uint8_t *memoryView = nullptr;
    uint8_t* ioPage = nullptr;
    uint8_t* docRAM = nullptr;
    LogOutputNode *logNode = nullptr;
    ClemensBackendBreakpoint *breakpoints = nullptr;
    unsigned breakpointCount = 0;
    int logLevel;

    std::array<ClemensBackendDiskDriveState, kClemensDrive_Count> diskDrives;

    float emulatorSpeedMhz;
    ClemensClock emulatorClock;

    Clemens65C816 cpu;
    ClemensMonitor monitorFrame;
    ClemensVideo textFrame;
    ClemensVideo graphicsFrame;
    ClemensAudio audioFrame;

    IWMStatus iwm;
    DOCStatus doc;

    uint32_t vgcModeFlags;

    uint32_t irqs, nmis;

    uint8_t memoryViewBank = 0;

    unsigned backendCPUID;
    float fps;
    bool mmioWasInitialized = false;
    bool isTracing = false;
    bool isIWMTracing = false;
  };

  //  This state sticks around until processed by the UI frame - a hacky solution
  //  to the problem mentioned with FrameState.  In some cases we want to know
  //  when an event happened (command failed, termination, breakpoint hit, etc)
  struct LastCommandState {
    std::optional<bool> terminated;
    std::optional<bool> commandFailed;
    std::optional<ClemensBackendCommand::Type> commandType;
    std::optional<unsigned> hitBreakpoint;
    std::optional<std::string> message;
    LogOutputNode *logNode = nullptr;
    LogOutputNode *logNodeTail = nullptr;
    LogInstructionNode* logInstructionNode = nullptr;
    LogInstructionNode* logInstructionNodeTail = nullptr;
    uint32_t memoryCaptureAddress = 0;
    uint32_t memoryCaptureSize = 0;
    uint8_t* memory = nullptr;
    cinek::ByteBuffer audioBuffer;
  };

  ClemensBackendConfig config_;

  cinek::FixedStack frameWriteMemory_;
  cinek::FixedStack frameReadMemory_;
  cinek::FixedStack frameMemory_;
  FrameState frameWriteState_;
  FrameState frameReadState_;
  LastCommandState lastCommandState_;
  cinek::ByteBuffer thisFrameAudioBuffer_;

  ClemensCPUPins lastFrameCPUPins_;
  ClemensCPURegs lastFrameCPURegs_;
  IWMStatus lastFrameIWM_;
  uint32_t lastFrameIRQs_, lastFrameNMIs_;
  uint8_t lastFrameIORegs_[256];
  bool emulatorHasKeyboardFocus_;
  bool emulatorHasMouseFocus_;

  struct TerminalLine {
    enum Type { Debug, Info, Warn, Error, Command, Opcode };
    std::string text;
    Type type;
  };
  template <size_t N> using TerminalBuffer = cinek::CircularBuffer<TerminalLine, N>;
  TerminalBuffer<128> terminalLines_;
  TerminalBuffer<512> consoleLines_;
  bool terminalChanged_;
  bool consoleChanged_;

  enum class TerminalMode { Command, Log, Execution };
  TerminalMode terminalMode_;

  std::vector<ClemensBackendBreakpoint> breakpoints_;

  std::string diskLibraryRootPath_;
  ClemensDiskLibrary diskLibrary_;

  unsigned diskComboStateFlags_; // if opened, flag == 1 else 0

private:
  void doMachineDebugMemoryDisplay();
  void doMachineDebugDOCDisplay();
  void doMachineDebugCoreIODisplay();
  void doMachineDebugVideoIODisplay();
  void doMachineDebugDiskIODisplay();
  void doMachineDebugADBDisplay();
  void doMachineDebugSoundDisplay();

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
  void doNewBlankDiskFlow(int width, int height);
  void doNewBlankDisk(int width, int height);

  enum class GUIMode {
    Emulator,
    ImportDiskModal,
    BlankDiskModal,
    ImportDiskSetFlow,
    ImportDiskSetReplaceOld,
    ImportDiskSet,
    NewBlankDiskFlow,
    NewBlankDiskReplaceOld,
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
