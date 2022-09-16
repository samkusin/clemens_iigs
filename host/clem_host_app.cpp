
#include "clem_host_platform.h"
#include <cinttypes>

#if CLEMENS_PLATFORM_WINDOWS
#define SOKOL_D3D11
#endif

#include "imgui.h"

#include <array>
#include <cstdio>
#include <filesystem>

#define CLEMENS_NEW_GUI

#ifdef CLEMENS_NEW_GUI
#include "clem_front.hpp"
#else
#include "clem_host.hpp"
#endif

#define SOKOL_IMPL
#include "sokol/sokol_app.h"
#include "sokol/sokol_gfx.h"
#include "sokol/sokol_glue.h"
#include "sokol/sokol_imgui.h"


#ifdef CLEMENS_NEW_GUI
static ClemensFrontend* g_Host = nullptr;
#else
static ClemensHost* g_Host = nullptr;
#endif
static ClemensHostTimePoint g_LastTimepoint;
static sg_pass_action g_sgPassAction;
static unsigned g_ADBKeyToggleMask = 0;

std::array<unsigned, 512> g_sokolToADBKey;

cinek::ByteBuffer loadFont(const char* pathname) {
  FILE* fp = fopen(pathname, "rb");
  if (!fp) return cinek::ByteBuffer();

  fseek(fp, 0, SEEK_END);
  long sz = ftell(fp);
  unsigned char* buf = (unsigned char*)malloc(sz);
  fseek(fp, 0, SEEK_SET);
  fread(buf, 1, sz, fp);
  fclose(fp);
  return cinek::ByteBuffer(buf, (int32_t)sz, (int32_t)sz);
}

static void imguiFontSetup(const cinek::ByteBuffer& systemFontLoBuffer,
                           const cinek::ByteBuffer& systemFontHiBuffer) {
  auto& io = ImGui::GetIO();
    // add fonts
  io.Fonts->Clear();

  ImFontConfig font_cfg;
  font_cfg.FontDataOwnedByAtlas = false;


  strncpy(font_cfg.Name, "A2Lo", sizeof(font_cfg.Name));
  io.Fonts->AddFontFromMemoryTTF(const_cast<uint8_t*>(systemFontLoBuffer.getHead()),
                                 systemFontLoBuffer.getSize(),
                                 16.0f, &font_cfg);
  strncpy(font_cfg.Name, "A2Hi", sizeof(font_cfg.Name));
  io.Fonts->AddFontFromMemoryTTF(const_cast<uint8_t*>(systemFontHiBuffer.getHead()),
                                 systemFontHiBuffer.getSize(),
                                 16.0f, &font_cfg);

  if (!io.Fonts->IsBuilt()) {
    unsigned char *font_pixels;
    int font_width, font_height;
    io.Fonts->GetTexDataAsRGBA32(&font_pixels, &font_width, &font_height);
    sg_image_desc img_desc;
    memset(&img_desc, 0, sizeof(img_desc));
    img_desc.width = font_width;
    img_desc.height = font_height;
    img_desc.pixel_format = SG_PIXELFORMAT_RGBA8;
    img_desc.wrap_u = SG_WRAP_CLAMP_TO_EDGE;
    img_desc.wrap_v = SG_WRAP_CLAMP_TO_EDGE;
    img_desc.min_filter = SG_FILTER_LINEAR;
    img_desc.mag_filter = SG_FILTER_LINEAR;
    img_desc.data.subimage[0][0].ptr = font_pixels;
    img_desc.data.subimage[0][0].size = (size_t)(font_width * font_height) * sizeof(uint32_t);
    img_desc.label = "sokol-imgui-font";
    _simgui.img = sg_make_image(&img_desc);
    io.Fonts->TexID = (ImTextureID)(uintptr_t)_simgui.img.id;
  }
}

static void initDirectories() {
#ifdef CLEMENS_NEW_GUI
  std::filesystem::create_directory(CLEM_HOST_LIBRARY_DIR);
  std::filesystem::create_directory(CLEM_HOST_SNAPSHOT_DIR);
#endif
}

