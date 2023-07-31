#include "clem_startup_view.hpp"
#include "clem_configuration.hpp"
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

#if defined(_MSC_VER) && defined(WIN32)
    #define NOMINMAX
    #include <Windows.h>
    #include "spdlog/sinks/msvc_sink.h"
#endif

namespace {

void setupLogger(const ClemensConfiguration &config) {
    static spdlog::level::level_enum levels[] = {spdlog::level::debug, spdlog::level::info,
                                                 spdlog::level::warn, spdlog::level::err,
                                                 spdlog::level::err};



    auto logLevel = levels[std::clamp(config.logLevel, 0, CLEM_DEBUG_LOG_FATAL)];
    std::shared_ptr<spdlog::sinks::sink> console_sink;
#if defined(_MSC_VER)
    if (IsDebuggerPresent()) {
        console_sink = std::make_shared<spdlog::sinks::msvc_sink_mt>();
    } else {
        console_sink = std::make_shared<spdlog::sinks::stdout_color_sink_mt>();
    }
#else
    console_sink = std::make_shared<spdlog::sinks::stdout_color_sink_mt>();
#endif
    console_sink->set_level(logLevel);

    auto logPath = std::filesystem::path(config.dataDirectory) / "clem_host.log";
    auto file_sink = std::make_shared<spdlog::sinks::basic_file_sink_mt>(logPath.string(), true);
    file_sink->set_level(logLevel);

    std::vector<spdlog::sink_ptr> sinks{console_sink, file_sink};
    auto main_logger = std::make_shared<spdlog::logger>("clemens", sinks.begin(), sinks.end());
    spdlog::set_default_logger(main_logger);
    spdlog::set_level(logLevel);
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
ClemensStartupView::ClemensStartupView(ClemensConfiguration& config) : mode_(Mode::Initial), config_(config) {}

auto ClemensStartupView::frame(int width, int height, double /*deltaTime */,
                               ClemensHostInterop &interop) -> ViewType {
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
                setupLogger(config_);
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

void ClemensStartupView::input(ClemensInputEvent) {}

void ClemensStartupView::pasteText(const char *, unsigned) {}

void ClemensStartupView::lostFocus() {}

void ClemensStartupView::gainFocus() {}
