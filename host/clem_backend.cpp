#include "clem_backend.hpp"
#include "clem_disk_utils.hpp"
#include "clem_host_platform.h"
#include "clem_mem.h"
#include "clem_program_trace.hpp"
#include "clem_serializer.hpp"
#include "emulator.h"
#include "emulator_mmio.h"
#include "iocards/mockingboard.h"

#include <charconv>
#include <chrono>
#include <cstdarg>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <optional>

#include "cinek/circular_buffer.hpp"
#include "fmt/format.h"

static constexpr unsigned kSlabMemorySize = 32 * 1024 * 1024;
static constexpr unsigned kLogOutputLineLimit = 1024;

struct ClemensRunSampler {
    //  our method of keeping the simulation in sync with real time is to rely
    //  on two counters
    //    - sDT = simulation counter that ticks at a fixed rate, and
    //    - aDT = real-time timer that counts the actual time passed
    //
    //  if aDT < sDT, then sleep the amount of time needed to catch up with sDT
    //  if aDT >= sDT, then clamp aDT to sDT
    std::chrono::microseconds fixedTimeInterval;
    std::chrono::microseconds actualTimeInterval;
    std::chrono::microseconds sampledFrameTime;

    double sampledFramesPerSecond;

    cinek::CircularBuffer<std::chrono::microseconds, 120> frameTimeBuffer;

    clem_clocks_time_t sampledClocksSpent;
    uint64_t sampledCyclesSpent;
    cinek::CircularBuffer<clem_clocks_duration_t, 120> clocksBuffer;
    cinek::CircularBuffer<clem_clocks_duration_t, 120> cyclesBuffer;

    double sampledEmulatorSpeedMhz;

    ClemensRunSampler() { reset(); }

    void reset() {
        fixedTimeInterval = std::chrono::microseconds::zero();
        actualTimeInterval = std::chrono::microseconds::zero();
        sampledFrameTime = std::chrono::microseconds::zero();
        sampledClocksSpent = 0;
        sampledCyclesSpent = 0;
        sampledFramesPerSecond = 0.0f;
        sampledEmulatorSpeedMhz = 0.0f;
        frameTimeBuffer.clear();
        clocksBuffer.clear();
        cyclesBuffer.clear();
    }

    void update(std::chrono::microseconds fixedFrameInterval,
                std::chrono::microseconds actualFrameInterval, clem_clocks_duration_t clocksSpent,
                unsigned cyclesSpent) {
        fixedTimeInterval += fixedFrameInterval;
        actualTimeInterval += actualFrameInterval;
        //  see notes at the head of this class for how delta times are calculated
        //  in an attempt to maintain a fixed frame rate on systems where sleep()
        //  delays can overshoot the desired sleep time (Windows especially.)
        if (actualTimeInterval < fixedTimeInterval) {
            std::this_thread::sleep_for(fixedTimeInterval - actualTimeInterval);
            fixedTimeInterval -= actualTimeInterval;
            actualTimeInterval = std::chrono::microseconds::zero();
        } else {
            std::this_thread::yield();
            if (actualTimeInterval - fixedFrameInterval > std::chrono::microseconds(500000)) {
                actualTimeInterval = fixedTimeInterval = std::chrono::microseconds::zero();
            }
        }

        if (frameTimeBuffer.isFull()) {
            decltype(frameTimeBuffer)::ValueType lruFrametime;
            frameTimeBuffer.pop(lruFrametime);
            sampledFrameTime -= lruFrametime;
        }
        frameTimeBuffer.push(actualFrameInterval);
        sampledFrameTime += actualFrameInterval;

        if (sampledFrameTime >= std::chrono::microseconds(100000)) {
            sampledFramesPerSecond = frameTimeBuffer.size() * 1e6 / sampledFrameTime.count();
        }

        //  calculate emulator speed by using cycles_spent * CLEM_CLOCKS_MEGA2_CYCLE
        //  as a reference for 1.023mhz where
        //    reference_clocks = cycles_spent * CLEM_CLOCKS_MEGA2_CYCLE
        //    acutal_clocks = sampledClocksSpent
        //    (reference / actual) * 1.023mhz is the emulator speed
        if (clocksBuffer.isFull()) {
            decltype(clocksBuffer)::ValueType lruClocksSpent = 0;
            clocksBuffer.pop(lruClocksSpent);
            sampledClocksSpent -= lruClocksSpent;
        }
        clocksBuffer.push(clocksSpent);
        sampledClocksSpent += clocksSpent;

        if (cyclesBuffer.isFull()) {
            decltype(cyclesBuffer)::ValueType lruCycles = 0;
            cyclesBuffer.pop(lruCycles);
            sampledCyclesSpent -= lruCycles;
        }
        cyclesBuffer.push(cyclesSpent);
        sampledCyclesSpent += cyclesSpent;
        if (sampledClocksSpent > (CLEM_CLOCKS_MEGA2_CYCLE * CLEM_MEGA2_CYCLES_PER_SECOND / 10)) {
            sampledEmulatorSpeedMhz =
                1.023 * double(CLEM_CLOCKS_MEGA2_CYCLE * sampledCyclesSpent) / sampledClocksSpent;
        }
    }
};

template <typename... Args>
void ClemensBackend::localLog(int log_level, const char *msg, Args... args) {
    if (logOutput_.size() >= kLogOutputLineLimit)
        return;
    ClemensBackendOutputText logLine{
        log_level,
    };
    logLine.text = fmt::format(msg, args...);
    logOutput_.emplace_back(logLine);
}

