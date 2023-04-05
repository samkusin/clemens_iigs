#include "clem_ui_smartport_unit.hpp"
#include "clem_command_queue.hpp"
#include "clem_host_platform.h"

#include "fmt/format.h"
#include "imgui.h"
#include "imgui_filedialog/ImGuiFileDialog.h"

#include "clem_2img.h"

#include <filesystem>

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
                    selectedPath = diskPath.parent_path().filename() / diskPath.filename();
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
        doImportDiskFlow(viewportSize.x, viewportSize.y);
        break;
    case Mode::InsertBlankDisk:
        doBlankDiskFlow(viewportSize.x, viewportSize.y);
        break;
    case Mode::Exit:
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

    Clemens2IMGDisk disk;
    if (!clem_2img_parse_header(&disk, header, header + sizeof(header)))
        return;
    localDiskPaths_.emplace_back(entry.path());
}

void ClemensSmartPortUnitUI::doImportDiskFlow(float width, float height) {}

void ClemensSmartPortUnitUI::doExit(float width, float height) {}

void ClemensSmartPortUnitUI::startFlow(Mode mode) {}

void ClemensSmartPortUnitUI::finish(std::string errorString) {}
