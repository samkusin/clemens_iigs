#include "clem_ui_smartport_unit.hpp"
#include "clem_command_queue.hpp"
#include "clem_host_platform.h"

#include "fmt/format.h"
#include "imgui.h"
#include "imgui_filedialog/ImGuiFileDialog.h"

#include "clem_2img.h"

#include <filesystem>

namespace {
const ImVec2 guiDialogSizeLarge(float viewWidth, float viewHeight) {
    return ImVec2(std::max(800.0f, viewWidth * 0.80f), std::max(480.0f, viewHeight * 0.60f));
}

const ImVec2 guiDialogSizeSmall(float viewWidth, float viewHeight) {
    return ImVec2(std::max(640.0f, viewWidth * 0.50f), std::max(200.0f, viewHeight * 0.25f));
}
} // namespace

ClemensSmartPortUnitUI::ClemensSmartPortUnitUI(unsigned driveIndex,
                                               std::filesystem::path diskLibraryPath)
    : diskRootPath_(diskLibraryPath), mode_(Mode::None), finishedMode_(Mode::None),
      driveIndex_(driveIndex), generatingDiskList_(false), libraryRootIterator_(diskRootPath_) {}

bool ClemensSmartPortUnitUI::frame(float width, float height, ClemensCommandQueue &backend,
                                   const ClemensBackendDiskDriveState &diskDrive,
                                   const char *driveName, bool showLabel) {
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
            localDiskPaths_.clear();
            libraryRootIterator_ = std::filesystem::directory_iterator(diskRootPath_);
            generatingDiskList_ = true;
        } else {
            discoverNextLocalDiskPath();
        }
        if (!diskDrive.imagePath.empty() && !diskDrive.isEjecting && ImGui::Selectable("<eject>")) {
            backend.ejectSmartPortDisk(driveIndex_);
        }
        if (diskDrive.imagePath.empty()) {
            if (ImGui::Selectable("<insert blank disk>")) {
                startFlow(Mode::InsertBlankDisk);
            }
            if (ImGui::Selectable("<...>")) {
                startFlow(Mode::ImportDisks);
            }
            ImGui::Separator();
            std::filesystem::path selectedPath;
            for (auto &diskPath : localDiskPaths_) {
                auto relativePath = diskPath.parent_path().filename() / diskPath.stem();
                if (ImGui::Selectable(relativePath.string().c_str())) {
                    selectedPath = diskPath;
                }
            }
            if (!selectedPath.empty()) {
                backend.insertSmartPortDisk(driveIndex_, selectedPath.string());
            }
            ImGui::Separator();
        }
        ImGui::EndCombo();
    } else {
        generatingDiskList_ = false;
    }

    const ImVec2 &viewportSize = ImGui::GetMainViewport()->Size;
    switch (mode_) {
    case Mode::ImportDisks:
        doImportDiskFlow(viewportSize.x, viewportSize.y, backend);
        break;
    case Mode::InsertBlankDisk:
        doBlankDiskFlow(viewportSize.x, viewportSize.y, backend);
        break;
    case Mode::Exit:
        doExit(viewportSize.x, viewportSize.y);
        break;
    case Mode::None:
        break;
    }

    return true;
}

void ClemensSmartPortUnitUI::discoverNextLocalDiskPath() {
    if (!generatingDiskList_)
        return;
    if (libraryRootIterator_ == std::filesystem::directory_iterator())
        return;

    auto &entry = *(libraryRootIterator_++);
    if (!entry.is_regular_file())
        return;
    std::ifstream dskFile(entry.path(), std::ios_base::in | std::ios_base::binary);
    if (!dskFile.is_open()) {
        fmt::print("Unable to open disk image {} for inspection\n", entry.path().string());
        return;
    }
    uint8_t header[64];
    if (dskFile.read((char *)header, sizeof(header)).fail())
        return;

    //  PO and 2MG images are supported
    Clemens2IMGDisk disk{};
    if (clem_2img_parse_header(&disk, header, header + sizeof(header))) {
        localDiskPaths_.emplace_back(entry.path());
        return;
    }
    //  PO images are not validated at this point.  Extension checks are good
    //  enough (the user will be informed of a problem when mounting)
    if (entry.path().has_extension() && entry.path().extension() == ".po" ||
        entry.path().extension() == ".PO") {
        localDiskPaths_.emplace_back(entry.path());
        return;
    }
}

void ClemensSmartPortUnitUI::doImportDiskFlow(float width, float height,
                                              ClemensCommandQueue &backend) {
    if (!ImGuiFileDialog::Instance()->IsOpened("choose_disk_images")) {
        const char *filters = "ProDOS disk image files (*.2mg *.po){.2mg,.po}";
        ImGuiFileDialog::Instance()->OpenDialog("choose_disk_images", "Choose Disk Image", filters,
                                                ".", "", 1, nullptr, ImGuiFileDialogFlags_Modal);
    }
    if (ImGuiFileDialog::Instance()->Display("choose_disk_images", ImGuiWindowFlags_NoCollapse,
                                             guiDialogSizeLarge(width, height),
                                             ImVec2(width, height))) {
        if (ImGuiFileDialog::Instance()->IsOk()) {
            auto selection = ImGuiFileDialog::Instance()->GetFilePathName();
            backend.insertSmartPortDisk(driveIndex_, selection);
            finish();
        } else {
            cancel();
        }
        ImGuiFileDialog::Instance()->Close();
    }
}

void ClemensSmartPortUnitUI::doBlankDiskFlow(float width, float height,
                                             ClemensCommandQueue &backend) {
    //  file browser for save as... (.2mg)
    //  if it already exists, prompt that this operation will overwriting the
    //  existing:
    //      - yes, then proceed
    //      - no, then cancel
    //  else:
    //      - proceed
    //      - backend command to mount disk
    //      - wait for confirmation from backend (using a new event succeeded or failed)
    //  succeeded:
    //      - end
    //  failed:
    //      - end with error
    //  end:
    //      message that operation completed or an error occurred
}

void ClemensSmartPortUnitUI::doExit(float width, float height) {}

void ClemensSmartPortUnitUI::startFlow(Mode mode) {
    mode_ = mode;
    finishedMode_ = Mode::None;
    errorString_.clear();
}

void ClemensSmartPortUnitUI::cancel() {
    finishedMode_ = mode_;
    mode_ = Mode::None;
}

void ClemensSmartPortUnitUI::finish(std::string errorString) {
    errorString_ = std::move(errorString);
    finishedMode_ = mode_;
    mode_ = Mode::Exit;
}
