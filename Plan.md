# Clements GS

## Summary

The Clements GS emulator's goal is to *emulate* the Apple IIgs design (ROM3
with ROM1 if possible compatibility.)


## TODO

This is a running todo list.

* Implement remaining opcodes
* Implement interrupt opcodes (COP, BRK, CLI, RTI, etc)
* More debugging utility
* What is going on with the ROM writing to 01 and e1 banks (shadowing)
* Stronger memory mapping
  * (using I/O switches)
  * shadowing
* Data accesses across banks are allowed (but not stack accesses?)
* Emulation vs Native on some microinstructions (i.e. choosing a long address
  mode while in emulation does... what?)
* Decimal math

## DONE

* RESET handling and CPU initialization
* Load ROM into an accessible region by emulated instructions
* Handle first batch of instructions based on ROM code
* XCE -> ROM bank conflict (PBR = bank 0, but on switch to native, what happens)
* Implement ADC and all addressing modes
* Implement LDA like ADC
* Implement ORA, etc (primary group)
* Implement branches
* Implement ASL, BIT