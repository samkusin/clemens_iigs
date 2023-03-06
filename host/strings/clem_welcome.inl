const char *kWelcomeText[] = {R"txt(
Clemens IIGS Emulator v%d.%d

This release contains the following changes:

- User-minimal interface mode (by default)
- macOS Intel and Apple Silicon support
- Better emulation of Apple II graphics modes
- Snapshot save/load fixes in some cases
- Improved Keyboard shortcut support


The following IIGS features are not yet supported:
                            
- Serial Controller emulation
- ROM 1 support
- ROM 0 (//e-like) support
- Emulator Localization (PAL, Monochrome)
)txt"};

const char *kFirstTimeUse[] = {R"txt(
General instructions for the using the Emualtor:\n

The emulator requires a ROM binary (ROM 03 as of this build.

Disk images are loaded by 'importing' them into the Emulator's library folder.

- Choose a drive from the four drive widgets in the top left of the Emulator view
- Select 'import' to import one or more disk images as a set (.DSK, .2MG, .PO, .DO, .WOZ)
- Enter the desired disk set name
- A folder will be created in the library folder for the disk set containing the imported images
)txt"};
