
#include "spdlog/common.h"
#include <cinttypes>

#ifdef _WIN32
#include <combaseapi.h>
#endif

#include "clem_host.hpp"
#include "clem_imgui.hpp"

#include <array>
#include <clocale>
#include <cmath>
#include <cstdio>
#include <filesystem>

#include "clem_host_platform.h"

#include "clem_assets.hpp"
#include "clem_front.hpp"
#include "clem_startup_view.hpp"

#include "sokol/sokol_app.h"
#include "sokol/sokol_audio.h"
#include "sokol/sokol_gfx.h"
#include "sokol/sokol_glue.h"
#include "sokol/sokol_imgui.h"
#include "sokol/sokol_time.h"
#include "spdlog/spdlog.h"

#include "fonts/font_bloada1024.h"
#include "fonts/font_printchar21.h"
#include "fonts/font_prnumber3.h"

struct SharedAppData {
    SharedAppData(int argc, char *argv[]) {
        if (argc > 1) {
            rootPathOverride = argv[1];
        }
    }

    std::string rootPathOverride;
    ImVector<ImWchar> imguiUnicodeRanges;
};

static uint64_t g_lastTime = 0;
static ClemensHostView *g_Host = nullptr;
static sg_pass_action g_sgPassAction;
static unsigned g_ADBKeyToggleMask = 0;
static cinek::ByteBuffer g_systemFontLoBuffer;
static cinek::ByteBuffer g_systemFontHiBuffer;
static const unsigned kClipboardTextLimit = 8192;
static ClemensHostInterop g_interop{};

//  Keyboard customization
//  Typically the OS specific "super" key is used to augment key combinations that
//  may otherwise be intercepted by the OS.  This usage really depends on the target
//  platform.  See each platform's implementation below for exceptional cases.

#if defined(CLEMENS_PLATFORM_LINUX)
//  utility for platforms that require special mapping of function keys
static int xlatToFnKey(const sapp_event *evt) {
    int fnKey = -1;
    if (evt->key_code >= SAPP_KEYCODE_0 && evt->key_code <= SAPP_KEYCODE_9) {
        fnKey = evt->key_code - SAPP_KEYCODE_0;
        if (fnKey == 0)
            fnKey = 10;
    } else if (evt->key_code == SAPP_KEYCODE_MINUS) {
        fnKey = 11;
    } else if (evt->key_code == SAPP_KEYCODE_EQUAL) {
        fnKey = 12;
    }
    return fnKey;
}
#endif

#if defined(CLEMENS_PLATFORM_WINDOWS)
// The SUPER key maps to the Windows Key, which is pretty much off limits
// for key-mapping.  Windows also traps the CTRL+ESC key combination.

// The logic below ensures that the ESC key can be used in the emulated
// machine in combination with either CTRL or ALT.
//
//  CTRL + F1   = CTRL-ESC
//  ALT + F1    = APPLE or OPTION - ESC
//

static bool g_escapeKeyDown = false;

static sapp_keycode onKeyDown(const sapp_event *evt) {
    if (evt->modifiers & (SAPP_MODIFIER_CTRL + SAPP_MODIFIER_ALT)) {
        if (evt->key_code == SAPP_KEYCODE_F1 && !g_escapeKeyDown) {
            g_escapeKeyDown = true;
            return SAPP_KEYCODE_ESCAPE;
        }
    }
    if (evt->key_code == SAPP_KEYCODE_ESCAPE) {
        if (g_escapeKeyDown)
            return SAPP_KEYCODE_INVALID;
    }
    return evt->key_code;
}

