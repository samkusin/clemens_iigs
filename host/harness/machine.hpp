#ifndef CLEM_HOST_TEST_HARNESS_MACHINE_HPP
#define CLEM_HOST_TEST_HARNESS_MACHINE_HPP

#include "clem_mmio_types.h"
#include "clem_types.h"

#include <string>

class ClemensTestHarness {
  public:
    ClemensTestHarness();

    ClemensMachine &getMachine() { return core_; }
    ClemensMMIO &getMMIO() { return mmio_; }
    bool hasFailed() const { return failed_; }

    void reset();
    void step(unsigned cnt, unsigned statusStepRate);
    void printStatus();
    void log(int level, const std::string &message);

  private:
    static void logger(int level, ClemensMachine *machine, const char *msg);
    static void opcode(struct ClemensInstruction *inst, const char *operand, void *userptr);

    void stepOne();

    void setupBootROM();

    ClemensMachine core_;
    ClemensMMIO mmio_;
    uint8_t mega2Memory_[128 * 1024];
    uint8_t fpiMemory_[128 * 1024];
    uint8_t rom_[64 * 1024];
    uint8_t cards_[2048 * CLEM_CARD_SLOT_COUNT];

    char execout_[32];
    uint64_t execCounter_;
    bool failed_;
};

#endif