ClemensBackend::ClemensBackend(std::string romPathname, const Config &config,
                               PublishStateDelegate publishDelegate)
    : config_(config), slabMemory_(kSlabMemorySize, malloc(kSlabMemorySize)),
      breakpoints_(std::move(config_.breakpoints)), logLevel_(CLEM_DEBUG_LOG_INFO),
      debugMemoryPage_(0x00), areInstructionsLogged_(false) {

    diskContainers_.fill(ClemensWOZDisk{});
    diskDrives_.fill(ClemensBackendDiskDriveState{});
    smartPortDrives_.fill(ClemensBackendDiskDriveState{});

    loggedInstructions_.reserve(10000);

    initEmulatedDiskLocalStorage();

    romBuffer_ = loadROM(romPathname.c_str());
    if (romBuffer_.isEmpty()) {
        //  TODO: load a dummy ROM, for now an empty buffer
        romBuffer_ = cinek::ByteBuffer((uint8_t *)slabMemory_.allocate(CLEM_IIGS_BANK_SIZE),
                                       CLEM_IIGS_BANK_SIZE);
        memset(romBuffer_.forwardSize(CLEM_IIGS_BANK_SIZE).first, 0, CLEM_IIGS_BANK_SIZE);
    }

    memset(&machine_, 0, sizeof(machine_));
    memset(&mmio_, 0, sizeof(mmio_));
    clemens_host_setup(&machine_, &ClemensBackend::emulatorLog, this);

    switch (config_.type) {
    case ClemensBackendConfig::Type::Apple2GS:
        initApple2GS();
        break;
    }

    //  TODO: Only use this when opcode debugging is enabled to save the no-op
    //        callback overhead
    clemens_opcode_callback(&machine_, &ClemensBackend::emulatorOpcodeCallback);

    for (size_t driveIndex = 0; driveIndex < diskDrives_.size(); ++driveIndex) {
        if (diskDrives_[driveIndex].imagePath.empty())
            continue;
        auto driveType = static_cast<ClemensDriveType>(driveIndex);
        if (!loadDisk(driveType, false)) {
            fmt::print("Failed to load image '{}' into drive {}\n",
                       diskDrives_[driveIndex].imagePath,
                       ClemensDiskUtilities::getDriveName(driveType));
        } else {
            fmt::print("Loaded image '{}' into drive {}\n", diskDrives_[driveIndex].imagePath,
                       ClemensDiskUtilities::getDriveName(driveType));
        }
    }

    for (size_t driveIndex = 0; driveIndex < smartPortDrives_.size(); ++driveIndex) {
        if (smartPortDrives_[driveIndex].imagePath.empty())
            continue;
        loadSmartPortDisk(driveIndex);
        fmt::print("Loaded SmartPort image '{}' into drive {}\n",
                   smartPortDrives_[driveIndex].imagePath, driveIndex);
    }

    //  Everything is ready for the main thread
    runner_ = std::thread(&ClemensBackend::main, this, std::move(publishDelegate));
}

ClemensBackend::~ClemensBackend() {
    terminate();
    runner_.join();

    free(slabMemory_.getHead());
}

void ClemensBackend::terminate() { queueToFront(Command{Command::Terminate}); }

void ClemensBackend::reset() { queue(Command{Command::ResetMachine}); }

void ClemensBackend::resetMachine() {
    machine_.cpu.pins.resbIn = false;
    machine_.resb_counter = 3;
}

void ClemensBackend::setRefreshFrequency(unsigned hz) {
    queue(Command{Command::SetHostUpdateFrequency, fmt::format("{}", hz)});
}

void ClemensBackend::run() { queue(Command{Command::RunMachine}); }

void ClemensBackend::step(unsigned count) {
    queue(Command{Command::StepMachine, fmt::format("{}", count)});
}

unsigned ClemensBackend::stepMachine(const std::string_view &inputParam) {
    unsigned count;
    if (std::from_chars(inputParam.data(), inputParam.data() + inputParam.size(), count).ec !=
        std::errc{}) {
        return 0;
    }
    return count;
}

void ClemensBackend::publish() { queue(Command{Command::Publish}); }

void ClemensBackend::insertDisk(ClemensDriveType driveType, std::string diskPath) {
    queue(Command{Command::InsertDisk,
                  fmt::format("{}={}", ClemensDiskUtilities::getDriveName(driveType), diskPath)});
}

bool ClemensBackend::insertDisk(const std::string_view &inputParam, bool allowBlank) {
    auto sepPos = inputParam.find('=');
    if (sepPos == std::string_view::npos)
        return false;
    auto driveName = inputParam.substr(0, sepPos);
    auto imagePath = inputParam.substr(sepPos + 1);
    auto driveType = ClemensDiskUtilities::getDriveType(driveName);
    if (driveType == kClemensDrive_Invalid)
        return false;

    diskDrives_[driveType].imagePath = imagePath;
    return loadDisk(driveType, allowBlank);
}

void ClemensBackend::insertBlankDisk(ClemensDriveType driveType, std::string diskPath) {
    queue(Command{Command::InsertBlankDisk,
                  fmt::format("{}={}", ClemensDiskUtilities::getDriveName(driveType), diskPath)});
}

void ClemensBackend::ejectDisk(ClemensDriveType driveType) {
    queue(Command{Command::EjectDisk, std::string(ClemensDiskUtilities::getDriveName(driveType))});
}

void ClemensBackend::ejectDisk(const std::string_view &inputParam) {
    auto driveType = ClemensDiskUtilities::getDriveType(inputParam);
    diskDrives_[driveType].isEjecting = true;
}

void ClemensBackend::writeProtectDisk(ClemensDriveType driveType, bool wp) {
    queue(Command{Command::WriteProtectDisk,
                  fmt::format("{},{}", ClemensDiskUtilities::getDriveName(driveType), wp ? 1 : 0)});
}

bool ClemensBackend::writeProtectDisk(const std::string_view &inputParam) {
    auto sepPos = inputParam.find(',');
    if (sepPos == std::string_view::npos)
        return false;
    auto driveParam = inputParam.substr(0, sepPos);
    auto driveType = ClemensDiskUtilities::getDriveType(driveParam);
    auto *drive = clemens_drive_get(&mmio_, driveType);
    if (!drive || !drive->has_disk)
        return false;

    auto enableParam = inputParam.substr(sepPos + 1);
    drive->disk.is_write_protected = enableParam == "1";

    return true;
}

void ClemensBackend::debugMemoryPage(uint8_t page) {
    queue(Command{Command::DebugMemoryPage, std::to_string(page)});
}

void ClemensBackend::debugMemoryWrite(uint16_t addr, uint8_t value) {
    queue(Command{Command::WriteMemory, fmt::format("{}={}", addr, value)});
}

void ClemensBackend::writeMemory(const std::string_view &inputParam) {
    auto sepPos = inputParam.find('=');
    if (sepPos == std::string_view::npos) {
        return;
    }
    uint16_t addr;
    if (std::from_chars(inputParam.data(), inputParam.data() + sepPos, addr).ec != std::errc{})
        return;
    auto valStr = inputParam.substr(sepPos + 1);
    uint8_t value;
    if (std::from_chars(valStr.data(), valStr.data() + valStr.size(), value).ec != std::errc{})
        return;

    clem_write(&machine_, value, addr, debugMemoryPage_, CLEM_MEM_FLAG_NULL);
}

void ClemensBackend::debugLogLevel(int logLevel) {
    queue(Command{Command::DebugLogLevel, fmt::format("{}", logLevel)});
}

void ClemensBackend::debugMessage(std::string msg) {
    queue(Command{Command::DebugMessage, std::move(msg)});
}

void ClemensBackend::debugProgramTrace(std::string op, std::string path) {
    queue(Command{Command::DebugProgramTrace,
                  fmt::format("{},{}", op, path.empty() ? "#" : path.c_str())});
}

