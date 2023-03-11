#include "clem_ui_disk_unit.hpp"

#include "clem_command_queue.hpp"
#include "clem_disk.h"
#include "clem_disk_library.hpp"
#include "clem_host_platform.h"
#include "clem_host_shared.hpp"

#include "imgui.h"
#include "imgui_filedialog/ImGuiFileDialog.h"

ClemensDiskUnitUI::ClemensDiskUnitUI(ClemensDiskLibrary &diskLibrary,
                                     ClemensDriveType diskDriveType)
    : diskLibrary_(diskLibrary), diskDriveType_(diskDriveType), mode_(Mode::None),
      generatingDiskList_(false) {

    if (diskDriveType_ == kClemensDrive_3_5_D1 || diskDriveType_ == kClemensDrive_3_5_D2) {
        diskDriveCategoryType_ = CLEM_DISK_TYPE_3_5;
    } else if (diskDriveType_ == kClemensDrive_5_25_D1 || diskDriveType_ == kClemensDrive_5_25_D2) {
        diskDriveCategoryType_ = CLEM_DISK_TYPE_5_25;
    } else {
        diskDriveCategoryType_ = CLEM_DISK_TYPE_NONE;
    }
}

bool ClemensDiskUnitUI::frame(float width, float height, ClemensCommandQueue &backendQueue,
                              const ClemensBackendDiskDriveState &diskDrive, const char *driveName,
                              bool showLabel) {
    // ALWAYS RENDER THE SELECTOR as it is part of the main GUI.
    // The other modes will place ImGui in its Modal mode and will redirect input
    // to the various import flows.

    //  2 states: empty, has disk
    //    options if empty: <blank disk>, <import image>, image 0, image 1, ...
    //    options if full: <eject>
    char tempPath[CLEMENS_PATH_MAX];
    if (diskDrive.isEjecting) {
        strncpy(tempPath, "Ejecting...", sizeof(tempPath) - 1);
    } else if (diskDrive.imagePath.empty()) {
        strncpy(tempPath, "* No Disk *", sizeof(tempPath) - 1);
    } else {
        strncpy(tempPath, std::filesystem::path(diskDrive.imagePath).stem().string().c_str(),
                sizeof(tempPath) - 1);
    }
    tempPath[sizeof(tempPath) - 1] = '\0';

    char label[32];
    snprintf(label, sizeof(label) - 1, "%s%s", !showLabel ? "##" : "", driveName);
    if (!showLabel) {
        //  enlarge the combo-box to account for the blank label space.
        ImGui::PushItemWidth(width);
    }
    if (ImGui::BeginCombo(label, tempPath,
                          ImGuiComboFlags_NoArrowButton | ImGuiComboFlags_HeightLarge)) {
        if (!generatingDiskList_) {
            diskLibrary_.reset(diskDriveCategoryType_);
            generatingDiskList_ = true;
        } else {
            diskLibrary_.update();
        }
        if (!diskDrive.imagePath.empty() && !diskDrive.isEjecting && ImGui::Selectable("<eject>")) {
            backendQueue.ejectDisk(diskDriveType_);
        }
        if (diskDrive.imagePath.empty()) {
            if (ImGui::Selectable("<insert blank disk>")) {
                mode_ = Mode::InsertBlankDisk;
            }
            if (ImGui::Selectable("<import master>")) {
                importDiskFiles_.clear();
                mode_ = Mode::ImportDisks;
            }
            ImGui::Separator();
            std::filesystem::path selectedPath;
            diskLibrary_.iterate([&selectedPath](const ClemensDiskLibrary::DiskEntry &entry) {
                auto relativePath = entry.location.parent_path().filename() / entry.location.stem();
                if (ImGui::Selectable(relativePath.string().c_str())) {
                    selectedPath =
                        entry.location.parent_path().filename() / entry.location.filename();
                }
            });
            if (!selectedPath.empty()) {
                backendQueue.insertDisk(diskDriveType_, selectedPath.string());
            }
            ImGui::Separator();
        }
        ImGui::EndCombo();
    } else {
        generatingDiskList_ = false;
    }

    const ImVec2 &viewportSize = ImGui::GetMainViewport()->Size;

    switch (mode_) {
    case Mode::InsertBlankDisk:
        doBlankDiskFlow(viewportSize.x, viewportSize.y);
        break;

    case Mode::ImportDisks:
        doImportDiskFlow(viewportSize.x, viewportSize.y);
        break;

    case Mode::CreateDiskSet:
        break;

    case Mode::FinishImportDisks:
        // TODO
        break;

    case Mode::None:
        break;
    }

    return true;
}

