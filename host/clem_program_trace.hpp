#ifndef CLEM_HOST_PROGRAM_TRACE_HPP
#define CLEM_HOST_PROGRAM_TRACE_HPP

#include <cstdint>
#include <utility>

#include <array>
#include <vector>

#include "clem_types.h"

struct ClemensTraceExecutedInstruction {
  uint32_t cycles_spent;
  uint32_t pc;
  uint32_t size;
  char opcode[4];
  char operand[24];
};



class ClemensProgramTrace
{
public:
  ClemensTraceExecutedInstruction& addExecutedInstruction(
    const ClemensInstruction& instruction,
    const ClemensMachine& machineState);

  void reset();

  void exportTrace(const char* filename);

private:
  uint64_t nextSeqNo_ = 0;

  //  <Execution Order> | <Bank>::<PC> | Opcode Operand
  struct Action {
    uint64_t seqno;
    ClemensTraceExecutedInstruction inst;
  };

  using ActionBank = std::array<Action, 65536>;
  std::vector<std::pair<unsigned, ActionBank>> actionBanks_;
};

#endif
