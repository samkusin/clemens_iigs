#ifndef CLEM_HOST_FRONT_HPP
#define CLEM_HOST_FRONT_HPP

#include "cinek/keyframe.hpp"
#include "clem_assets.hpp"
#include "clem_command_queue.hpp"
#include "clem_disk.h"
#include "clem_host_platform.h"
#include "clem_host_view.hpp"

#include "clem_asset_browser.hpp"
#include "clem_configuration.hpp"
#include "clem_debugger.hpp"
#include "clem_display.hpp"
#include "clem_frame_state.hpp"
#include "clem_ui_load_snapshot.hpp"
#include "clem_ui_save_snapshot.hpp"
#include "clem_ui_settings.hpp"

#include "cinek/buffer.hpp"
#include "cinek/circular_buffer.hpp"
#include "cinek/equation.hpp"
#include "cinek/fixedstack.hpp"
#include "clem_audio.hpp"
#include "core/clem_apple2gs_config.hpp"
#include "imgui.h"

#include <array>
#include <condition_variable>
#include <deque>
#include <memory>
#include <mutex>
#include <optional>
#include <thread>
#include <vector>

struct ImFont;

class ClemensBackend;
struct ClemensBackendState;

class ClemensFrontend : public ClemensHostView, ClemensDebuggerListener {
  public:
    ClemensFrontend(ClemensConfiguration config, const cinek::ByteBuffer &systemFontLoBuffer,
                    const cinek::ByteBuffer &systemFontHiBuffer);
    ~ClemensFrontend();

    ViewType getViewType() const final { return ViewType::Main; }
    ViewType frame(int width, int height, double deltaTime, ClemensHostInterop &interop) final;
    void input(ClemensInputEvent input) final;
    bool emulatorHasFocus() const final;
    void pasteText(const char *text, unsigned textSizeLimit) final;
    void lostFocus() final;

  private:
    void onDebuggerCommandReboot() override;
    void onDebuggerCommandShutdown() override;
    void onDebuggerCommandPaste() override;

  private:
    template <typename TBufferType> friend struct FormatView;

    struct ViewToMonitorTranslation {
        ImVec2 screenUVs;
        ImVec2 size;
        ImVec2 workSize;
    };

    void startBackend();
    void runBackend(std::unique_ptr<ClemensBackend> backend);
    void stopBackend();
    bool syncBackend(bool copyState);
    bool isBackendRunning() const;

    //  the backend state delegate is run on a separate thread and notifies
    //  when a frame has been published
    void processBackendResult(const ClemensBackendResult &result);

    void doEmulatorInterface(ImVec2 anchor, ImVec2 dimensions,
                             const ViewToMonitorTranslation &viewToMonitor, double deltaTime);

    void doDebuggerLayout(ImVec2 anchor, ImVec2 dimensions,
                          const ViewToMonitorTranslation &viewToMonitor);

    void doMainMenu(ImVec2 &anchor, ClemensHostInterop &interop);
    void doSidePanelLayout(ImVec2 anchor, ImVec2 dimensions);
    void doMachinePeripheralDisplay(float width);
    void doDebuggerQuickbar(float width);
    void doInfoStatusLayout(ImVec2 anchor, ImVec2 dimensions, float dividerXPos);
    void doViewInputInstructions(ImVec2 dimensions);

    void doMachineDiagnosticsDisplay();
    void doMachineDiskDisplay(float width);
    void doMachineDiskStatus(ClemensDriveType driveType, float width);
    void doMachineSmartDriveStatus(unsigned driveIndex, const char* label, bool allowSelect, bool allowHotswap);

    void doMachineDiskMotorStatus(const ImVec2 &pos, const ImVec2 &size, bool isSpinning);

    void doDebugView(ImVec2 anchor, ImVec2 size);
    void doSetupUI(ImVec2 anchor, ImVec2 dimensions);
    void doMachineViewLayout(ImVec2 rootAnchor, ImVec2 rootSize,
                             const ViewToMonitorTranslation &viewToMonitor);

