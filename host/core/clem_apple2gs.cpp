#include "clem_apple2gs.hpp"

#include "clem_mem.h"
#include "clem_prodos_disk.hpp"
#include "clem_storage_unit.hpp"

#include "clem_defs.h"
#include "clem_disk.h"
#include "clem_mmio_types.h"
#include "core/clem_apple2gs_config.hpp"
#include "emulator.h"
#include "emulator_mmio.h"
#include "serializer.h"

#include "devices/hddcard.h"
#include "devices/mockingboard.h"

#include "external/mpack.h"
#include "fmt/core.h"
#include "fmt/format.h"
#include "spdlog/logger.h"
#include "spdlog/spdlog.h"

#include <cassert>
#include <cstdlib>
#include <fstream>

namespace {

//  This is the version of the Snapshot object serialized (the machine, mmio, etc
//  objects have their own versions managed in their serializer.)
//
constexpr unsigned kSnapshotVersion = 1;
constexpr unsigned kMachineSlabMaximumSize = 32 * 1024 * 1024;

unsigned calculateSlabMemoryRequirements(const ClemensAppleIIGS::Config &config) {
    unsigned bytesRequired = 0;

    bytesRequired += 2 * CLEM_DISK_525_MAX_DATA_SIZE;
    bytesRequired += 2 * CLEM_DISK_35_MAX_DATA_SIZE;

    bytesRequired += config.memory * 1024;     // FPI Memory
    bytesRequired += 2 * CLEM_IIGS_BANK_SIZE;  // Mega 2 Memory
    bytesRequired += 16 * CLEM_IIGS_BANK_SIZE; // ROM Memory limit
    bytesRequired += 2048 * CLEM_CARD_SLOT_COUNT;
    bytesRequired += 1 * 1024 * 1024; // Some extra buffer?

    assert(bytesRequired <= kMachineSlabMaximumSize);

    return bytesRequired;
}

ClemensCard *createCard(const char *name) {
    ClemensCard *card = NULL;
    if (!strcmp(name, kClemensCardMockingboardName)) {
        card = new ClemensCard;
        memset(card, 0, sizeof(*card));
        clem_card_mockingboard_initialize(card);
    } else if (!strcmp(name, kClemensCardHardDiskName)) {
        card = new ClemensCard;
        memset(card, 0, sizeof(*card));
        clem_card_hdd_initialize(card);
    }
    return card;
}

void destroyCard(ClemensCard *card) {
    //  use card->io_name() to identify
    if (!card)
        return;
    const char *name = card->io_name(card->context);
    if (!strcmp(name, kClemensCardMockingboardName)) {
        clem_card_mockingboard_uninitialize(card);
    } else if (!strcmp(name, kClemensCardHardDiskName)) {
        clem_card_hdd_uninitialize(card);
    }
    delete card;
}

} // namespace

std::array<const char*, kClemensCardLimitPerSlot> getCardNamesForSlot(unsigned slotIndex) {
    std::array<const char*, kClemensCardLimitPerSlot> cards;
    cards.fill(nullptr);

    ++slotIndex;
    switch (slotIndex) {
        case 4:
            cards[0] = kClemensCardMockingboardName;
            break;
        case 7:
            cards[0] = kClemensCardHardDiskName;
            break;
    }
    return cards;
}

