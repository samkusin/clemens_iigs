#include "clem_program_trace.hpp"

#include "clem_mmio_defs.h"

#include <array>
#include <cstdio>
#include <cstdint>
#include <cstring>

#include <inttypes.h>

static std::array<const char*, 0x20> kToolsetNames = {
  "Tool Locator",
  "Memory Manager",
  "Miscellaneous",
  "QuickDraw II",
  "Desk Manager",
  "Event Manager",
  "Scheduler",
  "Sound",
  "Apple Desktop Bus",
  "SANE",
  "Integer Math",
  "Text Tool",
  "Reserved for Apple use",
  "Window Manager",
  "Menu Manager",
  "Control Manager",
  "Loader",
  "QuickDraw II Auxillary",
  "Print Manager",
  "LineEdit",
  "Dialog Manager",
  "Scrap Manager",
  "Standard File Operations",
  "Disk Utilities",
  "Note Synthensizer",
  "Note Sequencer",
  "Font Manager",
  "List Manager",
  "Unknown",
  "Unknown",
  "Unknown"
};

ClemensProgramTrace::ClemensProgramTrace() :
  enableToolboxLogging_(false),
  enableIWMLogging_(false)
{
  reset();
}

void ClemensProgramTrace::enableToolboxLogging(bool enable) {
  enableToolboxLogging_ = enable;
}

void ClemensProgramTrace::enableIWMLogging(bool enable) {
  enableIWMLogging_ = enable;
}

ClemensTraceExecutedInstruction& ClemensProgramTrace::addExecutedInstruction(
  const ClemensInstruction& instruction,
  const char* operand,
  const ClemensMachine& machineState
) {
  /*if (machineState.cpu.regs.PC == 0x125 && machineState.cpu.regs.PBR == 0xfe) {
    printf("TBC: %04X (%s)\n", machineState.cpu.regs.X,
           kToolsetNames[(machineState.cpu.regs.X & 0xff)-1]);
  }
  */



  //  check instruction before and after our newly added instruction to see if
  //    there's overlap
  //    if overlap, convert those actions to misc bytes
  //  find where in the action list to insert our instruction
  uint32_t newCurrentActionIdx;
  if (freeActionIndices_.empty()) {
    actions_.emplace_back();
    newCurrentActionIdx = actions_.size() - 1;
  } else {
    newCurrentActionIdx = freeActionIndices_.back();
    freeActionIndices_.pop_back();
  }

  //  at this point, the actions_ vector will not be modified, so pointers are
  //  ok.
  Action* current = &actions_[newCurrentActionIdx];
  current->inst.fromInstruction(instruction, operand);
  memcpy(&current->regs, &machineState.cpu.regs, sizeof(ClemensCPURegs));
  current->emulation = machineState.cpu.pins.emulation;

  Action* prev = &actions_[actionCurrent_];
  Action* next = &actions_[prev->next];

  while (current) {
    if (prev->inst.pc > current->inst.pc && prev->seq != UINT64_MAX) {
      next = prev;
      prev = &actions_[prev->prev];
    } else if (current->inst.pc >= next->inst.pc && next->seq != UINT64_MAX) {
      prev = next;
      next = &actions_[next->next];
    } else {
      //  insert after prev and before next
      prev->next = newCurrentActionIdx;
      next->prev = newCurrentActionIdx;
      current->prev = (prev - actions_.data());
      current->next = (next - actions_.data());
      current->seq = nextSeq_;

      // if our current action overlaps any neighbors, remove them
      // this results in 'destroyed' actions if there are partial overlaps
      // (i.e. prev instruction overlaps the current)
      // it's POSSIBLE but highly unlikely that overlayed code overlapping
      // existing actions would result in valid executable code... well not
      // TOTALLY impossible.  but handling these overlaps is not trival and
      // until needed, won't be done here.
      if (prev->seq != UINT64_MAX) {
        if (prev->inst.pc + prev->inst.size > current->inst.pc) {
          freeActionIndices_.emplace_back(current->prev);
          current->prev = prev->prev;
          prev = &actions_[current->prev];
          prev->next = newCurrentActionIdx;
        }
      }
      if (next->seq != UINT64_MAX) {
        if (current->inst.pc + current->inst.size > next->inst.pc) {
          freeActionIndices_.emplace_back(current->next);
          current->next = next->next;
          next = &actions_[current->next];
          next->prev = newCurrentActionIdx;
        }
      }

      ++nextSeq_;
      actionCurrent_ = newCurrentActionIdx;
      current = nullptr;
    }
  }

  current = &actions_[newCurrentActionIdx];
  if (enableToolboxLogging_) {
    if (instruction.opc == CLEM_OPC_JSL && instruction.bank == 0xe1 && instruction.value == 0x0000) {
      toolboxCalls_.emplace_back();
      toolboxCalls_.back().call = machineState.cpu.regs.X;
      toolboxCalls_.back().pc = instruction.addr;
      toolboxCalls_.back().pbr = instruction.pbr;
    }
  }
  if (enableIWMLogging_ && machineState.cpu.pins.ioOut) {
    if (machineState.cpu.pins.vdaOut && (
      (machineState.cpu.pins.adr >= 0xc0e0 && machineState.cpu.pins.adr <= 0xc0ef) ||
      machineState.cpu.pins.adr == 0xc031)
    ) {
      memoryOps_.emplace_back();
      auto& ops = memoryOps_.back();
      ops.seq = current->seq;
      memcpy(ops.opname, instruction.desc->name, sizeof(ops.opname));
      ops.adr = machineState.cpu.pins.adr;
      ops.dbr = machineState.cpu.pins.bank;
      ops.pbr = instruction.pbr;
      ops.pc = instruction.addr;
      ops.value = machineState.cpu.pins.data;
    }
  }

  return current->inst;
}

