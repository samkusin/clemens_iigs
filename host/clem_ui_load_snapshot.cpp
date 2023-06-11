#include "clem_ui_load_snapshot.hpp"
#include "clem_command_queue.hpp"

#include "clem_backend.hpp"
#include "clem_imgui.hpp"
#include "fmt/format.h"
#include "imgui.h"
#include "spdlog/spdlog.h"

#include "clem_l10n.hpp"

#include <filesystem>

bool ClemensLoadSnapshotUI::isStarted() const { return mode_ != Mode::None; }

void ClemensLoadSnapshotUI::start(ClemensCommandQueue &backend, const std::string &snapshotDir) {
    mode_ = Mode::Browser;
    snapshotDir_ = snapshotDir;
    snapshotName_[0] = '\0';
    backend.breakExecution();
    resumeExecutionOnExit_ = true;
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
            ImGui::BeginDisabled(snapshotName_[0] == '\0');
            if (ImGui::Button(CLEM_L10N_OK_LABEL) || ImGui::IsKeyPressed(ImGuiKey_Enter)) {
                isOk = true;
            }
            ImGui::EndDisabled();
            ImGui::SameLine();
            if (ImGui::Button(CLEM_L10N_CANCEL_LABEL) && !isOk) {
                ImGui::CloseCurrentPopup();
                mode_ = Mode::Cancelled;
            }
            ImGui::SameLine();
            ImGui::BeginDisabled(snapshotName_[0] == '\0');
            if (ImGui::Button(CLEM_L10N_LABEL(kLabelDelete))) {
                //  open a delete prompt
                ImGui::OpenPopup(CLEM_L10N_LABEL(kModalDeleteSnapshot));
            }
            ImGui::EndDisabled();
            if (isOk && snapshotName_[0] != '\0') {
                ImGui::CloseCurrentPopup();
                backend.loadMachine(snapshotName_);
                spdlog::info("ClemensLoadSnapshotUI - loading...");
                mode_ = Mode::WaitForResponse;
            }
            bool deleteError = false;
            if (ImGui::BeginPopupModal(CLEM_L10N_LABEL(kModalDeleteSnapshot), NULL,
                                       ImGuiWindowFlags_AlwaysAutoResize)) {
                ImGui::Spacing();
                ImGui::Text(CLEM_L10N_LABEL(kLabelDeleteConfirm), snapshotName_);
                ImGui::Spacing();

                ImGui::Separator();
                if (ImGui::Button(CLEM_L10N_LABEL(kLabelDelete))) {
                    std::error_code errc{};
                    auto snapshotPath = std::filesystem::path(snapshotDir_) / snapshotName_;
                    if (!std::filesystem::remove(snapshotPath, errc)) {
                        spdlog::error("Unable to delete snapshot {} (error={})",
                                      snapshotPath.string(), errc.message());
                        deleteError = true;
                    }
                    snapshotName_[0] = '\0';
                    ImGui::CloseCurrentPopup();
                }
                ImGui::SameLine();
                if (ImGui::Button(CLEM_L10N_CANCEL_LABEL)) {
                    ImGui::CloseCurrentPopup();
                }
                ImGui::EndPopup();
            }
            if (deleteError) {
                ImGui::OpenPopup("Error");
            }
            if (ImGui::BeginPopupModal("Error", NULL, ImGuiWindowFlags_AlwaysAutoResize)) {
                ImGui::Spacing();
                ImGui::Text(CLEM_L10N_LABEL(kLabelDeleteFailed));
                ImGui::Spacing();
                ImGui::Separator();
                if (ImGui::Button(CLEM_L10N_OK_LABEL)) {
                    ImGui::CloseCurrentPopup();
                }
                ImGui::EndPopup();
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
            ImGui::SameLine();
            if (ImGui::Button("Break")) {
                ImGui::CloseCurrentPopup();
                resumeExecutionOnExit_ = false;
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
    if (resumeExecutionOnExit_) {
        backend.run();
    }
    mode_ = Mode::None;
}

void ClemensLoadSnapshotUI::fail() { mode_ = Mode::Failed; }

void ClemensLoadSnapshotUI::succeeded() { mode_ = Mode::Succeeded; }