ClemensAppleIIGS::ClemensAppleIIGS(const std::string &romPath, const Config &config,
                                   ClemensSystemListener &listener)
    : listener_(listener), status_(Status::Offline),
      slab_(calculateSlabMemoryRequirements(config),
            malloc(calculateSlabMemoryRequirements(config))),
      machine_{}, mmio_{}, storage_(), mockingboard_(nullptr), hddcard_(nullptr) {

    // Ensure a valid ROM buffer regardless of whether a valid ROM was loaded
    // In the error case, we'll want to have a placeholder ROM.
    cinek::ByteBuffer romBuffer;
    if (!romPath.empty()) {
        std::ifstream romFileStream(romPath, std::ios::binary | std::ios::ate);
        unsigned romMemorySize = 0;
        if (romFileStream.is_open()) {
            romMemorySize = unsigned(romFileStream.tellg());
            romBuffer = cinek::ByteBuffer((uint8_t *)slab_.allocate(romMemorySize), romMemorySize);
            romFileStream.seekg(0, std::ios::beg);
            romFileStream.read((char *)romBuffer.forwardSize(romMemorySize).first, romMemorySize);
            romFileStream.close();
        }
    }
    if (romBuffer.isEmpty()) {
        //  TODO: load a dummy ROM, for a truly placeholder infinite loop ROM
        romBuffer =
            cinek::ByteBuffer((uint8_t *)slab_.allocate(CLEM_IIGS_BANK_SIZE), CLEM_IIGS_BANK_SIZE);
        memset(romBuffer.forwardSize(CLEM_IIGS_BANK_SIZE).first, 0, CLEM_IIGS_BANK_SIZE);
        auto *rom = romBuffer.getHead();
        rom[CLEM_6502_RESET_VECTOR_LO_ADDR] = 0x62;
        rom[CLEM_6502_RESET_VECTOR_HI_ADDR] = 0xfa;
        //  BRA -2 (infinite loop)
        rom[0xfa62] = 0x80;
        rom[0xfa63] = 0xFE;
    }

    //  Initialize the Machine
    const unsigned kFPIROMBankCount =
        (romBuffer.getSize() + CLEM_IIGS_BANK_SIZE - 1) / CLEM_IIGS_BANK_SIZE;
    const unsigned kFPIRAMBankCount =
        ((config.memory * 1024) + CLEM_IIGS_BANK_SIZE - 1) / CLEM_IIGS_BANK_SIZE;
    const uint32_t kClocksPerFastCycle = CLEM_CLOCKS_PHI2_FAST_CYCLE;
    const uint32_t kClocksPerSlowCycle = CLEM_CLOCKS_PHI0_CYCLE;

    int initResult = clemens_init(
        &machine_, kClocksPerSlowCycle, kClocksPerFastCycle, romBuffer.getHead(), kFPIROMBankCount,
        slab_.allocate(CLEM_IIGS_BANK_SIZE), slab_.allocate(CLEM_IIGS_BANK_SIZE),
        slab_.allocate(kFPIRAMBankCount * CLEM_IIGS_BANK_SIZE), kFPIRAMBankCount);
    if (initResult != 0) {
        fmt::print(stderr, "Clemens library failed to initialize with err code ({})\n", initResult);
        status_ = Status::Failed;
        return;
    }
    clem_mmio_init(&mmio_, &machine_.dev_debug, machine_.mem.bank_page_map,
                   slab_.allocate(2048 * CLEM_CARD_SLOT_COUNT), kFPIRAMBankCount, kFPIROMBankCount,
                   machine_.mem.mega2_bank_map[0], machine_.mem.mega2_bank_map[1], &machine_.tspec);

    ClemensAudioMixBuffer audioMixBuffer;
    audioMixBuffer.frames_per_second = config.audioSamplesPerSecond;
    audioMixBuffer.stride = 2 * sizeof(float);
    audioMixBuffer.frame_count = audioMixBuffer.frames_per_second / 4;
    audioMixBuffer.data =
        (uint8_t *)(slab_.allocate(audioMixBuffer.frame_count * audioMixBuffer.stride));
    clemens_assign_audio_mix_buffer(&mmio_, &audioMixBuffer);

    //  TODO: clemens API clemens_mmio_card_insert()
    for (size_t cardIdx = 0; cardIdx < config.cardNames.size(); ++cardIdx) {
        auto &cardName = config.cardNames[cardIdx];
        if (!cardName.empty()) {
            mmio_.card_slot[cardIdx] = createCard(cardName.c_str());
            if (cardName == kClemensCardMockingboardName) {
                mockingboard_ = mmio_.card_slot[cardIdx];
            } else if (cardName == kClemensCardHardDiskName) {
                hddcard_ = mmio_.card_slot[cardIdx];
            }
        } else {
            mmio_.card_slot[cardIdx] = NULL;
        }
    }
    //  Update bram and save off configuration (Extended BRAM)
    memcpy(mmio_.dev_rtc.bram, config.bram.data(), CLEM_RTC_BRAM_SIZE);
    clemens_rtc_set_bram_dirty(&mmio_);

    //  And insert the disks!
    uint8_t *bits_data = slab_.allocateArray<uint8_t>(CLEM_DISK_525_MAX_DATA_SIZE);
    clemens_assign_disk_buffer(&mmio_, kClemensDrive_5_25_D1, bits_data,
                               bits_data + CLEM_DISK_525_MAX_DATA_SIZE);
    bits_data = slab_.allocateArray<uint8_t>(CLEM_DISK_525_MAX_DATA_SIZE);
    clemens_assign_disk_buffer(&mmio_, kClemensDrive_5_25_D2, bits_data,
                               bits_data + CLEM_DISK_525_MAX_DATA_SIZE);
    bits_data = slab_.allocateArray<uint8_t>(CLEM_DISK_35_MAX_DATA_SIZE);
    clemens_assign_disk_buffer(&mmio_, kClemensDrive_3_5_D1, bits_data,
                               bits_data + CLEM_DISK_35_MAX_DATA_SIZE);
    bits_data = slab_.allocateArray<uint8_t>(CLEM_DISK_35_MAX_DATA_SIZE);
    clemens_assign_disk_buffer(&mmio_, kClemensDrive_3_5_D2, bits_data,
                               bits_data + CLEM_DISK_35_MAX_DATA_SIZE);

    status_ = Status::Initialized;

    //  Finally save out the final config
    configMemory_ = (kFPIRAMBankCount * CLEM_IIGS_BANK_SIZE) / 1024;
    configAudioSamplesPerSecond_ = config.audioSamplesPerSecond;
    cardNames_ = config.cardNames;
    diskNames_ = config.diskImagePaths;
    smartDiskNames_ = config.smartPortImagePaths;
}

