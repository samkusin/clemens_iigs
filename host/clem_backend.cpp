#include "clem_backend.hpp"

#include "clem_command_queue.hpp"
#include "core/clem_apple2gs.hpp"
#include "core/clem_apple2gs_config.hpp"
#include "core/clem_disk_asset.hpp"
#include "core/clem_snapshot.hpp"

#include "clem_disk.h"
#include "clem_host_platform.h"
#include "clem_program_trace.hpp"

#include "clem_device.h"
#include "clem_mem.h"
#include "clem_shared.h"

#include "emulator.h"
#include "emulator_mmio.h"

#include "cinek/buffer.hpp"
#include "external/mpack.h"
#include "fmt/format.h"
#include "spdlog/common.h"
#include "spdlog/spdlog.h"

#include <charconv>
#include <chrono>
#include <ctime>
#include <filesystem>
#include <fstream>
#include <ios>

namespace {

constexpr unsigned kInterpreterMemorySize = 1 * 1024 * 1024;
constexpr unsigned kLogOutputLineLimit = 1024;

//  TODO: candidate for moving into the platform-specific codebase if the
//  C runtime method doesn't work on all platforms
//
//  reference: https://stackoverflow.com/a/44063597
//
//  The idea here is that since gmtime returns the calendar time, mktime will
//  return the epoch time, converting it back to UTC
int get_local_epoch_time_delta_in_seconds() {
    struct tm split_time_utc;
    struct tm *split_time_utc_ptr;
    time_t time_raw = time(NULL);
    time_t time_utc;
#if CK_COMPILER_MSVC
    gmtime_s(&split_time_utc, &time_raw);
    split_time_utc_ptr = &split_time_utc;
#else
    split_time_utc_ptr = gmtime_r(&time_raw, &split_time_utc);
#endif
    split_time_utc_ptr->tm_isdst = -1;
    time_utc = mktime(split_time_utc_ptr);
    return (int)difftime(time_raw, time_utc);
}

} // namespace

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
    fastModeDisabledThisFrame = false;
    frameTimeBuffer.clear();
    clocksBuffer.clear();
    cyclesBuffer.clear();
    vblsBuffer.clear();
}

void ClemensRunSampler::enableFastMode() { fastModeEnabled = true; }

