#ifndef CLEM_HOST_SHARED_HPP
#define CLEM_HOST_SHARED_HPP

#include "clem_disk.h"
#include "clem_mmio_types.h"
#include "clem_smartport.h"

#include <array>
#include <chrono>
#include <optional>
#include <string>
#include <vector>

#define CLEM_HOST_LIBRARY_DIR  "library"
#define CLEM_HOST_SNAPSHOT_DIR "snapshots"
#define CLEM_HOST_TRACES_DIR   "traces"

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

struct ClemensBackendCommand {
    enum Type {
        Undefined,
        Terminate,
        ResetMachine,
        RunMachine,
        StepMachine,
        InsertDisk,
        EjectDisk,
        InsertSmartPortDisk,
        EjectSmartPortDisk,
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
        DebugMessage,
        SendText,
        SaveBinary,
        LoadBinary,
        FastMode
    };
    Type type = Undefined;
    std::string operand;
};

struct ClemensBackendResult {
    ClemensBackendCommand cmd;
    enum Type { Succeeded, Failed };
    Type type;
};

#endif