ClemensAppleIIGS::ClemensAppleIIGS(mpack_reader_t *reader, ClemensSystemListener &listener)
    : listener_(listener), status_(Status::Offline), machine_{}, mmio_{}, storage_(),
      mockingboard_(nullptr), hddcard_(nullptr) {

    std::string componentName = "root";
    char buf[1024];
    bool success = false;
    unsigned slabSize = 0;
    unsigned elementCount = 0;
    unsigned cardCount = 0;
    unsigned index;
    ClemensUnserializerContext unserializerContext;

    unserializerContext.allocCb = unserializerAllocateHook;
    unserializerContext.allocUserPtr = this;

    mpack_expect_map(reader);
    //  read the memory requirements first and create the slab
    mpack_expect_cstr_match(reader, "version");
    if (mpack_expect_uint(reader) > kSnapshotVersion) {
        status_ = Status::UnsupportedSnapshotVersion;
        goto load_done;
    }
    mpack_expect_cstr_match(reader, "config.memory");
    configMemory_ = mpack_expect_uint(reader);
    mpack_expect_cstr_match(reader, "config.audio.samples");
    configAudioSamplesPerSecond_ = mpack_expect_uint(reader);

    mpack_expect_cstr_match(reader, "slab");
    slabSize = mpack_expect_uint_max(reader, kMachineSlabMaximumSize);
    if (mpack_reader_error(reader) != mpack_ok)
        goto load_done;

    slab_ = cinek::FixedStack(slabSize, malloc(slabSize));

    componentName = "machine";
    mpack_expect_cstr_match(reader, componentName.c_str());
    clemens_unserialize_machine(reader, &machine_, unserializerAllocateHook, this);
    if (mpack_reader_error(reader) != mpack_ok)
        goto load_done;

    componentName = "mmio";
    mpack_expect_cstr_match(reader, componentName.c_str());
    clemens_unserialize_mmio(reader, &mmio_, &machine_, unserializerAllocateHook, this);
    if (mpack_reader_error(reader) != mpack_ok)
        goto load_done;

    componentName = "cards";
    mpack_expect_cstr_match(reader, componentName.c_str());
    cardCount = mpack_expect_array_max(reader, CLEM_CARD_SLOT_COUNT);
    for (index = 0; index < cardCount; index++) {
        mmio_.card_slot[index] = NULL;
        if (mpack_expect_map_max_or_nil(reader, 16, &elementCount)) {
            mpack_expect_cstr_match(reader, "name");
            mpack_expect_cstr(reader, buf, sizeof(buf));
            cardNames_[index] = buf;
            mmio_.card_slot[index] = createCard(cardNames_[index].c_str());
            mpack_expect_cstr_match(reader, "card");
            if (mpack_peek_tag(reader).type == mpack_type_nil) {
                mpack_expect_nil(reader);
            } else if (cardNames_[index] == kClemensCardMockingboardName) {
                clem_card_mockingboard_unserialize(reader, mmio_.card_slot[index],
                                                   unserializerAllocateHook, this);
                mockingboard_ = mmio_.card_slot[index];
            } else if (cardNames_[index] == kClemensCardHardDiskName) {
                clem_card_hdd_unserialize(reader, mmio_.card_slot[index], unserializerAllocateHook,
                                          this);
                hddcard_ = mmio_.card_slot[index];
            } else {
                localLog(CLEM_DEBUG_LOG_WARN, "ClemensAppleIIGS(): invalid card entry {}",
                         cardNames_[index]);
                mpack_reader_flag_error(reader, mpack_error_data);
            }
            mpack_done_map(reader);
        }
    }
    mpack_done_array(reader);

    componentName = "storage";
    mpack_expect_cstr_match(reader, componentName.c_str());
    if (!storage_.unserialize(mmio_, reader, unserializerContext))
        goto load_done;

    for (unsigned driveIndex = 0; driveIndex < (unsigned)diskNames_.size(); ++driveIndex) {
        auto driveType = static_cast<ClemensDriveType>(driveIndex);
        diskNames_[driveIndex] = storage_.getDriveStatus(driveType).assetPath;
    }
    for (unsigned driveIndex = 0; driveIndex < (unsigned)smartDiskNames_.size(); ++driveIndex) {
        smartDiskNames_[driveIndex] = storage_.getSmartPortStatus(driveIndex).assetPath;
    }
    if (mpack_reader_error(reader) != mpack_ok)
        goto load_done;

    success = true;
load_done:
    if (status_ == Status::Offline) {
        status_ = success ? Status::Loaded : Status::Failed;
    }
    if (status_ != Status::Loaded) {
        localLog(CLEM_DEBUG_LOG_WARN, "ClemensAppleIIGS(): Bad load in component '{}'",
                 componentName);
    }
    mpack_done_map(reader);
}

