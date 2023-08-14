#ifndef CLEM_HOST_COMMAND_QUEUE_HPP
#define CLEM_HOST_COMMAND_QUEUE_HPP

#include "clem_host_shared.hpp"

#include "cinek/circular_buffer.hpp"

#include <memory>
#include <optional>
#include <string>

class ClemensCommandData {
  public:
    enum class Type { MinizPNG };

    virtual ~ClemensCommandData(){};
    virtual Type getType() const = 0;
};

class ClemensCommandMinizPNG : public ClemensCommandData {
  public:
    ClemensCommandMinizPNG(void *data, size_t sz, int width, int height);
    virtual ~ClemensCommandMinizPNG() override;

    virtual ClemensCommandData::Type getType() const override { return ClemensCommandData::Type::MinizPNG; }
    std::pair<void *, size_t> getData() const { return std::make_pair(data_, dataSize_); }
    int getWidth() const { return width_; }
    int getHeight() const { return height_; }

  private:
    void *data_;
    size_t dataSize_;
    int width_;
    int height_;
};

class ClemensCommandQueueListener {
  public:
    virtual ~ClemensCommandQueueListener() {}

    virtual void onCommandReset() = 0;
    virtual void onCommandRun() = 0;
    virtual void onCommandBreakExecution() = 0;
    virtual void onCommandStep(unsigned count) = 0;
    virtual void onCommandAddBreakpoint(const ClemensBackendBreakpoint &breakpoint) = 0;
    virtual bool onCommandRemoveBreakpoint(int index) = 0;
    virtual void onCommandInputEvent(const ClemensInputEvent &inputEvent) = 0;
    virtual bool onCommandInsertDisk(ClemensDriveType driveType, std::string diskPath) = 0;
    virtual void onCommandEjectDisk(ClemensDriveType driveType) = 0;
    virtual bool onCommandWriteProtectDisk(ClemensDriveType driveType, bool wp) = 0;
    virtual bool onCommandInsertSmartPortDisk(unsigned driveIndex, std::string diskPath) = 0;
    virtual void onCommandEjectSmartPortDisk(unsigned driveIndex) = 0;
    virtual void onCommandDebugMemoryPage(uint8_t pageIndex) = 0;
    virtual void onCommandDebugMemoryWrite(uint16_t addr, uint8_t value) = 0;
    virtual void onCommandDebugLogLevel(int logLevel) = 0;
    virtual bool onCommandDebugProgramTrace(std::string_view op, std::string_view path) = 0;
    virtual bool onCommandSaveMachine(std::string path,
                                      std::unique_ptr<ClemensCommandMinizPNG> pngData) = 0;
    virtual bool onCommandLoadMachine(std::string path) = 0;
    virtual bool onCommandRunScript(std::string command) = 0;
    virtual void onCommandFastDiskEmulation(bool enabled) = 0;
    virtual std::string onCommandDebugMessage(std::string msg) = 0;
    virtual void onCommandSendText(std::string text) = 0;
    virtual bool onCommandBinaryLoad(std::string pathname, unsigned address) = 0;
    virtual bool onCommandBinarySave(std::string pathname, unsigned address, unsigned length) = 0;
    virtual void onCommandFastMode(bool enabled) = 0;
};

class ClemensCommandQueue {
  public:
    using ResultBuffer = std::vector<ClemensBackendResult>;
    using DispatchResult = std::pair<ResultBuffer, bool>;

    void queue(ClemensCommandQueue &other);
    bool isEmpty() const { return queue_.isEmpty(); }
    size_t getSize() const { return queue_.size(); }

    //  execute all commands
    DispatchResult dispatchAll(ClemensCommandQueueListener &listener);

