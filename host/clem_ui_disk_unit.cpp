#include "clem_ui_disk_unit.hpp"

#include "clem_command_queue.hpp"
#include "clem_disk_library.hpp"
#include "clem_host_platform.h"
#include "clem_host_shared.hpp"
#include "clem_import_disk.hpp"

#include "clem_disk.h"
#include "clem_woz.h"

#include "fmt/format.h"
#include "imgui.h"
#include "imgui_filedialog/ImGuiFileDialog.h"

namespace {

const ImVec2 guiDialogSizeLarge(float viewWidth, float viewHeight) {
    return ImVec2(std::max(800.0f, viewWidth * 0.80f), std::max(480.0f, viewHeight * 0.60f));
}

const ImVec2 guiDialogSizeMedium(float viewWidth, float viewHeight) {
    return ImVec2(std::max(640.0f, viewWidth * 0.60f), std::max(320.0f, viewHeight * 0.50f));
}

const ImVec2 guiDialogSizeSmall(float viewWidth, float viewHeight) {
    return ImVec2(std::max(640.0f, viewWidth * 0.50f), std::max(200.0f, viewHeight * 0.25f));
}

void positionMessageModal(ImVec2 size) {
    ImVec2 center = ImGui::GetMainViewport()->GetCenter();
    ImGui::SetNextWindowPos(center, ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
    ImGui::SetNextWindowSize(size);
}

static const char *sDriveDescription[] = {"3.5 inch 800K", "3.5 inch 800K", "5.25 inch 140K",
                                          "5.25 inch 140K"};

} // namespace

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

bool ClemensDiskUnitUI::frame(float width, float /* height */, ClemensCommandQueue &backendQueue,
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
                startFlow(Mode::InsertBlankDisk);
            }
            if (ImGui::Selectable("<import master>")) {
                startFlow(Mode::ImportDisks);
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

    //  all states can transition to Exit
    switch (mode_) {
    case Mode::InsertBlankDisk:
        //  output will be importDiskFiles_ being empty,
        //  transitions to CreateDiskSet or CreateBlankDisk
        doBlankDiskFlow(viewportSize.x, viewportSize.y);
        break;
    case Mode::ImportDisks:
        //  output will be importDiskFiles_ if not empty
        //  transitions to CreateDiskSet or FinishImportDisks
        doImportDiskFlow(viewportSize.x, viewportSize.y);
        break;
    case Mode::CreateBlankDisk:
        //  selectedDiskSetName_ will be the disk set name
        //  transition from InsertBlankDisk or CreateDiskSet
        //  transition to FinishCreateBlankDisk, and set importDiskFiles_[0] when done
        doCreateBlankDisk(viewportSize.x, viewportSize.y, backendQueue);
        break;
    case Mode::FinishImportDisks:
        //  transition from CreateDiskSet if importDiskFiles_ is not empty
        //  transition to Exit when done
        doFinishImportDisks(viewportSize.x, viewportSize.y);
        break;
    case Mode::Retry:
        //  this is a catchall state that relies on the retryMode_ (mode where
        //  retry was initiated.)  details in the doRetryFlow() method
        doRetryFlow(viewportSize.x, viewportSize.y, backendQueue);
        break;
    case Mode::Exit:
        //  final state, transition to None or display error
        doExit(viewportSize.x, viewportSize.y);
        break;
    case Mode::None:
        break;
    }

    return true;
}

void ClemensDiskUnitUI::startFlow(Mode mode) {
    mode_ = mode;
    retryMode_ = Mode::None;
    finishedMode_ = Mode::None;
    errorString_.clear();
    importDiskFiles_.clear();
    selectedDiskSetName_.clear();
    diskNameEntry_[0] = '\0';
}

void ClemensDiskUnitUI::retry() {
    retryMode_ = mode_;
    mode_ = Mode::Retry;
}

void ClemensDiskUnitUI::cancel() {
    finishedMode_ = mode_;
    mode_ = Mode::None;
}

void ClemensDiskUnitUI::finish(std::string errorString) {
    errorString_ = std::move(errorString);
    finishedMode_ = mode_;
    mode_ = Mode::Exit;
}

void ClemensDiskUnitUI::doImportDiskFlow(float width, float height) {
    if (importDiskFiles_.empty()) {
        if (!ImGuiFileDialog::Instance()->IsOpened("choose_disk_images")) {
            const char *filters =
                "Disk image files (*.dsk *.do *.po *.2mg *.woz){.dsk,.do,.po,.2mg,.woz}";
            ImGuiFileDialog::Instance()->OpenDialog(
                "choose_disk_images", "Choose Disk Image", filters, ".", "", 16,
                (void *)((intptr_t)diskDriveType_), ImGuiFileDialogFlags_Modal);
        }
        if (ImGuiFileDialog::Instance()->Display("choose_disk_images", ImGuiWindowFlags_NoCollapse,
                                                 guiDialogSizeLarge(width, height),
                                                 ImVec2(width, height))) {
            if (ImGuiFileDialog::Instance()->IsOk()) {
                auto selection = ImGuiFileDialog::Instance()->GetSelection();
                auto filepath = ImGuiFileDialog::Instance()->GetCurrentPath();
                for (auto &e : selection) {
                    importDiskFiles_.emplace_back(e.second);
                }
            }
            if (importDiskFiles_.empty()) {
                cancel();
                return;
            }
            ImGuiFileDialog::Instance()->Close();
        }
        return;
    }
    auto selectorResult = doDiskSetSelector(width, height);
    if (selectorResult == DiskSetSelectorResult::Ok) {
        mode_ = Mode::FinishImportDisks;
    } else if (selectorResult == DiskSetSelectorResult::Create) {
        createDiskSet();
    } else if (selectorResult == DiskSetSelectorResult::Cancel) {
        cancel();
    } else if (selectorResult == DiskSetSelectorResult::Retry) {
        retry();
    }
}

void ClemensDiskUnitUI::doBlankDiskFlow(float width, float height) {
    auto selectorResult = doDiskSetSelector(width, height);
    if (selectorResult == DiskSetSelectorResult::Ok) {
        //  checks rely on importDiskFiles being blank for creating blank disks
        importDiskFiles_.clear();
        mode_ = Mode::CreateBlankDisk;
    } else if (selectorResult == DiskSetSelectorResult::Create) {
        createDiskSet();
    } else if (selectorResult == DiskSetSelectorResult::Cancel) {
        cancel();
    } else if (selectorResult == DiskSetSelectorResult::Retry) {
        retry();
    }
}

auto ClemensDiskUnitUI::doDiskSetSelector(float width, float height) -> DiskSetSelectorResult {
    DiskSetSelectorResult result = DiskSetSelectorResult::None;
    if (!ImGui::IsPopupOpen("Select Destination")) {
        ImGui::OpenPopup("Select Destination");
        selectedDiskSetName_ = "";
        diskNameEntry_[0] = '\0';
    }
    positionMessageModal(guiDialogSizeMedium(width, height));
    if (ImGui::BeginPopupModal("Select Destination")) {
        float footerSize = 4 * ImGui::GetFrameHeightWithSpacing();
        ImVec2 listSize(-FLT_MIN, ImGui::GetWindowHeight() - footerSize);
        bool isOk = false;

        if (ImGui::BeginListBox("##DestinationList", listSize)) {
            diskLibrary_.iterateSets([&isOk, this](const ClemensDiskLibrary::DiskEntry &entry) {
                auto filename = entry.location.string();
                bool isSelected =
                    ImGui::Selectable(filename.c_str(), filename == selectedDiskSetName_,
                                      ImGuiSelectableFlags_AllowDoubleClick);
                if (isSelected || selectedDiskSetName_.empty()) {
                    selectedDiskSetName_ = filename;
                }
                if (!isOk && isSelected) {
                    if (ImGui::IsItemHovered() &&
                        ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left)) {
                        isOk = true;
                    }
                }
                ImGui::Separator();
            });
            ImGui::EndListBox();
        }
        if (ImGui::Button("Create Directory") && diskNameEntry_[0] != '\0') {
            result = DiskSetSelectorResult::Create;
        }
        ImGui::SameLine();
        if (ImGui::InputText("##DiskSetName", diskNameEntry_, sizeof(diskNameEntry_),
                             ImGuiInputTextFlags_EnterReturnsTrue)) {
            result = DiskSetSelectorResult::Create;
        }
        ImGui::Separator();
        ImGui::Spacing();
        if (ImGui::Button("Ok") || isOk ||
            (ImGui::IsKeyPressed(ImGuiKey_Enter) && result != DiskSetSelectorResult::Create)) {
            if (diskNameEntry_[0] != '\0') {
                result = DiskSetSelectorResult::Create;
            } else {
                result = DiskSetSelectorResult::Ok;
            }
        }
        ImGui::SameLine();
        if (ImGui::Button("Cancel")) {
            result = DiskSetSelectorResult::Cancel;
        }
        if (result != DiskSetSelectorResult::None) {
            // disk set requirement enforced here
            if (result != DiskSetSelectorResult::Cancel) {
                if (diskNameEntry_[0]) {
                    selectedDiskSetName_ = diskNameEntry_;
                }
                if (selectedDiskSetName_.empty()) {
                    result = DiskSetSelectorResult::Retry;
                }
            }
            ImGui::CloseCurrentPopup();
        }
        ImGui::EndPopup();
    }
    return result;
}

