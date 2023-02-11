#include "clem_startup_view.hpp"
#include "clem_host_platform.h"
#include "clem_preamble.hpp"

#include "imgui.h"

#include "imgui_filedialog/ImGuiFileDialog.h"

#include <filesystem>

namespace {

//  For all platforms, the config file is guaranteed to be located in a
//  predefined location.  The config file is effectively our 'registry' to
//  use windows terminology.
//
ClemensConfiguration createConfiguration(const std::filesystem::path defaultDataDirectory) {
    auto configPath = defaultDataDirectory / "config.ini";
    return ClemensConfiguration(configPath.string(), defaultDataDirectory.string());
}

ClemensConfiguration findConfiguration(const std::string &rootDirectoryOverride) {
    //  find the config file on the system, or using the directory override
    if (!rootDirectoryOverride.empty()) {
        return createConfiguration(std::filesystem::path(rootDirectoryOverride));
    }

    //  local directory configuration check
    char localpath[CLEMENS_PATH_MAX];
    size_t localpathSize = sizeof(localpath);
    if (get_process_executable_path(localpath, &localpathSize)) {
        if (strnlen(localpath, CLEMENS_PATH_MAX - 1) >= sizeof(localpath) - 1) {
            //  If this is a problem, later code will determine whether the path
            //  was actually truncated
            printf("Warning: discovered configuration pathname is likely truncated!\n");
        }
        auto dataDirectory = std::filesystem::path(localpath).parent_path();
        auto configPath = dataDirectory / "config.ini";
        if (std::filesystem::exists(configPath)) {
            return createConfiguration(dataDirectory);
        }
    } else {
        //  TODO: handle systems that support dynamic path sizes (i.e. localpathSize != sizeof(localpath))
        printf("Warning: unable to obtain our local executable path. Falling back to user data "
               "paths.\n");
    }
    if (!get_local_user_data_directory(localpath, sizeof(localpath), CLEM_HOST_COMPANY_NAME,
                                       CLEM_HOST_APPLICATION_NAME)) {
        return ClemensConfiguration();
    }
    return createConfiguration(std::filesystem::path(localpath));
}

} // namespace

/**
 * @brief Constructs an Application Startup view
 *
 * This is the first GUI the user will see.  It validates the environment running
 * this process.
 *
 * For Clemens IIGS, StartupView sets up an environment for emulation data assets
 * that's highly dependent on the current host platform.
 *
 * Steps:
 *  - obtain a configuration object that is passed to the main emulator
 *  - validate asset folders, making sure that they exist, and locate them
 *    based on the OS and location of the current executable.
 *
 *  - All platforms
 *    - Optional argument to specify a local data directory via command line
 *      arguments (mainly if developing or really want to locate your data
 *      someplace other than what this application prefers...)
 *
 *  - Windows
 *    - Supports both Portable and User installs
 *    - Check if the directory where this process is running has write protections
 *    - Also check if its placed in a system-specific location (i.e. Program Files)
 *      - If located in %ProgramFiles% or  %LocalAppData%\Programs,
 *        - Use %LOCALAPPDATA%
 *      - Otherwise offer the option of a portable install vs per-user
 *        - If portable, use the current folder
 *        - If per-user, use %LOCALAPPDATA%
 *  - Linux
 *    - Supports only User installs since many times the app will be installed in
 *      /usr/local/bin or /usr/bin
 *  - MacOS
 *    - Supports only User installs and store in ~/Library/Application or equivalent
 */
ClemensStartupView::ClemensStartupView(std::string rootDirectoryOverride)
    : mode_(Mode::Initial), config_(findConfiguration(rootDirectoryOverride)) {}

