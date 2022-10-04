#ifndef CLEM_HOST_UTILS_HPP
#define CLEM_HOST_UTILS_HPP

#include "clem_types.h"

#include <cstdint>

struct ClemensTraceExecutedInstruction {
  uint32_t cycles_spent;
  uint32_t pc;
  uint16_t size;
  char opcode[4];
  char operand[24];

  static void initialize();

  ClemensTraceExecutedInstruction& fromInstruction(
    const ClemensInstruction& instruction,
    const char* operand);
};

ClemensCard* createCard(const char* name);
void destroyCard(ClemensCard* card);

#endif
