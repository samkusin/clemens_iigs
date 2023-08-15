const char *kWelcomeText[] = {R"md(
# Clemens IIGS Emulator v%d.%d

This release contains the following changes:

- "User" mode GUI for regular use with larger view
- macOS Intel and Apple Silicon support
- Better emulation of system time based on the IIgs 28khz master clock frequency
- IWM fixes for 3.5 disk timings and overall emulation speed
- Dual Slot 7 smartport hard drive
- Better emulation of all graphics modes
- Ensoniq emulation improved
- Mouse tracking for GSOS/System Software
- Added fast disk and general fast mode emulation
- Passes all System Diagnostic tests

The following IIGS features are not yet supported:
                            
- Serial and Print Controllers
- ROM 1 support
- ROM 0 (//e-like) support
- Emulator Localization (PAL, Monochrome)
- Ethernet via Bridge Network
- ROM 3 mouse emulation

)md"};
