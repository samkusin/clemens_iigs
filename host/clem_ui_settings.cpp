#include "clem_ui_settings.hpp"
#include "clem_l10n.hpp"

#include "clem_imgui.hpp"
#include "fmt/format.h"
#include "imgui.h"
#include "imgui_internal.h"

#include <filesystem>

namespace {
static constexpr int kSupportedRAMSizeCount = 3;
static int s_supportedRAMSizes[kSupportedRAMSizeCount] = {1024, 4096, 8192};
} // namespace

bool ClemensSettingsUI::isStarted() const { return mode_ != Mode::None; }

void ClemensSettingsUI::start(const ClemensConfiguration &config) {
    mode_ = Mode::Active;
    config_ = config;
    auto cnt = config_.romFilename.copy(romFilename_, sizeof(romFilename_) - 1);
    romFilename_[cnt] = '\0';
    //  effectively support only our selectable values
    //  and disable the radio button otherwise
    ramSizeKB_ = config_.ramSizeKB;
    nonstandardRAMSize_ = true;
    for (int i = 0; i < kSupportedRAMSizeCount; ++i) {
        if (ramSizeKB_ == s_supportedRAMSizes[i]) {
            nonstandardRAMSize_ = false;
        }
    }
}

bool ClemensSettingsUI::frame(float width, float height) {
    ImVec2 center = ImGui::GetMainViewport()->GetCenter();
    switch (mode_) {
    case Mode::None:
        break;
    case Mode::Active: {
        if (!ImGui::IsPopupOpen("Settings")) {
            ImGui::OpenPopup("Settings");
        }
        ImGui::SetNextWindowPos(center, ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
        ImGui::SetNextWindowSize(
            ImVec2(std::max(512.0f, width * 0.50f), std::max(512.0f, height * 0.50f)));
        if (ImGui::BeginPopupModal("Settings", NULL, ImGuiWindowFlags_Modal)) {
            const float kFooterSize = ImGui::GetFrameHeightWithSpacing() * 2;
            const ImVec2 kContentRegion = ImGui::GetContentRegionAvail();
            ImGui::BeginChild("SettingsTab",
                              ImVec2(kContentRegion.x, kContentRegion.y - kFooterSize));
            if (ImGui::BeginTabBar("Sections")) {
                const ImVec2 &itemSpacing = ImGui::GetStyle().ItemSpacing;
                ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing,
                                    ImVec2(itemSpacing.x, itemSpacing.y * 1.5f));
                if (ImGui::BeginTabItem(CLEM_L10N_LABEL(kSettingsTabMachine))) {
                    ImGui::SeparatorText(CLEM_L10N_LABEL(kSettingsMachineSystemSetup));
                    ImGui::TextWrapped("%s", CLEM_L10N_LABEL(kSettingsMachineSystemSetupInfo));
                    ImGui::Spacing();
                    if (ImGui::Button(CLEM_HOST_FOLDER_LEFT_UTF8 CLEM_HOST_FOLDER_RIGHT_UTF8)) {
                        mode_ = Mode::ROMFileBrowse;
                    }
                    ImGui::SameLine();
                    ImGui::InputText(CLEM_L10N_LABEL(kSettingsMachineROMFilename), romFilename_,
                                     sizeof(romFilename_));
                    ImGui::SeparatorText(CLEM_L10N_LABEL(kSettingsMachineSystemMemory));
                    if (nonstandardRAMSize_) {
                        ImGui::PushItemFlag(ImGuiItemFlags_Disabled, true);
                        ImGui::PushStyleVar(ImGuiStyleVar_Alpha, ImGui::GetStyle().Alpha * 0.5f);
                        ImGui::Text("Configured: %dK", ramSizeKB_);
                        ImGui::Spacing();
                    }
                    for (int ramSizeIdx = 0; ramSizeIdx < kSupportedRAMSizeCount; ++ramSizeIdx) {
                        char radioLabel[8]{};
                        snprintf(radioLabel, sizeof(radioLabel) - 1, "%dK",
                                 s_supportedRAMSizes[ramSizeIdx]);
                        ImGui::RadioButton(radioLabel, &ramSizeKB_,
                                           s_supportedRAMSizes[ramSizeIdx]);
                    }
                    if (nonstandardRAMSize_) {
                        ImGui::PopItemFlag();
                        ImGui::PopStyleVar();
                    }
                    ImGui::Spacing();
                    ImGui::SeparatorText(CLEM_L10N_LABEL(kSettingsMachineCards));

                    // ImGui::Combo("Slot 4", )

                    ImGui::EndTabItem();
                }
                if (ImGui::BeginTabItem(CLEM_L10N_LABEL(kSettingsTabEmulation))) {
                    ImGui::EndTabItem();
                }
                ImGui::PopStyleVar();
                ImGui::EndTabBar();
            }
            ImGui::EndChild();

            ImGui::Separator();
            if (ImGui::Button("Ok") || ImGui::IsKeyPressed(ImGuiKey_Enter)) {
                ImGui::CloseCurrentPopup();
                mode_ = Mode::Commit;
            }
            ImGui::SameLine();
            if (ImGui::Button("Cancel")) {
                ImGui::CloseCurrentPopup();
                mode_ = Mode::Cancelled;
            }
            ImGui::EndPopup();
        }
    } break;
    case Mode::ROMFileBrowse: {
        auto result = ClemensHostImGui::ROMFileBrowser(width, height, config_.dataDirectory);
        if (result.type == ClemensHostImGui::ROMFileBrowserResult::Ok) {
            config_.romFilename = result.filename;
            auto cnt = config_.romFilename.copy(romFilename_, sizeof(romFilename_) - 1);
            romFilename_[cnt] = '\0';
            mode_ = Mode::Active;
        } else if (result.type == ClemensHostImGui::ROMFileBrowserResult::Cancel) {
            mode_ = Mode::Active;
        } else if (result.type == ClemensHostImGui::ROMFileBrowserResult::Error) {
            errorMessage_ =
                fmt::format("There was a problem copying the ROM file {}.", result.filename);
            mode_ = Mode::ROMFileBrowseError;
        }
        break;
    }
    case Mode::ROMFileBrowseError:
        if (!ImGui::IsPopupOpen("SettingsError")) {
            ImGui::OpenPopup("SettingsError");
        }
        ImGui::SetNextWindowPos(center, 0, ImVec2(0.5f, 0.5f));
        ImGui::SetNextWindowSize(ImVec2(width * 0.50f, 0.0f));
        if (ImGui::BeginPopupModal("SettingsError", NULL, ImGuiWindowFlags_AlwaysAutoResize)) {
            ImGui::PushTextWrapPos();
            ImGui::NewLine();
            ImGui::TextUnformatted(errorMessage_.c_str());
            ImGui::NewLine();
            ImGui::Separator();
            ImGui::PopTextWrapPos();
            if (ImGui::Button("OK", ImVec2(240, 0))) {
                mode_ = Mode::Active;
                ImGui::CloseCurrentPopup();
            }
            ImGui::EndPopup();
        }
        break;
    case Mode::Commit:
        return true;
    case Mode::Cancelled:
        return true;
    }
    return false;
}

bool ClemensSettingsUI::shouldBeCommitted() const { return mode_ == Mode::Commit; }

void ClemensSettingsUI::stop() { mode_ = Mode::None; }
