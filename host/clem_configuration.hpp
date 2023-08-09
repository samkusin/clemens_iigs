#ifndef CLEM_HOST_CONFIGURATION_HPP
#define CLEM_HOST_CONFIGURATION_HPP

#include "core/clem_apple2gs_config.hpp"

#include <array>

#if defined(CLEMENS_PLATFORM_WINDOWS)
#define CLEM_HOST_COMPANY_NAME     "Cinekine"
#define CLEM_HOST_APPLICATION_NAME "Clemens"
#else
#define CLEM_HOST_COMPANY_NAME     "cinekine"
#define CLEM_HOST_APPLICATION_NAME "Clemens"
#endif

#define CLEM_EMULATOR_RAM_MINIMUM 256U
#define CLEM_EMULATOR_RAM_DEFAULT 4096U
#define CLEM_EMULATOR_RAM_MAXIMUM 8192U

#include <array>
#include <string>

namespace ClemensHostStyle {
    constexpr float kSideBarMinWidth = 160.0f;
    constexpr int kScreenWidth = 720;
    constexpr int kScreenHeight = 480;
    constexpr int kDiskTrayHeight = 360;
}

struct ClemensJoystickBindings {
    int axisAdj[2];
    unsigned button[2];
};

struct ClemensConfiguration {
    enum class ViewMode {
      Windowed,
      Fullscreen
    };

    std::string iniPathname;
    unsigned majorVersion;
    unsigned minorVersion;
    std::string dataDirectory;
    std::string romFilename;
    int logLevel;
    ViewMode viewMode;
    bool poweredOn;
    bool hybridInterfaceEnabled;

    std::array<ClemensJoystickBindings, 2> joystickBindings;

    ClemensAppleIIGSConfig gs;

    bool fastEmulationEnabled;

    ClemensConfiguration();
    ClemensConfiguration(std::string pathname, std::string datadir);
    ClemensConfiguration(const ClemensConfiguration &other) { copyFrom(other); }
    ClemensConfiguration &operator=(const ClemensConfiguration &other) {
        copyFrom(other);
        return *this;
    }

    bool isNewInstall() const { return majorVersion == 0 && minorVersion == 0; }

    // clears the dirty flag
    bool save();
    // sets the dirty flag
    void setDirty();

  private:
    void copyFrom(const ClemensConfiguration &other);
    static int handler(void *user, const char *section, const char *name, const char *value);
    bool isDirty;
};

ClemensConfiguration findConfiguration();

#endif