bool ClemensBackend::programTrace(const std::string_view &inputParam) {
    auto sepPos = inputParam.find(',');
    auto op = inputParam.substr(0, sepPos);
    std::string_view param;
    if (sepPos != std::string_view::npos) {
        param = inputParam.substr(sepPos + 1);
        if (param == "#")
            param = std::string_view();
    } else {
        param = std::string_view();
    }
    auto path = param;
    if (programTrace_ == nullptr && op == "on") {
        nextTraceSeq_ = 0;
        programTrace_ = std::make_unique<ClemensProgramTrace>();
        programTrace_->enableToolboxLogging(true);
        fmt::print("Program trace enabled\n");
        return true;
    }
    bool ok = true;
    if (programTrace_ != nullptr && !path.empty()) {
        //  save if a path was supplied
        auto exportPath = std::filesystem::path(CLEM_HOST_TRACES_DIR) / path;
        ok = programTrace_->exportTrace(exportPath.string().c_str());
        if (ok) {
            programTrace_->reset();
            fmt::print("Exported program trace to '{}'.\n", exportPath.string());
        } else {
            fmt::print("ERROR: failed to export program trace to '{}'.  Trace not cleared.\n",
                       exportPath.string());
        }
    }
    if (programTrace_ != nullptr && op == "off") {
        fmt::print("Program trace disabled\n");
        programTrace_ = nullptr;
    }
    if (programTrace_) {
        if (op == "iwm") {
            programTrace_->enableIWMLogging(!programTrace_->isIWMLoggingEnabled());
            fmt::print("{} tracing = {}\n", op, programTrace_->isIWMLoggingEnabled());
        } else {
            fmt::print("{} tracing is not recognized.\n", op);
        }
    }
    return ok;
}

void ClemensBackend::saveMachine(std::string path) {
    queue(Command{Command::SaveMachine, std::move(path)});
}

bool ClemensBackend::saveSnapshot(const std::string_view &inputParam) {
    auto outputPath = std::filesystem::path(CLEM_HOST_SNAPSHOT_DIR) / inputParam;
    return ClemensSerializer::save(outputPath.string(), &machine_, &mmio_, diskContainers_.size(),
                                   diskContainers_.data(), diskDrives_.data(),
                                   CLEM_SMARTPORT_DRIVE_LIMIT, smartPortDisks_.data(),
                                   smartPortDrives_.data(), breakpoints_);
}

void ClemensBackend::loadMachine(std::string path) {
    queue(Command{Command::LoadMachine, std::move(path)});
}

bool ClemensBackend::loadSnapshot(const std::string_view &inputParam) {
    auto outputPath = std::filesystem::path(CLEM_HOST_SNAPSHOT_DIR) / inputParam;
    bool res = ClemensSerializer::load(
        outputPath.string(), &machine_, &mmio_, diskContainers_.size(), diskContainers_.data(),
        diskDrives_.data(), CLEM_SMARTPORT_DRIVE_LIMIT, smartPortDisks_.data(),
        smartPortDrives_.data(), breakpoints_, &ClemensBackend::unserializeAllocate, this);
    saveBRAM();
    return res;
}

//  TODO: Move into Clemens API clemens_mmio_find_card_name()
static ClemensCard *findMockingboardCard(ClemensMMIO *mmio) {
    for (int cardIdx = 0; cardIdx < 7; ++cardIdx) {
        if (mmio->card_slot[cardIdx]) {
            const char *cardName =
                mmio->card_slot[cardIdx]->io_name(mmio->card_slot[cardIdx]->context);
            if (!strcmp(cardName, kClemensCardMockingboardName)) {
                return mmio->card_slot[cardIdx];
            }
        }
    }
    return NULL;
}

uint8_t *ClemensBackend::unserializeAllocate(unsigned sz, void *context) {
    //  TODO: allocation from a slab that doesn't reset may cause problems if the
    //        snapshots require allocation per load - take a look at how to fix
    //        this (like a separate slab for data that is unserialized requiring
    //        allocation like memory banks, mix buffers, etc (see serializer.h))
    auto *host = reinterpret_cast<ClemensBackend *>(context);
    return (uint8_t *)host->slabMemory_.allocate(sz);
}

bool ClemensBackend::loadDisk(ClemensDriveType driveType, bool allowBlank) {
    diskBuffer_.reset();
    auto imagePath =
        std::filesystem::path(config_.diskLibraryRootPath) / diskDrives_[driveType].imagePath;
    std::ifstream input(imagePath, std::ios_base::in | std::ios_base::binary);
    if (input.is_open()) {
        input.seekg(0, std::ios_base::end);
        size_t inputImageSize = input.tellg();
        if (inputImageSize <= (size_t)diskBuffer_.getCapacity()) {
            auto bits = diskBuffer_.forwardSize(inputImageSize);
            input.seekg(0);
            input.read((char *)bits.first, inputImageSize);
            if (input.good()) {
                diskContainers_[driveType].nib = &disks_[driveType];
                auto parseBuffer = cinek::ConstCastRange<uint8_t>(bits);
                if (ClemensDiskUtilities::parseWOZ(&diskContainers_[driveType], parseBuffer)) {
                    if (clemens_assign_disk(&mmio_, driveType, &disks_[driveType])) {
                        return true;
                    }
                }
            }
        }
    } else if (allowBlank) {
        resetDisk(driveType);
        if (ClemensDiskUtilities::createWOZ(&diskContainers_[driveType], &disks_[driveType])) {
            if (clemens_assign_disk(&mmio_, driveType, &disks_[driveType])) {
                return true;
            }
        }
        return true;
    }
    diskDrives_[driveType].imagePath.clear();
    return false;
}

bool ClemensBackend::saveDisk(ClemensDriveType driveType) {
    diskBuffer_.reset();
    auto writeOut = diskBuffer_.forwardSize(diskBuffer_.getCapacity());

    size_t writeOutCount = cinek::length(writeOut);
    diskContainers_[driveType].nib = &disks_[driveType];
    if (!clem_woz_serialize(&diskContainers_[driveType], writeOut.first, &writeOutCount)) {
        return false;
    }

    auto imagePath =
        std::filesystem::path(config_.diskLibraryRootPath) / diskDrives_[driveType].imagePath;

    std::ofstream out(imagePath, std::ios_base::out | std::ios_base::binary);
    if (out.fail())
        return false;
    out.write((char *)writeOut.first, writeOutCount);
    if (out.fail() || out.bad())
        return false;
    return true;
}

