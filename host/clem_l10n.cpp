#include "clem_l10n.hpp"

namespace ClemensL10N {

#include "strings/clem_hotkeys.inl"
#include "strings/clem_welcome.inl"

const char *kMouseLock[] = {"Click in View to lock mouse input"};
const char *kViewInput[] = {"Move mouse into view for key input"};

const char *kSettingsTabMachine[] = {"Machine"};
const char *kSettingsMachineSystemMemory[] = {"Memory"};

const char *kSettingsMachineSystemSetup[] = {"System Setup"};
const char *kSettingsMachineSystemSetupInfo[] = {
    "These options will take effect after power cycling the emulated machine."};
const char *kSettingsMachineROMFilename[] = {"ROM Filename"};

const char *kSettingsMachineCards[] = {"Cards"};

const char *kSettingsTabEmulation[] = {"Emulation"};

#include "strings/clem_help.inl"

} // namespace ClemensL10N
