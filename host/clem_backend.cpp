#include "clem_backend.hpp"
#include "clem_disk_utils.hpp"
#include "clem_host_platform.h"
#include "clem_host_shared.hpp"
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
#include <vector>

#include "fmt/format.h"

static constexpr unsigned kSlabMemorySize = 32 * 1024 * 1024;
static constexpr unsigned kInterpreterMemorySize = 1 * 1024 * 1024;
static constexpr unsigned kLogOutputLineLimit = 1024;
static constexpr unsigned kSmartPortDiskBlockCount = 32 * 1024 * 2; // 32 MB blocks

ClemensRunSampler::ClemensRunSampler() { reset(); }

void ClemensRunSampler::reset() {
    referenceFrameTimer = std::chrono::microseconds::zero();
    actualFrameTimer = std::chrono::microseconds::zero();
    sampledFrameTime = std::chrono::microseconds::zero();
    lastFrameTimePoint = std::chrono::high_resolution_clock::now();
    sampledClocksSpent = 0;
    sampledCyclesSpent = 0;
    sampledFramesPerSecond = 0.0f;
    sampledMachineSpeedMhz = 0.0f;
    avgVBLsPerFrame = 0.0f;
    sampledVblsSpent = 0;
    emulatorVblsPerFrame = 1;
    fastModeEnabled = false;
    frameTimeBuffer.clear();
    clocksBuffer.clear();
    cyclesBuffer.clear();
    vblsBuffer.clear();
}

void ClemensRunSampler::enableFastMode() { fastModeEnabled = true; }

void ClemensRunSampler::disableFastMode() {
    fastModeEnabled = false;
    emulatorVblsPerFrame = 1;
}

void ClemensRunSampler::update(clem_clocks_duration_t clocksSpent, unsigned cyclesSpent) {
    auto currentFrameTimePoint = std::chrono::high_resolution_clock::now();
    auto actualFrameInterval = std::chrono::duration_cast<std::chrono::microseconds>(
        currentFrameTimePoint - lastFrameTimePoint);
    lastFrameTimePoint = currentFrameTimePoint;

    actualFrameTimer += actualFrameInterval;

    if (frameTimeBuffer.isFull()) {
        decltype(frameTimeBuffer)::ValueType lruFrametime;
        if (frameTimeBuffer.pop(lruFrametime)) {
            sampledFrameTime -= lruFrametime;
        }
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
        double cyclesPerClock = (double)sampledCyclesSpent / sampledClocksSpent;
        sampledMachineSpeedMhz = 1.023 * cyclesPerClock * CLEM_CLOCKS_MEGA2_CYCLE;
    }

    if (vblsBuffer.isFull()) {
        decltype(vblsBuffer)::ValueType lruVbls = 0;
        vblsBuffer.pop(lruVbls);
        sampledVblsSpent -= lruVbls;
    }
    vblsBuffer.push(emulatorVblsPerFrame);
    sampledVblsSpent += emulatorVblsPerFrame;
    if (fastModeEnabled) {
        if (sampledFramesPerSecond > 45.0) {
            emulatorVblsPerFrame += 1;
        } else if (sampledFramesPerSecond < 35.0) {
            emulatorVblsPerFrame -= 1;
        }
        if (emulatorVblsPerFrame < 1)
            emulatorVblsPerFrame = 1;

    } else {
        emulatorVblsPerFrame = 1;
    }
    avgVBLsPerFrame = (double)sampledVblsSpent / vblsBuffer.size();
}

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

ClemensBackend::ClemensBackend(std::string romPathname, const Config &config)
    : config_(config), slabMemory_(kSlabMemorySize, malloc(kSlabMemorySize)),
      mockingboard_(nullptr),
      interpreter_(cinek::FixedStack(kInterpreterMemorySize, malloc(kInterpreterMemorySize))),
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

    clocksRemainingInTimeslice_ = 0;
    //  TODO: hacky - basically we start the machine in a stopped state, so a
    //  command queued for 'run' will start everything.  Maybe it isn't hacky?
    stepsRemaining_ = 0;
}

