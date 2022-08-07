/* The Clemens Disassembler leverages the opcode definitions contained in the
   emulator portion.

   1. To use, create a disassembler context
   2. Set the context's current PC (program counter)
   3. Set the context's current status (emulation, accumulator, index size)
   4. Diassemble current instruction and return next *possible* PC values
      a) PC following current instruction
      b) PC if conditional branch was taken
*/