auto ClemensStartupView::frame(int width, int height, double /*deltaTime */,
                               FrameAppInterop &interop) -> ViewType {
    auto nextView = ViewType::Startup;

    switch (mode_) {
    case Mode::Initial: {
        //  if newly installed, inform user of the data directory location and
        //  allow option to change
        if (config_.isNewInstall()) {
            if (!ImGui::IsPopupOpen("Configure Local Data Directory")) {
                ImGui::OpenPopup("Configure Local Data Directory");
            }
        } else {
            //  Popup will be closed, and so the remaining logic will be skipped.
            mode_ = Mode::Preamble;
        }
        ImVec2 center = ImGui::GetMainViewport()->GetCenter();
        ImGui::SetNextWindowPos(center, 0, ImVec2(0.5f, 0.5f));
        ImGui::SetNextWindowSize(ImVec2(width * 0.75f, 0.0f));
        if (ImGui::BeginPopupModal("Configure Local Data Directory", NULL,
                                   ImGuiWindowFlags_AlwaysAutoResize)) {
            ImGui::PushTextWrapPos();
            ImGui::NewLine();
            ImGui::TextUnformatted("The directory below,");
            ImGui::NewLine();
            ImGui::Text("    %s", config_.dataDirectory.c_str());
            ImGui::NewLine();
            ImGui::TextUnformatted(
                "Will be created to contain imported disks, save states and other file outputs.");
            ImGui::NewLine();
            ImGui::Separator();
            ImGui::PopTextWrapPos();
            if (ImGui::Button("OK", ImVec2(240, 0))) {
                mode_ = Mode::Preamble;
                ImGui::CloseCurrentPopup();
            }
            ImGui::SetItemDefaultFocus();
            ImGui::SameLine();
            if (ImGui::Button("Change Directory", ImVec2(240, 0))) {
                mode_ = Mode::ChangeDataDirectory;
                ImGui::CloseCurrentPopup();
                ImGuiFileDialog::Instance()->OpenDialog("ChooseDirDlgKey", "Select a Directory",
                                                        nullptr, ".", 1, nullptr,
                                                        ImGuiFileDialogFlags_Modal);
            }
            ImGui::EndPopup();
        }
    } break;

    case Mode::ChangeDataDirectory:
        if (ImGuiFileDialog::Instance()->Display(
                "ChooseDirDlgKey", ImGuiWindowFlags_NoCollapse,
                ImVec2(std::max(width * 0.75f, 640.0f), std::max(height * 0.75f, 480.0f)),
                ImVec2(width, height))) {
            if (ImGuiFileDialog::Instance()->IsOk()) {
                auto filePath = ImGuiFileDialog::Instance()->GetCurrentPath();
                config_.dataDirectory = filePath;
                mode_ = Mode::Initial;
            }
            ImGuiFileDialog::Instance()->Close();
        }
        break;

    case Mode::Preamble:
        if (!preamble_) {
            if (validateDirectories()) {
                preamble_ = std::make_unique<ClemensPreamble>(config_);
            } else {
                mode_ = Mode::SetupError;
            }
        }
        if (preamble_) {
            auto preambleResult = preamble_->frame(width, height);
            if (preambleResult != ClemensPreamble::Result::Active) {
                if (preambleResult == ClemensPreamble::Result::Ok) {
                    preamble_ = nullptr;
                    mode_ = Mode::Finished;
                } else if (preambleResult == ClemensPreamble::Result::Exit) {
                    preamble_ = nullptr;
                    mode_ = Mode::Aborted;
                }
            }
        }
        break;

    case Mode::SetupError: {
        if (!ImGui::IsPopupOpen("Error")) {
            ImGui::OpenPopup("Error");
        }

        ImVec2 center = ImGui::GetMainViewport()->GetCenter();
        ImGui::SetNextWindowPos(center, 0, ImVec2(0.5f, 0.5f));
        ImGui::SetNextWindowSize(ImVec2(width * 0.50f, 0.0f));
        if (ImGui::BeginPopupModal("Error", NULL, ImGuiWindowFlags_AlwaysAutoResize)) {
            ImGui::PushTextWrapPos();
            ImGui::NewLine();
            ImGui::TextUnformatted("There was a problem saving the configuration.");
            ImGui::NewLine();
            ImGui::TextUnformatted(setupError_.c_str());
            ImGui::NewLine();
            ImGui::TextUnformatted("The application will exit.");
            ImGui::NewLine();
            ImGui::Separator();
            ImGui::PopTextWrapPos();
            if (ImGui::Button("OK", ImVec2(240, 0))) {
                mode_ = Mode::Aborted;
                ImGui::CloseCurrentPopup();
            }
            ImGui::EndPopup();
        }
    } break;

    case Mode::Aborted:
        interop.exitApp = true;
        break;

    case Mode::Finished:
        nextView = ViewType::Main;
        break;
    }

    return nextView;
}

bool ClemensStartupView::validateDirectories() {
    auto rootDir = std::filesystem::path(config_.iniPathname).parent_path();
    std::error_code errc;

    if (rootDir.is_absolute()) {
        if (!std::filesystem::exists(rootDir)) {
            if (!std::filesystem::create_directories(rootDir, errc)) {
                setupError_ = "Invalid configuration path " + config_.iniPathname;
                return false;
            }
        }
    }
    std::array<std::string, 4> dataDirs = {"", CLEM_HOST_LIBRARY_DIR, CLEM_HOST_SNAPSHOT_DIR,
                                           CLEM_HOST_TRACES_DIR};
    rootDir = std::filesystem::path(config_.dataDirectory);
    for (auto &dataDir : dataDirs) {
        if ((bool)errc) {
            setupError_ = errc.message();
            return false;
        }

        auto dataPath = rootDir;
        if (!dataDir.empty())
            dataPath /= dataDir;
        if (!std::filesystem::exists(dataPath)) {
            std::filesystem::create_directories(dataPath, errc);
        }
    }
    return true;
}

void ClemensStartupView::input(ClemensInputEvent) {}

void ClemensStartupView::lostFocus() {}
