#ifndef CLEM_HOST_APPLE2GS_CONFIG_HPP
#define CLEM_HOST_APPLE2GS_CONFIG_HPP

#include "clem_disk_status.hpp"
#include "clem_mmio_types.h"

#include <array>
#include <string>

constexpr const char *kClemensCardMockingboardName = "mockingboard_c";

struct ClemensAppleIIGSConfig {
    //  RAM in Kilobytes, not counting Mega 2 memory
    unsigned memory = 0;
    //  Usually 48000 or equivalent to the target mix rate
    unsigned audioSamplesPerSecond = 0;
    //  Baterry RAM as laid out on the IIGS
    std::array<uint8_t, CLEM_RTC_BRAM_SIZE> bram;
    //  Drive images (can be empty)
    std::array<std::string, kClemensDrive_Count> diskImagePaths;
    std::array<std::string, CLEM_SMARTPORT_DRIVE_LIMIT> smartPortImagePaths;
    //  Card namaes
    std::array<std::string, CLEM_CARD_SLOT_COUNT> cardNames;
};

struct ClemensAppleIIGSFrame {
    ClemensMonitor monitor;
    ClemensVideo graphics;
    ClemensVideo text;
    ClemensAudio audio;
    std::array<ClemensDiskDriveStatus, kClemensDrive_Count> diskDriveStatuses;
    std::array<ClemensDiskDriveStatus, CLEM_SMARTPORT_DRIVE_LIMIT> smartPortStatuses;
};

#endif
