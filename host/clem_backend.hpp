#ifndef CLEM_HOST_BACKEND_HPP
#define CLEM_HOST_BACKEND_HPP

#include "clem_types.h"
#include "cinek/fixedstack.hpp"
#include "cinek/buffer.hpp"

#include <condition_variable>
#include <deque>
#include <mutex>
#include <string>
#include <thread>


//  TODO: Machine type logic could be subclassed into an Apple2GS backend, etc.
class ClemensBackend {
public:
  enum class Type {
    Apple2GS
  };
  struct Config {
    unsigned audioSamplesPerSecond;
    Type type;
  };
  ClemensBackend(std::string romPathname, const Config& config);
  ~ClemensBackend();

  //  Queues a command to the backend.   Most commands are processed on the next
  //  execution frame.  Certain commands may hold the queue (i.e. like wait())
  //  Priority commands like Cancel or Terminate are pushed to the front,
  //  overriding commands like Wait.
  void terminate();

  //  Issues a soft reset to the machine.   This is roughly equivalent to pressing
  //  the power button.
  void reset();

  //  The host should expect emulator state refreshes at this frequency if in
  //  run mode
  void setRefreshFrequency(unsigned hz);

private:
  struct Command {
    enum Type {
      Undefined,
      Terminate,
      SetHostUpdateFrequency,
      ResetMachine
    };
    Type type;
    std::string operand;
  };

  void resetMachine();

  void run();
  void queue(const Command& cmd);
  void queueToFront(const Command& cmd);

  cinek::CharBuffer loadROM(const char* romPathname);
  //  TODO: These methods could be moved into a subclass as they are specific
  //        to machine type
  void initApple2GS();
  void loadBRAM();
  void saveBRAM();

  static void emulatorLog(int log_level, ClemensMachine *machine,
                          const char *msg);
  static void emulatorOpcodeCallback(struct ClemensInstruction *inst,
                                     const char *operand, void *this_ptr);

private:
  Config config_;
  std::thread runner_;
  std::deque<Command> commandQueue_;
  std::mutex commandQueueMutex_;
  std::condition_variable commandQueueCondition_;

  //  memory allocated once for the machine
  cinek::FixedStack slabMemory_;
  //  the actual machine object
  cinek::CharBuffer romBuffer_;
  ClemensMachine machine_;
  uint64_t emulatorStepsSinceReset_;
};

#endif
