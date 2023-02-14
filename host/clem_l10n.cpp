namespace ClemensL10N {
const char *kWelcomeText[] = {"Clemens IIGS Emulator v%d.%d\n"
                              "\n\n"
                              "This release contains the following changes:\n\n"
                              "- SmartPort hard drive emulation\n"
                              "- Joystick/Paddles support\n"
                              "- Ctrl-Apple-Escape trigger for the Desktop Manager\n"
                              "- Minor fixes on the IWM\n"
                              "- Separation of 65816 and I/O emulator libraries\n"
                              "- UI improvements\n"
                              "\n\n"
                              "The following IIGS features are not yet supported:\n"
                              "\n"
                              "- Serial Controller emulation\n"
                              "- ROM 1 support\n"
                              "- ROM 0 (//e-like) support\n"
                              "- Emulator Localization (PAL, Monochrome)"};

const char *kFirstTimeUse[] = {
    "Usage instructions for the Emulator:\n"
    "\n"
    "At the terminal prompt (lower-right side of the view), enter 'help' to display instructions "
    "for supported commands.\n"
    "\n"
    "Useful Terminal Commands:\n"
    "- reboot                       : reboots the system\n"
    "- help                         : displays all supported commands\n"
    "- break                        : breaks into the debugger\n"
    "- save <snapshot_name>         : saves a snapshot of the current system\n"
    "- load <snapshot_name>         : loads a saved snapshot\n"
    "\n"
    "The emulator requires a ROM binary (ROM 03 as of this build.)  This file must be stored in "
    "the same directory as the application.\n"
    "\n"
    "Disk images are loaded by 'importing' them into the Emulator's library folder.\n"
    "\n"
    "- Choose a drive from the four drive widgets in the top left of the Emulator view\n"
    "- Select 'import' to import one or more disk images as a set (.DSK, .2MG, .PO, .DO, .WOZ)\n"
    "- Enter the desired disk set name\n"
    "- A folder will be created in the library folder for the disk set containing the imported "
    "images\n"
    "\n"};

const char *kMouseLock[] = {"Click in View to lock mouse input"};
const char *kViewInput[] = {"Move mouse into view for key input"};

#if defined(__linux__)
const char *kGSKeyboardCommands[] = {
    "IIGS Keyboard Commands:\n"
    "\n"
    "The Tux/Super key when used in combination with number keys will convert them "
    " to Function Keys.\n"
    "\n"
    "- Tux - 1 = F1\n"
    "- Tux - Minus = F11\n"
    "- Tux - Equal = F12\n"
    "\n"
    "- Tux - Ctrl - Alt - Equal to reboot the system\n"
    "- Tux - Ctrl - Equal for CTRL-RESET\n"
    "- Tux - Ctrl - Left Alt - 1 to enter the Control Panel\n"};

const char *kMouseUnlock[] = {"Press both ALT keys and CTRL to unlock mouse"};
#elif defined(__APPLE__)
const char *kGSKeyboardCommands[] = {"IIGS Keyboard Commands:\n"
                                     "\n"
                                     "- Ctrl-Command-F12 to reboot the system\n"
                                     "- Ctrl-F12 for CTRL-RESET\n"
                                     "- Ctrl-Option-F1 to enter the Control Panel\n"};
const char *kMouseUnlock[] = {"Press Option + Command + and CTRL to unlock mouse:"};
#else
const char *kGSKeyboardCommands[] = {"IIGS Keyboard Commands:\n"
                                     "\n"
                                     "- Ctrl-Right ALT-F12 to reboot the system\n"
                                     "- Ctrl-F12 for CTRL-RESET\n"
                                     "- Ctrl-Left ALT-F1 to enter the Control Panel\n"};

const char *kMouseUnlock[] = {"Press both ALT keys and CTRL to unlock mouse"};
#endif
} // namespace ClemensL10N
