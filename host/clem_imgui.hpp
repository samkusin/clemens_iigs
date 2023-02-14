#ifndef CLEM_HOST_IMGUI_HPP
#define CLEM_HOST_IMGUI_HPP

#include "imgui.h"

namespace ClemensHostImGui {

enum StatusBarFlags { StatusBarFlags_Inactive, StatusBarFlags_Active };

void StatusBarField(StatusBarFlags flags, const char *fmt, ...);

} // namespace ClemensHostImGui

#endif
