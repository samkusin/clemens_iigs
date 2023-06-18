#ifndef CLEM_HOST_H
#define CLEM_HOST_H

struct ClemensHostInterop {
    bool mouseLock;
    bool mouseShow;
    bool pasteFromClipboard;
    bool exitApp;
};

void clemens_host_init(ClemensHostInterop* interop);

void clemens_host_terminate();


#endif