void ClemensBackend::resetDisk(ClemensDriveType driveType) {
    auto &nib = disks_[driveType];
    unsigned max_track_size_bytes = 0, track_byte_offset = 0;

    auto trackDataSize = nib.bits_data_end - nib.bits_data;
    memset(nib.bits_data, 0, trackDataSize);
    nib.is_write_protected = false;
    nib.is_double_sided = false;

    switch (driveType) {
    case kClemensDrive_3_5_D1:
    case kClemensDrive_3_5_D2:
        nib.disk_type = CLEM_DISK_TYPE_3_5;
        nib.is_double_sided = true; // TODO: an option?
        nib.bit_timing_ns = 2000;
        nib.track_count = nib.is_double_sided ? 160 : 80;
        for (unsigned i = 0; i < CLEM_DISK_LIMIT_QTR_TRACKS; ++i) {
            if (nib.is_double_sided) {
                nib.meta_track_map[i] = i;
            } else if ((i % 2) == 0) {
                nib.meta_track_map[i] = (i / 2);
            } else {
                nib.meta_track_map[i] = 0xff;
            }
        }
        track_byte_offset = 0;
        for (unsigned region_index = 0; region_index < 5; ++region_index) {
            unsigned bits_cnt = CLEM_DISK_35_CALC_BYTES_FROM_SECTORS(
                                    g_clem_max_sectors_per_region_35[region_index]) *
                                8;
            max_track_size_bytes = bits_cnt / 8;
            for (unsigned i = g_clem_track_start_per_region_35[region_index];
                 i < g_clem_track_start_per_region_35[region_index + 1];) {
                unsigned track_index = i;
                if (!nib.is_double_sided) {
                    track_index /= 2;
                    i += 2;
                } else {
                    i += 1;
                }
                nib.track_byte_offset[track_index] = track_byte_offset;
                nib.track_byte_count[track_index] = max_track_size_bytes;
                nib.track_bits_count[track_index] = bits_cnt;
                nib.track_initialized[track_index] = 0;
                track_byte_offset += max_track_size_bytes;
            }
        }
        break;

    case kClemensDrive_5_25_D1:
    case kClemensDrive_5_25_D2:
        nib.disk_type = CLEM_DISK_TYPE_5_25;
        nib.bit_timing_ns = 4000;
        nib.track_count = 35;
        max_track_size_bytes = CLEM_DISK_525_BYTES_PER_TRACK;
        for (unsigned i = 0; i < CLEM_DISK_LIMIT_QTR_TRACKS; ++i) {
            if ((i % 4) == 0 || (i % 4) == 1) {
                nib.meta_track_map[i] = (i / 4);
            } else {
                nib.meta_track_map[i] = 0xff;
            }
        }
        track_byte_offset = 0;
        for (unsigned i = 0; i < nib.track_count; ++i) {
            nib.track_byte_offset[i] = track_byte_offset;
            nib.track_byte_count[i] = max_track_size_bytes;
            nib.track_bits_count[i] = CLEM_DISK_BLANK_TRACK_BIT_LENGTH_525;
            nib.track_initialized[i] = 0;
            track_byte_offset += max_track_size_bytes;
        }
        break;

    default:
        assert(false);
        break;
    }
}

void ClemensBackend::loadSmartPortDisk(unsigned driveIndex) {
    //  load into our HDD slot
    auto imagePath =
        std::filesystem::path(config_.diskLibraryRootPath) / smartPortDrives_[driveIndex].imagePath;
    std::ifstream input(imagePath, std::ios_base::in | std::ios_base::binary);
    if (input.is_open()) {
        auto sz = input.seekg(0, std::ios_base::end).tellg();
        std::vector<uint8_t> buffer(sz);
        input.seekg(0);
        input.read((char *)buffer.data(), sz);
        smartPortDisks_[driveIndex] = ClemensSmartPortDisk(std::move(buffer));
    } else {
        smartPortDisks_[driveIndex] =
            ClemensSmartPortDisk(std::move(ClemensSmartPortDisk::createData(8192)));
    }
    ClemensSmartPortDevice device;
    clemens_assign_smartport_disk(&mmio_, driveIndex,
                                  smartPortDisks_[driveIndex].createSmartPortDevice(&device));
}

bool ClemensBackend::saveSmartPortDisk(unsigned driveIndex) {
    auto &drive = smartPortDrives_[driveIndex];
    auto &disk = smartPortDisks_[driveIndex].getDisk();
    auto imagePath = std::filesystem::path(config_.diskLibraryRootPath) / drive.imagePath;
    std::ofstream out(imagePath, std::ios_base::out | std::ios_base::binary);
    if (out.fail())
        return false;
    out.write((char *)disk.image_buffer, disk.image_buffer_length);
    if (out.fail() || out.bad())
        return false;
    return true;
}

static const char *sInputKeys[] = {"",      "keyD", "keyU",   "mouseD", "mouseU",
                                   "mouse", "padl", "nopadl", NULL};

void ClemensBackend::inputEvent(const ClemensInputEvent &input) {
    CK_ASSERT_RETURN(*sInputKeys[input.type] != '\0');
    Command cmd{Command::Input};
    if (input.type == kClemensInputType_Paddle ||
        input.type == kClemensInputType_PaddleDisconnected) {
        cmd.operand = fmt::format("{}={},{},{}", sInputKeys[input.type], input.value_a,
                                  input.value_b, input.gameport_button_mask);
    } else {
        cmd.operand = fmt::format("{}={},{},{}", sInputKeys[input.type], input.value_a,
                                  input.value_b, input.adb_key_toggle_mask);
    }
    queue(cmd);
}

void ClemensBackend::inputMachine(const std::string_view &inputParam) {
    if (!clemens_is_initialized_simple(&machine_)) {
        return;
    }
    auto equalsTokenPos = inputParam.find('=');
    auto name = inputParam.substr(0, equalsTokenPos);
    auto value = inputParam.substr(equalsTokenPos + 1);
    for (const char **keyName = &sInputKeys[0]; *keyName != NULL; ++keyName) {
        if (name == *keyName) {
            ClemensInputEvent inputEvent{};
            inputEvent.type = (ClemensInputType)((int)(keyName - &sInputKeys[0]));
            //  oh why, oh why no straightforward C++ conversion from std::string_view
            //  to number
            auto commaPos = value.find(',');
            auto inputValueA = value.substr(0, commaPos);
            inputEvent.value_a = (int16_t)std::stol(std::string(inputValueA));
            if (commaPos != std::string_view::npos) {
                std::string_view inputModifiers;
                auto inputValueB = value.substr(commaPos + 1);
                commaPos = inputValueB.find(',');
                if (commaPos != std::string_view::npos) {
                    inputModifiers = inputValueB.substr(commaPos + 1);
                    inputValueB = inputValueB.substr(0, commaPos);
                }
                inputEvent.value_b = (int16_t)std::stol(std::string(inputValueB));
                if (inputEvent.type == kClemensInputType_Paddle ||
                    inputEvent.type == kClemensInputType_PaddleDisconnected) {
                    inputEvent.gameport_button_mask = std::stoul(std::string(inputModifiers));
                } else {
                    inputEvent.adb_key_toggle_mask = std::stoul(std::string(inputModifiers));
                }
            }
            clemens_input(&mmio_, &inputEvent);
        }
    }
}

void ClemensBackend::breakExecution() { queue(Command{Command::Break}); }

void ClemensBackend::addBreakpoint(const ClemensBackendBreakpoint &breakpoint) {
    Command cmd;
    cmd.operand = fmt::format("{:s}:{:06X}",
                              breakpoint.type == ClemensBackendBreakpoint::DataRead ? "r"
                              : breakpoint.type == ClemensBackendBreakpoint::Write  ? "w"
                              : breakpoint.type == ClemensBackendBreakpoint::IRQ    ? "i"
                                                                                    : "",
                              breakpoint.address);
    cmd.type = Command::AddBreakpoint;
    queue(std::move(cmd));
}

