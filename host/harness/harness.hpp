#ifndef CLEM_HOST_TEST_HARNESS_HPP
#define CLEM_HOST_TEST_HARNESS_HPP

#include "core/clem_apple2gs.hpp"

#include "json.hpp"

#include <memory>

class ClemensTestHarness : public ClemensSystemListener {
  public:
    ClemensTestHarness();
    ~ClemensTestHarness();
    bool hasFailed() const { return failed_; }

    bool run(nlohmann::json &command);

  private:
    static void opcode(struct ClemensInstruction *inst, const char *operand, void *userptr);

    void onClemensSystemMachineLog(int logLevel, const ClemensMachine *machine,
                                   const char *msg) final;
    void onClemensSystemLocalLog(int logLevel, const char *msg) final;
    void onClemensSystemWriteConfig(const ClemensAppleIIGS::Config &config) final;

  private:
    bool reset();
    bool step(nlohmann::json params);
    bool insertDisk(nlohmann::json params);
    bool ejectDisk(nlohmann::json params);

  private:
    void step(unsigned cnt, unsigned statusStepRate);
    void printStatus();

    template <typename... Args>
    void localLog(int log_level, const char *type, const char *msg, Args... args);

    std::unique_ptr<ClemensAppleIIGS> a2gs_;
    uint64_t execCounter_;
    bool failed_;
};

#endif
