#ifndef CLEM_HOST_SHARED_HPP
#define CLEM_HOST_SHARED_HPP

#include "clem_mmio_types.h"

#include <array>
#include <chrono>
#include <optional>
#include <string>
#include <vector>

#define CLEM_HOST_LIBRARY_DIR  "library"
#define CLEM_HOST_SNAPSHOT_DIR "snapshots"
#define CLEM_HOST_TRACES_DIR   "traces"

constexpr const char *kClemensCardMockingboardName = "mockingboard_c";

struct ClemensBackendOutputText {
    int level;
    std::string text;
};

struct ClemensBackendExecutedInstruction {
    ClemensInstruction data;
    char operand[32];
};

struct ClemensBackendDiskDriveState {
    std::string imagePath;
    bool isWriteProtected;
    bool isSpinning;
    bool isEjecting;
    bool saveFailed;
};

struct ClemensBackendBreakpoint {
    enum Type { Undefined, Execute, DataRead, Write, IRQ, BRK };
    Type type;
    uint32_t address;
};

struct ClemensBackendConfig {
    enum class Type { Apple2GS };
    std::string dataRootPath;
    std::string diskLibraryRootPath;
    std::string snapshotRootPath;
    std::string traceRootPath;
    std::array<ClemensBackendDiskDriveState, kClemensDrive_Count> diskDriveStates;
    std::array<ClemensBackendDiskDriveState, 1> smartPortDriveStates;
    std::array<std::string, CLEM_CARD_SLOT_COUNT> cardNames;
    std::vector<ClemensBackendBreakpoint> breakpoints;
    unsigned audioSamplesPerSecond;
    unsigned ramSizeKB;
    int logLevel;
    bool enableFastEmulation;
    Type type;
};

enum class ClemensBackendMachineProperty {
    RegA,
    RegB,
    RegC,
    RegX,
    RegY,
    RegP,
    RegD,
    RegSP,
    RegDBR,
    RegPBR,
    RegPC
};

struct ClemensEmulatorStats {
    clem_clocks_time_t clocksSpent;
};

template <typename Stats> struct ClemensEmulatorDiagnostics {
    Stats stats;

    std::chrono::microseconds deltaTime; //  diagnostics current delta time from frame start
    std::chrono::microseconds frameTime; //  display diagnostics every frameTime seconds

    void reset(std::chrono::microseconds displayInterval) {
        stats = Stats{};
        deltaTime = std::chrono::microseconds::zero();
        frameTime = displayInterval;
    }

    std::pair<double, bool> update(std::chrono::microseconds deltaInterval) {
        deltaTime += deltaInterval;
        if (deltaTime >= frameTime) {
            using IntervalType = std::chrono::duration<double>;
            auto rateScalar = 1.0 / std::chrono::duration_cast<IntervalType>(deltaTime).count();
            return std::make_pair(rateScalar, true);
        }
        return std::make_pair(0.0, false);
    }
};

struct ClemensBackendCommand {
    enum Type {
        Undefined,
        Terminate,
        ResetMachine,
        RunMachine,
        StepMachine,
        InsertDisk,
        InsertBlankDisk,
        EjectDisk,
        Input,
        Break,
        AddBreakpoint,
        DelBreakpoint,
        WriteProtectDisk,
        DebugMemoryPage,
        WriteMemory,
        DebugLogLevel,
        DebugProgramTrace,
        SaveMachine,
        LoadMachine,
        RunScript,
        FastDiskEmulation,
        DebugMessage
    };
    Type type = Undefined;
    std::string operand;
};

struct ClemensBackendResult {
    ClemensBackendCommand cmd;
    enum Type { Succeeded, Failed };
    Type type;
};

struct ClemensBackendState {
    ClemensMachine *machine;
    ClemensMMIO *mmio;
    double fps;
    bool isRunning;
    bool isTracing;
    bool isIWMTracing;
    bool mmioWasInitialized;

    ClemensMonitor monitor;
    ClemensVideo text;
    ClemensVideo graphics;
    ClemensAudio audio;

    unsigned hostCPUID;
    int logLevel;
    const ClemensBackendOutputText *logBufferStart;
    const ClemensBackendOutputText *logBufferEnd;
    const ClemensBackendBreakpoint *bpBufferStart;
    const ClemensBackendBreakpoint *bpBufferEnd;
    std::optional<unsigned> bpHitIndex;
    const ClemensBackendDiskDriveState *diskDrives;
    const ClemensBackendDiskDriveState *smartDrives;
    const ClemensBackendExecutedInstruction *logInstructionStart;
    const ClemensBackendExecutedInstruction *logInstructionEnd;

    uint8_t ioPageValues[256]; // 0xc000 - 0xc0ff
    uint8_t debugMemoryPage;

    float machineSpeedMhz;
    float avgVBLsPerFrame;
    bool fastEmulationOn;

    // valid if a debugMessage() command was issued from the frontend
    std::optional<std::string> message;
};

#endif
