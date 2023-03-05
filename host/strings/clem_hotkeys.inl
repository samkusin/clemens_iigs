#if defined(__linux__)
const char *kGSKeyboardCommands[] = {R"(
IIGS Keyboard Commands:

The Tux/Super key when used in combination with number keys will convert them to Function Keys."

- Tux - 1 = F1
- Tux - Minus = F11
- Tux - Equal = F12
    
- Tux - Ctrl - Alt - Equal to reboot the system
- Tux - Ctrl - Equal for CTRL-RESET
- Tux - Ctrl - Left Alt - 1 to enter the Control Panel
- Tux - Ctrl - Left Alt - Minus to switch to Debugger Mode)"};

const char *kMouseUnlock[] = {"Press both ALT keys and CTRL to unlock mouse"};
#elif defined(__APPLE__)
const char *kGSKeyboardCommands[] = {R"(
IIGS Keyboard Commands:

- Ctrl-Command-F12 to reboot the system
- Ctrl-F12 for CTRL-RESET
- Ctrl-Option-F1 to enter the Control Panel
- Ctrl-Option-F11 to switch to Debugger Mode)"};

const char *kMouseUnlock[] = {"Press Option + CTRL + Command to unlock mouse:"};
#else
const char *kGSKeyboardCommands[] = {R"(
IIGS Keyboard Commands:\n"

- Ctrl-Right ALT-F12 to reboot the system
- Ctrl-F12 for CTRL-RESET
- Ctrl-Left ALT-F1 to enter the Control Panel
- Ctrl-Left ALT-F11 to switch to Debugger Mode)"};

const char *kMouseUnlock[] = {"Press both ALT keys and CTRL to unlock mouse"};
#endif
