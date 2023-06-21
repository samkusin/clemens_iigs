#ifndef CLEM_HOST_APP_VIEW_HPP
#define CLEM_HOST_APP_VIEW_HPP

#include "clem_mmio_types.h"
#include "clem_host.hpp"

#include <cstdint>

class ClemensHostView {
  public:
    enum class ViewType { Startup, Main };
 
  public:
    virtual ~ClemensHostView() = default;

    // reflection on the host view being executed
    virtual ViewType getViewType() const = 0;
    // per frame execution, returning the next view type
    virtual ViewType frame(int width, int height, double deltaTime, ClemensHostInterop &interop) = 0;
    //  application input from OS
    virtual void input(ClemensInputEvent input) = 0;
    //  is the emulator accepting input events
    virtual bool emulatorHasFocus() const = 0;
    //  paste text from the clipboard
    virtual void pasteText(const char *text, unsigned textSizeLimit) = 0;
    //  application lost focus
    virtual void lostFocus() = 0;
};

#endif
