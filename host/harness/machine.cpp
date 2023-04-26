#include "machine.hpp"

#include "clem_defs.h"
#include "clem_disk.h"
#include "disklib/clem_disk_utils.hpp"
#include "emulator.h"
#include "emulator_mmio.h"

#include "fmt/color.h"
#include "fmt/format.h"

#include <cstdio>
#include <cstdlib>
#include <string>

static constexpr unsigned kEmulatorSlabMemory = 16 * 1024 * 1024;

ClemensTestHarness::ClemensTestHarness()
    : slab_((uint8_t *)malloc(kEmulatorSlabMemory), kEmulatorSlabMemory),
      execCounter_(0), core_{}, mmio_{} {
    unsigned kFPIBankCount = 4;
    unsigned kFPIROMBankCount = sizeof(rom_) / CLEM_IIGS_BANK_SIZE;
    int result = clemens_init(
        &core_, CLEM_CLOCKS_PHI0_CYCLE, CLEM_CLOCKS_PHI2_FAST_CYCLE, rom_, kFPIROMBankCount,
        emulatorMemoryAllocate(CLEM_EMULATOR_ALLOCATION_MEGA2_MEMORY_BANK, 1, this),
        emulatorMemoryAllocate(CLEM_EMULATOR_ALLOCATION_MEGA2_MEMORY_BANK, 1, this),
        emulatorMemoryAllocate(CLEM_EMULATOR_ALLOCATION_FPI_MEMORY_BANK, kFPIBankCount, this),
        kFPIBankCount);
    failed_ = result < 0;
    if (failed_) {
        fmt::print(stderr, fg(fmt::terminal_color::bright_red) | fmt::emphasis::bold,
                   "Error executing clemens_init() = {}\n", result);
        return;
    }
    clemens_host_setup(&core_, &ClemensTestHarness::logger, this);

    clem_mmio_init(
        &mmio_, &core_.dev_debug, core_.mem.bank_page_map,
        emulatorMemoryAllocate(CLEM_EMULATOR_ALLOCATION_CARD_BUFFER, CLEM_CARD_SLOT_COUNT, this),
        kFPIBankCount, kFPIROMBankCount, core_.mem.mega2_bank_map[0], core_.mem.mega2_bank_map[1],
        &core_.tspec);

    setupBootROM();
}

ClemensTestHarness::~ClemensTestHarness() { free(slab_.getHead()); }

void ClemensTestHarness::log(int level, const std::string &message) {
    logger(level, &core_, message.c_str());
}

void ClemensTestHarness::logger(int level, ClemensMachine *machine, const char *msg) {
    auto *self = reinterpret_cast<ClemensTestHarness *>(machine->debug_user_ptr);
    auto color = fg(fmt::terminal_color::white);
    static const char *names[] = {"DEBUG", "INFO", "WARN", "UNIMP", "FATAL"};
    FILE *fpout = stdout;
    switch (level) {
    case CLEM_DEBUG_LOG_UNIMPL:
        color = fg(fmt::terminal_color::bright_red) | fmt::emphasis::bold;
        fpout = stderr;
        break;
    case CLEM_DEBUG_LOG_FATAL:
        color = fg(fmt::terminal_color::bright_red) | fmt::emphasis::bold;
        fpout = stderr;
        break;
    case CLEM_DEBUG_LOG_WARN:
        color = fg(fmt::terminal_color::yellow) | fmt::emphasis::bold;
        fpout = stderr;
        break;
    case CLEM_DEBUG_LOG_DEBUG:
        color = fg(fmt::terminal_color::white) | fmt::emphasis::faint;
        break;
    }
    fmt::print(
        fpout, "[{:<16}][{:<5}] {}\n",
        fmt::styled(self->execCounter_, fg(fmt::terminal_color::white) | fmt::emphasis::faint),
        fmt::styled(names[level], color | fmt::emphasis::bold), fmt::styled(msg, color));
}

void ClemensTestHarness::opcode(struct ClemensInstruction *inst, const char *operand,
                                void *userptr) {
    auto *self = reinterpret_cast<ClemensTestHarness *>(userptr);
    auto execColor = fg(fmt::terminal_color::white) | fmt::emphasis::faint;
    fmt::print(execColor, "[{:<16}][EXEC  ] {} {}\n", self->execCounter_, inst->desc->name,
               operand);
}

uint8_t *ClemensTestHarness::emulatorMemoryAllocate(unsigned type, unsigned sz, void *context) {
    uint8_t *block = nullptr;
    auto *self = reinterpret_cast<ClemensTestHarness *>(context);
    unsigned bytesSize;
    switch (type) {
    case CLEM_EMULATOR_ALLOCATION_FPI_MEMORY_BANK:
        bytesSize = sz * CLEM_IIGS_BANK_SIZE;
        break;
    case CLEM_EMULATOR_ALLOCATION_MEGA2_MEMORY_BANK:
        bytesSize = sz * CLEM_IIGS_BANK_SIZE;
        break;
    case CLEM_EMULATOR_ALLOCATION_DISK_NIB_3_5:
        bytesSize = sz * CLEM_DISK_35_MAX_DATA_SIZE;
        break;
    case CLEM_EMULATOR_ALLOCATION_DISK_NIB_5_25:
        bytesSize = sz * CLEM_DISK_525_MAX_DATA_SIZE;
        break;
    case CLEM_EMULATOR_ALLOCATION_CARD_BUFFER:
        bytesSize = sz * 2048;
        break;
    default:
        bytesSize = sz;
        break;
    }
    block = self->slab_.forwardSize(bytesSize).first;
    return block;
}