ClemensAppleIIGS::~ClemensAppleIIGS() {
    unmount();
    for (int i = 0; i < CLEM_CARD_SLOT_COUNT; ++i) {
        destroyCard(mmio_.card_slot[i]);
        mmio_.card_slot[i] = NULL;
    }
    free(slab_.getHead());
}

void ClemensAppleIIGS::mount() {
    if (status_ == Status::Initialized) {
        spdlog::info("ClemensAppleIIGS(): mounting new machine");
        clemens_host_setup(&machine_, loggerHook, this);
        for (unsigned driveIndex = 0; driveIndex < (unsigned)diskNames_.size(); ++driveIndex) {
            auto driveType = static_cast<ClemensDriveType>(driveIndex);
            if (diskNames_[driveIndex].empty())
                continue;
            storage_.insertDisk(mmio_, driveType, diskNames_[driveIndex]);
        }
        for (unsigned driveIndex = 0; driveIndex < (unsigned)smartDiskNames_.size(); ++driveIndex) {
            if (smartDiskNames_[driveIndex].empty())
                continue;
            storage_.assignSmartPortDisk(mmio_, driveIndex, smartDiskNames_[driveIndex]);
        }
    } else if (status_ == Status::Loaded) {
        spdlog::info("ClemensAppleIIGS(): mounting loaded snapshot");
        clemens_host_setup(&machine_, loggerHook, this);
        storage_.saveAllDisks(mmio_);
    } else {
        spdlog::error("ClemensAppleIIGS(): cannot mount as machine is already active.");
        return;
    }

    //  any follow up initialization goes here
    saveConfig();
    status_ = Status::Ready;
}

