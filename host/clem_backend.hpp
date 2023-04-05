#ifndef CLEM_HOST_BACKEND_HPP
#define CLEM_HOST_BACKEND_HPP

#include "clem_command_queue.hpp"
#include "clem_host_shared.hpp"
#include "clem_interpreter.hpp"
#include "clem_smartport_disk.hpp"

#include "cinek/buffer.hpp"
#include "cinek/fixedstack.hpp"
#include "clem_woz.h"

#include <array>
#include <chrono>
#include <condition_variable>
#include <deque>
#include <functional>
#include <string>
#include <string_view>
#include <vector>

class ClemensProgramTrace;

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

//  TODO: Machine type logic could be subclassed into an Apple2GS backend, etc.
class ClemensBackend : public ClemensCommandQueueListener {
  public:
    using Config = ClemensBackendConfig;

    //  The PublishStateDelegate provides backend state to the front end GUI
    //  Data passed through the delegate is guaranteed to be valid within its
    //    scope.  The front-end should copy what it needs for its display during
    //    the delegate's scope. Once the delegate has finished, the data in
    //    ClemensBackendState should be considered invalid/undefined.
    using PublishStateDelegate =
        std::function<void(ClemensCommandQueue &, const ClemensCommandQueue::ResultBuffer &,
                           const ClemensBackendState &)>;
    ClemensBackend(std::string romPathname, const Config &config);
    ~ClemensBackend();

    ClemensCommandQueue::DispatchResult
    main(ClemensBackendState &backendState, const ClemensCommandQueue::ResultBuffer &commandResults,
         PublishStateDelegate delegate);

    //  these methods do not queue instructions to execute on the runner
    //  and must be executed instead on the runner thread.  They are made public
    //  for access by ClemensInterpreter
    using MachineProperty = ClemensBackendMachineProperty;
    //  Properties can be 8/16 or 32-bit.   Registers are 8/16 bit.  The incoming
    //  value is truncated by masking/downcast from 32-bit accordingly.
    void assignPropertyToU32(MachineProperty property, uint32_t value);

  private:
    void onCommandReset() final;
    void onCommandRun() final;
    void onCommandBreakExecution() final;
    void onCommandStep(unsigned count) final;
    void onCommandAddBreakpoint(const ClemensBackendBreakpoint &breakpoint) final;
    bool onCommandRemoveBreakpoint(int index) final;
    void onCommandInputEvent(const ClemensInputEvent &inputEvent) final;
    bool onCommandInsertDisk(ClemensDriveType driveType, std::string diskPath) final;
    bool onCommandInsertBlankDisk(ClemensDriveType driveType, std::string diskPath) final;
    void onCommandEjectDisk(ClemensDriveType driveType) final;
    bool onCommandWriteProtectDisk(ClemensDriveType driveType, bool wp) final;
    bool onCommandInsertSmartPortDisk(unsigned driveIndex, std::string diskPath) final;
    bool onCommandInsertBlankSmartPortDisk(unsigned driveIndex, std::string diskPath) final;
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

    std::optional<unsigned> checkHitBreakpoint();
    bool isRunning() const;

    void initEmulatedDiskLocalStorage();
    bool loadDisk(ClemensDriveType driveType, bool allowBlank);
    bool saveDisk(ClemensDriveType driveType);

    void createSmartPortDisk(unsigned driveindex);
    bool loadSmartPortDisk(unsigned driveIndex);
    bool saveSmartPortDisk(unsigned driveIndex);
    bool removeSmartPortDisk(unsigned driveIndex);

    cinek::ByteBuffer loadROM(const char *romPathname);

    //  TODO: These methods could be moved into a subclass as they are specific
    //        to machine type
    void initApple2GS();
    void loadBRAM();
    void saveBRAM();

    template <typename... Args> void localLog(int log_level, const char *msg, Args... args);

    static void emulatorLog(int log_level, ClemensMachine *machine, const char *msg);
    static void emulatorOpcodeCallback(struct ClemensInstruction *inst, const char *operand,
                                       void *this_ptr);
    static uint8_t *unserializeAllocate(unsigned sz, void *context);

  private:
    Config config_;

    //  memory allocated once for the machine
    cinek::FixedStack slabMemory_;
    //  the actual machine object
    cinek::ByteBuffer romBuffer_;
    cinek::ByteBuffer diskBuffer_;
    ClemensMachine machine_;
    ClemensMMIO mmio_;
    ClemensCard *mockingboard_;

    ClemensInterpreter interpreter_;

    std::vector<ClemensBackendOutputText> logOutput_;
    std::vector<ClemensBackendBreakpoint> breakpoints_;
    std::vector<ClemensBackendExecutedInstruction> loggedInstructions_;
    std::array<ClemensWOZDisk, kClemensDrive_Count> diskContainers_;
    std::array<ClemensNibbleDisk, kClemensDrive_Count> disks_;
    std::array<ClemensBackendDiskDriveState, kClemensDrive_Count> diskDrives_;
    std::array<ClemensBackendDiskDriveState, 1> smartPortDrives_;
    std::array<ClemensSmartPortDisk, 1> smartPortDisks_;

    uint64_t nextTraceSeq_;
    std::unique_ptr<ClemensProgramTrace> programTrace_;

    int logLevel_;
    uint8_t debugMemoryPage_;
    bool areInstructionsLogged_;

    ClemensRunSampler runSampler_;

    std::optional<int> stepsRemaining_;
    int64_t clocksRemainingInTimeslice_;
};

#endif
