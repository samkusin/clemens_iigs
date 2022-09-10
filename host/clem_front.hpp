#ifndef CLEM_HOST_FRONT_HPP
#define CLEM_HOST_FRONT_HPP

#include "clem_host_shared.hpp"
#include "clem_display.hpp"
#include "clem_audio.hpp"
#include "cinek/buffer.hpp"
#include "cinek/circular_buffer.hpp"
#include "cinek/fixedstack.hpp"
#include "imgui/imgui.h"

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
  ClemensFrontend(const cinek::ByteBuffer& systemFontLoBuffer,
                  const cinek::ByteBuffer& systemFontHiBuffer);
  ~ClemensFrontend();

  //  application rendering hook
  void frame(int width, int height, float deltaTime);
  //  application input from OS
  void input(const ClemensInputEvent &input);

private:
  template<typename TBufferType> friend struct FormatView;

  //  the backend state delegate is run on a separate thread and notifies
  //  when a frame has been published
  void backendStateDelegate(const ClemensBackendState& state);

  void doMachineStateLayout(ImVec2 rootAnchor, ImVec2 rootSize);
  void doMachineDiagnosticsDisplay();
  void doMachineDiskDisplay();
  void doMachineDiskSelection(ClemensDriveType driveType);
  void doMachineCPUInfoDisplay();
  void doMachineViewLayout(ImVec2 rootAnchor, ImVec2 rootSize,
                           float screenU, float screenV);
  void doMachineTerminalLayout(ImVec2 rootAnchor, ImVec2 rootSize);

  void layoutTerminalLines();
  void layoutConsoleLines();

  void executeCommand(std::string_view command);
  void cmdHelp(std::string_view operand);
  void cmdBreak(std::string_view operand);
  void cmdList(std::string_view operand);
  void cmdRun(std::string_view operand);

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
    unsigned sz;          // size of log text following this struct in memory
    LogOutputNode* next;
  };

  struct FrameState {
    uint8_t* bankE0;
    uint8_t* bankE1;
    uint8_t* memoryView;
    uint8_t* audioBuffer;
    LogOutputNode* logNode;
    ClemensBackendBreakpoint* breakpoints;
    ClemensBackendBreakpoint* hitBreakpoint;
    unsigned breakpointCount;

    std::array<ClemensBackendDiskDrive, kClemensDrive_Count> diskDrives;

    Clemens65C816 cpu;
    ClemensMonitor monitorFrame;
    ClemensVideo textFrame;
    ClemensVideo graphicsFrame;
    ClemensAudio audioFrame;

    uint32_t vgcModeFlags;

    uint32_t irqs, nmis;

    unsigned backendCPUID;
    float fps;
    bool mmioWasInitialized;

    std::optional<bool> commandFailed;
  };

  cinek::FixedStack frameWriteMemory_;
  cinek::FixedStack frameReadMemory_;
  FrameState frameWriteState_;
  FrameState frameReadState_;
  ClemensCPUPins lastFrameCPUPins_;
  ClemensCPURegs lastFrameCPURegs_;
  uint32_t lastFrameIRQs_, lastFrameNMIs_;
  bool emulatorHasKeyboardFocus_;

  struct TerminalLine {
    enum Type {
      Debug,
      Info,
      Warn,
      Error,
      Command
    };
    std::string text;
    Type type;
  };
  template<size_t N>
  using TerminalBuffer = cinek::CircularBuffer<TerminalLine, N>;
  TerminalBuffer<128> terminalLines_;
  TerminalBuffer<512> consoleLines_;

  enum class TerminalMode {
    Command,
    Log,
    Execution
  };
  TerminalMode terminalMode_;

  std::vector<ClemensBackendBreakpoint> breakpoints_;
};

#endif
