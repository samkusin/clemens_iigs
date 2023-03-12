
#ifndef CLEM_HOST_DISK_UNIT_UI_HPP
#define CLEM_HOST_DISK_UNIT_UI_HPP

#include "clem_disk.h"

#include <string>
#include <vector>

class ClemensCommandQueue;
class ClemensDiskLibrary;
struct ClemensBackendDiskDriveState;

/**
 * @brief Represents a disk drive device
 *
 */
class ClemensDiskUnitUI {
  public:
    ClemensDiskUnitUI(ClemensDiskLibrary &diskLibrary, ClemensDriveType diskDriveType);

    bool frame(float width, float height, ClemensCommandQueue &backend,
               const ClemensBackendDiskDriveState &diskDrive, const char *driveName,
               bool showLabel);

  private:
    void doImportDiskFlow(float width, float height);
    void doBlankDiskFlow(float width, float height);
    void doCreateBlankDisk(float width, float height, ClemensCommandQueue &backend);
    void doFinishImportDisks(float width, float height);
    void doRetryFlow(float width, float height, ClemensCommandQueue &backend);
    void doExit(float width, float height);

    enum DiskSetSelectorResult {
        None,   //  no selection made
        Ok,     //  diskNameEntry_ is populated with the name of the set to use
        Create, //  diskNameEntry_ is populated with the name of the set to create
        Retry,
        Cancel
    };

    DiskSetSelectorResult doDiskSetSelector(float width, float height);

    enum class Mode {
        None,
        InsertBlankDisk,
        ImportDisks,
        CreateBlankDisk,
        FinishImportDisks,
        Retry,
        Exit
    };
    enum class ErrorMode { None, CannotCreateDiskSet, CannotCreateDisk, ImportFailed };

    void startFlow(Mode mode);
    void retry();
    void cancel();
    void finish(std::string errorString = "");
    void createBlankDisk(ClemensCommandQueue &backendQueue);
    void createDiskSet();
    std::pair<std::string, bool> importDisks(const std::string &outputPath);

  private:
    ClemensDiskLibrary &diskLibrary_;
    ClemensDriveType diskDriveType_;
    unsigned int diskDriveCategoryType_;

    Mode mode_;
    Mode retryMode_;
    Mode finishedMode_;
    std::string errorString_;

    bool generatingDiskList_;

    std::vector<std::string> importDiskFiles_;
    std::string selectedDiskSetName_;
    char diskNameEntry_[256];
};

#endif