void ClemensTestHarness::setupBootROM() {
    rom_[CLEM_6502_RESET_VECTOR_LO_ADDR] = 0x62;
    rom_[CLEM_6502_RESET_VECTOR_HI_ADDR] = 0xfa;
    //  BRA -2 (infinite loop)
    rom_[0xfa62] = 0x80;
    rom_[0xfa63] = 0xFE;
}

void ClemensTestHarness::stepOne() {
    clemens_emulate_cpu(&core_);
    clemens_emulate_mmio(&core_, &mmio_);
    execCounter_++;
}

void ClemensTestHarness::reset() {
    //  TODO: this could be a simple API clemens_cpu_reset();
    if (failed_)
        return;

    execCounter_ = 0;

    core_.cpu.pins.resbIn = false;
    core_.resb_counter = 3;
    while (!core_.cpu.pins.resbIn) {
        printStatus();
        stepOne();
    }
}

void ClemensTestHarness::step(unsigned cnt, unsigned statusStepRate) {
    if (failed_)
        return;

    for (unsigned i = 0; i < cnt; ++i) {
        bool outputStatus = statusStepRate > 0 && (i % statusStepRate) == 0;
        if (outputStatus) {
            printStatus();
            clemens_opcode_callback(&core_, &ClemensTestHarness::opcode);
        }
        stepOne();
        if (outputStatus) {
            clemens_opcode_callback(&core_, NULL);
        }
    }
}

void ClemensTestHarness::printStatus() {
    auto lite = fg(fmt::terminal_color::white);
    auto dark = fg(fmt::terminal_color::white) | fmt::emphasis::conceal;
    uint8_t p = core_.cpu.regs.P;
    bool emul = core_.cpu.pins.emulation;
    auto execColor = fg(fmt::terminal_color::white) | fmt::emphasis::faint;

    fmt::print(execColor, "[{:<16}][STAT ] {}{}{}  {}{}{}{}{}{}{}{} ", execCounter_,
               core_.cpu.pins.resbIn ? ' ' : 'r', core_.cpu.pins.irqbIn ? ' ' : 'i',
               emul ? 'e' : ' ', fmt::styled('n', p & kClemensCPUStatus_Negative ? lite : dark),
               fmt::styled('v', p & kClemensCPUStatus_Overflow ? lite : dark),
               fmt::styled(emul ? ' ' : 'm', p & kClemensCPUStatus_MemoryAccumulator ? lite : dark),
               fmt::styled(emul ? ' ' : 'x', p & kClemensCPUStatus_Index ? lite : dark),
               fmt::styled('d', p & kClemensCPUStatus_Negative ? lite : dark),
               fmt::styled('i', p & kClemensCPUStatus_IRQDisable ? lite : dark),
               fmt::styled('z', p & kClemensCPUStatus_Zero ? lite : dark),
               fmt::styled('c', p & kClemensCPUStatus_Carry ? lite : dark));
    fmt::print(execColor, "{:02x}/{:04x}\n", core_.cpu.regs.PBR, core_.cpu.regs.PC);
}

bool ClemensTestHarness::run(nlohmann::json &command) {
    auto action = command.find("act");
    if (action == command.end())
        return false;
    if (!action->is_string())
        return false;
    auto actionName = action->get<std::string>();
    auto param = command.find("param");
    fmt::print("[{:<16}][CMD  ] {}\n", execCounter_, command.dump());

    if (actionName == "reset") {
        reset();
        return true;
    } else if (actionName == "step") {
        if (param == command.end()) {
            step(1, 0);
            return true;
        } else if (param->is_number()) {
            step(param->get<unsigned>(), 0);
            return true;
        } else if (param->is_object()) {
            unsigned statFrequency = 0;
            unsigned count = 1;
            auto freq = param->find("status");
            if (freq != param->end())
                statFrequency = freq->get<unsigned>();
            auto cnt = param->find("count");
            if (cnt != param->end())
                count = cnt->get<unsigned>();
            step(count, statFrequency);
            return true;
        }
    } else if (actionName == "insert_disk") {
        if (param != command.end()) {
            ClemensDriveType driveType = kClemensDrive_Invalid;
            std::string imageName;
            auto drive = param->find("drive");
            if (drive != param->end()) {
                driveType = ClemensDiskUtilities::getDriveType(drive->get<std::string>());
            }
            auto disk = param->find("disk");
            if (disk != param->end()) {
                imageName = disk->get<std::string>();
            }
            if (driveType != kClemensDrive_Invalid && !imageName.empty()) {
                failed_ = !storageUnit_.insertDisk(mmio_, driveType, imageName);
                return true;
            }
        }
    } else if (actionName == "eject_disk") {
        if (param != command.end()) {
            ClemensDriveType driveType = kClemensDrive_Invalid;
            std::string imageName;
            auto drive = param->find("drive");
            if (drive != param->end()) {
                driveType = ClemensDiskUtilities::getDriveType(drive->get<std::string>());
            }
            if (driveType != kClemensDrive_Invalid) {
                storageUnit_.ejectDisk(mmio_, driveType);
                return true;
            }
        }
    }
    return false;
}
