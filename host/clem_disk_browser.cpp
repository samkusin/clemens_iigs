#include "clem_disk_browser.hpp"

#include "core/clem_disk_asset.hpp"
#include "imgui.h"

#include <algorithm>
#include <cassert>
#include <chrono>
#include <ctime>
#include <filesystem>
#include <fstream>
#include <future>
#include <ios>
#include <system_error>

#include "fmt/format.h"

#include "clem_2img.h"
#include "clem_disk.h"
#include "clem_woz.h"

#if defined(_WIN32)
#define WIN32_LEAN_AND_MEAN
#include <Windows.h>
static unsigned win32GetDriveLettersBitmask() { return ::GetLogicalDrives(); }
static struct tm *getTimeSpecFromTime(struct tm *tspec, std::time_t timet) {
    localtime_s(tspec, &timet);
    return tspec;
}
#else
static unsigned win32GetDriveLettersBitmask() { return 0; }
static struct tm *getTimeSpecFromTime(struct tm *tspec, std::time_t timet) {
    return localtime_r(&timet, tspec);
}
#endif

//  Working around the ugly fact that C++17 cannot convert std::filesystem::file_time_type
//  to an actual string representation!  Taken from https://stackoverflow.com/a/58237530
template <typename TP> std::time_t to_time_t(TP tp) {
    using namespace std::chrono;
    auto sctp = std::chrono::time_point_cast<std::chrono::system_clock::duration>(
        tp - TP::clock::now() + std::chrono::system_clock::now());
    return std::chrono::system_clock::to_time_t(sctp);
}

