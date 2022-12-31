#ifndef CLEM_HOST_CONFIGURATION_HPP
#define CLEM_HOST_CONFIGURATION_HPP

#include <string>

struct ClemensConfiguration {
    unsigned majorVersion;
    unsigned minorVersion;

    ClemensConfiguration(std::string iniPathname);

    void save();

  private:
    std::string iniPathname_;

    static int handler(void *user, const char *section, const char *name, const char *value);
};

#endif
