#ifndef CLEM_HOST_DISK_BROWSER_HPP
#define CLEM_HOST_DISK_BROWSER_HPP

#include "clem_disk_asset.hpp"

#include "imgui.h"

#include <chrono>
#include <future>
#include <utility>

//  Using the ClemensDiskBrowser
//    - Browse for disk images supported by the emulator for the specified DiskType
//    - The selected file path asset is then used to create an actual loaded
//      asset (see ClemensDiskAsset interface.)
//    - The selected asset path/driveType will be 'sent' to the backend so it
//      can perform the nibblization and own the associated memory for the asset.
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
};

#endif
