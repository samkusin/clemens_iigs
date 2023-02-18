#include "clem_preamble.hpp"
#include "clem_configuration.hpp"
#include "clem_l10n.hpp"

#include "imgui.h"
#include "version.h"

//  TODO: pass the configuration object (ClemensConfiguration) into this
//        class from ClemensFrontend (created there, it loads the INI file and
//        defines some configuration values.)

ClemensPreamble::ClemensPreamble(ClemensConfiguration &config)
    : config_(config), mode_(Mode::Start) {}

auto ClemensPreamble::frame(int width, int height) -> Result {
    const unsigned kLanguageDefault = ClemensL10N::kLanguageDefault;
    switch (mode_) {
    case Mode::Start:
        if (config_.majorVersion == CLEM_HOST_VERSION_MAJOR &&
            config_.minorVersion == CLEM_HOST_VERSION_MINOR) {
            mode_ = Mode::Continue;
        } else {
            ImGui::OpenPopup("Welcome");
            mode_ = Mode::NewVersion;
        }
        break;
    case Mode::NewVersion:
        ImGui::SetNextWindowSize(
            ImVec2(std::max(width * 0.75f, 512.0f), std::max(height * 0.66f, 400.0f)));
        if (ImGui::BeginPopupModal("Welcome", NULL, ImGuiWindowFlags_NoResize)) {
            auto contentSize = ImGui::GetContentRegionAvail();
            ImGui::BeginChild(ImGui::GetID("text"), ImVec2(-FLT_MIN, contentSize.y - 50));
            ImGui::TextWrapped(ClemensL10N::kWelcomeText[kLanguageDefault], CLEM_HOST_VERSION_MAJOR,
                               CLEM_HOST_VERSION_MINOR);
            ImGui::EndChild();
            if (ImGui::Button("Ok") || ImGui::IsKeyPressed(ImGuiKey_Enter) ||
                ImGui::IsKeyPressed(ImGuiKey_Space)) {
                ImGui::CloseCurrentPopup();
                if (!config_.majorVersion && !config_.minorVersion) {
                    mode_ = Mode::FirstUse;
                } else {
                    mode_ = Mode::Continue;
                }
            }
            ImGui::EndPopup();
            if (mode_ == Mode::FirstUse) {
                ImGui::OpenPopup("FirstUse");
            }
        }
        break;
    case Mode::FirstUse:
        ImGui::SetNextWindowSize(
            ImVec2(std::max(width * 0.75f, 512.0f), std::max(height * 0.66f, 400.0f)));
        if (ImGui::BeginPopupModal("FirstUse", NULL, ImGuiWindowFlags_NoResize)) {
            auto contentSize = ImGui::GetContentRegionAvail();
            ImGui::BeginChild(ImGui::GetID("text"), ImVec2(-FLT_MIN, contentSize.y - 50));
            ImGui::TextWrapped(ClemensL10N::kFirstTimeUse[kLanguageDefault],
                               CLEM_HOST_VERSION_MAJOR, CLEM_HOST_VERSION_MINOR);
            ImGui::TextWrapped(ClemensL10N::kGSKeyboardCommands[kLanguageDefault],
                               CLEM_HOST_VERSION_MAJOR, CLEM_HOST_VERSION_MINOR);
            ImGui::EndChild();
            if (ImGui::Button("Ok") || ImGui::IsKeyPressed(ImGuiKey_Enter)) {
                ImGui::CloseCurrentPopup();
                mode_ = Mode::Continue;
            }
            ImGui::EndPopup();
        }
        break;
    case Mode::Continue:
        //  create data directory
        config_.majorVersion = CLEM_HOST_VERSION_MAJOR;
        config_.minorVersion = CLEM_HOST_VERSION_MINOR;
        //  TODO: build data directory
        config_.save();
        return Result::Ok;
    case Mode::Exit:
        return Result::Exit;
    }
    return Result::Active;
}