static sapp_keycode onKeyUp(const sapp_event *evt, bool *doDownEvent) {
    if (g_escapeKeyDown) {
        if (evt->key_code == SAPP_KEYCODE_F1) {
            g_escapeKeyDown = false;
            return SAPP_KEYCODE_ESCAPE;
        } else if (evt->key_code == SAPP_KEYCODE_ESCAPE) {
            return SAPP_KEYCODE_INVALID;
        }
    }
    *doDownEvent = false;
    return evt->key_code;
}
#elif defined(CLEMENS_PLATFORM_LINUX)
//  The Super/Tux key seems special-cased in Linux to bypass X Windows keyboard
//  shortcuts involving CTRL and ALT.  To prevent accidental triggering of
//  disruptive shortcut keys like ALT-Fx, the Super Key must be used in-tandem
//  with CTRL or ALT key down events before passing the event to the emulator.
//  As a side effect, ALT-Fx cannot be supported on X Windows -
//
//  SO: if Super Key is Down, Fx keys are remapped to 1 - 0, to avoid
//      conflicts between CTRL+ALT+Fx key presses.
//      delete key also maps to F12
//
static bool g_leftSuperKeyDown = false;
static bool g_rightSuperKeyDown = false;
static bool g_escapeKeyDown = false;
static bool g_fnKeys[12];

static sapp_keycode onKeyDown(const sapp_event *evt) {
    sapp_keycode outKeyCode = evt->key_code;

    if (!g_leftSuperKeyDown && evt->key_code == SAPP_KEYCODE_LEFT_SUPER)
        g_leftSuperKeyDown = true;
    if (!g_rightSuperKeyDown && evt->key_code == SAPP_KEYCODE_RIGHT_SUPER)
        g_rightSuperKeyDown = true;

    int fnKey = xlatToFnKey(evt);
    if (g_leftSuperKeyDown || g_rightSuperKeyDown) {
        if (fnKey > 0) {
            g_fnKeys[fnKey - 1] = true;
            outKeyCode = static_cast<sapp_keycode>(static_cast<int>(SAPP_KEYCODE_F1) + (fnKey - 1));
        }
    }
    if (evt->modifiers & (SAPP_MODIFIER_CTRL + SAPP_MODIFIER_ALT)) {
        if (g_fnKeys[0] && !g_escapeKeyDown) {
            g_escapeKeyDown = true;
            outKeyCode = SAPP_KEYCODE_ESCAPE;
        }
    }
    if (evt->key_code == SAPP_KEYCODE_ESCAPE) {
        if (g_escapeKeyDown)
            outKeyCode = SAPP_KEYCODE_INVALID;
    }
    return outKeyCode;
}

static sapp_keycode onKeyUp(const sapp_event *evt, bool *doDownEvent) {
    sapp_keycode outKeyCode = evt->key_code;

    if (g_leftSuperKeyDown && evt->key_code == SAPP_KEYCODE_LEFT_SUPER)
        g_leftSuperKeyDown = false;
    else if (g_rightSuperKeyDown && evt->key_code == SAPP_KEYCODE_RIGHT_SUPER)
        g_leftSuperKeyDown = false;

    int fnKey = xlatToFnKey(evt);
    if (fnKey > 0) {
        g_fnKeys[fnKey - 1] = false;
        outKeyCode = static_cast<sapp_keycode>(static_cast<int>(SAPP_KEYCODE_F1) + (fnKey - 1));
    }
    if (g_escapeKeyDown) {
        if (fnKey == 1) {
            g_escapeKeyDown = false;
            outKeyCode = SAPP_KEYCODE_ESCAPE;
        } else if (evt->key_code == SAPP_KEYCODE_ESCAPE) {
            outKeyCode = SAPP_KEYCODE_INVALID;
        }
    }
    *doDownEvent = false;
    return outKeyCode;
}

#elif defined(CLEMENS_PLATFORM_MACOS)
//  Option -> Alt (Option/Closed apple)
//  Command -> Super (Open Apple)
//  Both Option and Command do not have a 'right' equivalent exposed by NSEvent
//  Function keys often require pressing the Fn key on macOS (unless this feature
//      was turned off by the user via macOS preferences.)

