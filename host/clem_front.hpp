#ifndef CLEM_HOST_FRONT_HPP
#define CLEM_HOST_FRONT_HPP

#include "clem_command_queue.hpp"
#include "clem_host_platform.h"
#include "clem_host_view.hpp"

#include "core/clem_apple2gs_config.hpp"

#include "cinek/buffer.hpp"
#include "cinek/circular_buffer.hpp"
#include "cinek/fixedstack.hpp"
#include "clem_audio.hpp"
#include "clem_configuration.hpp"
#include "clem_disk_browser.hpp"
#include "clem_display.hpp"
#include "clem_host_shared.hpp"
#include "clem_ui_load_snapshot.hpp"
#include "clem_ui_save_snapshot.hpp"

#include "imgui.h"
#include "imgui_memory_editor.h"

#include <array>
#include <condition_variable>
#include <memory>
#include <mutex>
#include <optional>
#include <thread>
#include <vector>

struct ImFont;

class ClemensBackend;
struct ClemensBackendState;

class ClemensFrontend : public ClemensHostView {
  public:
    ClemensFrontend(ClemensConfiguration config, const cinek::ByteBuffer &systemFontLoBuffer,
                    const cinek::ByteBuffer &systemFontHiBuffer);
    ~ClemensFrontend();

    ViewType getViewType() const final { return ViewType::Main; }
    ViewType frame(int width, int height, double deltaTime, FrameAppInterop &interop) final;
    void input(ClemensInputEvent input) final;
    void lostFocus() final;

  private:
    template <typename TBufferType> friend struct FormatView;

    void startBackend();
    void runBackend(std::unique_ptr<ClemensBackend> backend);
    void stopBackend();
    bool isBackendRunning() const;

    //  the backend state delegate is run on a separate thread and notifies
    //  when a frame has been published
    void backendStateDelegate(const ClemensBackendState &state,
                              const ClemensCommandQueue::ResultBuffer &results);
    void processBackendResult(const ClemensBackendResult &result);

    void doEmulatorInterface(ImVec2 dimensions, ImVec2 screenUVs, double deltaTime);
    void doDebuggerInterface(ImVec2 dimensions, ImVec2 screenUVs, double deltaTime);

    void doSidePanelLayout(ImVec2 anchor, ImVec2 dimensions);
    void doUserMenuDisplay(float width);
    void doMachinePeripheralDisplay(float width);
    void doDebuggerQuickbar(float width);
    void doInfoStatusLayout(ImVec2 anchor, ImVec2 dimensions, float dividerXPos);
    void doViewInputInstructions(ImVec2 dimensions);
    void doMachineStateLayout(ImVec2 rootAnchor, ImVec2 rootSize);
    void doMachineDiagnosticsDisplay();
    void doMachineDiskDisplay(float width);
    void doMachineDiskStatus(ClemensDriveType driveType, float width);
    void doMachineDiskSelection(ClemensDriveType driveType, float width, bool showLabel);
    void doMachineDiskMotorStatus(const ImVec2 &pos, const ImVec2 &size, bool isSpinning);
    void doMachineSmartDriveStatus(unsigned driveIndex, float width);
    void doMachineCPUInfoDisplay();
    void doSetupUI(ImVec2 anchor, ImVec2 dimensions);
    void doMachineViewLayout(ImVec2 rootAnchor, ImVec2 rootSize, float screenU, float screenV);
    void doMachineInfoBar(ImVec2 rootAnchor, ImVec2 rootSize);
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
    void cmdShutdown(std::string_view operand);
    void cmdReset(std::string_view operand);
    void cmdDisk(std::string_view operand);
    void cmdStep(std::string_view operand);
    void cmdLog(std::string_view operand);
    void cmdDump(std::string_view operand);
    void cmdTrace(std::string_view operand);
    void cmdSave(std::string_view operand);
    void cmdLoad(std::string_view operand);
    void cmdGet(std::string_view operand);
    void cmdADBMouse(std::string_view operand);
    void cmdMinimode(std::string_view operand);
    void cmdScript(std::string_view command);

    void rebootInternal();

  private:
    ClemensConfiguration config_;
    ClemensDisplayProvider displayProvider_;

    ClemensDisplay display_;
    ClemensAudioDevice audio_;

    std::unique_ptr<ClemensBackend> backend_;
    int logLevel_;

    //  Handles synchronization between the emulator and the UI(main) thread
    //  The backendQueue is filled by the UI.  It will "stage" these commands
    //  every UI frame and report its timestamp.
    //
    //  The worker will handle commands as long as the frameTimestamp changes.
    //  It uses this timestamp to make sure it doesn't emulate frames beyond
    //  those that are needed to keep in sync with the UI.  This should more or
    //  less work with fast emulation enabled, since frames are reported back to
    //  the frontend based on the UI's framerate using this counter.
    std::thread backendThread_;
    ClemensCommandQueue backendQueue_;
    ClemensCommandQueue stagedBackendQueue_;
    double uiFrameTimeDelta_;

    // These counters are used to identify unique frames between the two threads
    // frameSeqNo is updated per executed emulator frame.  The UI thread will
    // check frameSeqNo against frameLastSeqNo during the synchronization phase
    // and if they differ, then the UI has data for a new emulator state.
    uint64_t frameSeqNo_, frameLastSeqNo_;
    static const uint64_t kFrameSeqNoInvalid;

    // Synchronization - UI thread will notify the emulator of new commands and
    // timestamp.  The worker will wait until the timestamp changes and
    // execute frames to maintain sync with the UI thread.
    std::condition_variable readyForFrame_;
    std::mutex frameMutex_;

