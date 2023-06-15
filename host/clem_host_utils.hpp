#ifndef CLEM_HOST_UTILS_HPP
#define CLEM_HOST_UTILS_HPP

#include "clem_types.h"

#include <cstdint>

//  TODO: move into a new utility header for debugger tracing/listing

struct ClemensTraceExecutedInstruction {
    uint64_t seq;
    uint32_t cycles_spent;
    uint32_t pc;
    uint16_t size;
    char opcode[4];
    char operand[24];

    static void initialize();

    ClemensTraceExecutedInstruction &fromInstruction(const ClemensInstruction &instruction,
                                                     const char *operand);
};

#endif