static void onInit()
{
  clem_host_timepoint_init();
  clem_host_timepoint_now(&g_LastTimepoint);

  initDirectories();

  CoInitializeEx(NULL, COINIT_MULTITHREADED);

  sg_desc desc = {};
  desc.context = sapp_sgcontext();
  sg_setup(desc);

  g_sgPassAction.colors[0].action = SG_ACTION_CLEAR;
  g_sgPassAction.colors[0].value = { 0.0f, 0.5f, 0.75f, 1.0f };

  simgui_desc_t simguiDesc = {};
#ifdef CLEMENS_NEW_GUI
  simguiDesc.no_default_font = true;
#endif
  simgui_setup(simguiDesc);

  g_sokolToADBKey.fill((unsigned)(-1));
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
  g_sokolToADBKey[SAPP_KEYCODE_F11] = CLEM_ADB_KEY_RESET;
  g_sokolToADBKey[SAPP_KEYCODE_F12] = CLEM_ADB_KEY_ESCAPE;
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
  g_sokolToADBKey[SAPP_KEYCODE_LEFT_SUPER] = CLEM_ADB_KEY_COMMAND_APPLE;
  g_sokolToADBKey[SAPP_KEYCODE_RIGHT_SHIFT] = CLEM_ADB_KEY_RSHIFT;
  g_sokolToADBKey[SAPP_KEYCODE_RIGHT_CONTROL] = CLEM_ADB_KEY_RCTRL;
  g_sokolToADBKey[SAPP_KEYCODE_RIGHT_ALT] = CLEM_ADB_KEY_ROPTION;
  //g_sokolToADBKey[SAPP_KEYCODE_RIGHT_SUPER] = CLEM_ADB_KEY_COMMAND_APPLE;

  auto systemFontLoBuffer = loadFont("fonts/PrintChar21.ttf");
  auto systemFontHiBuffer = loadFont("fonts/PRNumber3.ttf");
#ifdef CLEMENS_NEW_GUI
  imguiFontSetup(systemFontLoBuffer, systemFontHiBuffer);
  g_Host = new ClemensFrontend(systemFontLoBuffer, systemFontHiBuffer);
#else
  g_Host = new ClemensHost(systemFontLoBuffer, systemFontHiBuffer);
#endif
  free(systemFontLoBuffer.getHead());
  free(systemFontHiBuffer.getHead());
}

static void onFrame()
{
  const int frameWidth = sapp_width();
  const int frameHeight = sapp_height();
  ClemensHostTimePoint currentTimepoint;

  clem_host_timepoint_now(&currentTimepoint);
  auto deltaTime = clem_host_timepoint_deltaf(
    &currentTimepoint, &g_LastTimepoint);

  simgui_new_frame(frameWidth, frameHeight, deltaTime);

  g_Host->frame(frameWidth, frameHeight, deltaTime);

  sg_begin_default_pass(&g_sgPassAction, frameWidth, frameHeight);
  simgui_render();
  sg_end_pass();
  sg_commit();

  g_LastTimepoint = currentTimepoint;
}

static void onEvent(const sapp_event* evt)
{
  struct ClemensInputEvent clemInput {};
  switch (evt->type) {
    case SAPP_EVENTTYPE_KEY_DOWN:
      clemInput.value = g_sokolToADBKey[evt->key_code];
      clemInput.type = kClemensInputType_KeyDown;
      break;
    case SAPP_EVENTTYPE_KEY_UP:
      clemInput.value = g_sokolToADBKey[evt->key_code];
      clemInput.type = kClemensInputType_KeyUp;
      break;
    case SAPP_EVENTTYPE_MOUSE_DOWN:
      clemInput.type = kClemensInputType_MouseButtonDown;
      if (evt->mouse_button == SAPP_MOUSEBUTTON_LEFT) {
        clemInput.value |= 0x01;
      }
      break;
    case SAPP_EVENTTYPE_MOUSE_UP:
      clemInput.type = kClemensInputType_MouseButtonUp;
      if (evt->mouse_button == SAPP_MOUSEBUTTON_LEFT) {
        clemInput.value |= 0x01;
      }
      break;
    default:
      clemInput.type = kClemensInputType_None;
      break;
  }
  if (clemInput.type != kClemensInputType_None) {
    if (clem_host_get_caps_lock_state()) {
      g_ADBKeyToggleMask |= CLEM_ADB_KEYB_TOGGLE_CAPS_LOCK;
    } else {
      g_ADBKeyToggleMask &= ~CLEM_ADB_KEYB_TOGGLE_CAPS_LOCK;
    }
    clemInput.adb_key_toggle_mask = g_ADBKeyToggleMask;
    g_Host->input(clemInput);
  }

  simgui_handle_event(evt);
}

static void onCleanup()
{
  delete g_Host;

  g_Host = nullptr;

  CoUninitialize();

  simgui_shutdown();
  sg_shutdown();
}

static void onFail(const char* msg)
{
  printf("app failure: %s", msg);
}


sapp_desc sokol_main(int argc, char* argv[])
{
  sapp_desc sapp = {};

  sapp.width = 1440;
  sapp.height = 900;
  sapp.init_cb = &onInit;
  sapp.frame_cb = &onFrame;
  sapp.cleanup_cb = &onCleanup;
  sapp.event_cb = &onEvent;
  sapp.fail_cb = &onFail;
  sapp.window_title = "Clemens IIgs Developer";
  sapp.win32_console_create = true;
  sapp.win32_console_attach = true;

  return sapp;
}
