
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
* Most disk image formats (.2mg, .woz, .po, .do, .dsk)

## Missing Features

* ROM 1 support
* Host desktop mouse integration
* Serial (SCC) communications
* Monochrome display
* Uthernet
)md"};

const char *kDiskSelectionHelp[] = {R"md(
## Disk Selection

Disk images are loaded by 'importing' them into the Emulator's library folder.

* Choose a drive from the four drive widgets in the top left of the Emulator view
* Select 'import' to import one or more disk images as a set (.DSK, .2MG, .PO, .DO, .WOZ)
* Enter the desired disk set name
* A folder will be created in the library folder for the disk set containing the imported images
)md"};

const char *kDebuggerHelp[] = {R"md(
## Debugger Help

To enter the debugger from user mode (the default, minimal interface) see the Hotkeys tab for  the key combination.

From user mode you can also enter debugger mode by pressing the bug icon on the lower left of the screen.

Once in debugger mode, type 'help' in the terminal to display a full list of available commands.
)md"};
