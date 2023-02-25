#ifndef CLEM_HOST_LI0N_HPP
#define CLEM_HOST_LI0N_HPP

#define CLEM_L10N_LABEL(_name_) ClemensL10N::_name_[ClemensL10N::kLanguageDefault]

namespace ClemensL10N {

extern const char *kWelcomeText[];
extern const char *kFirstTimeUse[];
extern const char *kGSKeyboardCommands[];
extern const char *kMouseLock[];
extern const char *kViewInput[];
extern const char *kMouseUnlock[];
extern const char *kSettingsTabMachine[];
extern const char *kSettingsMachineSystemSetup[];
extern const char *kSettingsMachineSystemSetupInfo[];
extern const char *kSettingsMachineROMFilename[];
extern const char *kSettingsMachineSystemMemory[];
extern const char *kSettingsMachineCards[];
extern const char *kSettingsTabEmulation[];

constexpr unsigned kLanguageDefault = 0;

} // namespace ClemensL10N

#endif