static unsigned readDiskImageHeaderBytes(const std::filesystem::path &path, uint8_t *data,
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

auto getRecordsFromDirectory(std::string directoryPathname, ClemensDiskAsset::DiskType diskType)
    -> ClemensDiskBrowser::Records {
    ClemensDiskBrowser::Records records;
    uint8_t headerData[128];
    bool isSmartPortDrive = false;

    //  cwdName_ identifies the directory to introspect
    //  flat structure (do not descent into directories)
    auto directoryPath = std::filesystem::path(directoryPathname);
    assert(directoryPath.is_absolute());

    //  directories on top
    for (auto &entry : std::filesystem::directory_iterator(directoryPath)) {
        ClemensDiskBrowser::Record record;
        record.fileTime = to_time_t(std::filesystem::last_write_time(entry.path()));
        if (entry.path().stem().string().front() == '.')
            continue;
        if (entry.is_directory()) {
            record.asset = ClemensDiskAsset(entry.path().string(), kClemensDrive_Invalid);
            records.emplace_back(std::move(record));
            continue;
        }
    }
    for (auto &entry : std::filesystem::directory_iterator(directoryPath)) {
        if (entry.is_directory())
            continue;
        ClemensDriveType driveType = kClemensDrive_Invalid;

        //  is this a supported disk image?
        auto extension = entry.path().extension().string();
        std::transform(extension.begin(), extension.end(), extension.begin(),
                       [](char c) { return std::tolower(c); });
        auto fileSize = std::filesystem::file_size(entry.path());
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
                readDiskImageHeaderBytes(entry.path(), headerData, CLEM_2IMG_HEADER_BYTE_SIZE);
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
            auto cnt = readDiskImageHeaderBytes(entry.path(), headerData, sizeof(headerData));
            const uint8_t *wozCurrent = clem_woz_check_header(headerData, cnt);
            if (wozCurrent) {
                const uint8_t *wozEnd = headerData + cnt;
                ClemensWOZChunkHeader wozChunk;
                ClemensWOZDisk disk{};
                wozCurrent =
                    clem_woz_parse_chunk_header(&wozChunk, wozCurrent, wozEnd - wozCurrent);
                //  INFO chunk is always first.
                if (wozChunk.type == CLEM_WOZ_CHUNK_INFO && wozCurrent != nullptr) {
                    if (clem_woz_parse_info_chunk(&disk, &wozChunk, wozCurrent,
                                                  wozEnd - wozCurrent) != nullptr) {
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

        if (driveType != kClemensDrive_Invalid) {
            if ((diskType == ClemensDiskAsset::Disk35 && driveType == kClemensDrive_3_5_D1) ||
                (diskType == ClemensDiskAsset::Disk525 && driveType == kClemensDrive_5_25_D1)) {
                ClemensDiskBrowser::Record record{};
                record.asset = ClemensDiskAsset(entry.path().string(), driveType);
                record.size = fileSize;
                record.fileTime = to_time_t(std::filesystem::last_write_time(entry.path()));
                records.emplace_back(record);
            } else if (diskType == ClemensDiskAsset::DiskHDD && isSmartPortDrive) {
                // TODO: add HDD disk asset which needs a constructor in ClemensDiskAsset
            }
        }
    }

    return records;
}

bool ClemensDiskBrowser::Record::isDirectory() const {
    return size == 0 && asset.diskType() == ClemensDiskAsset::DiskNone &&
           asset.imageType() == ClemensDiskAsset::ImageNone;
}

bool ClemensDiskBrowser::isOpen() const { return ImGui::IsPopupOpen(idName_.c_str()); }

void ClemensDiskBrowser::open(ClemensDiskAsset::DiskType diskType, const std::string &browsePath) {

    ImGui::OpenPopup(idName_.c_str());
    diskType_ = diskType;
    selectedRecord_ = Record();
    finishedStatus_ = BrowserFinishedStatus::Active;
    cwdName_ = browsePath;
    nextRefreshTime_ = std::chrono::steady_clock::now();
    createDiskFilename_[0] = '\0';
    createDiskImageType_ = ClemensDiskAsset::ImageNone;
}

bool ClemensDiskBrowser::display(const ImVec2 &maxSize) {
    if (!ImGui::IsPopupOpen(idName_.c_str()))
        return false;
    ImGui::SetNextWindowSize(maxSize);
    if (!ImGui::BeginPopupModal(idName_.c_str(), NULL,
                                ImGuiWindowFlags_Modal | ImGuiWindowFlags_NoScrollbar))
        return false;

    auto cwdPath = std::filesystem::path(cwdName_).make_preferred();
    if (!cwdPath.is_absolute()) {
        cwdPath = std::filesystem::absolute(cwdPath);
        cwdPath = std::filesystem::canonical(cwdPath);
    }

    if (!getRecordsResult_.valid() && std::chrono::steady_clock::now() >= nextRefreshTime_) {
        getRecordsResult_ =
            std::async(std::launch::async, &getRecordsFromDirectory, cwdPath.string(), diskType_);
    }
    if (getRecordsResult_.valid()) {
        if (getRecordsResult_.wait_for(std::chrono::milliseconds(1)) == std::future_status::ready) {
            records_ = getRecordsResult_.get();
            nextRefreshTime_ = std::chrono::steady_clock::now() + std::chrono::seconds(1);
        }
    }

    //  UI for the directory listing.
    bool selectionMade = false;
    bool selectionFound = false;

    //  Current Volume Combo (Win32 Only)
    //  Current Path Edit Box
    auto cwdIter = cwdPath.begin();
    unsigned driveLetterMask = win32GetDriveLettersBitmask();
    if (driveLetterMask) {
        //  "C:" combo
        ++cwdIter;
    }
    for (; cwdIter != cwdPath.end(); ++cwdIter) {
        auto name = (*cwdIter).string();
        auto nextX = ImGui::GetCursorPosX() + ImGui::GetStyle().FramePadding.x +
                     ImGui::CalcTextSize(name.c_str()).x;
        if (nextX >= ImGui::GetContentRegionMax().x) {
            ImGui::NewLine();
        }
        if (ImGui::Button(name.c_str())) {
            auto it = cwdPath.begin();
            std::filesystem::path selectedWorkingDirectory;
            for (auto itEnd = cwdIter; it != itEnd; ++it) {
                selectedWorkingDirectory /= *it;
            }
            selectedWorkingDirectory /= *it;
            cwdName_ = selectedWorkingDirectory.string();
        }
        ImGui::SameLine();
    }
    ImGui::NewLine();
    //  Listbox
    ImVec2 cursorPos = ImGui::GetCursorPos();
    // account for bottom separator plus one row of buttons
    ImVec2 listSize(-FLT_MIN,
                    6 * (ImGui::GetStyle().FrameBorderSize + ImGui::GetStyle().FramePadding.y) +
                        ImGui::GetTextLineHeightWithSpacing());
    listSize.y = ImGui::GetWindowHeight() - listSize.y - cursorPos.y;
    if (ImGui::BeginTable("##FileList", 4, 0, listSize)) {

        ImGui::TableSetupColumn("", ImGuiTableColumnFlags_WidthFixed,
                                ImGui::CalcTextSize("5.25").x);
        ImGui::TableSetupColumn("", ImGuiTableColumnFlags_WidthStretch);
        ImGui::TableSetupColumn("", ImGuiTableColumnFlags_WidthFixed,
                                ImGui::CalcTextSize("9999 Kb").x);
        ImGui::TableSetupColumn("", ImGuiTableColumnFlags_WidthFixed,
                                ImGui::CalcTextSize("XXXX-XX-XX XX:XX").x);
        for (auto const &record : records_) {
            //      icon (5.25, 3.5, HDD), filename, date
            auto filename = std::filesystem::path(record.asset.path()).filename().string();
            ImGui::TableNextRow();
            ImGui::TableSetColumnIndex(0);
            if (record.asset.diskType() == ClemensDiskAsset::Disk35) {
                ImGui::TextUnformatted("3.5");
            } else if (record.asset.diskType() == ClemensDiskAsset::Disk525) {
                ImGui::TextUnformatted("5.25");
            } else if (record.asset.diskType() == ClemensDiskAsset::DiskHDD) {
                ImGui::TextUnformatted("HDD");
            } else {
                ImGui::TextUnformatted(" ");
            }
            ImGui::TableSetColumnIndex(1);
            bool isSelected = ImGui::Selectable(
                filename.c_str(), record.asset.path() == selectedRecord_.asset.path(),
                ImGuiSelectableFlags_AllowDoubleClick | ImGuiSelectableFlags_SpanAllColumns |
                    ImGuiSelectableFlags_DontClosePopups);
            if (!selectionMade && isSelected) {
                selectionFound = true;
                selectedRecord_ = record;
                if (ImGui::IsItemHovered() && ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left)) {
                    selectionMade = true;
                    nextRefreshTime_ = std::chrono::steady_clock::now();
                }
            }
            ImGui::TableSetColumnIndex(2);
            if (!record.isDirectory()) {
                if (record.size >= 1024 * 1000) {
                    ImGui::Text("%.1f MB", record.size / (1024 * 1000.0));
                } else {
                    ImGui::Text("%zu KB", record.size / 1024);
                }
            } else {
                ImGui::Text(" ");
            }
            ImGui::TableSetColumnIndex(3);
            struct tm tspec;
            char timeFormatted[64];
            getTimeSpecFromTime(&tspec, record.fileTime);
            std::strftime(timeFormatted, sizeof(timeFormatted), "%F %R", &tspec);
            ImGui::TextUnformatted(timeFormatted);
        }
        ImGui::EndTable();
    }
    if (!selectionFound) {
        //  might have been cleared by a directory change, or if the file was deleted
        selectedRecord_ = Record();
    }

    ImGui::Spacing();
    if (ImGui::Button("Select") || selectionMade) {
        if (selectedRecord_.isDirectory()) {
            cwdName_ = selectedRecord_.asset.path();
            nextRefreshTime_ = std::chrono::steady_clock::now();
        } else {
            finishedStatus_ = BrowserFinishedStatus::Selected;
        }
    }
    ImGui::SameLine();
    if (ImGui::Button("Cancel") ||
        (ImGui::IsKeyPressed(ImGuiKey_Escape) && ImGui::IsWindowFocused())) {
        finishedStatus_ = BrowserFinishedStatus::Cancelled;
    }
    ImGui::SameLine();
    if (ImGui::Button("Create Disk")) {
        ImGui::OpenPopup("Create Disk");
        createDiskFilename_[0] = '\0';
        if (diskType_ == ClemensDiskAsset::Disk35) {
            createDiskImageType_ = ClemensDiskAsset::Image2IMG;
        } else if (diskType_ == ClemensDiskAsset::Disk525) {
            createDiskImageType_ = ClemensDiskAsset::ImageProDOS;
        } else {
            createDiskImageType_ = ClemensDiskAsset::ImageNone;
        }
    }

    //  Permit DSK, DO, PO, 2MG, WOZ for 5.25 disks
    //  Permit PO,2MG,WOZ for 3.5 disks
    if (ImGui::IsPopupOpen("Create Disk")) {
        ImVec2 popupSize(std::max(640.0f, maxSize.x * 0.66f), 0.0f);
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
                default:
                    ImGui::Selectable(ClemensDiskAsset::imageName(ClemensDiskAsset::ImageNone));
                    break;
                }
                ImGui::EndCombo();
            }
            ImGui::Spacing();
            ImGui::Separator();
            if (ImGui::Button("OK")) {
                std::filesystem::path fileName = createDiskFilename_;
                finishedStatus_ = BrowserFinishedStatus::Selected;
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
                    selectedRecord_.asset = (std::filesystem::path(cwdName_) / fileName).string();
                }
                ImGui::CloseCurrentPopup();
            }
            ImGui::SameLine();
            if (ImGui::Button("Cancel") || ImGui::IsKeyPressed(ImGuiKey_Escape)) {
                createDiskImageType_ = ClemensDiskAsset::ImageNone;
                ImGui::CloseCurrentPopup();
            }
            ImGui::EndPopup();
        }
    }

    if (finishedStatus_ != BrowserFinishedStatus::Active) {
        ImGui::CloseCurrentPopup();
    }

    ImGui::EndPopup();

    return finishedStatus_ == BrowserFinishedStatus::Selected ||
           finishedStatus_ == BrowserFinishedStatus::Cancelled;
}

void ClemensDiskBrowser::close() {
    diskType_ = ClemensDiskAsset::DiskNone;
    finishedStatus_ = BrowserFinishedStatus::None;
}