//  edge case where CTRL + ESC does not report the ESCAPE down event
static bool g_escape_down = false;
static sapp_keycode onKeyDown(const sapp_event *evt) {
    sapp_keycode outKeyCode = evt->key_code;
    if (evt->key_code == SAPP_KEYCODE_LEFT_SUPER) {
        outKeyCode = SAPP_KEYCODE_RIGHT_ALT;
    }
    if (evt->modifiers & (SAPP_MODIFIER_CTRL + SAPP_MODIFIER_ALT)) {
        if (evt->key_code == SAPP_KEYCODE_F1 && !g_escape_down) {
            g_escape_down = true;
            return SAPP_KEYCODE_ESCAPE;
        }
    }
    if (evt->key_code == SAPP_KEYCODE_ESCAPE) {
        if (g_escape_down)
            return SAPP_KEYCODE_INVALID;
        g_escape_down = true;
    }
    return outKeyCode;
}

static sapp_keycode onKeyUp(const sapp_event *evt, bool *doDownEvent) {
    sapp_keycode outKeyCode = evt->key_code;
    *doDownEvent = false;
    if (evt->key_code == SAPP_KEYCODE_LEFT_SUPER) {
        outKeyCode = SAPP_KEYCODE_RIGHT_ALT;
    }
    if (evt->key_code == SAPP_KEYCODE_ESCAPE) {
        if (!g_escape_down)
            *doDownEvent = true;
        g_escape_down = false;
    } else if (g_escape_down) {
        if (evt->key_code == SAPP_KEYCODE_F1 || evt->key_code == SAPP_KEYCODE_ESCAPE) {
            g_escape_down = false;
            outKeyCode = SAPP_KEYCODE_ESCAPE;
        }
    }

    return outKeyCode;
}
#else
static sapp_keycode onKeyDown(const sapp_event *evt) { return evt->key_code; }

static sapp_keycode onKeyUp(const sapp_event *evt, bool *doDownEvent) {
    *doDownEvent = false;
    return evt->key_code;
}
#endif

std::array<int16_t, 512> g_sokolToADBKey;

void sokolLogger(const char *tag,              // e.g. 'sg'
                 uint32_t log_level,           // 0=panic, 1=error, 2=warn, 3=info
                 uint32_t,                     // SG_LOGITEM_*
                 const char *message_or_null,  // a message string, may be nullptr in release mode
                 uint32_t line_nr,             // line number in sokol_gfx.h
                 const char *filename_or_null, // source filename, may be nullptr in release mode
                 void *) {
    static spdlog::level::level_enum levels[] = {spdlog::level::critical, spdlog::level::err,
                                                 spdlog::level::warn, spdlog::level::info};
    if (!message_or_null)
        return;
    spdlog::log(levels[log_level], "[{}] {}({}) {}", tag, filename_or_null, line_nr,
                message_or_null);
}

static cinek::ByteBuffer loadFont(const char *pathname) {
    cinek::ByteBuffer buffer;
    if (!strcasecmp(pathname, "fonts/PrintChar21.ttf")) {
        buffer = cinek::ByteBuffer(PrintChar21_ttf, PrintChar21_ttf_len, PrintChar21_ttf_len);
    } else if (!strcasecmp(pathname, "fonts/PRNumber3.ttf")) {
        buffer = cinek::ByteBuffer(PRNumber3_ttf, PRNumber3_ttf_len, PRNumber3_ttf_len);
    }
    return buffer;
}

