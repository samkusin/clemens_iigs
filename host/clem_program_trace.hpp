#ifndef CLEM_HOST_PROGRAM_TRACE_HPP
#define CLEM_HOST_PROGRAM_TRACE_HPP

#include <cstdint>
#include <utility>

#include <array>
#include <vector>

#include "clem_host_utils.hpp"

class ClemensProgramTrace {
  public:
    ClemensProgramTrace();

    void enableToolboxLogging(bool enable);
    void enableIWMLogging(bool enable);

    bool isToolboxLoggingEnabled() const { return enableToolboxLogging_; }
    bool isIWMLoggingEnabled() const { return enableIWMLogging_; }

    ClemensTraceExecutedInstruction &addExecutedInstruction(uint64_t seq,
                                                            const ClemensInstruction &instruction,
                                                            const char *operand,
                                                            const ClemensMachine &machineState);

    void reset();

    bool exportTrace(const char *filename);

  private:
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

    struct MemoryOperation {
        uint64_t seq;
        char opname[4];
        uint16_t pc;
        uint16_t adr;
        uint8_t pbr;
        uint8_t dbr;
        uint8_t value; //  this could be parts of a 16-bit value
    };
    std::vector<MemoryOperation> memoryOps_;

    bool enableToolboxLogging_;
    bool enableIWMLogging_;
};

#endif
