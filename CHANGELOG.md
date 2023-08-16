# Changelog

See [planned features](https://github.com/samkusin/clemens_iigs/issues?q=is%3Aissue+is%3Aopen+label%3Aenhancement) for a list of TODOs.   Feel free to add to the list.

## Version 0.6

### Features

* User view mode where IIgs display is maximized
* Debugger mode GUI cleaned up (may be missing some features from prior versions)
* Snapshots and other user data now saved to a specific location set by the user
* Removed disk import feature and redesigned disk selection GUI
* Slot 7 (ProDOS only) hard drive emulation
* macOS Catalina and above support
* Gamepad support on all supported platforms
* Fast disk emulation where the machine is sped up during read and write operations
* Fast emulation mode (not the most optimal, but it works to get 10-20x speed)
* Partial SCC emluation to pass diagnostic tests
* IIgs GSOS/Finder/Toolbox mouse to host desktop tracking
* Paste text as keyboard input (does not work correctly on GSOS)
* Debugger bload and bsave functionality
* Linux Flatpak

### Fixes

* Fixed snapshot save issues
* Fixed Apple II hires and double hires RGB emulation issues
* Better 640 resolution mode support (no dithering still...)
* Better support for per scanline color
* VGC scanline and vertical sync/IRQ handling issues fixed
* Fixed softswitches RDMIX and STATEREG.PAGE2
* CPU and bus clock counters all based on an ideal master frequency of 28.636 Mhz, including 1Mhz stretch cycles
* Apple II style floating bus
* IWM refactored to reflect changes in clock frequency handling
* Debugger: faster instruction tracing
* DOC/Ensoniq fixes to pass diagnostic tests
* Audio sync and lower playback latency
* ADB SRQ/TALK keyboard functionality and system reset
* 16-bit DBR based read and writes on bank boundaries operate on the next bank
* Fixed page wrap logic to only occur if DL=0 on emulation mode
* Clear key strobe on write to c010-1f
* Ensure shadow map on writes is valid based on the final write page (bank 0,1 relevant)
* 3.5" read/write bit cell timing fix for copy protection on certain titles
* Disk switch detection for 3.5 and hard drives improved
* WOZ CRC generation
* RTC uses local timezone vs GMT clock
* WAI fix
* Removed s2d1 (IWM smartport) GUI (experimental feature still not fully developed)

### Tested Devices

All 60 fps emulation when not in Fast disk mode

* i5 2 core 1.4 Ghz (Ubuntu, 2011 Macbook Air 4GB)
* i3 2 core 2.3 Ghz (macOS Catalina Hackintosh, 2016 Intel NUC6i3SYH 8GB)
* i7 4 core (Windows 10 and Manjaro Dual Boot, 2016 i7 Desktop, 32GB)
* i7-10750H 6 core 2.6 Ghz (Windows 11, 2021 Dell XPS 15 9500, 16GB)
* M2 Macbook Air (macOS Ventura, 16GB)

### Known Issues

* GSOS boot on freshly installed hard drive images may crash to the monitor on
  first couple startups
  * The problem "goes away" after this  
  * Under investigation
* Textfunk results in MAME-like results vs KEGS and Crossrunner
* Mockingboard emulation may slow down the system due to an inefficient VIA implementation

### Planned for 0.7

* French keyboards
* Debugger improvements
* General GUI cleanup


## Version 0.5

* SmartPort 32MB Hard Drive on the disk port, virtual slot 2, drive 1
* Gamepad and Joystick support on Windows (DirectInput) and Linux
* ADB fix allowing Ctrl-RightAlt-F1 to trigger the Control Panel
* IWM properly report spindle status on each drive
* Decouple Apple IIGS device types and APIs from the 65816 emulator library into its own library
* Moved some graphics mode bitmap rendering into the library
* Added register set commands in the debugger

## Version 0.4

* New Debugger GUI and improved diagnostics display
* New Disk Import/Save/Load system for disk images (save/load as WOZ image)
* Mouse Support
* Lores and Double Lores Support
* Linux Builds with GCC using OpenGL for Rendering
* Various Fixes
  * Fixed Super Hires Renderer to display correct colors
  * Ensoniq fix that didn't account for the default voice (Neuromancer music plays)
  * Fixed confusion between INTCXROM and SLOTROMSEL switches
  * IRQs in native mode were firing the emulated IRQBRK vector
  * Incorrect sync timing lead to invalid Vertical and Horizontal VGC counters
  * Corrected BIT immediate flag manipulation
  * Fixed 3.5" drive emulation disk-in-place query value (allowing GS/OS to boot)
* Flashing Text
* List toolbox calls in program trace
* Updated dependencies
  *  `ImGui` to `1.88` and
  *  `fmt` to `9.1.0`
  *  `sokol` to latest

## Version 0.3

* Super Hi-res mode and ProDOS 16 boot to finder
* 2MG 3.5 and 5.25" support
* DO, PO and DSK Load and Save
* Mockingboard C without SSI-223 (voice) support
* Improved Apple II Speaker support (still flawed but better)
* Boots Nox Archaist and various Apple II 128K games
* Ensoniq support - boots Tower of Myraglen

## Version 0.2

* Serialization (Save/Load emulation state)
* Various debugging improvements, breakpoints, trace logging
* Baseline Apple IIe level support with ProDOS 8 and limited ProDOS 16 booting
* Apple II Speaker support (simple square wave, flawed implementation.)
* 3.5" disk formatting
* HGR/DHGR Display
* 40/80 column Text Display
* 128K support
* Several 65816 emulation fixes

## Version 0.1

* 64K, 16K bank IO flag support
* 5.25" WOZ Disk Support
* ROM 03 partial boot
* Host application with 65816 debugging support
* Basic 65816 emulation and native modes
