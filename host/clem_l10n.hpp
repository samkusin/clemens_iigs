#ifndef CLEM_HOST_LI0N_HPP
#define CLEM_HOST_LI0N_HPP

#define CLEM_L10N_LABEL(_name_) ClemensL10N::_name_[ClemensL10N::kLanguageDefault]

#define CLEM_L10N_OK_LABEL     ClemensL10N::kLabelOk[ClemensL10N::kLanguageDefault]
#define CLEM_L10N_CANCEL_LABEL ClemensL10N::kLabelCancel[ClemensL10N::kLanguageDefault]

namespace ClemensL10N {

extern const char *kExitMessage[];

extern const char *kWelcomeText[];
extern const char *kGSKeyboardCommands[];
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
extern const char *kSettingsROMFileWarning[];
extern const char *kSettingsROMFileError[];

extern const char *kEmulatorHelp[];
extern const char *kDiskSelectionHelp[];
extern const char *kDebuggerHelp[];

extern const char *kModalDeleteSnapshot[];

extern const char *kLabelDelete[];
extern const char *kLabelOk[];
extern const char *kLabelCancel[];
extern const char *kLabelDeleteConfirm[];
extern const char *kLabelDeleteFailed[];

extern const char *kDebugNotAvailableWhileRunning[];
extern const char *kDebugDiskNoTrackData[];

extern const char* kTitleJoystickConfiguration[];
extern const char* kLabelJoystickConfirm[];
extern const char* kLabelJoystickButtonBinding[];
extern const char* kLabelJoystickNone[];
extern const char* kLabelJoystickId[];
extern const char* kLabelJoystickHelp[];
extern const char* kLabelJoystick2Help[];
extern const char* kButtonJoystickButton1[];
extern const char* kButtonJoystickButton2[];

//  shortcuts
extern const char *kHybridModeShortcutText[];
extern const char *kLockMouseShortcutText[];
extern const char *kTogglePauseShortcutText[];
extern const char *kFastModeShortCutText[];

constexpr unsigned kLanguageDefault = 0;

} // namespace ClemensL10N

#endif