static void onInit(void *userdata) {
    auto *appdata = reinterpret_cast<SharedAppData *>(userdata);

    clemens_host_init(&g_interop);
    stm_setup();

#if defined(CLEMENS_PLATFORM_WINDOWS)
    CoInitializeEx(NULL, COINIT_MULTITHREADED);
#endif

    sg_desc desc = {};
    desc.context = sapp_sgcontext();
    desc.logger.func = sokolLogger;
    sg_setup(desc);

    g_sgPassAction.colors[0].action = SG_ACTION_CLEAR;
    g_sgPassAction.colors[0].value = {0.0f, 0.5f, 0.75f, 1.0f};

    simgui_desc_t simguiDesc = {};
    simguiDesc.no_default_font = true;
    simgui_setup(simguiDesc);

    g_sokolToADBKey.fill(-1);
    g_sokolToADBKey[SAPP_KEYCODE_SPACE] = CLEM_ADB_KEY_SPACE;
    g_sokolToADBKey[SAPP_KEYCODE_APOSTROPHE] = CLEM_ADB_KEY_APOSTRAPHE;
    g_sokolToADBKey[SAPP_KEYCODE_COMMA] = CLEM_ADB_KEY_COMMA;
    g_sokolToADBKey[SAPP_KEYCODE_MINUS] = CLEM_ADB_KEY_MINUS;
    g_sokolToADBKey[SAPP_KEYCODE_PERIOD] = CLEM_ADB_KEY_PERIOD;
    g_sokolToADBKey[SAPP_KEYCODE_SLASH] = CLEM_ADB_KEY_FWDSLASH;
    g_sokolToADBKey[SAPP_KEYCODE_0] = CLEM_ADB_KEY_0;
    g_sokolToADBKey[SAPP_KEYCODE_1] = CLEM_ADB_KEY_1;
    g_sokolToADBKey[SAPP_KEYCODE_2] = CLEM_ADB_KEY_2;
    g_sokolToADBKey[SAPP_KEYCODE_3] = CLEM_ADB_KEY_3;
    g_sokolToADBKey[SAPP_KEYCODE_4] = CLEM_ADB_KEY_4;
    g_sokolToADBKey[SAPP_KEYCODE_5] = CLEM_ADB_KEY_5;
    g_sokolToADBKey[SAPP_KEYCODE_6] = CLEM_ADB_KEY_6;
    g_sokolToADBKey[SAPP_KEYCODE_7] = CLEM_ADB_KEY_7;
    g_sokolToADBKey[SAPP_KEYCODE_8] = CLEM_ADB_KEY_8;
    g_sokolToADBKey[SAPP_KEYCODE_9] = CLEM_ADB_KEY_9;
    g_sokolToADBKey[SAPP_KEYCODE_SEMICOLON] = CLEM_ADB_KEY_SEMICOLON;
    g_sokolToADBKey[SAPP_KEYCODE_EQUAL] = CLEM_ADB_KEY_EQUALS;
    g_sokolToADBKey[SAPP_KEYCODE_A] = CLEM_ADB_KEY_A;
    g_sokolToADBKey[SAPP_KEYCODE_B] = CLEM_ADB_KEY_B;
    g_sokolToADBKey[SAPP_KEYCODE_C] = CLEM_ADB_KEY_C;
    g_sokolToADBKey[SAPP_KEYCODE_D] = CLEM_ADB_KEY_D;
    g_sokolToADBKey[SAPP_KEYCODE_E] = CLEM_ADB_KEY_E;
    g_sokolToADBKey[SAPP_KEYCODE_F] = CLEM_ADB_KEY_F;
    g_sokolToADBKey[SAPP_KEYCODE_G] = CLEM_ADB_KEY_G;
    g_sokolToADBKey[SAPP_KEYCODE_H] = CLEM_ADB_KEY_H;
    g_sokolToADBKey[SAPP_KEYCODE_I] = CLEM_ADB_KEY_I;
    g_sokolToADBKey[SAPP_KEYCODE_J] = CLEM_ADB_KEY_J;
    g_sokolToADBKey[SAPP_KEYCODE_K] = CLEM_ADB_KEY_K;
    g_sokolToADBKey[SAPP_KEYCODE_L] = CLEM_ADB_KEY_L;
    g_sokolToADBKey[SAPP_KEYCODE_M] = CLEM_ADB_KEY_M;
    g_sokolToADBKey[SAPP_KEYCODE_N] = CLEM_ADB_KEY_N;
    g_sokolToADBKey[SAPP_KEYCODE_O] = CLEM_ADB_KEY_O;
    g_sokolToADBKey[SAPP_KEYCODE_P] = CLEM_ADB_KEY_P;
    g_sokolToADBKey[SAPP_KEYCODE_Q] = CLEM_ADB_KEY_Q;
    g_sokolToADBKey[SAPP_KEYCODE_R] = CLEM_ADB_KEY_R;
    g_sokolToADBKey[SAPP_KEYCODE_S] = CLEM_ADB_KEY_S;
    g_sokolToADBKey[SAPP_KEYCODE_T] = CLEM_ADB_KEY_T;
    g_sokolToADBKey[SAPP_KEYCODE_U] = CLEM_ADB_KEY_U;
    g_sokolToADBKey[SAPP_KEYCODE_V] = CLEM_ADB_KEY_V;
    g_sokolToADBKey[SAPP_KEYCODE_W] = CLEM_ADB_KEY_W;
    g_sokolToADBKey[SAPP_KEYCODE_X] = CLEM_ADB_KEY_X;
    g_sokolToADBKey[SAPP_KEYCODE_Y] = CLEM_ADB_KEY_Y;
    g_sokolToADBKey[SAPP_KEYCODE_Z] = CLEM_ADB_KEY_Z;
    g_sokolToADBKey[SAPP_KEYCODE_LEFT_BRACKET] = CLEM_ADB_KEY_LBRACKET;
    g_sokolToADBKey[SAPP_KEYCODE_BACKSLASH] = CLEM_ADB_KEY_LBRACKET;
    g_sokolToADBKey[SAPP_KEYCODE_RIGHT_BRACKET] = CLEM_ADB_KEY_LBRACKET;
    g_sokolToADBKey[SAPP_KEYCODE_GRAVE_ACCENT] = CLEM_ADB_KEY_BACKQUOTE;
    g_sokolToADBKey[SAPP_KEYCODE_ESCAPE] = CLEM_ADB_KEY_ESCAPE;
    g_sokolToADBKey[SAPP_KEYCODE_ENTER] = CLEM_ADB_KEY_RETURN;
    g_sokolToADBKey[SAPP_KEYCODE_TAB] = CLEM_ADB_KEY_TAB;
    g_sokolToADBKey[SAPP_KEYCODE_BACKSPACE] = CLEM_ADB_KEY_DELETE;
    g_sokolToADBKey[SAPP_KEYCODE_INSERT] = CLEM_ADB_KEY_HELP_INSERT;
    g_sokolToADBKey[SAPP_KEYCODE_DELETE] = CLEM_ADB_KEY_DELETE;
    g_sokolToADBKey[SAPP_KEYCODE_RIGHT] = CLEM_ADB_KEY_RIGHT;
    g_sokolToADBKey[SAPP_KEYCODE_LEFT] = CLEM_ADB_KEY_LEFT;
    g_sokolToADBKey[SAPP_KEYCODE_DOWN] = CLEM_ADB_KEY_DOWN;
    g_sokolToADBKey[SAPP_KEYCODE_UP] = CLEM_ADB_KEY_UP;
    g_sokolToADBKey[SAPP_KEYCODE_PAGE_UP] = CLEM_ADB_KEY_PAGEUP;
    g_sokolToADBKey[SAPP_KEYCODE_PAGE_DOWN] = CLEM_ADB_KEY_PAGEDOWN;
    g_sokolToADBKey[SAPP_KEYCODE_HOME] = CLEM_ADB_KEY_HOME;
    g_sokolToADBKey[SAPP_KEYCODE_END] = CLEM_ADB_KEY_END;
    g_sokolToADBKey[SAPP_KEYCODE_CAPS_LOCK] = CLEM_ADB_KEY_CAPSLOCK;
    g_sokolToADBKey[SAPP_KEYCODE_NUM_LOCK] = CLEM_ADB_KEY_PAD_CLEAR_NUMLOCK;
    g_sokolToADBKey[SAPP_KEYCODE_F1] = CLEM_ADB_KEY_F1;
    g_sokolToADBKey[SAPP_KEYCODE_F2] = CLEM_ADB_KEY_F2;
    g_sokolToADBKey[SAPP_KEYCODE_F3] = CLEM_ADB_KEY_F3;
    g_sokolToADBKey[SAPP_KEYCODE_F4] = CLEM_ADB_KEY_F4;
    g_sokolToADBKey[SAPP_KEYCODE_F5] = CLEM_ADB_KEY_F5;
    g_sokolToADBKey[SAPP_KEYCODE_F6] = CLEM_ADB_KEY_F6;
    g_sokolToADBKey[SAPP_KEYCODE_F7] = CLEM_ADB_KEY_F7;
    g_sokolToADBKey[SAPP_KEYCODE_F8] = CLEM_ADB_KEY_F8;
    g_sokolToADBKey[SAPP_KEYCODE_F9] = CLEM_ADB_KEY_F9;
    g_sokolToADBKey[SAPP_KEYCODE_F10] = CLEM_ADB_KEY_F10;
    g_sokolToADBKey[SAPP_KEYCODE_F11] = CLEM_ADB_KEY_F11;
    g_sokolToADBKey[SAPP_KEYCODE_F12] = CLEM_ADB_KEY_RESET;
    g_sokolToADBKey[SAPP_KEYCODE_F13] = CLEM_ADB_KEY_F13;
    g_sokolToADBKey[SAPP_KEYCODE_F14] = CLEM_ADB_KEY_F14;
    g_sokolToADBKey[SAPP_KEYCODE_F15] = CLEM_ADB_KEY_F15;
    g_sokolToADBKey[SAPP_KEYCODE_KP_0] = CLEM_ADB_KEY_PAD_0;
    g_sokolToADBKey[SAPP_KEYCODE_KP_1] = CLEM_ADB_KEY_PAD_1;
    g_sokolToADBKey[SAPP_KEYCODE_KP_2] = CLEM_ADB_KEY_PAD_2;
    g_sokolToADBKey[SAPP_KEYCODE_KP_3] = CLEM_ADB_KEY_PAD_3;
    g_sokolToADBKey[SAPP_KEYCODE_KP_4] = CLEM_ADB_KEY_PAD_4;
    g_sokolToADBKey[SAPP_KEYCODE_KP_5] = CLEM_ADB_KEY_PAD_5;
    g_sokolToADBKey[SAPP_KEYCODE_KP_6] = CLEM_ADB_KEY_PAD_6;
    g_sokolToADBKey[SAPP_KEYCODE_KP_7] = CLEM_ADB_KEY_PAD_7;
    g_sokolToADBKey[SAPP_KEYCODE_KP_8] = CLEM_ADB_KEY_PAD_8;
    g_sokolToADBKey[SAPP_KEYCODE_KP_9] = CLEM_ADB_KEY_PAD_9;
    g_sokolToADBKey[SAPP_KEYCODE_KP_DECIMAL] = CLEM_ADB_KEY_PAD_DECIMAL;
    g_sokolToADBKey[SAPP_KEYCODE_KP_DIVIDE] = CLEM_ADB_KEY_PAD_DIVIDE;
    g_sokolToADBKey[SAPP_KEYCODE_KP_MULTIPLY] = CLEM_ADB_KEY_PAD_MULTIPLY;
    g_sokolToADBKey[SAPP_KEYCODE_KP_SUBTRACT] = CLEM_ADB_KEY_PAD_MINUS;
    g_sokolToADBKey[SAPP_KEYCODE_KP_ADD] = CLEM_ADB_KEY_PAD_PLUS;
    g_sokolToADBKey[SAPP_KEYCODE_KP_ENTER] = CLEM_ADB_KEY_PAD_ENTER;
    g_sokolToADBKey[SAPP_KEYCODE_KP_EQUAL] = CLEM_ADB_KEY_PAD_EQUALS;
    g_sokolToADBKey[SAPP_KEYCODE_LEFT_SHIFT] = CLEM_ADB_KEY_LSHIFT;
    g_sokolToADBKey[SAPP_KEYCODE_LEFT_CONTROL] = CLEM_ADB_KEY_LCTRL;
    g_sokolToADBKey[SAPP_KEYCODE_LEFT_ALT] = CLEM_ADB_KEY_OPTION;
    g_sokolToADBKey[SAPP_KEYCODE_RIGHT_SHIFT] = CLEM_ADB_KEY_RSHIFT;
    g_sokolToADBKey[SAPP_KEYCODE_RIGHT_CONTROL] = CLEM_ADB_KEY_RCTRL;
    g_sokolToADBKey[SAPP_KEYCODE_RIGHT_ALT] = CLEM_ADB_KEY_COMMAND_OPEN_APPLE;

    g_systemFontLoBuffer = loadFont("fonts/PrintChar21.ttf");
    g_systemFontHiBuffer = loadFont("fonts/PRNumber3.ttf");
    ClemensHostImGui::FontSetup(appdata->imguiUnicodeRanges, g_systemFontLoBuffer,
                                g_systemFontHiBuffer);
    ClemensHostAssets::initialize();

    g_Host = new ClemensStartupView();
}

