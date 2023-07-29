#include "clem_ui_load_snapshot.hpp"
#include "clem_command_queue.hpp"

#include "clem_assets.hpp"
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
    refresh();
}

void ClemensLoadSnapshotUI::refresh() {
    snapshotNames_.clear();
    snapshotMetadatas_.clear();
    for (auto const &entry : std::filesystem::directory_iterator(snapshotDir_)) {
        auto filename = entry.path().filename();
        auto extension = filename.extension().string();
        if (extension != ".clemens-sav")
            continue;
        snapshotNames_.emplace_back(std::move(filename.stem().string()));

        ClemensSnapshot snapshot(entry.path().string());
        auto metadata = snapshot.unserializeMetadata();
        if (metadata.second) {
            snapshotMetadatas_.emplace_back(std::move(metadata.first));
        } else {
            snapshotMetadatas_.emplace_back();
        }
    }
}

void ClemensLoadSnapshotUI::loadSnapshotImage(unsigned snapshotIndex) {
    if (snapshotImage_) {
        ClemensHostAssets::freeLoadedImage(snapshotImage_);
        snapshotImage_ = 0;
    }
    auto &metadata = snapshotMetadatas_[snapshotIndex];
    snapshotImage_ =
        ClemensHostAssets::loadImageFromPNG(metadata.imageData.data(), metadata.imageData.size(),
                                            &snapshotImageWidth_, &snapshotImageHeight_);
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
        ImGui::SetNextWindowSize(ImVec2(width * 0.75f, height * 0.66f));
        if (ImGui::BeginPopupModal("Load Snapshot", NULL,
                                   ImGuiWindowFlags_Modal | ImGuiWindowFlags_AlwaysAutoResize)) {
            //  A custom listbox
            bool isOk = false;
            ImGui::Spacing();
            // account for bottom separator plus one row of buttons
            ImVec2 contentRegion = ImGui::GetContentRegionAvail();
            ImVec2 footerRegion(contentRegion.x, ImGui::GetFrameHeightWithSpacing() * 1.5f);
            ImVec2 listSize(contentRegion.x * 0.66f, contentRegion.y - footerRegion.y);
            snapshotIndex_ = 0;
            if (ImGui::BeginListBox("##SnapshotList", listSize)) {
                for (size_t nameIndex = 0; nameIndex < snapshotNames_.size(); ++nameIndex) {
                    auto &filename = snapshotNames_[nameIndex];
                    if (filename == snapshotName_) {
                        snapshotIndex_ = nameIndex;
                    }
                    bool isSelected = ImGui::Selectable(filename.c_str(), filename == snapshotName_,
                                                        ImGuiSelectableFlags_AllowDoubleClick);
                    if (!isOk && isSelected) {
                        auto cnt = filename.copy(snapshotName_, sizeof(snapshotName_) - 1);
                        loadSnapshotImage(nameIndex);
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
            ImGui::SameLine();
            ImGui::BeginChild("##SnapshotDetail",
                              ImVec2(ImGui::GetContentRegionAvail().x, listSize.y));
            contentRegion = ImGui::GetContentRegionAvail();
            if (snapshotImage_ != 0) {
                auto &metadata = snapshotMetadatas_[snapshotIndex_];
                float imageAspect = snapshotImageHeight_ / (float)snapshotImageWidth_;
                float imageWidth = contentRegion.x;
                ImGui::Image((ImTextureID)snapshotImage_,
                             ImVec2(imageWidth, imageWidth * imageAspect));
                ImGui::LabelText("Memory", "%u", snapshotMetadatas_[snap])
            }
            ImGui::EndChild();
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
                backend.loadMachine(fmt::format("{}.clemens-sav", snapshotName_));
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
                    auto snapshotFilename = fmt::format("{}.clemens-sav", snapshotName_);
                    auto snapshotPath = std::filesystem::path(snapshotDir_) / snapshotFilename;
                    if (!std::filesystem::remove(snapshotPath, errc)) {
                        spdlog::error("Unable to delete snapshot {} (error={})",
                                      snapshotPath.string(), errc.message());
                        deleteError = true;
                    }
                    snapshotName_[0] = '\0';
                    refresh();
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
                ImGui::TextUnformatted(CLEM_L10N_LABEL(kLabelDeleteFailed));
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
    if (snapshotImage_ != 0) {
        ClemensHostAssets::freeLoadedImage(snapshotImage_);
        snapshotImage_ = 0;
    }
    snapshotNames_.clear();
    snapshotMetadatas_.clear();
}

void ClemensLoadSnapshotUI::fail() { mode_ = Mode::Failed; }

void ClemensLoadSnapshotUI::succeeded() { mode_ = Mode::Succeeded; }
