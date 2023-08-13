#if defined(__linux__)
const char *kGSKeyboardCommands[] = {R"md(
## Mouse Control

Currently to use the mouse in a IIGS application, the emulator locks the mouse to the view (i.e. mouselock.)

To transfer control of the mouse to the IIGS session, click within the view.  To exit the view, follow the instructions shown in the bottom of the view.  This instruction is also detailed in the Key Bindings section.

## Key Bindings

The Tux/Super key when combined with number keys will treat them as function keys.  For example:

- Tux + 1 = F1
- Tux + 2 = F2
- Tux + Minus = F11
- Tux + Equals = F12
- Tux + Ctrl + Equals = CTRL-RESET
- Tux + Ctrl + Alt + Equals = CTRL-ALT-RESET (reboot)
- Tux + Ctrl + Left Alt + 1 to enter the Control Panel
- Tux + Ctrl + Left Alt + Minus to switch to Debugger Mode
- Tux + Ctrl + Alt + F10 to lock the mouse

)md"};
const char *kMouseUnlock[] = {"Press Tux, Ctrl, Alt and F10 to unlock mouse"};

const char *kHybridModeShortCutText[]= {"Tux+Ctrl+LAlt+minus"};
const char *kLockMouseShortCutText[]={"Tux+Ctrl+LAlt+F10"};
const char *kTogglePauseText[]={"Tux+Ctrl+LAlt+5"};

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

)md"};
const char *kMouseUnlock[] = {"Press CTRL + Option + F10 to unlock mouse"};

//  UNUSED since these are handled outside of ImGUI
const char *kHybridModeShortCutText[]= {"Ctrl + Option + F11"};
const char *kLockMouseShortCutText[]={"Ctrl + Option+ F10"};
const char *kTogglePauseText[]={"Ctrl + Option + F5"};
#else
const char *kGSKeyboardCommands[] = {R"md(
## Mouse Control

Currently to use the mouse in a IIGS application, the emulator locks the mouse to the view (i.e. mouselock.)

To transfer control of the mouse to the IIGS session, click within the view.  To exit the view, follow the instructions shown in the bottom of the view.  This instruction is also detailed in the Key Bindings section.

- Ctrl-Right ALT-F12 to reboot the system
- Ctrl-F12 for CTRL-RESET
- Ctrl-Left ALT-F1 to enter the Control Panel
- Ctrl-Left ALT-F11 to switch to Debugger Mode
- Ctrl-Left ALT-F10 to lock mouse)md"};
const char *kMouseUnlock[] = {"Press  CTRL + ALT + F10 to unlock mouse"};

const char *kHybridModeShortCutText[]= {"Ctrl + Left Alt + F11"};
const char *kLockMouseShortCutText[]={"Ctrl + Left Alt + F10"};
const char *kTogglePauseText[]={"Ctrl + Left Alt + F5"};
#endif