bool ClemensBackend::addBreakpoint(const std::string_view &inputParam) {
    auto sepPos = inputParam.find(':');
    assert(sepPos != std::string_view::npos);
    auto type = inputParam.substr(0, sepPos);
    auto address = inputParam.substr(sepPos + 1);
    ClemensBackendBreakpoint bp;
    if (type == "r") {
        bp.type = ClemensBackendBreakpoint::DataRead;
    } else if (type == "w") {
        bp.type = ClemensBackendBreakpoint::Write;
    } else if (type == "i") {
        bp.type = ClemensBackendBreakpoint::IRQ;
    } else {
        bp.type = ClemensBackendBreakpoint::Execute;
    }
    bp.address = (uint32_t)std::stoul(std::string(address), nullptr, 16);
    auto range = std::equal_range(
        breakpoints_.begin(), breakpoints_.end(), bp,
        [](const ClemensBackendBreakpoint &bp, const ClemensBackendBreakpoint &newBp) {
            return bp.address < newBp.address;
        });
    while (range.first != range.second) {
        if (range.first->type == bp.type) {
            break;
        }
        range.first++;
    }
    if (range.first == range.second) {
        breakpoints_.emplace(range.second, bp);
    }
    return true;
}

void ClemensBackend::removeBreakpoint(unsigned index) {
    queue(Command{Command::DelBreakpoint, std::to_string(index)});
}

bool ClemensBackend::delBreakpoint(const std::string_view &inputParam) {
    if (inputParam.empty()) {
        breakpoints_.clear();
        return true;
    }
    unsigned index = (unsigned)std::stoul(std::string(inputParam));
    if (index >= breakpoints_.size()) {
        return false;
    }
    breakpoints_.erase(breakpoints_.begin() + index);
    return true;
}

void ClemensBackend::queue(const Command &cmd) {
    {
        std::lock_guard<std::mutex> queuelock(commandQueueMutex_);
        commandQueue_.emplace_back(cmd);
    }
    commandQueueCondition_.notify_one();
}

void ClemensBackend::queueToFront(const Command &cmd) {
    {
        std::lock_guard<std::mutex> queuelock(commandQueueMutex_);
        commandQueue_.emplace_front(cmd);
    }
    commandQueueCondition_.notify_one();
}

static int64_t calculateClocksPerTimeslice(ClemensMMIO *mmio, unsigned hz) {
    bool is_machine_slow;
    return int64_t(clemens_clocks_per_second(mmio, &is_machine_slow) / hz);
}

#if defined(__GNUC__)
//  Despite guarding with std::optional<>::has_value(), annoying GCC warning
//  that I may be accessing an uninitialized optional with a folloing value
//  access (if has_value() returns true) - which is the accepted method
//  or checking optionals if exception handling is turned off
//  https://gcc.gnu.org/bugzilla/show_bug.cgi?id=80635
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wmaybe-uninitialized"
#endif

