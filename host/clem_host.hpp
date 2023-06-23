#ifndef CLEM_HOST_H
#define CLEM_HOST_H

struct ClemensHostInterop {
    bool exitApp;
    bool mouseShow;

    //  system/menu notifications to the main app
    bool mouseLock;
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
        About
    } action;
    
    //  events from the main app
    bool poweredOn;
};

void clemens_host_init(ClemensHostInterop *interop);

void clemens_host_update();

void clemens_host_terminate();

#endif
