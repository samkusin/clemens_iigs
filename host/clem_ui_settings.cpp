#include "clem_ui_settings.hpp"
#include "clem_configuration.hpp"
#include "clem_l10n.hpp"

#include "clem_imgui.hpp"

#include "clem_mmio_defs.h"
#include "core/clem_apple2gs_config.hpp"
#include "fmt/format.h"
#include "imgui.h"
#include "imgui_internal.h"

#include <filesystem>

namespace {
std::array<int, 3> kSupportedRAMSizes = {1024, 4096, 8192};
} // namespace

ClemensSettingsUI::ClemensSettingsUI(ClemensConfiguration &config)
    : config_(config), mode_(Mode::None), romFileExists_(false) {}

void ClemensSettingsUI::stop() { mode_ = Mode::None; }

void ClemensSettingsUI::start() {
    mode_ = Mode::Main;
    romFileExists_ = !config_.romFilename.empty() && std::filesystem::exists(config_.romFilename);

    for (unsigned slotIdx = 0; slotIdx < CLEM_CARD_SLOT_COUNT; ++slotIdx) {
        cardCounts_[slotIdx] = 0;

        auto names = getCardNamesForSlot(slotIdx);
        for (unsigned cardIdx = 0; cardIdx < (unsigned)names.size(); ++cardIdx) {
            if (names[0] == nullptr)
                break;
            cardCounts_[slotIdx]++;
        }
    }
}

bool ClemensSettingsUI::frame() {
    bool startMachine = false;
    switch (mode_) {
    case Mode::None:
        break;
    case Mode::Main: {
        ImGui::SeparatorText(CLEM_L10N_LABEL(kSettingsMachineSystemSetup));
        ImGui::BeginTable("Machine", 2);
        ImGui::TableSetupColumn("", ImGuiTableColumnFlags_WidthFixed,
                                ImGui::GetFont()->GetCharAdvance('A') * 20);
        ImGui::TableSetupColumn("", ImGuiTableColumnFlags_WidthStretch);
        ImGui::TableNextRow();
        {
            ImGui::TableNextColumn();
            ImGui::TextUnformatted(CLEM_L10N_LABEL(kSettingsMachineROMFilename));
            ImGui::TableNextColumn();
            ImGui::GetContentRegionMax();
            if (ImGui::Button(CLEM_HOST_FOLDER_LEFT_UTF8 CLEM_HOST_FOLDER_RIGHT_UTF8 "  ")) {
                mode_ = Mode::ROMFileBrowse;
            }
            ImGui::SameLine();
            ImGui::TextUnformatted(config_.romFilename.c_str());
            if (!romFileExists_) {
                ImGui::Spacing();
                ImGui::Indent();
                ImGui::SameLine();
                ImGui::PushStyleColor(ImGuiCol_Text, IM_COL32(255, 32, 0, 255));
                if (config_.romFilename.empty()) {
                    ImGui::TextWrapped("%s", CLEM_L10N_LABEL(kSettingsROMFileWarning));
                } else {
                    ImGui::TextWrapped("%s", CLEM_L10N_LABEL(kSettingsROMFileError));
                }
                ImGui::PopStyleColor();
                ImGui::Unindent();
            }
            ImGui::NewLine();
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
        ImGui::TableNextRow();
        ImGui::TableNextColumn();
        ImGui::Text("Cards");
        ImGui::TableNextColumn();
        for (unsigned slotIdx = 0; slotIdx < CLEM_CARD_SLOT_COUNT; ++slotIdx) {
            char slotLabel[32];
            if (cardCounts_[slotIdx] > 0) {
                auto cards = getCardNamesForSlot(slotIdx);
                snprintf(slotLabel, sizeof(slotLabel), "Slot %u", slotIdx + 1);
                if (ImGui::BeginCombo(slotLabel, !config_.gs.cardNames[slotIdx].empty()
                                                     ? config_.gs.cardNames[slotIdx].c_str() 
                                                     : "None")) {
                    if (ImGui::Selectable("None", config_.gs.cardNames[slotIdx].empty())) {
                        config_.gs.cardNames[slotIdx].clear();
                    }
                    for (auto cardNameIt = cards.begin(); cardNameIt != cards.end(); ++cardNameIt) {
                        if (*cardNameIt == nullptr) break;
                        if (ImGui::Selectable(*cardNameIt,
                                              config_.gs.cardNames[slotIdx] == *cardNameIt)) {
                            config_.gs.cardNames[slotIdx] = *cardNameIt;
                        }
                    }
                    ImGui::EndCombo();
                }
            }
        }
        ImGui::EndTable();

        ImGui::NewLine();
        if (ImGui::Button("Power On", ImVec2(ImGui::GetFont()->GetCharAdvance('W') * 20,
                                             ImGui::GetTextLineHeight() * 2))) {
            startMachine = true;
        }
        ImGui::NewLine();

        ImGui::SeparatorText(CLEM_L10N_LABEL(kSettingsTabEmulation));
        ImGui::BeginTable("Emulation", 2, ImGuiTableFlags_SizingStretchSame);
        ImGui::TableSetupColumn("", ImGuiTableColumnFlags_WidthFixed,
                                ImGui::GetFont()->GetCharAdvance('A') * 20);
        ImGui::TableSetupColumn("", ImGuiTableColumnFlags_WidthStretch);
        ImGui::TableNextRow();
        {
            ImGui::TableNextColumn();
            ImGui::TextUnformatted(CLEM_L10N_LABEL(kSettingsEmulationFastDisk));
            ImGui::TableNextColumn();
            ImGui::Checkbox("", &config_.fastEmulationEnabled);
            ImGui::Spacing();
            ImGui::Indent();
            ImGui::SameLine();
            ImGui::PushStyleColor(ImGuiCol_Text, IM_COL32(255, 255, 0, 255));
            ImGui::TextWrapped("%s", CLEM_L10N_LABEL(kSettingsEmulationFaskDiskHelp));
            ImGui::PopStyleColor();
            ImGui::Unindent();
        }
        ImGui::EndTable();
        break;
    }
    case Mode::ROMFileBrowse: {
        ImGui::SeparatorText(CLEM_L10N_LABEL(kSettingsMachineROMFilename));
        auto size = ImGui::GetContentRegionAvail();
        fileBrowser_.frame(size);
        if (fileBrowser_.isDone()) {
            if (fileBrowser_.isSelected()) {
                config_.romFilename = fileBrowser_.getCurrentPathname();
                romFileExists_ = std::filesystem::exists(config_.romFilename);
            }
            mode_ = Mode::Main;
        }
        break;
    }
    }

    return startMachine;
}