void ClemensBackend::main(PublishStateDelegate publishDelegate) {
    int64_t clocksRemainingInTimeslice = 0;
    std::optional<int> stepsRemaining = 0;
    bool isTerminated = false;
    bool isMachineReady = false;

    ClemensRunSampler runSampler;

    //  TODO: clemens API clemens_mmio_card_insert()
    for (size_t cardIdx = 0; cardIdx < config_.cardNames.size(); ++cardIdx) {
        auto &cardName = config_.cardNames[cardIdx];
        if (!cardName.empty()) {
            mmio_.card_slot[cardIdx] = createCard(cardName.c_str());
        } else {
            mmio_.card_slot[cardIdx] = NULL;
        }
    }

    fmt::print("Starting backend thread.\n");

    ClemensCard *mockingboard = findMockingboardCard(&mmio_);
    uint64_t publishSeqNo = 0;
    unsigned emulatorRefreshFrequency = 60;
    auto fixedFrameInterval =
        std::chrono::microseconds((long)std::floor(1e6 / emulatorRefreshFrequency));
    auto lastFrameTimePoint = std::chrono::high_resolution_clock::now();
    std::optional<unsigned> hitBreakpoint;
    std::optional<bool> commandFailed;
    std::optional<Command::Type> commandType;
    std::optional<std::string> debugMessage;

    while (!isTerminated) {
        bool isRunning = !stepsRemaining.has_value() || *stepsRemaining > 0;
        bool publishState = false;
        bool updateSeqNo = false;

        std::unique_lock<std::mutex> queuelock(commandQueueMutex_);
        if (!isRunning) {
            //  waiting for commands
            commandQueueCondition_.wait(queuelock, [this] { return !commandQueue_.empty(); });
        }
        //  TODO: we may just be able to use a vector for the command queue and
        //        create a local copy of the queue to minimize time spent executing
        //        commands.   the mutex seems to be used just for adds to the
        //        command queue.
        while (!commandQueue_.empty() && !isTerminated) {
            auto command = commandQueue_.front();
            commandQueue_.pop_front();
            if (command.type != Command::Publish && command.type != Command::Input) {
                if (!commandFailed.has_value()) {
                    commandFailed = false;
                }
            }
            switch (command.type) {
            case Command::Terminate:
                isTerminated = true;
                break;
            case Command::ResetMachine:
                resetMachine();
                isMachineReady = true;
                mockingboard = findMockingboardCard(&mmio_);
                break;
            case Command::SetHostUpdateFrequency:
                emulatorRefreshFrequency = std::stoul(command.operand);
                if (emulatorRefreshFrequency) {
                    fixedFrameInterval =
                        std::chrono::microseconds((long)std::floor(1e6 / emulatorRefreshFrequency));
                } else {
                    fixedFrameInterval = std::chrono::microseconds::zero();
                }
                break;
            case Command::RunMachine:
                stepsRemaining = std::nullopt;
                isRunning = true;
                runSampler.reset();
                break;
            case Command::StepMachine:
                *stepsRemaining = stepMachine(command.operand);
                isRunning = true;
                runSampler.reset();
                break;
            case Command::Publish:
                publishState = true;
                break;
            case Command::InsertDisk:
                if (!insertDisk(command.operand, false))
                    commandFailed = true;
                break;
            case Command::InsertBlankDisk:
                if (!insertDisk(command.operand, true))
                    commandFailed = true;
                break;
            case Command::EjectDisk:
                ejectDisk(command.operand);
                break;
            case Command::WriteProtectDisk:
                writeProtectDisk(command.operand);
                break;
            case Command::Input:
                inputMachine(command.operand);
                break;
            case Command::Break:
                stepsRemaining = 0;
                break;
            case Command::AddBreakpoint:
                if (!addBreakpoint(command.operand))
                    commandFailed = true;
                break;
            case Command::DelBreakpoint:
                if (!delBreakpoint(command.operand))
                    commandFailed = true;
                break;
            case Command::DebugMemoryPage:
                debugMemoryPage_ = (uint8_t)(std::stoul(command.operand));
                break;
            case Command::WriteMemory:
                writeMemory(command.operand);
                break;
            case Command::DebugLogLevel:
                logLevel_ = (int)(std::stol(command.operand));
                break;
            case Command::DebugMessage:
                debugMessage = std::move(command.operand);
                break;
            case Command::DebugProgramTrace:
                if (!programTrace(command.operand))
                    commandFailed = true;
                break;
            case Command::SaveMachine:
                if (!saveSnapshot(command.operand))
                    commandFailed = true;
                break;
            case Command::LoadMachine:
                if (loadSnapshot(command.operand)) {
                    mockingboard = findMockingboardCard(&mmio_);
                } else {
                    //  TODO: this should force a restart - hopefully the
                    //        frontend will save the current state prior to loading
                    //        a new one to avoid data loss.
                    commandFailed = true;
                }
                break;
            case Command::Undefined:
                break;
            }
            if (commandFailed.has_value() && *commandFailed == true && !commandType.has_value()) {
                commandType = command.type;
            }
        }
        queuelock.unlock();

        //  TODO: these edge cases seem sloppy - but we'll need to prevent the
        //        thread from spinning if the machine will not run
        if (!isMachineReady || emulatorRefreshFrequency == 0) {
            std::this_thread::sleep_for(std::chrono::milliseconds(15));
            continue;
        }
        //  if isRunning is false, we use a condition var/wait to hold the thread
        if (isRunning && !isTerminated) {
            //  Run the emulator in either 'step' or 'run' mode.
            //
            //  RUN MODE executes several instructions in time slices to maximize
            //  performance while providing feedback to the frontend.
            //
            //  STEP MODE executes a single instruction and decrements a 'step' counter
            //
            //  If neither mode is applicable, the emulator holds and this loop will
            //  wait for commands from the frontend
            //
            areInstructionsLogged_ = stepsRemaining.has_value() && (*stepsRemaining > 0);

            const time_t kEpoch1904To1970Seconds = 2082844800;
            auto epoch_time_1904 =
                (std::chrono::system_clock::to_time_t(std::chrono::system_clock::now()) +
                 kEpoch1904To1970Seconds);

            clemens_rtc_set(&mmio_, (unsigned)epoch_time_1904);

            auto lastClocksSpent = machine_.tspec.clocks_spent;
            int64_t clocksPerTimeslice =
                calculateClocksPerTimeslice(&mmio_, emulatorRefreshFrequency);
            clocksRemainingInTimeslice += clocksPerTimeslice;

            machine_.cpu.cycles_spent = 0;
            while (clocksRemainingInTimeslice > 0 &&
                   (!stepsRemaining.has_value() || *stepsRemaining > 0)) {
                clem_clocks_time_t pre_emulate_time = machine_.tspec.clocks_spent;
                clemens_emulate_cpu(&machine_);
                clemens_emulate_mmio(&machine_, &mmio_);
                clem_clocks_duration_t emulate_step_time =
                    machine_.tspec.clocks_spent - pre_emulate_time;
                clocksRemainingInTimeslice -= emulate_step_time;
                if (stepsRemaining.has_value()) {
                    stepsRemaining = *stepsRemaining - 1;
                }
                //  TODO: MMIO bypass
                if (!breakpoints_.empty()) {
                    if ((hitBreakpoint = checkHitBreakpoint()).has_value()) {
                        stepsRemaining = 0;
                        break;
                    }
                }
            } // clocksRemainingInTimeslice

            if (stepsRemaining.has_value() && *stepsRemaining == 0) {
                //  if we've finished stepping through code, we are also done with our
                //  timeslice and will wait for a new step/run request
                clocksRemainingInTimeslice = 0;
                isRunning = false;
                areInstructionsLogged_ = false;
            }

            auto currentFrameTimePoint = std::chrono::high_resolution_clock::now();
            auto actualFrameInterval = std::chrono::duration_cast<std::chrono::microseconds>(
                currentFrameTimePoint - lastFrameTimePoint);
            lastFrameTimePoint = currentFrameTimePoint;

            runSampler.update(
                fixedFrameInterval, actualFrameInterval,
                (clem_clocks_duration_t)(machine_.tspec.clocks_spent - lastClocksSpent),
                machine_.cpu.cycles_spent);

            for (auto diskDriveIt = diskDrives_.begin(); diskDriveIt != diskDrives_.end();
                 ++diskDriveIt) {
                auto &diskDrive = *diskDriveIt;
                auto driveIndex = unsigned(diskDriveIt - diskDrives_.begin());
                auto driveType = static_cast<ClemensDriveType>(driveIndex);
                auto *clemensDrive = clemens_drive_get(&mmio_, driveType);
                diskDrive.isSpinning = clemensDrive->is_spindle_on;
                diskDrive.isWriteProtected = clemensDrive->disk.is_write_protected;
                diskDrive.saveFailed = false;
                if (diskDrive.isEjecting) {
                    if (clemens_eject_disk_async(&mmio_, driveType, &disks_[driveIndex])) {
                        diskDrive.isEjecting = false;
                        if (!saveDisk(driveType))
                            diskDrive.saveFailed = true;
                        diskDrive.imagePath.clear();
                        resetDisk(driveType);
                    }
                }
            }
            for (auto diskDriveIt = smartPortDrives_.begin(); diskDriveIt != smartPortDrives_.end();
                 ++diskDriveIt) {
                auto &diskDrive = *diskDriveIt;
                // auto driveIndex = unsigned(diskDriveIt - smartPortDrives_.begin());
                //  auto *clemensUnit = clemens_smartport_unit_get(&mmio_, driveIndex);
                //   TODO: detect SmartPort drive status - enable2 only detects if the
                //         whole bus is active - which may be fine for now since we just support
                //         one SmartPort drive!
                diskDrive.isSpinning = mmio_.dev_iwm.enable2;
                diskDrive.isWriteProtected = false;
                diskDrive.saveFailed = false;
                if (diskDrive.isEjecting) {
                    //  TODO: SmartPort drive ejection
                }
            }
            updateSeqNo = true;
        }

        if (isTerminated) {
            //  eject and save all disks
            for (auto diskDriveIt = diskDrives_.begin(); diskDriveIt != diskDrives_.end();
                 ++diskDriveIt) {
                auto &diskDrive = *diskDriveIt;
                if (diskDrive.imagePath.empty())
                    continue;

                auto driveIndex = unsigned(diskDriveIt - diskDrives_.begin());
                auto driveType = static_cast<ClemensDriveType>(driveIndex);
                if (clemens_eject_disk_async(&mmio_, driveType, &disks_[driveIndex])) {
                    saveDisk(driveType);
                    localLog(CLEM_DEBUG_LOG_INFO, "Saved {}", diskDrive.imagePath);
                }
            }
            for (auto hardDriveIt = smartPortDrives_.begin(); hardDriveIt != smartPortDrives_.end();
                 ++hardDriveIt) {
                auto &drive = *hardDriveIt;
                if (drive.imagePath.empty())
                    continue;
                auto driveIndex = unsigned(hardDriveIt - smartPortDrives_.begin());
                ClemensSmartPortDevice device;
                clemens_remove_smartport_disk(&mmio_, driveIndex, &device);
                smartPortDisks_[driveIndex].destroySmartPortDevice(&device);
                saveSmartPortDisk(driveIndex);
                localLog(CLEM_DEBUG_LOG_INFO, "Saved {}", drive.imagePath);
            }
            publishState = true;
            updateSeqNo = true;
        }

        updateSeqNo = updateSeqNo || commandFailed.has_value();

        if (updateSeqNo) {
            publishSeqNo++;
        }

        //  TODO: publish emulator state using a callback
        //        the recipient will synchronize with UI accordingly with the
        //        assumption that once the callback returns, we can alter the state
        //        again as needed next timeslice.
        if (publishState) {
            ClemensBackendState publishedState{};
            publishedState.mmio_was_initialized = clemens_is_initialized_simple(&machine_);
            if (publishedState.mmio_was_initialized) {
                clemens_get_monitor(&publishedState.monitor, &mmio_);
                clemens_get_text_video(&publishedState.text, &mmio_);
                clemens_get_graphics_video(&publishedState.graphics, &machine_, &mmio_);
                if (clemens_get_audio(&publishedState.audio, &mmio_)) {
                    if (mockingboard) {
                        auto &audio = publishedState.audio;
                        float *audio_frame_head = reinterpret_cast<float *>(
                            audio.data + audio.frame_start * audio.frame_stride);
                        clem_card_ay3_render(mockingboard, audio_frame_head, audio.frame_count,
                                             audio.frame_stride / sizeof(float),
                                             config_.audioSamplesPerSecond);
                    }
                }
            }
            publishedState.isRunning = isRunning;
            if (programTrace_ != nullptr) {
                publishedState.isTracing = true;
                publishedState.isIWMTracing = programTrace_->isIWMLoggingEnabled();
            } else {
                publishedState.isTracing = false;
                publishedState.isIWMTracing = false;
            }
            publishedState.machine = &machine_;
            publishedState.mmio = &mmio_;
            publishedState.seqno = publishSeqNo;
            publishedState.fps = runSampler.sampledFramesPerSecond;
            publishedState.hostCPUID = clem_host_get_processor_number();
            publishedState.logLevel = logLevel_;
            publishedState.logBufferStart = logOutput_.data();
            publishedState.logBufferEnd = logOutput_.data() + logOutput_.size();
            publishedState.logInstructionStart = loggedInstructions_.data();
            publishedState.logInstructionEnd =
                loggedInstructions_.data() + loggedInstructions_.size();
            publishedState.bpBufferStart = breakpoints_.data();
            publishedState.bpBufferEnd = breakpoints_.data() + breakpoints_.size();
            if (hitBreakpoint.has_value()) {
                publishedState.bpHitIndex = *hitBreakpoint;
            }
            publishedState.diskDrives = diskDrives_.data();
            publishedState.smartDrives = smartPortDrives_.data();
            publishedState.commandFailed = std::move(commandFailed);
            publishedState.commandType = std::move(commandType);
            publishedState.message = std::move(debugMessage);
            if (isTerminated) {
                publishedState.terminated = isTerminated;
            }
            //  read IO memory from bank 0xe0 which ignores memory shadow settings
            for (uint16_t ioAddr = 0xc000; ioAddr < 0xc0ff; ++ioAddr) {
                clem_read(&machine_, &publishedState.ioPageValues[ioAddr - 0xc000], ioAddr, 0xe0,
                          CLEM_MEM_FLAG_NULL);
            }
            publishedState.debugMemoryPage = debugMemoryPage_;
            publishedState.emulatorSpeedMhz = runSampler.sampledEmulatorSpeedMhz;

            publishDelegate(publishedState);
            if (publishedState.mmio_was_initialized) {
                clemens_audio_next_frame(&mmio_, publishedState.audio.frame_count);
            }
            logOutput_.clear();
            loggedInstructions_.clear();
            hitBreakpoint = std::nullopt;
            commandFailed = std::nullopt;
            commandType = std::nullopt;
            debugMessage = std::nullopt;
        }
    } // !isTerminated

    saveBRAM();

    //  TODO: clemens_mmio_card_eject() will clear the slot but it still needs
    //        to be destroyed by the app
    for (int i = 0; i < 7; ++i) {
        destroyCard(mmio_.card_slot[i]);
        mmio_.card_slot[i] = NULL;
    }

    fmt::print("Terminated backend refresh thread.\n");
}

