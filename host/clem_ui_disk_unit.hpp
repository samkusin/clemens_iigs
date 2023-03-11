
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

  private:
    ClemensDiskLibrary &diskLibrary_;
    ClemensDriveType diskDriveType_;
    unsigned int diskDriveCategoryType_;

    enum class Mode { None, InsertBlankDisk, ImportDisks, FinishImportDisks, CreateDiskSet, Exit };
    Mode mode_;
    bool generatingDiskList_;

    std::vector<std::string> importDiskFiles_;
    std::string selectedEntry_;
    char diskNameEntry_[256];
};

#endif