void ClemensRunSampler::disableFastMode() {
    if (fastModeEnabled) fastModeDisabledThisFrame = true;
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

    //  calculate emulator speed by using cycles_spent * CLEM_CLOCKS_PHI0_CYCLE
    //  as a reference for 1.023mhz where
    //    reference_clocks = cycles_spent * CLEM_CLOCKS_PHI0_CYCLE
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
    if (sampledClocksSpent > (CLEM_CLOCKS_PHI0_CYCLE * CLEM_MEGA2_CYCLES_PER_SECOND / 10)) {
        double cyclesPerClock = (double)sampledCyclesSpent / sampledClocksSpent;
        sampledMachineSpeedMhz = 1.023 * cyclesPerClock * CLEM_CLOCKS_PHI0_CYCLE;
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

ClemensBackend::ClemensBackend(std::string romPath, const Config &config)
    : config_(config), gsConfigUpdated_(false),
      interpreter_(cinek::FixedStack(kInterpreterMemorySize, malloc(kInterpreterMemorySize))),
      breakpoints_(std::move(config_.breakpoints)), logLevel_(config_.logLevel),
      debugMemoryPage_(0x00), areInstructionsLogged_(false), stepsRemaining_(0),
      clocksRemainingInTimeslice_(0) {

    loggedInstructions_.reserve(10000);

    clemens_register();

    switch (config_.type) {
    case Config::Type::Apple2GS:
        GS_ = std::make_unique<ClemensAppleIIGS>(romPath, config_.GS, *this);
        GS_->mount();
        break;
    }

    clipboardHead_ = 0;
}

ClemensBackend::~ClemensBackend() {}

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

auto ClemensBackend::step(ClemensCommandQueue &commands) -> ClemensCommandQueue::DispatchResult {
    constexpr clem_clocks_time_t kClocksPerSecond =
        1e9 * CLEM_CLOCKS_14MHZ_CYCLE / CLEM_14MHZ_CYCLE_NS;

    logOutput_.clear();
    loggedInstructions_.clear();

    bool isMachineRunning = isRunning();
    if (isMachineRunning && GS_->isOk()) {
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
        if (areInstructionsLogged_ || programTrace_) {
            GS_->enableOpcodeLogging(true);
        }
        // TODO: this should be an adaptive scalar to support a variety of devices.
        // Though if the emulator runs hot (cannot execute the desired cycles in enough time),
        // that shouldn't matter too much given that this 'speed boost' is meant to be
        // temporary, and the emulator should catch up once the IWM is inactive.
        if (clemens_is_drive_io_active(&GS_->getMMIO()) && config_.enableFastEmulation) {
            runSampler_.enableFastMode();
        } else {
            runSampler_.disableFastMode();
        }

        if (clocksInSecondPeriod_ >= kClocksPerSecond) {
            updateRTC();
        }

        //  TODO: GS_->beginTimeslice();
        auto &machine = GS_->getMachine();
        auto lastClocksSpent = machine.tspec.clocks_spent;
        machine.cpu.cycles_spent = 0;

        unsigned emulatorVblCounter = runSampler_.emulatorVblsPerFrame;
        while (emulatorVblCounter > 0 && isRunning()) {
            auto machineResult = GS_->stepMachine();
            if (test(machineResult, ClemensAppleIIGS::ResultFlags::Resetting)) {
                lastClocksSpent = machine.tspec.clocks_spent; // clocks being reset
            }
            if (test(machineResult, ClemensAppleIIGS::ResultFlags::VerticalBlank)) {
                if (clipboardHead_ < clipboardText_.size()) {
                    clipboardHead_ +=
                        GS_->consume_utf8_input(clipboardText_.data() + clipboardHead_,
                                                clipboardText_.data() + clipboardText_.size());
                }
                emulatorVblCounter--;
            }
            if (stepsRemaining_.has_value()) {
                stepsRemaining_ = *stepsRemaining_ - 1;
            }

            if (!breakpoints_.empty()) {
                if ((hitBreakpoint_ = checkHitBreakpoint()).has_value()) {
                    stepsRemaining_ = 0;
                    break;
                }
            }
            if (GS_->getStatus() == ClemensAppleIIGS::Status::Stopped)
                break;
        }

        if (stepsRemaining_.has_value() && *stepsRemaining_ == 0) {
            //  if we've finished stepping through code, we are also done with our
            //  timeslice and will wait for a new step/run request
            clocksRemainingInTimeslice_ = 0;
            isMachineRunning = false;
            areInstructionsLogged_ = false;
        }

        GS_->enableOpcodeLogging(false);

        runSampler_.update((clem_clocks_duration_t)(machine.tspec.clocks_spent - lastClocksSpent),
                           machine.cpu.cycles_spent);
        clocksInSecondPeriod_ += machine.tspec.clocks_spent - lastClocksSpent;
    }

    auto result = commands.dispatchAll(*this);
    //  if we're starting a run, reset the sampler so framerate can be calculated correctly
    if (isMachineRunning != isRunning()) {
        if (!isMachineRunning) {
            runSampler_.reset();
        }
    }
    return result;
}

std::pair<ClemensAudio, bool> ClemensBackend::renderAudioFrame() { 
    std::pair<ClemensAudio, bool> out { GS_->renderAudio(), runSampler_.fastModeDisabledThisFrame};
    return out;
}

void ClemensBackend::post(ClemensBackendState &backendState) {
    auto &machine = GS_->getMachine();
    auto &mmio = GS_->getMMIO();

    backendState.machine = &machine;
    backendState.mmio = &mmio;
    backendState.fps = runSampler_.sampledFramesPerSecond;
    backendState.isRunning = isRunning();
    if (programTrace_ != nullptr) {
        backendState.isTracing = true;
        backendState.isIWMTracing = programTrace_->isIWMLoggingEnabled();
    } else {
        backendState.isTracing = false;
        backendState.isIWMTracing = false;
    }
    backendState.mmioWasInitialized = clemens_is_mmio_initialized(&mmio);

    GS_->getFrame(backendState.frame);
    if (gsConfigUpdated_) {
        backendState.config = gsConfig_;
        gsConfigUpdated_ = false;
    }
    backendState.hostCPUID = clem_host_get_processor_number();
    backendState.logLevel = logLevel_;
    backendState.logBufferStart = logOutput_.data();
    backendState.logBufferEnd = logOutput_.data() + logOutput_.size();
    backendState.bpBufferStart = breakpoints_.data();
    backendState.bpBufferEnd = breakpoints_.data() + breakpoints_.size();
    if (hitBreakpoint_.has_value()) {
        backendState.bpHitIndex = *hitBreakpoint_;
    } else {
        backendState.bpHitIndex = std::nullopt;
    }

    backendState.logInstructionStart = loggedInstructions_.data();
    backendState.logInstructionEnd = loggedInstructions_.data() + loggedInstructions_.size();

    //  read IO memory from bank 0xe0 which ignores memory shadow settings
    if (backendState.mmioWasInitialized) {
        for (uint16_t ioAddr = 0xc000; ioAddr < 0xc0ff; ++ioAddr) {
            clem_read(&machine, &backendState.ioPageValues[ioAddr - 0xc000], ioAddr, 0xe0,
                      CLEM_MEM_FLAG_NULL);
        }
    }
    backendState.debugMemoryPage = debugMemoryPage_;
    backendState.machineSpeedMhz = runSampler_.sampledMachineSpeedMhz;
    backendState.avgVBLsPerFrame = runSampler_.avgVBLsPerFrame;
    backendState.fastEmulationOn = runSampler_.emulatorVblsPerFrame > 1;

    GS_->finishFrame(backendState.frame);
    hitBreakpoint_ = std::nullopt;
    runSampler_.fastModeDisabledThisFrame = false;
}

#if defined(__GNUC__)
#if !defined(__clang__)
#pragma GCC diagnostic pop
#endif
#endif

std::optional<unsigned> ClemensBackend::checkHitBreakpoint() {
    auto &machine = GS_->getMachine();
    for (auto it = breakpoints_.begin(); it != breakpoints_.end(); ++it) {
        uint16_t b_adr = (uint16_t)(it->address & 0xffff);
        uint8_t b_bank = (uint8_t)(it->address >> 16);
        unsigned index = (unsigned)(it - breakpoints_.begin());
        switch (it->type) {
        case ClemensBackendBreakpoint::Execute:
            if (machine.cpu.regs.PBR == b_bank && machine.cpu.regs.PC == b_adr) {
                return index;
            }
            break;
        case ClemensBackendBreakpoint::DataRead:
        case ClemensBackendBreakpoint::Write:
            if (machine.cpu.pins.bank == b_bank && machine.cpu.pins.adr == b_adr) {
                if (machine.cpu.pins.vdaOut) {
                    if (it->type == ClemensBackendBreakpoint::DataRead && machine.cpu.pins.rwbOut) {
                        return index;
                    } else if (it->type == ClemensBackendBreakpoint::Write &&
                               !machine.cpu.pins.rwbOut) {
                        return index;
                    }
                }
            }
            break;
        case ClemensBackendBreakpoint::IRQ:
            if (machine.cpu.state_type == kClemensCPUStateType_IRQ) {
                return index;
            }
            break;
        case ClemensBackendBreakpoint::BRK:
            if (machine.cpu.regs.IR == CLEM_OPC_BRK) {
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

void ClemensBackend::assignPropertyToU32(MachineProperty property, uint32_t value) {
    auto &machine = GS_->getMachine();
    bool emulation = machine.cpu.pins.emulation;
    bool acc8 = emulation || (machine.cpu.regs.P & kClemensCPUStatus_MemoryAccumulator) != 0;
    bool idx8 = emulation || (machine.cpu.regs.P & kClemensCPUStatus_Index) != 0;

    switch (property) {
    case MachineProperty::RegA:
        if (acc8)
            machine.cpu.regs.A = (machine.cpu.regs.A & 0xff00) | (uint16_t)(value & 0xff);
        else
            machine.cpu.regs.A = (uint16_t)(value & 0xffff);
        break;
    case MachineProperty::RegB:
        machine.cpu.regs.A = (machine.cpu.regs.A & 0xff) | (uint16_t)((value & 0xff) << 8);
        break;
    case MachineProperty::RegC:
        machine.cpu.regs.A = (uint16_t)(value & 0xffff);
        break;
    case MachineProperty::RegX:
        if (emulation)
            machine.cpu.regs.X = (uint16_t)(value & 0xff);
        else if (idx8)
            machine.cpu.regs.X = (machine.cpu.regs.X & 0xff00) | (uint16_t)(value & 0xff);
        else
            machine.cpu.regs.X = (uint16_t)(value & 0xffff);
        break;
    case MachineProperty::RegY:
        if (emulation)
            machine.cpu.regs.Y = (uint16_t)(value & 0xff);
        else if (idx8)
            machine.cpu.regs.Y = (machine.cpu.regs.Y & 0xff00) | (uint16_t)(value & 0xff);
        else
            machine.cpu.regs.Y = (uint16_t)(value & 0xffff);
        break;
    case MachineProperty::RegP:
        if (emulation) {
            machine.cpu.regs.P = (value & 0x30); // do not affect MX flags
        } else {
            machine.cpu.regs.P = (value & 0xff);
        }
        break;
    case MachineProperty::RegD:
        machine.cpu.regs.D = (uint16_t)(value & 0xffff);
        break;
    case MachineProperty::RegSP:
        if (emulation) {
            machine.cpu.regs.S = (machine.cpu.regs.S & 0xff00) | (uint16_t)(value & 0xff);
        } else {
            machine.cpu.regs.S = (uint16_t)(value & 0xffff);
        }
        break;
    case MachineProperty::RegDBR:
        machine.cpu.regs.DBR = (uint8_t)(value & 0xff);
        break;
    case MachineProperty::RegPBR:
        machine.cpu.regs.PBR = (uint8_t)(value & 0xff);
        break;
    case MachineProperty::RegPC:
        machine.cpu.regs.PC = (uint16_t)(value & 0xff);
        break;
    };
}

bool ClemensBackend::queryConfig(ClemensAppleIIGSConfig &config) {
    if (!GS_)
        return false;
    GS_->saveConfig();
    config = gsConfig_;
    return true;
}

template <typename... Args>
void ClemensBackend::localLog(int log_level, const char *msg, Args... args) {
    if (logOutput_.size() >= kLogOutputLineLimit)
        return;
    ClemensBackendOutputText logLine{
        log_level,
    };
    logLine.text = fmt::format(msg, args...);
    fmt::print(log_level < CLEM_DEBUG_LOG_WARN ? stdout : stderr, "Backend: {}\n", logLine.text);
    logOutput_.emplace_back(logLine);
}

bool ClemensBackend::serialize(const std::string &path) const {
    ClemensSnapshot snapshot(path);

    return snapshot.serialize(*GS_, [this](mpack_writer_t *writer, ClemensAppleIIGS &) -> bool {
        mpack_start_map(writer, 1);
        mpack_write_cstr(writer, "breakpoints");
        mpack_start_array(writer, (uint32_t)breakpoints_.size());
        for (auto &breakpoint : breakpoints_) {
            mpack_start_map(writer, 2);
            mpack_write_cstr(writer, "type");
            mpack_write_i32(writer, static_cast<int>(breakpoint.type));
            mpack_write_cstr(writer, "address");
            mpack_write_u32(writer, breakpoint.address);
            mpack_finish_map(writer);
        }
        mpack_finish_array(writer);
        mpack_finish_map(writer);
        return mpack_writer_error(writer) == mpack_ok;
    });
}

bool ClemensBackend::unserialize(const std::string &path) {
    ClemensSnapshot snapshot(path);

    std::vector<ClemensBackendBreakpoint> breakpoints;

    auto gs = snapshot.unserialize(
        *this, [&breakpoints](mpack_reader_t *reader, ClemensAppleIIGS &) -> bool {
            //  detect if this snapshot requires remapping storage paths to the local host
            //  if platforms, differ then all paths will be remapped
            //  otherwise check each path and check if the file's parent directory exists
            //  if not, then mark as -to-remap-
            //  if any of the paths require remapping, abort here and the frontend should offer
            //  the option to remap paths - which will be passed to the unserializer on a second
            //  pass

            //  read debug settings
            mpack_expect_map(reader);
            mpack_expect_cstr_match(reader, "breakpoints");
            uint32_t breakpointCount = mpack_expect_array_max(reader, 1024);
            breakpoints.clear();
            breakpoints.reserve(breakpointCount);
            for (uint32_t breakpointIdx = 0; breakpointIdx < breakpointCount; ++breakpointIdx) {
                breakpoints.emplace_back();
                auto &breakpoint = breakpoints.back();
                mpack_expect_map(reader);
                mpack_expect_cstr_match(reader, "type");
                breakpoint.type =
                    static_cast<ClemensBackendBreakpoint::Type>(mpack_expect_i32(reader));
                mpack_expect_cstr_match(reader, "address");
                breakpoint.address = mpack_expect_u32(reader);
                mpack_done_map(reader);
            }
            mpack_done_array(reader);
            mpack_done_map(reader);

            return mpack_reader_error(reader) == mpack_ok;
        });
    if (!gs)
        return false;
    GS_->unmount();
    GS_ = std::move(gs);
    updateRTC();
    GS_->mount();
    breakpoints_ = std::move(breakpoints);
    return true;
}

void ClemensBackend::updateRTC() {
    GS_->setLocalEpochTime(get_local_epoch_time_delta_in_seconds());
    clocksInSecondPeriod_ = 0;
}

////////////////////////////////////////////////////////////////////////////////
//  ClemensAppleIIGS events
//
void ClemensBackend::onClemensSystemMachineLog(int logLevel, const ClemensMachine *,
                                               const char *msg) {
    spdlog::level::level_enum levelEnums[] = {spdlog::level::debug, spdlog::level::info,
                                              spdlog::level::warn, spdlog::level::warn,
                                              spdlog::level::err};
    if (logLevel_ > logLevel)
        return;
    if (logOutput_.size() >= kLogOutputLineLimit)
        return;

    if (logLevel >= CLEM_DEBUG_LOG_INFO) {
        spdlog::log(levelEnums[logLevel], "[a2gs] {}", msg);
    }

    logOutput_.emplace_back(ClemensBackendOutputText{logLevel, msg});
}
void ClemensBackend::onClemensSystemLocalLog(int logLevel, const char *msg) {
    if (logOutput_.size() >= kLogOutputLineLimit)
        return;
    ClemensBackendOutputText logLine{
        logLevel,
    };
    logLine.text = msg;
    fmt::print(logLevel < CLEM_DEBUG_LOG_WARN ? stdout : stderr, "Backend: {}\n", logLine.text);
    logOutput_.emplace_back(logLine);
}

void ClemensBackend::onClemensSystemWriteConfig(const ClemensAppleIIGS::Config &config) {
    gsConfig_ = config;
    gsConfigUpdated_ = true;
}

//  If enabled, this emulator issues this callback per instruction
//  This is great for debugging but should be disabled otherwise (see TODO)
void ClemensBackend::onClemensInstruction(struct ClemensInstruction *inst, const char *operand) {
    if (programTrace_) {
        programTrace_->addExecutedInstruction(nextTraceSeq_++, *inst, operand, GS_->getMachine());
    }
    if (!areInstructionsLogged_)
        return;
    loggedInstructions_.emplace_back();
    auto &loggedInst = loggedInstructions_.back();
    loggedInst.data = *inst;
    strncpy(loggedInst.operand, operand, sizeof(loggedInst.operand) - 1);
    loggedInst.operand[sizeof(loggedInst.operand) - 1] = '\0';
}

////////////////////////////////////////////////////////////////////////////////
// ClemensCommandQueue handlers
//
void ClemensBackend::onCommandReset() { GS_->reset(); }

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
    if (!clemens_is_initialized_simple(&GS_->getMachine())) {
        return;
    }
    clemens_input(&GS_->getMMIO(), &inputEvent);
}

bool ClemensBackend::onCommandInsertDisk(ClemensDriveType driveType, std::string diskPath) {
    bool result = GS_->getStorage().insertDisk(GS_->getMMIO(), driveType, diskPath);
    if (result) {
        GS_->saveConfig();
    }
    return result;
}

void ClemensBackend::onCommandEjectDisk(ClemensDriveType driveType) {
    GS_->getStorage().ejectDisk(GS_->getMMIO(), driveType);
    GS_->saveConfig();
}

bool ClemensBackend::onCommandWriteProtectDisk(ClemensDriveType driveType, bool wp) {
    GS_->getStorage().writeProtectDisk(GS_->getMMIO(), driveType, wp);
    return true;
}

bool ClemensBackend::onCommandInsertSmartPortDisk(unsigned driveIndex, std::string diskPath) {
    bool result = GS_->getStorage().assignSmartPortDisk(GS_->getMMIO(), driveIndex, diskPath);
    GS_->saveConfig();
    return result;
}

void ClemensBackend::onCommandEjectSmartPortDisk(unsigned driveIndex) {
    GS_->getStorage().ejectSmartPortDisk(GS_->getMMIO(), driveIndex);
}

void ClemensBackend::onCommandDebugMemoryPage(uint8_t pageIndex) { debugMemoryPage_ = pageIndex; }

void ClemensBackend::onCommandDebugMemoryWrite(uint16_t addr, uint8_t value) {
    clem_write(&GS_->getMachine(), value, addr, debugMemoryPage_, CLEM_MEM_FLAG_NULL);
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
        if (programTrace_->isIWMLoggingEnabled()) {
            clem_iwm_debug_stop(&GS_->getMMIO().dev_iwm);
            programTrace_->enableIWMLogging(false);
        }
        programTrace_ = nullptr;
    }
    if (programTrace_) {
        if (op == "iwm") {
            programTrace_->enableIWMLogging(!programTrace_->isIWMLoggingEnabled());
            fmt::print("{} tracing = {}\n", op, programTrace_->isIWMLoggingEnabled());
            if (programTrace_->isIWMLoggingEnabled()) {
                clem_iwm_debug_start(&GS_->getMMIO().dev_iwm);
            } else {
                clem_iwm_debug_stop(&GS_->getMMIO().dev_iwm);
            }
        } else {
            fmt::print("{} tracing is not recognized.\n", op);
        }
    }
    return ok;
}

bool ClemensBackend::onCommandSaveMachine(std::string path) {
    auto outputPath = std::filesystem::path(config_.snapshotRootPath) / path;

    return serialize(outputPath.string());
}

bool ClemensBackend::onCommandLoadMachine(std::string path) {
    auto snapshotPath = std::filesystem::path(config_.snapshotRootPath) / path;
    return unserialize(snapshotPath.string());
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
    localLog(CLEM_DEBUG_LOG_INFO, "{} fast disk emulation when IWM is active\n",
             enabled ? "Enable" : "Disable");
    config_.enableFastEmulation = enabled;
}

//  TODO: remove when interpreter commands are supported!
static std::string_view trimToken(const std::string_view &token, size_t off = 0,
                                  size_t length = std::string_view::npos) {
    auto tmp = token.substr(off, length);
    auto left = tmp.begin();
    for (; left != tmp.end(); ++left) {
        if (!std::isspace(*left))
            break;
    }
    tmp = tmp.substr(left - tmp.begin());
    auto right = tmp.rbegin();
    for (; right != tmp.rend(); ++right) {
        if (!std::isspace(*right))
            break;
    }
    tmp = tmp.substr(0, tmp.size() - (right - tmp.rbegin()));
    return tmp;
}
static std::tuple<std::array<std::string_view, 8>, std::string_view, size_t>
gatherMessageParams(std::string_view &message, bool with_cmd = false) {
    std::array<std::string_view, 8> params;
    size_t paramCount = 0;

    size_t sepPos = std::string_view::npos;
    std::string_view cmd;
    if (with_cmd) {
        sepPos = message.find(' ');
        cmd = message.substr(0, sepPos);
    }
    if (sepPos != std::string_view::npos) {
        message = message.substr(sepPos + 1);
    }
    while (!message.empty() && paramCount < params.size()) {
        sepPos = message.find(',');
        params[paramCount++] = trimToken(message.substr(0, sepPos));
        if (sepPos != std::string_view::npos) {
            message = message.substr(sepPos + 1);
        } else {
            message = "";
        }
    }
    return {params, cmd, paramCount};
}

std::string ClemensBackend::onCommandDebugMessage(std::string msg) {
    std::string_view debugmsg(msg);
    auto [params, cmd, paramCount] = gatherMessageParams(debugmsg, true);
    if (cmd != "dump") {
        return fmt::format("UNK:{}", cmd);
    }
    unsigned startBank, endBank;
    if (std::from_chars(params[0].data(), params[0].data() + params[0].size(), startBank, 16).ec !=
        std::errc{}) {
        return fmt::format("FAIL:{} {},{}", cmd, params[2], params[3]);
    }
    if (std::from_chars(params[1].data(), params[1].data() + params[1].size(), endBank, 16).ec !=
        std::errc{}) {
        return fmt::format("FAIL:{} {},{}", cmd, params[2], params[3]);
    }
    startBank &= 0xff;
    endBank &= 0xff;

    size_t dumpedMemorySize = ((endBank - startBank) + 1) << 16;
    unsigned dumpedMemoryAddress = startBank << 16;
    std::vector<uint8_t> dumpedMemory(dumpedMemorySize);
    uint8_t *memoryOut = dumpedMemory.data();
    for (; startBank <= endBank; ++startBank, memoryOut += 0x10000) {
        clemens_out_bin_data(&GS_->getMachine(), memoryOut, 0x10000, startBank, 0);
    }

    auto outPath = std::filesystem::path(config_.traceRootPath) / params[2];
    std::ios_base::openmode flags = std::ios_base::out | std::ios_base::binary;
    std::ofstream outstream(outPath, flags);
    if (outstream.is_open()) {
        if (params[3] == "bin") {
            outstream.write((char *)dumpedMemory.data(), dumpedMemorySize);
            outstream.close();
        } else {
            constexpr unsigned kHexByteCountPerLine = 64;
            char hexDump[kHexByteCountPerLine * 2 + 8 + 1];
            unsigned adrBegin = dumpedMemoryAddress;
            unsigned adrEnd = dumpedMemoryAddress + dumpedMemorySize;
            uint8_t *memoryOut = dumpedMemory.data();
            while (adrBegin < adrEnd) {
                snprintf(hexDump, sizeof(hexDump), "%06X: ", adrBegin);
                clemens_out_hex_data_from_memory(hexDump + 8, memoryOut, kHexByteCountPerLine * 2,
                                                 adrBegin);
                hexDump[sizeof(hexDump) - 1] = '\n';
                outstream.write(hexDump, sizeof(hexDump));
                adrBegin += 0x40;
                memoryOut += 0x40;
            }
            outstream.close();
        }
    }
    return fmt::format("OK:{} {},{}", cmd, params[2], params[3]);
}

void ClemensBackend::onCommandSendText(std::string text) {
    clipboardText_.erase(0, clipboardHead_);
    clipboardHead_ = 0;
    clipboardText_.append(text);
}

bool ClemensBackend::onCommandBinaryLoad(std::string pathname, unsigned address) {
    std::ifstream input(pathname, std::ios_base::binary);
    if (!input.is_open()) {
        localLog(CLEM_DEBUG_LOG_WARN, "Unable to open '{}' for binary load.", pathname);
        return false;
    }
    auto length = input.seekg(0, std::ios::end).tellg();
    input.seekg(0, std::ios::beg);
    std::vector<uint8_t> data(length);
    if (input.read((char *)data.data(), data.size()).fail()) {
        localLog(CLEM_DEBUG_LOG_WARN, "Unable to read {} bytes from '{}' for binary load.", length,
                 pathname);
        return false;
    }
    input.close();

    bool ok = GS_->writeDataToMemory(data.data(), address, (unsigned)data.size());
    if (ok) {
        localLog(CLEM_DEBUG_LOG_INFO, "Loaded {} bytes from '{}'.", length, pathname);
    }
    return ok;
}

bool ClemensBackend::onCommandBinarySave(std::string pathname, unsigned address, unsigned length) {
    std::ofstream output(pathname, std::ios_base::binary);
    if (!output.is_open()) {
        localLog(CLEM_DEBUG_LOG_WARN, "Unable to open '{}' for binary write.", pathname);
        return false;
    }
    std::vector<uint8_t> data(length);
    if (!GS_->readDataFromMemory(data.data(), address, (unsigned)data.size())) {
        localLog(CLEM_DEBUG_LOG_WARN, "Unable to read {} bytes from ${:x} for binary save to '{}'.",
                 length, address, pathname);
        return false;
    }
    if (output.write((char *)data.data(), data.size()).fail()) {
        localLog(CLEM_DEBUG_LOG_WARN, "Unable to save {} bytes to '{}' for binary save.", length,
                 pathname);
        return false;
    }
    localLog(CLEM_DEBUG_LOG_INFO, "Saved {} bytes to '{}'.", length, pathname);
    return true;
}