static void onFrame(void *) {
    const int frameWidth = sapp_width();
    const int frameHeight = sapp_height();

    uint64_t deltaTicks = stm_laptime(&g_lastTime);
    double deltaTime = stm_sec(deltaTicks);
    bool exitApp = g_interop.exitApp;

    simgui_frame_desc_t imguiFrameDesc = {};
    imguiFrameDesc.delta_time = deltaTime;
    imguiFrameDesc.dpi_scale = 1.0f;
    imguiFrameDesc.width = frameWidth;
    imguiFrameDesc.height = frameHeight;

    simgui_new_frame(imguiFrameDesc);
    if (g_Host) {
        g_interop.mouseLock = sapp_mouse_locked();
        g_interop.mouseShow = sapp_mouse_shown();
        g_interop.poweredOn = false;

        auto nextViewType = g_Host->frame(frameWidth, frameHeight, deltaTime, g_interop);
        sapp_lock_mouse(g_interop.mouseLock);
        if (g_interop.mouseShow != sapp_mouse_shown()) {
            sapp_show_mouse(g_interop.mouseShow);
        }
        if (g_interop.action == ClemensHostInterop::PasteFromClipboard) {
            //  this is separate from ImGui's clipboard support so that the emulator controls when
            //  it receives clipboard data
            g_Host->pasteText(sapp_get_clipboard_string(), kClipboardTextLimit);
        }
        g_interop.action = ClemensHostInterop::None;
        exitApp = g_interop.exitApp;

        clemens_host_update();

        if (nextViewType != g_Host->getViewType()) {
            auto *oldHost = g_Host;
            switch (nextViewType) {
            case ClemensHostView::ViewType::Startup:
                g_Host = new ClemensStartupView();
                break;
            case ClemensHostView::ViewType::Main: {
                ClemensConfiguration config;
                if (oldHost->getViewType() == ClemensHostView::ViewType::Startup) {
                    config = static_cast<ClemensStartupView *>(oldHost)->getConfiguration();
                }
                g_Host = new ClemensFrontend(config, g_systemFontLoBuffer, g_systemFontHiBuffer);
                break;
            }
            default:
                g_Host = nullptr;
                CK_ASSERT(false);
                exitApp = true;
                break;
            }

            if (oldHost) {
                delete oldHost;
            }
        }
    }
    sg_begin_default_pass(&g_sgPassAction, frameWidth, frameHeight);
    simgui_render();
    sg_end_pass();
    sg_commit();

    if (exitApp) {
        sapp_request_quit();
    }
}

