#ifndef CLEM_HOST_LI0N_HPP
#define CLEM_HOST_LI0N_HPP

#define CLEM_L10N_LABEL(_name_) ClemensL10N::_name_[ClemensL10N::kLanguageDefault]

namespace ClemensL10N {

extern const char *kExitMessage[];

extern const char *kWelcomeText[];
extern const char *kGSKeyboardCommands[];
extern const char *kMouseLock[];
extern const char *kViewInput[];
extern const char *kMouseUnlock[];
extern const char *kSettingsTabMachine[];
extern const char *kSettingsNotAvailable[];
extern const char *kSettingsMachineSystemSetup[];
extern const char *kSettingsMachineSystemSetupInfo[];
extern const char *kSettingsMachineROMFilename[];
extern const char *kSettingsMachineSystemMemory[];
extern const char *kSettingsMachineCards[];
extern const char *kSettingsTabEmulation[];
extern const char *kSettingsEmulationFastDisk[];
extern const char *kSettingsEmulationFaskDiskHelp[];

extern const char *kEmulatorHelp[];
extern const char *kDiskSelectionHelp[];
extern const char *kDebuggerHelp[];

constexpr unsigned kLanguageDefault = 0;

} // namespace ClemensL10N

#endif
