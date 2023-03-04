#include "clem_ui_load_snapshot.hpp"
#include "clem_command_queue.hpp"

#include "clem_backend.hpp"
#include "clem_imgui.hpp"
#include "fmt/format.h"
#include "imgui.h"

#include <filesystem>

bool ClemensLoadSnapshotUI::isStarted() const { return mode_ != Mode::None; }

void ClemensLoadSnapshotUI::start(ClemensCommandQueue &backend, const std::string &snapshotDir,
                                  bool isEmulatorRunning) {
    mode_ = Mode::Browser;
    interruptedExecution_ = isEmulatorRunning;
    snapshotDir_ = snapshotDir;
    snapshotName_[0] = '\0';
    backend.breakExecution();
}

bool ClemensLoadSnapshotUI::frame(float width, float height, ClemensCommandQueue &backend) {
    ImVec2 center = ImGui::GetMainViewport()->GetCenter();
    switch (mode_) {
    case Mode::None:
        break;
    case Mode::Browser: {
        if (!ImGui::IsPopupOpen("Load Snapshot")) {
            ImGui::OpenPopup("Load Snapshot");
        }
        ImGui::SetNextWindowPos(center, ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
        ImGui::SetNextWindowSize(
            ImVec2(std::max(720.0f, width * 0.66f), std::max(512.0f, height * 0.66f)));
        if (ImGui::BeginPopupModal("Load Snapshot", NULL, ImGuiWindowFlags_Modal)) {
            //  A custom listbox
            bool isOk = false;
            ImGui::Spacing();
            ImVec2 cursorPos = ImGui::GetCursorPos();
            // account for bottom separator plus one row of buttons
            ImVec2 listSize(-FLT_MIN, 6 * (ImGui::GetStyle().FrameBorderSize +
                                           ImGui::GetStyle().FramePadding.y) +
                                          ImGui::GetTextLineHeightWithSpacing());
            listSize.y = ImGui::GetWindowHeight() - listSize.y - cursorPos.y;
            if (ImGui::BeginListBox("##SnapshotList", listSize)) {
                for (auto const &entry : std::filesystem::directory_iterator(snapshotDir_)) {
                    auto filename = entry.path().filename().string();
                    bool isSelected = ImGui::Selectable(filename.c_str(), filename == snapshotName_,
                                                        ImGuiSelectableFlags_AllowDoubleClick);
                    if (!isOk && isSelected) {
                        auto cnt = filename.copy(snapshotName_, sizeof(snapshotName_) - 1);
                        snapshotName_[cnt] = '\0';
                        if (ImGui::IsItemHovered() &&
                            ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left)) {
                            isOk = true;
                        }
                    }
                    ImGui::Separator();
                }
                ImGui::EndListBox();
            }
            ImGui::Separator();
            if (ImGui::Button("Ok") || ImGui::IsKeyPressed(ImGuiKey_Enter)) {
                isOk = true;
            }
            ImGui::SameLine();
            if (ImGui::Button("Cancel") && !isOk) {
                ImGui::CloseCurrentPopup();
                mode_ = Mode::Cancelled;
            }
            if (isOk && snapshotName_[0] != '\0') {
                ImGui::CloseCurrentPopup();

                backend.loadMachine(snapshotName_);
                fmt::print("LoadSnapshotMode: loading...\n");
                mode_ = Mode::WaitForResponse;
            }
            ImGui::EndPopup();
        }
    } break;
    case Mode::WaitForResponse:
        break;
    case Mode::Succeeded: {
        bool done = false;
        if (!ImGui::IsPopupOpen("Load Completed")) {
            ImGui::OpenPopup("Load Completed");
        }
        if (ImGui::BeginPopupModal("Load Completed", NULL, ImGuiWindowFlags_Modal)) {
            ImGui::Spacing();
            ImGui::Text("Snapshot '%s' loaded.", snapshotName_);
            ImGui::Separator();
            if (ImGui::Button("Ok") || ImGui::IsKeyPressed(ImGuiKey_Enter)) {
                ImGui::CloseCurrentPopup();
                done = true;
            }
            ImGui::EndPopup();
        }
        return done;
    } break;
    case Mode::Failed: {
        bool done = false;
        if (!ImGui::IsPopupOpen("Load Failed")) {
            ImGui::OpenPopup("Load Failed");
        }
        if (ImGui::BeginPopupModal("Load Failed", NULL, ImGuiWindowFlags_Modal)) {
            ImGui::Spacing();
            ImGui::Text("Failed to load snapshot '%s'.", snapshotName_);
            ImGui::Separator();
            if (ImGui::Button("Ok") || ImGui::IsKeyPressed(ImGuiKey_Enter)) {
                ImGui::CloseCurrentPopup();
                done = true;
            }
            ImGui::EndPopup();
        }
        return done;
    } break;
    case Mode::Cancelled:
        return true;
    }
    return false;
}

void ClemensLoadSnapshotUI::stop(ClemensCommandQueue &backend) {
    if (interruptedExecution_) {
        backend.run();
    }
    mode_ = Mode::None;
}

void ClemensLoadSnapshotUI::fail() { mode_ = Mode::Failed; }

void ClemensLoadSnapshotUI::succeeded() { mode_ = Mode::Succeeded; }