void ClemensProgramTrace::reset()
{
  actions_.clear();
  freeActionIndices_.clear();

  actions_.emplace_back();
  actionAnchor_ = uint32_t(actions_.size() - 1);
  actionCurrent_ = actionAnchor_;
  actions_[actionAnchor_].prev = actionAnchor_;
  actions_[actionAnchor_].next = actionAnchor_;
  actions_[actionAnchor_].seq = UINT64_MAX;
}

bool ClemensProgramTrace::exportTrace(const char* filename)
{
  const Action& anchor = actions_[actionAnchor_];
  unsigned actionIndex = anchor.next;

  char line[256];

  FILE* fp = fopen(filename, "w");
  if (fp) {
    while (actionIndex != actionAnchor_) {
      const Action& action = actions_[actionIndex];
      char* out = &line[0];
      size_t outLeft = sizeof(line);
      int amt = snprintf(
              out, outLeft,
              "%16" PRIu64 " | %02X | %04X | (%2u) %4s %-10s | ",
              action.seq,
              action.inst.pc >> 16, action.inst.pc & 0xffff,
              action.inst.cycles_spent,
              action.inst.opcode, action.inst.operand);
      outLeft -= amt;
      out += amt;
      amt = snprintf(
              out, outLeft, "PC=%04X, PBR=%02X, DBR=%02X, S=%04X, D=%04X, e=%u, ",
              action.regs.PC, action.regs.PBR, action.regs.DBR,
              action.regs.S, action.regs.D, action.emulation ? 1 : 0);
      out += amt;
      outLeft -= amt;
      if (action.emulation) {
        amt = snprintf(
                out, outLeft, "A=%02X, X=%02X, Y=%02X, %c%c*%c%c%c%c%c",
                action.regs.A & 0xff, action.regs.X & 0xff, action.regs.Y & 0xff,
                (action.regs.P & kClemensCPUStatus_Negative) ? '1' : '0',
                (action.regs.P & kClemensCPUStatus_Overflow) ? '1' : '0',
                (action.regs.P & kClemensCPUStatus_EmulatedBrk) ? '1' : '0',
                (action.regs.P  & kClemensCPUStatus_Decimal) ? '1' : '0',
                (action.regs.P  & kClemensCPUStatus_IRQDisable) ? '1' : '0',
                (action.regs.P  & kClemensCPUStatus_Zero) ? '1' : '0',
                (action.regs.P  & kClemensCPUStatus_Carry) ? '1' : '0');
      } else {
          if (action.regs.P & kClemensCPUStatus_MemoryAccumulator) {
            amt = snprintf(
                out, outLeft, "A=%02X, ", action.regs.A & 0xff);
          } else {
            amt = snprintf(
                out, outLeft, "A=%04X, ", action.regs.A);
          }
          out += amt;
          outLeft -= amt;
          if (action.regs.P & kClemensCPUStatus_Index) {
            amt = snprintf(
                out, outLeft, "X=%02X, Y=%02X, ", action.regs.X & 0xff, action.regs.Y & 0xff);
          } else {
            amt = snprintf(
                out, outLeft, "X=%04X, Y=%04X, ", action.regs.X, action.regs.Y);
          }
          out += amt;
          outLeft -= amt;
          amt = snprintf(
                out, outLeft, "%c%c%c%c%c%c%c%c",
                (action.regs.P & kClemensCPUStatus_Negative) ? '1' : '0',
                (action.regs.P & kClemensCPUStatus_Overflow) ? '1' : '0',
                (action.regs.P & kClemensCPUStatus_MemoryAccumulator) ? '1' : '0',
                (action.regs.P & kClemensCPUStatus_Index) ? '1' : '0',
                (action.regs.P  & kClemensCPUStatus_Decimal) ? '1' : '0',
                (action.regs.P  & kClemensCPUStatus_IRQDisable) ? '1' : '0',
                (action.regs.P  & kClemensCPUStatus_Zero) ? '1' : '0',
                (action.regs.P  & kClemensCPUStatus_Carry) ? '1' : '0');
      }
      out += amt;
      outLeft -= amt;
      out[0] = '\n';
      out[1] = '\0';
      fputs(line, fp);
      actionIndex = action.next;
    }

    if (!toolboxCalls_.empty()) {
      fputs("\nTOOLBOX:\n=================================================\n", fp);

      for (auto& tbc: toolboxCalls_) {
        char* out = &line[0];
        size_t outLeft = sizeof(line);
        int amt;
        unsigned toolset = tbc.call - 1;
        if ((toolset & 0xff) >= kToolsetNames.size()) {
          amt = snprintf(out, outLeft, "%02X:%04X CALL #%04X ???", tbc.pbr, tbc.pc, tbc.call);
        } else {
          amt = snprintf(out, outLeft, "%02X:%04X CALL #%04X %s", tbc.pbr, tbc.pc, tbc.call,
                         kToolsetNames[toolset & 0xff]);

        }
        outLeft -= amt;
        out += amt;
        out[0] = '\n';
        out[1] = '\0';
        fputs(line, fp);
      }
    }

    if (!memoryOps_.empty()) {
      fputs("\nOPS:\n=================================================\n", fp);

      for (auto& ops: memoryOps_) {
        char* out = &line[0];
        size_t outLeft = sizeof(line);

        int amt = snprintf(out, outLeft, "%16" PRIu64 " %02X:%04X %s $%04X %02X", ops.seq, ops.pbr,
                           ops.pc, ops.opname, ops.adr, ops.value);

        outLeft -= amt;
        out += amt;
        out[0] = '\n';
        out[1] = '\0';
        fputs(line, fp);
      }
    }
    fclose(fp);
  }
  return fp != NULL;
}
