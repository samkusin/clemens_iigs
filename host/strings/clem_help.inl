
const char *kEmulatorHelp[] = {R"md(
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
* Most disk image formats (.2mg, .woz, .po, .do, .dsk, .hdv)

## Missing Features

* ROM 1 support
* Host desktop mouse integration
* Serial (SCC) communications
* Monochrome display
* Uthernet
)md"};

const char *kDiskSelectionHelp[] = {R"md(
## Disk Selection

The disk tray resides on the top left most section of the interface.

Each widget represents a physical disk slot.

* **s5d1** and **s5d2** disks were 800K 3.5" disk images used to distribute most Apple IIgs titles
* **s6d1** and **s6d2** disks were 140K 5.25" disk images used to legacy Apple II titles
* **s7d1** and **s7d2** are disks that can support up to 32MB hard drive images

All disk widgets have eject buttons.  Currently s7d1 and s7d2 do not support write protection.

)md"};

const char *kDebuggerHelp[] = {R"md(
## Debugger Help

To enter the debugger from user mode (the default, minimal interface) see the Hotkeys tab for  the key combination.

From user mode you can also enter debugger mode by pressing the bug icon on the lower left of the screen.

Once in debugger mode, type 'help' in the terminal to display a full list of available commands.
)md"};