void ClemensDiskUnitUI::doImportDiskFlow(float width, float height) {
    const ImVec2 dialogSize(std::max(800.0f, width * 0.80f), std::max(600.0f, width * 0.60f));

    if (!ImGuiFileDialog::Instance()->IsOpened("choose_disk_images")) {
        const char *filters =
            "Disk image files (*.dsk *.do *.po *.2mg *.woz){.dsk,.do,.po,.2mg,.woz}";
        ImGuiFileDialog::Instance()->OpenDialog("choose_disk_images", "Choose Disk Image", filters,
                                                ".", "", 16, (void *)((intptr_t)diskDriveType_),
                                                ImGuiFileDialogFlags_Modal);
    }
    if (ImGuiFileDialog::Instance()->Display("choose_disk_images", ImGuiWindowFlags_NoCollapse,
                                             dialogSize, ImVec2(width, height))) {
        if (ImGuiFileDialog::Instance()->IsOk()) {
            auto selection = ImGuiFileDialog::Instance()->GetSelection();
            auto filepath = ImGuiFileDialog::Instance()->GetCurrentPath();
            for (auto &e : selection) {
                importDiskFiles_.emplace_back(e.second);
            }
        }

        ImGuiFileDialog::Instance()->Close();
    }
    if (importDiskFiles_.empty()) {
        mode_ = Mode::Exit;
        return;
    }
    //  display a list of all folders and an option to create a new
    //  folder within the images main folder
    if (!ImGui::IsPopupOpen("Select Destination")) {
        ImGui::OpenPopup("Select Destination");
        selectedEntry_ = "";
        diskNameEntry_[0] = '\0';
    }
    ImVec2 center = ImGui::GetMainViewport()->GetCenter();
    ImGui::SetNextWindowPos(center, ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
    ImGui::SetNextWindowSize(dialogSize);
    if (ImGui::BeginPopupModal("Select Destination")) {
        float footerSize = 3 * ImGui::GetFrameHeightWithSpacing();
        ImVec2 listSize(-FLT_MIN, ImGui::GetWindowHeight() - footerSize);
        bool isOk = false;
        if (ImGui::BeginListBox("##DestinationList", listSize)) {
            auto &rootPath = diskLibrary_.getLibraryRootPath();
            for (auto const &entry : std::filesystem::directory_iterator(rootPath)) {
                if (!entry.is_directory())
                    continue;

                auto filename = entry.path().parent_path();
                bool isSelected =
                    ImGui::Selectable(filename.string().c_str(), filename == selectedEntry_,
                                      ImGuiSelectableFlags_AllowDoubleClick);
                if (!isOk && isSelected) {
                    if (ImGui::IsItemHovered() &&
                        ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left)) {
                        isOk = true;
                    }
                }
                ImGui::Separator();
            }
            ImGui::EndListBox();
        }
        if (ImGui::Button("Create Directory")) {
        }
        ImGui::SameLine();
        if (ImGui::InputText("##DiskSetName", diskNameEntry_, sizeof(diskNameEntry_),
                             ImGuiInputTextFlags_EnterReturnsTrue)) {
            mode_ = Mode::CreateDiskSet;
            ImGui::CloseCurrentPopup();
        }
        ImGui::Separator();
        if (ImGui::Button("Ok") ||
            (ImGui::IsKeyPressed(ImGuiKey_Enter) && mode_ != Mode::CreateDiskSet)) {
            isOk = true;
        }
        ImGui::SameLine();
        if (ImGui::Button("Cancel") && !isOk) {
            ImGui::CloseCurrentPopup();
            mode_ = Mode::Exit;
        }
        if (isOk && selectedEntry_[0] != '\0') {
            mode_ = Mode::FinishImportDisks;
            ImGui::CloseCurrentPopup();
        }
        ImGui::EndPopup();
    }
}

void ClemensDiskUnitUI::doBlankDiskFlow(float width, float height) {
    if (!ImGui::IsPopupOpen("Enter Disk Set Name")) {
        ImGui::OpenPopup("Enter Disk Set Name");
    }
}
