#include "clem_configuration.hpp"
#include "fmt/format.h"
#include "ini.h"

#include <cstdio>
#include <cstring>
#include <filesystem>

ClemensConfiguration::ClemensConfiguration()
    : majorVersion(0), minorVersion(0), hybridInterfaceEnabled(false) {}

ClemensConfiguration::ClemensConfiguration(std::string pathname, std::string datadir)
    : ClemensConfiguration() {
    iniPathname = std::move(pathname);
    dataDirectory = std::move(datadir);
    if (ini_parse(iniPathname.c_str(), &ClemensConfiguration::handler, this)) {
        return;
    }
}

void ClemensConfiguration::copyFrom(const ClemensConfiguration &other) {
    iniPathname = other.iniPathname;
    dataDirectory = other.dataDirectory;
    romFilename = other.romFilename;
    majorVersion = other.majorVersion;
    minorVersion = other.minorVersion;
    hybridInterfaceEnabled = other.hybridInterfaceEnabled;
}

bool ClemensConfiguration::save() {
    FILE *fp = fopen(iniPathname.c_str(), "w");
    if (!fp) {
        fmt::print("ClemensConfiguration: failed to write {}\n", iniPathname);
        return false;
    }
    fprintf(fp, "[host]\n");
    fprintf(fp, "major=%u\n", majorVersion);
    fprintf(fp, "minor=%u\n", minorVersion);
    fprintf(fp, "data=%s\n", dataDirectory.c_str());
    fprintf(fp, "hybrid=%u\n", hybridInterfaceEnabled ? 1 : 0);
    fprintf(fp, "\n");
    fprintf(fp, "[emulator]\n");
    fprintf(fp, "romfile=%s\n", romFilename.c_str());
    fprintf(fp, "\n");

    fclose(fp);
    return true;
}

int ClemensConfiguration::handler(void *user, const char *section, const char *name,
                                  const char *value) {
    auto *config = reinterpret_cast<ClemensConfiguration *>(user);
    if (strncmp(section, "host", 16) == 0) {
        if (strncmp(name, "major", 16) == 0) {
            config->majorVersion = (unsigned)(atoi(value));
        } else if (strncmp(name, "minor", 16) == 0) {
            config->minorVersion = (unsigned)(atoi(value));
        } else if (strncmp(name, "data", 16) == 0) {
            config->dataDirectory = value;
        } else if (strncmp(name, "hybrid", 16) == 0) {
            config->hybridInterfaceEnabled = atoi(value) > 0 ? true : false;
        }
    } else if (strncmp(section, "emulator", 16) == 0) {
        if (strncmp(name, "romfile", 16) == 0) {
            config->romFilename = value;
        }
    }
    return 1;
}
