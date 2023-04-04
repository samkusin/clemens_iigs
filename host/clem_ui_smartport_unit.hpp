
#ifndef CLEM_HOST_SMARTPORT_UNIT_UI_HPP
#define CLEM_HOST_SMARTPORT_UNIT_UI_HPP

#include <filesystem>
#include <string>
#include <vector>

class ClemensCommandQueue;
struct ClemensBackendDiskDriveState;

/**
 * @brief Represents a smartport unit drive device
 *
 * Smartport ProDOS images are block devices that leverage the 2IMG file format
 * and the storage of disk data as regular vs GCR encoded blocks of data.  This
 * format bypasses the ClemensDiskLibrary disk sandbox and references 2IMG files
 * on the host's disk system.
 *
 * - Displays the relative path of the 2iMG device (one level up + the file image
 *   name.)
 * - Select a 2IMG image using a file browser
 * - Display the default smartport.2mg image (located in the Disk Library Root Path
 *   in the combo as well
 */
class ClemensSmartPortUnitUI {
  public:
    ClemensSmartPortUnitUI(std::filesystem::path diskLibraryPath);

    bool frame(float width, float height, ClemensCommandQueue &backend,
               const ClemensBackendDiskDriveState &diskDrive, const char *driveName,
               bool showLabel);

  private:
    void doImportDiskFlow(float width, float height);
    void doExit(float width, float height);

    enum class Mode { None, ImportDisks, Exit };
    enum class ErrorMode { None, ImportFailed };

    void startFlow(Mode mode);
    void finish(std::string errorString = "");

    void discoverNextLocalDiskPath();

  private:
    std::filesystem::path diskRootPath_;
    Mode mode_;
    Mode finishedMode_;
    std::string errorString_;
    bool generatingDiskList_;

    std::vector<std::filesystem::path> localDiskPaths_;
    std::filesystem::directory_iterator libraryRootIterator_;
};

#endif
