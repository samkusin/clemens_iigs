#include "clem_preamble.hpp"
#include "clem_configuration.hpp"

#include "imgui.h"
#include "version.h"

namespace ClemensL10N {
const char *kWelcomeText[] = {"Clemens IIGS Emulator v%d.%d\n"
                              "\n\n"
                              "This release contains the following changes:\n\n"
                              "- SmartPort hard drive emulation\n"
                              "- Joystick/Paddles support\n"
                              "- Ctrl-Apple-Escape trigger for the Desktop Manager\n"
                              "- Minor fixes on the IWM\n"
                              "- Separation of 65816 and I/O emulator libraries\n"
                              "- UI improvements\n"
                              "\n\n"
                              "The following IIGS features are not yet supported:\n"
                              "\n"
                              "- Serial Controller emulation\n"
                              "- ROM 1 support\n"
                              "- ROM 0 (//e-like) support\n"
                              "- Emulator Localization (PAL, Monochrome)"};

const char *kFirstTimeUse[] = {
    "Usage instructions for the Emulator:\n"
    "\n"
    "At the terminal prompt (lower-right side of the view), enter 'help' to display instructions "
    "for supported commands.\n"
    "\n"
    "Useful Terminal Commands:\n"
    "- reboot                       : reboots the system\n"
    "- help                         : displays all supported commands\n"
    "- break                        : breaks into the debugger\n"
    "- save <snapshot_name>         : saves a snapshot of the current system\n"
    "- load <snapshot_name>         : loads a saved snapshot\n"
    "\n"
    "The emulator requires a ROM binary (ROM 03 as of this build.)  This file must be stored in "
    "the same directory as the application.\n"
    "\n"
    "Disk images are loaded by 'importing' them into the Emulator's library folder.\n"
    "\n"
    "- Choose a drive from the four drive widgets in the top left of the Emulator view\n"
    "- Select 'import' to import one or more disk images as a set (.DSK, .2MG, .PO, .DO, .WOZ)\n"
    "- Enter the desired disk set name\n"
    "- A folder will be created in the library folder for the disk set containing the imported "
    "images\n"
    "\n"
    "IIGS Keyboard Commands:\n"
    "\n"
    "- Ctrl-Alt-F12 to reboot the system\n"
    "- Ctrl-F12 for CTRL-RESET\n"
    "- Ctrl-Left Alt-F1 to enter the Control Panel\n"};
} // namespace ClemensL10N

//  TODO: pass the configuration object (ClemensConfiguration) into this
//        class from ClemensFrontend (created there, it loads the INI file and
//        defines some configuration values.)

ClemensPreamble::ClemensPreamble(ClemensConfiguration &config)
    : config_(config), mode_(Mode::Start) {}

auto ClemensPreamble::frame(int width, int height) -> Result {
    const unsigned kLanguageDefault = 0;
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
            ImVec2(std::max(width * 0.75f, 512.0f), std::max(height * 0.6f, 384.0f)));
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
            ImVec2(std::max(width * 0.75f, 512.0f), std::max(height * 0.6f, 384.0f)));
        if (ImGui::BeginPopupModal("FirstUse", NULL, ImGuiWindowFlags_NoResize)) {
            auto contentSize = ImGui::GetContentRegionAvail();
            ImGui::BeginChild(ImGui::GetID("text"), ImVec2(-FLT_MIN, contentSize.y - 50));
            ImGui::TextWrapped(ClemensL10N::kFirstTimeUse[kLanguageDefault],
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
        config_.majorVersion = CLEM_HOST_VERSION_MAJOR;
        config_.minorVersion = CLEM_HOST_VERSION_MINOR;
        config_.save();
        return Result::Ok;
    case Mode::Exit:
        return Result::Exit;
    }
    return Result::Active;
}
