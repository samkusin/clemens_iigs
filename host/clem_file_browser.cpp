#include "clem_file_browser.hpp"

#include "imgui.h"

#include <cassert>
#include <system_error>

#if defined(_WIN32)
#define WIN32_LEAN_AND_MEAN

#ifndef NOMINMAX
#define NOMINMAX
#endif

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

namespace {
//  Working around the ugly fact that C++17 cannot convert std::filesystem::file_time_type
//  to an actual string representation!  Taken from https://stackoverflow.com/a/58237530
template <typename TP> std::time_t to_time_t(TP tp) {
    using namespace std::chrono;
    auto sctp = std::chrono::time_point_cast<std::chrono::system_clock::duration>(
        tp - TP::clock::now() + std::chrono::system_clock::now());
    return std::chrono::system_clock::to_time_t(sctp);
}
} // namespace

ClemensFileBrowser::ClemensFileBrowser() {
    nextRefreshTime_ = std::chrono::steady_clock::now();
    selectionStatus_ = BrowserFinishedStatus::None;
}

void ClemensFileBrowser::setCurrentDirectory(const std::string &directory) {
    if (directory.empty()) {
        currentDirectoryPath_ = std::filesystem::current_path();
    } else {
        currentDirectoryPath_ = directory;
    }
    forceRefresh();
}

void ClemensFileBrowser::forceRefresh() { nextRefreshTime_ = std::chrono::steady_clock::now(); }