void ClemensDiskUnitUI::createDiskSet() {
    //  selectedDiskSetName_ will be the disk set name
    //  detect if the disk set already exists
    //      true: just continue to next state
    //      false: create it and exit if there's an error
    assert(!selectedDiskSetName_.empty());
    auto diskSetPath = diskLibrary_.getLibraryRootPath() / selectedDiskSetName_;
    if (!std::filesystem::exists(diskSetPath)) {
        std::error_code errc{};
        if (!std::filesystem::create_directory(diskSetPath, errc)) {
            finish(fmt::format("Unable to create disk set '{}'", selectedDiskSetName_));
            return;
        } else {
            fmt::print("Created directory '{}'\n", selectedDiskSetName_);
        }
    }
    if (importDiskFiles_.empty()) {
        mode_ = Mode::CreateBlankDisk;
    } else {
        mode_ = Mode::FinishImportDisks;
    }
}

void ClemensDiskUnitUI::doCreateBlankDisk(float width, float height, ClemensCommandQueue &backend) {
    //  bring up a name prompt
    //  on name selection peform the following:
    //      if the disk already exists, prompt the user and try again
    //      othewise create the disk
    //
    if (!ImGui::IsPopupOpen("Enter Disk Name")) {
        ImGui::OpenPopup("Enter Disk Name");
        diskNameEntry_[0] = '\0';
    }
    std::filesystem::path blankDiskPath;
    positionMessageModal(guiDialogSizeSmall(width, height));
    if (ImGui::BeginPopupModal("Enter Disk Name", NULL, 0)) {
        const float footerSize = 2 * ImGui::GetFrameHeightWithSpacing();
        const float footerY = ImGui::GetWindowHeight() - footerSize;
        ImGui::Spacing();
        bool isOk = false;
        //  TODO: seems the user has to enter text and press enter always to confirm
        //        adding an OK button seems that the right thing to do, but there's
        //        AFAIK no way to force InputText() to return true without some
        //       'next frame return true' hack flag.  Investigate but no need to
        //        waste cycles on figuring out this minor UI convenience for now.
        ImGui::BeginTable("Disk Label Entry", 2);
        ImGui::TableSetupColumn("", ImGuiTableColumnFlags_WidthFixed,
                                ImGui::CalcTextSize("Disk Name").x +
                                    ImGui::GetStyle().ColumnsMinSpacing);
        ImGui::TableSetupColumn("", ImGuiTableColumnFlags_WidthStretch);
        ImGui::TableNextRow();
        ImGui::TableNextColumn();
        ImGui::TextUnformatted("Disk Name");
        ImGui::TableNextColumn();
        isOk = ImGui::InputText("##2", diskNameEntry_, sizeof(diskNameEntry_),
                                ImGuiInputTextFlags_EnterReturnsTrue);
        ImGui::EndTable();
        ImGui::SetCursorPosY(footerY);
        ImGui::Separator();
        ImGui::Spacing();
        if (ImGui::Button("Ok") || isOk) {
            //  create the blank disk here
            blankDiskPath = diskLibrary_.getLibraryRootPath() / selectedDiskSetName_;
            blankDiskPath /= std::string(diskNameEntry_);
            ImGui::CloseCurrentPopup();
        }
        ImGui::SameLine();
        if (ImGui::Button("Cancel")) {
            finish();
            ImGui::CloseCurrentPopup();
        }
        ImGui::Spacing();
        ImGui::EndPopup();
    }
    if (!blankDiskPath.empty()) {
        blankDiskPath.replace_extension("woz");
        importDiskFiles_.push_back(blankDiskPath.string());
        if (std::filesystem::exists(blankDiskPath)) {
            retry();
            return;
        }
        createBlankDisk(backend);
        finish();
    }
}

