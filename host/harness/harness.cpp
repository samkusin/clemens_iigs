#include "harness.hpp"

#include "core/clem_apple2gs.hpp"
#include "core/clem_disk_utils.hpp"

#include "emulator.h"
#include "emulator_mmio.h"

#include "fmt/color.h"
#include "fmt/format.h"

ClemensTestHarness::ClemensTestHarness() : execCounter_(0), failed_(false) {
    ClemensAppleIIGS::Config config{};
    config.audioSamplesPerSecond = 48000;
    config.memory = 256;
    a2gs_ = std::make_unique<ClemensAppleIIGS>(config, *this);
    if (a2gs_->isOk()) {
        localLog(CLEM_DEBUG_LOG_INFO, "STAT", "Machine created.");
    } else {
        localLog(CLEM_DEBUG_LOG_FATAL, "STAT", "Machine failed to initialize.");
        failed_ = true;
    }
}

ClemensTestHarness::~ClemensTestHarness() {}

bool ClemensTestHarness::run(nlohmann::json &command) {
    if (!a2gs_->isOk())
        return false;

    auto action = command.find("act");
    if (action == command.end())
        return false;
    if (!action->is_string())
        return false;
    auto actionName = action->get<std::string>();
    auto paramIt = command.find("param");
    auto params = (paramIt != command.end()) ? *paramIt : nlohmann::json();

    localLog(CLEM_DEBUG_LOG_INFO, "CMD", "{}", command.dump());

    if (actionName == "reset") {
        return reset();
    } else if (actionName == "step") {
        return step(params);
    } else if (actionName == "insert_disk") {
        return insertDisk(params);
    } else if (actionName == "eject_disk") {
        return ejectDisk(params);
    }
    return false;
}

bool ClemensTestHarness::reset() {
    execCounter_ = 0;
    a2gs_->reset();
    while (clemens_is_resetting(&a2gs_->getMachine())) {
        printStatus();
        a2gs_->stepMachine();
        execCounter_++;
    }
    return true;
}

bool ClemensTestHarness::step(nlohmann::json params) {
    if (params.empty()) {
        step(1, 0);
    } else if (params.is_number_integer()) {
        step(params.get<unsigned>(), 0);
    } else if (params.is_object()) {
        unsigned statFrequency = 0;
        unsigned count = 1;
        auto freq = params.find("status");
        if (freq != params.end())
            statFrequency = freq->get<unsigned>();
        auto cnt = params.find("count");
        if (cnt != params.end())
            count = cnt->get<unsigned>();
        step(count, statFrequency);
    } else {
        return false;
    }
    return true;
}

bool ClemensTestHarness::insertDisk(nlohmann::json params) {
    if (params.empty()) {
        localLog(CLEM_DEBUG_LOG_WARN, "CMD", "insertDisk <drive_name> <image_path>");
        return false;
    }
    ClemensDriveType driveType = kClemensDrive_Invalid;
    std::string imageName;
    auto drive = params.find("drive");
    if (drive != params.end()) {
        driveType = ClemensDiskUtilities::getDriveType(drive->get<std::string>());
    }
    auto disk = params.find("disk");
    if (disk != params.end()) {
        imageName = disk->get<std::string>();
    }
    if (driveType == kClemensDrive_Invalid || imageName.empty()) {
        localLog(CLEM_DEBUG_LOG_WARN, "CMD", "insertDisk invalid parameters");
        return false;
    }
    return a2gs_->getStorage().insertDisk(a2gs_->getMMIO(), driveType, imageName);
}

bool ClemensTestHarness::ejectDisk(nlohmann::json params) {
    if (params.empty()) {
        localLog(CLEM_DEBUG_LOG_WARN, "CMD", "ejectDisk <drive_name>");
        return false;
    }

    ClemensDriveType driveType = kClemensDrive_Invalid;
    auto drive = params.find("drive");
    if (drive != params.end()) {
        driveType = ClemensDiskUtilities::getDriveType(drive->get<std::string>());
    }
    if (driveType == kClemensDrive_Invalid) {
        localLog(CLEM_DEBUG_LOG_WARN, "CMD", "ejectDisk invalid parameters");
        return false;
    }

    return a2gs_->getStorage().ejectDisk(a2gs_->getMMIO(), driveType);
}