static void doHostInputEvent(struct ClemensInputEvent &clemInput, uint32_t modifiers) {
    if (clemInput.type == kClemensInputType_None)
        return;
    if (modifiers & SAPP_MODIFIER_CAPS) {
        g_ADBKeyToggleMask |= CLEM_ADB_KEYB_TOGGLE_CAPS_LOCK;
    } else {
        g_ADBKeyToggleMask &= ~CLEM_ADB_KEYB_TOGGLE_CAPS_LOCK;
    }
    clemInput.adb_key_toggle_mask = g_ADBKeyToggleMask;
    if (g_Host)
        g_Host->input(clemInput);
}

static void onEvent(const sapp_event *evt, void *) {
    struct ClemensInputEvent clemInput {};

    simgui_handle_event(evt);

    sapp_keycode keycode;
    bool doDownEventOnKeyUp = false;

    switch (evt->type) {
    case SAPP_EVENTTYPE_UNFOCUSED:
        if (g_Host)
            g_Host->lostFocus();
        break;
    case SAPP_EVENTTYPE_FOCUSED:
        if (g_Host)
            g_Host->gainFocus();
        break;
    case SAPP_EVENTTYPE_KEY_DOWN:
        keycode = onKeyDown(evt);
        if (keycode != SAPP_KEYCODE_INVALID) {
            clemInput.value_a = g_sokolToADBKey[keycode];
            clemInput.type = kClemensInputType_KeyDown;
        }
        break;
    case SAPP_EVENTTYPE_KEY_UP:
        keycode = onKeyUp(evt, &doDownEventOnKeyUp);
        if (keycode != SAPP_KEYCODE_INVALID) {
            clemInput.value_a = g_sokolToADBKey[keycode];
        }
        if (doDownEventOnKeyUp) {
            printf("Do ESCAPE DOWN\n");
            //  on lost key down events, emulate a key tap.
            if (keycode != SAPP_KEYCODE_INVALID) {
                clemInput.type = kClemensInputType_KeyDown;
            }
            doHostInputEvent(clemInput, evt->modifiers);
        }
        if (keycode != SAPP_KEYCODE_INVALID) {
            clemInput.type = kClemensInputType_KeyUp;
        }
        break;
    case SAPP_EVENTTYPE_MOUSE_DOWN:
        clemInput.type = kClemensInputType_MouseButtonDown;
        if (evt->mouse_button == SAPP_MOUSEBUTTON_LEFT) {
            clemInput.value_a |= 0x01;
            clemInput.value_b |= 0x01;
        }
        break;
    case SAPP_EVENTTYPE_MOUSE_UP:
        clemInput.type = kClemensInputType_MouseButtonUp;
        if (evt->mouse_button == SAPP_MOUSEBUTTON_LEFT) {
            clemInput.value_a |= 0x01;
            clemInput.value_b |= 0x01;
        }
        break;
    case SAPP_EVENTTYPE_MOUSE_MOVE:
        clemInput.type = kClemensInputType_MouseMove;
        clemInput.value_a = (int16_t)(std::floor(evt->mouse_dx));
        clemInput.value_b = (int16_t)(std::floor(evt->mouse_dy));
        break;
    default:
        clemInput.type = kClemensInputType_None;
        break;
    }
    doHostInputEvent(clemInput, evt->modifiers);
}

