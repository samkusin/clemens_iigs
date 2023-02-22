#ifndef CLEM_HOST_IMGUI_HPP
#define CLEM_HOST_IMGUI_HPP

#include "imgui.h"

namespace ClemensHostImGui {

enum StatusBarFlags { StatusBarFlags_Inactive, StatusBarFlags_Active };

void StatusBarField(StatusBarFlags flags, const char *fmt, ...);
void StatusBarField(StatusBarFlags flags, ImTextureID icon, const ImVec2 &size);

//  use styling to color the image only with transparent background
bool IconButton(const char *strId, ImTextureID texId, const ImVec2 &size);

} // namespace ClemensHostImGui

#endif
