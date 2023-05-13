#ifndef CLEM_HOST_FILE_BROWSER_HPP
#define CLEM_HOST_FILE_BROWSER_HPP

#include "imgui.h"

#include <chrono>
#include <ctime>
#include <filesystem>
#include <future>
#include <string>
#include <utility>
#include <vector>

//  Using the ClemensFileBrowser
//    - Display files according to a filter at the 'current directory'
//    - Highlight (single click) of directories and files
//    - Select (double click) of directory descends into directory
//    - Select of file is a signal to end browsing
//

class ClemensFileBrowser {
  public:
    ClemensFileBrowser();

    void setCurrentDirectory(const std::string &directory);

    void frame(ImVec2 size);

    //  user selected the current highlight
    bool isSelected() const;
    bool isCancelled() const;
    bool isDone() const;

    //  gets the currently selected or highlighted item
    std::string getCurrentPathname() const;
    std::string getCurrentDirectory() const;

  protected:
    struct Record {
        std::string name;
        uintptr_t context = 0;
        size_t size = 0;
        std::time_t fileTime = 0;
        bool isDirectory;
    };
    virtual void onModifyRecord(Record &){};
    virtual std::string onDisplayRecord(const Record &record);

  private:
    using Records = std::vector<Record>;
    Records getRecordsFromDirectory(std::string directoryPathname);

    std::future<Records> getRecordsResult_;
    //  list of records on the current path;
    std::filesystem::path currentDirectoryPath_;
    Record selectedRecord_;
    std::vector<Record> records_;
    std::chrono::steady_clock::time_point nextRefreshTime_;

    enum class BrowserFinishedStatus { None, Selected, Cancelled };
    BrowserFinishedStatus selectionStatus_;
};

#endif