#if defined(__GNUC__)
#pragma GCC diagnostic pop
#endif

std::optional<unsigned> ClemensBackend::checkHitBreakpoint() {
    for (auto it = breakpoints_.begin(); it != breakpoints_.end(); ++it) {
        uint16_t b_adr = (uint16_t)(it->address & 0xffff);
        uint8_t b_bank = (uint8_t)(it->address >> 16);
        unsigned index = (unsigned)(it - breakpoints_.begin());
        switch (it->type) {
        case ClemensBackendBreakpoint::Execute:
            if (machine_.cpu.regs.PBR == b_bank && machine_.cpu.regs.PC == b_adr) {
                return index;
            }
            break;
        case ClemensBackendBreakpoint::DataRead:
        case ClemensBackendBreakpoint::Write:
            if (machine_.cpu.pins.bank == b_bank && machine_.cpu.pins.adr == b_adr) {
                if (machine_.cpu.pins.vdaOut) {
                    if (it->type == ClemensBackendBreakpoint::DataRead &&
                        machine_.cpu.pins.rwbOut) {
                        return index;
                    } else if (it->type == ClemensBackendBreakpoint::Write &&
                               !machine_.cpu.pins.rwbOut) {
                        return index;
                    }
                }
            }
            break;
        case ClemensBackendBreakpoint::IRQ:
            if (machine_.cpu.state_type == kClemensCPUStateType_IRQ) {
                return index;
            }
            break;
        default:
            assert(false);
            break;
        }
    }
    return std::nullopt;
}

