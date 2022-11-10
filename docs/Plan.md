# Clemens GS

## Summary

The Clemens GS emulator's goal is to *emulate* the Apple IIgs design (ROM3
with ROM1 if possible compatibility.)

## TODO

This is a running todo list.

### Version 0.5 Wishlist

* Emulator detection of ROM version through introspection
* Speed up disk access when reading/writing (fast mode for emulation)
* Smartport Hard Drive Support
* ROM 01
* Joystick/Paddle Support
* Refactor tests with new test backend

### Miscellaneous

* CYA vs MEGA2 I/O access speeds
  * Some CYA registers are still "slow" so resolve these so they run at the IIgs
    fast speed
* Verify Mega II Interrupts work
* SCC Support
* Apple IIgs Diagnostic Test
* Monochrome Support
* Dblhires is offset by 4 pixels (1 block) in display due to conversion method
* Black and White DBLHIRES ($C029 bit 5)
* Data accesses across banks are allowed (but not stack accesses?)
* Audit Direct Page in emulation mode (wrap zero page, only if D=0000 for 6502
  instructions, but for 65816 instructions there is no wrapping?)
* Emulation vs Native on some microinstructions (i.e. choosing a long address mode while in emulation does... what?)
* Ghosting banks?
* ADB Mouse on Keypad Feature
* clipboard (paste)

## DONE

Listed in order of completion

* Flashing Characters
* Double Lores Mode (should be simple given how clem_display.cpp is written)
* Lores mode
* Linux support
* Clean up ADB support (Mouse)
* Fix Slot 3 I/O (defaulting to card vs internal ROM) - broke when Mockingboard
  support added
* New GUI Host (better debugging, more intuitive, quality of life)
* VGC Interrupt Support
* Better Apple II Speaker Support
* Ensoniq Support - http://www.buchty.net/ensoniq/5503.html
* Support 320x200 Super Hires Mode
  * C029 Bits 7 and 6 (Super Hires and Bank 01 memory layout)
  * Per scanline information to display color fill/320 vs 4 color 640
* Mockingboard Support (MB C no SSI-263)
* Support DSK and PO, 2IMG files
  * 2IMG serialization
  * Allow PO loading from the 2IMG system (for .po files)
* Enable custom logic for card slot reads/writes since different cards map
  registers to their dedicated page instead of at $c0xx
* Remove ClemensDrive dependencies on WOZ and 2IMG (ClemensNibbleDisk)
  * Serializer changes with new ClemensNibbleDisk and has_disk field for
    ClemensDrive
* IWM 3.5" write support
* Scan boot to slot 5 may not work vs direct boot to 5
* GUI improvements
* ProDOS 16 boot
* IWM 3.5" read support
* Fixed machine power cycle and 3.5" disks not being persisted
* ProDOS boot (1.x or ProDOS 8)
  * Fixed IWM drive enable after a delayed disable
* Fixed memory mapping bug for page 0x3 in main/aux banks (RAMRD)
* VGC Pass 3 : Dblhires Graphics (not SHGR)
  * Convert bit string for a scanline into colors
  * 4-bits per color, 7 bits per byte of data from bit 0 to 6 (7 ignored)
  * Alternate between aux and main (just like 80 column text)
  * Same scanlines as the hires mode
  * Use original IIgs conversion from composite to RGB
  * Test one double-hires title
* Support Disk switching in the IDE
* Capslock
* Save Battery RAM
* AN3 and Dblhires
* Load State
* fix audio restart
* Save state
* Some Simple Apple II games testing
  * Oregon Trail Plays
  * Sammy Lightfoot Plays
  * Ultima I plays (two disks)
* Fixed c08x write protect switches (and now Oregon Trail plays!)
* Fixed BRK and Decimal mode according to test failures
* Run 6502 test suite from Klaus
* Fixed c08x write enable switches
* Fixed boot on write-protect disabled (enbl2 minimal support)
* Commando gets to the QuickDOS loading screen but spins indefinitely at
  Track 34 after that
* Sammy Lightfoot starts
* Diagnosed Hard Hat Mack WOZ issue - likely it's the IIgs internal boot rom
  for slot 6 (ROM 03, maybe 1)
  * The 16 sector read from track 0 on boot is too long and the internal rom
    will timeout and invoke the Check Startup Disk error
  * At some point support Disk II card selection since there's no timeout there
* Fixed STA (dp), y timing bug (likely there are others...)
* Audio Phase I (just get the system beeps working, speaker clicks too)
* DOS newly formatted disk via INIT working (haven't tried booting yet..., but
  catalog, save/load, work...)
* Many simple instructions we're clocked at 1 cycle (incorrect - 2 cycles!)
  * INY/DEY, INX,DEX, TXA, etc.
* Timing Issues - fix clem_cycle bug!
  * STA a,X  4(5)
  * PLA, PLX, etc 3(4)
  * RTS 5(6)
* First 5.25" disk boot (DOS 3.3)
* Fixed stepper motor logic for 5.25" drive
* Fixed timing issues when disk motor turns on (slow mode)
* Fix timing per IWM frame so that we dont persist lag, since LSS cycles should
  consume all CPU cycles per frame
* audio registers (interrupt) need implementation
* Serial register read/write simple
* Fix various unimplemented stuff
* Get Control Panel Working
* RTC clock time (read)
* VGC Pass 2 : Lores Graphics (not SHGR)
* Fix Reset key = F11 vs Delete key
* Fix 80 column mode
* Fixed ROR Absolute (BUG!)
* Fix repeat/key issues
* Border and text colors
  * C022 Text Color Mode
* C01F 80COL yes/no state
* HGR -> TEXT fix
* Fixed bad TYA instruction for 16-bit A <- 8-bit Y register
  * a 2 day headache to debug a monitor memory set issue
  * ended up affecting boot time (faster now)
  * This could've been caught by tests...
* 80 column text (PR#3)
* look at typing to see if there are bugs and fix them at the basic prompt
* test cold reset
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
