#ifndef CLEM_HOST_TEST_HARNESS_MACHINE_HPP
#define CLEM_HOST_TEST_HARNESS_MACHINE_HPP

#include "core/clem_storage_unit.hpp"

#include "clem_mmio_types.h"
#include "clem_types.h"

#include "cinek/buffer.hpp"
#include "ext/json.hpp"

#include <string>

class ClemensTestHarness {
  public:
    ClemensTestHarness();
    ~ClemensTestHarness();

    ClemensMachine &getMachine() { return core_; }
    ClemensMMIO &getMMIO() { return mmio_; }
    bool hasFailed() const { return failed_; }

    bool run(nlohmann::json &command);

    void reset();
    void step(unsigned cnt, unsigned statusStepRate);
    void printStatus();
    void log(int level, const std::string &message);

  private:
    static void logger(int level, ClemensMachine *machine, const char *msg);
    static void opcode(struct ClemensInstruction *inst, const char *operand, void *userptr);
    static uint8_t *emulatorMemoryAllocate(unsigned type, unsigned sz, void *context);

    void stepOne();

    void setupBootROM();

    cinek::ByteBuffer slab_;
    uint8_t rom_[64 * 1024];
    uint64_t execCounter_;
    bool failed_;

    ClemensMachine core_;
    ClemensMMIO mmio_;
    ClemensStorageUnit storageUnit_;
};

#endif
