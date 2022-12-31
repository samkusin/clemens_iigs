#include "clem_configuration.hpp"
#include "fmt/format.h"
#include "ini.h"

#include <cstdio>
#include <cstring>

ClemensConfiguration::ClemensConfiguration(std::string iniPathname)
    : iniPathname_(std::move(iniPathname)), majorVersion(0), minorVersion(0) {

    if (ini_parse(iniPathname_.c_str(), &ClemensConfiguration::handler, this)) {
        return;
    }
}

void ClemensConfiguration::save() {
    FILE *fp = fopen(iniPathname_.c_str(), "w");
    if (!fp) {
        fmt::print("ClemensConfiguration: failed to write {}\n", iniPathname_);
        return;
    }
    fprintf(fp, "[host]\n");
    fprintf(fp, "major=%u\n", majorVersion);
    fprintf(fp, "minor=%u\n", minorVersion);
    fprintf(fp, "\n");

    fclose(fp);
}

int ClemensConfiguration::handler(void *user, const char *section, const char *name,
                                  const char *value) {
    auto *config = reinterpret_cast<ClemensConfiguration *>(user);
    if (strncmp(section, "host", 16) == 0) {
        if (strncmp(name, "major", 16) == 0) {
            config->majorVersion = (unsigned)(atoi(value));
        } else if (strncmp(name, "minor", 16) == 0) {
            config->minorVersion = (unsigned)(atoi(value));
        }
    }
    return 1;
}