void ClemensDiskUnitUI::createBlankDisk(ClemensCommandQueue &backendQueue) {
    auto diskPath = std::filesystem::path(importDiskFiles_[0]);
    diskPath.replace_extension("woz");
    backendQueue.insertBlankDisk(diskDriveType_, diskPath.string());
}

void ClemensDiskUnitUI::doFinishImportDisks(float /*width */, float /*height */) {
    //  TODO: schedule a job for importDisks() as it can for over a second if
    //        there are > 4 disks.  this is why I've kept this method as a
    //        state so GUI can be implemented when its needed
    auto diskSetPath = diskLibrary_.getLibraryRootPath() / selectedDiskSetName_;
    auto result = importDisks(diskSetPath.string());
    if (!result.second) {
        finish(std::move(result.first));
    } else {
        finish();
    }
    fmt::print("Finishing up dialog...\n");
}

std::pair<std::string, bool> ClemensDiskUnitUI::importDisks(const std::string &outputPath) {
    //  parse file extension for supported types:
    //    WOZ
    //    2MG
    //    DSK
    //    DO
    //    PO
    //  only succeed if each image works with the desired drive type
    ClemensDiskImporter importer(diskDriveType_, importDiskFiles_.size());
    for (auto &imagePath : importDiskFiles_) {
        ClemensWOZDisk *disk = importer.add(imagePath);
        if (!disk) {
            return std::make_pair(fmt::format("Failed to import disk image {} for drive format {}",
                                              imagePath, sDriveDescription[diskDriveType_]),
                                  false);
        } else {
            fmt::print("Adding disk image {} to import set.\n", imagePath);
        }
        switch (diskDriveType_) {
        case kClemensDrive_3_5_D1:
        case kClemensDrive_3_5_D2:
            if (disk->nib->disk_type != CLEM_DISK_TYPE_3_5) {
                return std::make_pair(fmt::format("Disk image {} with type 3.5 doesn't match "
                                                  "drive with required format {}",
                                                  imagePath, sDriveDescription[diskDriveType_]),
                                      false);
            }
            break;
        case kClemensDrive_5_25_D1:
        case kClemensDrive_5_25_D2:
            if (disk->nib->disk_type != CLEM_DISK_TYPE_5_25) {
                return std::make_pair(fmt::format("Disk image {} with type 5.25 doesn't match "
                                                  "drive with required format {}",
                                                  imagePath, sDriveDescription[diskDriveType_]),
                                      false);
            }
            break;
        default:
            break;
        }
    }
    if (!importer.build(outputPath)) {
        //  TODO: mare information please!
        return std::make_pair(fmt::format("Import build step failed for drive type {}",
                                          sDriveDescription[diskDriveType_]),
                              false);
    } else {
        fmt::print("Imported disk images to set '{}'\n", selectedDiskSetName_);
    }

    return std::make_pair(outputPath, true);
}