void ClemensFileBrowser::frame(ImVec2 size) {
    //  standardize our current working directory
    if (currentDirectoryPath_.empty()) {
        currentDirectoryPath_ = std::filesystem::current_path();
    }
    auto cwd = currentDirectoryPath_.make_preferred();
    if (!cwd.is_absolute()) {
        cwd = std::filesystem::absolute(cwd);
        cwd = std::filesystem::canonical(cwd);
    }
    //  query records using this directory and a specified filter
    if (!getRecordsResult_.valid() && std::chrono::steady_clock::now() >= nextRefreshTime_) {
        getRecordsResult_ = std::async(
            std::launch::async, &ClemensFileBrowser::getRecordsFromDirectory, this, cwd.string());
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

    //  this is reset always so the caller must call selected() after calling frame() to detect
    //  a selection
    selectionStatus_ = BrowserFinishedStatus::None;

    //  Current Volume Combo (Win32 Only)
    //  Current Path Edit Box
    ImGui::BeginChild("#FileBrowser", size);
    auto cwdIter = cwd.begin();
    unsigned driveLetterMask = win32GetDriveLettersBitmask();
    if (driveLetterMask) {
        //  "C:" combo
        ++cwdIter;
    }
    for (; cwdIter != cwd.end(); ++cwdIter) {
        auto name = (*cwdIter).string();
        auto nextX = ImGui::GetCursorPosX() + ImGui::GetStyle().FramePadding.x +
                     ImGui::CalcTextSize(name.c_str()).x;
        if (nextX >= ImGui::GetContentRegionMax().x) {
            ImGui::NewLine();
        }
        if (ImGui::Button(name.c_str())) {
            auto it = cwd.begin();
            std::filesystem::path selectedWorkingDirectory;
            for (auto itEnd = cwdIter; it != itEnd; ++it) {
                selectedWorkingDirectory /= *it;
            }
            selectedWorkingDirectory /= *it;
            currentDirectoryPath_ = selectedWorkingDirectory;
            forceRefresh();
        }
        ImGui::SameLine();
    }
    ImGui::NewLine();
    //  Listbox
    ImVec4 evenRowColor = ImGui::GetStyleColorVec4(ImGuiCol_WindowBg);
    ImVec4 oddRowColor = ImVec4(evenRowColor.x * 0.75f, evenRowColor.y * 0.75f,
                                evenRowColor.z * 0.75f, evenRowColor.w);
    ImGui::PushStyleColor(ImGuiCol_TableRowBg, evenRowColor);
    ImGui::PushStyleColor(ImGuiCol_TableRowBgAlt, oddRowColor);

    ImVec2 cursorPos = ImGui::GetCursorPos();
    // account for bottom separator plus one row of buttons
    ImVec2 listSize(-FLT_MIN,
                    6 * (ImGui::GetStyle().FrameBorderSize + ImGui::GetStyle().FramePadding.y) +
                        ImGui::GetTextLineHeightWithSpacing());
    listSize.y = ImGui::GetWindowHeight() - listSize.y - cursorPos.y;
    if (ImGui::BeginTable("##FileList", 4, ImGuiTableFlags_ScrollY | ImGuiTableFlags_RowBg,
                          listSize)) {

        ImGui::TableSetupColumn("", ImGuiTableColumnFlags_WidthFixed, ImGui::CalcTextSize("---").x);
        ImGui::TableSetupColumn("", ImGuiTableColumnFlags_WidthStretch);
        ImGui::TableSetupColumn("", ImGuiTableColumnFlags_WidthFixed,
                                ImGui::CalcTextSize("9999 Kb").x);
        ImGui::TableSetupColumn("", ImGuiTableColumnFlags_WidthFixed,
                                ImGui::CalcTextSize("XXXX-XX-XX XX:XX").x);
        std::string filename;
        for (auto const &record : records_) {
            //      icon (5.25, 3.5, HDD), filename, date
            ImGui::TableNextRow();
            ImGui::TableSetColumnIndex(0);
            auto filename = onDisplayRecord(record);
            ImGui::TableSetColumnIndex(1);
            bool isSelected = ImGui::Selectable(
                filename.c_str(), record.name == selectedRecord_.name,
                ImGuiSelectableFlags_AllowDoubleClick | ImGuiSelectableFlags_SpanAllColumns |
                    ImGuiSelectableFlags_DontClosePopups);
            if (!selectionMade && (isSelected || record.name == selectedRecord_.name)) {
                selectionFound = true;
                selectedRecord_ = record;
                if (ImGui::IsItemHovered() && ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left)) {
                    selectionMade = true;
                    forceRefresh();
                }
            }
            ImGui::TableSetColumnIndex(2);
            if (!record.isDirectory) {
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
    ImGui::PopStyleColor(2);
    if (!selectionFound) {
        //  might have been cleared by a directory change, or if the file was deleted
        selectedRecord_ = Record();
    }

    ImGui::Spacing();
    if (ImGui::Button("Select") || selectionMade || ImGui::IsKeyPressed(ImGuiKey_Enter)) {
        if (selectedRecord_.isDirectory) {
            currentDirectoryPath_ = cwd / selectedRecord_.name;
            forceRefresh();
        } else {
            selectionStatus_ = BrowserFinishedStatus::Selected;
        }
    }
    ImGui::SameLine();
    if (ImGui::Button("Cancel") || ImGui::IsKeyPressed(ImGuiKey_Escape)) {
        selectionStatus_ = BrowserFinishedStatus::Cancelled;
    }

    if (selectionStatus_ == BrowserFinishedStatus::None) {
        selectionStatus_ = onExtraSelectionUI(size, selectedRecord_);
    }

    ImGui::EndChild();
}

//  user selected the current highlight
bool ClemensFileBrowser::isSelected() const {
    return selectionStatus_ == BrowserFinishedStatus::Selected;
}

bool ClemensFileBrowser::isCancelled() const {
    return selectionStatus_ == BrowserFinishedStatus::Cancelled;
}

bool ClemensFileBrowser::isDone() const { return isSelected() || isCancelled(); }

//  gets the currently selected or highlighted item
std::string ClemensFileBrowser::getCurrentPathname() const { return selectedRecord_.path; }

std::string ClemensFileBrowser::getCurrentDirectory() const {
    return currentDirectoryPath_.string();
}

size_t ClemensFileBrowser::getFileSize() const { return selectedRecord_.size; }

std::string ClemensFileBrowser::onDisplayRecord(const Record &record) {
    //  display the file type
    ImGui::TextUnformatted(" ");
    return record.name;
}

auto ClemensFileBrowser::getRecordsFromDirectory(std::string directoryPathname) -> Records {
    Records records;

    //  cwdName_ identifies the directory to introspect
    //  flat structure (do not descent into directories)
    auto directoryPath = std::filesystem::path(directoryPathname);
    assert(directoryPath.is_absolute());
    std::error_code errc{};

    //  directories on top
    for (auto &entry : std::filesystem::directory_iterator(directoryPath)) {
        Record record{};

        auto writeTime = std::filesystem::last_write_time(entry.path(), errc);
        if (errc)
            continue;
        record.fileTime = to_time_t(writeTime);
        if (entry.path().stem().string().front() == '.')
            continue;
        if (entry.is_directory()) {
            record.path = entry.path().string();
            record.name = entry.path().filename().string();
            record.isDirectory = true;
            records.emplace_back(std::move(record));
            continue;
        }
    }
    for (auto &entry : std::filesystem::directory_iterator(directoryPath)) {
        if (entry.is_directory())
            continue;

        Record record{};
        auto writeTime = std::filesystem::last_write_time(entry.path(), errc);
        if (errc)
            continue;
        auto fileSize = std::filesystem::file_size(entry.path(), errc);
        if (errc)
            continue;
        record.path = entry.path().string();
        record.name = entry.path().filename().string();
        record.size = fileSize;
        record.fileTime = to_time_t(writeTime);
        if (onCreateRecord(entry, record)) {
            records.emplace_back(record);
        }
    }

    return records;
}
