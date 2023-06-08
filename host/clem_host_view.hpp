#ifndef CLEM_HOST_APP_VIEW_HPP
#define CLEM_HOST_APP_VIEW_HPP

#include "clem_mmio_types.h"

#include <cstdint>

class ClemensHostView {
  public:
    enum class ViewType { Startup, Main };
    struct FrameAppInterop {
        bool mouseShow; // show or hide the mouse
        bool mouseLock; // enable mouse lock
        bool exitApp;   // terminate the app
    };

  public:
    virtual ~ClemensHostView() = default;

    // reflection on the host view being executed
    virtual ViewType getViewType() const = 0;
    // per frame execution, returning the next view type
    virtual ViewType frame(int width, int height, double deltaTime, FrameAppInterop &interop) = 0;
    //  application input from OS
    virtual void input(ClemensInputEvent input) = 0;
    //  application lost focus
    virtual void lostFocus() = 0;
};

#endif
