#ifndef CLEM_HOST_SHARED_HPP
#define CLEM_HOST_SHARED_HPP

#include "clem_types.h"

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

struct ClemensBackendBreakpoint {
    enum Type { Undefined, Execute, DataRead, Write, IRQ };
    Type type;
    uint32_t address;
};

struct ClemensBackendDiskDriveState {
    std::string imagePath;
    bool isWriteProtected;
    bool isSpinning;
    bool isEjecting;
    bool saveFailed;
};

struct ClemensBackendConfig {
    enum class Type { Apple2GS };
    std::string diskLibraryRootPath;
    std::array<ClemensBackendDiskDriveState, kClemensDrive_Count> diskDriveStates;
    std::array<ClemensBackendDiskDriveState, 1> smartPortDriveStates;
    std::array<std::string, 7> cardNames;
    std::vector<ClemensBackendBreakpoint> breakpoints;
    unsigned audioSamplesPerSecond;
    Type type;
};

struct ClemensBackendCommand {
    enum Type {
        Undefined,
        Terminate,
        SetHostUpdateFrequency,
        ResetMachine,
        RunMachine,
        StepMachine,
        Publish,
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
        DebugMessage,
        DebugProgramTrace,
        SaveMachine,
        LoadMachine
    };
    Type type = Undefined;
    std::string operand;
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

struct ClemensBackendState {
    ClemensMachine *machine;
    ClemensMMIO *mmio;
    double fps;
    uint64_t seqno;
    bool isRunning;
    bool isTracing;
    bool mmio_was_initialized;
    std::optional<bool> terminated;
    std::optional<bool> commandFailed;
    // valid if commandFailed
    std::optional<ClemensBackendCommand::Type> commandType;
    // valid if a debugMessage() command was issued from the frontend
    std::optional<std::string> message;

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
    const ClemensBackendExecutedInstruction *logInstructionStart;
    const ClemensBackendExecutedInstruction *logInstructionEnd;

    uint8_t ioPageValues[256]; // 0xc000 - 0xc0ff
    uint8_t debugMemoryPage;

    float emulatorSpeedMhz;
};

#endif
