#ifndef CLEM_HOST_SHARED_HPP
#define CLEM_HOST_SHARED_HPP

#include "clem_types.h"

#include <array>
#include <optional>
#include <string>

#define CLEM_HOST_LIBRARY_DIR "library"
#define CLEM_HOST_SNAPSHOT_DIR "snapshots"

struct ClemensBackendOutputText {
  int level;
  std::string text;
};

struct ClemensBackendExecutedInstruction {
  ClemensInstruction data;
  char operand[32];
};

struct ClemensBackendBreakpoint {
  enum Type {
    Undefined,
    Execute,
    DataRead,
    Write
  };
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
  enum class Type {
    Apple2GS
  };

  std::array<ClemensBackendDiskDriveState, kClemensDrive_Count> diskDriveStates;
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
    EjectDisk,
    Input,
    Break,
    AddBreakpoint,
    DelBreakpoint,
    WriteProtectDisk,
    DebugMemoryPage,
    WriteMemory
  };
  Type type = Undefined;
  std::string operand;
};


struct ClemensBackendState {
  ClemensMachine* machine;
  double fps;
  uint64_t seqno;
  bool isRunning;
  bool mmio_was_initialized;
  std::optional<bool> terminated;
  std::optional<bool> commandFailed;
  // valid if commandFailed
  std::optional<ClemensBackendCommand::Type> commandType;

  ClemensMonitor monitor;
  ClemensVideo text;
  ClemensVideo graphics;
  ClemensAudio audio;

  unsigned hostCPUID;

  const ClemensBackendOutputText* logBufferStart;
  const ClemensBackendOutputText* logBufferEnd;
  const ClemensBackendBreakpoint* bpBufferStart;
  const ClemensBackendBreakpoint* bpBufferEnd;
  std::optional<unsigned> bpHitIndex;
  const ClemensBackendDiskDriveState* diskDrives;
  const ClemensBackendExecutedInstruction* logInstructionStart;
  const ClemensBackendExecutedInstruction* logInstructionEnd;

  uint8_t ioPageValues[256];      // 0xc000 - 0xc0ff
  uint8_t debugMemoryPage;
};

#endif
