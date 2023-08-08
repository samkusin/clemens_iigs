# Clemens IIGS

Clemens is an Apple IIgs emulator for macOS (Catalina or later), Linux and Windows 10.  
It runs most IIgs software under a ROM 3 compliant backend.  It also aims to provide a solid
frontend for both users and developers.

**NOTE** This software is in late Alpha.  A variety of bugs have been fixed in
the latest version. While this software will run most IIgs software, features
like serial communication, printers and internationalization are currently planned
but unfinished.

There is a work-in-progress [manual](https://github.com/samkusin/clemens_iigs/blob/main/MANUAL.md)

Please refer to the list of [Missing Features](https://github.com/samkusin/clemens_iigs/blob/main/MANUAL.md#known-issues) for details.

![Rastan](https://clemens-doc-assets.s3.amazonaws.com/rastan_m3.gif)
![Bards Tale](https://clemens-doc-assets.s3.amazonaws.com/btgs_mid.gif)

## Emulation

* Standard Apple IIgs and IIx video modes
* ROM 3 up to 8MB
* Ensoniq, Mockingboard C (without SSI voice) and A2 speaker audio
* WOZ 2.0, 2IMG, PO, DO and DSK support
* 5.25", Apple 3.5" and hard drive emulation
* Joystick/Gamepad support
* Passes Self-Diagnostic Tests
* Load and save machine snapshots

## Debugging

![Debugger](https://clemens-doc-assets.s3.amazonaws.com/debugger.png)

* Instruction level debugging
* Breakpoints on execution at, read from or write to an address
* Break on IRQ
* Execution trace output and export
* Memory bank dump to file

## Installation

See the [Releases](https://github.com/samkusin/clemens_iigs/releases) page for the latest release.

### macOS
Download and mount the DMG to copy Clemens IIGS into your Applications folder.

### Linux
There is a 64-bit .deb for Debian/Ubuntu systems.
```
# Download the .deb file, copy it to a directory and run (for example, version 0.6.0):
sudo dpkg -i Clemens_IIGS-0.6.0-linux-x64.deb
```

### Windows
A single Windows 64-bit executable is available.  You will receive a SmartScreen warning
since this executable is not code-signed with a certificate (this is free software, developed
by a single developer, and not a corporation, during his spare time.)

Simply select 'More Info' and 'Run Anyway' if your security policy (and personal comfort level) allows for running untrusted applications.

## Buliding from Source
This is always an option instead of installation if you have the prerequisites on your machine.

All platforms require [CMake 3.15 or later](https://cmake.org/download/).

### macOS

Requires Xcode 12 or later.

```
# For debug builds
cmake -B build -S . -DCMAKE_BUILD_TYPE=Debug
cmake --build build --config=Debug

# For Release builds
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --config=Release
```

### Linux

The following packages must be installed to successfully build the binary.

* GCC version 8 or later
* libx11-dev
* libxcursor-dev
* libxi-dev
* mesa-common-dev
* libasound-dev
* uuid-dev

Inside the project directory execute the following.

```
cmake -B build -DCMAKE_BUILD_TYPE=<Debug|RelWithDbgInfo|Release> -DBUILD_TESTING=OFF
cmake --build build --config <Debug|RelWithDbgInfo|Release>
cp build/host/clemens_iigs <your_selected_install_directory>
```

### Windows

Requires Visual Studio 2019 or later.  Optionally you may run these commands
in a Developer Prompt if CMake does not detect your compiler from a regular
Command Prompt.

Inside the project directory execute the following.

```
cmake -B build -DBUILD_TESTING=OFF
cmake --build build --config <Debug|RelWithDbgInfo|Release>
cp build-run/clemens_iigs/<Config>/clemens_iigs.exe <your_selected_install_directory>
```

## Contributions

They are welcome.  Refer to the repository issue list for details.

## Author's Note

This project is not entirely novel as there are finished emulator projects like KEGS and MAME.

I've open-sourced this project for exposure.  Contributions are welcome if they are made.

The best ways to run Apple IIgs software are:

* From a legitimate upgraded Apple IIgs with a [Floppy Emu](https://www.bigmessowires.com/floppy-emu/)
* A KEGS port available on MacOS or Linux (Windows ports are available though some haven't been updated in a while - GSPort, GSPlus, etc.)
* MAME in the browser https://archive.org/details/softwarelibrary_apple2gs
* Clemens - it's catching up!

![GSOS6](https://clemens-doc-assets.s3.amazonaws.com/gsos6.png)

## Third Party Software

External libraries referenced below are used by the `host` project (the debugging front-end.)  The actual Clemens emulator backend does not have any dependencies beyond the C standard library and the two listed dependencies included in the project.

All projects below have permissive enough licenses (MIT, BSD, Apache) to be distributed by this project.

### Clemens Emulator Library

* [mpack](https://github.com/ludocode/mpack) : A single source file MessagePack implementation written in C in the `external` folder used for serialization of a machine
* [unity](https://github.com/ThrowTheSwitch/Unity) : A C testing framework

### Host

* [stb_truetype.h](https://github.com/nothings/stb/blob/master/stb_truetype.h) For font rendering
* [Fonts](https://www.kreativekorp.com/software/fonts/apple2.shtml) From Kreative Korp for 40/80 column text
* [Dear ImGui](https://github.com/ocornut/imgui) A well-known IMGUI C++ library
* [Dear ImGui Markdown](https://github.com/juliettef/imgui_markdown) ImGui Markdown Implementation
* [Sokol](https://github.com/floooh/sokol) A cross-platform minimal rendering backend
* [fmt](https://github.com/fmtlib/fmt) A minimal std::format implementation
* [doctest](https://github.com/doctest/doctest) A minimal C++ single-header testing framework
* [miniz](https://github.com/richgel999/miniz) A Zlib compliant single file library with PNG write support
* [inih](https://github.com/benhoyt/inih) A minimal C based INI file parser
* [json](https://github.com/nlohmann/json) Used for test serialization

## License

See LICENSE.txt (MIT)
