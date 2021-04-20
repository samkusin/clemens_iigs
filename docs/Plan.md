# Clements GS

## Summary

The Clements GS emulator's goal is to *emulate* the Apple IIgs design (ROM3
with ROM1 if possible compatibility.)


## TODO

This is a running todo list.

* C046 phase 1 - what's needed?  set to 0 for now?  bit 7 is mouse or diagnostic?
  * may depend if this is being accessed from an IRQ? (INTTYPE vs DIAGTYPE)
  * System IRQ line status
* Keyboard registers (ADB),c025
* ADB command 73
* C000 CLR80COL - disable 80 column store
* C001 SET80COL - enable 80 column store
* C002 RDMAINRAM - read main 48K
* C003 RDALTRAM - read alt 48K
* C004 WRMAINRAM - write main 48K
* C005 WRALTRAM - write alt 48K
* C041 INTEN - enable Mega II, VBL, quartersec interrupts
* C047 clearing timer interrupt flags for VBL and quartersec
* VGC pass 1 : text modes and switches
* ADB Mouse
* remaining //e softswitches
* Decimal math
* Test ROM 1
* VGC Pass 2 : Graphics modes (not SHGR)
* C039 Register (Serial Communications Command register )
* Border screen color
* Data accesses across banks are allowed (but not stack accesses?)
* Emulation vs Native on some microinstructions (i.e. choosing a long address mode while in emulation does... what?)
* Test ROM iteration?
* Ghosting banks?

## DONE

* Keyboard registers (ADB), c000, c010
* Expansion Slot ROM switching
* Add interrupt system IRQ
* ADB command/glu framework
* C02D Slot Register
* RTC BRAM reads and writes
* More debugging utilities
* RESET handling and CPU initialization
* Load ROM into an accessible region by emulated instructions
* Handle first batch of instructions based on ROM code
* XCE -> ROM bank conflict (PBR = bank 0, but on switch to native, what happens)
* Implement ADC and all addressing modes
* Implement LDA like ADC
* Implement ORA, etc (primary group)
* Implement branches
* Implement ASL, BIT
* Implement ROL, ROR, STX, STY, STZ
* Implement remaining opcodes (T??, JMP, stragglers)
* Implement interrupt opcodes (COP, BRK, CLI, RTI, etc)
* Stronger memory mapping
  * (using I/O switches)
  * shadowing
* Verify emulation <-> native switch initializes everything correctly


## Video Controller (VDC)

The host program is responsible for rendering video.  At a certain frequency
the host polls for video data.   The host will have buffer-like access to the
mega2 banks (e0, e1):
  40 column page (main)
  80 column page (aux)
  hires pages(0,1)
  Double hires pages(0,1)
  Super hires pages
