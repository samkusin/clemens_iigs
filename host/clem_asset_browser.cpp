#include "clem_asset_browser.hpp"
#include "clem_file_browser.hpp"
#include "core/clem_disk_asset.hpp"
#include "core/clem_disk_utils.hpp"

#include "clem_2img.h"
#include "clem_mmio_types.h"
#include "clem_woz.h"

#include <algorithm>
#include <fstream>

namespace {

unsigned readDiskImageHeaderBytes(const std::filesystem::path &path, uint8_t *data,
                                  unsigned dataSizeLimit) {
    std::ifstream is(path, std::ios_base::binary);
    if (!is.is_open())
        return 0;
    is.read((char *)data, dataSizeLimit);
    if (is.fail()) {
        if (is.eof()) {
            return is.gcount();
        } else {
            return 0;
        }
    }
    return dataSizeLimit;
}

} // namespace

ClemensAssetBrowser::ClemensAssetBrowser() : diskType_(ClemensDiskAsset::DiskNone) {}

void ClemensAssetBrowser::setDiskType(ClemensDiskAsset::DiskType diskType) {
    diskType_ = diskType;
    createDiskImageType_ = ClemensDiskAsset::ImageNone;
    createDiskFilename_[0] = '\0';
    createDiskMBCount_ = 0;
    forceRefresh();
}

bool ClemensAssetBrowser::onCreateRecord(const std::filesystem::directory_entry &direntry,
                                         ClemensFileBrowser::Record &record) {
    ClemensDriveType driveType = kClemensDrive_Invalid;
    bool isSmartPortDrive = true;
    uint8_t headerData[128];

    //  is this a supported disk image?
    auto extension = direntry.path().extension().string();
    std::transform(extension.begin(), extension.end(), extension.begin(),
                   [](char c) { return std::tolower(c); });
    auto fileSize = record.size;
    if (extension == ".dsk") {
        if (fileSize == 140 * 1024) {
            driveType = kClemensDrive_5_25_D1;
        } else if (fileSize == 800 * 1024) {
            driveType = kClemensDrive_3_5_D1;
        }
    } else if (extension == ".do") {
        if (fileSize == 140 * 1024) {
            driveType = kClemensDrive_5_25_D1;
        }
    } else if (extension == ".po") {
        if (fileSize == 140 * 1024) {
            driveType = kClemensDrive_5_25_D1;
        } else if (fileSize == 800 * 1024) {
            driveType = kClemensDrive_3_5_D1;
        } else {
            isSmartPortDrive = true;
        }
    } else if (extension == ".2mg") {
        auto cnt =
            readDiskImageHeaderBytes(direntry.path(), headerData, CLEM_2IMG_HEADER_BYTE_SIZE);
        if (cnt >= CLEM_2IMG_HEADER_BYTE_SIZE) {
            Clemens2IMGDisk disk;
            if (clem_2img_parse_header(&disk, headerData, headerData + cnt)) {
                if (disk.block_count > 0) {
                    if (disk.block_count == CLEM_DISK_525_PRODOS_BLOCK_COUNT) {
                        driveType = kClemensDrive_5_25_D1;
                    } else if (disk.block_count == CLEM_DISK_35_PRODOS_BLOCK_COUNT ||
                               disk.block_count == CLEM_DISK_35_DOUBLE_PRODOS_BLOCK_COUNT) {
                        driveType = kClemensDrive_3_5_D1;
                    } else {
                        isSmartPortDrive = true;
                    }
                } else {
                    //  DOS 140K disk assumed
                    driveType = kClemensDrive_5_25_D1;
                }
            }
        }
    } else if (extension == ".woz") {
        auto cnt = readDiskImageHeaderBytes(direntry.path(), headerData, sizeof(headerData));
        const uint8_t *wozCurrent = clem_woz_check_header(headerData, cnt);
        if (wozCurrent) {
            const uint8_t *wozEnd = headerData + cnt;
            ClemensWOZChunkHeader wozChunk;
            ClemensWOZDisk disk{};
            wozCurrent = clem_woz_parse_chunk_header(&wozChunk, wozCurrent, wozEnd - wozCurrent);
            //  INFO chunk is always first.
            if (wozChunk.type == CLEM_WOZ_CHUNK_INFO && wozCurrent != nullptr) {
                if (clem_woz_parse_info_chunk(&disk, &wozChunk, wozCurrent, wozEnd - wozCurrent) !=
                    nullptr) {
                    if (disk.disk_type == CLEM_WOZ_DISK_5_25 &&
                        (disk.boot_type != CLEM_WOZ_BOOT_5_25_13)) {
                        driveType = kClemensDrive_5_25_D1;
                    } else if (disk.disk_type == CLEM_WOZ_DISK_3_5) {
                        driveType = kClemensDrive_3_5_D1;
                    }
                }
            }
        }
    }

    if ((diskType_ == ClemensDiskAsset::Disk35 && driveType == kClemensDrive_3_5_D1) ||
        (diskType_ == ClemensDiskAsset::Disk525 && driveType == kClemensDrive_5_25_D1)) {
        ClemensDiskAsset diskAsset(direntry.path().string(), driveType);
        ClemensAssetData *asset = ::new (record.context) ClemensAssetData;
        asset->diskType = diskAsset.diskType();
        asset->imageType = diskAsset.imageType();
    } else if (diskType_ == ClemensDiskAsset::DiskHDD && isSmartPortDrive) {
        ClemensDiskAsset diskAsset(direntry.path().string(), driveType);
        ClemensAssetData *asset = ::new (record.context) ClemensAssetData;
        asset->diskType = diskAsset.diskType();
        asset->imageType = diskAsset.imageType();
    } else {
        return false;
    }
    return true;
}

