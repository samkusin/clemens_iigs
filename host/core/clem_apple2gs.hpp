#ifndef CLEM_HOST_APPLE2GS_HPP
#define CLEM_HOST_APPLE2GS_HPP

#include "clem_defs.h"
#include "clem_disk.h"
#include "clem_mmio_types.h"
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
    struct Config {
        unsigned memory;
        unsigned audioSamplesPerSecond;
        std::string romPath;
        uint8_t bram[CLEM_RTC_BRAM_SIZE];
        std::array<std::string, kClemensDrive_Count> diskImagePaths;
        std::array<std::string, CLEM_SMARTPORT_DRIVE_LIMIT> smartPortImagePaths;
        std::array<std::string, CLEM_CARD_SLOT_COUNT> cardNames;
    };

    enum class Status {
        Offline,
        Failed,
        UnsupportedSnapshotVersion,
        CorruptedSnapshot,
        Initialized,
        Online,
        Stopped
    };

    enum class ResultFlags { None = 0, VerticalBlank = 1 << 0, Resetting = 1 << 1 };

    struct Frame {
        ClemensMonitor monitor;
        ClemensVideo graphics;
        ClemensVideo text;
        ClemensAudio audio;
        std::array<ClemensDiskDriveStatus, kClemensDrive_Count> diskDriveStatuses;
        std::array<ClemensDiskDriveStatus, CLEM_SMARTPORT_DRIVE_LIMIT> smartPortStatuses;
        uint8_t e0bank[CLEM_IIGS_BANK_SIZE];
        uint8_t e1bank[CLEM_IIGS_BANK_SIZE];
        uint16_t rgb[3200];
    };

    static constexpr const char *kClemensCardMockingboardName = "mockingboard_c";

    using OnWriteConfigCallback = std::function<void(const Config &)>;
    using OnLocalLogCallback = std::function<void(int, const char *)>;

    //  Initialize a new machine
    ClemensAppleIIGS(const Config &config, ClemensSystemListener &listener);
    //  Initialize a machine from the input stream
    ClemensAppleIIGS(mpack_reader_t *reader, ClemensSystemListener &listener);
    //  Destructor (saves state)
    ~ClemensAppleIIGS();

    //  If construction was successful, returns true.  This should be checked
    //  after creating the object.
    bool isOk() const {
        return status_ == Status::Initialized || status_ == Status::Online ||
               status_ == Status::Stopped;
    }

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
    //  Retrieves
    Frame &getFrame(Frame &frame);

    //  Direct access to emulator state
    ClemensStorageUnit &getStorage() { return storage_; }
    ClemensMachine &getMachine() { return machine_; }
    ClemensMMIO &getMMIO() { return mmio_; }

  private:
    static void loggerHook(int logLevel, ClemensMachine *machine, const char *msg);
    static uint8_t *unserializerAllocateHook(unsigned type, unsigned sz, void *context);

    void saveConfig();

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
    std::string configROMPath_;
    std::array<std::string, CLEM_CARD_SLOT_COUNT> cardNames_;
};

class ClemensSystemListener {
  public:
    virtual ~ClemensSystemListener() = default;

    virtual void onClemensSystemMachineLog(int logLevel, const ClemensMachine *machine,
                                           const char *msg) = 0;
    virtual void onClemensSystemLocalLog(int logLevel, const char *msg) = 0;
    virtual void onClemensSystemWriteConfig(const ClemensAppleIIGS::Config &config) = 0;
};

#endif
