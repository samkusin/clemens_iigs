
#include "clem_host_platform.h"

#if CLEMENS_PLATFORM_WINDOWS
#define SOKOL_D3D11
#endif

#include "imgui/imgui.h"

#define SOKOL_IMPL
#include "sokol/sokol_app.h"
#include "sokol/sokol_gfx.h"
#include "sokol/sokol_glue.h"
#include "sokol/sokol_imgui.h"

#include <cstdio>

#include "clem_host.hpp"

static ClemensHost* g_Host = nullptr;
static ClemensHostTimePoint g_LastTimepoint;
static sg_pass_action g_sgPassAction;

static void onInit()
{
  clem_host_timepoint_init();
  clem_host_timepoint_now(&g_LastTimepoint);

  sg_desc desc = {};
  desc.context = sapp_sgcontext();
  sg_setup(desc);

  g_sgPassAction.colors[0].action = SG_ACTION_CLEAR;
  g_sgPassAction.colors[0].value = { 0.0f, 0.5f, 0.75f, 1.0f };

  simgui_desc_t simguiDesc = {};
  simgui_setup(simguiDesc);

  g_Host = new ClemensHost();
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
  simgui_handle_event(evt);
}

static void onCleanup()
{
  delete g_Host;
  g_Host = nullptr;

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

  return sapp;
}