void ClemensAppleIIGS::unmount() {
    if (!isMounted())
        return;
    storage_.ejectAllDisks(mmio_);
    //  detach logger and hackily clear the debug context here
    //  since two machines cannot be mounted at once.
    spdlog::info("ClemensAppleIIGS(): unmounting machine");
    clemens_host_setup(&machine_, NULL, NULL);
    clemens_debug_context(NULL);
    status_ = Status::Initialized;
}

void ClemensAppleIIGS::loggerHook(int logLevel, ClemensMachine *machine, const char *msg) {
    auto *self = reinterpret_cast<ClemensAppleIIGS *>(machine->debug_user_ptr);
    self->listener_.onClemensSystemMachineLog(logLevel, machine, msg);
}

uint8_t *ClemensAppleIIGS::unserializerAllocateHook(unsigned type, unsigned sz, void *context) {
    auto *self = reinterpret_cast<ClemensAppleIIGS *>(context);
    unsigned bytesSize;
    switch (type) {
    case CLEM_EMULATOR_ALLOCATION_FPI_MEMORY_BANK:
        bytesSize = sz * CLEM_IIGS_BANK_SIZE;
        SPDLOG_DEBUG("ClemensAppleIIGS() - FPI Bank was allocated {} bytes", bytesSize);
        break;
    case CLEM_EMULATOR_ALLOCATION_MEGA2_MEMORY_BANK:
        bytesSize = sz * CLEM_IIGS_BANK_SIZE;
        SPDLOG_DEBUG("ClemensAppleIIGS() - Mega II Bank was allocated {} bytes", bytesSize);
        break;
    case CLEM_EMULATOR_ALLOCATION_DISK_NIB_3_5:
        bytesSize = sz;
        SPDLOG_DEBUG("ClemensAppleIIGS() - Disk 3.5 buffer was allocated {} bytes", bytesSize);
        break;
    case CLEM_EMULATOR_ALLOCATION_DISK_NIB_5_25:
        bytesSize = sz;
        SPDLOG_DEBUG("ClemensAppleIIGS() - Disk 5.25 buffer was allocated {} bytes", bytesSize);
        break;
    case CLEM_EMULATOR_ALLOCATION_CARD_BUFFER:
        bytesSize = sz * 2048;
        SPDLOG_DEBUG("ClemensAppleIIGS() - Card buffer was allocated {} bytes", bytesSize);
        break;
    default:
        bytesSize = sz;
        SPDLOG_DEBUG("ClemensAppleIIGS() - Generic buffer was allocated {} bytes", bytesSize);
        break;
    }

    return self->slab_.allocateArray<uint8_t>(bytesSize);
}

template <typename... Args>
void ClemensAppleIIGS::localLog(int logLevel, const char *msg, Args... args) {
    auto text = fmt::format(msg, args...);
    listener_.onClemensSystemLocalLog(logLevel, text.c_str());
}

auto ClemensAppleIIGS::getStatus() const -> Status { return status_; }

