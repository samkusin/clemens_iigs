#ifndef CLEM_HOST_DISK_STATUS_HPP
#define CLEM_HOST_DISK_STATUS_HPP

#include <string>

struct ClemensDiskDriveStatus {
    enum class Error { None, MountFailed, SaveFailed };
    enum class Origin { None, DiskPort, CardPort };
    std::string assetPath;
    bool isWriteProtected = false;
    bool isSpinning = false;
    bool isEjecting = false;
    bool isSaved = false;
    Error error = Error::None;
    Origin origin = Origin::None;

    void mount(const std::string &path, Origin o);
    void unmount();
    void saveFailed();
    void mountFailed();
    void saved();
    bool isMounted() const;
};

#endif
