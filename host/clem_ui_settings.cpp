#include "clem_ui_settings.hpp"
#include "clem_configuration.hpp"
#include "clem_l10n.hpp"

#include "clem_imgui.hpp"
#include "fmt/format.h"
#include "imgui.h"
#include "imgui_internal.h"

#include <filesystem>

namespace {
std::array<int, 3> kSupportedRAMSizes = {1024, 4096, 8192};
} // namespace

ClemensSettingsUI::ClemensSettingsUI(ClemensConfiguration &config) : config_(config) {}

void ClemensSettingsUI::frame() {
    char romFilename[1024];

    ImGui::SeparatorText(CLEM_L10N_LABEL(kSettingsMachineSystemSetup));
    ImGui::BeginTable("Machine", 2, ImGuiTableFlags_SizingStretchSame);
    ImGui::TableNextRow();
    {
        ImGui::TableNextColumn();
        ImGui::TextUnformatted(CLEM_L10N_LABEL(kSettingsMachineROMFilename));
        ImGui::TableNextColumn();
        fmt::format_to_n(romFilename, sizeof(romFilename), "{}{} {}", CLEM_HOST_FOLDER_LEFT_UTF8,
                         CLEM_HOST_FOLDER_RIGHT_UTF8, config_.romFilename.c_str());
        if (ImGui::Button(romFilename)) {
            // mode_ = Mode::ROMFileBrowse;
        }
    }
    ImGui::TableNextRow();
    {
        ImGui::TableNextColumn();
        ImGui::TextUnformatted(CLEM_L10N_LABEL(kSettingsMachineSystemMemory));
        ImGui::TableNextColumn();
        int ramSizeKB = config_.gs.memory;
        bool nonstandardRAMSize = true;
        for (unsigned i = 0; i < kSupportedRAMSizes.size(); ++i) {
            if (ramSizeKB == kSupportedRAMSizes[i]) {
                nonstandardRAMSize = false;
            }
        }
        if (nonstandardRAMSize) {
            ImGui::PushItemFlag(ImGuiItemFlags_Disabled, true);
            ImGui::PushStyleVar(ImGuiStyleVar_Alpha, ImGui::GetStyle().Alpha * 0.5f);
            ImGui::Text("Configured: %dK", ramSizeKB);
            ImGui::Spacing();
        }
        for (unsigned ramSizeIdx = 0; ramSizeIdx < kSupportedRAMSizes.size(); ++ramSizeIdx) {
            char radioLabel[8]{};
            snprintf(radioLabel, sizeof(radioLabel) - 1, "%dK", kSupportedRAMSizes[ramSizeIdx]);
            ImGui::RadioButton(radioLabel, &ramSizeKB, kSupportedRAMSizes[ramSizeIdx]);
        }
        if (nonstandardRAMSize) {
            ImGui::PopItemFlag();
            ImGui::PopStyleVar();
        }
        config_.gs.memory = ramSizeKB;
    }
    ImGui::EndTable();

    ImGui::SeparatorText(CLEM_L10N_LABEL(kSettingsTabEmulation));
    ImGui::BeginTable("Emulation", 2, ImGuiTableFlags_SizingStretchSame);
    ImGui::TableNextRow();
    {
        ImGui::TableNextColumn();
        ImGui::TextUnformatted(CLEM_L10N_LABEL(kSettingsEmulationFastDisk));
        ImGui::TableNextColumn();
        ImGui::Checkbox("", &config_.fastEmulationEnabled);
        ImGui::Indent();
        ImGui::SameLine();
        ImGui::PushStyleColor(ImGuiCol_Text, IM_COL32(255, 255, 0, 255));
        ImGui::TextWrapped("%s", CLEM_L10N_LABEL(kSettingsEmulationFaskDiskHelp));
        ImGui::PopStyleColor();
        ImGui::Unindent();
    }
    ImGui::EndTable();
}

/*
bool ClemensSettingsUI::isStarted() const { return mode_ != Mode::None; }

void ClemensSettingsUI::start(const ClemensConfiguration &config) {
    mode_ = Mode::Active;
    config_ = config;
    auto cnt = config_.romFilename.copy(romFilename_, sizeof(romFilename_) - 1);
    romFilename_[cnt] = '\0';
    //  effectively support only our selectable values
    //  and disable the radio button otherwise
    ramSizeKB_ = config_.gs.memory;
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
                    ImGui::Spacing();
                    ImGui::PushStyleColor(ImGuiCol_Text, IM_COL32(255, 255, 0, 255));
                    ImGui::TextWrapped("%s", CLEM_L10N_LABEL(kSettingsMachineSystemSetupInfo));
                    ImGui::PopStyleColor();
                    ImGui::Spacing();
                    ImGui::SeparatorText(CLEM_L10N_LABEL(kSettingsMachineSystemSetup));
                    if (ImGui::Button(CLEM_HOST_FOLDER_LEFT_UTF8 CLEM_HOST_FOLDER_RIGHT_UTF8)) {
                        mode_ = Mode::ROMFileBrowse;
                    }
                    ImGui::SameLine();
                    ImGui::InputText(CLEM_L10N_LABEL(kSettingsMachineROMFilename), romFilename_,
                                     sizeof(romFilename_));
                    ImGui::Spacing();
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
                    ImGui::Spacing();
                    ImGui::PushStyleColor(ImGuiCol_Text, IM_COL32(255, 255, 0, 255));
                    ImGui::TextWrapped("%s", CLEM_L10N_LABEL(kSettingsNotAvailable));
                    ImGui::PopStyleColor();
                    ImGui::Spacing();
                    ImGui::EndTabItem();
                }
                if (ImGui::BeginTabItem(CLEM_L10N_LABEL(kSettingsTabEmulation))) {
                    ImGui::Checkbox(CLEM_L10N_LABEL(kSettingsEmulationFastDisk),
                                    &config_.fastEmulationEnabled);
                    ImGui::Indent();
                    ImGui::PushStyleColor(ImGuiCol_Text, IM_COL32(255, 255, 0, 255));
                    ImGui::TextWrapped("%s", CLEM_L10N_LABEL(kSettingsEmulationFaskDiskHelp));
                    ImGui::PopStyleColor();
                    ImGui::Unindent();
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
    case Mode::Commit: {
        bool confirmed = false;
        if (willRequireRestart()) {
            if (!ImGui::IsPopupOpen("Settings Committed")) {
                ImGui::OpenPopup("Settings Committed");
            }
            if (ImGui::BeginPopupModal("Settings Committed")) {
                ImGui::Spacing();
                ImGui::Text("Machine settings will take effect on the next power cycle.");
                ImGui::Separator();
                if (ImGui::Button("Ok")) {
                    ImGui::CloseCurrentPopup();
                    updateConfig();
                    confirmed = true;
                }
                ImGui::EndPopup();
            }
        } else {
            updateConfig();
            confirmed = true;
        }
        return confirmed;
    }
    case Mode::Cancelled:
        return true;
    }
    return false;
}

bool ClemensSettingsUI::willRequireRestart() const {
    return (config_.gs.memory != (unsigned)(ramSizeKB_)) || (config_.romFilename != romFilename_);
}

void ClemensSettingsUI::updateConfig() {
    config_.gs.memory = ramSizeKB_;
    config_.romFilename = romFilename_;
}

bool ClemensSettingsUI::shouldBeCommitted() const { return mode_ == Mode::Commit; }

void ClemensSettingsUI::stop() { mode_ = Mode::None; }
*/
