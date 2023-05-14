#include "clem_configuration.hpp"
#include "clem_shared.h"
#include "core/clem_disk_utils.hpp"

#include "clem_disk.h"
#include "fmt/format.h"
#include "ini.h"
#include "spdlog/spdlog.h"

#include <charconv>
#include <cstdio>
#include <cstring>

ClemensConfiguration::ClemensConfiguration()
    : majorVersion(0), minorVersion(0), logLevel(CLEM_DEBUG_LOG_INFO), poweredOn(false),
      hybridInterfaceEnabled(false), fastEmulationEnabled(true), isDirty(true) {
    gs.audioSamplesPerSecond = 0;
    gs.memory = CLEM_EMULATOR_RAM_DEFAULT;
    gs.cardNames[3] = kClemensCardMockingboardName;
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
    logLevel = other.logLevel;
    poweredOn = other.poweredOn;

    gs = other.gs;

    hybridInterfaceEnabled = other.hybridInterfaceEnabled;
    fastEmulationEnabled = other.fastEmulationEnabled;
    isDirty = true;
}

bool ClemensConfiguration::save() {
    if (!isDirty)
        return true;

    FILE *fp = fopen(iniPathname.c_str(), "w");
    if (fp == NULL) {
        spdlog::error("ClemensConfiguration: failed to write {}", iniPathname);
        return false;
    }
    fmt::print(fp,
               "[host]\n"
               "major={}\n"
               "minor={}\n"
               "data={}\n"
               "hybrid={}\n"
               "logger={}\n"
               "power={}\n"
               "\n",
               majorVersion, minorVersion, dataDirectory, hybridInterfaceEnabled ? 1 : 0, logLevel,
               poweredOn ? 1 : 0);
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
    for (unsigned i = 0; i < (unsigned)gs.cardNames.size(); i++) {
        fmt::print(fp, "gs.card.{}={}\n", i, gs.cardNames[i]);
    }
    assert(gs.bram.size() == 256);
    for (unsigned i = 0; i < 256; i += 16) {
        fmt::print(fp,
                   "gs.bram{:02X}={:02X} {:02X} {:02X} {:02X} {:02X} {:02X} {:02X} {:02X} {:02X} "
                   "{:02X} {:02X} {:02X} {:02X} {:02X} {:02X} {:02X}\n",
                   i, gs.bram[i + 0x0], gs.bram[i + 0x1], gs.bram[i + 0x2], gs.bram[i + 0x3],
                   gs.bram[i + 0x4], gs.bram[i + 0x5], gs.bram[i + 0x6], gs.bram[i + 0x7],
                   gs.bram[i + 0x8], gs.bram[i + 0x9], gs.bram[i + 0xa], gs.bram[i + 0xb],
                   gs.bram[i + 0xc], gs.bram[i + 0xd], gs.bram[i + 0xe], gs.bram[i + 0xf]);
    }

    fclose(fp);

    spdlog::info("Configuration saved.");

    isDirty = false;

    return true;
}

void ClemensConfiguration::setDirty() { isDirty = true; }

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
            config->hybridInterfaceEnabled = atoi(value) > 0;
        } else if (strncmp(name, "logger", 16) == 0) {
            config->logLevel = atoi(value);
        } else if (strncmp(name, "power", 16) == 0) {
            config->poweredOn = atoi(value) > 0;
        }
    } else if (strncmp(section, "emulator", 16) == 0) {
        if (strncmp(name, "romfile", 16) == 0) {
            config->romFilename = value;
        } else if (strncmp(name, "fastiwm", 16) == 0) {
            config->fastEmulationEnabled = atoi(value) > 0;
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
                    partial += 2;
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
        } else if (strncmp(name, "gs.card.", 9) == 0) {
            const char *partial = name + 9;
            unsigned cardIndex;
            if (std::from_chars(partial, partial + 1, cardIndex, 10).ec != std::errc{}) {
                fmt::print(stderr, "Invalid Card configuration {}={}\n", name, value);
                return 0;
            }
            config->gs.cardNames[cardIndex] = value;
        }
    }

    return 1;
}