    //  Queues a command to the backend.   Most commands are processed on the next
    //  execution frame.  Certain commands may hold the queue (i.e. like wait())
    //  Priority commands like Cancel or Terminate are pushed to the front,
    //  overriding commands like Wait.
    void terminate();
    //  Issues a soft reset to the machine.   This is roughly equivalent to pressing
    //  the power button.
    void reset();
    //  Clears step mode and enter run mode
    void run();
    //  Steps the emulator
    void step(unsigned count);
    //  Send host input to the emulator
    void inputEvent(const ClemensInputEvent &inputEvent);
    //  Insert disk
    void insertDisk(ClemensDriveType driveType, std::string diskPath);
    //  Eject disk
    void ejectDisk(ClemensDriveType driveType);
    //  Sets the write protect status on a disk in a drive
    void writeProtectDisk(ClemensDriveType driveType, bool wp);
    //  Insert SmartPort device
    void insertSmartPortDisk(unsigned driveIndex, std::string diskPath);
    //  Eject SmartPort device
    void ejectSmartPortDisk(unsigned driveIndex);
    //  Break
    void breakExecution();
    //  Add a breakpoint
    void addBreakpoint(const ClemensBackendBreakpoint &breakpoint);
    //  Remove a breakpoint
    void removeBreakpoint(unsigned index);
    //  Sets the active debug memory page that can be read from or written to by
    //  the front end (this value is communicated on publish)
    void debugMemoryPage(uint8_t pageIndex);
    //  Write a single byte to machine memory at the current debugMemoryPage
    void debugMemoryWrite(uint16_t addr, uint8_t value);
    //  Set logging level
    void debugLogLevel(int logLevel);
    //  Enable a program trace
    void debugProgramTrace(std::string op, std::string path);
    //  Save and load the machine
    void saveMachine(std::string path, std::unique_ptr<ClemensCommandMinizPNG> image);
    void loadMachine(std::string path);
    //  Runs a script command for debugging
    void runScript(std::string command);
    //  Enables fast disk emulation
    void enableFastDiskEmulation(bool enable);
    //  TODO: remove in place of interpreter commands
    void debugMessage(std::string msg);
    //  Sends text to the emulator's keyboard queue
    void sendText(std::string text);
    //  Save binary to disk
    void bsave(std::string pathname, unsigned address, unsigned length);
    //  Load binary from disk
    void bload(std::string pathname, unsigned address);
    //  Toggle fast mode
    void fastMode(bool enable);

  private:
    bool insertDisk(ClemensCommandQueueListener &listener, const std::string_view &inputParam);
    void ejectDisk(ClemensCommandQueueListener &listener, const std::string_view &inputParam);
    bool insertSmartPortDisk(ClemensCommandQueueListener &listener,
                             const std::string_view &inputParam);
    void ejectSmartPortDisk(ClemensCommandQueueListener &listener,
                            const std::string_view &inputParam);
    bool writeProtectDisk(ClemensCommandQueueListener &listener,
                          const std::string_view &inputParam);
    void writeMemory(ClemensCommandQueueListener &listener, const std::string_view &inputParam);
    void inputMachine(ClemensCommandQueueListener &listener, const std::string_view &inputParam);
    bool addBreakpoint(ClemensCommandQueueListener &listener, const std::string_view &inputParam);
    bool delBreakpoint(ClemensCommandQueueListener &listener, const std::string_view &inputParam);
    bool programTrace(ClemensCommandQueueListener &listener, const std::string_view &inputParam);
    bool runScriptCommand(ClemensCommandQueueListener &listener, const std::string_view &command);
    bool saveBinary(ClemensCommandQueueListener &listener, const std::string_view &command);
    bool loadBinary(ClemensCommandQueueListener &listener, const std::string_view &command);

    using Command = ClemensBackendCommand;
    using Data = std::unique_ptr<ClemensCommandData>;
    cinek::CircularBuffer<Command, 16> queue_;
    cinek::CircularBuffer<Data, 16> dataQueue_;

    void queue(const Command &cmd, Data data = Data());
};

#endif