void ClemensTestHarness::step(unsigned cnt, unsigned statusStepRate) {
    if (failed_)
        return;

    for (unsigned i = 0; i < cnt; ++i) {
        bool outputStatus = statusStepRate > 0 && (i % statusStepRate) == 0;
        if (outputStatus) {
            printStatus();
            clemens_opcode_callback(&a2gs_->getMachine(), &ClemensTestHarness::opcode);
        }
        a2gs_->stepMachine();
        execCounter_++;
        if (outputStatus) {
            clemens_opcode_callback(&a2gs_->getMachine(), NULL);
        }
    }
}

void ClemensTestHarness::printStatus() {
    auto lite = fg(fmt::terminal_color::white);
    auto dark = fg(fmt::terminal_color::white) | fmt::emphasis::conceal;
    auto &core = a2gs_->getMachine();
    uint8_t p = core.cpu.regs.P;
    bool emul = core.cpu.pins.emulation;
    auto execColor = fg(fmt::terminal_color::white) | fmt::emphasis::faint;

    fmt::print(execColor, "[{:<16}][STAT ] {}{}{}  {}{}{}{}{}{}{}{} ", execCounter_,
               core.cpu.pins.resbIn ? ' ' : 'r', core.cpu.pins.irqbIn ? ' ' : 'i', emul ? 'e' : ' ',
               fmt::styled('n', p & kClemensCPUStatus_Negative ? lite : dark),
               fmt::styled('v', p & kClemensCPUStatus_Overflow ? lite : dark),
               fmt::styled(emul ? ' ' : 'm', p & kClemensCPUStatus_MemoryAccumulator ? lite : dark),
               fmt::styled(emul ? ' ' : 'x', p & kClemensCPUStatus_Index ? lite : dark),
               fmt::styled('d', p & kClemensCPUStatus_Negative ? lite : dark),
               fmt::styled('i', p & kClemensCPUStatus_IRQDisable ? lite : dark),
               fmt::styled('z', p & kClemensCPUStatus_Zero ? lite : dark),
               fmt::styled('c', p & kClemensCPUStatus_Carry ? lite : dark));
    fmt::print(execColor, "{:02x}/{:04x}\n", core.cpu.regs.PBR, core.cpu.regs.PC);
}

template <typename... Args>
void ClemensTestHarness::localLog(int logLevel, const char *type, const char *msg, Args... args) {
    //  TODO: There's probably a better way to do this in fmt... but this shouldn't be called
    //        often and we're not super concerned with performance in the test harness.
    auto text = fmt::format(msg, args...);
    fmt::print(logLevel >= CLEM_DEBUG_LOG_WARN ? stderr : stdout, "[{:<16}][{:<5}] {}\n",
               execCounter_, type, text);
}

void ClemensTestHarness::opcode(struct ClemensInstruction *inst, const char *operand,
                                void *userptr) {
    auto *self = reinterpret_cast<ClemensTestHarness *>(userptr);
    auto execColor = fg(fmt::terminal_color::white) | fmt::emphasis::faint;
    fmt::print(execColor, "[{:<16}][EXEC  ] {} {}\n", self->execCounter_, inst->desc->name,
               operand);
}

void ClemensTestHarness::onClemensSystemMachineLog(int logLevel, const ClemensMachine *,
                                                   const char *msg) {
    fmt::print(logLevel >= CLEM_DEBUG_LOG_WARN ? stderr : stdout, "[{:<16}][CLEM ] {}\n",
               execCounter_, msg);
}

void ClemensTestHarness::onClemensSystemLocalLog(int logLevel, const char *msg) {
    fmt::print(logLevel >= CLEM_DEBUG_LOG_WARN ? stderr : stdout, "[{:<16}][A2GS ] {}\n",
               execCounter_, msg);
}

void ClemensTestHarness::onClemensSystemWriteConfig(const ClemensAppleIIGS::Config &config) {
    // TODO:
}
