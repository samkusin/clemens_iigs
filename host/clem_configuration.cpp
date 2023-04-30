#include "clem_configuration.hpp"
#include "core/clem_disk_utils.hpp"

#include "clem_disk.h"
#include "fmt/format.h"
#include "ini.h"

#include <charconv>
#include <cstdio>
#include <cstring>

ClemensConfiguration::ClemensConfiguration()
    : majorVersion(0), minorVersion(0), hybridInterfaceEnabled(false), fastEmulationEnabled(true) {
    gs.audioSamplesPerSecond = 0;
    gs.memory = CLEM_EMULATOR_RAM_DEFAULT;
}

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
    gs = other.gs;
    hybridInterfaceEnabled = other.hybridInterfaceEnabled;
    fastEmulationEnabled = other.fastEmulationEnabled;
}

bool ClemensConfiguration::save() {
    FILE *fp = fopen(iniPathname.c_str(), "w");
    if (fp == NULL) {
        fmt::print(stderr, "ClemensConfiguration: failed to write {}\n", iniPathname);
        return false;
    }
    fmt::print(fp,
               "[host]\n"
               "major={}\n"
               "minor={}\n"
               "data={}\n"
               "hybrid={}\n"
               "\n",
               majorVersion, minorVersion, dataDirectory, hybridInterfaceEnabled ? 1 : 0);
    fmt::print(fp,
               "[emulator]\n"
               "romfile={}\n"
               "fastiwm={}\n"
               "gs.ramkb={}\n"
               "gs.audio_samples={}\n",
               romFilename, fastEmulationEnabled ? 1 : 0, gs.memory, gs.audioSamplesPerSecond);
    for (unsigned i = 0; i < (unsigned)gs.diskImagePaths.size(); i++) {
        auto driveType = static_cast<ClemensDriveType>(i);
        fmt::print(fp, "gs.disk.{}={}\n", ClemensDiskUtilities::getDriveName(driveType),
                   gs.diskImagePaths[i]);
    }
    for (unsigned i = 0; i < (unsigned)gs.smartPortImagePaths.size(); i++) {
        fmt::print(fp, "gs.smart.{}={}\n", i, gs.smartPortImagePaths[i]);
    }
    assert(gs.bram.size() == 256);
    for (unsigned i = 0; i < 256; i += 16) {
        fmt::print(fp,
                   "gs.bram{:02X}={:02X} {:02X} {:02X} {:02X} {:02X} {:02X} {:02X} {:02X} {:02X} "
                   "{:02X} {:02X} {:02X} {:02X} {:02X} {:02X} {:02X}\n",
                   gs.bram[i + 0x0], gs.bram[i + 0x1], gs.bram[i + 0x2], gs.bram[i + 0x3],
                   gs.bram[i + 0x4], gs.bram[i + 0x5], gs.bram[i + 0x6], gs.bram[i + 0x7],
                   gs.bram[i + 0x8], gs.bram[i + 0x9], gs.bram[i + 0xa], gs.bram[i + 0xb],
                   gs.bram[i + 0xc], gs.bram[i + 0xd], gs.bram[i + 0xe], gs.bram[i + 0xf]);
    }

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
        } else if (strncmp(name, "fastiwm", 16) == 0) {
            config->fastEmulationEnabled = atoi(value) > 0 ? true : false;
        } else if (strncmp(name, "gs.ramkb", 16) == 0) {
            config->gs.memory = (unsigned)atoi(value);
        } else if (strncmp(name, "gs.audio_samples", 32) == 0) {
            config->gs.audioSamplesPerSecond = (unsigned)atoi(value);
        } else if (strncmp(name, "gs.bram", 7) == 0) {
            const char *partial = name + 7;
            uint8_t offset, byte;
            if (std::from_chars(partial, partial + 2, offset, 16).ec == std::errc{}) {
                for (partial = value; partial && *partial;) {
                    if (*partial == ' ') {
                        partial++;
                        continue;
                    }
                    if (std::from_chars(partial, partial + 2, byte, 16).ec != std::errc{}) {
                        partial = NULL;
                        continue;
                    }
                    if (offset >= config->gs.bram.size()) {
                        partial = NULL;
                        continue;
                    }
                    config->gs.bram[offset++] = byte;
                }
            } else {
                partial = NULL;
            }
            if (partial == NULL) {
                fmt::print(stderr, "Invalid BRAM configuration {}={}\n", name, value);
                return 0;
            }
        } else if (strncmp(name, "gs.disk.", 8) == 0) {
            const char *partial = name + 8;
            auto driveType = ClemensDiskUtilities::getDriveType(partial);
            if (driveType == kClemensDrive_Invalid) {
                fmt::print(stderr, "Invalid DISK configuration {}={}\n", name, value);
                return 0;
            }
            config->gs.diskImagePaths[driveType] = value;
        } else if (strncmp(name, "gs.smart.", 9) == 0) {
            const char *partial = name + 9;
            unsigned driveIndex;
            if (std::from_chars(partial, partial + 1, driveIndex, 10).ec != std::errc{}) {
                fmt::print(stderr, "Invalid SmartPort configuration {}={}\n", name, value);
                return 0;
            }
            config->gs.smartPortImagePaths[driveIndex] = value;
        }
    }

    return 1;
}
