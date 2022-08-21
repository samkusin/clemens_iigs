#ifndef CLEM_HOST_PROGRAM_TRACE_HPP
#define CLEM_HOST_PROGRAM_TRACE_HPP

#include <cstdint>
#include <utility>

#include <array>
#include <vector>

#include "clem_host_utils.hpp"

class ClemensProgramTrace
{
public:
  ClemensProgramTrace();

  void enableToolboxLogging(bool enable);

  ClemensTraceExecutedInstruction& addExecutedInstruction(
    const ClemensInstruction& instruction,
    const char* operand,
    const ClemensMachine& machineState);

  void reset();

  void exportTrace(const char* filename);

private:
  uint64_t nextSeq_ = 0;

  //  <Execution Order> | <Bank>::<PC> | Opcode Operand
  struct Action {
    uint32_t prev;
    uint32_t next;
    uint64_t seq;
    ClemensTraceExecutedInstruction inst;
    ClemensCPURegs regs;
    bool emulation;
  };

  uint32_t actionAnchor_;
  uint32_t actionCurrent_;
  std::vector<Action> actions_;
  std::vector<uint32_t> freeActionIndices_;

  struct Toolbox {
    uint16_t call;
    uint16_t pc;
    uint8_t pbr;
  };
  std::vector<Toolbox> toolboxCalls_;
  bool enableToolboxLogging_;
};

#endif