void ClemensDiskUnitUI::doRetryFlow(float width, float height, ClemensCommandQueue &backend) {
    if (!ImGui::IsPopupOpen("Retry")) {
        ImGui::OpenPopup("Retry");
    }
    positionMessageModal(guiDialogSizeSmall(width, height));
    if (ImGui::BeginPopupModal("Retry")) {
        auto cursorPos = ImGui::GetCursorPos();
        auto contentRegionAvail = ImGui::GetContentRegionAvail();
        ImGui::Spacing();
        ImGui::PushTextWrapPos(0.0f);
        switch (retryMode_) {
        case Mode::CreateBlankDisk:
            ImGui::TextUnformatted(importDiskFiles_.front().c_str());
            ImGui::TextUnformatted("already exists.");
            break;

        case Mode::ImportDisks:
        case Mode::InsertBlankDisk:
            ImGui::TextUnformatted("You must select or create a disk set when importing disks.");
            break;
        default:
            assert(false);
            break;
        }
        ImGui::PopTextWrapPos();
        ImGui::Spacing();
        ImGui::SetCursorPos(ImVec2(
            cursorPos.x, cursorPos.y + contentRegionAvail.y -
                             (ImGui::GetStyle().FramePadding.y * 2 + ImGui::GetTextLineHeight())));
        switch (retryMode_) {
        case Mode::CreateBlankDisk:
            if (ImGui::Button("Overwrite")) {
                createBlankDisk(backend);
                ImGui::CloseCurrentPopup();
                finish();
            }
            ImGui::SameLine();
            break;
        case Mode::ImportDisks:
        case Mode::InsertBlankDisk:
            break;
        default:
            assert(false);
            break;
        }
        if (ImGui::Button("Back")) {
            ImGui::CloseCurrentPopup();
            mode_ = retryMode_;
        }
        ImGui::SameLine();
        if (ImGui::Button("Cancel")) {
            ImGui::CloseCurrentPopup();
            if (retryMode_ == Mode::CreateBlankDisk) {
                finish("Could not create a blank disk.");
            } else {
                cancel();
            }
        }
        ImGui::EndPopup();
    }
}