std::string ClemensAssetBrowser::onDisplayRecord(const ClemensFileBrowser::Record &record) {
    const ClemensAssetData *asset = reinterpret_cast<const ClemensAssetData *>(record.context);
    if (asset->diskType == ClemensDiskAsset::Disk35) {
        ImGui::TextUnformatted("3.5");
    } else if (asset->diskType == ClemensDiskAsset::Disk525) {
        ImGui::TextUnformatted("5.25");
    } else if (asset->diskType == ClemensDiskAsset::DiskHDD) {
        ImGui::TextUnformatted("HDD");
    } else {
        ImGui::TextUnformatted(" ");
    }
    return record.name;
}

auto ClemensAssetBrowser::onExtraSelectionUI(ImVec2 dimensions,
                                             ClemensFileBrowser::Record &selectedRecord)
    -> BrowserFinishedStatus {
    auto finishedStatus = BrowserFinishedStatus::None;
    ImGui::SameLine();
    if (ImGui::Button("Create Disk")) {
        ImGui::OpenPopup("Create Disk");
        createDiskFilename_[0] = '\0';
        createDiskMBCount_ = ClemensDiskUtilities::kMaximumHDDSizeInMB;
        if (diskType_ == ClemensDiskAsset::Disk35) {
            createDiskImageType_ = ClemensDiskAsset::Image2IMG;
        } else if (diskType_ == ClemensDiskAsset::Disk525) {
            createDiskImageType_ = ClemensDiskAsset::ImageProDOS;
        } else if (diskType_ == ClemensDiskAsset::DiskHDD) {
            createDiskImageType_ = ClemensDiskAsset::Image2IMG;
        } else {
            createDiskImageType_ = ClemensDiskAsset::ImageNone;
        }
    }

    //  Permit DSK, DO, PO, 2MG, WOZ for 5.25 disks
    //  Permit PO,2MG,WOZ for 3.5 disks
    if (ImGui::IsPopupOpen("Create Disk")) {
        ImVec2 popupSize(std::max(640.0f, dimensions.x * 0.66f), 0.0f);
        ImGui::SetNextWindowSize(popupSize);
        if (ImGui::BeginPopupModal("Create Disk", NULL,
                                   ImGuiWindowFlags_Modal | ImGuiWindowFlags_NoScrollbar |
                                       ImGuiWindowFlags_AlwaysAutoResize)) {

            ImGui::InputText("Filename", createDiskFilename_, sizeof(createDiskFilename_));
            if (ImGui::BeginCombo("Type", ClemensDiskAsset::imageName(createDiskImageType_))) {
                switch (diskType_) {
                case ClemensDiskAsset::Disk35:
                    if (ImGui::Selectable(
                            ClemensDiskAsset::imageName(ClemensDiskAsset::ImageProDOS))) {
                        createDiskImageType_ = ClemensDiskAsset::ImageProDOS;
                    }
                    if (ImGui::Selectable(
                            ClemensDiskAsset::imageName(ClemensDiskAsset::Image2IMG))) {
                        createDiskImageType_ = ClemensDiskAsset::Image2IMG;
                    }
                    if (ImGui::Selectable(
                            ClemensDiskAsset::imageName(ClemensDiskAsset::ImageWOZ))) {
                        createDiskImageType_ = ClemensDiskAsset::ImageWOZ;
                    }
                    break;
                case ClemensDiskAsset::Disk525:
                    if (ImGui::Selectable(
                            ClemensDiskAsset::imageName(ClemensDiskAsset::ImageProDOS))) {
                        createDiskImageType_ = ClemensDiskAsset::ImageProDOS;
                    }
                    if (ImGui::Selectable(
                            ClemensDiskAsset::imageName(ClemensDiskAsset::ImageDSK))) {
                        createDiskImageType_ = ClemensDiskAsset::ImageDSK;
                    }
                    if (ImGui::Selectable(
                            ClemensDiskAsset::imageName(ClemensDiskAsset::ImageDOS))) {
                        createDiskImageType_ = ClemensDiskAsset::ImageDOS;
                    }
                    if (ImGui::Selectable(
                            ClemensDiskAsset::imageName(ClemensDiskAsset::ImageWOZ))) {
                        createDiskImageType_ = ClemensDiskAsset::ImageWOZ;
                    }
                    if (ImGui::Selectable(
                            ClemensDiskAsset::imageName(ClemensDiskAsset::Image2IMG))) {
                        createDiskImageType_ = ClemensDiskAsset::Image2IMG;
                    }
                    break;
                case ClemensDiskAsset::DiskHDD:
                    if (ImGui::Selectable(
                            ClemensDiskAsset::imageName(ClemensDiskAsset::ImageProDOS))) {
                        createDiskImageType_ = ClemensDiskAsset::ImageProDOS;
                    }
                    if (ImGui::Selectable(
                            ClemensDiskAsset::imageName(ClemensDiskAsset::Image2IMG))) {
                        createDiskImageType_ = ClemensDiskAsset::Image2IMG;
                    }
                    break;
                default:
                    ImGui::Selectable(ClemensDiskAsset::imageName(ClemensDiskAsset::ImageNone));
                    break;
                }
                ImGui::EndCombo();
            }
            if (diskType_ == ClemensDiskAsset::DiskHDD) {
                //  allow size selection
                ImGui::SliderInt("Size (MB)", &createDiskMBCount_, 1,
                                 int(ClemensDiskUtilities::kMaximumHDDSizeInMB), "%d",
                                 ImGuiSliderFlags_AlwaysClamp);
            }
            ImGui::Spacing();
            ImGui::Separator();
            if (ImGui::Button("OK")) {
                std::filesystem::path fileName = createDiskFilename_;
                finishedStatus = BrowserFinishedStatus::Selected;
                switch (createDiskImageType_) {
                case ClemensDiskAsset::Image2IMG:
                    fileName.replace_extension(".2mg");
                    break;
                case ClemensDiskAsset::ImageDSK:
                    fileName.replace_extension(".dsk");
                    break;
                case ClemensDiskAsset::ImageDOS:
                    fileName.replace_extension(".do");
                    break;
                case ClemensDiskAsset::ImageProDOS:
                    fileName.replace_extension(".po");
                    break;
                case ClemensDiskAsset::ImageWOZ:
                    fileName.replace_extension(".woz");
                    break;
                default:
                    fileName.clear();
                    break;
                }
                if (!fileName.empty()) {
                    selectedRecord.path =
                        (std::filesystem::path(getCurrentDirectory()) / fileName).string();
                    selectedRecord.name = fileName.string();
                    selectedRecord.size = createDiskMBCount_ * 1024 * 1024;
                }
                ImGui::CloseCurrentPopup();
            }
            ImGui::SameLine();
            if (ImGui::Button("Cancel") || ImGui::IsKeyPressed(ImGuiKey_Escape)) {
                createDiskImageType_ = ClemensDiskAsset::ImageNone;
                createDiskMBCount_ = 0;
                ImGui::CloseCurrentPopup();
            }
            ImGui::EndPopup();
        }
    }

    return finishedStatus;
}