std::pair<std::string, bool> ClemensAppleIIGS::save(mpack_writer_t *writer) {
    std::string componentName = "root";
    unsigned slotIndex;
    bool result = false;

    mpack_start_map(writer, 8);

    if (getStatus() != Status::Online && getStatus() != Status::Ready)
        goto save_done;

    mpack_write_cstr(writer, "version");
    mpack_write_uint(writer, kSnapshotVersion);

    //  serialize config attributes
    mpack_write_cstr(writer, "config.memory");
    mpack_write_uint(writer, configMemory_);
    mpack_write_cstr(writer, "config.audio.samples");
    mpack_write_uint(writer, configAudioSamplesPerSecond_);

    //  card names are serialized in the "cards" section
    //  save slab requirements
    mpack_write_cstr(writer, "slab");
    mpack_write_uint(writer, slab_.capacity());

    //  serialize machine and mmio
    componentName = "machine";
    mpack_write_cstr(writer, componentName.c_str());
    clemens_serialize_machine(writer, &machine_);
    if (mpack_writer_error(writer) != mpack_ok)
        goto save_done;

    componentName = "mmio";
    mpack_write_cstr(writer, componentName.c_str());
    clemens_serialize_mmio(writer, &mmio_);
    if (mpack_writer_error(writer) != mpack_ok)
        goto save_done;

    //  serialize cards in slot order
    componentName = "cards";
    mpack_write_cstr(writer, componentName.c_str());
    mpack_start_array(writer, CLEM_CARD_SLOT_COUNT);
    for (slotIndex = 0; slotIndex < CLEM_CARD_SLOT_COUNT; slotIndex++) {
        if (mmio_.card_slot[slotIndex] != NULL) {
            mpack_start_map(writer, 2);
            mpack_write_cstr(writer, "name");
            mpack_write_cstr(writer, cardNames_[slotIndex].c_str());
            mpack_write_cstr(writer, "card");
            if (cardNames_[slotIndex] == kClemensCardMockingboardName) {
                clem_card_mockingboard_serialize(writer, mmio_.card_slot[slotIndex]);
            } else if (cardNames_[slotIndex] == kClemensCardHardDiskName) {
                clem_card_hdd_serialize(writer, mmio_.card_slot[slotIndex]);
            } else {
                mpack_write_nil(writer);
            }
            mpack_finish_map(writer);
        } else {
            mpack_write_nil(writer);
        }
    }
    mpack_finish_array(writer);
    if (mpack_writer_error(writer) != mpack_ok)
        goto save_done;

    //  serialize storage unit
    componentName = "storage";
    mpack_write_cstr(writer, componentName.c_str());
    if (!storage_.serialize(mmio_, writer))
        goto save_done;

    result = true;

    //  DONE
save_done:
    if (!result) {
        localLog(CLEM_DEBUG_LOG_WARN, "ClemensAppleIIGS::save(): Bad save in component '{}'",
                 componentName);
    }
    mpack_finish_map(writer);
    
    return std::make_pair(componentName, result);
}

void ClemensAppleIIGS::saveConfig() {
    Config finalConfig;

    finalConfig.memory = configMemory_;
    finalConfig.audioSamplesPerSecond = configAudioSamplesPerSecond_;

    memcpy(finalConfig.bram.data(), clemens_rtc_get_bram(&mmio_, NULL), CLEM_RTC_BRAM_SIZE);
    for (unsigned driveIndex = 0; driveIndex < (unsigned)finalConfig.diskImagePaths.size();
         ++driveIndex) {
        auto status = storage_.getDriveStatus(static_cast<ClemensDriveType>(driveIndex));
        if (status.isMounted()) {
            finalConfig.diskImagePaths[driveIndex] = status.assetPath;
        }
    }
    for (unsigned driveIndex = 0; driveIndex < (unsigned)finalConfig.smartPortImagePaths.size();
         ++driveIndex) {
        auto status = storage_.getSmartPortStatus(driveIndex);
        if (status.isMounted()) {
            finalConfig.smartPortImagePaths[driveIndex] = status.assetPath;
        }
    }
    for (unsigned cardIndex = 0; cardIndex < CLEM_CARD_SLOT_COUNT; ++cardIndex) {
        if (mmio_.card_slot[cardIndex] != NULL) {
            finalConfig.cardNames[cardIndex] = cardNames_[cardIndex];
        }
    }
    listener_.onClemensSystemWriteConfig(finalConfig);
}

void ClemensAppleIIGS::setLocalEpochTime(int localEpochTime) {
    time_t epochTime = time(NULL);
    constexpr time_t kEpoch1904To1970Seconds = 2082844800;
    auto epochTime1904 = epochTime + localEpochTime + kEpoch1904To1970Seconds;
    clemens_rtc_set(&mmio_, (unsigned)epochTime1904);
}

void ClemensAppleIIGS::reset() {
    machine_.cpu.pins.resbIn = false;
    machine_.resb_counter = 3;
}

