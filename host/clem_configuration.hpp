#ifndef CLEM_HOST_CONFIGURATION_HPP
#define CLEM_HOST_CONFIGURATION_HPP

#include "clem_host_platform.h"

#if defined(CLEMENS_PLATFORM_WINDOWS)
#define CLEM_HOST_COMPANY_NAME     "Cinekine"
#define CLEM_HOST_APPLICATION_NAME "Clemens"
#else
#define CLEM_HOST_COMPANY_NAME     "cinekine"
#define CLEM_HOST_APPLICATION_NAME "Clemens"
#endif

#include <string>

struct ClemensConfiguration {
    std::string iniPathname;
    std::string dataDirectory;
    unsigned majorVersion;
    unsigned minorVersion;

    ClemensConfiguration();
    ClemensConfiguration(std::string pathname, std::string datadir);
    ClemensConfiguration(const ClemensConfiguration &other) { copyFrom(other); }
    ClemensConfiguration &operator=(const ClemensConfiguration &other) {
        copyFrom(other);
        return *this;
    }

    bool isNewInstall() const { return majorVersion == 0 && minorVersion == 0; }

    bool save();

  private:
    void copyFrom(const ClemensConfiguration &other);
    static int handler(void *user, const char *section, const char *name, const char *value);
};

#endif
