#include "clem_program_trace.hpp"

#include <cstdio>
#include <cstdint>

#include <inttypes.h>

ClemensProgramTrace::ClemensProgramTrace()
{
  reset();
}


ClemensTraceExecutedInstruction& ClemensProgramTrace::addExecutedInstruction(
  const ClemensInstruction& instruction,
  const char* operand,
  const ClemensMachine& machineState
) {
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

void ClemensProgramTrace::exportTrace(const char* filename)
{
  const Action& anchor = actions_[actionAnchor_];
  unsigned actionIndex = anchor.next;

  char line[256];

  FILE* fp = fopen(filename, "w");

  while (actionIndex != actionAnchor_) {
    const Action& action = actions_[actionIndex];

    snprintf(line, sizeof(line),
             "%-14" PRIu64 " | %02X:%04X (%02u) %4s %s\n",
             action.seq, action.inst.cycles_spent,
             action.inst.pc >> 16, action.inst.pc & 0xffff,
             action.inst.opcode, action.inst.operand);

    fputs(line, fp);
    actionIndex = action.next;
  }
}
