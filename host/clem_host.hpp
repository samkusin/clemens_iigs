#ifndef CLEM_HOST_H
#define CLEM_HOST_H

struct ClemensHostInterop {
    bool exitApp;
    bool mouseShow;

    //  system/menu notifications to the main app
    bool nativeMenu;
    enum {
        None,
        PasteFromClipboard,
        LoadSnapshot,
        SaveSnapshot,
        Power,
        Shutdown,
        Reboot,
        Help,
        DiskHelp,
        Debugger,
        Standard,
        AspectView,
        JoystickConfig,
        MouseLock,
        MouseUnlock,
        PauseExecution,
        ContinueExecution,
        About
    } action;

    //  events from the main app
    bool poweredOn;
    bool debuggerOn;
    bool allowConfigureJoystick;
    bool mouseLock;
    bool running;
    unsigned viewWidth;         //   0 = no change
    unsigned viewHeight;        //   0 = no change
    unsigned minWindowWidth;
    unsigned minWindowHeight;
};

void clemens_host_init(ClemensHostInterop *interop);

void clemens_host_update();

void clemens_host_terminate();

#endif
