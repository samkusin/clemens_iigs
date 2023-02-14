#include "clem_imgui.hpp"
#include "imgui.h"

#include <cstdio>

namespace ClemensHostImGui {

char tempCharBuffer[256];

void StatusBarField(StatusBarFlags flags, const char *fmt, ...) {
    va_list args;
    va_start(args, fmt);
    int tempOutCount = vsnprintf(tempCharBuffer, sizeof(tempCharBuffer) - 1, fmt, args);
    tempCharBuffer[tempOutCount] = '\0';
    va_end(args);

    ImVec2 cursorPos = ImGui::GetCursorScreenPos();
    ImVec2 labelSize =
        ImGui::GetFont()->CalcTextSizeA(ImGui::GetFontSize(), FLT_MAX, 0.0f, tempCharBuffer);
    ImVec2 labelPadding = ImGui::GetStyle().FramePadding;
    float labelBorderSize = ImGui::GetStyle().FrameBorderSize;
    ImVec2 widgetSize((labelBorderSize + labelPadding.x) * 2.0f + labelSize.x,
                      (labelBorderSize + labelPadding.y) * 2.0f + labelSize.y);
    ImGui::Dummy(widgetSize);

    ImColor cellBorderColor = ImGui::GetStyleColorVec4(ImGuiCol_Border);
    ImColor cellColor = ImGui::GetStyleColorVec4(ImGuiCol_TableRowBg);
    ImColor textColor;
    if (flags == StatusBarFlags_Active)
        textColor = ImColor(ImGui::GetStyleColorVec4(ImGuiCol_Text));
    else
        textColor = ImColor(ImGui::GetStyleColorVec4(ImGuiCol_TextDisabled));

    ImDrawList *drawList = ImGui::GetWindowDrawList();
    ImVec2 ltanchor = cursorPos;
    ImVec2 rbanchor = ltanchor;
    rbanchor.x += widgetSize.x;
    rbanchor.y += widgetSize.y;
    drawList->AddRect(ltanchor, rbanchor, (ImU32)cellBorderColor);
    ltanchor.x += labelBorderSize;
    ltanchor.y += labelBorderSize;
    rbanchor.x -= labelBorderSize;
    rbanchor.y -= labelBorderSize;
    drawList->AddRectFilled(ltanchor, rbanchor, (ImU32)cellColor);
    ltanchor.x += labelPadding.x;
    ltanchor.y += labelPadding.y;
    rbanchor.x -= labelPadding.x;
    rbanchor.y -= labelPadding.y;
    drawList->AddText(ltanchor, (ImU32)textColor, tempCharBuffer);
}

} // namespace ClemensHostImGui