ClemensBackend::~ClemensBackend() {
    //  eject and save all disks
    for (auto diskDriveIt = diskDrives_.begin(); diskDriveIt != diskDrives_.end(); ++diskDriveIt) {
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

    saveBRAM();

    //  TODO: clemens_mmio_card_eject() will clear the slot but it still needs
    //        to be destroyed by the app
    for (int i = 0; i < CLEM_CARD_SLOT_COUNT; ++i) {
        destroyCard(mmio_.card_slot[i]);
        mmio_.card_slot[i] = NULL;
    }

    free(slabMemory_.getHead());
}

//  TODO: Move into Clemens API clemens_mmio_find_card_name()
static ClemensCard *findMockingboardCard(ClemensMMIO *mmio) {
    for (int cardIdx = 0; cardIdx < CLEM_CARD_SLOT_COUNT; ++cardIdx) {
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
        ClemensDiskUtilities::createEmptyDisk(driveType, disks_[driveType]);
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
        auto diskData = ClemensSmartPortDisk::createData(kSmartPortDiskBlockCount);
        smartPortDisks_[driveIndex] = ClemensSmartPortDisk(std::move(diskData));
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

/*
//  from the emulator, obtain the number of clocks per second desired
//
static int64_t calculateClocksPerTimeslice(ClemensMMIO *mmio, unsigned hz) {
    bool is_machine_slow;
    return int64_t(clemens_clocks_per_second(mmio, &is_machine_slow) / hz);
}
*/

bool ClemensBackend::isRunning() const {
    return !stepsRemaining_.has_value() || *stepsRemaining_ > 0;
}

#if defined(__GNUC__)
#if !defined(__clang__)
//  Despite guarding with std::optional<>::has_value(), annoying GCC warning
//  that I may be accessing an uninitialized optional with a folloing value
//  access (if has_value() returns true) - which is the accepted method
//  or checking optionals if exception handling is turned off
//  https://gcc.gnu.org/bugzilla/show_bug.cgi?id=80635
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wmaybe-uninitialized"
#endif
#endif

ClemensCommandQueue::DispatchResult
ClemensBackend::main(ClemensBackendState &backendState,
                     const ClemensCommandQueue::ResultBuffer &commandResults,
                     PublishStateDelegate delegate) {

    std::optional<unsigned> hitBreakpoint;

    bool isMachineRunning = isRunning();

    if (isMachineRunning) {
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
        areInstructionsLogged_ = stepsRemaining_.has_value() && (*stepsRemaining_ > 0);

        const time_t kEpoch1904To1970Seconds = 2082844800;
        auto epoch_time_1904 =
            (std::chrono::system_clock::to_time_t(std::chrono::system_clock::now()) +
             kEpoch1904To1970Seconds);

        clemens_rtc_set(&mmio_, (unsigned)epoch_time_1904);

        // TODO: this should be an adaptive scalar to support a variety of devices.
        // Though if the emulator runs hot (cannot execute the desired cycles in enough time),
        // that shouldn't matter too much given that this 'speed boost' is meant to be
        // temporary, and the emulator should catch up once the IWM is inactive.
        if (clemens_is_drive_io_active(&mmio_) && config_.enableFastEmulation) {
            runSampler_.enableFastMode();
        } else {
            runSampler_.disableFastMode();
        }
        auto lastClocksSpent = machine_.tspec.clocks_spent;
        machine_.cpu.cycles_spent = 0;

        unsigned emulatorVblCounter = runSampler_.emulatorVblsPerFrame;
        bool vblActive = mmio_.vgc.vbl_started;
        while (emulatorVblCounter > 0 && isRunning()) {
            clemens_emulate_cpu(&machine_);
            clemens_emulate_mmio(&machine_, &mmio_);
            if (vblActive && !mmio_.vgc.vbl_started) {
                emulatorVblCounter--;
            }
            vblActive = mmio_.vgc.vbl_started;
            if (stepsRemaining_.has_value()) {
                stepsRemaining_ = *stepsRemaining_ - 1;
            }
            //  TODO: MMIO bypass
            if (!breakpoints_.empty()) {
                if ((hitBreakpoint = checkHitBreakpoint()).has_value()) {
                    stepsRemaining_ = 0;
                    break;
                }
            }
            if (!machine_.cpu.enabled)
                break;
        }

        if (stepsRemaining_.has_value() && *stepsRemaining_ == 0) {
            //  if we've finished stepping through code, we are also done with our
            //  timeslice and will wait for a new step/run request
            clocksRemainingInTimeslice_ = 0;
            isMachineRunning = false;
            areInstructionsLogged_ = false;
        }

        runSampler_.update((clem_clocks_duration_t)(machine_.tspec.clocks_spent - lastClocksSpent),
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
                    ClemensDiskUtilities::createEmptyDisk(driveType, disks_[driveType]);
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
    }

    backendState.machine = &machine_;
    backendState.mmio = &mmio_;
    backendState.fps = runSampler_.sampledFramesPerSecond;
    backendState.isRunning = isMachineRunning;
    if (programTrace_ != nullptr) {
        backendState.isTracing = true;
        backendState.isIWMTracing = programTrace_->isIWMLoggingEnabled();
    } else {
        backendState.isTracing = false;
        backendState.isIWMTracing = false;
    }
    backendState.mmioWasInitialized = clemens_is_mmio_initialized(&mmio_);
    if (backendState.mmioWasInitialized) {
        clemens_get_monitor(&backendState.monitor, &mmio_);
        clemens_get_text_video(&backendState.text, &mmio_);
        clemens_get_graphics_video(&backendState.graphics, &machine_, &mmio_);
        if (clemens_get_audio(&backendState.audio, &mmio_)) {
            if (mockingboard_) {
                auto &audio = backendState.audio;
                float *audio_frame_head =
                    reinterpret_cast<float *>(audio.data + audio.frame_start * audio.frame_stride);
                clem_card_ay3_render(mockingboard_, audio_frame_head, audio.frame_count,
                                     audio.frame_stride / sizeof(float),
                                     config_.audioSamplesPerSecond);
            }
        }
    } else {
        memset(&backendState.monitor, 0, sizeof(backendState.monitor));
        memset(&backendState.text, 0, sizeof(backendState.text));
        memset(&backendState.graphics, 0, sizeof(backendState.graphics));
        memset(&backendState.audio, 0, sizeof(backendState.audio));
    }
    backendState.hostCPUID = clem_host_get_processor_number();
    backendState.logLevel = logLevel_;
    backendState.logBufferStart = logOutput_.data();
    backendState.logBufferEnd = logOutput_.data() + logOutput_.size();
    backendState.bpBufferStart = breakpoints_.data();
    backendState.bpBufferEnd = breakpoints_.data() + breakpoints_.size();
    if (hitBreakpoint.has_value()) {
        backendState.bpHitIndex = *hitBreakpoint;
    } else {
        backendState.bpHitIndex = std::nullopt;
    }

    backendState.diskDrives = diskDrives_.data();
    backendState.smartDrives = smartPortDrives_.data();
    backendState.logInstructionStart = loggedInstructions_.data();
    backendState.logInstructionEnd = loggedInstructions_.data() + loggedInstructions_.size();

    //  read IO memory from bank 0xe0 which ignores memory shadow settings
    if (backendState.mmioWasInitialized) {
        for (uint16_t ioAddr = 0xc000; ioAddr < 0xc0ff; ++ioAddr) {
            clem_read(&machine_, &backendState.ioPageValues[ioAddr - 0xc000], ioAddr, 0xe0,
                      CLEM_MEM_FLAG_NULL);
        }
    }
    backendState.debugMemoryPage = debugMemoryPage_;
    backendState.machineSpeedMhz = runSampler_.sampledMachineSpeedMhz;
    backendState.avgVBLsPerFrame = runSampler_.avgVBLsPerFrame;
    backendState.fastEmulationOn = runSampler_.emulatorVblsPerFrame > 1;

    ClemensCommandQueue commands;
    delegate(commands, commandResults, backendState);

    if (backendState.mmioWasInitialized) {
        clemens_audio_next_frame(&mmio_, backendState.audio.frame_count);
    }
    logOutput_.clear();
    loggedInstructions_.clear();

    auto result = commands.dispatchAll(*this);
    //  if we're starting a run, reset the sampler so framerate can be calculated correctly
    if (isMachineRunning != isRunning()) {
        if (!isMachineRunning) {
            runSampler_.reset();
        }
    }

    return result;
}

#if defined(__GNUC__)
#if !defined(__clang__)
#pragma GCC diagnostic pop
#endif
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
        case ClemensBackendBreakpoint::BRK:
            if (machine_.cpu.regs.IR == CLEM_OPC_BRK) {
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

    //  TODO: clemens API clemens_mmio_card_insert()
    for (size_t cardIdx = 0; cardIdx < config_.cardNames.size(); ++cardIdx) {
        auto &cardName = config_.cardNames[cardIdx];
        if (!cardName.empty()) {
            mmio_.card_slot[cardIdx] = createCard(cardName.c_str());
        } else {
            mmio_.card_slot[cardIdx] = NULL;
        }
    }

    mockingboard_ = findMockingboardCard(&mmio_);
}

void ClemensBackend::saveBRAM() {
    bool isDirty = false;
    const uint8_t *bram = clemens_rtc_get_bram(&mmio_, &isDirty);
    if (!isDirty)
        return;
    auto bramPath = std::filesystem::path(config_.dataRootPath) / "clem.bram";
    std::ofstream bramFile(bramPath, std::ios::binary);
    if (bramFile.is_open()) {
        bramFile.write((char *)bram, CLEM_RTC_BRAM_SIZE);
    } else {
        //  TODO: display error?
    }
}

void ClemensBackend::loadBRAM() {
    auto bramPath = std::filesystem::path(config_.dataRootPath) / "clem.bram";
    std::ifstream bramFile(bramPath, std::ios::binary);
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

///////////////////////////////////////////////////////////////////////////////

void ClemensBackend::assignPropertyToU32(MachineProperty property, uint32_t value) {
    bool emulation = machine_.cpu.pins.emulation;
    bool acc8 = emulation || (machine_.cpu.regs.P & kClemensCPUStatus_MemoryAccumulator) != 0;
    bool idx8 = emulation || (machine_.cpu.regs.P & kClemensCPUStatus_Index) != 0;

    switch (property) {
    case MachineProperty::RegA:
        if (acc8)
            machine_.cpu.regs.A = (machine_.cpu.regs.A & 0xff00) | (uint16_t)(value & 0xff);
        else
            machine_.cpu.regs.A = (uint16_t)(value & 0xffff);
        break;
    case MachineProperty::RegB:
        machine_.cpu.regs.A = (machine_.cpu.regs.A & 0xff) | (uint16_t)((value & 0xff) << 8);
        break;
    case MachineProperty::RegC:
        machine_.cpu.regs.A = (uint16_t)(value & 0xffff);
        break;
    case MachineProperty::RegX:
        if (emulation)
            machine_.cpu.regs.X = (uint16_t)(value & 0xff);
        else if (idx8)
            machine_.cpu.regs.X = (machine_.cpu.regs.X & 0xff00) | (uint16_t)(value & 0xff);
        else
            machine_.cpu.regs.X = (uint16_t)(value & 0xffff);
        break;
    case MachineProperty::RegY:
        if (emulation)
            machine_.cpu.regs.Y = (uint16_t)(value & 0xff);
        else if (idx8)
            machine_.cpu.regs.Y = (machine_.cpu.regs.Y & 0xff00) | (uint16_t)(value & 0xff);
        else
            machine_.cpu.regs.Y = (uint16_t)(value & 0xffff);
        break;
    case MachineProperty::RegP:
        if (emulation) {
            machine_.cpu.regs.P = (value & 0x30); // do not affect MX flags
        } else {
            machine_.cpu.regs.P = (value & 0xff);
        }
        break;
    case MachineProperty::RegD:
        machine_.cpu.regs.D = (uint16_t)(value & 0xffff);
        break;
    case MachineProperty::RegSP:
        if (emulation) {
            machine_.cpu.regs.S = (machine_.cpu.regs.S & 0xff00) | (uint16_t)(value & 0xff);
        } else {
            machine_.cpu.regs.S = (uint16_t)(value & 0xffff);
        }
        break;
    case MachineProperty::RegDBR:
        machine_.cpu.regs.DBR = (uint8_t)(value & 0xff);
        break;
    case MachineProperty::RegPBR:
        machine_.cpu.regs.PBR = (uint8_t)(value & 0xff);
        break;
    case MachineProperty::RegPC:
        machine_.cpu.regs.PC = (uint16_t)(value & 0xff);
        break;
    };
}

void ClemensBackend::onCommandReset() {
    machine_.cpu.pins.resbIn = false;
    machine_.resb_counter = 3;
    mockingboard_ = findMockingboardCard(&mmio_);
}

void ClemensBackend::onCommandRun() { stepsRemaining_ = std::nullopt; }

void ClemensBackend::onCommandBreakExecution() { stepsRemaining_ = 0; }

void ClemensBackend::onCommandStep(unsigned count) { stepsRemaining_ = count; }

void ClemensBackend::onCommandAddBreakpoint(const ClemensBackendBreakpoint &breakpoint) {
    auto range = std::equal_range(
        breakpoints_.begin(), breakpoints_.end(), breakpoint,
        [](const ClemensBackendBreakpoint &breakpoint, const ClemensBackendBreakpoint &newBp) {
            return breakpoint.address < newBp.address;
        });
    while (range.first != range.second) {
        if (range.first->type == breakpoint.type) {
            break;
        }
        range.first++;
    }
    if (range.first == range.second) {
        breakpoints_.emplace(range.second, breakpoint);
    }
}

bool ClemensBackend::onCommandRemoveBreakpoint(int index) {
    if (index < 0) {
        breakpoints_.clear();
        return true;
    }
    if (index >= (int)breakpoints_.size()) {
        return false;
    }
    breakpoints_.erase(breakpoints_.begin() + index);
    return true;
}

void ClemensBackend::onCommandInputEvent(const ClemensInputEvent &inputEvent) {
    if (!clemens_is_initialized_simple(&machine_)) {
        return;
    }
    clemens_input(&mmio_, &inputEvent);
}

bool ClemensBackend::onCommandInsertDisk(ClemensDriveType driveType, std::string diskPath) {
    diskDrives_[driveType].imagePath = diskPath;
    return loadDisk(driveType, false);
}

bool ClemensBackend::onCommandInsertBlankDisk(ClemensDriveType driveType, std::string diskPath) {
    diskDrives_[driveType].imagePath = diskPath;
    return loadDisk(driveType, true);
}

void ClemensBackend::onCommandEjectDisk(ClemensDriveType driveType) {
    diskDrives_[driveType].isEjecting = true;
}

bool ClemensBackend::onCommandWriteProtectDisk(ClemensDriveType driveType, bool wp) {
    auto *drive = clemens_drive_get(&mmio_, driveType);
    if (!drive || !drive->has_disk)
        return false;
    drive->disk.is_write_protected = wp;
    return true;
}

void ClemensBackend::onCommandDebugMemoryPage(uint8_t pageIndex) { debugMemoryPage_ = pageIndex; }

void ClemensBackend::onCommandDebugMemoryWrite(uint16_t addr, uint8_t value) {
    clem_write(&machine_, value, addr, debugMemoryPage_, CLEM_MEM_FLAG_NULL);
}

void ClemensBackend::onCommandDebugLogLevel(int logLevel) { logLevel_ = logLevel; }

bool ClemensBackend::onCommandDebugProgramTrace(std::string_view op, std::string_view path) {
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
        auto exportPath = std::filesystem::path(config_.traceRootPath) / path;
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

bool ClemensBackend::onCommandSaveMachine(std::string path) {
    auto outputPath = std::filesystem::path(config_.snapshotRootPath) / path;
    saveBRAM();
    return ClemensSerializer::save(outputPath.string(), &machine_, &mmio_, diskContainers_.size(),
                                   diskContainers_.data(), diskDrives_.data(),
                                   CLEM_SMARTPORT_DRIVE_LIMIT, smartPortDisks_.data(),
                                   smartPortDrives_.data(), breakpoints_);
}

bool ClemensBackend::onCommandLoadMachine(std::string path) {
    auto outputPath = std::filesystem::path(config_.snapshotRootPath) / path;
    bool res = ClemensSerializer::load(
        outputPath.string(), &machine_, &mmio_, diskContainers_.size(), diskContainers_.data(),
        diskDrives_.data(), CLEM_SMARTPORT_DRIVE_LIMIT, smartPortDisks_.data(),
        smartPortDrives_.data(), breakpoints_, &ClemensBackend::unserializeAllocate, this);
    mockingboard_ = findMockingboardCard(&mmio_);
    saveBRAM();
    return res;
}

bool ClemensBackend::onCommandRunScript(std::string command) {
    auto result = interpreter_.parse(command);
    if (result.type == ClemensInterpreter::Result::Ok) {
        interpreter_.execute(this);
    } else if (result.type == ClemensInterpreter::Result::SyntaxError) {
        // CLEM_TERM_COUT.print(TerminalLine::Error, "Syntax Error");
        return false;
    } else {
        //        CLEM_TERM_COUT.print(TerminalLine::Error, "Unrecognized command!");
        return false;
    }
    return true;
}

void ClemensBackend::onCommandFastDiskEmulation(bool enabled) {
    fmt::print("{} fast disk emulation when IWM is active\n", enabled ? "Enable" : "Disable");
    config_.enableFastEmulation = enabled;
}
