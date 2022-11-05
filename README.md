# Clemens IIGS

![Boot Sequence Demo](https://samirsinha.com/images/dotc-title.gif)

The Clemens IIgs emulator is an attempt at writing a modern emulator for
the Apple IIgs machine.  Its design focuses on developers versus casual play,
providing tools to debug IIgs applications at runtime.

My eventual goal is to provide a solid Apple IIgs backend and debugger with a
functional frontend.  At this writing I'd say the project is about 80% there
with the following features missing (in rough priority of what's planned next):

* Smartport Hard Drive
* Gamepad/Joystick
* ROM 01 Support
* Serial (SCC)
* Diagnostic Tests Pass
* 8mhz and other fast execution modes
* PAL and Monochrome modes
* AppleTalk

## Features

### Emulation

* Super Hires 640/320 modes with VBL and Scanline IRQs
* Ensoniq, Mockingboard C (without SSI voice) and A2 Speaker Audio
* WOZ 2.0, 2IMG, PO, DO and DSK support
* 5.25" and Apple 3.5" Drive Emulation
* Mouse and Keyboard
* Runs on ROM 3
* All Apple IIe graphics modes (hires is OK, could be better for some titles)

### Debugging

* Instruction level debugging
* Breakpoints on execution at, read from or write to an address
* Break on IRQ
* Execution trace output and export
* Memory bank dump to file

See [The Plan](https://github.com/samkusin/clemens_iigs/blob/main/docs/Plan.md)
for details on the next release's upcoming features.

The [Documentation](https://github.com/samkusin/clemens_iigs/blob/main/docs/) folder
contains some very rough notes on various systems and technical notes for the IIgs.

This information will be moved to [the Wiki](https://github.com/samkusin/clemens_iigs/wiki/The-Clemens-Apple-IIgs-Emulator) in stages.

## Current Screenshots

![Ultima V](https://samirsinha.com/images/u5a2_small.png)

![GS/OS 5](https://samirsinha.com/images/gsos5_small.png)

## Installation

Both Linux and Windows are supported in this release.  Currently only the Windows
build is available as a binary on the releases page.

### Windows

There is no installer and the application is contained in whole within a single exectuable.  Simply unzip the executable into a directory.  Because this
executable is not code signed, you'll get a warning from the OS when running
the binary.  Simply select 'More Info' and 'Run Anyway' if you're security
policy (and personal comfort level) allows for running untrusted applications.

An alternative to installing the binary is to build the project locally.

### Linux

Currently only builds from source are supported.  A future version will supply
packages for Debian and possibly other distributions.

## Getting Started

1. Copy a ROM 03 file to the same directory as the executable
2. Run the executable
3. Download disk images from https://archive.org/details/wozaday or https://mirrors.apple2.org.za/ftp.apple.asimov.net/
4. Import the images using one of the four available drives from the UI on the upper left handed pane.
   1. Slot 5 refers to 3.5" disks
   2. Slot 6 refers to 5.25" disks
5. Once imported, you can select the image by selecting the drop down.  The imported image will appear there.


### Emulator Input

To send input to the emulated machine, click the emulator view.  Mouse input
will be locked to the emulator view.  Keyboard events will also be directed to
the emulator.

To leave the emulator view, on Windows keyboards press the `Windows-Alt-Space` keys.

The F11 Key represents the RESET key.  To sent a Ctrl-RESET to the emulator,
press Ctrl-F11.

### Terminal Commands

The Terminal is currently the primary console interface available to send commands to the emulator.

Type 'help' to list the available commands.  The most important commands to
remember are:

```
reboot                  - reboots the machine
break                   - breaks execution at the current PC
step                    - step instruction at current PC
trace on                - enables program tracing
trace off, filename.txt - outputs the trace to the specified path
```

Please consult [the Wiki](https://github.com/samkusin/clemens_iigs/wiki/The-Clemens-Apple-IIgs-Emulator) for more detailed usage instructions.  The
following can be used to begin working with the emulator.

## Build

Both Linux 64-bit and Windows 10 or greater are supported in this release.  On all platforms, [CMake](https://cmake.org/) is required to build the project.

### Linux

#### Prerequisites

The following packages must be installed to successfully build the binary.

* GCC version 8 or later
* libx11-dev
* libxcursor-dev
* libxi-dev
* mesa-common-dev
* libasound-dev

#### Commands

Inside the project directory execute the following.

```
cmake -B build -DCMAKE_BUILD_TYPE=<Debug|RelWithDbgInfo|Release> -DBUILD_TESTING=OFF
cmake --build build --config=<Debug|RelWithDbgInfo|Release>
cp build-run/clemens_iigs/clemens_iigs <your_selected_install_directory>
```

### Windows

#### Commands

Inside the project directory execute the following.

```
cmake -B build -DBUILD_TESTING=OFF
cmake --build build --config=<Debug|RelWithDbgInfo|Release>
cp build-run/clemens_iigs/<Config>/clemens_iigs.exe <your_selected_install_directory>
```

## Author's Note

This project is not entirely novel as there are finished emulator projects like KEGS and MAME.

I've open-sourced this project for exposure.  Contributions are welcome if they
are made.

The best ways to run Apple IIgs software are:

* From a legitimate upgraded Apple IIgs with a [Floppy Emu](https://www.bigmessowires.com/floppy-emu/)
* A KEGS port available on MacOS or Linux (Windows ports have been spotty IMO GSPort, GSPlus, etc.)
* MAME in the browser https://archive.org/details/softwarelibrary_apple2gs

Honestly I think if someone can write a modern port of KEGS to Windows with a decent GUI frontend, that'll be the emulator you want to use.

So why do we need another emulator.  I've found the currently solutions fine for
running GS software.   This emulator is meant for IIgs *application* developers first.   I hope to replicate or improve upon the fine debugger AppleWin supplies
for Apple II software developers.

The Apple IIgs is an odd system.   It's a precursor to the line of Classic Macinitosh machines that ended with the iMac and later Power PC machines (ADB, IWM, Smartport, color GUI.)  It also marked the end of the Apple II line of computers on which I first learned to program in BASIC and assembly.

It's an 16-bit machine on the surface with a 24-bit addressing space and an 8-bit memory bus.  Seemingly it was underclocked on release for two reasons: Apple II compatibilty and (likely) to not compete with the 680x0 Macintosh line.  The sound capabilities are top-notch for its time.  As my computer lab/teacher back in 1987 said - it's a "Toy Mac".

I've attempted to document in source code original reference material for the various ICs and disk systems emulated in Clemens.  At some point I'll host some source reference materials and link to others.   Finally I've attempted to write this more for code readability vs performance.   There are certainly places where the CPU emulation could be sped up.   If I have time I'll take a look at those.


## Third Party Software

External libraries referenced below are used by the `host` project (the debugging front-end.)  The actual Clemens emulator backend does not have any dependencies beyond the C standard library and one listed dependency that's included in the project.

### Clemens Emulator Library

* [mpack](https://github.com/ludocode/mpack) : A single source file MessagePack implementation written in C in the `external` folder used for serialization of a machine

### Host

* [stb_truetype.h](https://github.com/nothings/stb/blob/master/stb_truetype.h) For font rendering
* [Fonts](https://www.kreativekorp.com/software/fonts/apple2.shtml) From Kreative Korp for 40/80 column text
* [Dear ImGui](https://github.com/ocornut/imgui) A well-known IMGUI C++ library
* [Sokol](https://github.com/floooh/sokol) A cross-platform minimal rendering backend
* [fmt](https://github.com/fmtlib/fmt) A minimal std::format implementation


## License

See LICENSE.txt (MIT)
