#include "machine.hpp"

#include "emulator.h"
#include "emulator_mmio.h"

#include "fmt/color.h"
#include "fmt/format.h"

#include <cstdio>
#include <string>

ClemensTestHarness::ClemensTestHarness() : core_{}, mmio_{}, execCounter_(0) {
    unsigned kFPIBankCount = sizeof(fpiMemory_) / CLEM_IIGS_BANK_SIZE;
    unsigned kFPIROMBankCount = sizeof(rom_) / CLEM_IIGS_BANK_SIZE;
    int result = clemens_init(&core_, CLEM_CLOCKS_PHI0_CYCLE, CLEM_CLOCKS_PHI2_FAST_CYCLE, rom_,
                              kFPIROMBankCount, &mega2Memory_[0], &mega2Memory_[65536], fpiMemory_,
                              kFPIBankCount);
    failed_ = result < 0;
    if (failed_) {
        fmt::print(stderr, fg(fmt::terminal_color::bright_red) | fmt::emphasis::bold,
                   "Error executing clemens_init() = {}\n", result);
        return;
    }
    clemens_host_setup(&core_, &ClemensTestHarness::logger, this);

    clem_mmio_init(&mmio_, &core_.dev_debug, core_.mem.bank_page_map, cards_, kFPIBankCount,
                   kFPIROMBankCount, core_.mem.mega2_bank_map[0], core_.mem.mega2_bank_map[1],
                   &core_.tspec);

    setupBootROM();
}

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
