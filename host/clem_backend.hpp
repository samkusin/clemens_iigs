#ifndef CLEM_HOST_BACKEND_HPP
#define CLEM_HOST_BACKEND_HPP

#include "cinek/fixedstack.hpp"
#include "clem_command_queue.hpp"
#include "clem_frame_state.hpp"
#include "clem_interpreter.hpp"

#include "cinek/circular_buffer.hpp"
#include "core/clem_apple2gs.hpp"

#include "clem_shared.h"
#include "core/clem_apple2gs_config.hpp"

#include <chrono>
#include <memory>
#include <optional>
#include <string>
#include <vector>

//  Forward Decls
class ClemensProgramTrace;

//
//  ClemensRunSampler controls the execution rate of and provides metrics for the
//  underlying machine.
//
struct ClemensRunSampler {
    std::chrono::microseconds referenceFrameTimer;
    std::chrono::microseconds actualFrameTimer;
    std::chrono::microseconds sampledFrameTime;

    double sampledFramesPerSecond;

    cinek::CircularBuffer<std::chrono::microseconds, 60> frameTimeBuffer;

    clem_clocks_time_t sampledClocksSpent;
    uint64_t sampledCyclesSpent;
    cinek::CircularBuffer<clem_clocks_duration_t, 60> clocksBuffer;
    cinek::CircularBuffer<clem_clocks_duration_t, 60> cyclesBuffer;

    double sampledMachineSpeedMhz;
    std::chrono::high_resolution_clock::time_point lastFrameTimePoint;

    double avgVBLsPerFrame;
    cinek::CircularBuffer<unsigned, 30> vblsBuffer;
    unsigned sampledVblsSpent;
    unsigned emulatorVblsPerFrame;
    bool fastModeEnabled;

    ClemensRunSampler();

    void reset();
    void update(clem_clocks_duration_t clocksSpent, unsigned cyclesSpent);
    void enableFastMode();
    void disableFastMode();
};

struct ClemensBackendConfig {
    int logLevel;
    std::string dataRootPath;
    std::string snapshotRootPath;
    std::string traceRootPath;
    std::vector<ClemensBackendBreakpoint> breakpoints;
    bool enableFastEmulation;

    enum class Type { Apple2GS };
    Type type;
    ClemensAppleIIGS::Config GS;
};

//  ClemensBackend controls the emulator and publishes its state to the frontend.
//      The ClemensAppleIIGS emulator module is constructed upon backend creation
//      or when a snapshot is loaded.
//      - The backend funnels frontend requests to ClemensAppleIIGS.
//      - In turn the backend reports ClemensAppleIIGS state to the frontend in
//        regular intervals.
//      - The backend also implements debugger functionality that controls and
//        views ClemensAppleIIGS state as well.
//
class ClemensBackend : public ClemensSystemListener, ClemensCommandQueueListener {
  public:
    using Config = ClemensBackendConfig;

    ClemensBackend(std::string romPath, const Config &config);
    virtual ~ClemensBackend();

    //  Executes a single emulator timeslice
    ClemensCommandQueue::DispatchResult step(ClemensCommandQueue& commands);
    //  Populate a backend state
    void post(ClemensBackendState& backendState);

    //  these methods do not queue instructions to execute on the runner
    //  and must be executed instead on the runner thread.  They are made public
    //  for access by ClemensInterpreter
    using MachineProperty = ClemensBackendMachineProperty;
    //  Properties can be 8/16 or 32-bit.   Registers are 8/16 bit.  The incoming
    //  value is truncated by masking/downcast from 32-bit accordingly.
    void assignPropertyToU32(MachineProperty property, uint32_t value);

    //  this method will poll the machine for the config and return it here
    //  for saving if necessary
    bool queryConfig(ClemensAppleIIGSConfig &config);

  private:
    // ClemensSystemListener
    void onClemensSystemMachineLog(int logLevel, const ClemensMachine *machine,
                                   const char *msg) final;
    void onClemensSystemLocalLog(int logLevel, const char *msg) final;
    void onClemensSystemWriteConfig(const ClemensAppleIIGS::Config &config) final;
    void onClemensInstruction(struct ClemensInstruction *inst, const char *operand);

    //  ClemensCommandQueueListener
    void onCommandReset() final;
    void onCommandRun() final;
    void onCommandBreakExecution() final;
    void onCommandStep(unsigned count) final;
    void onCommandAddBreakpoint(const ClemensBackendBreakpoint &breakpoint) final;
    bool onCommandRemoveBreakpoint(int index) final;
    void onCommandInputEvent(const ClemensInputEvent &inputEvent) final;
    bool onCommandInsertDisk(ClemensDriveType driveType, std::string diskPath) final;
    void onCommandEjectDisk(ClemensDriveType driveType) final;
    bool onCommandWriteProtectDisk(ClemensDriveType driveType, bool wp) final;
    bool onCommandInsertSmartPortDisk(unsigned driveIndex, std::string diskPath) final;
    void onCommandEjectSmartPortDisk(unsigned driveIndex) final;
    void onCommandDebugMemoryPage(uint8_t pageIndex) final;
    void onCommandDebugMemoryWrite(uint16_t addr, uint8_t value) final;
    void onCommandDebugLogLevel(int logLevel) final;
    bool onCommandDebugProgramTrace(std::string_view op, std::string_view path) final;
    bool onCommandSaveMachine(std::string path) final;
    bool onCommandLoadMachine(std::string path) final;
    bool onCommandRunScript(std::string command) final;
    void onCommandFastDiskEmulation(bool enabled) final;
    std::string onCommandDebugMessage(std::string msg) final;
    void onCommandSendText(std::string msg) final;

    //  internal
    bool isRunning() const;
    std::optional<unsigned> checkHitBreakpoint();
    template <typename... Args> void localLog(int log_level, const char *msg, Args... args);

    bool serialize(const std::string &path) const;
    bool unserialize(const std::string &path);
    void updateRTC();

  private:
    Config config_;
    ClemensAppleIIGSConfig gsConfig_;

    std::unique_ptr<ClemensAppleIIGS> GS_;
    bool gsConfigUpdated_;

    ClemensInterpreter interpreter_;
    std::vector<ClemensBackendOutputText> logOutput_;
    std::vector<ClemensBackendBreakpoint> breakpoints_;
    std::vector<ClemensBackendExecutedInstruction> loggedInstructions_;
    std::optional<unsigned> hitBreakpoint_;

    uint64_t nextTraceSeq_;
    std::unique_ptr<ClemensProgramTrace> programTrace_;

    int logLevel_;
    uint8_t debugMemoryPage_;
    bool areInstructionsLogged_;

    ClemensRunSampler runSampler_;

    std::optional<int> stepsRemaining_;
    int64_t clocksRemainingInTimeslice_;
    uint64_t clocksInSecondPeriod_;

    std::string clipboardText_;
    unsigned clipboardHead_;
};

#endif
