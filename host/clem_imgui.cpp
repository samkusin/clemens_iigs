#include "clem_imgui.hpp"
#include "clem_assets.hpp"

#include "imgui.h"
#include "imgui_filedialog/ImGuiFileDialog.h"
#include "imgui_markdown.h"

#include "sokol/sokol_gfx.h"

#include <cstdio>
#include <filesystem>
#include <string_view>

namespace ClemensHostImGui {

static char tempCharBuffer[256];
static sg_image g_imgui_font_img;

static ImFont *g_fonts[kFontTotalCount];

static ImU32 getMarkdownH1Color() { return IM_COL32(255, 255, 0, 255); }

static ImU32 getMarkdownH2Color() {
    return (ImU32)(ImColor(ImGui::GetStyleColorVec4(ImGuiCol_TextDisabled)));
}

static ImU32 getMarkdownH3Color() {
    return (ImU32)(ImColor(ImGui::GetStyleColorVec4(ImGuiCol_TextDisabled)));
}

void FontSetup(ImVector<ImWchar> &unicodeRanges, const cinek::ByteBuffer &systemFontLoBuffer,
               const cinek::ByteBuffer &systemFontHiBuffer) {
    auto &io = ImGui::GetIO();
    // add fonts
    io.Fonts->Clear();

    const ImWchar *latinCodepoints = io.Fonts->GetGlyphRangesDefault();

    ImFontGlyphRangesBuilder glyphRangesBuilder;
    glyphRangesBuilder.AddRanges(latinCodepoints);
    glyphRangesBuilder.AddChar(0xe010); // open apple
    glyphRangesBuilder.AddChar(0xe098); // folder left
    glyphRangesBuilder.AddChar(0xe099); // folder right
    glyphRangesBuilder.BuildRanges(&unicodeRanges);

    constexpr float kFontSize = 16.0f; // 2 times the original pixel size

    ImFontConfig font_cfg;
    font_cfg.FontDataOwnedByAtlas = false;
    strncpy(font_cfg.Name, "A2Lo", sizeof(font_cfg.Name));
    g_fonts[kFontDefault] = io.Fonts->AddFontFromMemoryTTF(
        const_cast<uint8_t *>(systemFontLoBuffer.getHead()), systemFontLoBuffer.getSize(),
        kFontSize, &font_cfg, unicodeRanges.Data);

    font_cfg = ImFontConfig();
    font_cfg.FontDataOwnedByAtlas = false;
    strncpy(font_cfg.Name, "A2Hi", sizeof(font_cfg.Name));
    g_fonts[kFontNarrow] = io.Fonts->AddFontFromMemoryTTF(
        const_cast<uint8_t *>(systemFontHiBuffer.getHead()), systemFontHiBuffer.getSize(),
        kFontSize, &font_cfg, unicodeRanges.Data);

    font_cfg = ImFontConfig();
    font_cfg.FontDataOwnedByAtlas = false;
    strncpy(font_cfg.Name, "A2LoMed", sizeof(font_cfg.Name));
    g_fonts[kFontDefaultMedium] = io.Fonts->AddFontFromMemoryTTF(
        const_cast<uint8_t *>(systemFontLoBuffer.getHead()), systemFontLoBuffer.getSize(),
        kFontSize * 1.5f, &font_cfg, unicodeRanges.Data);

    font_cfg = ImFontConfig();
    font_cfg.FontDataOwnedByAtlas = false;
    strncpy(font_cfg.Name, "A2HiMed", sizeof(font_cfg.Name));
    g_fonts[kFontNarrowMedium] = io.Fonts->AddFontFromMemoryTTF(
        const_cast<uint8_t *>(systemFontHiBuffer.getHead()), systemFontHiBuffer.getSize(),
        kFontSize * 1.5f, &font_cfg, unicodeRanges.Data);

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
        g_imgui_font_img = sg_make_image(&img_desc);
        io.Fonts->TexID = (ImTextureID)(uintptr_t)g_imgui_font_img.id;
    }
}

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

void StatusBarField(StatusBarFlags flags, ImTextureID icon, const ImVec2 &labelSize) {
    ImVec2 cursorPos = ImGui::GetCursorScreenPos();
    ImVec2 labelPadding = ImGui::GetStyle().FramePadding;
    float labelBorderSize = ImGui::GetStyle().FrameBorderSize;
    ImVec2 widgetSize((labelBorderSize + labelPadding.x) * 2.0f + labelSize.x,
                      (labelBorderSize + labelPadding.y) * 2.0f + labelSize.y);
    ImGui::Dummy(widgetSize);

    ImColor cellBorderColor = ImGui::GetStyleColorVec4(ImGuiCol_Border);
    ImColor cellColor = ImGui::GetStyleColorVec4(ImGuiCol_TableRowBg);
    ImColor iconColor;
    if (flags == StatusBarFlags_Active)
        iconColor = ImColor(ImGui::GetStyleColorVec4(ImGuiCol_Text));
    else
        iconColor = ImColor(ImGui::GetStyleColorVec4(ImGuiCol_TextDisabled));

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
    drawList->AddImage(icon, ltanchor, rbanchor, ImVec2(0, 0), ImVec2(1, 1), (ImU32)iconColor);
}

