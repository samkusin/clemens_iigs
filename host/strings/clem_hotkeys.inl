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

To restore mouse control to the system, press both ALT keys and CTRL.
)md"};
const char *kMouseUnlock[] = {"Press both ALT keys and CTRL to unlock mouse"};
#elif defined(__APPLE__)
const char *kGSKeyboardCommands[] = {R"md(
## Mouse Control

Currently to use the mouse in a IIGS application, the emulator locks the mouse to the view (i.e. mouselock.)

To transfer control of the mouse to the IIGS session, click within the view.  To exit the view, follow the instructions shown in the bottom of the view.  This instruction is also detailed in the Key Bindings section.

## Key Bindings (MacOS)

- Ctrl + F12 for CTRL-RESET
- Ctrl + Command + F12 to reboot the system
- Ctrl + Option + F1 to enter the Control Panel
- Ctrl + Option + F11 to switch to Debugger Mode

To restore mouse control to the system, press Ctrl + Option + Command.
)md"};
const char *kMouseUnlock[] = {"Press CTRL + Option + Command to unlock mouse:"};
#else
const char *kGSKeyboardCommands[] = {R"md(
## Mouse Control

Currently to use the mouse in a IIGS application, the emulator locks the mouse to the view (i.e. mouselock.)

To transfer control of the mouse to the IIGS session, click within the view.  To exit the view, follow the instructions shown in the bottom of the view.  This instruction is also detailed in the Key Bindings section.

- Ctrl-Right ALT-F12 to reboot the system
- Ctrl-F12 for CTRL-RESET
- Ctrl-Left ALT-F1 to enter the Control Panel
- Ctrl-Left ALT-F11 to switch to Debugger Mode)md"};
const char *kMouseUnlock[] = {"Press both ALT keys and CTRL to unlock mouse"};
#endif
