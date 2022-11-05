# Changelog

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
