# Clemens IIGS

Clemens IIGS is a cross-platform Apple IIGS emulator for both casual use and development of Apple IIGS software.

Below are a list of key IIGS features supported by this emulator, followed by a list of missing features.

First time users will be prompted to set a location where the emulator will store its data.  Also users will be prompted to locate a ROM (version 3 only as of this writing) that will be copied to the directory specified in the last step.

## Key Features

* ROM 3 supported
* All Apple IIGS modes including 3200 color (experimental)
* All Apple II graphics modes
* Ensoniq and Mockingboard (Slot 4) audio
* Joystick
* Save/Load snapshots
* Most disk image formats (.2mg, .woz, .po, .do, .dsk)
* Smartport Hard Drives (32MB only on Slot 2, Drive 1)

## Missing Features

* ROM 1 support
* Serial (SCC) communications
* PAL display
* Monochrome display

## Mouse Control

Currently to use the mouse in a IIGS application, the emulator locks the mouse to the view (i.e. mouselock.)

To transfer control of the mouse to the IIGS session, click within the view.  To exit the view, follow the instructions shown in the bottom of the view.  This instruction is also detailed in the Key Bindings section.

## Key Bindings (Linux)

The Tux/Super key when combined with number keys will treat them as function keys.  For example:

- Tux + 1 = F1
- Tux + 2 = F2
- Tux + Minus = F11
- Tux + Equals = F12
- Tux + Ctrl + Equals = CTRL-RESET
- Tux + Ctrl + Alt + Equals = CTRL-ALT-RESET (reboot)
- Tux + Ctrl + Left Alt + 1 to enter the Control Panel
- Tux + Ctrl + Left Alt + Minus to switch to Debugger Mode

To restore mouse control to the system, press both ALT keys and CTRL.

## Key Bindings (Windows)

- Ctrl + F12 for Control RESET
- Ctrl + Right Alt + F12 to reboot the system
- Ctrl + Left Alt + F1 to reboot and enter the Control Panel
- Ctrl + Left Alt + F11 to switch to Debugger Mode

To restore mouse control to the system, press both ALT keys and CTRL.

## Key Bindings (MacOS)

- Ctrl + F12 for CTRL-RESET
- Ctrl + Command + F12 to reboot the system
- Ctrl + Option + F1 to enter the Control Panel
- Ctrl + Option + F11 to switch to Debugger Mode

To restore mouse control to the system, press Ctrl + Option + Command.

