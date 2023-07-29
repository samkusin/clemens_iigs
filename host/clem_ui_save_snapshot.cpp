#include "clem_ui_save_snapshot.hpp"
#include "clem_command_queue.hpp"

#include "clem_backend.hpp"
#include "clem_display.hpp"
#include "clem_imgui.hpp"
#include "fmt/format.h"
#include "imgui.h"

#include "miniz.h"

#include <filesystem>

bool ClemensSaveSnapshotUI::isStarted() const { return mode_ != Mode::None; }

void ClemensSaveSnapshotUI::start(ClemensCommandQueue &backend, bool isEmulatorRunning) {
    mode_ = Mode::PromptForName;
    interruptedExecution_ = isEmulatorRunning;
    snapshotName_[0] = '\0';
    backend.breakExecution();
}

bool ClemensSaveSnapshotUI::frame(float width, float /* height */, ClemensDisplay &display,
                                  ClemensCommandQueue &backend) {
    ImVec2 center = ImGui::GetMainViewport()->GetCenter();
    switch (mode_) {
    case Mode::None:
        break;
    case Mode::PromptForName: {
        if (!ImGui::IsPopupOpen("Save Snapshot")) {
            ImGui::OpenPopup("Save Snapshot");
        }
        ImGui::SetNextWindowPos(center, ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
        ImGui::SetNextWindowSize(
            ImVec2(std::max(720.0f, width * 0.33f), 7 * ImGui::GetTextLineHeightWithSpacing()));
        if (ImGui::BeginPopupModal("Save Snapshot", NULL, ImGuiWindowFlags_Modal)) {
            bool isOk = false;
            ImGui::Spacing();
            ImGui::Text("Enter the name of this snapshot.");
            ImGui::SetNextItemWidth(ImGui::GetWindowContentRegionWidth() -
                                    ImGui::GetStyle().WindowPadding.x);
            ImGui::SetItemDefaultFocus();
            if (ImGui::InputText("##", snapshotName_, sizeof(snapshotName_),
                                 ImGuiInputTextFlags_EnterReturnsTrue)) {
                isOk = true;
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
                auto selectedPath = std::filesystem::path(snapshotName_);
                if (!selectedPath.has_extension()) {
                    selectedPath.replace_extension(".clemens-sav");
                }
                int screenWidth, screenHeight;
                auto screenData = display.capture(&screenWidth, &screenHeight);
                size_t imageDataLen = 0;

                //  now compress to PNG
                void *imageData = tdefl_write_image_to_png_file_in_memory_ex(
                    screenData.data(), screenWidth, screenHeight, 4, &imageDataLen,
                    MZ_DEFAULT_LEVEL, display.shouldFlipTarget());
                auto pngData = std::make_unique<ClemensCommandMinizPNG>(imageData, imageDataLen,
                                                                        screenWidth, screenHeight);
                backend.saveMachine(selectedPath.string(), std::move(pngData));
                mode_ = Mode::WaitForResponse;
            }
            ImGui::EndPopup();
        }
    } break;
    case Mode::WaitForResponse:
        break;
    case Mode::Succeeded: {
        bool done = false;
        if (!ImGui::IsPopupOpen("Save Completed")) {
            ImGui::OpenPopup("Save Completed");
        }
        if (ImGui::BeginPopupModal("Save Completed", NULL, ImGuiWindowFlags_Modal)) {
            ImGui::Spacing();
            ImGui::Text("Snapshot '%s' finished.", snapshotName_);
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
        if (!ImGui::IsPopupOpen("Save Failed")) {
            ImGui::OpenPopup("Save Failed");
        }
        if (ImGui::BeginPopupModal("Save Failed", NULL, ImGuiWindowFlags_Modal)) {
            ImGui::Spacing();
            ImGui::Text("Snapshot '%s' failed to save.", snapshotName_);
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

void ClemensSaveSnapshotUI::stop(ClemensCommandQueue &backend) {
    if (interruptedExecution_) {
        backend.run();
    }
    mode_ = Mode::None;
}

void ClemensSaveSnapshotUI::fail() { mode_ = Mode::Failed; }

void ClemensSaveSnapshotUI::succeeded() { mode_ = Mode::Succeeded; }
