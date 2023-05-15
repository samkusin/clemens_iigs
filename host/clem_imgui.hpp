#ifndef CLEM_HOST_IMGUI_HPP
#define CLEM_HOST_IMGUI_HPP

#include "imgui.h"

#include "cinek/buffer.hpp"
#include <string>
#include <string_view>

#define CLEM_HOST_OPEN_APPLE_UTF8   "\xee\x80\x90"
#define CLEM_HOST_FOLDER_LEFT_UTF8  "\xee\x82\x98"
#define CLEM_HOST_FOLDER_RIGHT_UTF8 "\xee\x82\x99"

namespace ClemensHostImGui {

constexpr unsigned kFontDefault = 0;
constexpr unsigned kFontNarrow = 1;
constexpr unsigned kFontDefaultMedium = 2;
constexpr unsigned kFontNarrowMedium = 3;
constexpr unsigned kFontTotalCount = 4;

void FontSetup(ImVector<ImWchar> &unicodeRanges, const cinek::ByteBuffer &systemFontLoBuffer,
               const cinek::ByteBuffer &systemFontHiBuffer);

enum StatusBarFlags { StatusBarFlags_Inactive, StatusBarFlags_Active };

void StatusBarField(StatusBarFlags flags, const char *fmt, ...);
void StatusBarField(StatusBarFlags flags, ImTextureID icon, const ImVec2 &size);

//  use styling to color the image only with transparent background
bool IconButton(const char *strId, ImTextureID texId, const ImVec2 &size);

//  uses the ImGui markdown widget.
//  returns the link to navigate to if selected
std::string Markdown(std::string_view markdown);

//  styling hooks for common widgets
void PushStyleButtonEnabled();
void PushStyleButtonDisabled();
void PopStyleButton();

} // namespace ClemensHostImGui

#endif
