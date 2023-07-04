#include "clem_host_utils.hpp"
#include "devices/mockingboard.h"

#include <cstring>

static uint32_t kAddrModeSizes[kClemensCPUAddrMode_Count];

void ClemensTraceExecutedInstruction::initialize() {
    // these are 8-bit sizes that are augmented by the instruction opcode width
    // (opc8)
    kAddrModeSizes[kClemensCPUAddrMode_None] = 1;
    kAddrModeSizes[kClemensCPUAddrMode_Immediate] = 2;
    kAddrModeSizes[kClemensCPUAddrMode_Absolute] = 3;
    kAddrModeSizes[kClemensCPUAddrMode_AbsoluteLong] = 4;
    kAddrModeSizes[kClemensCPUAddrMode_DirectPage] = 2;
    kAddrModeSizes[kClemensCPUAddrMode_DirectPageIndirect] = 2;
    kAddrModeSizes[kClemensCPUAddrMode_DirectPageIndirectLong] = 2;
    kAddrModeSizes[kClemensCPUAddrMode_Absolute_X] = 3;
    kAddrModeSizes[kClemensCPUAddrMode_AbsoluteLong_X] = 4;
    kAddrModeSizes[kClemensCPUAddrMode_Absolute_Y] = 3;
    kAddrModeSizes[kClemensCPUAddrMode_DirectPage_X] = 2;
    kAddrModeSizes[kClemensCPUAddrMode_DirectPage_Y] = 2;
    kAddrModeSizes[kClemensCPUAddrMode_DirectPage_X_Indirect] = 2;
    kAddrModeSizes[kClemensCPUAddrMode_DirectPage_Indirect_Y] = 2;
    kAddrModeSizes[kClemensCPUAddrMode_DirectPage_IndirectLong_Y] = 2;
    kAddrModeSizes[kClemensCPUAddrMode_MoveBlock] = 3;
    kAddrModeSizes[kClemensCPUAddrMode_Stack_Relative] = 2;
    kAddrModeSizes[kClemensCPUAddrMode_Stack_Relative_Indirect_Y] = 2;
    kAddrModeSizes[kClemensCPUAddrMode_PCRelative] = 2;
    kAddrModeSizes[kClemensCPUAddrMode_PCRelativeLong] = 3;
    kAddrModeSizes[kClemensCPUAddrMode_PC] = 3;
    kAddrModeSizes[kClemensCPUAddrMode_PCIndirect] = 3;
    kAddrModeSizes[kClemensCPUAddrMode_PCIndirect_X] = 3;
    kAddrModeSizes[kClemensCPUAddrMode_PCLong] = 4;
    kAddrModeSizes[kClemensCPUAddrMode_PCLongIndirect] = 3;
    kAddrModeSizes[kClemensCPUAddrMode_Operand] = 2;
}

ClemensTraceExecutedInstruction &
ClemensTraceExecutedInstruction::fromInstruction(const ClemensInstruction &instruction,
                                                 const char *oper) {
    strncpy(opcode, instruction.desc->name, sizeof(opcode));
    opcode[sizeof(opcode) - 1] = '\0';
    strncpy(operand, oper, sizeof(operand) - 1);
    operand[sizeof(operand) - 1] = '\0';
    cycles_spent = instruction.cycles_spent;
    pc = (uint32_t(instruction.pbr) << 16) | instruction.addr;
    size = kAddrModeSizes[instruction.desc->addr_mode];

    return *this;
}
