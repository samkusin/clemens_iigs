#ifndef CLEM_HOST_FILE_BROWSER_HPP
#define CLEM_HOST_FILE_BROWSER_HPP

#include "core/clem_disk_asset.hpp"

#include "imgui.h"

#include <chrono>
#include <future>
#include <utility>

//  Using the ClemensFileBrowser
//    - Display files according to a filter at the 'current directory'
//    - Highlight (single click) of directories and files
//    - Select (double click) of directory descends into directory
//    - Select of file is a signal to end browsing
//
class ClemensDiskBrowser {
  public:
    ClemensDiskBrowser(const std::string &idName) : idName_(idName) {}
    bool isOpen() const;
    void open(ClemensDiskAsset::DiskType diskType, const std::string &browsePath = "");
    bool display(const ImVec2 &maxSize);
    void close();

    bool isOk() const { return finishedStatus_ == BrowserFinishedStatus::Selected; }
    bool isCancel() const { return finishedStatus_ == BrowserFinishedStatus::Cancelled; }

    ClemensDiskAsset acquireSelectedFilePath() { return std::move(selectedRecord_.asset); }
    unsigned acquireSelectedBlockCount() { return selectedRecord_.size / 512; }

    bool isSelectedFilePathNewFile() const {
        return createDiskImageType_ != ClemensDiskAsset::ImageNone;
    }

  private:
    struct Record {
        ClemensDiskAsset asset;
        size_t size = 0;
        std::time_t fileTime;
        bool isDirectory() const;
    };
    using Records = std::vector<Record>;
    std::future<Records> getRecordsResult_;
    friend Records getRecordsFromDirectory(std::string directoryPathname,
                                           ClemensDiskAsset::DiskType diskType);

    enum class BrowserFinishedStatus { None, Active, Selected, Cancelled };
    //  list of records on the current path;
    std::string idName_;
    ClemensDiskAsset::DiskType diskType_ = ClemensDiskAsset::DiskNone;
    std::string cwdName_;
    Record selectedRecord_;
    std::vector<Record> records_;
    BrowserFinishedStatus finishedStatus_;
    std::chrono::steady_clock::time_point nextRefreshTime_;

    char createDiskFilename_[128];
    int createDiskMBCount_;
    ClemensDiskAsset::ImageType createDiskImageType_;
};