void ClemensDiskUnitUI::doExit(float width, float height) {
    //  do error gui or just quit based on error mode
    if (!errorString_.empty()) {
        if (!ImGui::IsPopupOpen("Error")) {
            ImGui::OpenPopup("Error");
        }
        positionMessageModal(guiDialogSizeSmall(width, height));
        if (ImGui::BeginPopupModal("Error")) {
            ImGui::Spacing();
            ImGui::PushTextWrapPos(0.0f);
            ImGui::TextUnformatted(errorString_.c_str());
            ImGui::PopTextWrapPos();
            ImGui::Spacing();
            ImGui::Separator();
            if (ImGui::Button("Ok") || ImGui::IsKeyPressed(ImGuiKey_Enter)) {
                ImGui::CloseCurrentPopup();
                mode_ = Mode::None;
            }
            ImGui::EndPopup();
        }
    } else if (mode_ == Mode::Exit) {
        if (!ImGui::IsPopupOpen("Message")) {
            ImGui::OpenPopup("Message");
        }
        positionMessageModal(guiDialogSizeSmall(width, height));
        if (ImGui::BeginPopupModal("Message")) {
            const float footerSize = 2 * ImGui::GetFrameHeightWithSpacing();
            const float footerY = ImGui::GetWindowHeight() - footerSize;
            ImGui::Spacing();
            switch (finishedMode_) {
            case Mode::FinishImportDisks:
                ImGui::TextUnformatted(
                    fmt::format("Import disks into {} completed.", selectedDiskSetName_).c_str());
                break;
            default:
                ImGui::TextUnformatted("Operation completed.");
            }
            ImGui::SetCursorPosY(footerY);
            ImGui::Separator();
            ImGui::Spacing();
            if (ImGui::Button("Ok") || ImGui::IsKeyPressed(ImGuiKey_Enter)) {
                mode_ = Mode::None;
                ImGui::CloseCurrentPopup();
            }
            ImGui::EndPopup();
        }
    }
}
