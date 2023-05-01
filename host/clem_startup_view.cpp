#include "clem_startup_view.hpp"
#include "clem_host_platform.h"
#include "clem_imgui.hpp"
#include "clem_preamble.hpp"

#include "fmt/core.h"
#include "fmt/format.h"
#include "imgui.h"
#include "spdlog/common.h"
#include "spdlog/sinks/basic_file_sink.h"
#include "spdlog/sinks/stdout_color_sinks.h"
#include "spdlog/spdlog.h"

#include "imgui_filedialog/ImGuiFileDialog.h"

#include <filesystem>
#include <memory>
#include <system_error>

namespace {

//  For all platforms, the config file is guaranteed to be located in a
//  predefined location.  The config file is effectively our 'registry' to
//  use windows terminology.
//
ClemensConfiguration createConfiguration(const std::filesystem::path defaultDataDirectory) {
    auto configPath = defaultDataDirectory / "config.ini";
    spdlog::info("Configuration created at {}", defaultDataDirectory.string());
    return ClemensConfiguration(configPath.string(), defaultDataDirectory.string());
}

ClemensConfiguration findConfiguration() {
    //  find the config file on the system

    //  local directory configuration check
    char localpath[CLEMENS_PATH_MAX];
    size_t localpathSize = sizeof(localpath);
    if (get_process_executable_path(localpath, &localpathSize)) {
        if (strnlen(localpath, CLEMENS_PATH_MAX - 1) >= sizeof(localpath) - 1) {
            //  If this is a problem, later code will determine whether the path
            //  was actually truncated
            spdlog::warn("Discovered configuration pathname is likely truncated!");
        }
        auto dataDirectory = std::filesystem::path(localpath).parent_path();
        auto configPath = dataDirectory / "config.ini";
        spdlog::info("Checking for configuration in {}", configPath.string());
        if (std::filesystem::exists(configPath)) {
            return createConfiguration(dataDirectory);
        }
    } else {
        //  TODO: handle systems that support dynamic path sizes (i.e. localpathSize !=
        //  sizeof(localpath))
        spdlog::warn("Unable to obtain our local executable path. Falling back to user data paths");
    }
    if (!get_local_user_data_directory(localpath, sizeof(localpath), CLEM_HOST_COMPANY_NAME,
                                       CLEM_HOST_APPLICATION_NAME)) {
        spdlog::error("Unable to obtain the OS specific user data directory.");
        return ClemensConfiguration();
    }
    return createConfiguration(std::filesystem::path(localpath));
}

void setupLogger(const std::string &logDirectory) {
    auto console_sink = std::make_shared<spdlog::sinks::stdout_color_sink_mt>();
    console_sink->set_level(spdlog::level::info);

    auto logPath = std::filesystem::path(logDirectory) / "clem_host.log";
    auto file_sink = std::make_shared<spdlog::sinks::basic_file_sink_mt>(logPath.string(), true);
    file_sink->set_level(spdlog::level::info);

    std::vector<spdlog::sink_ptr> sinks{console_sink, file_sink};
    auto main_logger = std::make_shared<spdlog::logger>("clemens", sinks.begin(), sinks.end());
    spdlog::set_default_logger(main_logger);
    spdlog::info("Log file at {}", logPath.string());
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
ClemensStartupView::ClemensStartupView() : mode_(Mode::Initial), config_(findConfiguration()) {}

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
                setupLogger(config_.dataDirectory);
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
                    mode_ = Mode::ROMCheck;
                } else if (preambleResult == ClemensPreamble::Result::Exit) {
                    preamble_ = nullptr;
                    mode_ = Mode::Aborted;
                }
            }
        }
        break;

    case Mode::ROMCheck:
        //  if no rom file exists, ask the user to select one, and copy that
        //  to our data directory root
        if (!ImGui::IsPopupOpen("Select ROM Image") && !ImGui::IsPopupOpen("ROM File Not Found")) {
            int checkROMResult = checkROMFile();
            if (checkROMResult == 0) {
                ImGui::OpenPopup("Select ROM Image");
            } else if (checkROMResult < 0) {
                ImGui::OpenPopup("ROM File Not Found");
            } else {
                mode_ = Mode::Finished;
            }

        } else {
            ImVec2 center = ImGui::GetMainViewport()->GetCenter();
            ImGui::SetNextWindowPos(center, 0, ImVec2(0.5f, 0.5f));
            ImGui::SetNextWindowSize(ImVec2(width * 0.75f, 0.0f));
            if (ImGui::BeginPopupModal("Select ROM Image", NULL,
                                       ImGuiWindowFlags_AlwaysAutoResize)) {
                ImGui::PushTextWrapPos();
                ImGui::NewLine();
                ImGui::TextUnformatted("Clemens IIGS requires a ROM 3 binary to run properly.  "
                                       "Press 'OK' to select a ROM binary file.");
                ImGui::NewLine();
                ImGui::TextUnformatted(
                    "Press 'Continue without ROM' to skip this step.  If this is your "
                    "choice, the emulated machine will hang at startup.");
                ImGui::NewLine();
                ImGui::Separator();
                ImGui::PopTextWrapPos();
                if (ImGui::Button("OK", ImVec2(240, 0))) {
                    mode_ = Mode::ROMFileBrowser;
                    ImGui::CloseCurrentPopup();
                }
                ImGui::SameLine();
                if (ImGui::Button("Continue without ROM", ImVec2(300, 0))) {
                    mode_ = Mode::Finished;
                    ImGui::CloseCurrentPopup();
                }
                ImGui::EndPopup();
            }
            ImGui::SetNextWindowPos(center, 0, ImVec2(0.5f, 0.5f));
            ImGui::SetNextWindowSize(ImVec2(width * 0.75f, 0.0f));
            if (ImGui::BeginPopupModal("ROM File Not Found", NULL,
                                       ImGuiWindowFlags_AlwaysAutoResize)) {
                ImGui::PushTextWrapPos();
                ImGui::NewLine();
                ImGui::Text("The file '%s' could not be found or opened.  Press 'OK' to select a "
                            "ROM binary file.",
                            config_.romFilename.c_str());
                ImGui::NewLine();
                ImGui::TextUnformatted(
                    "Press 'Continue without ROM' to skip this step.  If this is your "
                    "choice, the emulated machine will hang at startup.");
                ImGui::NewLine();
                ImGui::Separator();
                ImGui::PopTextWrapPos();
                if (ImGui::Button("OK", ImVec2(240, 0))) {
                    mode_ = Mode::ROMFileBrowser;
                    ImGui::CloseCurrentPopup();
                    ImGuiFileDialog::Instance()->OpenDialog("Select ROM", "Select a ROM", ".*", ".",
                                                            1, nullptr, ImGuiFileDialogFlags_Modal);
                }
                ImGui::SameLine();
                if (ImGui::Button("Continue without ROM", ImVec2(300, 0))) {
                    mode_ = Mode::Finished;
                    ImGui::CloseCurrentPopup();
                }
                ImGui::EndPopup();
            }
        }
        break;

    case Mode::ROMFileBrowser: {
        auto result = ClemensHostImGui::ROMFileBrowser(width, height, config_.dataDirectory);
        if (result.type == ClemensHostImGui::ROMFileBrowserResult::Ok) {
            config_.romFilename = result.filename;
            mode_ = Mode::ROMCheck;
        } else if (result.type == ClemensHostImGui::ROMFileBrowserResult::Cancel) {
            mode_ = Mode::ROMCheck;
        } else if (result.type == ClemensHostImGui::ROMFileBrowserResult::Error) {
            setupError_ = fmt::format("Failed to copy ROM file {}", result.filename);
            mode_ = Mode::SetupError;
        }
        break;
    }

    case Mode::SetupError: {
        if (!ImGui::IsPopupOpen("Error")) {
            ImGui::OpenPopup("Error");
            spdlog::error("Startup failure - {}", setupError_);
        }

        ImVec2 center = ImGui::GetMainViewport()->GetCenter();
        ImGui::SetNextWindowPos(center, 0, ImVec2(0.5f, 0.5f));
        ImGui::SetNextWindowSize(ImVec2(width * 0.50f, 0.0f));
        if (ImGui::BeginPopupModal("Error", NULL, ImGuiWindowFlags_AlwaysAutoResize)) {
            ImGui::PushTextWrapPos();
            ImGui::NewLine();
            ImGui::TextUnformatted("There was a problem setting up the emulator.");
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
        if (config_.gs.memory < CLEM_EMULATOR_RAM_MINIMUM ||
            config_.gs.memory > CLEM_EMULATOR_RAM_MAXIMUM) {
            auto reconfiguredKB =
                std::clamp(config_.gs.memory, CLEM_EMULATOR_RAM_MINIMUM, CLEM_EMULATOR_RAM_MAXIMUM);
            config_.gs.memory = reconfiguredKB;
            spdlog::error("Configured emulator RAM of {}K is not supported.  Clamping to supported "
                          "value {}K\n",
                          config_.gs.memory, reconfiguredKB);
        }
        if ((config_.gs.memory % 64) != 0) {
            auto reconfiguredKB = (config_.gs.memory / 64) * 64;
            config_.gs.memory = reconfiguredKB;
            spdlog::error("Configured emulator RAM {}K must be bank-aligned, clamping to {}\n",
                          config_.gs.memory, reconfiguredKB);
        }
        config_.save();
        nextView = ViewType::Main;
        spdlog::info("Startup completed");
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

int ClemensStartupView::checkROMFile() {
    //  check for existence of rom file  <data_directory>/<config::romFilename>
    //  if ROM file exists, == 1, if no ROM file configured == 0,
    //  if filename points to a nonexistent ROM file, -1

    if (config_.romFilename.empty()) {
        //  if the ROM file past versions expect exists, configure that and retry.
        auto legacyROMPath = std::filesystem::path(config_.dataDirectory) / "gs_rom_3.rom";
        if (std::filesystem::exists(legacyROMPath)) {
            config_.romFilename = "gs_rom_3.rom";
            return checkROMFile();
        }
        return 0;
    }
    if (!std::filesystem::exists(std::filesystem::path(config_.dataDirectory) /
                                 config_.romFilename)) {
        return -1;
    }
    return 1;
}

void ClemensStartupView::input(ClemensInputEvent) {}

void ClemensStartupView::lostFocus() {}
