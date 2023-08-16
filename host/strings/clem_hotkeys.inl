#if defined(__linux__)
const char *kGSKeyboardCommands[] = {R"md(
## Mouse Control

The emulator attempts to seamlessly integrate mouse movement between the Apple IIGS and your desktop.

There may be titles where this mouse emulation does not work.  In those cases, the user can lock mouse input to the IIGS screen. See the hotkey help for details.

## Key Bindings

- Ctrl + F12 = CTRL-RESET
- Ctrl + Alt + Equals = CTRL-ALT-RESET (reboot)
- Ctrl + Left Alt + 1 to enter the Control Panel
- Ctrl + Left Alt + Minus to switch to Debugger Mode
- Ctrl + Alt + F10 to lock the mouse
- Ctrl + Alt + F8 to toggle fast mode
- Ctrl + Alt + F5 to pause and resume emulation

)md"};
const char *kMouseUnlock[] = {"Press Ctrl + Alt + 0 to unlock mouse"};

const char *kHybridModeShortcutText[] = {"Ctrl + LAlt + minus"};
const char *kLockMouseShortcutText[] = {"Ctrl + LAlt + 0"};
const char *kTogglePauseShortcutText[] = {"Ctrl + LAlt + 5"};
const char *kFastModeShortCutText[] = {"Ctrl + LAlt + 8"};

#elif defined(__APPLE__)
const char *kGSKeyboardCommands[] = {R"md(
## Mouse Control

The emulator attempts to seamlessly integrate mouse movement between the Apple IIGS and your desktop.

There may be titles where this mouse emulation does not work.  In those cases, the user can lock mouse input to the IIGS screen. See the hotkey help for details.

## Key Bindings (MacOS)

- Ctrl + F12 for CTRL-RESET
- Ctrl + Command + F12 to reboot the system
- Ctrl + Option + F1 to enter the Control Panel
- Ctrl + Option + F11 to switch to Debugger Mode
- Ctrl + Option + F10 to lock mouse
- Ctrl + Option + F8 to toggle fast mode
- Ctrl + Option + F5 to pause and resume emulation

)md"};
const char *kMouseUnlock[] = {"Press CTRL + Option + F10 to unlock mouse"};

//  UNUSED since these are handled outside of ImGUI
const char *kHybridModeShortcutText[] = {"Ctrl + Option + F11"};
const char *kLockMouseShortcutText[] = {"Ctrl + Option+ F10"};
const char *kTogglePauseShortcutText[] = {"Ctrl + Option + F5"};
const char *kFastModeShortCutText[] = {"Ctrl + Option + F8"};
#else
const char *kGSKeyboardCommands[] = {R"md(
## Mouse Control

The emulator attempts to seamlessly integrate mouse movement between the Apple IIGS and your desktop.

There may be titles where this mouse emulation does not work.  In those cases, the user can lock mouse input to the IIGS screen. See the hotkey help for details.

- Ctrl-Right ALT-F12 to reboot the system
- Ctrl-F12 for CTRL-RESET
- Ctrl-Left ALT-F1 to enter the Control Panel
- Ctrl-Left ALT-F11 to switch to Debugger Mode
- Ctrl-Left ALT-F10 to lock mouse
- Ctrl-Left ALT-F8 to toggle fast mode
- Ctrl-Left ALT-F5 to pause and resume emulation
)md"};
const char *kMouseUnlock[] = {"Press  CTRL + ALT + F10 to unlock mouse"};

const char *kHybridModeShortcutText[] = {"Ctrl + Left Alt + F11"};
const char *kLockMouseShortcutText[] = {"Ctrl + Left Alt + F10"};
const char *kTogglePauseShortcutText[] = {"Ctrl + Left Alt + F5"};
const char *kFastModeShortCutText[] = {"Ctrl + Left Alt + F8"};
#endif
