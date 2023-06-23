#ifndef CLEM_HOST_APPLE2GS_HPP
#define CLEM_HOST_APPLE2GS_HPP

#include "clem_smartport.h"
#include "core/clem_apple2gs_config.hpp"

#include "clem_disk.h"
#include "clem_storage_unit.hpp"
#include "clem_types.h"

#include "cinek/buffer.hpp"

#include <functional>

//  forward declarations
typedef struct mpack_reader_t mpack_reader_t;
typedef struct mpack_writer_t mpack_writer_t;

class ClemensSystemListener;

class ClemensAppleIIGS {

  public:
    using Config = ClemensAppleIIGSConfig;
    using Frame = ClemensAppleIIGSFrame;

    enum class Status {
        Offline,
        Failed,
        UnsupportedSnapshotVersion,
        CorruptedSnapshot,
        Initialized,
        Loaded,
        Ready,
        Online,
        Stopped
    };

    enum class ResultFlags { None = 0, VerticalBlank = 1 << 0, Resetting = 1 << 1 };

    using OnWriteConfigCallback = std::function<void(const Config &)>;
    using OnLocalLogCallback = std::function<void(int, const char *)>;

    //  Initialize a new machine
    ClemensAppleIIGS(const std::string &romPath, const Config &config,
                     ClemensSystemListener &listener);
    //  Initialize a machine from the input stream
    ClemensAppleIIGS(mpack_reader_t *reader, ClemensSystemListener &listener);
    //  Destructor (saves state)
    ~ClemensAppleIIGS();

    //  If construction was successful, returns true.  This should be checked
    //  after creating the object.
    bool isOk() const {
        return status_ == Status::Initialized || status_ == Status::Loaded || isMounted();
    }
    bool isMounted() const {
        return status_ == Status::Ready || status_ == Status::Online || status_ == Status::Stopped;
    }

    //  This makes the machine instance the active machine
    //  which is important to ensure one machine has access to disks and
    //  any system resources
    void mount();
    //  Forces eject and save of all disks
    void unmount();
    //  Get details of any failure or more detailed status
    Status getStatus() const;

    //  Save the current state into the output stream
    std::pair<std::string, bool> save(mpack_writer_t *writer);

    //  Instigates a machine reset
    void reset();
    //  Updates the RTC wall clock (do not run every frame)
    void setLocalEpochTime(int localEpochTime);
    //  Queues an input event from the host
    void input(const ClemensInputEvent &input);
    //  Executes a single emulation step
    ResultFlags stepMachine();
    //  Render current audio frame.  This will not advance the audio frame buffer
    //  which is done by finishFrame().  This should be done after rendering a sufficient number 
    //  of 'frames' - which may be VBL frames, or whatever the application decides
    //  should be an audio 'frame'
    ClemensAudio renderAudio();
    //  Retrieves frame information for display/audio/disks
    Frame &getFrame(Frame &frame);
    //  Finishes the frame
    void finishFrame(Frame &frame);
    //  Forces the trigger for saving the config
    void saveConfig();
    //  Enables opcode logging
    void enableOpcodeLogging(bool enable);
    //  Sends a UTF8 character from the input stream
    unsigned consume_utf8_input(const char* in, const char* inEnd);

    //  Direct access to emulator state
    ClemensStorageUnit &getStorage() { return storage_; }
    ClemensMachine &getMachine() { return machine_; }
    ClemensMMIO &getMMIO() { return mmio_; }

  private:
    static void loggerHook(int logLevel, ClemensMachine *machine, const char *msg);
    static uint8_t *unserializerAllocateHook(unsigned type, unsigned sz, void *context);
    static void emulatorOpcodeCallback(struct ClemensInstruction *inst, const char *operand,
                                       void *this_ptr);

    template <typename... Args> void localLog(int log_level, const char *msg, Args... args);

  private:
    ClemensSystemListener &listener_;
    Status status_;

    cinek::FixedStack slab_;

    // Machine state
    ClemensMachine machine_;
    ClemensMMIO mmio_;
    ClemensStorageUnit storage_;
    ClemensCard *mockingboard_;

    // Persisted configuration attributes
    unsigned configMemory_;
    unsigned configAudioSamplesPerSecond_;
    std::array<std::string, CLEM_CARD_SLOT_COUNT> cardNames_;
    std::array<std::string, kClemensDrive_Count> diskNames_;
    std::array<std::string, CLEM_SMARTPORT_DRIVE_LIMIT> smartDiskNames_;
};

class ClemensSystemListener {
  public:
    virtual ~ClemensSystemListener() = default;

    virtual void onClemensSystemMachineLog(int logLevel, const ClemensMachine *machine,
                                           const char *msg) = 0;
    virtual void onClemensSystemLocalLog(int logLevel, const char *msg) = 0;
    virtual void onClemensSystemWriteConfig(const ClemensAppleIIGS::Config &config) = 0;
    virtual void onClemensInstruction(struct ClemensInstruction *inst, const char *operand) = 0;
};

inline ClemensAppleIIGS::ResultFlags operator|(ClemensAppleIIGS::ResultFlags l,
                                               ClemensAppleIIGS::ResultFlags r) {
    return static_cast<ClemensAppleIIGS::ResultFlags>(
        static_cast<std::underlying_type<ClemensAppleIIGS::ResultFlags>::type>(l) |
        static_cast<std::underlying_type<ClemensAppleIIGS::ResultFlags>::type>(r));
}

inline ClemensAppleIIGS::ResultFlags operator&(ClemensAppleIIGS::ResultFlags l,
                                               ClemensAppleIIGS::ResultFlags r) {
    return static_cast<ClemensAppleIIGS::ResultFlags>(
        static_cast<std::underlying_type<ClemensAppleIIGS::ResultFlags>::type>(l) &
        static_cast<std::underlying_type<ClemensAppleIIGS::ResultFlags>::type>(r));
}

inline bool test(ClemensAppleIIGS::ResultFlags l, ClemensAppleIIGS::ResultFlags r) {
    return ((l & r) == r);
}

#endif