//  Queues an input event from the host
void ClemensAppleIIGS::input(const ClemensInputEvent &input) { clemens_input(&mmio_, &input); }

auto ClemensAppleIIGS::stepMachine() -> ResultFlags {
    auto resultFlags = ResultFlags::None;
    auto vblStarted = mmio_.vgc.vbl_started;

    clemens_emulate_cpu(&machine_);
    clemens_emulate_mmio(&machine_, &mmio_);

    if (clemens_is_resetting(&machine_)) {
        resultFlags = resultFlags | ResultFlags::Resetting;
    }
    if (vblStarted && !mmio_.vgc.vbl_started) {
        resultFlags = resultFlags | ResultFlags::VerticalBlank;
    }
    status_ = machine_.cpu.enabled ? Status::Online : Status::Offline;

    return resultFlags;
}

ClemensAudio ClemensAppleIIGS::renderAudio() {
    ClemensAudio audio{};
    if (clemens_get_audio(&audio, &mmio_)) {
        if (mockingboard_) {
            float *audio_frame_head =
                reinterpret_cast<float *>(audio.data + audio.frame_start * audio.frame_stride);
            clem_card_ay3_render(mockingboard_, audio_frame_head, audio.frame_count,
                                 audio.frame_stride / sizeof(float), configAudioSamplesPerSecond_);
        }
    }
    return audio;
}

auto ClemensAppleIIGS::getFrame(Frame &frame) -> Frame & {
    if (!isOk())
        return frame;
    storage_.update(mmio_);
    clemens_get_monitor(&frame.monitor, &mmio_);
    clemens_get_text_video(&frame.text, &mmio_);
    clemens_get_graphics_video(&frame.graphics, &machine_, &mmio_);
    clemens_get_audio(&frame.audio, &mmio_);

    for (unsigned i = 0; i < (unsigned)frame.diskDriveStatuses.size(); ++i) {
        auto driveType = static_cast<ClemensDriveType>(i);
        frame.diskDriveStatuses[i] = storage_.getDriveStatus(driveType);
    }
    for (unsigned i = 0; i < (unsigned)frame.smartPortStatuses.size(); ++i) {
        frame.smartPortStatuses[i] = storage_.getSmartPortStatus(i);
    }

    return frame;
}

void ClemensAppleIIGS::finishFrame(Frame &frame) {
    if (!isOk())
        return;
    clemens_audio_next_frame(&mmio_, frame.audio.frame_count);
}

void ClemensAppleIIGS::enableOpcodeLogging(bool enable) {
    clemens_opcode_callback(&machine_, enable ? &ClemensAppleIIGS::emulatorOpcodeCallback : NULL);
}

void ClemensAppleIIGS::emulatorOpcodeCallback(struct ClemensInstruction *inst, const char *operand,
                                              void *this_ptr) {

    auto *host = reinterpret_cast<ClemensAppleIIGS *>(this_ptr);
    host->listener_.onClemensInstruction(inst, operand);
}

unsigned ClemensAppleIIGS::consume_utf8_input(const char *in, const char *inEnd) {
    const char *cur = clemens_clipboard_push_utf8_atom(&mmio_, in, inEnd);
    return (unsigned)(cur - in);
}

bool ClemensAppleIIGS::writeDataToMemory(const uint8_t *data, unsigned address, unsigned length) {
    if (address > 0xffffff)
        return false;
    for (unsigned i = 0; i < length; ++i) {
        clem_write(&machine_, data[i], (uint16_t)((address + i) & 0xffff),
                   (uint8_t)(((address + i) >> 16) & 0xff), 0);
    }
    return true;
}

bool ClemensAppleIIGS::readDataFromMemory(uint8_t *data, unsigned address, unsigned length) {
    if (address > 0xffffff)
        return false;
    for (unsigned i = 0; i < length; ++i) {
        clem_read(&machine_, &data[i], (uint16_t)((address + i) & 0xffff),
                  (uint8_t)(((address + i) >> 16) & 0xff), 0);
    }
    return true;
}
