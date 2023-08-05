#if defined(CK3D_BACKEND_D3D11)
#define SOKOL_D3D11
#elif defined(CK3D_BACKEND_GL)
#define SOKOL_GLCORE33
#endif

#include "imgui.h"

#define SOKOL_IMPL
#include "sokol/sokol_app.h"
#include "sokol/sokol_audio.h"
#include "sokol/sokol_gfx.h"
#include "sokol/sokol_gfx_ext.h"
#include "sokol/sokol_glue.h"
#include "sokol/sokol_imgui.h"
#include "sokol/sokol_time.h"

#include "clem_host.hpp"

static ClemensHostInterop *s_interop = nullptr;

void clemens_host_init(struct ClemensHostInterop *interop) { s_interop = interop; }

void clemens_host_update() {
#if defined(WIN32)
    // augment the minimum height using the title bar metric
    // assuming the default style used by sokol_app
    RECT windowRect;
    windowRect.left = 0;
    windowRect.top = 0;
    windowRect.right = s_interop->minWindowWidth;
    windowRect.bottom = s_interop->minWindowHeight;
    AdjustWindowRectEx(&windowRect, 0, FALSE, WS_EX_OVERLAPPEDWINDOW);
    sapp_win32_set_min_window_size(windowRect.right - windowRect.left,
                                   windowRect.bottom - windowRect.top);
#endif
}

void clemens_host_terminate() {}
