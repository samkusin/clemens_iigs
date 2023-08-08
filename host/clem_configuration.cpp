#include "clem_configuration.hpp"
#include "clem_host_platform.h"
#include "clem_shared.h"
#include "core/clem_apple2gs_config.hpp"
#include "core/clem_disk_utils.hpp"

#include "clem_disk.h"
#include "fmt/format.h"
#include "ini.h"
#include "spdlog/spdlog.h"

#include <charconv>
#include <cstdio>
#include <cstring>
#include <filesystem>

static constexpr unsigned kViewModeNameCount = 2;
static const char *kViewModeNames[kViewModeNameCount] = {"windowed", "fullscreen"};

//  For all platforms, the config file is guaranteed to be located in a
//  predefined location.  The config file is effectively our 'registry' to
//  use windows terminology.
//
ClemensConfiguration createConfiguration(const std::filesystem::path configDataDirectory,
                                         const std::filesystem::path defaultDataDirectory) {
    auto configPath = configDataDirectory / "config.ini";
    spdlog::info("Configuration created at {}", configPath.string());
    return ClemensConfiguration(configPath.string(), defaultDataDirectory.string());
}

ClemensConfiguration findConfiguration() {
    //  find the config file on the system

    //  local directory configuration check
    char localpath[CLEMENS_PATH_MAX];
    char datapath[CLEMENS_PATH_MAX];
    size_t localpathSize = sizeof(localpath);
    if (get_process_executable_path(localpath, &localpathSize)) {
        if (strnlen(localpath, CLEMENS_PATH_MAX - 1) >= sizeof(localpath) - 1) {
            //  If this is a problem, later code will determine whether the path
            //  was actually truncated
            spdlog::warn("Discovered configuration pathname is likely truncated!");
        }
        auto dataDirectory = std::filesystem::path(localpath).parent_path();
        auto configPath = dataDirectory / "config.ini";
        spdlog::info("Checking for configuration in {}", configPath.string());
        if (std::filesystem::exists(configPath)) {
            return createConfiguration(dataDirectory, dataDirectory);
        }
    } else {
        //  TODO: handle systems that support dynamic path sizes (i.e. localpathSize !=
        //  sizeof(localpath))
        spdlog::warn("Unable to obtain our local executable path. Falling back to user data paths");
    }
    if (!get_local_user_config_directory(localpath, sizeof(localpath), CLEM_HOST_COMPANY_NAME,
                                       CLEM_HOST_APPLICATION_NAME)) {
        spdlog::error("Unable to obtain the OS specific user config directory.");
        return ClemensConfiguration();
    }
    if (!get_local_user_data_directory(datapath, sizeof(datapath), CLEM_HOST_COMPANY_NAME,
                                       CLEM_HOST_APPLICATION_NAME)) {
        spdlog::error("Unable to obtain the OS specific user data directory.");
        return ClemensConfiguration();
    }
    return createConfiguration(std::filesystem::path(localpath), std::filesystem::path(datapath));
}

ClemensConfiguration::ClemensConfiguration()
    : majorVersion(0), minorVersion(0), logLevel(CLEM_DEBUG_LOG_INFO), viewMode(ViewMode::Windowed),
      poweredOn(false), hybridInterfaceEnabled(false), fastEmulationEnabled(true), isDirty(true) {
    gs.audioSamplesPerSecond = 0;
    gs.memory = CLEM_EMULATOR_RAM_DEFAULT;
    gs.cardNames[6] = kClemensCardHardDiskName;

    for (auto &joystickBinding : joystickBindings) {
        joystickBinding.axisAdj[0] = 0;
        joystickBinding.axisAdj[1] = 0;
        joystickBinding.button[0] = 0;
        joystickBinding.button[1] = 1;
    }
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
    viewMode = other.viewMode;
    logLevel = other.logLevel;
    poweredOn = other.poweredOn;
    joystickBindings = other.joystickBindings;

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
               "view={}\n"
               "logger={}\n"
               "power={}\n"
               "\n",
               majorVersion, minorVersion, dataDirectory, hybridInterfaceEnabled ? 1 : 0,
               kViewModeNames[static_cast<unsigned>(viewMode) % kViewModeNameCount], logLevel,
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
    for (unsigned i = 0; i < (unsigned)joystickBindings.size(); i++) {
        fmt::print(fp, "joystick.{}.adjX={}\n", i, joystickBindings[i].axisAdj[0]);
        fmt::print(fp, "joystick.{}.adjY={}\n", i, joystickBindings[i].axisAdj[1]);
        fmt::print(fp, "joystick.{}.btn0={}\n", i, joystickBindings[i].button[0]);
        fmt::print(fp, "joystick.{}.btn1={}\n", i, joystickBindings[i].button[1]);
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
        } else if (strncmp(name, "view", 16) == 0) {
            for (unsigned i = 0; i < kViewModeNameCount; ++i) {
                if (strncmp(value, kViewModeNames[i], 16) == 0) {
                    config->viewMode = static_cast<ViewMode>(i);
                }
            }
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
        } else if (strncmp(name, "gs.card.", 8) == 0) {
            const char *partial = name + 8;
            unsigned cardIndex;
            if (std::from_chars(partial, partial + 1, cardIndex, 10).ec != std::errc{}) {
                fmt::print(stderr, "Invalid Card configuration {}={}\n", name, value);
                return 0;
            }
            config->gs.cardNames[cardIndex] = value;
        } else if (strncmp(name, "joystick.", 9) == 0) {
            const char *partial = name + 9;
            if (partial[1] != '.') {
                fmt::print(stderr, "Invalid Joystick binding Id {}={}\n", name, value);
                return 0;
            }
            unsigned joyIndex;
            if (std::from_chars(partial, partial + 1, joyIndex, 10).ec != std::errc{}) {
                fmt::print(stderr, "Invalid Joystick binding index not found {}={}\n", name, value);
                return 0;
            } else if (joyIndex >= 2) {
                fmt::print(stderr, "Invalid Joystick binding index {}\n", joyIndex);
                return 0;
            }

            int valueInt;
            if (std::from_chars(value, value + strnlen(value, 4), valueInt, 10).ec != std::errc{}) {
                fmt::print(stderr, "Invalid Joystick binding index not found {}={}\n", name, value);
                return 0;
            }

            partial += 2;
            if (strncmp(partial, "adjX", 4) == 0) {
                config->joystickBindings[joyIndex].axisAdj[0] = valueInt;
            } else if (strncmp(partial, "adjY", 4) == 0) {
                config->joystickBindings[joyIndex].axisAdj[1] = valueInt;
            } else if (strncmp(partial, "btn0", 4) == 0) {
                config->joystickBindings[joyIndex].button[0] = valueInt;
            } else if (strncmp(partial, "btn1", 4) == 0) {
                config->joystickBindings[joyIndex].button[1] = valueInt;
            }
        }
    }

    return 1;
}
