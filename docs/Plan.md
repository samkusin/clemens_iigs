# Clements GS

## Summary

The Clements GS emulator's goal is to *emulate* the Apple IIgs design (ROM3
with ROM1 if possible compatibility.)


## TODO

This is a running todo list.

* test cold reset
* look at typing to see if there are bugs and fix them at the basic prompt
* Border and text colors
  * C022 Text Color Mode
* Decimal math
* 80 column text (PR#3)
* First 5.25" disk boot (DOS 3.3)
* Writable disk data
* Look at fast vs slow for iigs specific IO registers (FPI, CYA based)
* Audio Phase I (just get the system beeps working, speaker clicks too)
* CLEM_MMIO_REG_VGC_IRQ_BYTE
* C01F 80COL yes/no state
* C07F? - AN3?
* C041 INTEN - enable Mega II, VBL, quartersec interrupts
* C047 clearing timer interrupt flags for VBL and quartersec
* ADB Mouse Support and Data Interrupts
* C046 phase 2 - Mouse
* remaining //e softswitches
* Test ROM 1
* VGC Pass 2 : Graphics modes (not SHGR)
* C039 Register (Serial Communications Command register )
* Data accesses across banks are allowed (but not stack accesses?)
* Audit Direct Page in emulation mode (wrap zero page, only if D=0000 for 6502
  instructions, but for 65816 instructions there is no wrapping?)
* Emulation vs Native on some microinstructions (i.e. choosing a long address mode while in emulation does... what?)
* Test ROM iteration?
* Ghosting banks?
* IWM 3.5" read write support
* VGC Pass 3: SHGR
* Ensoniq!
* ADB Mouse on Keypad Feature
* Flashing characters

## DONE

* Investigate crash - currently looking at SetWAPT during reset
  * Look at fe/079f (this method is where we call SetWAPT again for toolset 0x16)
* Display video
* VGC Pass 0.1 - Find apple ii fonts to use
* VGC Pass 0.2 - define API to access the text page
* VGC pass 1 : text modes and switches
* C019 (vblbar)
* IWM? Disk support? 5.25 for now...
* C036 Disk Motor Detector flags
* Quality of life host app improvements
* RTC register instructions
* Read write from empty ROM (for invalid banks)
* First pass Ensoniq research (ROM is getting to this point)
* C00C 40 column mode (W)
* C00D 80 column mode (W)
* CLEM_MMIO_REG_LANGSEL
* Add ADB SetMode, ClearMode, XMIT 2, UNDOCS 12, 13
* Add ADB Read Memory, Write Memory
  * RAM starts empty
  * ROM -- WIP?
* Added ADB GetVersion
* C050, C051 TXTSET
* C000 CLR80COL - disable 80 column store
  * TXTPAGE2 (R/W) refers to text page 2 ($800) in main memory
* C001 SET80COL - enable 80 column store
  * TXTPAGE2 (R/W) refers to aux text page for 80 column
* C054, C055 TXTPAGE1, TXTPAGE2
* C002 RDMAINRAM - read main 48K
* C003 RDALTRAM - read alt 48K
* C004 WRMAINRAM - write main 48K
* C005 WRALTRAM - write alt 48K
* VGC Pass 0 - research and defining data structures for host
* ADB command 73
* c046 System IRQ line status
* Keyboard registers (ADB), c000, c010, c025
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