    void doMachineDiskBrowserInterface(ImVec2 anchor, ImVec2 dimensions);
    void doMachineSmartDiskBrowserInterface(ImVec2 anchor, ImVec2 dimensions);

    void layoutTerminalLines();
    void layoutConsoleLines();

    template <typename... Args> void doModalError(const char *msg, Args... args);

    std::pair<std::string, bool> importDisks(std::string outputPath, std::string name,
                                             ClemensDriveType driveType,
                                             std::vector<std::string> imagePaths);

    void rebootInternal();

  private:
    ClemensConfiguration config_;
    ClemensDisplayProvider displayProvider_;

    ClemensDisplay display_;
    ClemensAudioDevice audio_;
    ClemensBackendState backendState_;
    ClemensCommandQueue::DispatchResult backendCommandResutls_;

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
    double dtEmulatorNextUpdateInterval_;

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

    cinek::FixedStack frameReadMemory_;
    ClemensFrame::FrameState frameReadState_;
    ClemensFrame::LastCommandState lastCommandState_;
    cinek::ByteBuffer thisFrameAudioBuffer_;

    ClemensCPUPins lastFrameCPUPins_;
    ClemensCPURegs lastFrameCPURegs_;
    ClemensFrame::ADBStatus lastFrameADBStatus_;
    ClemensFrame::IWMStatus lastFrameIWM_;
    uint32_t lastFrameIRQs_, lastFrameNMIs_;
    uint8_t lastFrameIORegs_[256];
    bool emulatorHasKeyboardFocus_;
    bool emulatorHasMouseFocus_;
    bool mouseInEmulatorScreen_;
    bool pasteClipboardToEmulator_;

    std::string snapshotRootPath_;

  private:
    enum class DebugIOMode { Core };
    DebugIOMode debugIOMode_;

    int vgcDebugMinScanline_, vgcDebugMaxScanline_;

    std::array<ClemensHostJoystick, CLEM_HOST_JOYSTICK_LIMIT> joysticks_;
    unsigned joystickSlotCount_;

    void pollJoystickDevices();

  private:
    void doHelpScreen(int width, int height);

    bool isEmulatorStarting() const;
    bool isEmulatorActive() const;

    enum class GUIMode {
        None,
        Preamble,
        Setup,
        Emulator,
        LoadSnapshot,
        LoadSnapshotAfterPowerOn,
        SaveSnapshot,
        Help,
        HelpShortcuts,
        HelpDisk,
        RebootEmulator,
        StartingEmulator,
        ShutdownEmulator
    };
    void setGUIMode(GUIMode guiMode);

    GUIMode guiMode_;
    GUIMode guiPrevMode_;
    double appTime_;
    double nextUIFlashCycleAppTime_;
    float uiFlashAlpha_;

    std::string messageModalString_;

    ClemensDebugger debugger_;
    ClemensLoadSnapshotUI loadSnapshotMode_;
    ClemensSaveSnapshotUI saveSnapshotMode_;
    ClemensAssetBrowser assetBrowser_;
    ClemensSettingsUI settingsView_;

    std::optional<ClemensDriveType> browseDriveType_;
    std::optional<unsigned> browseSmartDriveIndex_;
    std::optional<float> delayRebootTimer_;
    std::array<std::string, kClemensDrive_Count> savedDiskBrowsePaths_;
    std::array<std::string, kClemensSmartPortDiskLimit> savedSmartDiskBrowsePaths_;

    struct Animation {
        using Keyframe = cinek::keyframe<ImVec2>;
        Keyframe a; //  t = 0
        Keyframe b; //  t = end
        cinek::equation<ImVec2> transition;
        enum class Mode { A, AToB, BToA, B };
        Mode mode = Mode::A;
        double t = 0.0f;
    };

    Animation diskWidgetAnimations_[kClemensDrive_Count];
    Animation smartPortWidgetAnimation_;

    //  Used for debugging client-side features (i.e. audio playback, framerate)
    struct DebugDiagnostics {
        int16_t mouseX = 0;
        int16_t mouseY = 0;
    };
    DebugDiagnostics diagnostics_;
};

#endif