bool IconButton(const char *strId, ImTextureID texId, const ImVec2 &size) {
    bool result = false;
    const ImGuiStyle &style = ImGui::GetStyle();
    ImDrawList *drawList = ImGui::GetWindowDrawList();
    ImVec2 cursorPos = ImGui::GetCursorScreenPos();
    ImVec2 rbanchorPos = cursorPos;
    rbanchorPos.x += size.x;
    rbanchorPos.y += size.y;
    result = ImGui::InvisibleButton(strId, size);
    ImVec4 imageColor;
    if (ImGui::IsItemActive()) {
        imageColor = style.Colors[ImGuiCol_ButtonActive];
    } else if (ImGui::IsItemHovered()) {
        imageColor = style.Colors[ImGuiCol_ButtonHovered];
    } else {
        imageColor = style.Colors[ImGuiCol_Button];
    }
    drawList->AddImage(texId, cursorPos, rbanchorPos, ImVec2(0.0f, 0.0f), ImVec2(1.0f, 1.0f),
                       (ImU32)ImColor(imageColor));
    return result;
}

void markdownLinkCallback(ImGui::MarkdownLinkCallbackData data) {
    // TODO: implement per platform the call to view a browser page
}

// https://github.com/juliettef/imgui_markdown/blob/e1e7885b1f02fe279ee4af3e513cecfa672d127d/imgui_markdown.h#L119
ImGui::MarkdownImageData markdownImageCallback(ImGui::MarkdownLinkCallbackData data) {
    ImGui::MarkdownImageData imageData;

    //  find the image asset - we only allow assets managed by ClemensHostAssets
    auto imageId =
        ClemensHostAssets::getImageFromName(std::string_view(data.link, data.linkLength));
    if (imageId != ClemensHostAssets::kInvalidImageId) {
        imageData.isValid = true;
    } else {
        imageId = ClemensHostAssets::kHelp;
        imageData.useLinkCallback = false;
    }
    imageData.isValid = true;
    imageData.user_texture_id = ImTextureID(ClemensHostAssets::getImage(imageId));
    imageData.size = ImVec2(ClemensHostAssets::getImageWidth(imageId),
                            ClemensHostAssets::getImageHeight(imageId));

    // For image resize when available size.x > image width, add
    ImVec2 const contentSize = ImGui::GetContentRegionAvail();
    if (imageData.size.x > contentSize.x) {
        float const ratio = imageData.size.y / imageData.size.x;
        imageData.size.x = contentSize.x;
        imageData.size.y = contentSize.x * ratio;
    }
    return imageData;
}

void markdownFormatCallback(const ImGui::MarkdownFormatInfo &markdownFormatInfo, bool start) {

    switch (markdownFormatInfo.type) {
        // example: change the colour of heading level 2
    case ImGui::MarkdownFormatType::HEADING: {
        if (start) {
            if (markdownFormatInfo.level == 1) {
                ImGui::PushFont(g_fonts[kFontDefaultMedium]);
                ImGui::PushStyleColor(ImGuiCol_Text, getMarkdownH1Color());
            } else if (markdownFormatInfo.level == 2) {
                ImGui::PushFont(g_fonts[kFontDefaultMedium]);
                ImGui::PushStyleColor(ImGuiCol_Text, getMarkdownH2Color());
            } else {
                ImGui::PushFont(g_fonts[kFontDefault]);
                ImGui::PushStyleColor(ImGuiCol_Text, getMarkdownH3Color());
            }
        } else {
            ImGui::PopStyleColor();
            ImGui::PopFont();
            if (markdownFormatInfo.level == 1) {
                ImGui::Separator();
                ImGui::Spacing();
            } else if (markdownFormatInfo.level == 2) {
                ImGui::Separator();
            } else {
                ImGui::Separator();
            }
        }

        break;
    }
    default:
        break;
    }
}

//  uses the ImGui markdown widget
void Markdown(std::string_view markdown) {
    ImGui::MarkdownConfig config;
    config.linkCallback = markdownLinkCallback;
    config.tooltipCallback = nullptr;
    config.imageCallback = markdownImageCallback;
    config.formatCallback = markdownFormatCallback;
    ImGui::Markdown(markdown.data(), markdown.size(), config);
}

auto ROMFileBrowser(int width, int height, const std::string &dataDirectory)
    -> ROMFileBrowserResult {
    ROMFileBrowserResult result;
    result.type = ROMFileBrowserResult::Continue;
    if (!ImGuiFileDialog::Instance()->IsOpened("Select ROM)")) {
        ImGuiFileDialog::Instance()->OpenDialog("Select ROM", "Select a ROM", ".*", ".", 1, nullptr,
                                                ImGuiFileDialogFlags_Modal);
    }
    if (ImGuiFileDialog::Instance()->Display(
            "Select ROM", ImGuiWindowFlags_NoCollapse,
            ImVec2(std::max(width * 0.75f, 640.0f), std::max(height * 0.75f, 480.0f)),
            ImVec2(width, height))) {

        if (ImGuiFileDialog::Instance()->IsOk()) {
            std::error_code errc;
            auto filePath = std::filesystem::path(ImGuiFileDialog::Instance()->GetFilePathName());
            auto fileName = ImGuiFileDialog::Instance()->GetCurrentFileName();
            auto destinationPath = std::filesystem::path(dataDirectory) / fileName;
            if (filePath == destinationPath) {
                result.filename = fileName;
                result.type = ROMFileBrowserResult::Ok;
            } else if (std::filesystem::copy_file(filePath, destinationPath,
                                                  std::filesystem::copy_options::overwrite_existing,
                                                  errc)) {
                result.filename = fileName;
                result.type = ROMFileBrowserResult::Ok;
            } else {
                result.filename = fileName;
                result.type = ROMFileBrowserResult::Error;
            }
        } else {
            result.type = ROMFileBrowserResult::Cancel;
        }
        ImGuiFileDialog::Instance()->Close();
    }
    return result;
}

} // namespace ClemensHostImGui