    //  Toggle between frame memory buffers so that backendStateDelegate and frame
    //  write to and read from separate buffers, to minimize the time we need to
    //  keep the frame mutex between the two threads.
    struct LogOutputNode {
        int logLevel;
        unsigned sz; // size of log text following this struct in memory
        LogOutputNode *next;
    };

    struct LogInstructionNode {
        ClemensBackendExecutedInstruction *begin;
        ClemensBackendExecutedInstruction *end;
        LogInstructionNode *next;
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

        void copyFrom(ClemensMMIO &mmio, const ClemensDeviceIWM &iwm);
    };

    struct DOCStatus {
        /** PCM output (floating point per channel) */
        float voice[16];

        uint8_t reg[256];      /**< DOC register values */
        unsigned acc[32];      /**< Oscillator running accumulator */
        uint16_t ptr[32];      /**< Stored pointer from last cycle */
        uint8_t osc_flags[32]; /**< IRQ flagged */

        void copyFrom(const ClemensDeviceEnsoniq &doc);
    };

    struct ADBStatus {
        unsigned mod_states;
        uint16_t mouse_reg[4];

        void copyFrom(ClemensMMIO &mmio);
    };

    //  This state sticks around until processed by the UI frame - a hacky solution
    //  to the problem mentioned with FrameState.  In some cases we want to know
    //  when an event happened (command failed, termination, breakpoint hit, etc)
    struct LastCommandState {
        std::vector<ClemensBackendResult> results;
        std::optional<unsigned> hitBreakpoint;
        std::optional<std::string> message;
        std::optional<ClemensAppleIIGSConfig> gsConfig;
        LogOutputNode *logNode = nullptr;
        LogOutputNode *logNodeTail = nullptr;
        LogInstructionNode *logInstructionNode = nullptr;
        LogInstructionNode *logInstructionNodeTail = nullptr;
        cinek::ByteBuffer audioBuffer;
        bool isFastEmulationOn;
    };

    // This state comes in for any update to the emulator per frame.  As such
    // its possible to "lose" state if the emulator runs faster than the UI.
    // This is OK in most cases as the UI will only present this data per frame
    struct FrameState {
        ClemensClock emulatorClock;
        Clemens65C816 cpu;
        ClemensAppleIIGSFrame frame;
        std::array<std::string, CLEM_CARD_SLOT_COUNT> cards;

        IWMStatus iwm;
        DOCStatus doc;
        ADBStatus adb;

        uint8_t *memoryView = nullptr;
        uint8_t *docRAM = nullptr;
        uint8_t *ioPage = nullptr;
        uint8_t *bram = nullptr;
        uint8_t *e0bank = nullptr;
        uint8_t *e1bank = nullptr;

        ClemensBackendBreakpoint *breakpoints = nullptr;
        unsigned breakpointCount = 0;
        int logLevel;

        float machineSpeedMhz;
        float avgVBLsPerFrame;
        uint32_t vgcModeFlags;
        uint32_t irqs, nmis;
        uint8_t memoryViewBank = 0;

        unsigned backendCPUID;
        float fps;
        bool mmioWasInitialized = false;
        bool isTracing = false;
        bool isIWMTracing = false;
        bool isRunning = false;

        void copyState(const ClemensBackendState &state, LastCommandState &commandState,
                       cinek::FixedStack &frameMemory);
    };

    cinek::FixedStack frameWriteMemory_;
    cinek::FixedStack frameReadMemory_;
    FrameState frameWriteState_;
    FrameState frameReadState_;
    LastCommandState lastCommandState_;
    cinek::ByteBuffer thisFrameAudioBuffer_;

    ClemensCPUPins lastFrameCPUPins_;
    ClemensCPURegs lastFrameCPURegs_;
    ADBStatus lastFrameADBStatus_;
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

    std::string snapshotRootPath_;

  private:
    void doMachineDebugMemoryDisplay();
    void doMachineDebugDOCDisplay();
    void doMachineDebugVGCDisplay();
    void doMachineDebugCoreIODisplay();
    void doMachineDebugVideoIODisplay();
    void doMachineDebugDiskIODisplay();
    void doMachineDebugADBDisplay();
    void doMachineDebugSoundDisplay();
    void doMachineDebugIORegister(uint8_t *ioregsold, uint8_t *ioregs, uint8_t reg);

    enum class DebugIOMode { Core };
    DebugIOMode debugIOMode_;

    MemoryEditor debugMemoryEditor_;

    static ImU8 imguiMemoryEditorRead(const ImU8 *mem_ptr, size_t off);
    static void imguiMemoryEditorWrite(ImU8 *mem_ptr, size_t off, ImU8 value);

    int vgcDebugMinScanline_, vgcDebugMaxScanline_;

    std::array<ClemensHostJoystick, CLEM_HOST_JOYSTICK_LIMIT> joysticks_;
    unsigned joystickSlotCount_;

    void pollJoystickDevices();

  private:
    void doHelpScreen(int width, int height);

    bool isEmulatorStarting() const;
    bool isEmulatorActive() const;

    enum class GUIMode {
        Preamble,
        Setup,
        Emulator,
        LoadSnapshot,
        SaveSnapshot,

        Help,
        DiskBrowser,
        RebootEmulator,
        StartingEmulator,
        ShutdownEmulator
    };
    void setGUIMode(GUIMode guiMode);

    GUIMode guiMode_;
    GUIMode guiPrevMode_;

    std::string messageModalString_;

    ClemensLoadSnapshotUI loadSnapshotMode_;
    ClemensSaveSnapshotUI saveSnapshotMode_;
    ClemensDiskBrowser diskBrowserMode_;

    std::optional<ClemensDriveType> browseDriveType_;
    std::optional<float> delayRebootTimer_;
};

#endif
