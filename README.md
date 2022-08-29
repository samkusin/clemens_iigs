# Clemens IIGS

![Boot Sequence Demo](https://samirsinha.com/images/sample-iigs-boot.gif)

![Myraglen](https://samirsinha.com/images/myraglen.png)

![ProDOS 16](https://samirsinha.com/images/prodos16.png)


The Clemens Apple IIgs emulator project is a personal attempt to write a modern functional emulator for the 65816 CPU based computer system from the late 1980s by Apple Computer, Inc.

## Summary

The current state of the project is a IMGUI (Dear ImGui) debugger front end running the emulator with a very `gdb` like interface to inspect memory, load disks and reset the system.  Documentation to come.

My goal is to provide a solid Apple IIgs backend + debugger for ambitious developers who want to write a modern frontend.

The emulator backend is written in C.  The `host` front-end is written in C++ with a few header only libraries for boilerplate rendering, I/O, GUI.

### Disk types supported as of Version 0.2

* 5.25" and 3.5" WOZ images
* DSK, 2MG, DO, PO images

### Working Features

* Disk II copy protected image boot
* Apple II speaker
* Prodos 16 system boot (until the GUI)
* Hi-res and Double Hi-res graphics
* 80-column
* IRQ/BRK/RESET interrupts
* ROM 3 support
* Super Hi-Res 320/640 modes
* Mockingboard C without SSI-223
* Ensoniq audio playback

See CHANGELOG.md for details on the next release's upcoming features.

### Missing Features

These are essential IIgs features that will be worked on.  This cannot be considered a working emulator until the following are functional:

* Monochrome Display
* Serial Communication
* Extended character set text page
* Joystick emulation
* Accurate Hi-res display (RGB color monitor)
* PAL/50hz emulation
* ROM 01 support

These are features that make Clemens an option for casual users.

* Smartport (hard drive emulation)
* Fast disk support - when disk is on, emulator runs at fastest possible speed
* 8mhz mode Transwarp-like mode
* A user-friendly front-end
* There are others

The CHANGELOG.md contains what has been done and what will be done in the current version.

The `docs` folder contains some very rough notes on various systems and technical notes for the IIgs.

## Building

> **NOTE**
> This needs to be fleshed out/cleaned up

Windows only for now.   Most of the third-party libraries I use for cross-platform support will likely make porting to Linux and MacOS relatively simple (audio perhaps requiring the most work.)

This project uses CMake.

```
# release will work too - this creates a folder called ./build-run in the project root
cd build
cmake ..
cmake --build . --config debug --target app_assets_clemens_iigs

# navigate to this directory and run the executable based on the config built
cd ../build-run/clemens_iigs
./Debug/clemens_iigs.exe
```

## Usage

The ROM 3 needed to boot the system needs to be supplied by the user.  There are ways to find it on common non-commercial disk image sites.

Once found, copy it to the ./build-run/clemens_iigs folder and rename it `gs_rom_3.rom`.

Then in the front-end Terminal window, (click on the lower left 'Terminal' window) type:

```
.power
run
```

Documentation to come.

## Author's Note

This project is not entirely novel as there are finished emulator projects like KEGS and MAME.  I doubt Clemens will ever achieve parity with those projects.  Given the relative popularity of other equivalent classic home computer systems (C64, Amiga) and the IIgs software library, it's unclear whether this emulator will ever see genuine usage.

I've open-sourced this project for exposure.  I doubt external contributions will be useful until I complete a few more core systems (Ensoniq audio, super-hires and smartport being the big ones.)

The best ways to run Apple IIgs software are:

* From a legitimate upgraded Apple IIgs with a [Floppy Emu](https://www.bigmessowires.com/floppy-emu/)
* A KEGS port available on MacOS or Linux (Windows ports have been spotty IMO GSPort, GSPlus, etc.)
* MAME in the browser https://archive.org/details/softwarelibrary_apple2gs

Honestly I think if something can write a modern port of KEGS to Windows with a decent GUI frontend, that'll be the emulator you want to use.

So why do we need another emulator.   We don't.  This is a personal project that I thought wouldn't be too hard to write (oh, really?).  Also I started this project to address a midlife programming crisis of sorts.

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
