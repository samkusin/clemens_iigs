
This has been a nice opportunity to implement lesser used assembler instructions
that deal with emulation -> native mode, and status flag setters/clears.

And a couple important opcodes, like JSR, RTS

1. Set $c029 bit 0 to 1
2. Set Interrupt Disable
3. Set Stack Pointer to 0x01fb
4. Invoke a native mode ROM subroutine
5. Native ROM routine
   1.
