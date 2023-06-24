# Clemens IIGS Emulator

Clemens IIGS is an emulator for the Apple IIGS 16-bit system from the late 1980s.  
Its components include its own implementation of the various emulated hardware 
with a rich user frontend.

## Table of Contents

1. [Features](#section-support)
2. [Roadmap](#roadmap)
3. [Getting Started](#getting-started)
4. [Hotkeys](#hotkeys)
5. [Debugger](#debugger)
6. [Troubleshooting](#troubleshooting)

## Features <a name="section-support"></a>

- Windows 10, macOS Catalina, or Linux 64-bit

**Stock Graphics and Audio modes:**

- All Apple II modes (NTSC with no artifacts as was the case with the IIGS.)
- 320 and 640 super-hires modes
- 3200 color mode (experimental)
- Apple II speaker
- Mockingboard C without SSI-263 (voice)
- Ensoniq 5503 DOC wavetable synth with 64K sound RAM

**Hardware**

- 2.8 MHZ system speed
- 8 MB RAM
- ROM 3
- 5.25", 3.5" and Slot 5 Smartport hard drive emulation
- WOZ 2.0, DSK, DO, PO, 2IMG disk image formats
- Serial Ports with terminal emulation

**User:**

- US keyboard layout
- Machine snapshot load and save
- Transfer of binary files to and from the system
- Paste text as keyboard input (at the BASIC/Monitor prompts)
- Debugging facilities (partial)

## Roadmap <a name="roadmap"></a>

- ROM 1
- French and other international keyboard layouts
- Slot 7 Hard Drive
- Serial Printers

## Getting Started <a name="getting-started"></a>

Please see the [README](https://github.com/samkusin/clemens_iigs#readme) for the 
basics, installation and identifying a ROM image for starting the emulator.  

This section will briefly cover what's required to get a system up and running
once installed and a proper ROM image has been set up on the settings screen.

### First Steps

After installation and initial launch of Clemens, a dialog will come up prompting 
the user to select where he wants the place the `data` directory.  In most cases
the user can just select the default and continue.

![Setup Data](docs/manual-setup-data.png)

This data directory will contain items such as snapshots, traces and log files.
The default location for this directory depends on the host's operating system.
The host in this case is the OS the user runs Clemens on (i.e. Windows, macOS
or Linux.)  

> **NOTE** 
>
> Typically this folder is hidden to users without explicitly making this folder visible by means
> specific to the OS.  The user can always select another visible location if desired.
>
> See [Portable Installation] to control exactly where the data directory and configuration files are located.

### Configuration

Upon setting up a data directory the user is presented with the setup screen.  This screen is accessible before powering on the emulated machine.

![Setup Machine](docs/manual-setup.png)

All configuration options are saved in a `config.ini` file located in a folder reserved for 
Clemens on the system (typically the same as the default data directory mentioned above.)

See [Portable Installation](#portable-installation) (as mentioned above) to ensure configurations are 
saved in a folder you choose.


## Hotkeys <a name="hotkeys"></a>

## Debugger <a name="debugger"></a>

## Troubleshooting <a name="troubleshooting"></a>
