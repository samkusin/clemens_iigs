const char *kSettingsNotAvailable[] = {"This setting is not available in this version."};
const char *kSettingsTabMachine[] = {"Machine"};

const char *kSettingsMachineSystemMemory[] = {"Memory"};
const char *kSettingsMachineSystemSetup[] = {"System Setup"};
const char *kSettingsMachineSystemSetupInfo[] = {
    "These options will take effect after power cycling the emulated machine."};
const char *kSettingsMachineROMFilename[] = {"ROM Filename"};
const char *kSettingsMachineCards[] = {"Cards"};

const char *kSettingsTabEmulation[] = {"Emulation"};
const char *kSettingsEmulationFastDisk[] = {"Fast Disk Mode"};
const char *kSettingsEmulationFaskDiskHelp[] = {R"txt(
Enabling this mode will speed up disk access in most cases.

Fast disk emulation will effect audio playback (i.e. Mockingboard) while the disk drive is active.  It may cause undefined behavior for certain titles.  Disabling this feature will result in authentic real-world timing (and sluggish disk access ca. 1987).)txt"};

const char *kSettingsROMFileWarning[] = {R"txt(
A ROM 3 file is necessary to emulate an Apple IIGS.  Without such a file, the emulator will hang on startup.
)txt"};

const char *kSettingsROMFileError[] = {"The file cannot be found."};