static void onCleanup(void *userdata) {
    auto *appdata = reinterpret_cast<SharedAppData *>(userdata);
    if (g_Host) {
        delete g_Host;
        g_Host = nullptr;
    }
#if defined(CLEMENS_PLATFORM_WINDOWS)
    CoUninitialize();
#endif
    delete appdata;

    ClemensHostAssets::terminate();

    spdlog::shutdown();

    simgui_shutdown();
    sg_shutdown();
    clemens_host_terminate();
}

sapp_desc sokol_main(int argc, char *argv[]) {
    sapp_desc sapp = {};

    //  TODO: investigate other locales based on localization features
    if (const char *loc = std::setlocale(LC_ALL, "en_US.UTF-8")) {
        fprintf(stdout, "locale: %s\n", loc);
    }
    //  TODO: logging startup should go here

    spdlog::set_level(spdlog::level::info);
    spdlog::flush_on(spdlog::level::err);
    spdlog::info("Setting up host frameworks");

    sapp.user_data = new SharedAppData(argc, argv);
    sapp.height = 900;
    sapp.width = 1440;
    sapp.init_userdata_cb = &onInit;
    sapp.frame_userdata_cb = &onFrame;
    sapp.cleanup_userdata_cb = &onCleanup;
    sapp.event_userdata_cb = &onEvent;
    sapp.window_title = "Clemens IIGS";
    sapp.win32_console_create = true;
    sapp.win32_console_attach = true;
    sapp.logger.func = sokolLogger;
    sapp.clipboard_size = kClipboardTextLimit;
    sapp.enable_clipboard = true;

    return sapp;
}
