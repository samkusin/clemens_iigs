#if defined(CK3D_BACKEND_METAL)
#define SOKOL_METAL
#elif defined(CK3D_BACKEND_GL)
#define SOKOL_GLCORE33
#endif

#include "imgui.h"

#define SOKOL_IMPL
#include "sokol/sokol_app.h"
#include "sokol/sokol_audio.h"
#include "sokol/sokol_gfx.h"
#include "sokol/sokol_glue.h"
#include "sokol/sokol_imgui.h"
#include "sokol/sokol_time.h"
