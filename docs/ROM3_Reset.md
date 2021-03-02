
This has been a nice opportunity to implement lesser used assembler instructions
that deal with emulation -> native mode, and status flag setters/clears.

And a couple important opcodes, like JSR, RTS

1. Set $c029 bit 0 to 1
2. Set Interrupt Disable
3. Set Stack Pointer to 0x01fb
4. Some video/key code
5. Set up a simple jump table at $e10198...9f (two entries)
6. Check speed register $c035 in native mode
   1. Set to fast (bit 7)
7. Jump to FF7629
   1. LDA c015
   2. STA c007
8. Set STATEREG c068 bits 3 and 4
