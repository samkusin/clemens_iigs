#include "clem_ui_settings.hpp"

#include "clem_imgui.hpp"
#include "fmt/format.h"
#include "imgui.h"

#include <filesystem>

bool ClemensSettingsUI::isStarted() const { return mode_ != Mode::None; }

void ClemensSettingsUI::start(ClemensBackend *) { mode_ = Mode::Active; }

bool ClemensSettingsUI::frame(float width, float /* height */, ClemensBackend * /*backend */) {
    ImVec2 center = ImGui::GetMainViewport()->GetCenter();
    switch (mode_) {
    case Mode::None:
        break;
    case Mode::Active: {
        if (!ImGui::IsPopupOpen("Settings")) {
            ImGui::OpenPopup("Settings");
        }
        ImGui::SetNextWindowPos(center, ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
        ImGui::SetNextWindowSize(ImVec2(std::max(720.0f, width * 0.66f), 0.0f));
        if (ImGui::BeginPopupModal("Settings", NULL, ImGuiWindowFlags_Modal)) {
            ImGui::Spacing();

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
    case Mode::Commit:
        return true;
    case Mode::Cancelled:
        return true;
    }
    return false;
}

void ClemensSettingsUI::stop(ClemensBackend *) { mode_ = Mode::None; }
