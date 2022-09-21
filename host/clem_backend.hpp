#ifndef CLEM_HOST_BACKEND_HPP
#define CLEM_HOST_BACKEND_HPP

#include "clem_host_shared.hpp"

#include "cinek/buffer.hpp"
#include "cinek/fixedstack.hpp"
#include "clem_woz.h"

#include <array>
#include <condition_variable>
#include <deque>
#include <functional>
#include <mutex>
#include <string>
#include <string_view>
#include <thread>
#include <vector>

//  TODO: Machine type logic could be subclassed into an Apple2GS backend, etc.
class ClemensBackend {
public:
  using Config = ClemensBackendConfig;

  //  The PublishStateDelegate provides backend state to the front end GUI
  //  Data passed through the delegate is guaranteed to be valid within its
  //    scope.  The front-end should copy what it needs for its display during
  //    the delegate's scope. Once the delegate has finished, the data in
  //    ClemensBackendState should be considered invalid/undefined.
  using PublishStateDelegate = std::function<void(const ClemensBackendState &)>;
  ClemensBackend(std::string romPathname, const Config &config,
                 PublishStateDelegate publishDelegate);
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
  //  Clears step mode and enter run mode
  void run();
  //  Will issue the publish delegate on the next machine iteration
  void publish();
  //  Send host input to the emulator
  void inputEvent(const ClemensInputEvent &inputEvent);
  //  Insert disk
  void insertDisk(ClemensDriveType driveType, std::string diskPath);
  //  Eject disk
  void ejectDisk(ClemensDriveType driveType);
  //  Break
  void breakExecution();
  //  Add a breakpoint
  void addBreakpoint(const ClemensBackendBreakpoint &breakpoint);
  //  Remove a breakpoint
  void removeBreakpoint(unsigned index);
  //  Sets the write protect status on a disk in a drive
  void writeProtectDisk(ClemensDriveType driveType, bool wp);

private:
  using Command = ClemensBackendCommand;

  void queue(const Command &cmd);
  void queueToFront(const Command &cmd);

  void main(PublishStateDelegate publishDelegate);
  void resetMachine();
  bool insertDisk(const std::string_view &inputParam);
  void ejectDisk(const std::string_view &inputParam);
  bool writeProtectDisk(const std::string_view &inputParam);
  void inputMachine(const std::string_view &inputParam);
  bool addBreakpoint(const std::string_view &inputParam);
  bool delBreakpoint(const std::string_view &inputParam);
  std::optional<unsigned> checkHitBreakpoint();

  void initEmulatedDiskLocalStorage();
  bool loadDisk(ClemensDriveType driveType);
  bool saveDisk(ClemensDriveType driveType);
  cinek::ByteBuffer loadROM(const char *romPathname);

  //  TODO: These methods could be moved into a subclass as they are specific
  //        to machine type
  void initApple2GS();
  void loadBRAM();
  void saveBRAM();

  template <typename... Args> void localLog(int log_level, const char *msg, Args... args);

  static void emulatorLog(int log_level, ClemensMachine *machine, const char *msg);
  static void emulatorOpcodeCallback(struct ClemensInstruction *inst, const char *operand,
                                     void *this_ptr);

private:
  Config config_;

  std::thread runner_;
  std::deque<Command> commandQueue_;
  std::mutex commandQueueMutex_;
  std::condition_variable commandQueueCondition_;

  //  memory allocated once for the machine
  cinek::FixedStack slabMemory_;
  //  the actual machine object
  cinek::ByteBuffer romBuffer_;
  cinek::ByteBuffer diskBuffer_;
  ClemensMachine machine_;

  std::vector<ClemensBackendOutputText> logOutput_;
  std::vector<ClemensBackendBreakpoint> breakpoints_;
  std::array<ClemensWOZDisk, kClemensDrive_Count> diskContainers_;
  std::array<ClemensNibbleDisk, kClemensDrive_Count> disks_;
  std::array<ClemensBackendDiskDriveState, kClemensDrive_Count> diskDrives_;
};

#endif
