#ifndef CLEM_HOST_IMGUI_HPP
#define CLEM_HOST_IMGUI_HPP

#include "imgui.h"

#define CLEM_HOST_OPEN_APPLE_UTF8   "\xee\x80\x90"
#define CLEM_HOST_FOLDER_LEFT_UTF8  "\xee\x82\x98"
#define CLEM_HOST_FOLDER_RIGHT_UTF8 "\xee\x82\x99"

namespace ClemensHostImGui {

enum StatusBarFlags { StatusBarFlags_Inactive, StatusBarFlags_Active };

void StatusBarField(StatusBarFlags flags, const char *fmt, ...);
void StatusBarField(StatusBarFlags flags, ImTextureID icon, const ImVec2 &size);

//  use styling to color the image only with transparent background
bool IconButton(const char *strId, ImTextureID texId, const ImVec2 &size);

} // namespace ClemensHostImGui

#endif