void ClemensBackend::initEmulatedDiskLocalStorage() {
    diskBuffer_ = cinek::ByteBuffer((uint8_t *)slabMemory_.allocate(CLEM_DISK_35_MAX_DATA_SIZE),
                                    CLEM_DISK_35_MAX_DATA_SIZE + 4096);
    disks_.fill(ClemensNibbleDisk{});
    disks_[kClemensDrive_3_5_D1].bits_data =
        (uint8_t *)slabMemory_.allocate(CLEM_DISK_35_MAX_DATA_SIZE);
    disks_[kClemensDrive_3_5_D1].bits_data_end =
        disks_[kClemensDrive_3_5_D1].bits_data + CLEM_DISK_35_MAX_DATA_SIZE;
    disks_[kClemensDrive_3_5_D2].bits_data =
        (uint8_t *)slabMemory_.allocate(CLEM_DISK_35_MAX_DATA_SIZE);
    disks_[kClemensDrive_3_5_D2].bits_data_end =
        disks_[kClemensDrive_3_5_D2].bits_data + CLEM_DISK_35_MAX_DATA_SIZE;

    disks_[kClemensDrive_5_25_D1].bits_data =
        (uint8_t *)slabMemory_.allocate(CLEM_DISK_525_MAX_DATA_SIZE);
    disks_[kClemensDrive_5_25_D1].bits_data_end =
        disks_[kClemensDrive_5_25_D1].bits_data + CLEM_DISK_525_MAX_DATA_SIZE;
    disks_[kClemensDrive_5_25_D2].bits_data =
        (uint8_t *)slabMemory_.allocate(CLEM_DISK_525_MAX_DATA_SIZE);
    disks_[kClemensDrive_5_25_D2].bits_data_end =
        disks_[kClemensDrive_5_25_D2].bits_data + CLEM_DISK_525_MAX_DATA_SIZE;

    //  some sanity values to initialize
    for (size_t driveIndex = 0; driveIndex < diskDrives_.size(); ++driveIndex) {
        auto &diskDrive = diskDrives_[driveIndex];
        diskDrive.isEjecting = false;
        diskDrive.isSpinning = false;
        diskDrive.isWriteProtected = true;
        diskDrive.saveFailed = false;
        diskDrive.imagePath = config_.diskDriveStates[driveIndex].imagePath;
    }

    for (size_t driveIndex = 0; driveIndex < smartPortDrives_.size(); ++driveIndex) {
        auto &diskDrive = smartPortDrives_[driveIndex];
        diskDrive.isEjecting = false;
        diskDrive.isSpinning = false;
        diskDrive.isWriteProtected = true;
        diskDrive.saveFailed = false;
        diskDrive.imagePath = config_.smartPortDriveStates[driveIndex].imagePath;
    }
}

cinek::ByteBuffer ClemensBackend::loadROM(const char *romPathname) {
    cinek::ByteBuffer romBuffer;
    std::ifstream romFileStream(romPathname, std::ios::binary | std::ios::ate);
    unsigned romMemorySize = 0;

    if (romFileStream.is_open()) {
        romMemorySize = unsigned(romFileStream.tellg());
        romBuffer =
            cinek::ByteBuffer((uint8_t *)slabMemory_.allocate(romMemorySize), romMemorySize);
        romFileStream.seekg(0, std::ios::beg);
        romFileStream.read((char *)romBuffer.forwardSize(romMemorySize).first, romMemorySize);
        romFileStream.close();
    }
    return romBuffer;
}

void ClemensBackend::initApple2GS() {
    const unsigned kFPIBankCount = CLEM_IIGS_FPI_MAIN_RAM_BANK_LIMIT;
    const uint32_t kClocksPerFastCycle = CLEM_CLOCKS_FAST_CYCLE;
    const uint32_t kClocksPerSlowCycle = CLEM_CLOCKS_MEGA2_CYCLE;
    int result =
        clemens_init(&machine_, kClocksPerSlowCycle, kClocksPerFastCycle, romBuffer_.getHead(),
                     romBuffer_.getSize(), slabMemory_.allocate(CLEM_IIGS_BANK_SIZE),
                     slabMemory_.allocate(CLEM_IIGS_BANK_SIZE),
                     slabMemory_.allocate(CLEM_IIGS_BANK_SIZE * kFPIBankCount), kFPIBankCount);
    clem_mmio_init(&mmio_, &machine_.dev_debug, machine_.mem.bank_page_map,
                   machine_.tspec.clocks_step_mega2, slabMemory_.allocate(2048 * 7), kFPIBankCount);
    if (result < 0) {
        fmt::print("Clemens library failed to initialize with err code (%d)\n", result);
        return;
    }
    loadBRAM();

    //  TODO: It seems the internal audio code expects 2 channel float PCM,
    //        so we only need buffer size and frequency.
    ClemensAudioMixBuffer audioMixBuffer;
    audioMixBuffer.frames_per_second = config_.audioSamplesPerSecond;
    audioMixBuffer.stride = 2 * sizeof(float);
    audioMixBuffer.frame_count = audioMixBuffer.frames_per_second / 4;
    audioMixBuffer.data =
        (uint8_t *)(slabMemory_.allocate(audioMixBuffer.frame_count * audioMixBuffer.stride));
    clemens_assign_audio_mix_buffer(&mmio_, &audioMixBuffer);
}

void ClemensBackend::saveBRAM() {
    bool isDirty = false;
    const uint8_t *bram = clemens_rtc_get_bram(&mmio_, &isDirty);
    if (!isDirty)
        return;

    std::ofstream bramFile("clem.bram", std::ios::binary);
    if (bramFile.is_open()) {
        bramFile.write((char *)bram, CLEM_RTC_BRAM_SIZE);
    } else {
        //  TODO: display error?
    }
}

void ClemensBackend::loadBRAM() {
    std::ifstream bramFile("clem.bram", std::ios::binary);
    if (bramFile.is_open()) {
        bramFile.read((char *)mmio_.dev_rtc.bram, CLEM_RTC_BRAM_SIZE);
    } else {
        //  TODO: display warning?
    }
}

void ClemensBackend::emulatorLog(int log_level, ClemensMachine *machine, const char *msg) {
    auto *host = reinterpret_cast<ClemensBackend *>(machine->debug_user_ptr);
    if (host->logLevel_ > log_level)
        return;
    if (host->logOutput_.size() >= kLogOutputLineLimit)
        return;

    host->logOutput_.emplace_back(ClemensBackendOutputText{log_level, msg});
}

//  If enabled, this emulator issues this callback per instruction
//  This is great for debugging but should be disabled otherwise (see TODO)
void ClemensBackend::emulatorOpcodeCallback(struct ClemensInstruction *inst, const char *operand,
                                            void *this_ptr) {
    auto *host = reinterpret_cast<ClemensBackend *>(this_ptr);
    if (host->programTrace_) {
        host->programTrace_->addExecutedInstruction(host->nextTraceSeq_++, *inst, operand,
                                                    host->machine_);
    }
    if (!host->areInstructionsLogged_)
        return;
    host->loggedInstructions_.emplace_back();
    auto &loggedInst = host->loggedInstructions_.back();
    loggedInst.data = *inst;
    strncpy(loggedInst.operand, operand, sizeof(loggedInst.operand) - 1);
    loggedInst.operand[sizeof(loggedInst.operand) - 1] = '\0';
}
