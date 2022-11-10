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

class ClemensProgramTrace;

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
    //  Steps the emulator
    void step(unsigned count);
    //  Will issue the publish delegate on the next machine iteration
    void publish();
    //  Send host input to the emulator
    void inputEvent(const ClemensInputEvent &inputEvent);
    //  Insert disk
    void insertDisk(ClemensDriveType driveType, std::string diskPath);
    //  Insert blank disk
    void insertBlankDisk(ClemensDriveType driveType, std::string diskPath);
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
    //  Sets the active debug memory page that can be read from or written to by
    //  the front end (this value is communicated on publish)
    void debugMemoryPage(uint8_t pageIndex);
    //  Write a single byte to machine memory at the current debugMemoryPage
    void debugMemoryWrite(uint16_t addr, uint8_t value);
    //  Set logging level
    void debugLogLevel(int logLevel);
    //  Send a message to the publish delegate from the frontend
    void debugMessage(std::string msg);
    //  Enable a program trace
    void debugProgramTrace(std::string op, std::string path);
    //  Save and load the machine
    void saveMachine(std::string path);
    void loadMachine(std::string path);

  private:
    using Command = ClemensBackendCommand;

    void queue(const Command &cmd);
    void queueToFront(const Command &cmd);

    void main(PublishStateDelegate publishDelegate);
    void resetMachine();
    unsigned stepMachine(const std::string_view &inputParam);
    bool insertDisk(const std::string_view &inputParam, bool allowBlank);
    void ejectDisk(const std::string_view &inputParam);
    bool writeProtectDisk(const std::string_view &inputParam);
    void writeMemory(const std::string_view &inputParam);
    void inputMachine(const std::string_view &inputParam);
    bool addBreakpoint(const std::string_view &inputParam);
    bool delBreakpoint(const std::string_view &inputParam);
    bool programTrace(const std::string_view &inputParam);
    bool saveSnapshot(const std::string_view &inputParam);
    bool loadSnapshot(const std::string_view &inputParam);

    std::optional<unsigned> checkHitBreakpoint();

    void initEmulatedDiskLocalStorage();
    bool loadDisk(ClemensDriveType driveType, bool allowBlank);
    bool saveDisk(ClemensDriveType driveType);
    void resetDisk(ClemensDriveType driveType);
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
    static uint8_t *unserializeAllocate(unsigned sz, void *context);

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
    std::vector<ClemensBackendExecutedInstruction> loggedInstructions_;
    std::array<ClemensWOZDisk, kClemensDrive_Count> diskContainers_;
    std::array<ClemensNibbleDisk, kClemensDrive_Count> disks_;
    std::array<ClemensBackendDiskDriveState, kClemensDrive_Count> diskDrives_;

    std::unique_ptr<ClemensProgramTrace> programTrace_;

    int logLevel_;
    uint8_t debugMemoryPage_;
    bool areInstructionsLogged_;
};

#endif
