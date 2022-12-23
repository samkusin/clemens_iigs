#include "clem_front.hpp"
#include "clem_backend.hpp"
#include "clem_disk_utils.hpp"
#include "clem_host_platform.h"
#include "clem_host_utils.hpp"
#include "clem_import_disk.hpp"
#include "clem_mem.h"
#include "clem_mmio_defs.h"
#include "emulator.h"
#include "emulator_mmio.h"
#include "version.h"

#include "cinek/encode.h"
#include "fmt/format.h"
#include "imgui_filedialog/ImGuiFileDialog.h"

#include <charconv>
#include <filesystem>
#include <tuple>

//  TODO: Platform specific user data directory (ROM, disk images, traces, etc)
//  TODO: Insert card to slot (non-gui)
//  TODO: Mouse x,y scaling based on display view size vs desktop size
//  TODO: blank disk gui selection for disk set (selecting combo create will
//        enable another input widget, unselecting will gray out that widget.)
//  TODO: preroll audio for some buffer on to handle sound clipping on lower end
//        systems

//  DONE: Diagnose GL offsets issue with emulated display
//  DONE: Diagnose Linux caps lock issue
//  DONE: Break on IRQ
//  DONE: List IRQ flags from emulator
//  DONE: Mouse lock and release (via keyboard combo ctl-f12?)
//  DONE: Mouse issues - in emulator - may be related to VGCINT?
//  DONE: Fix 80 column mode and card vs internal slot 3 ram mapping
//  DONE: restored mockingboard support
//  DONE: Fix sound clipping/starvation
//  DONE: Diagnostics
//  DONE: Save/Load snapshot
//  DONE: instruction trace (improved)
//  DONE: Fix reboot so that publish() copies *everything*
//  DONE: implement all current commands documented in help
//  DONE: memory debug gui
//  DONE: IO Debug View
//  DONE: Instruction step and debug out
//  DONE: memory dump command (non-gui)

template <typename TBufferType> struct FormatView {
    using BufferType = TBufferType;
    using StringType = typename BufferType::ValueType;
    using LevelType = typename StringType::Type;

    BufferType &buffer_;
    bool &viewChanged_;
    FormatView(BufferType &buffer, bool &viewChanged)
        : buffer_(buffer), viewChanged_(viewChanged) {}
    template <typename... Args>
    void format(LevelType type, const char *formatStr, const Args &...args) {
        size_t sz = fmt::formatted_size(formatStr, args...);
        auto &line = rotateBuffer();
        line.type = type;
        line.text.resize(sz);
        fmt::format_to_n(line.text.data(), sz, formatStr, args...);
    }
    void print(LevelType type, const char *text) {
        auto &line = rotateBuffer();
        line.type = type;
        line.text.assign(text);
    }
    void print(LevelType type, std::string_view text) {
        auto &line = rotateBuffer();
        line.type = type;
        line.text.assign(text);
    }
    void newline() {
        auto &line = rotateBuffer();
        line.type = LevelType::Info;
        line.text.clear();
    }

  private:
    StringType &rotateBuffer() {
        if (buffer_.isFull()) {
            buffer_.pop();
        }
        StringType &tail = *buffer_.acquireTail();
        buffer_.push();
        viewChanged_ = true;
        return tail;
    }
};

namespace {

constexpr uint8_t kIWMStatusDriveSpin = 0x01;
constexpr uint8_t kIWMStatusDrive35 = 0x02;
constexpr uint8_t kIWMStatusDriveAlt = 0x04;
constexpr uint8_t kIWMStatusDriveOn = 0x08;
constexpr uint8_t kIWMStatusDriveWP = 0x10;
constexpr uint8_t kIWMStatusDriveSel = 0x20;
constexpr uint8_t kIWMStatusIWMQ6 = 0x40;
constexpr uint8_t kIWMStatusIWMQ7 = 0x80;

//  TODO: move into clemens library
struct ClemensIODescriptor {
    char readLabel[16];
    char writeLabel[16];
    char readDesc[48];
    char writeDesc[48];
    uint8_t reg;
};

std::array<ClemensIODescriptor, 256> sDebugIODescriptors;

void initDebugIODescriptors() {
    sDebugIODescriptors[CLEM_MMIO_REG_KEYB_READ] = ClemensIODescriptor{
        "KBD",
        "CLR80COL",
        "Last key pressed.",
        "Disable 80-column store.",
    };
    sDebugIODescriptors[CLEM_MMIO_REG_80STOREON_WRITE] = ClemensIODescriptor{
        "",
        "SET80COL",
        "",
        "Enable 80-column store.",
    };
    sDebugIODescriptors[CLEM_MMIO_REG_RDMAINRAM] = ClemensIODescriptor{
        "",
        "RDMAINRAM",
        "",
        "Read from main 48K RAM.",
    };
    sDebugIODescriptors[CLEM_MMIO_REG_RDCARDRAM] = ClemensIODescriptor{
        "",
        "RDCARDRAM",
        "",
        "Read from alt 48K RAM.",
    };
    // TODO: 0x04 - 0x0f
    sDebugIODescriptors[CLEM_MMIO_REG_ANYKEY_STROBE] = ClemensIODescriptor{
        "KBDSTRB",
        "",
        "Turn off keypressed, b7=ANYKEY",
        "",
    };
    sDebugIODescriptors[CLEM_MMIO_REG_LC_BANK_TEST] = ClemensIODescriptor{
        "RDLCBNK2",
        "",
        "b7=LC Bank 2 enabled",
        "",
    };
    sDebugIODescriptors[CLEM_MMIO_REG_ROM_RAM_TEST] = ClemensIODescriptor{
        "RDLCRAM",
        "",
        "b7=LC RAM enabled",
        "",
    };
    sDebugIODescriptors[CLEM_MMIO_REG_RAMRD_TEST] = ClemensIODescriptor{
        "RDRAMRD",
        "",
        "b7=Alt 48K read enabled",
        "",
    };
    sDebugIODescriptors[CLEM_MMIO_REG_RAMWRT_TEST] = ClemensIODescriptor{
        "RDRAMWRT",
        "",
        "b7=Alt 48K write enabled",
        "",
    };
    sDebugIODescriptors[CLEM_MMIO_REG_READCXROM] = ClemensIODescriptor{
        "RDCXROM",
        "",
        "b7=Internal slot ROM enabled",
        "",
    };
    sDebugIODescriptors[CLEM_MMIO_REG_RDALTZP_TEST] = ClemensIODescriptor{
        "RDALTZP",
        "",
        "b7=Aux bank zero page + LC",
        "",
    };
    sDebugIODescriptors[CLEM_MMIO_REG_READC3ROM] = ClemensIODescriptor{
        "RDC3ROM",
        "",
        "b7=Slot C300 space enabled",
        "",
    };
    sDebugIODescriptors[CLEM_MMIO_REG_80COLSTORE_TEST] = ClemensIODescriptor{
        "RD80COL",
        "",
        "b7=80-column store enabled",
        "",
    };
    sDebugIODescriptors[CLEM_MMIO_REG_VBLBAR] = ClemensIODescriptor{
        "RDVBLBAR",
        "",
        "b7=Not in vertical blank region",
        "",
    };
    sDebugIODescriptors[CLEM_MMIO_REG_TXT_TEST] = ClemensIODescriptor{
        "RDTEXT",
        "",
        "b7=Text mode enabled",
        "",
    };
    sDebugIODescriptors[CLEM_MMIO_REG_MIXED_TEST] = ClemensIODescriptor{
        "RDMIX",
        "",
        "b7=Text+Graphics mode enabled",
        "",
    };
    sDebugIODescriptors[CLEM_MMIO_REG_TXTPAGE2_TEST] = ClemensIODescriptor{
        "RDPAGE2",
        "",
        "b7=Text Page 2 enabled",
        "",
    };
    sDebugIODescriptors[CLEM_MMIO_REG_HIRES_TEST] = ClemensIODescriptor{
        "RDHIRES",
        "",
        "b7=Hires mode on",
        "",
    };
    sDebugIODescriptors[CLEM_MMIO_REG_ALTCHARSET_TEST] = ClemensIODescriptor{
        "ALTCHARSET",
        "",
        "b7=Alternate chararacter set on",
        "",
    };
    sDebugIODescriptors[CLEM_MMIO_REG_80COLUMN_TEST] = ClemensIODescriptor{
        "RD80VID",
        "",
        "b7=80-column hardware on",
        "",
    };
    //  NOP: 0x20
    sDebugIODescriptors[CLEM_MMIO_REG_VGC_MONO] = ClemensIODescriptor{
        "MONOCOLOR",
        "MONOCOLOR",
        "b7=Monochrome enabled",
        "b7=monochrome enabled",
    };
    sDebugIODescriptors[CLEM_MMIO_REG_VGC_TEXT_COLOR] = ClemensIODescriptor{
        "TBCOLOR",
        "TBCOLOR",
        "bits [7:4]=foreground, [3:0]=background",
        "bits [7:4]=foreground, [3:0]=background",
    };
    sDebugIODescriptors[CLEM_MMIO_REG_VGC_IRQ_BYTE] = ClemensIODescriptor{
        "VGCINT",
        "",
        "VGC Interrupt register",
        "",
    };
    sDebugIODescriptors[CLEM_MMIO_REG_ADB_MOUSE_DATA] = ClemensIODescriptor{
        "MOUSEDATA",
        "",
        "ADB Mouse status",
        "",
    };
    sDebugIODescriptors[CLEM_MMIO_REG_ADB_MODKEY] = ClemensIODescriptor{
        "KEYMODREG",
        "",
        "ADB Modifier Key status",
        "",
    };
    sDebugIODescriptors[CLEM_MMIO_REG_ADB_CMD_DATA] = ClemensIODescriptor{
        "DATAREG",
        "DATAREG",
        "ADB Data In",
        "ADB Data Out",
    };
    sDebugIODescriptors[CLEM_MMIO_REG_ADB_STATUS] = ClemensIODescriptor{
        "KMSTATUS",
        "",
        "ADB Status",
        "",
    };
    // NOP: 0x28
    sDebugIODescriptors[CLEM_MMIO_REG_NEWVIDEO] = ClemensIODescriptor{
        "NEWVIDEO",
        "NEWVIDEO",
        "IIGS video state/super-hires",
        "IIGS video state/super-hires",
    };
    //  NOP: 0x2A
    sDebugIODescriptors[CLEM_MMIO_REG_LANGSEL] = ClemensIODescriptor{
        "LANGSEL",
        "LANGSEL",
        "IIGS language and NTSC/PAL",
        "IIGS language and NTSC/PAL",
    };
    //  NOP: 0x2C
    sDebugIODescriptors[CLEM_MMIO_REG_SLOTROMSEL] = ClemensIODescriptor{
        "SLTROMSEL",
        "SLTROMSEL",
        "Slot int/card ROM select",
        "Slot int/card ROM status",
    };
    sDebugIODescriptors[CLEM_MMIO_REG_VGC_VERTCNT] = ClemensIODescriptor{
        "VERTCNT",
        "",
        "IIGS vert. video counter bits",
        "",
    };
    sDebugIODescriptors[CLEM_MMIO_REG_VGC_HORIZCNT] = ClemensIODescriptor{
        "HORIZCNT",
        "",
        "IIGS horiz. video counter bits",
        "",
    };
    sDebugIODescriptors[CLEM_MMIO_REG_SPKR] = ClemensIODescriptor{
        "SPKR",
        "SPKR",
        "Apple II speaker pulse",
        "Apple II speaker pulse",
    };
    sDebugIODescriptors[CLEM_MMIO_REG_DISK_INTERFACE] = ClemensIODescriptor{
        "DISKREG",
        "DISKREG",
        "Apple 3.5 disk enable",
        "Apple 3.5 disk enable",
    };
    sDebugIODescriptors[CLEM_MMIO_REG_RTC_VGC_SCANINT] = ClemensIODescriptor{
        "",
        "SCANINT",
        "",
        "Scanline + 1 sec. interrupt control",
    };
    sDebugIODescriptors[CLEM_MMIO_REG_RTC_DATA] = ClemensIODescriptor{
        "CLOCKDATA",
        "CLOCKDATA",
        "RTC GLU Data register in",
        "RTC GLU Data register out",
    };
    sDebugIODescriptors[CLEM_MMIO_REG_RTC_CTL] = ClemensIODescriptor{
        "CLOCKCTL",
        "CLOCKCTL",
        "RTC GLU Control register out",
        "RTC GLU Control register in",
    };
    sDebugIODescriptors[CLEM_MMIO_REG_SHADOW] = ClemensIODescriptor{
        "SHADOW",
        "SHADOW",
        "IIGS Shadow I/O RAM Register",
        "IIGS Shadow I/O RAM Register",
    };
    sDebugIODescriptors[CLEM_MMIO_REG_SPEED] = ClemensIODescriptor{
        "CYAREG",
        "CYAREG",
        "IIGS System + Disk Speed",
        "IIGS System + Disk Speed",
    };
    //  TODO: 0x37 - 0x3B
    sDebugIODescriptors[CLEM_MMIO_REG_AUDIO_CTL] = ClemensIODescriptor{
        "SOUNDCTL",
        "SOUNDCTL",
        "IIGS Audio GLU Control",
        "IIGS Audio GLU Control",
    };
    sDebugIODescriptors[CLEM_MMIO_REG_AUDIO_DATA] = ClemensIODescriptor{
        "SOUNDDATA",
        "SOUNDDATA",
        "IIGS Audio GLU Data",
        "IIGS Audio GLU Data",
    };
    sDebugIODescriptors[CLEM_MMIO_REG_AUDIO_ADRLO] = ClemensIODescriptor{
        "SOUNDADRL",
        "SOUNDADRL",
        "IIGS Audio GLU Address Lo",
        "IIGS Audio GLU Address Lo",
    };
    sDebugIODescriptors[CLEM_MMIO_REG_AUDIO_ADRHI] = ClemensIODescriptor{
        "SOUNDADRH",
        "SOUNDADRH",
        "IIGS Audio GLU Address Hi",
        "IIGS Audio GLU Address Hi",
    };
    //  NOP: 0x40
    sDebugIODescriptors[CLEM_MMIO_REG_MEGA2_INTEN] = ClemensIODescriptor{
        "SOUNDADRL",
        "SOUNDADRL",
        "IIGS Mega2 Interrupt Enable",
        "IIGS Mega2 Interrupt Enable",
    };
    //  NOP: 0x42, 0x43
    //  TODO: 0x44 - 0x45
    sDebugIODescriptors[CLEM_MMIO_REG_DIAG_INTTYPE] = ClemensIODescriptor{
        "INTFLAG",
        "INTFLAG",
        "IIGS Mega2 Interrupt Status",
        "IIGS Mega2 Interrupt Status",
    };
    sDebugIODescriptors[CLEM_MMIO_REG_CLRVBLINT] = ClemensIODescriptor{
        "",
        "CLRVBLINT",
        "",
        "IIGS Clear VBL Interrupt",
    };
    //  TODO: 0x48
    //  NOP: 0x49 - 0x4e
    sDebugIODescriptors[CLEM_MMIO_REG_EMULATOR] = ClemensIODescriptor{
        "EMULATOR",
        "",
        "Emulator detection byte",
        "",
    };
    sDebugIODescriptors[CLEM_MMIO_REG_TXTCLR] = ClemensIODescriptor{
        "TXTCLR",
        "TXTCLR",
        "Switch to graphics mode",
        "Switch to graphics mode",
    };
    sDebugIODescriptors[CLEM_MMIO_REG_TXTSET] = ClemensIODescriptor{
        "TXTSET",
        "TXTSET",
        "Switch to text mode",
        "Switch to text mode",
    };
    sDebugIODescriptors[CLEM_MMIO_REG_MIXCLR] = ClemensIODescriptor{
        "MIXCLR",
        "MIXCLR",
        "Clear mixed graphics+text",
        "Clear mixed graphics+text",
    };
    sDebugIODescriptors[CLEM_MMIO_REG_MIXSET] = ClemensIODescriptor{
        "MIXSET",
        "MIXSET",
        "Sets mixed graphics+text",
        "Sets mixed graphics+text",
    };
    sDebugIODescriptors[CLEM_MMIO_REG_TXTPAGE1] = ClemensIODescriptor{
        "MIXSET",
        "MIXSET",
        "Sets text page 1",
        "Sets text page 1",
    };
    sDebugIODescriptors[CLEM_MMIO_REG_TXTPAGE2] = ClemensIODescriptor{
        "MIXSET",
        "MIXSET",
        "Sets text page 2",
        "Sets text page 2",
    };
    sDebugIODescriptors[CLEM_MMIO_REG_LORES] = ClemensIODescriptor{
        "LORES",
        "LORES",
        "Sets lo-res graphics mode",
        "Sets lo-res graphics mode",
    };
    sDebugIODescriptors[CLEM_MMIO_REG_HIRES] = ClemensIODescriptor{
        "HIRES",
        "HIRES",
        "Sets hi-res graphics mode",
        "Sets hi-res graphics mode",
    };
    //  TODO: 0x58 - 0x5F
    sDebugIODescriptors[CLEM_MMIO_REG_SW3] = ClemensIODescriptor{
        "BUTN3",
        "",
        "Reads switch 3",
        "",
    };
    sDebugIODescriptors[CLEM_MMIO_REG_SW0] = ClemensIODescriptor{
        "BUTN0",
        "",
        "Reads switch 0 open apple",
        "",
    };
    sDebugIODescriptors[CLEM_MMIO_REG_SW1] = ClemensIODescriptor{
        "BUTN1",
        "",
        "Reads switch 1 closed apple",
        "",
    };
    sDebugIODescriptors[CLEM_MMIO_REG_SW2] = ClemensIODescriptor{
        "BUTN2",
        "",
        "Reads switch 2",
        "",
    };
    sDebugIODescriptors[CLEM_MMIO_REG_PADDL0] = ClemensIODescriptor{
        "PADDL0",
        "",
        "Reads paddle axis port 0",
        "",
    };
    sDebugIODescriptors[CLEM_MMIO_REG_PADDL1] = ClemensIODescriptor{
        "PADDL1",
        "",
        "Reads paddle axis port 1",
        "",
    };
    sDebugIODescriptors[CLEM_MMIO_REG_PADDL2] = ClemensIODescriptor{
        "PADDL2",
        "",
        "Reads paddle axis port 2",
        "",
    };
    sDebugIODescriptors[CLEM_MMIO_REG_PADDL3] = ClemensIODescriptor{
        "PADDL3",
        "",
        "Reads paddle axis port 3",
        "",
    };
    sDebugIODescriptors[CLEM_MMIO_REG_STATEREG] = ClemensIODescriptor{
        "STATEREG",
        "STATEREG",
        "IIGS multi-purpose state register",
        "IIGS multi-purpose state register",
    };

    for (unsigned regIndex = 0; regIndex < 256; ++regIndex) {
        sDebugIODescriptors[regIndex].reg = regIndex;
    }
}

} // namespace

#define CLEM_TERM_COUT                                                                             \
    FormatView<decltype(ClemensFrontend::terminalLines_)>(terminalLines_, terminalChanged_)

static constexpr size_t kFrameMemorySize = 8 * 1024 * 1024;
static constexpr size_t kLogMemorySize = 8 * 1024 * 1024;

static std::string getCommandTypeName(ClemensBackendCommand::Type type) {
    switch (type) {
    case ClemensBackendCommand::AddBreakpoint:
        return "AddBreakpoint";
    case ClemensBackendCommand::Break:
        return "Break";
    case ClemensBackendCommand::DelBreakpoint:
        return "DelBreakpoint";
    case ClemensBackendCommand::EjectDisk:
        return "EjectDisk";
    case ClemensBackendCommand::InsertDisk:
        return "InsertDisk";
    case ClemensBackendCommand::ResetMachine:
        return "ResetMachine";
    case ClemensBackendCommand::RunMachine:
        return "RunMachine";
    case ClemensBackendCommand::SetHostUpdateFrequency:
        return "SetHostUpdateFrequency";
    case ClemensBackendCommand::Terminate:
        return "Terminate";
    default:
        return "Command";
    }
}

void ClemensFrontend::DOCStatus::copyFrom(const ClemensDeviceEnsoniq &doc) {
    memcpy(voice, doc.voice, sizeof(voice));
    memcpy(reg, doc.reg, sizeof(reg));
    memcpy(acc, doc.acc, sizeof(acc));
    memcpy(ptr, doc.ptr, sizeof(ptr));
    memcpy(osc_flags, doc.osc_flags, sizeof(osc_flags));
}

const uint64_t ClemensFrontend::kFrameSeqNoInvalid = std::numeric_limits<uint64_t>::max();

ClemensFrontend::ClemensFrontend(const cinek::ByteBuffer &systemFontLoBuffer,
                                 const cinek::ByteBuffer &systemFontHiBuffer)
    : displayProvider_(systemFontLoBuffer, systemFontHiBuffer), display_(displayProvider_),
      audio_(), frameSeqNo_(kFrameSeqNoInvalid), frameLastSeqNo_(kFrameSeqNoInvalid),
      frameWriteMemory_(kFrameMemorySize, malloc(kFrameMemorySize)),
      frameReadMemory_(kFrameMemorySize, malloc(kFrameMemorySize)),
      frameMemory_(kLogMemorySize, malloc(kLogMemorySize)), lastFrameCPUPins_{},
      lastFrameCPURegs_{}, lastFrameIWM_{}, lastFrameIRQs_(0), lastFrameNMIs_(0),
      emulatorHasKeyboardFocus_(true), emulatorHasMouseFocus_(false), terminalChanged_(false),
      consoleChanged_(false), terminalMode_(TerminalMode::Command),
      diskLibraryRootPath_{
          (std::filesystem::current_path() / std::filesystem::path(CLEM_HOST_LIBRARY_DIR))
              .string()},
      diskLibrary_(diskLibraryRootPath_, CLEM_DISK_TYPE_NONE, 256, 512), diskComboStateFlags_(0),
      debugIOMode_(DebugIOMode::Core), validJoystickIds_{-1, -1, -1, -1},
      guiMode_(GUIMode::Emulator) {

    ClemensTraceExecutedInstruction::initialize();

    initDebugIODescriptors();
    clem_joystick_open_devices(CLEM_HOST_JOYSTICK_PROVIDER_DEFAULT);

    audio_.start();
    config_.type = ClemensBackend::Config::Type::Apple2GS;
    config_.audioSamplesPerSecond = audio_.getAudioFrequency();

    auto audioBufferSize = config_.audioSamplesPerSecond * audio_.getBufferStride() / 2;
    lastCommandState_.audioBuffer =
        cinek::ByteBuffer(new uint8_t[audioBufferSize], audioBufferSize);
    thisFrameAudioBuffer_ = cinek::ByteBuffer(new uint8_t[audioBufferSize], audioBufferSize);

    config_.cardNames[3] = kClemensCardMockingboardName; // load the mockingboard
    // TODO: This should be selectable like regular drives - this will require some
    //       UI to make it happen
    config_.smartPortDriveStates[0].imagePath =
        (std::filesystem::path(diskLibraryRootPath_) / "smartport.2mg").string();
    backend_ = createBackend();

    debugMemoryEditor_.ReadFn = &ClemensFrontend::imguiMemoryEditorRead;
    debugMemoryEditor_.WriteFn = &ClemensFrontend::imguiMemoryEditorWrite;
    CLEM_TERM_COUT.format(TerminalLine::Info, "Welcome to the Clemens IIgs Emulator {}.{}",
                          CLEM_HOST_VERSION_MAJOR, CLEM_HOST_VERSION_MINOR);
}

ClemensFrontend::~ClemensFrontend() {
    backend_ = nullptr;
    audio_.stop();
    clem_joystick_close_devices();
    delete[] thisFrameAudioBuffer_.getHead();
    delete[] lastCommandState_.audioBuffer.getHead();
    free(frameWriteMemory_.getHead());
    free(frameReadMemory_.getHead());
}

void ClemensFrontend::input(ClemensInputEvent input) {
    if (input.type == kClemensInputType_MouseButtonDown ||
        input.type == kClemensInputType_MouseButtonUp ||
        input.type == kClemensInputType_MouseMove) {
        if (!emulatorHasMouseFocus_) {
            input.type = kClemensInputType_None;
        }
    }
    if (emulatorHasKeyboardFocus_ && input.type != kClemensInputType_None) {
        if (input.type == kClemensInputType_MouseMove) {
            input.value_a = std::clamp(input.value_a, (int16_t)(-63), (int16_t)(63));
            input.value_b = std::clamp(input.value_b, (int16_t)(-63), (int16_t)(63));
        }
        backend_->inputEvent(input);
    }
}

std::unique_ptr<ClemensBackend> ClemensFrontend::createBackend() {
    constexpr unsigned refreshFrequency_ = 60;
    auto backend = std::make_unique<ClemensBackend>(
        "gs_rom_3.rom", config_,
        std::bind(&ClemensFrontend::backendStateDelegate, this, std::placeholders::_1));
    backend->setRefreshFrequency(refreshFrequency_);
    backend->reset();
    backend->run();
    guiMode_ = GUIMode::Emulator;
    fmt::print("Creating new backend emulator refreshing @ {} Hz.\n", refreshFrequency_);
    return backend;
}

void ClemensFrontend::backendStateDelegate(const ClemensBackendState &state) {
    copyState(state);
    framePublished_.notify_all();
}

void ClemensFrontend::copyState(const ClemensBackendState &state) {
    std::lock_guard<std::mutex> frameLock(frameMutex_);

    frameSeqNo_ = state.seqno;

    frameWriteMemory_.reset();

    frameWriteState_.cpu = state.machine->cpu;
    frameWriteState_.monitorFrame = state.monitor;

    //  copy scanlines as this data may become invalid on a frame-to-frame
    //  basis
    frameWriteState_.textFrame = state.text;
    if (frameWriteState_.textFrame.format != kClemensVideoFormat_None) {
        frameWriteState_.textFrame.scanlines =
            frameWriteMemory_.allocateArray<ClemensScanline>(state.text.scanline_limit);
        memcpy(frameWriteState_.textFrame.scanlines, state.text.scanlines,
               sizeof(ClemensScanline) * state.text.scanline_limit);
    }
    frameWriteState_.graphicsFrame = state.graphics;
    if (frameWriteState_.graphicsFrame.format != kClemensVideoFormat_None) {
        frameWriteState_.graphicsFrame.scanlines =
            frameWriteMemory_.allocateArray<ClemensScanline>(state.graphics.scanline_limit);
        memcpy(frameWriteState_.graphicsFrame.scanlines, state.graphics.scanlines,
               sizeof(ClemensScanline) * state.graphics.scanline_limit);
    }
    frameWriteState_.audioFrame = state.audio;
    frameWriteState_.backendCPUID = state.hostCPUID;
    frameWriteState_.fps = state.fps;
    frameWriteState_.mmioWasInitialized = state.mmio_was_initialized;
    frameWriteState_.isTracing = state.isTracing;
    frameWriteState_.emulatorSpeedMhz = state.emulatorSpeedMhz;
    frameWriteState_.emulatorClock.ts = state.machine->tspec.clocks_spent;
    frameWriteState_.emulatorClock.ref_step = CLEM_CLOCKS_MEGA2_CYCLE;
    //  copy over component state as needed
    frameWriteState_.vgcModeFlags = state.mmio->vgc.mode_flags;
    frameWriteState_.irqs = state.mmio->irq_line;
    frameWriteState_.nmis = state.mmio->nmi_line;

    const ClemensDeviceIWM &iwm = state.mmio->dev_iwm;
    const ClemensDrive *iwmDrive = nullptr;
    frameWriteState_.iwm.status = 0;
    if (iwm.io_flags & CLEM_IWM_FLAG_DRIVE_ON) {
        frameWriteState_.iwm.status |= kIWMStatusDriveOn;
    }
    if (iwm.io_flags & CLEM_IWM_FLAG_DRIVE_35) {
        frameWriteState_.iwm.status |= kIWMStatusDrive35;
        iwmDrive = clemens_drive_get(state.mmio, kClemensDrive_3_5_D1);
    } else {
        iwmDrive = clemens_drive_get(state.mmio, kClemensDrive_5_25_D1);
    }
    if (iwm.io_flags & CLEM_IWM_FLAG_DRIVE_2) {
        frameWriteState_.iwm.status |= kIWMStatusDriveAlt;
        ++iwmDrive;
    }
    if (iwm.q6_switch) {
        frameWriteState_.iwm.status |= kIWMStatusIWMQ6;
    }
    if (iwm.q7_switch) {
        frameWriteState_.iwm.status |= kIWMStatusIWMQ7;
    }
    if (iwmDrive->is_spindle_on) {
        frameWriteState_.iwm.status |= kIWMStatusDriveSpin;
    }

    frameWriteState_.iwm.qtr_track_index = iwmDrive->qtr_track_index;
    frameWriteState_.iwm.track_byte_index = iwmDrive->track_byte_index;
    frameWriteState_.iwm.track_bit_shift = iwmDrive->track_bit_shift;
    frameWriteState_.iwm.track_bit_length = iwmDrive->track_bit_length;
    frameWriteState_.iwm.data = iwm.data;
    frameWriteState_.iwm.latch = iwm.latch;
    frameWriteState_.iwm.ph03 = (uint8_t)(iwm.out_phase & 0xff);

    const uint8_t *diskBits = iwmDrive->disk.bits_data;
    unsigned diskTrackIndex = frameWriteState_.iwm.qtr_track_index;
    constexpr auto diskBufferSize = sizeof(frameWriteState_.iwm.buffer);
    if (iwmDrive->disk.track_initialized[diskTrackIndex]) {
        unsigned trackByteCount = iwmDrive->disk.track_byte_count[diskTrackIndex];
        unsigned left, right;
        if (iwmDrive->track_byte_index > 0) {
            left = iwmDrive->track_byte_index - 1;
        } else {
            left = trackByteCount - 1;
        }
        right = iwmDrive->track_byte_index + diskBufferSize - 1;
        if (right >= trackByteCount) {
            right = right - trackByteCount;
        }
        diskBits += iwmDrive->disk.track_byte_offset[diskTrackIndex];
        unsigned bufferIndex = 0;
        if (left > right) {
            for (; left < trackByteCount; ++left, ++bufferIndex) {
                frameWriteState_.iwm.buffer[bufferIndex] = diskBits[left];
            }
            left = 0;
        }
        for (; left <= right; ++left, ++bufferIndex) {
            frameWriteState_.iwm.buffer[bufferIndex] = diskBits[left];
        }
    } else {
        memset(frameWriteState_.iwm.buffer, 0, diskBufferSize);
    }

    //  copy over memory banks as needed
    frameWriteState_.bankE0 = (uint8_t *)frameWriteMemory_.allocate(CLEM_IIGS_BANK_SIZE);
    memcpy(frameWriteState_.bankE0, state.machine->mem.mega2_bank_map[0], CLEM_IIGS_BANK_SIZE);
    frameWriteState_.bankE1 = (uint8_t *)frameWriteMemory_.allocate(CLEM_IIGS_BANK_SIZE);
    memcpy(frameWriteState_.bankE1, state.machine->mem.mega2_bank_map[1], CLEM_IIGS_BANK_SIZE);

    frameWriteState_.ioPage = (uint8_t *)frameWriteMemory_.allocate(256);
    memcpy(frameWriteState_.ioPage, state.ioPageValues, 256);

    frameWriteState_.memoryViewBank = state.debugMemoryPage;
    if (!state.isRunning) {
        frameWriteState_.memoryView = (uint8_t *)frameWriteMemory_.allocate(CLEM_IIGS_BANK_SIZE);
        //  read every byte from the memory controller - which can be 'slow' enough
        //  to effect framerate on some systems.   so we only update memory state
        //  when the emulator isn't actively running instructions
        uint8_t *memoryView = reinterpret_cast<uint8_t *>(frameWriteState_.memoryView);
        for (unsigned addr = 0; addr < 0x10000; ++addr) {
            clem_read(state.machine, &memoryView[addr], addr, state.debugMemoryPage,
                      CLEM_MEM_FLAG_NULL);
        }

        constexpr size_t kDOCRAMSize = 65536;

        frameWriteState_.docRAM = (uint8_t *)frameWriteMemory_.allocate(kDOCRAMSize);
        memcpy(frameWriteState_.docRAM, &state.mmio->dev_audio.doc.sound_ram, kDOCRAMSize);
    } else {
        frameWriteState_.memoryView = nullptr;
        frameWriteState_.docRAM = nullptr;
    }
    frameWriteState_.doc.copyFrom(state.mmio->dev_audio.doc);

    const ClemensBackendDiskDriveState *driveState = state.diskDrives;
    for (auto &diskDrive : frameWriteState_.diskDrives) {
        diskDrive = *driveState;
        ++driveState;
    }

    frameWriteState_.breakpoints = frameWriteMemory_.allocateArray<ClemensBackendBreakpoint>(
        state.bpBufferEnd - state.bpBufferStart);
    frameWriteState_.breakpointCount = (unsigned)(state.bpBufferEnd - state.bpBufferStart);
    auto *bpDest = frameWriteState_.breakpoints;
    for (auto *bpCur = state.bpBufferStart; bpCur != state.bpBufferEnd; ++bpCur, ++bpDest) {
        *bpDest = *bpCur;
        if (state.bpHitIndex.has_value() && !lastCommandState_.hitBreakpoint.has_value()) {
            if (unsigned(bpCur - state.bpBufferStart) == *state.bpHitIndex) {
                lastCommandState_.hitBreakpoint = *state.bpHitIndex;
            }
        }
    }
    if (!lastCommandState_.message.has_value() && state.message.has_value()) {
        lastCommandState_.message = cmdMessageFromBackend(*state.message, state.machine);
    }
    if (!lastCommandState_.commandFailed.has_value()) {
        lastCommandState_.commandFailed = state.commandFailed;
    }
    if (!lastCommandState_.commandType.has_value()) {
        lastCommandState_.commandType = state.commandType;
    }
    if (!lastCommandState_.terminated.has_value()) {
        lastCommandState_.terminated = state.terminated;
    }

    auto audioBufferSize = int32_t(state.audio.frame_count * state.audio.frame_stride);
    auto audioBufferRange = lastCommandState_.audioBuffer.forwardSize(audioBufferSize);
    memcpy(audioBufferRange.first, state.audio.data, cinek::length(audioBufferRange));

    frameWriteState_.logLevel = state.logLevel;
    for (auto *logItem = state.logBufferStart; logItem != state.logBufferEnd; ++logItem) {
        LogOutputNode *logMemory = reinterpret_cast<LogOutputNode *>(frameMemory_.allocate(
            sizeof(LogOutputNode) + CK_ALIGN_SIZE_TO_ARCH(logItem->text.size())));
        logMemory->logLevel = logItem->level;
        logMemory->sz = unsigned(logItem->text.size());
        logItem->text.copy(reinterpret_cast<char *>(logMemory) + sizeof(LogOutputNode),
                           std::string::npos);
        logMemory->next = nullptr;
        if (!lastCommandState_.logNode) {
            lastCommandState_.logNode = logMemory;
        } else {
            lastCommandState_.logNodeTail->next = logMemory;
        }
        lastCommandState_.logNodeTail = logMemory;
    }

    if (state.logInstructionStart != state.logInstructionEnd) {
        size_t instructionCount = state.logInstructionEnd - state.logInstructionStart;
        LogInstructionNode *logInstMemory = reinterpret_cast<LogInstructionNode *>(
            frameMemory_.allocate(sizeof(LogInstructionNode)));
        logInstMemory->begin =
            frameMemory_.allocateArray<ClemensBackendExecutedInstruction>(instructionCount);
        logInstMemory->end = logInstMemory->begin + instructionCount;
        logInstMemory->next = nullptr;
        memcpy(logInstMemory->begin, state.logInstructionStart,
               instructionCount * sizeof(ClemensBackendExecutedInstruction));
        if (!lastCommandState_.logInstructionNode) {
            lastCommandState_.logInstructionNode = logInstMemory;
        } else {
            lastCommandState_.logInstructionNodeTail->next = logInstMemory;
        }
        lastCommandState_.logInstructionNodeTail = logInstMemory;
    }

    if (state.message.has_value()) {
        printf("debug message: %s\n", (*state.message).c_str());
    }
}

void ClemensFrontend::pollJoystickDevices() {
    ClemensHostJoystick joysticks[CLEM_HOST_JOYSTICK_LIMIT];
    unsigned deviceCount = clem_joystick_poll(joysticks);
    unsigned joystickCount = 0;
    ClemensInputEvent inputs[2];
    constexpr int32_t kGameportAxisMagnitude = CLEM_GAMEPORT_PADDLE_AXIS_VALUE_MAX;
    constexpr int32_t kHostAxisMagnitude = CLEM_HOST_JOYSTICK_AXIS_DELTA * 2;
    unsigned index;
    for (index = 0; index < (unsigned)validJoystickIds_.size(); ++index) {
        if (index >= deviceCount || joystickCount >= 2)
            break;
        auto &input = inputs[joystickCount];
        if (joysticks[index].isConnected) {
            validJoystickIds_[index] = index;
            //  TODO: select (x,y)[0] or (x,y)[1] based on user preference for
            //        console style controllers (left or right stick, left always for now)
            input.type = kClemensInputType_Paddle;
            input.value_a =
                (int16_t)((int32_t)(joysticks[index].x[0] + CLEM_HOST_JOYSTICK_AXIS_DELTA) *
                          kGameportAxisMagnitude / kHostAxisMagnitude);
            input.value_b =
                (int16_t)((int32_t)(joysticks[index].y[0] + CLEM_HOST_JOYSTICK_AXIS_DELTA) *
                          kGameportAxisMagnitude / kHostAxisMagnitude);
            input.gameport_button_mask = 0;
            //  TODO: again, select button 1 or 2 based on user configuration
            if (joysticks[index].buttons & CLEM_HOST_JOYSTICK_BUTTON_A) {
                input.gameport_button_mask |= 0x1;
            }
            if (joysticks[index].buttons & CLEM_HOST_JOYSTICK_BUTTON_B) {
                input.gameport_button_mask |= 0x2;
            }
            // printf("SKS: [%u] - A:(%d,%d)  B:(%d,%d)  BTN:%08X\n", index, joysticks[index].x[0],
            //        joysticks[index].y[0], joysticks[index].x[1], joysticks[index].y[1],
            //        joysticks[index].buttons);
            joystickCount++;
        } else if (validJoystickIds_[index] != -1) {
            input.type = kClemensInputType_PaddleDisconnected;
            input.gameport_button_mask = 0;
            joystickCount++;
            validJoystickIds_[index] = -1;
        }
        if (joystickCount == 1) {
            input.gameport_button_mask |= CLEM_GAMEPORT_BUTTON_MASK_JOYSTICK_0;
        }
        if (joystickCount == 2) {
            input.gameport_button_mask |= CLEM_GAMEPORT_BUTTON_MASK_JOYSTICK_1;
        }
    }
    if (joystickCount < 1)
        return;
    backend_->inputEvent(inputs[0]);
    if (joystickCount < 2)
        return;
    backend_->inputEvent(inputs[1]);
}

void ClemensFrontend::frame(int width, int height, double deltaTime, FrameAppInterop &interop) {
    //  send commands to emulator thread
    //  get results from emulator thread
    //    video, audio, machine state, etc

    pollJoystickDevices();

    bool isNewFrame = false;
    bool isBackendTerminated = false;
    std::unique_lock<std::mutex> frameLock(frameMutex_);
    framePublished_.wait_for(frameLock, std::chrono::milliseconds::zero(),
                             [this]() { return frameSeqNo_ != frameLastSeqNo_; });

    isNewFrame = frameLastSeqNo_ != frameSeqNo_;
    if (isNewFrame) {
        lastFrameCPURegs_ = frameReadState_.cpu.regs;
        lastFrameCPUPins_ = frameReadState_.cpu.pins;
        lastFrameIRQs_ = frameReadState_.irqs;
        lastFrameNMIs_ = frameReadState_.nmis;
        lastFrameIWM_ = frameReadState_.iwm;
        if (frameReadState_.ioPage) {
            memcpy(lastFrameIORegs_, frameReadState_.ioPage, 256);
        }
        std::swap(frameWriteMemory_, frameReadMemory_);
        std::swap(frameWriteState_, frameReadState_);
        //  display log lines
        LogOutputNode *logNode = lastCommandState_.logNode;
        while (logNode) {
            if (consoleLines_.isFull()) {
                consoleLines_.pop();
            }
            TerminalLine logLine;
            logLine.text.assign((char *)logNode + sizeof(LogOutputNode), logNode->sz);
            switch (logNode->logLevel) {
            case CLEM_DEBUG_LOG_DEBUG:
                logLine.type = TerminalLine::Debug;
                break;
            case CLEM_DEBUG_LOG_INFO:
                logLine.type = TerminalLine::Info;
                break;
            case CLEM_DEBUG_LOG_WARN:
                logLine.type = TerminalLine::Warn;
                break;
            case CLEM_DEBUG_LOG_FATAL:
            case CLEM_DEBUG_LOG_UNIMPL:
                logLine.type = TerminalLine::Error;
                break;
            default:
                logLine.type = TerminalLine::Info;
                break;
            }
            consoleLines_.push(std::move(logLine));
            logNode = logNode->next;
            consoleChanged_ = true;
        }
        lastCommandState_.logNode = lastCommandState_.logNodeTail = nullptr;
        //  display last few log instructions
        LogInstructionNode *logInstructionNode = lastCommandState_.logInstructionNode;

        while (logInstructionNode) {
            ClemensBackendExecutedInstruction *execInstruction = logInstructionNode->begin;
            ClemensTraceExecutedInstruction instruction;
            while (execInstruction != logInstructionNode->end) {
                instruction.fromInstruction(execInstruction->data, execInstruction->operand);
                CLEM_TERM_COUT.format(TerminalLine::Opcode, "({}) {:02X}/{:04X} {} {}",
                                      instruction.cycles_spent, instruction.pc >> 16,
                                      instruction.pc & 0xffff, instruction.opcode,
                                      instruction.operand);
                ++execInstruction;
            }
            logInstructionNode = logInstructionNode->next;
        }
        lastCommandState_.logInstructionNode = lastCommandState_.logInstructionNodeTail = nullptr;

        breakpoints_.clear();
        for (unsigned bpIndex = 0; bpIndex < frameReadState_.breakpointCount; ++bpIndex) {
            breakpoints_.emplace_back(frameReadState_.breakpoints[bpIndex]);
            config_.breakpoints.push_back(breakpoints_.back());
        }
        config_.breakpoints = breakpoints_;
        if (lastCommandState_.commandFailed.has_value()) {
            if (*lastCommandState_.commandFailed) {
                CLEM_TERM_COUT.format(TerminalLine::Error, "{} Failed.",
                                      getCommandTypeName(*lastCommandState_.commandType));
            } else {
                CLEM_TERM_COUT.print(TerminalLine::Info, "Ok.");
            }
            lastCommandState_.commandFailed = std::nullopt;
            lastCommandState_.commandType = std::nullopt;
        }
        if (lastCommandState_.hitBreakpoint.has_value()) {
            unsigned bpIndex = *lastCommandState_.hitBreakpoint;
            CLEM_TERM_COUT.format(TerminalLine::Info, "Breakpoint {} hit {:02X}/{:04X}.", bpIndex,
                                  (breakpoints_[bpIndex].address >> 16) & 0xff,
                                  breakpoints_[bpIndex].address & 0xffff);
            emulatorHasMouseFocus_ = false;
            lastCommandState_.hitBreakpoint = std::nullopt;
        }
        if (lastCommandState_.message.has_value()) {
            cmdMessageLocal(*lastCommandState_.message);
            lastCommandState_.message = std::nullopt;
        }
        if (lastCommandState_.terminated.has_value()) {
            isBackendTerminated = *lastCommandState_.terminated;
            lastCommandState_.terminated = std::nullopt;
        }

        for (size_t driveIndex = 0; driveIndex < frameReadState_.diskDrives.size(); ++driveIndex) {
            config_.diskDriveStates[driveIndex] = frameReadState_.diskDrives[driveIndex];
        }

        frameMemory_.reset();

        std::swap(lastCommandState_.audioBuffer, thisFrameAudioBuffer_);

        frameLastSeqNo_ = frameSeqNo_;
    }
    frameLock.unlock();

    //  render video
    constexpr int kClemensScreenWidth = 720;
    constexpr int kClemensScreenHeight = 480;
    float screenUVs[2]{0.0f, 0.0f};

    if (frameReadState_.mmioWasInitialized && guiMode_ != GUIMode::RebootEmulator) {
        const uint8_t *e0mem = frameReadState_.bankE0;
        const uint8_t *e1mem = frameReadState_.bankE1;
        bool altCharSet = frameReadState_.vgcModeFlags & CLEM_VGC_ALTCHARSET;
        bool text80col = frameReadState_.vgcModeFlags & CLEM_VGC_80COLUMN_TEXT;
        display_.start(frameReadState_.monitorFrame, kClemensScreenWidth, kClemensScreenHeight);
        display_.renderTextGraphics(frameReadState_.textFrame, frameReadState_.graphicsFrame, e0mem,
                                    e1mem, text80col, altCharSet);
        if (frameReadState_.graphicsFrame.format == kClemensVideoFormat_Double_Hires) {
            display_.renderDoubleHiresGraphics(frameReadState_.graphicsFrame, e0mem, e1mem);
        } else if (frameReadState_.graphicsFrame.format == kClemensVideoFormat_Hires) {
            display_.renderHiresGraphics(frameReadState_.graphicsFrame, e0mem);
        } else if (frameReadState_.graphicsFrame.format == kClemensVideoFormat_Super_Hires) {
            display_.renderSuperHiresGraphics(frameReadState_.graphicsFrame, e1mem);
        }
        display_.finish(screenUVs);

        // render audio
        if (isNewFrame && thisFrameAudioBuffer_.getSize() > 0) {
            ClemensAudio audioFrame;
            audioFrame.data = thisFrameAudioBuffer_.getHead();
            audioFrame.frame_stride = frameReadState_.audioFrame.frame_stride;
            audioFrame.frame_start = 0;
            audioFrame.frame_count = thisFrameAudioBuffer_.getSize() / audioFrame.frame_stride;
            audioFrame.frame_total = thisFrameAudioBuffer_.getCapacity() / audioFrame.frame_stride;
            audio_.queue(audioFrame, deltaTime);
            thisFrameAudioBuffer_.reset();
        }
    }

    const int kLineSpacing = ImGui::GetTextLineHeightWithSpacing();
    const int kTerminalViewHeight = std::max(kLineSpacing * 6, int(height * 0.33f));
    const int kMachineStateViewWidth = std::max(int(width * 0.40f), 480);
    const int kMonitorX = kMachineStateViewWidth;
    const int kMonitorViewWidth = width - kMonitorX;
    const int kMonitorViewHeight = height - kTerminalViewHeight;
    ImVec2 monitorSize(kMonitorViewWidth, kMonitorViewHeight);

    doMachineStateLayout(ImVec2(0, 0), ImVec2(kMachineStateViewWidth, height));
    doMachineViewLayout(ImVec2(kMonitorX, 0), monitorSize, screenUVs[0], screenUVs[1]);
    doMachineTerminalLayout(ImVec2(kMonitorX, height - kTerminalViewHeight),
                            ImVec2(width - kMonitorX, kTerminalViewHeight));
    switch (guiMode_) {
    case GUIMode::ImportDiskModal:
    case GUIMode::BlankDiskModal:
        doModalOperations(width, height);
        break;
    case GUIMode::ImportDiskSetFlow:
        doImportDiskSetFlowStart(width, height);
        break;
    case GUIMode::ImportDiskSetReplaceOld:
    case GUIMode::NewBlankDiskReplaceOld:
        doImportDiskSetReplaceOld(width, height);
        break;
    case GUIMode::ImportDiskSet:
        doImportDiskSet(width, height);
        break;
    case GUIMode::NewBlankDiskFlow:
        doNewBlankDiskFlow(width, height);
        break;
    case GUIMode::NewBlankDisk:
        doNewBlankDisk(width, height);
        break;
    case GUIMode::RebootEmulator:
        if (isBackendTerminated) {
            backend_ = createBackend();
        }
        break;
    default:
        break;
    }

    backend_->publish();

    if (ImGui::IsKeyPressed(ImGuiKey_Space) && ImGui::GetIO().KeyAlt && ImGui::GetIO().KeySuper) {
        emulatorHasMouseFocus_ = false;
    }

    interop.mouseLock = emulatorHasMouseFocus_;
}

static ImColor getModifiedColor(bool hi) {
    const ImColor kModifiedColor(255, 0, 255, hi ? 255 : 128);
    return kModifiedColor;
}

static ImColor getDefaultColor(bool hi) {
    const ImColor kDefaultColor(255, 255, 255, hi ? 255 : 128);
    return kDefaultColor;
}

template <typename T> static ImColor getStatusFieldColor(T a, T b, T statusMask) {
    return (a & statusMask) != (b & statusMask) ? getModifiedColor(b & statusMask)
                                                : getDefaultColor(b & statusMask);
}

void ClemensFrontend::doMachineStateLayout(ImVec2 rootAnchor, ImVec2 rootSize) {
    ImGui::SetNextWindowPos(rootAnchor);
    ImGui::SetNextWindowSize(rootSize);
    ImGui::Begin("Status", nullptr,
                 ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize |
                     ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoBringToFrontOnFocus |
                     ImGuiWindowFlags_NoMove);
    doMachineDiagnosticsDisplay();
    ImGui::Separator();
    doMachineDiskDisplay();
    ImGui::Separator();
    doMachineCPUInfoDisplay();
    ImGui::Separator();

    if (ImGui::BeginTabBar("IODebug")) {
        if (ImGui::BeginTabItem("Core")) {
            doMachineDebugCoreIODisplay();
            ImGui::EndTabItem();
        }
        if (ImGui::BeginTabItem("Video")) {
            doMachineDebugVideoIODisplay();
            ImGui::EndTabItem();
        }
        if (ImGui::BeginTabItem("IWM")) {
            doMachineDebugDiskIODisplay();
            ImGui::EndTabItem();
        }
        if (ImGui::BeginTabItem("Input")) {
            doMachineDebugADBDisplay();
            ImGui::EndTabItem();
        }
        if (ImGui::BeginTabItem("Sound")) {
            doMachineDebugSoundDisplay();
            ImGui::EndTabItem();
        }
        ImGui::EndTabBar();
    }
    ImGui::Separator();
    ImGui::BeginChild("MachineStateLower", ImGui::GetContentRegionAvail());
    if (ImGui::BeginTabBar("CompDebug")) {
        if (ImGui::BeginTabItem("Memory")) {
            doMachineDebugMemoryDisplay();
            ImGui::EndTabItem();
        }
        if (ImGui::BeginTabItem("DOC")) {
            doMachineDebugDOCDisplay();
            ImGui::EndTabItem();
        }
        ImGui::EndTabBar();
    }
    ImGui::EndChild();
    ImGui::End();
}

void ClemensFrontend::doMachineDiagnosticsDisplay() {
    auto fontCharSize = ImGui::GetFont()->GetCharAdvance('A');
    uint64_t emulatorTime = 0;
    if (frameSeqNo_ != kFrameSeqNoInvalid) {
        emulatorTime =
            (uint64_t)(clem_calc_secs_from_clocks(&frameReadState_.emulatorClock) * 1000);
    }
    ImGui::BeginTable("Diagnostics", 3);
    ImGui::TableSetupColumn("CPU", ImGuiTableColumnFlags_WidthFixed, fontCharSize * 10);
    ImGui::TableSetupColumn("Type", ImGuiTableColumnFlags_WidthFixed, fontCharSize * 6);
    ImGui::TableSetupColumn("Value");
    ImGui::TableNextRow();
    ImGui::TableNextColumn();
    ImGui::Text("CPU %02u", clem_host_get_processor_number());
    ImGui::TableNextColumn();
    ImGui::Text("GUI");
    ImGui::TableNextColumn();
    ImGui::Text("%3.1f fps", ImGui::GetIO().Framerate);
    ImGui::TableNextRow();
    ImGui::TableNextColumn();
    ImGui::Text("CPU %02u", frameReadState_.backendCPUID);
    ImGui::TableNextColumn();
    ImGui::Text("EMU");
    ImGui::TableNextColumn();
    ImGui::Text("%3.1f fps", frameReadState_.fps);
    ImGui::TableNextRow();
    ImGui::TableNextColumn();
    ImGui::TextUnformatted("");
    ImGui::TableNextColumn();
    ImGui::Text("RUN");
    ImGui::TableNextColumn();
    ImGui::Text("%3.3f mhz", frameReadState_.emulatorSpeedMhz);
    ImGui::TableNextRow();
    ImGui::TableNextColumn();
    ImGui::TextUnformatted("");
    ImGui::TableNextColumn();
    ImGui::Text("TIME");
    ImGui::TableNextColumn();
    unsigned hours = emulatorTime / 3600000;
    unsigned minutes = (emulatorTime % 3600000) / 60000;
    unsigned seconds = ((emulatorTime % 3600000) % 60000) / 1000;
    unsigned milliseconds = ((emulatorTime % 3600000) % 60000) % 1000;
    ImGui::Text("%02u:%02u:%02u.%03u", hours, minutes, seconds, milliseconds);
    ImGui::EndTable();
}

void ClemensFrontend::doMachineDiskDisplay() {
    ImGui::BeginTable("DiskSelect", 2);
    ImGui::TableSetupColumn(
        "WP", ImGuiTableColumnFlags_WidthFixed,
        (ImGui::GetStyle().FramePadding.x + ImGui::GetFont()->GetCharAdvance('W')) * 2);
    ImGui::TableSetupColumn("Image", ImGuiTableColumnFlags_WidthStretch);
    ImGui::TableHeadersRow();
    ImGui::TableNextColumn();
    doMachineDiskStatus(kClemensDrive_3_5_D1);
    ImGui::TableNextColumn();
    doMachineDiskSelection(kClemensDrive_3_5_D1);
    ImGui::TableNextRow();
    ImGui::TableNextColumn();
    doMachineDiskStatus(kClemensDrive_3_5_D2);
    ImGui::TableNextColumn();
    doMachineDiskSelection(kClemensDrive_3_5_D2);
    ImGui::TableNextRow();
    ImGui::TableNextColumn();
    doMachineDiskStatus(kClemensDrive_5_25_D1);
    ImGui::TableNextColumn();
    doMachineDiskSelection(kClemensDrive_5_25_D1);
    ImGui::TableNextRow();
    ImGui::TableNextColumn();
    doMachineDiskStatus(kClemensDrive_5_25_D2);
    ImGui::TableNextColumn();
    doMachineDiskSelection(kClemensDrive_5_25_D2);
    ImGui::EndTable();
}

static const char *sDriveName[] = {"Slot5 D1", "Slot5 D2", "Slot6 D1", "Slot6 D2"};
static const char *sDriveDescription[] = {"3.5 inch 800K", "3.5 inch 800K", "5.25 inch 140K",
                                          "5.25 inch 140K"};

void ClemensFrontend::doMachineDiskSelection(ClemensDriveType driveType) {
    const ClemensBackendDiskDriveState &drive = frameReadState_.diskDrives[driveType];
    //  2 states: empty, has disk
    //    options if empty: <blank disk>, <import image>, image 0, image 1, ...
    //    options if full: <eject>
    const char *imageName;
    if (drive.isEjecting) {
        imageName = "Ejecting...";
    } else if (drive.imagePath.empty()) {
        imageName = "- Empty";
    } else {
        imageName = drive.imagePath.c_str();
    }
    if (ImGui::BeginCombo(sDriveName[driveType], imageName, ImGuiComboFlags_NoArrowButton)) {
        if (!(diskComboStateFlags_ & (1 << driveType))) {
            if (driveType == kClemensDrive_3_5_D1 || driveType == kClemensDrive_3_5_D2) {
                diskLibrary_.reset(diskLibraryRootPath_, CLEM_DISK_TYPE_3_5);
            } else if (driveType == kClemensDrive_5_25_D1 || driveType == kClemensDrive_5_25_D2) {
                diskLibrary_.reset(diskLibraryRootPath_, CLEM_DISK_TYPE_5_25);
            } else {
                diskLibrary_.reset(diskLibraryRootPath_, CLEM_DISK_TYPE_NONE);
            }
            diskComboStateFlags_ |= (1 << driveType);
        } else {
            diskLibrary_.update();
        }
        if (!drive.imagePath.empty() && !drive.isEjecting && ImGui::Selectable("<eject>")) {
            backend_->ejectDisk(driveType);
        }
        if (drive.imagePath.empty()) {
            if (ImGui::Selectable("<insert blank disk>")) {
                guiMode_ = GUIMode::BlankDiskModal;
                importDriveType_ = driveType;
            }
            if (ImGui::Selectable("<import master>")) {
                guiMode_ = GUIMode::ImportDiskModal;
                importDriveType_ = driveType;
            }
            ImGui::Separator();
            std::filesystem::path selectedPath;
            diskLibrary_.iterate([&selectedPath](const ClemensDiskLibrary::DiskEntry &entry) {
                auto relativePath = entry.location.parent_path().filename() / entry.location.stem();
                if (ImGui::Selectable(relativePath.string().c_str())) {
                    selectedPath = entry.location.string();
                }
            });
            if (!selectedPath.empty()) {
                backend_->insertDisk(driveType, selectedPath.string());
            }
            ImGui::Separator();
        }
        ImGui::EndCombo();
    } else {
        if (diskComboStateFlags_ & (1 << driveType)) {
            diskComboStateFlags_ &= ~(1 << driveType);
        }
    }
}

void ClemensFrontend::doMachineDiskStatus(ClemensDriveType driveType) {
    const ClemensBackendDiskDriveState &drive = frameReadState_.diskDrives[driveType];
    if (drive.imagePath.empty())
        return;

    bool wp = drive.isWriteProtected;
    if (ImGui::Checkbox("", &wp)) {
        backend_->writeProtectDisk(driveType, wp);
    }
}

#define CLEM_HOST_GUI_CPU_PINS_COLOR(_field_)                                                      \
    lastFrameCPUPins_._field_ != frameReadState_.cpu.pins._field_                                  \
        ? getModifiedColor(frameReadState_.cpu.pins._field_)                                       \
        : getDefaultColor(frameReadState_.cpu.pins._field_)

#define CLEM_HOST_GUI_CPU_PINS_INV_COLOR(_field_)                                                  \
    lastFrameCPUPins_._field_ != frameReadState_.cpu.pins._field_                                  \
        ? getModifiedColor(!frameReadState_.cpu.pins._field_)                                      \
        : getDefaultColor(!frameReadState_.cpu.pins._field_)

#define CLEM_HOST_GUI_CPU_FIELD_COLOR(_field_)                                                     \
    lastFrameCPURegs_._field_ != frameReadState_.cpu.regs._field_ ? getModifiedColor(true)         \
                                                                  : getDefaultColor(true)

void ClemensFrontend::doMachineCPUInfoDisplay() {
    ImGui::BeginTable("Machine", 3);
    // Registers
    ImGui::TableSetupColumn("Registers", ImGuiTableColumnFlags_WidthStretch);
    // Signals
    ImGui::TableSetupColumn("", ImGuiTableColumnFlags_WidthStretch);
    // I/O
    ImGui::TableSetupColumn("Pins", ImGuiTableColumnFlags_WidthStretch);
    ImGui::TableHeadersRow();
    //  Registers Column
    ImGui::TableNextColumn();
    ImGui::BeginTable("Reg1Inner", 1);
    ImGui::TableNextColumn();
    ImGui::TextColored(CLEM_HOST_GUI_CPU_FIELD_COLOR(PBR), "PBR  =%02X  ",
                       frameReadState_.cpu.regs.PBR);
    ImGui::TableNextRow();
    ImGui::TableNextColumn();
    ImGui::TextColored(CLEM_HOST_GUI_CPU_FIELD_COLOR(PC), "PC   =%04X",
                       frameReadState_.cpu.regs.PC);
    ImGui::TableNextRow();
    ImGui::TableNextColumn();
    ImGui::TextColored(CLEM_HOST_GUI_CPU_FIELD_COLOR(IR), "IR   =%02X  ",
                       frameReadState_.cpu.regs.IR);
    ImGui::TableNextRow();
    ImGui::TableNextColumn();
    ImGui::TextColored(CLEM_HOST_GUI_CPU_FIELD_COLOR(DBR), "DBR  =%02X  ",
                       frameReadState_.cpu.regs.DBR);
    ImGui::TableNextRow();
    ImGui::TableNextColumn();
    ImGui::TextColored(getStatusFieldColor<uint8_t>(lastFrameCPURegs_.P, frameReadState_.cpu.regs.P,
                                                    kClemensCPUStatus_Negative),
                       "n");
    ImGui::SameLine(0.0f, 4.0f);
    ImGui::TextColored(getStatusFieldColor<uint8_t>(lastFrameCPURegs_.P, frameReadState_.cpu.regs.P,
                                                    kClemensCPUStatus_Overflow),
                       "v");
    ImGui::SameLine(0.0f, 4.0f);
    ImGui::TextColored(getStatusFieldColor<uint8_t>(lastFrameCPURegs_.P, frameReadState_.cpu.regs.P,
                                                    kClemensCPUStatus_MemoryAccumulator),
                       frameReadState_.cpu.pins.emulation ? " " : "m");
    ImGui::SameLine(0.0f, 4.0f);
    ImGui::TextColored(getStatusFieldColor<uint8_t>(lastFrameCPURegs_.P, frameReadState_.cpu.regs.P,
                                                    kClemensCPUStatus_Index),
                       frameReadState_.cpu.pins.emulation ? " " : "x");
    ImGui::SameLine(0.0f, 4.0f);
    ImGui::TextColored(getStatusFieldColor<uint8_t>(lastFrameCPURegs_.P, frameReadState_.cpu.regs.P,
                                                    kClemensCPUStatus_Decimal),
                       "d");
    ImGui::SameLine(0.0f, 4.0f);
    ImGui::TextColored(getStatusFieldColor<uint8_t>(lastFrameCPURegs_.P, frameReadState_.cpu.regs.P,
                                                    kClemensCPUStatus_IRQDisable),
                       "i");
    ImGui::SameLine(0.0f, 4.0f);
    ImGui::TextColored(getStatusFieldColor<uint8_t>(lastFrameCPURegs_.P, frameReadState_.cpu.regs.P,
                                                    kClemensCPUStatus_Zero),
                       "z");
    ImGui::SameLine(0.0f, 4.0f);
    ImGui::TextColored(getStatusFieldColor<uint8_t>(lastFrameCPURegs_.P, frameReadState_.cpu.regs.P,
                                                    kClemensCPUStatus_Carry),
                       "c");
    ImGui::EndTable();

    ImGui::TableNextColumn();
    ImGui::BeginTable("Reg2Inner", 1);
    ImGui::TableNextColumn();
    ImGui::TextColored(CLEM_HOST_GUI_CPU_FIELD_COLOR(S), "S    =%04X", frameReadState_.cpu.regs.S);
    ImGui::TableNextRow();
    ImGui::TableNextColumn();
    ImGui::TextColored(CLEM_HOST_GUI_CPU_FIELD_COLOR(D), "D    =%04X", frameReadState_.cpu.regs.D);
    ImGui::TableNextRow();
    ImGui::TableNextColumn();
    ImGui::TextColored(CLEM_HOST_GUI_CPU_FIELD_COLOR(A), "A    =%04X  ",
                       frameReadState_.cpu.regs.A);
    ImGui::TableNextRow();
    ImGui::TableNextColumn();
    ImGui::TextColored(CLEM_HOST_GUI_CPU_FIELD_COLOR(X), "X    =%04X", frameReadState_.cpu.regs.X);
    ImGui::TableNextRow();
    ImGui::TableNextColumn();
    ImGui::TextColored(CLEM_HOST_GUI_CPU_FIELD_COLOR(Y), "Y    =%04X", frameReadState_.cpu.regs.Y);
    ImGui::EndTable();

    ImGui::TableNextColumn();
    ImGui::BeginTable("Pins", 1);
    ImGui::TableNextColumn();
    ImGui::TextColored(CLEM_HOST_GUI_CPU_PINS_COLOR(readyOut), "RDY");
    ImGui::TableNextRow();
    ImGui::TableNextColumn();
    ImGui::TextColored(CLEM_HOST_GUI_CPU_PINS_COLOR(resbIn), "RESB");
    ImGui::TableNextRow();
    ImGui::TableNextColumn();
    ImGui::TextColored(CLEM_HOST_GUI_CPU_PINS_COLOR(emulation), "E");
    ImGui::TableNextRow();
    ImGui::TableNextColumn();
    ImGui::TextColored(CLEM_HOST_GUI_CPU_PINS_INV_COLOR(irqbIn), "IRQ");
    ImGui::TableNextRow();
    ImGui::TableNextColumn();
    ImGui::TextColored(CLEM_HOST_GUI_CPU_PINS_INV_COLOR(nmibIn), "NMI");
    ImGui::EndTable();

    ImGui::EndTable();
}

void ClemensFrontend::doMachineDebugMemoryDisplay() {
    // float localContentWidth = ImGui::GetWindowContentRegionWidth();
    if (!frameReadState_.memoryView)
        return;
    uint8_t bank = frameReadState_.memoryViewBank;
    if (ImGui::InputScalar("Bank", ImGuiDataType_U8, &bank, NULL, NULL, "%02X",
                           ImGuiInputTextFlags_CharsHexadecimal)) {
        backend_->debugMemoryPage((uint8_t)(bank & 0xff));
    }
    debugMemoryEditor_.OptAddrDigitsCount = 4;
    debugMemoryEditor_.Cols = 8;
    debugMemoryEditor_.DrawContents(this, CLEM_IIGS_BANK_SIZE, (size_t)(bank) << 16);
}

void ClemensFrontend::doMachineDebugDOCDisplay() {

    auto &doc = frameReadState_.doc;

    ImGui::BeginTable("MMIO_Ensoniq_Global", 3);
    {
        ImGui::TableSetupColumn("OIR");
        ImGui::TableSetupColumn("OSC");
        ImGui::TableSetupColumn("ADC");
        ImGui::TableHeadersRow();
        ImGui::TableNextColumn();
        ImGui::Text("%c:%u", doc.reg[CLEM_ENSONIQ_REG_OSC_OIR] & 0x80 ? '-' : 'I',
                    (doc.reg[CLEM_ENSONIQ_REG_OSC_OIR] >> 1) & 0x1f);
        ImGui::TableNextColumn();
        ImGui::Text("%u + 1", (doc.reg[CLEM_ENSONIQ_REG_OSC_ENABLE] >> 1));
        ImGui::TableNextColumn();
        ImGui::Text("%02X", doc.reg[CLEM_ENSONIQ_REG_OSC_ADC]);
    }
    ImGui::EndTable();

    //  OSC 0, 1, ... N
    //  Per OSC: Control: Halt, Mode, Channel, IE, IRQ
    //           Data, ACC, PTR
    //
    auto contentAvail = ImGui::GetContentRegionAvail();
    auto fontCharSize = ImGui::GetFont()->GetCharAdvance('A');
    unsigned oscCount = (doc.reg[CLEM_ENSONIQ_REG_OSC_ENABLE] >> 1) + 1;
    ImGui::BeginTable("MMIO_Ensoniq_OSC", 10, ImGuiTableFlags_ScrollY, contentAvail);
    {
        ImGui::TableSetupColumn("OSC");
        ImGui::TableSetupColumn("IE");
        ImGui::TableSetupColumn("IR");
        ImGui::TableSetupColumn("M1");
        ImGui::TableSetupColumn("M0");
        ImGui::TableSetupColumn("CH");
        ImGui::TableSetupColumn("FC", ImGuiTableColumnFlags_WidthFixed, fontCharSize * 4);
        ImGui::TableSetupColumn("ACC", ImGuiTableColumnFlags_WidthFixed, fontCharSize * 6);
        ImGui::TableSetupColumn("TBL", ImGuiTableColumnFlags_WidthFixed, fontCharSize * 4);
        ImGui::TableSetupColumn("PTR", ImGuiTableColumnFlags_WidthFixed, fontCharSize * 4);
        ImGui::TableHeadersRow();
        ImColor oscActiveColor(0, 255, 255);
        ImColor oscHalted(64, 64, 64);
        for (unsigned oscIndex = 0; oscIndex < oscCount; ++oscIndex) {
            auto ctl = doc.reg[CLEM_ENSONIQ_REG_OSC_CTRL + oscIndex];
            uint16_t fc = ((uint16_t)doc.reg[CLEM_ENSONIQ_REG_OSC_FCHI + oscIndex] << 8) |
                          doc.reg[CLEM_ENSONIQ_REG_OSC_FCLOW + oscIndex];
            auto flags = doc.osc_flags[oscIndex];
            const ImColor &col = (ctl & CLEM_ENSONIQ_OSC_CTL_HALT) ? oscHalted : oscActiveColor;
            ImGui::TableNextColumn();
            ImGui::TextColored(col, "%u", oscIndex);
            ImGui::TableNextColumn();
            ImGui::TextColored(col, "%c", (ctl & CLEM_ENSONIQ_OSC_CTL_IE) ? '1' : '0');
            ImGui::TableNextColumn();
            ImGui::TextColored(col, "%c", (flags & CLEM_ENSONIQ_OSC_FLAG_IRQ) ? 'I' : ' ');
            ImGui::TableNextColumn();
            ImGui::TextColored(col, "%c", (ctl & CLEM_ENSONIQ_OSC_CTL_SYNC) ? '1' : '0');
            ImGui::TableNextColumn();
            ImGui::TextColored(col, "%c", (ctl & CLEM_ENSONIQ_OSC_CTL_M0) ? '1' : '0');
            ImGui::TableNextColumn();
            ImGui::TextColored(col, "%u", (ctl >> 4));
            ImGui::TableNextColumn();
            ImGui::TextColored(col, "%04X", fc);
            ImGui::TableNextColumn();
            ImGui::TextColored(col, "%06X", doc.acc[oscIndex] & 0x00ffffff);
            ImGui::TableNextColumn();
            ImGui::TextColored(col, "%04X",
                               (uint16_t)doc.reg[CLEM_ENSONIQ_REG_OSC_PTR + oscIndex] << 8);
            ImGui::TableNextColumn();
            ImGui::TextColored(col, "%04X", doc.ptr[oscIndex]);
            ImGui::TableNextRow();
        }
    }
    ImGui::EndTable();
}

ImU8 ClemensFrontend::imguiMemoryEditorRead(const ImU8 *mem_ptr, size_t off) {
    const auto *self = reinterpret_cast<const ClemensFrontend *>(mem_ptr);
    if (!self->frameReadState_.memoryView)
        return 0x00;
    return self->frameReadState_.memoryView[off & 0xffff];
}

void ClemensFrontend::imguiMemoryEditorWrite(ImU8 *mem_ptr, size_t off, ImU8 value) {
    auto *self = reinterpret_cast<ClemensFrontend *>(mem_ptr);
    self->backend_->debugMemoryWrite((uint16_t)(off & 0xffff), value);
}

static void doMachineDebugIORegister(uint8_t *ioregsold, uint8_t *ioregs, uint8_t reg) {
    auto &desc = sDebugIODescriptors[reg];
    bool changed = ioregsold[reg] != ioregs[reg];
    bool tooltip = false;
    ImGui::TableNextColumn();
    ImGui::PushStyleColor(ImGuiCol_Text,
                          (ImU32)(changed ? getModifiedColor(true) : getDefaultColor(true)));
    ImGui::TextUnformatted(desc.readLabel);
    ImGui::PopStyleColor();
    tooltip = tooltip || ImGui::IsItemHovered();
    ImGui::TableNextColumn();
    ImGui::TextColored(changed ? getModifiedColor(true) : getDefaultColor(true), "%04X",
                       0xc000 + reg);
    tooltip = tooltip || ImGui::IsItemHovered();
    ImGui::TableNextColumn();
    ImGui::TextColored(changed ? getModifiedColor(true) : getDefaultColor(true), "%02X",
                       ioregs[reg]);
    tooltip = tooltip || ImGui::IsItemHovered();
    if (tooltip) {
        ImGui::BeginTooltip();
        ImGui::PushTextWrapPos(ImGui::GetFontSize() * 50.0f);
        ImGui::Text("%04X - %s: %s", desc.reg + 0xc000, desc.readLabel, desc.readDesc);
        ImGui::PopTextWrapPos();
        ImGui::EndTooltip();
    }
}

void ClemensFrontend::doMachineDebugCoreIODisplay() {
    auto *ioregs = frameReadState_.ioPage;
    if (!ioregs)
        return;

    ImGui::BeginTable("IODEBUG", 2);
    ImGui::TableSetupColumn("Col1");
    ImGui::TableSetupColumn("Col2");
    ImGui::TableNextRow();

    ImGui::TableNextColumn();
    ImGui::BeginTable("IOREGS", 3);
    ImGui::TableSetupColumn("Symbol", ImGuiTableColumnFlags_WidthFixed,
                            ImGui::GetFont()->GetCharAdvance('A') * 9);
    ImGui::TableSetupColumn("Addr", ImGuiTableColumnFlags_WidthFixed,
                            ImGui::GetFont()->GetCharAdvance('0') * 4);
    ImGui::TableSetupColumn("Data", ImGuiTableColumnFlags_WidthFixed);
    ImGui::TableHeadersRow();
    doMachineDebugIORegister(lastFrameIORegs_, ioregs, CLEM_MMIO_REG_KEYB_READ);
    ImGui::TableNextRow();
    doMachineDebugIORegister(lastFrameIORegs_, ioregs, CLEM_MMIO_REG_NEWVIDEO);
    ImGui::TableNextRow();
    doMachineDebugIORegister(lastFrameIORegs_, ioregs, CLEM_MMIO_REG_SHADOW);
    ImGui::TableNextRow();
    doMachineDebugIORegister(lastFrameIORegs_, ioregs, CLEM_MMIO_REG_SPEED);
    ImGui::TableNextRow();
    doMachineDebugIORegister(lastFrameIORegs_, ioregs, CLEM_MMIO_REG_STATEREG);
    ImGui::TableNextRow();
    doMachineDebugIORegister(lastFrameIORegs_, ioregs, CLEM_MMIO_REG_SLOTROMSEL);
    ImGui::TableNextRow();
    doMachineDebugIORegister(lastFrameIORegs_, ioregs, CLEM_MMIO_REG_READCXROM);
    ImGui::EndTable();
    ImGui::TableNextColumn();
    ImGui::BeginTable("IOREGS", 3);
    ImGui::TableSetupColumn("Symbol", ImGuiTableColumnFlags_WidthFixed,
                            ImGui::GetFont()->GetCharAdvance('A') * 9);
    ImGui::TableSetupColumn("Addr", ImGuiTableColumnFlags_WidthFixed,
                            ImGui::GetFont()->GetCharAdvance('0') * 4);
    ImGui::TableSetupColumn("Data", ImGuiTableColumnFlags_WidthFixed);
    ImGui::TableHeadersRow();
    doMachineDebugIORegister(lastFrameIORegs_, ioregs, CLEM_MMIO_REG_LC_BANK_TEST);
    ImGui::TableNextRow();
    doMachineDebugIORegister(lastFrameIORegs_, ioregs, CLEM_MMIO_REG_ROM_RAM_TEST);
    ImGui::TableNextRow();
    doMachineDebugIORegister(lastFrameIORegs_, ioregs, CLEM_MMIO_REG_RAMRD_TEST);
    ImGui::TableNextRow();
    doMachineDebugIORegister(lastFrameIORegs_, ioregs, CLEM_MMIO_REG_RAMWRT_TEST);
    ImGui::TableNextRow();
    doMachineDebugIORegister(lastFrameIORegs_, ioregs, CLEM_MMIO_REG_RDALTZP_TEST);
    ImGui::TableNextRow();
    doMachineDebugIORegister(lastFrameIORegs_, ioregs, CLEM_MMIO_REG_80COLSTORE_TEST);
    ImGui::TableNextRow();
    doMachineDebugIORegister(lastFrameIORegs_, ioregs, CLEM_MMIO_REG_READC3ROM);
    ImGui::EndTable();

    ImGui::EndTable();
}

void ClemensFrontend::doMachineDebugVideoIODisplay() {
    auto *ioregs = frameReadState_.ioPage;
    if (!ioregs)
        return;

    ImGui::BeginTable("IODEBUG", 2);
    ImGui::TableSetupColumn("Col1");
    ImGui::TableSetupColumn("Col2");
    ImGui::TableNextRow();

    ImGui::TableNextColumn();
    ImGui::BeginTable("IOREGS", 3);
    ImGui::TableSetupColumn("Symbol", ImGuiTableColumnFlags_WidthFixed,
                            ImGui::GetFont()->GetCharAdvance('A') * 9);
    ImGui::TableSetupColumn("Addr", ImGuiTableColumnFlags_WidthFixed,
                            ImGui::GetFont()->GetCharAdvance('0') * 4);
    ImGui::TableSetupColumn("Data", ImGuiTableColumnFlags_WidthFixed);
    ImGui::TableHeadersRow();
    doMachineDebugIORegister(lastFrameIORegs_, ioregs, CLEM_MMIO_REG_NEWVIDEO);
    ImGui::TableNextRow();
    doMachineDebugIORegister(lastFrameIORegs_, ioregs, CLEM_MMIO_REG_VGC_MONO);
    ImGui::TableNextRow();
    doMachineDebugIORegister(lastFrameIORegs_, ioregs, CLEM_MMIO_REG_VGC_TEXT_COLOR);
    ImGui::TableNextRow();
    doMachineDebugIORegister(lastFrameIORegs_, ioregs, CLEM_MMIO_REG_VGC_IRQ_BYTE);
    ImGui::TableNextRow();
    doMachineDebugIORegister(lastFrameIORegs_, ioregs, CLEM_MMIO_REG_VGC_VERTCNT);
    ImGui::TableNextRow();
    doMachineDebugIORegister(lastFrameIORegs_, ioregs, CLEM_MMIO_REG_VGC_HORIZCNT);
    ImGui::TableNextRow();
    doMachineDebugIORegister(lastFrameIORegs_, ioregs, CLEM_MMIO_REG_80COLUMN_TEST);
    ImGui::EndTable();
    ImGui::TableNextColumn();
    ImGui::BeginTable("IOREGS", 3);
    ImGui::TableSetupColumn("Symbol", ImGuiTableColumnFlags_WidthFixed,
                            ImGui::GetFont()->GetCharAdvance('A') * 10);
    ImGui::TableSetupColumn("Addr", ImGuiTableColumnFlags_WidthFixed,
                            ImGui::GetFont()->GetCharAdvance('0') * 4);
    ImGui::TableSetupColumn("Data", ImGuiTableColumnFlags_WidthFixed);
    ImGui::TableHeadersRow();
    doMachineDebugIORegister(lastFrameIORegs_, ioregs, CLEM_MMIO_REG_VBLBAR);
    ImGui::TableNextRow();
    doMachineDebugIORegister(lastFrameIORegs_, ioregs, CLEM_MMIO_REG_ALTCHARSET_TEST);
    ImGui::TableNextRow();
    doMachineDebugIORegister(lastFrameIORegs_, ioregs, CLEM_MMIO_REG_TXT_TEST);
    ImGui::TableNextRow();
    doMachineDebugIORegister(lastFrameIORegs_, ioregs, CLEM_MMIO_REG_MIXED_TEST);
    ImGui::TableNextRow();
    doMachineDebugIORegister(lastFrameIORegs_, ioregs, CLEM_MMIO_REG_TXTPAGE2_TEST);
    ImGui::TableNextRow();
    doMachineDebugIORegister(lastFrameIORegs_, ioregs, CLEM_MMIO_REG_HIRES_TEST);
    ImGui::TableNextRow();
    doMachineDebugIORegister(lastFrameIORegs_, ioregs, CLEM_MMIO_REG_LANGSEL);
    ImGui::EndTable();

    ImGui::EndTable();
}

#define CLEM_HOST_GUI_IWM_STATUS_CHANGED(_flag_)                                                   \
    ((lastFrameIWM_.status & (_flag_)) != (frameReadState_.iwm.status & (_flag_)))

void ClemensFrontend::doMachineDebugDiskIODisplay() {
    auto *ioregs = frameReadState_.ioPage;
    auto &iwmState = frameReadState_.iwm;

    if (!ioregs)
        return;

    bool driveOn = (iwmState.status & kIWMStatusDriveOn);
    bool driveSpin = (iwmState.status & kIWMStatusDriveSpin);
    bool drive35 = (iwmState.status & kIWMStatusDrive35);
    ImColor fieldColor;

    //  IWM (Column 1)
    //    DiskType
    //    Disk Drive
    //    Q6
    //    Q7
    //    Read from IWM
    //    Data register
    //    Latch register
    float fontCharSize = ImGui::GetFont()->GetCharAdvance('A');
    ImGui::BeginTable("DISKDEBUG", 2);
    ImGui::TableSetupColumn("Col1", ImGuiTableColumnFlags_WidthFixed, fontCharSize * 15);
    ImGui::TableSetupColumn("Col2", ImGuiTableColumnFlags_WidthStretch);
    ImGui::TableNextRow();
    ImGui::TableNextColumn();
    ImGui::BeginTable("PARAMS", 2, ImGuiTableFlags_SizingFixedFit);
    ImGui::TableSetupColumn("Name", ImGuiTableColumnFlags_WidthFixed, fontCharSize * 8);
    ImGui::TableSetupColumn("Value", ImGuiTableColumnFlags_WidthFixed, fontCharSize * 6);
    ImGui::TableHeadersRow();
    ImGui::TableNextColumn();
    fieldColor = CLEM_HOST_GUI_IWM_STATUS_CHANGED(kIWMStatusDrive35) ? getModifiedColor(driveOn)
                                                                     : getDefaultColor(driveOn);
    ImGui::PushStyleColor(ImGuiCol_Text, (ImU32)fieldColor);
    ImGui::TextUnformatted("Disk");
    ImGui::TableNextColumn();
    if (driveOn) {
        if (drive35) {
            ImGui::TextUnformatted("3.5");
        } else {
            ImGui::TextUnformatted("5.25");
        }
    } else {
        ImGui::TextUnformatted("None");
    }
    ImGui::PopStyleColor();
    ImGui::TableNextRow();
    ImGui::TableNextColumn();
    fieldColor = CLEM_HOST_GUI_IWM_STATUS_CHANGED(kIWMStatusDriveAlt) ? getModifiedColor(driveOn)
                                                                      : getDefaultColor(driveOn);
    ImGui::PushStyleColor(ImGuiCol_Text, (ImU32)fieldColor);
    ImGui::TextUnformatted("Drive");
    ImGui::TableNextColumn();
    if (driveOn) {
        if (iwmState.status & kIWMStatusDriveAlt) {
            ImGui::TextUnformatted("2");
        } else {
            ImGui::TextUnformatted("1");
        }
    }
    ImGui::PopStyleColor();
    ImGui::TableNextRow();
    ImGui::TableNextColumn();
    fieldColor = CLEM_HOST_GUI_IWM_STATUS_CHANGED(kIWMStatusIWMQ6) ? getModifiedColor(true)
                                                                   : getDefaultColor(true);
    ImGui::TextColored(fieldColor, "Q6");
    ImGui::TableNextColumn();
    ImGui::TextColored(fieldColor, "%d", (iwmState.status & kIWMStatusIWMQ6) ? 1 : 0);
    ImGui::TableNextRow();
    ImGui::TableNextColumn();
    fieldColor = CLEM_HOST_GUI_IWM_STATUS_CHANGED(kIWMStatusIWMQ7) ? getModifiedColor(true)
                                                                   : getDefaultColor(true);
    ImGui::TextColored(fieldColor, "Q7");
    ImGui::TableNextColumn();
    ImGui::TextColored(fieldColor, "%d", (iwmState.status & kIWMStatusIWMQ7) ? 1 : 0);
    ImGui::TableNextRow();
    fieldColor = lastFrameIORegs_[CLEM_MMIO_REG_IWM_Q6_LO] != ioregs[CLEM_MMIO_REG_IWM_Q6_LO]
                     ? getModifiedColor(true)
                     : getDefaultColor(true);
    ImGui::TableNextColumn();
    ImGui::TextColored(fieldColor, "Read");
    ImGui::TableNextColumn();
    ImGui::TextColored(fieldColor, "%02X", ioregs[CLEM_MMIO_REG_IWM_Q6_LO]);
    ImGui::TableNextRow();
    fieldColor =
        lastFrameIWM_.data != iwmState.data ? getModifiedColor(driveOn) : getDefaultColor(driveOn);
    ImGui::TableNextColumn();
    ImGui::TextColored(fieldColor, "Data");
    ImGui::TableNextColumn();
    ImGui::TextColored(fieldColor, "%02X", iwmState.data);
    ImGui::TableNextRow();
    fieldColor = lastFrameIWM_.latch != iwmState.latch ? getModifiedColor(driveOn)
                                                       : getDefaultColor(driveOn);
    ImGui::TableNextColumn();
    ImGui::TextColored(fieldColor, "Latch");
    ImGui::TableNextColumn();
    ImGui::TextColored(fieldColor, "%02X", iwmState.latch);
    ImGui::EndTable();

    //  DRIVE (Column 2)
    //    Motor
    //    Phase 0-3
    //    HeadSel
    //    WriteProtect
    //    TrackIndex / TrackBitCount
    //    Buffer
    //    HeadPos

    ImGui::TableNextColumn();
    ImGui::BeginTable("PARAMS", 2, ImGuiTableFlags_SizingFixedFit);
    ImGui::TableSetupColumn("Name", ImGuiTableColumnFlags_WidthFixed, fontCharSize * 8);
    ImGui::TableSetupColumn("Value", ImGuiTableColumnFlags_WidthFixed, fontCharSize * 16);
    ImGui::TableHeadersRow();
    ImGui::TableNextColumn();
    fieldColor = CLEM_HOST_GUI_IWM_STATUS_CHANGED(kIWMStatusDriveSpin) ? getModifiedColor(driveOn)
                                                                       : getDefaultColor(driveOn);
    ImGui::TextColored(fieldColor, "Motor");
    ImGui::TableNextColumn();
    ImGui::TextColored(fieldColor, "%s", (iwmState.status & kIWMStatusDriveSpin) ? "on" : "off");
    ImGui::TableNextRow();
    ImGui::TableNextColumn();
    fieldColor =
        lastFrameIWM_.ph03 != iwmState.ph03 ? getModifiedColor(driveOn) : getDefaultColor(driveOn);
    ImGui::PushStyleColor(ImGuiCol_Text, (ImU32)fieldColor);
    ImGui::TextUnformatted("Phase");
    ImGui::TableNextColumn();
    ImGui::Text("%u%u%u%u", (iwmState.ph03 & 1) ? 1 : 0, (iwmState.ph03 & 2) ? 1 : 0,
                (iwmState.ph03 & 4) ? 1 : 0, (iwmState.ph03 & 8) ? 1 : 0);
    ImGui::PopStyleColor();
    ImGui::TableNextRow();
    fieldColor = CLEM_HOST_GUI_IWM_STATUS_CHANGED(kIWMStatusDriveSel) ? getModifiedColor(driveOn)
                                                                      : getDefaultColor(driveOn);
    ImGui::PushStyleColor(ImGuiCol_Text, (ImU32)fieldColor);
    ImGui::TableNextColumn();
    ImGui::TextUnformatted("HeadSel");
    ImGui::TableNextColumn();
    ImGui::Text("%d", iwmState.status & kIWMStatusDriveSel ? 1 : 0);
    ImGui::PopStyleColor();
    ImGui::TableNextRow();
    fieldColor = CLEM_HOST_GUI_IWM_STATUS_CHANGED(kIWMStatusDriveWP) ? getModifiedColor(driveOn)
                                                                     : getDefaultColor(driveOn);
    ImGui::PushStyleColor(ImGuiCol_Text, (ImU32)fieldColor);
    ImGui::TableNextColumn();
    ImGui::TextUnformatted("Sense");
    ImGui::TableNextColumn();
    ImGui::Text("%d", iwmState.status & kIWMStatusDriveWP ? 1 : 0);
    ImGui::PopStyleColor();
    ImGui::TableNextRow();
    fieldColor = lastFrameIWM_.qtr_track_index != iwmState.qtr_track_index
                     ? getModifiedColor(driveOn)
                     : getDefaultColor(driveOn);
    ImGui::PushStyleColor(ImGuiCol_Text, (ImU32)fieldColor);
    ImGui::TableNextColumn();
    ImGui::Text("Track");
    ImGui::TableNextColumn();
    if (driveOn) {
        if (drive35) {
            ImGui::Text("%02d S%d - %05d", iwmState.qtr_track_index / 2,
                        iwmState.qtr_track_index % 2, iwmState.track_bit_length);
        } else {
            ImGui::Text("%02d +%d - %05d", iwmState.qtr_track_index / 4,
                        iwmState.qtr_track_index % 4, iwmState.track_bit_length);
        }
    } else {
        ImGui::TextUnformatted("");
    }
    ImGui::PopStyleColor();
    ImGui::TableNextRow();
    ImGui::PushStyleColor(ImGuiCol_Text, (ImU32)fieldColor);
    ImGui::TableNextColumn();
    ImGui::TextUnformatted("Head");
    ImGui::TableNextColumn();
    if (driveSpin) {
        ImGui::Text("%05d[%d]", iwmState.track_byte_index, iwmState.track_bit_shift);
    }
    ImGui::PopStyleColor();
    ImGui::TableNextRow();
    fieldColor = ((lastFrameIWM_.track_byte_index != iwmState.track_byte_index) ||
                  (lastFrameIWM_.track_bit_shift != iwmState.track_bit_shift))
                     ? getModifiedColor(driveOn)
                     : getDefaultColor(driveOn);
    ImGui::PushStyleColor(ImGuiCol_Text, (ImU32)fieldColor);
    ImGui::TableNextColumn();
    ImGui::TextUnformatted("Bytes");
    ImGui::TableNextColumn();
    if (driveSpin) {
        ImGui::Text("%02X [%02X] %02X %02X", iwmState.buffer[0], iwmState.buffer[1],
                    iwmState.buffer[2], iwmState.buffer[3]);
    }
    ImGui::PopStyleColor();
    ImGui::EndTable();

    ImGui::EndTable();
}

void ClemensFrontend::doMachineDebugADBDisplay() {
    auto *ioregs = frameReadState_.ioPage;
    // auto& iwmState = frameReadState_.iwm;

    if (!ioregs)
        return;

    auto fontCharSize = ImGui::GetFont()->GetCharAdvance('A');

    ImGui::BeginTable("IODEBUG", 2);
    ImGui::TableSetupColumn("Col1");
    ImGui::TableSetupColumn("Col2");
    ImGui::TableNextRow();

    ImGui::TableNextColumn();
    ImGui::BeginTable("IOREGS", 3);
    ImGui::TableSetupColumn("Symbol", ImGuiTableColumnFlags_WidthFixed, fontCharSize * 9);
    ImGui::TableSetupColumn("Addr", ImGuiTableColumnFlags_WidthFixed, fontCharSize * 4);
    ImGui::TableSetupColumn("Data", ImGuiTableColumnFlags_WidthFixed);
    ImGui::TableHeadersRow();
    doMachineDebugIORegister(lastFrameIORegs_, ioregs, CLEM_MMIO_REG_KEYB_READ);
    ImGui::TableNextRow();
    doMachineDebugIORegister(lastFrameIORegs_, ioregs, CLEM_MMIO_REG_ANYKEY_STROBE);
    ImGui::TableNextRow();
    doMachineDebugIORegister(lastFrameIORegs_, ioregs, CLEM_MMIO_REG_ADB_MOUSE_DATA);
    ImGui::TableNextRow();
    doMachineDebugIORegister(lastFrameIORegs_, ioregs, CLEM_MMIO_REG_ADB_MODKEY);
    ImGui::TableNextRow();
    doMachineDebugIORegister(lastFrameIORegs_, ioregs, CLEM_MMIO_REG_ADB_STATUS);
    ImGui::TableNextRow();
    ImGui::TableNextColumn();
    ImGui::TextUnformatted(" ");
    ImGui::TableNextRow();
    ImGui::TableNextColumn();
    ImGui::TextUnformatted(" ");
    ImGui::EndTable();

    ImGui::TableNextColumn();
    ImGui::BeginTable("PARAMS", 2, ImGuiTableFlags_SizingFixedFit);
    ImGui::TableSetupColumn("Name", ImGuiTableColumnFlags_WidthFixed, fontCharSize * 8);
    ImGui::TableSetupColumn("Value", ImGuiTableColumnFlags_WidthFixed, fontCharSize * 6);
    ImGui::TableHeadersRow();
    ImGui::EndTable();

    ImGui::EndTable();
}

void ClemensFrontend::doMachineDebugSoundDisplay() {
    auto *ioregs = frameReadState_.ioPage;
    // auto& iwmState = frameReadState_.iwm;

    if (!ioregs)
        return;

    auto fontCharSize = ImGui::GetFont()->GetCharAdvance('A');

    ImGui::BeginTable("IODEBUG", 2);
    ImGui::TableSetupColumn("Col1");
    ImGui::TableSetupColumn("Col2");
    ImGui::TableNextRow();

    ImGui::TableNextColumn();
    ImGui::BeginTable("IOREGS", 3);
    ImGui::TableSetupColumn("Symbol", ImGuiTableColumnFlags_WidthFixed, fontCharSize * 9);
    ImGui::TableSetupColumn("Addr", ImGuiTableColumnFlags_WidthFixed, fontCharSize * 4);
    ImGui::TableSetupColumn("Data", ImGuiTableColumnFlags_WidthFixed);
    ImGui::TableHeadersRow();
    doMachineDebugIORegister(lastFrameIORegs_, ioregs, CLEM_MMIO_REG_AUDIO_CTL);
    ImGui::TableNextRow();
    doMachineDebugIORegister(lastFrameIORegs_, ioregs, CLEM_MMIO_REG_AUDIO_DATA);
    ImGui::TableNextRow();
    doMachineDebugIORegister(lastFrameIORegs_, ioregs, CLEM_MMIO_REG_AUDIO_ADRLO);
    ImGui::TableNextRow();
    doMachineDebugIORegister(lastFrameIORegs_, ioregs, CLEM_MMIO_REG_AUDIO_ADRHI);
    ImGui::TableNextRow();
    ImGui::TextUnformatted(" ");
    ImGui::TableNextRow();
    ImGui::TableNextColumn();
    ImGui::TextUnformatted(" ");
    ImGui::TableNextRow();
    ImGui::TableNextColumn();
    ImGui::TextUnformatted(" ");
    ImGui::EndTable();

    ImGui::TableNextColumn();
    ImGui::BeginTable("PARAMS", 2, ImGuiTableFlags_SizingFixedFit);
    ImGui::TableSetupColumn("Name", ImGuiTableColumnFlags_WidthFixed, fontCharSize * 8);
    ImGui::TableSetupColumn("Value", ImGuiTableColumnFlags_WidthFixed, fontCharSize * 6);
    ImGui::TableHeadersRow();
    ImGui::EndTable();

    ImGui::EndTable();
}

void ClemensFrontend::doMachineViewLayout(ImVec2 rootAnchor, ImVec2 rootSize, float screenU,
                                          float screenV) {
    ImGui::SetNextWindowPos(rootAnchor);
    ImGui::SetNextWindowSize(rootSize);
    ImGui::Begin("Display", nullptr,
                 ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize |
                     ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoBringToFrontOnFocus |
                     ImGuiWindowFlags_NoMove);
    ImVec4 tint_col = ImVec4(1.0f, 1.0f, 1.0f, 1.0f); // No tint
    ImTextureID texId{(void *)((uintptr_t)display_.getScreenTarget().id)};
    ImVec2 p = ImGui::GetCursorScreenPos();
    ImVec2 contentSize = ImGui::GetContentRegionAvail();
    ImVec2 monitorSize(contentSize.y * 1.5f, contentSize.y);
    ImVec2 monitorAnchor(p.x + (contentSize.x - monitorSize.x) * 0.5f, p.y);
    float screenV0 = 0.0f, screenV1 = screenV;
#if defined(CK3D_BACKEND_GL)
    // Flip the texture coords so that the top V is 1.0f
    screenV0 = 1.0f;
    screenV1 = 1.0f - screenV1;
#endif
    ImGui::GetWindowDrawList()->AddImage(
        texId, ImVec2(monitorAnchor.x, monitorAnchor.y),
        ImVec2(monitorAnchor.x + monitorSize.x, monitorAnchor.y + monitorSize.y),
        ImVec2(0, screenV0), ImVec2(screenU, screenV1), ImGui::GetColorU32(tint_col));

    if (ImGui::IsWindowFocused()) {
        ImGui::SetKeyboardFocusHere(0);
        emulatorHasKeyboardFocus_ = true;
    } else {
        emulatorHasKeyboardFocus_ = false;
    }
    if (ImGui::IsMouseClicked(ImGuiMouseButton_Left) && !emulatorHasMouseFocus_) {
        emulatorHasMouseFocus_ = ImGui::IsWindowHovered();
    }

    ImGui::End();
}

void ClemensFrontend::doMachineTerminalLayout(ImVec2 rootAnchor, ImVec2 rootSize) {
    char inputLine[128] = "";
    ImGui::SetNextWindowPos(rootAnchor);
    ImGui::SetNextWindowSize(rootSize);
    ImGui::Begin("Terminal", nullptr,
                 ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize |
                     ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoMove);
    ImVec2 contentSize = ImGui::GetContentRegionAvail();
    contentSize.y -= (ImGui::GetFrameHeightWithSpacing() + ImGui::GetStyle().ItemSpacing.y);
    ImGui::BeginChild("TerminalView", contentSize);
    {
        if (terminalMode_ == TerminalMode::Command) {
            layoutTerminalLines();
        } else if (terminalMode_ == TerminalMode::Log) {
            layoutConsoleLines();
        }
    }
    ImGui::EndChild();
    ImGui::Separator();
    ImGui::PushStyleColor(ImGuiCol_FrameBg, ImGui::GetStyleColorVec4(ImGuiCol_WindowBg));
    ImGui::Text("*");
    ImGui::SameLine();
    auto &style = ImGui::GetStyle();
    float xpos = ImGui::GetCursorPosX();
    float rightPaddingX = 3 * (ImGui::GetFont()->GetCharAdvance('A') + style.FramePadding.x * 2 +
                               style.ItemSpacing.x);
    ImGui::SetNextItemWidth(rootSize.x - xpos - ImGui::GetStyle().WindowPadding.x - rightPaddingX);
    if (ImGui::InputText("##", inputLine, sizeof(inputLine),
                         ImGuiInputTextFlags_EnterReturnsTrue)) {
        auto *inputLineEnd = &inputLine[0] + strnlen(inputLine, sizeof(inputLine));
        if (inputLineEnd != &inputLine[0]) {
            const char *inputStart = &inputLine[0];
            const char *inputEnd = inputLineEnd - 1;
            for (; std::isspace(*inputStart) && inputStart < inputEnd; ++inputStart)
                ;
            for (; std::isspace(*inputEnd) && inputEnd > inputStart; --inputEnd)
                ;
            if (inputStart <= inputEnd) {
                std::string_view commandLine(inputStart, (inputEnd - inputStart) + 1);
                executeCommand(commandLine);
            }
        }
        ImGui::SetKeyboardFocusHere(-1);
    }
    ImGui::PopStyleColor();
    ImGui::SameLine();
    auto activeButtonColor = style.Colors[ImGuiCol_ButtonActive];
    auto buttonColor = style.Colors[ImGuiCol_Button];
    ImGui::PushStyleColor(ImGuiCol_Button,
                          terminalMode_ == TerminalMode::Command ? activeButtonColor : buttonColor);
    if (ImGui::Button("C")) {
        terminalMode_ = TerminalMode::Command;
    }
    ImGui::PopStyleColor();
    ImGui::SameLine();
    ImGui::PushStyleColor(ImGuiCol_Button,
                          terminalMode_ == TerminalMode::Log ? activeButtonColor : buttonColor);
    if (ImGui::Button("L")) {
        terminalMode_ = TerminalMode::Log;
    }
    ImGui::PopStyleColor();
    ImGui::SameLine();
    ImGui::PushStyleColor(ImGuiCol_Button, terminalMode_ == TerminalMode::Execution
                                               ? activeButtonColor
                                               : buttonColor);
    if (ImGui::Button("E")) {
        terminalMode_ = TerminalMode::Execution;
    }
    ImGui::PopStyleColor();
    ImGui::End();
}

void ClemensFrontend::layoutTerminalLines() {
    for (size_t index = 0; index < terminalLines_.size(); ++index) {
        auto &line = terminalLines_.at(index);
        switch (line.type) {
        case TerminalLine::Debug:
            ImGui::PushStyleColor(ImGuiCol_Text, (ImU32)ImColor(192, 192, 192, 255));
            break;
        case TerminalLine::Warn:
            ImGui::PushStyleColor(ImGuiCol_Text, (ImU32)ImColor(255, 255, 0, 255));
            break;
        case TerminalLine::Error:
            ImGui::PushStyleColor(ImGuiCol_Text, (ImU32)ImColor(255, 0, 192, 255));
            break;
        case TerminalLine::Command:
            ImGui::PushStyleColor(ImGuiCol_Text, (ImU32)ImColor(0, 255, 255, 255));
            break;
        case TerminalLine::Opcode:
            ImGui::PushStyleColor(ImGuiCol_Text, (ImU32)ImColor(0, 255, 0, 255));
            break;
        default:
            break;
        }
        ImGui::TextUnformatted(terminalLines_.at(index).text.c_str());
        if (line.type != TerminalLine::Info) {
            ImGui::PopStyleColor();
        }
    }
    if (terminalChanged_) {
        ImGui::SetScrollHereY();
        terminalChanged_ = false;
    }
}

void ClemensFrontend::layoutConsoleLines() {
    for (size_t index = 0; index < consoleLines_.size(); ++index) {
        auto &line = consoleLines_.at(index);
        switch (line.type) {
        case TerminalLine::Debug:
            ImGui::PushStyleColor(ImGuiCol_Text, (ImU32)ImColor(192, 192, 192, 255));
            break;
        case TerminalLine::Warn:
            ImGui::PushStyleColor(ImGuiCol_Text, (ImU32)ImColor(255, 255, 0, 255));
            break;
        case TerminalLine::Error:
            ImGui::PushStyleColor(ImGuiCol_Text, (ImU32)ImColor(255, 0, 192, 255));
            break;
        case TerminalLine::Command:
            ImGui::PushStyleColor(ImGuiCol_Text, (ImU32)ImColor(0, 255, 255, 255));
            break;
        default:
            break;
        }
        ImGui::TextUnformatted(consoleLines_.at(index).text.c_str());
        if (line.type != TerminalLine::Info) {
            ImGui::PopStyleColor();
        }
    }
    if (consoleChanged_) {
        ImGui::SetScrollHereY();
        consoleChanged_ = false;
    }
}

void ClemensFrontend::doModalOperations(int width, int height) {
    bool newOperation = false;
    if (!ImGuiFileDialog::Instance()->WasOpenedThisFrame()) {
        if (guiMode_ == GUIMode::ImportDiskModal) {
            //  File GUI dialog
            const char *filters =
                "Disk image files (*.dsk *.do *.po *.2mg *.woz){.dsk,.do,.po,.2mg,.woz}";

            ImGuiFileDialog::Instance()->OpenDialog(
                "choose_disk_images", "Choose Disk Image", filters, ".", "", 16,
                (void *)((intptr_t)importDriveType_), ImGuiFileDialogFlags_Modal);
        }
    }
    if (ImGuiFileDialog::Instance()->Display("choose_disk_images", ImGuiWindowFlags_NoCollapse,
                                             ImVec2(1024, 512), ImVec2(width, height))) {
        if (ImGuiFileDialog::Instance()->IsOk()) {
            auto selection = ImGuiFileDialog::Instance()->GetSelection();
            auto filepath = ImGuiFileDialog::Instance()->GetCurrentPath();
            // auto driveType = (ClemensDriveType)(reinterpret_cast<intptr_t>(
            //     ImGuiFileDialog::Instance()->GetUserDatas()));
            importDiskFiles_.clear();
            for (auto &e : selection) {
                importDiskFiles_.emplace_back(e.second);
            }
            guiMode_ = GUIMode::ImportDiskSetFlow;
            ImGui::OpenPopup("Import Master Disk Set");
            newOperation = true;
        } else {
            guiMode_ = GUIMode::Emulator;
        }

        ImGuiFileDialog::Instance()->Close();
    }
    if (!ImGui::IsPopupOpen("Enter Disk Set Name")) {
        if (guiMode_ == GUIMode::BlankDiskModal) {
            ImGui::OpenPopup("Enter Disk Set Name");
            newOperation = true;
        }
    }
    if (newOperation) {
        importDiskSetPath_.clear();
        importDiskSetName_.clear();
    }
    ImVec2 center = ImGui::GetMainViewport()->GetCenter();
    ImGui::SetNextWindowPos(center, ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
    ImGui::SetNextWindowSize(
        ImVec2(std::max(720.0f, width * 0.33f), 7 * ImGui::GetTextLineHeightWithSpacing()));
    if (ImGui::BeginPopupModal("Enter Disk Set Name", NULL, 0)) {
        ImGui::Spacing();
        char collectionName[64]{};
        char imageName[64]{};

        //  TODO: seems the user has to enter text and press enter always to confirm
        //        adding an OK button seems that the right thing to do, but there's
        //        AFAIK no way to force InputText() to return true without some
        //       'next frame return true' hack flag.  Investigate but no need to
        //        waste cycles on figuring out this minor UI convenience for now.
        ImGui::BeginTable("Disk Label Entry", 2);
        ImGui::TableSetupColumn("", ImGuiTableColumnFlags_WidthFixed,
                                ImGui::CalcTextSize("Disk Name").x +
                                    ImGui::GetStyle().ColumnsMinSpacing);
        ImGui::TableSetupColumn("", ImGuiTableColumnFlags_WidthStretch);
        ImGui::TableNextRow();
        ImGui::TableNextColumn();
        ImGui::TextUnformatted("Disk Set");
        ImGui::TableNextColumn();
        importDiskSetPath_.copy(collectionName, importDiskSetPath_.size());
        if (ImGui::InputText("##1", collectionName, sizeof(collectionName))) {
            importDiskSetPath_ = collectionName;
        }
        ImGui::TableNextRow();
        ImGui::TableNextColumn();
        ImGui::TextUnformatted("Disk Name");
        ImGui::TableNextColumn();
        importDiskSetName_.copy(imageName, importDiskSetName_.size());
        if (ImGui::InputText("##2", imageName, sizeof(imageName))) {
            importDiskSetName_ = imageName;
        }
        ImGui::EndTable();
        if (ImGui::Button("Ok")) {
            if (!importDiskSetPath_.empty() && !std::filesystem::path(importDiskSetPath_).empty() &&
                !importDiskSetName_.empty()) {
                importDiskSetPath_ =
                    (std::filesystem::path(diskLibraryRootPath_) / importDiskSetPath_).string();
                guiMode_ = GUIMode::NewBlankDiskFlow;
            } else {
                guiMode_ = GUIMode::Emulator;
            }
            ImGui::CloseCurrentPopup();
        }
        ImGui::SameLine();
        if (ImGui::Button("Cancel")) {
            guiMode_ = GUIMode::Emulator;
            ImGui::CloseCurrentPopup();
        }
        ImGui::Spacing();
        ImGui::EndPopup();
    }
}

void ClemensFrontend::doImportDiskSetFlowStart(int /*width*/, int /*height*/) {
    auto guiModeLast = guiMode_;
    ImVec2 center = ImGui::GetMainViewport()->GetCenter();
    ImGui::SetNextWindowPos(center, ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
    if (ImGui::BeginPopupModal("Import Master Disk Set", NULL, ImGuiWindowFlags_AlwaysAutoResize)) {
        ImGui::Spacing();
        ImGui::Text("Enter the name of the imported disk set collection.");
        char collectionName[64] = "";
        ImGui::SetNextItemWidth(ImGui::GetWindowContentRegionWidth() -
                                ImGui::GetStyle().WindowPadding.x);
        //  TODO: seems the user has to enter text and press enter always to confirm
        //        adding an OK button seems that the right thing to do, but there's
        //        AFAIK no way to force InputText() to return true without some
        //       'next frame return true' hack flag.  Investigate but no need to
        //        waste cycles on figuring out this minor UI convenience for now.
        ImGui::SetItemDefaultFocus();
        bool doImport = false;
        if (ImGui::InputText("##", collectionName, sizeof(collectionName),
                             ImGuiInputTextFlags_EnterReturnsTrue)) {
            importDiskSetName_ = collectionName;
            doImport = true;
        }
        if (ImGui::Button("Ok")) {
            doImport = true;
        }
        if (doImport) {
            if (!importDiskSetName_.empty() && !std::filesystem::path(importDiskSetName_).empty()) {
                importDiskSetPath_ =
                    (std::filesystem::path(diskLibraryRootPath_) / importDiskSetName_).string();
                if (std::filesystem::exists(importDiskSetPath_)) {
                    guiMode_ = GUIMode::ImportDiskSetReplaceOld;
                } else {
                    guiMode_ = GUIMode::ImportDiskSet;
                }
            } else {
                guiMode_ = GUIMode::Emulator;
            }
            ImGui::CloseCurrentPopup();
        }
        ImGui::SameLine();
        if (ImGui::Button("Cancel")) {
            guiMode_ = GUIMode::Emulator;
            ImGui::CloseCurrentPopup();
        }
        ImGui::Spacing();
        ImGui::EndPopup();
    }
    if (guiModeLast != guiMode_) {
        if (guiMode_ == GUIMode::ImportDiskSetReplaceOld) {
            ImGui::OpenPopup("Replace Disk Set");
        } else if (guiMode_ == GUIMode::ImportDiskSet) {
            auto result = importDisks(importDiskSetPath_, importDiskSetName_, importDriveType_,
                                      std::move(importDiskFiles_));
            if (!result.second) {
                messageModalString_ = std::move(result.first);
                ImGui::OpenPopup("Import Disk Set Error");
            } else {
                messageModalString_ = fmt::format("Import {} completed.", importDiskSetName_);
                ImGui::OpenPopup("Confirmation");
            }
        }
    }
}

void ClemensFrontend::doImportDiskSetReplaceOld(int width, int height) {
    auto guiModeLast = guiMode_;
    ImVec2 center = ImGui::GetMainViewport()->GetCenter();
    ImGui::SetNextWindowPos(center, ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
    ImGui::SetNextWindowSize(
        ImVec2(std::max((float)width * 0.40f, 720.0f), std::max((float)height * 0.25f, 360.0f)),
        ImGuiCond_Appearing);
    bool failure = false;
    if (ImGui::BeginPopupModal("Replace Disk Set", NULL)) {
        ImGui::Spacing();
        ImGui::Spacing();
        ImGui::TextWrapped("%s already exists.", importDiskSetPath_.c_str());
        ImGui::Spacing();
        ImGui::Text("Replace?");
        ImGui::Spacing();
        ImGui::Spacing();

        auto cursorPos = ImGui::GetCursorPos();
        auto contentRegionAvail = ImGui::GetContentRegionAvail();

        ImGui::SetCursorPos(ImVec2(
            cursorPos.x, cursorPos.y + contentRegionAvail.y -
                             (ImGui::GetStyle().FramePadding.y * 2 + ImGui::GetTextLineHeight())));
        if (ImGui::Button("Yes")) {
            std::error_code errorCode;
            ImGui::CloseCurrentPopup();
            if (guiMode_ == GUIMode::ImportDiskSetReplaceOld) {
                guiMode_ = GUIMode::ImportDiskSet;
            } else if (guiMode_ == GUIMode::NewBlankDiskReplaceOld) {
                guiMode_ = GUIMode::NewBlankDisk;
            }
            if (std::filesystem::remove_all(importDiskSetPath_, errorCode) == std::uintmax_t(-1)) {
                failure = true;
            }
        }
        ImGui::SameLine();
        if (ImGui::Button("No")) {
            ImGui::CloseCurrentPopup();
            guiMode_ = GUIMode::Emulator;
        }
        ImGui::EndPopup();
    }

    if (guiModeLast != guiMode_) {
        if (guiMode_ == GUIMode::ImportDiskSet) {
            if (failure) {
                ImGui::OpenPopup("Import Disk Set Error");
                messageModalString_ = "Cancelled Disk Set import.";
            } else {
                auto result = importDisks(importDiskSetPath_, importDiskSetName_, importDriveType_,
                                          std::move(importDiskFiles_));
                if (!result.second) {
                    ImGui::OpenPopup("Import Disk Set Error");
                    messageModalString_ = std::move(result.first);
                } else {
                    messageModalString_ = fmt::format("Import {} completed.", importDiskSetName_);
                    ImGui::OpenPopup("Confirmation");
                }
            }
        }
    }
}

void ClemensFrontend::doImportDiskSet(int width, int height) {
    ImVec2 center = ImGui::GetMainViewport()->GetCenter();
    ImGui::SetNextWindowPos(center, ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
    ImGui::SetNextWindowSize(
        ImVec2(std::max((float)width * 0.40f, 720.0f), std::max((float)height * 0.25f, 360.0f)),
        ImGuiCond_Appearing);
    if (ImGui::BeginPopupModal("Import Disk Set Error", NULL)) {
        auto cursorPos = ImGui::GetCursorPos();
        auto contentRegionAvail = ImGui::GetContentRegionAvail();
        ImGui::Spacing();
        ImGui::Spacing();
        ImGui::PushTextWrapPos(0.0f);
        ImGui::TextUnformatted(messageModalString_.c_str());
        ImGui::PopTextWrapPos();
        ImGui::Spacing();
        ImGui::Spacing();
        ImGui::SetCursorPos(ImVec2(
            cursorPos.x, cursorPos.y + contentRegionAvail.y -
                             (ImGui::GetStyle().FramePadding.y * 2 + ImGui::GetTextLineHeight())));
        if (ImGui::Button("Ok")) {
            ImGui::CloseCurrentPopup();
            guiMode_ = GUIMode::Emulator;
        }
        ImGui::EndPopup();
    }
    ImGui::SetNextWindowPos(center, ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
    ImGui::SetNextWindowSize(
        ImVec2(std::max((float)width * 0.40f, 720.0f), std::max((float)height * 0.20f, 280.0f)),
        ImGuiCond_Appearing);
    if (ImGui::BeginPopupModal("Confirmation", NULL)) {
        auto cursorPos = ImGui::GetCursorPos();
        auto contentRegionAvail = ImGui::GetContentRegionAvail();
        ImGui::Spacing();
        ImGui::Spacing();
        ImGui::PushTextWrapPos(0.0f);
        ImGui::TextUnformatted(messageModalString_.c_str());
        ImGui::PopTextWrapPos();
        ImGui::Spacing();
        ImGui::SetCursorPos(ImVec2(
            cursorPos.x, cursorPos.y + contentRegionAvail.y -
                             (ImGui::GetStyle().FramePadding.y * 2 + ImGui::GetTextLineHeight())));
        if (ImGui::Button("Ok")) {
            ImGui::CloseCurrentPopup();
            guiMode_ = GUIMode::Emulator;
        }
        ImGui::EndPopup();
    }
}

void ClemensFrontend::doNewBlankDiskFlow(int /*width */, int /*height*/) {
    // importDriveType_ is the drive type of the disk to create
    // importDiskSetPath_ is the path of the disk set
    // importDiskSetName_ is the name of the blank dist
    // Must create or use the existing path pointed to in importDiskSetPath
    // location = importDiskSetPath_ / importDiskSetName_ + .woz
    // If a file exists (@location), then
    //  error out
    // Generate local disk set/name path for:
    //   auto relativePath = location.parent_path().filename() / location.stem();
    //  issue backend::InsertNewDisk()
    auto diskDirectory = std::filesystem::path(importDiskSetPath_);
    auto diskPath = diskDirectory / importDiskSetName_;

    if (std::filesystem::exists(diskPath)) {
        guiMode_ = GUIMode::NewBlankDiskReplaceOld;
        ImGui::OpenPopup("Replace Disk Set");
    } else {
        guiMode_ = GUIMode::NewBlankDisk;
    }
}

void ClemensFrontend::doNewBlankDisk(int /*width */, int /*height*/) {
    // importDriveType_ is the drive type of the disk to create
    // importDiskSetPath_ is the path of the disk set
    // importDiskSetName_ is the name of the blank dist
    // Must create or use the existing path pointed to in importDiskSetPath
    // location = importDiskSetPath_ / importDiskSetName_ + .woz
    // If a file exists (@location), then
    //  error out
    // Generate local disk set/name path for:
    //   auto relativePath = location.parent_path().filename() / location.stem();
    //  issue backend::InsertNewDisk()
    auto diskDirectory = std::filesystem::path(importDiskSetPath_);
    if (!std::filesystem::exists(diskDirectory)) {
        if (!std::filesystem::create_directories(diskDirectory)) {
            messageModalString_ =
                fmt::format("Unable to create directory {}", diskDirectory.string());
            ImGui::OpenPopup("Import Disk Set Error");
            guiMode_ = GUIMode::ImportDiskSetFlow;
            return;
        }
    }
    auto diskPath = diskDirectory / importDiskSetName_;
    diskPath.replace_extension("woz");
    backend_->insertBlankDisk(importDriveType_, diskPath.string());
    guiMode_ = GUIMode::Emulator;
}

std::pair<std::string, bool> ClemensFrontend::importDisks(std::string outputPath,
                                                          std::string /*collectionName */,
                                                          ClemensDriveType driveType,
                                                          std::vector<std::string> imagePaths) {
    //  parse file extension for supported types:
    //    WOZ
    //    2MG
    //    DSK
    //    DO
    //    PO
    //  only succeed if each image works with the desired drive type
    ClemensDiskImporter importer(driveType, imagePaths.size());
    for (auto &imagePath : imagePaths) {
        ClemensWOZDisk *disk = importer.add(imagePath);
        if (!disk) {
            return std::make_pair(fmt::format("Failed to import disk image {} for drive format {}",
                                              imagePath, sDriveDescription[driveType]),
                                  false);
        }
        switch (driveType) {
        case kClemensDrive_3_5_D1:
        case kClemensDrive_3_5_D2:
            if (disk->nib->disk_type != CLEM_DISK_TYPE_3_5) {
                return std::make_pair(
                    fmt::format(
                        "Disk image {} with type 3.5 doesn't match drive with required format {}",
                        imagePath, sDriveDescription[driveType]),
                    false);
            }
            break;
        case kClemensDrive_5_25_D1:
        case kClemensDrive_5_25_D2:
            if (disk->nib->disk_type != CLEM_DISK_TYPE_5_25) {
                return std::make_pair(
                    fmt::format(
                        "Disk image {} with type 5.25 doesn't match drive with required format {}",
                        imagePath, sDriveDescription[driveType]),
                    false);
            }
            break;
        default:
            break;
        }
    }
    if (!std::filesystem::create_directories(outputPath)) {
        return std::make_pair(fmt::format("Unable to create directory {}", outputPath), false);
    }
    if (!importer.build(outputPath)) {
        //  TODO: mare information please!
        return std::make_pair(
            fmt::format("Import build step failed for drive type {}", sDriveDescription[driveType]),
            false);
    }

    return std::make_pair(outputPath, true);
}

static std::string_view trimToken(const std::string_view &token, size_t off = 0,
                                  size_t length = std::string_view::npos) {
    auto tmp = token.substr(off, length);
    auto left = tmp.begin();
    for (; left != tmp.end(); ++left) {
        if (!std::isspace(*left))
            break;
    }
    tmp = tmp.substr(left - tmp.begin());
    auto right = tmp.rbegin();
    for (; right != tmp.rend(); ++right) {
        if (!std::isspace(*right))
            break;
    }
    tmp = tmp.substr(0, tmp.size() - (right - tmp.rbegin()));
    return tmp;
}

static bool parseBool(const std::string_view &token, bool &result) {
    if (token == "on" || token == "true") {
        result = true;
        return true;
    } else if (token == "off" || token == "false") {
        result = false;
        return false;
    }
    int v = 0;
    if (std::from_chars(token.data(), token.data() + token.size(), v).ec == std::errc{}) {
        result = v != 0;
        return true;
    }
    return false;
}

static bool parseInt(const std::string_view &token, int &result) {
    if (std::from_chars(token.data(), token.data() + token.size(), result).ec == std::errc{}) {
        return true;
    }
    return false;
}

void ClemensFrontend::executeCommand(std::string_view command) {
    CLEM_TERM_COUT.format(TerminalLine::Command, "* {}", command);
    auto sep = command.find(' ');
    auto action = trimToken(command, 0, sep);
    auto operand = sep != std::string_view::npos ? trimToken(command, sep + 1) : std::string_view();
    if (action == "help" || action == "?") {
        cmdHelp(operand);
    } else if (action == "run" || action == "r") {
        cmdRun(operand);
    } else if (action == "break" || action == "b") {
        cmdBreak(operand);
    } else if (action == "reboot") {
        cmdReboot(operand);
    } else if (action == "reset") {
        cmdReset(operand);
    } else if (action == "disk") {
        cmdDisk(operand);
    } else if (action == "step" || action == "s") {
        cmdStep(operand);
    } else if (action == "log") {
        cmdLog(operand);
    } else if (action == "dump") {
        cmdDump(operand);
    } else if (action == "trace") {
        cmdTrace(operand);
    } else if (action == "save") {
        cmdSave(operand);
    } else if (action == "load") {
        cmdLoad(operand);
    } else if (action == "get" || action == "g") {
        cmdGet(operand);
    } else if (action == "adbmouse") {
        cmdADBMouse(operand);
    } else if (!action.empty()) {
        CLEM_TERM_COUT.print(TerminalLine::Error, "Unrecognized command!");
    }
}

void ClemensFrontend::cmdHelp(std::string_view operand) {
    if (!operand.empty()) {
        CLEM_TERM_COUT.print(TerminalLine::Warn, "Command specific help not yet supported.");
    }
    CLEM_TERM_COUT.print(TerminalLine::Info,
                         "reset                       - soft reset of the machine");
    CLEM_TERM_COUT.print(TerminalLine::Info,
                         "reboot                      - hard reset of the machine");
    CLEM_TERM_COUT.print(TerminalLine::Info, "disk                        - disk information");
    CLEM_TERM_COUT.print(TerminalLine::Info, "disk <drive>,file=<image>   - insert disk");
    CLEM_TERM_COUT.print(TerminalLine::Info, "disk <drive>,wprot=<off|on> - write protect");
    CLEM_TERM_COUT.print(TerminalLine::Info, "disk <drive>,eject          - eject disk");
    CLEM_TERM_COUT.print(TerminalLine::Info,
                         "r]un                        - execute emulator until break");
    CLEM_TERM_COUT.print(TerminalLine::Info, "s]tep                       - steps one instruction");
    CLEM_TERM_COUT.print(TerminalLine::Info,
                         "s]tep <count>               - step 'count' instructions");
    CLEM_TERM_COUT.print(TerminalLine::Info,
                         "b]reak                      - break execution at current PC");
    CLEM_TERM_COUT.print(TerminalLine::Info,
                         "b]reak <address>            - break execution at address");
    CLEM_TERM_COUT.print(TerminalLine::Info,
                         "b]reak r:<address>          - break on data read from address");
    CLEM_TERM_COUT.print(TerminalLine::Info,
                         "b]reak w:<address>          - break on write to address");
    CLEM_TERM_COUT.print(TerminalLine::Info,
                         "b]reak erase,<index>        - remove breakpoint with index");
    CLEM_TERM_COUT.print(TerminalLine::Info, "b]reak irq                  - break on IRQ");
    CLEM_TERM_COUT.print(TerminalLine::Info, "b]reak list                 - list all breakpoints");
    CLEM_TERM_COUT.print(TerminalLine::Info,
                         "g]et <register>             - return the current value of a register");
    CLEM_TERM_COUT.print(TerminalLine::Info,
                         "v]iew {memory|doc}          - view browser in context area");
    CLEM_TERM_COUT.print(TerminalLine::Info,
                         "log {DEBUG|INFO|WARN|UNIMPL}- set the emulator log level");
    CLEM_TERM_COUT.print(TerminalLine::Info,
                         "dump <bank_begin>,          - dump memory from selected banks\n"
                         "     <bank_end>,              to a file with the specified\n"
                         "     <filename>, {bin|hex}    output format");
    CLEM_TERM_COUT.print(TerminalLine::Info,
                         "trace {on|off},<pathname>   - toggle program tracing and output to file");
    CLEM_TERM_COUT.print(
        TerminalLine::Info,
        "save <pathname>             - saves a snapshot into the snapshots folder");
    CLEM_TERM_COUT.print(
        TerminalLine::Info,
        "load <pathname>             - loads a snapshot into the snapshots folder");
    CLEM_TERM_COUT.print(TerminalLine::Info,
                         "adbmouse <dx>,<dy>          - injects a mouse move event");
    CLEM_TERM_COUT.print(
        TerminalLine::Info,
        "adbmouse {0|1}              - injects a mouse button event (1=up, 0=down)");
    CLEM_TERM_COUT.newline();
}

void ClemensFrontend::cmdBreak(std::string_view operand) {
    //  parse [r|w]<address>
    auto sepPos = operand.find(',');
    if (sepPos != std::string_view::npos) {
        //  multiple parameter breakpoint expression
        auto param = trimToken(operand, sepPos + 1);
        operand = trimToken(operand, 0, sepPos);
        if (operand == "erase") {
            int index = -1;
            if (!parseInt(param, index)) {
                CLEM_TERM_COUT.format(TerminalLine::Error, "Invalid index specified {}", param);
                return;
            } else if (index < 0 || index >= int(breakpoints_.size())) {
                CLEM_TERM_COUT.format(TerminalLine::Error, "Breakpoint {} doesn't exist", index);
                return;
            }
            backend_->removeBreakpoint(index);
        }
        return;
    }
    if (operand == "list") {
        //  TODO: granular listing based on operand
        static const char *bpType[] = {"unknown", "execute", "data-read", "write", "IRQ"};
        if (breakpoints_.empty()) {
            CLEM_TERM_COUT.print(TerminalLine::Info, "No breakpoints defined.");
            return;
        }
        for (size_t i = 0; i < breakpoints_.size(); ++i) {
            auto &bp = breakpoints_[i];
            if (bp.type == ClemensBackendBreakpoint::IRQ) {
                CLEM_TERM_COUT.format(TerminalLine::Info, "bp #{}: {}", i, bpType[bp.type]);
            } else {
                CLEM_TERM_COUT.format(TerminalLine::Info, "bp #{}: {:02X}/{:04X} {}", i,
                                      (bp.address >> 16) & 0xff, bp.address & 0xffff,
                                      bpType[bp.type]);
            }
        }
        return;
    }
    //  create breakpoint
    ClemensBackendBreakpoint breakpoint{ClemensBackendBreakpoint::Undefined};
    sepPos = operand.find(':');
    if (sepPos != std::string_view::npos) {
        auto typeStr = operand.substr(0, sepPos);
        if (typeStr.size() == 1) {
            if (typeStr[0] == 'r') {
                breakpoint.type = ClemensBackendBreakpoint::DataRead;
            } else if (typeStr[0] == 'w') {
                breakpoint.type = ClemensBackendBreakpoint::Write;
            }
        }
        if (breakpoint.type == ClemensBackendBreakpoint::Undefined) {
            CLEM_TERM_COUT.format(TerminalLine::Error, "Breakpoint type {} is invalid.", typeStr);
            return;
        }
        operand = trimToken(operand, sepPos + 1);
        if (operand.empty()) {
            CLEM_TERM_COUT.format(TerminalLine::Error, "Breakpoint type {} is invalid.", typeStr);
            return;
        }
    } else if (operand == "irq") {
        breakpoint.type = ClemensBackendBreakpoint::IRQ;
        breakpoint.address = 0x0;
        backend_->addBreakpoint(breakpoint);
        return;
    } else {
        breakpoint.type = ClemensBackendBreakpoint::Execute;
    }

    char address[16];
    auto bankSepPos = operand.find('/');
    if (bankSepPos == std::string_view::npos) {
        if (operand.size() >= 2) {
            snprintf(address, sizeof(address), "%02X", frameReadState_.cpu.regs.PBR);
            operand.copy(address + 2, 4, 2);
        } else if (!operand.empty()) {
            CLEM_TERM_COUT.format(TerminalLine::Error, "Address {} is invalid.", operand);
            return;
        }
    } else if (bankSepPos == 2 && operand.size() > bankSepPos) {
        operand.copy(address, bankSepPos, 0);
        operand.copy(address + bankSepPos, operand.size() - (bankSepPos + 1), bankSepPos + 1);
    } else {
        CLEM_TERM_COUT.format(TerminalLine::Error, "Address {} is invalid.", operand);
        return;
    }
    if (operand.empty()) {
        backend_->breakExecution();
    } else {
        address[6] = '\0';
        char *addressEnd = NULL;
        breakpoint.address = std::strtoul(address, &addressEnd, 16);
        if (addressEnd != address + 6) {
            CLEM_TERM_COUT.format(TerminalLine::Error, "Address format is invalid read from '{}'",
                                  operand);
            return;
        }
        backend_->addBreakpoint(breakpoint);
    }
}

void ClemensFrontend::cmdRun(std::string_view /*operand*/) { backend_->run(); }

void ClemensFrontend::cmdStep(std::string_view operand) {
    unsigned count = 1;
    if (!operand.empty()) {
        if (std::from_chars(operand.data(), operand.data() + operand.size(), count).ec !=
            std::errc{}) {
            CLEM_TERM_COUT.format(TerminalLine::Error, "Couldn't parse a number from '{}' for step",
                                  operand);
            return;
        }
    }
    backend_->step(count);
}

void ClemensFrontend::cmdReboot(std::string_view /*operand*/) {
    guiMode_ = GUIMode::RebootEmulator;
    backend_->terminate();
    CLEM_TERM_COUT.print(TerminalLine::Info, "Rebooting machine...");
}

void ClemensFrontend::cmdReset(std::string_view /*operand*/) { backend_->reset(); }

void ClemensFrontend::cmdDisk(std::string_view operand) {
    // disk
    // disk <drive>,file=<image>
    // disk <drive>,wprot=off|on
    if (operand.empty()) {
        for (auto it = frameReadState_.diskDrives.begin(); it != frameReadState_.diskDrives.end();
             ++it) {
            auto driveType = static_cast<ClemensDriveType>(it - frameReadState_.diskDrives.begin());
            CLEM_TERM_COUT.format(TerminalLine::Info, "{} {}: {}",
                                  it->isWriteProtected ? "wp" : "  ",
                                  ClemensDiskUtilities::getDriveName(driveType),
                                  it->imagePath.empty() ? "<none>" : it->imagePath);
        }
        return;
    }
    auto sepPos = operand.find(',');
    auto driveType = ClemensDiskUtilities::getDriveType(trimToken(operand, 0, sepPos));
    if (driveType == kClemensDrive_Invalid) {
        CLEM_TERM_COUT.format(TerminalLine::Error, "Invalid drive name {} specified.", operand);
        return;
    }
    auto &driveInfo = frameReadState_.diskDrives[driveType];
    std::string_view diskOpExpr;
    if (sepPos == std::string_view::npos || (diskOpExpr = trimToken(operand, sepPos + 1)).empty()) {

        CLEM_TERM_COUT.format(TerminalLine::Info, "{} {}: {}",
                              driveInfo.isWriteProtected ? "wp" : "  ",
                              ClemensDiskUtilities::getDriveName(driveType),
                              driveInfo.imagePath.empty() ? "<none>" : driveInfo.imagePath);
        return;
    }

    bool validOp = true;
    bool validValue = true;
    sepPos = diskOpExpr.find('=');
    auto diskOpType = trimToken(diskOpExpr, 0, sepPos);
    std::string_view diskOpValue;
    if (sepPos == std::string_view::npos) {
        if (diskOpType == "eject") {
            backend_->ejectDisk(driveType);
        } else {
            validOp = false;
        }
    } else {
        diskOpValue = trimToken(diskOpExpr, sepPos + 1);
        if (diskOpType == "file") {
            backend_->insertDisk(driveType, std::string(diskOpValue));
        } else if (diskOpType == "wprot") {
            bool wprot;
            if (parseBool(diskOpValue, wprot)) {
                backend_->writeProtectDisk(driveType, wprot);
            } else {
                validValue = false;
            }
        } else {
            validOp = false;
        }
    }
    if (!validValue) {
        CLEM_TERM_COUT.format(TerminalLine::Error, "Invalid value {} in expression.", diskOpValue);
        return;
    } else if (!validOp) {
        CLEM_TERM_COUT.format(TerminalLine::Error, "Invalid or unsupported operation {}.",
                              diskOpExpr);
        return;
    }
}

void ClemensFrontend::cmdLog(std::string_view operand) {
    static std::array<const char *, 5> logLevelNames = {"DEBUG", "INFO", "WARN", "UNIMPL", "FATAL"};
    if (operand.empty()) {
        CLEM_TERM_COUT.format(TerminalLine::Info, "Log level set to {}.",
                              logLevelNames[frameReadState_.logLevel]);
        return;
    }
    auto levelName = std::find_if(logLevelNames.begin(), logLevelNames.end(),
                                  [&operand](const char *name) { return operand == name; });
    if (levelName == logLevelNames.end()) {
        CLEM_TERM_COUT.format(
            TerminalLine::Error,
            "Log level '{}' is not one of the following: DEBUG, INFO, WARN, UNIMPL or FATAL",
            operand);
        return;
    }
    backend_->debugLogLevel(int(levelName - logLevelNames.begin()));
}

static std::tuple<std::array<std::string_view, 8>, std::string_view, size_t>
gatherMessageParams(std::string_view &message, bool with_cmd = false) {
    std::array<std::string_view, 8> params;
    size_t paramCount = 0;

    size_t sepPos = std::string_view::npos;
    std::string_view cmd;
    if (with_cmd) {
        sepPos = message.find(' ');
        cmd = message.substr(0, sepPos);
    }
    if (sepPos != std::string_view::npos) {
        message = message.substr(sepPos + 1);
    }
    while (!message.empty() && paramCount < params.size()) {
        sepPos = message.find(',');
        params[paramCount++] = trimToken(message.substr(0, sepPos));
        if (sepPos != std::string_view::npos) {
            message = message.substr(sepPos + 1);
        } else {
            message = "";
        }
    }
    return {params, cmd, paramCount};
}

void ClemensFrontend::cmdDump(std::string_view operand) {
    //  parse out parameters <start>, <end>, <filename>, <format>
    //  if format is absent, dumps to binary
    auto [params, cmd, paramIdx] = gatherMessageParams(operand);
    if (paramIdx < 3) {
        CLEM_TERM_COUT.print(TerminalLine::Error,
                             "Command requires <start_bank>, <end_bank>, <filename>");
        return;
    }
    uint8_t bankl, bankr;
    if (std::from_chars(params[0].data(), params[0].data() + params[0].size(), bankl, 16).ec !=
        std::errc{}) {
        CLEM_TERM_COUT.format(TerminalLine::Error, "Command start bank '{}' is invalid", params[0]);
        return;
    }
    if (std::from_chars(params[1].data(), params[1].data() + params[1].size(), bankr, 16).ec !=
            std::errc{} ||
        bankl > bankr) {
        CLEM_TERM_COUT.format(TerminalLine::Error, "Command end bank '{}' is invalid", params[1]);
        return;
    }
    if (paramIdx == 3) {
        params[3] = "bin";
    }
    if (paramIdx == 4 && params[3] != "hex" && params[3] != "bin") {
        CLEM_TERM_COUT.print(TerminalLine::Error, "Command format type must be 'hex' or 'bin'");
        return;
    }
    std::string message = "dump ";
    for (auto &param : params) {
        message += param;
        message += ",";
    }
    message.pop_back();
    backend_->debugMessage(std::move(message));
}

void ClemensFrontend::cmdTrace(std::string_view operand) {
    auto [params, cmd, paramCount] = gatherMessageParams(operand);
    if (paramCount > 2) {
        CLEM_TERM_COUT.format(TerminalLine::Error, "Trace command doesn't recognize parameter {}",
                              params[paramCount]);
        return;
    }
    if (paramCount == 0) {
        CLEM_TERM_COUT.format(TerminalLine::Info, "Trace is {}",
                              frameReadState_.isTracing ? "active" : "inactive");
        return;
    }
    std::optional<bool> enable;
    if (params[0] == "on") {
        enable = true;
    } else if (params[0] == "off") {
        enable = false;
    }
    std::string path;
    if (paramCount > 1) {
        path = params[1];
    }
    if (enable.has_value()) {
        if (!frameReadState_.isTracing) {
            if (!enable) {
                CLEM_TERM_COUT.print(TerminalLine::Info, "Not tracing.");
            } else {
                CLEM_TERM_COUT.print(TerminalLine::Info, "Enabling trace.");
            }
        } else {
            if (!enable) {
                if (path.empty()) {
                    CLEM_TERM_COUT.print(
                        TerminalLine::Warn,
                        "Trace will be lost as tracing was active but no output file"
                        " was specified");
                }
            }
            if (!path.empty()) {
                CLEM_TERM_COUT.format(TerminalLine::Info, "Trace will be saved to {}", path);
            }
        }
    } else if (frameReadState_.isTracing) {
        if (params[0] == "iwm") {
            if (frameReadState_.isIWMTracing) {
                CLEM_TERM_COUT.print(TerminalLine::Info, "IWM tracing deactivated");
            } else {
                CLEM_TERM_COUT.print(TerminalLine::Info, "IWM tracing activated");
            }
        } else {
            CLEM_TERM_COUT.format(TerminalLine::Error, "Invalid tracing option '{}'", params[0]);
        }
    } else {
        CLEM_TERM_COUT.print(TerminalLine::Error,
                             "Operation only allowed while tracing is active.");
    }
    backend_->debugProgramTrace(std::string(params[0]), path);
}

std::string ClemensFrontend::cmdMessageFromBackend(std::string_view message,
                                                   const ClemensMachine *machine) {
    auto [params, cmd, paramCount] = gatherMessageParams(message, true);
    if (cmd == "dump") {
        unsigned startBank, endBank;
        if (std::from_chars(params[0].data(), params[0].data() + params[0].size(), startBank, 16)
                .ec != std::errc{}) {
            return fmt::format("FAIL:{} {},{}", cmd, params[2], params[3]);
        }
        if (std::from_chars(params[1].data(), params[1].data() + params[1].size(), endBank, 16)
                .ec != std::errc{}) {
            return fmt::format("FAIL:{} {},{}", cmd, params[2], params[3]);
        }
        startBank &= 0xff;
        endBank &= 0xff;
        lastCommandState_.memoryCaptureAddress = startBank << 16;
        lastCommandState_.memoryCaptureSize = (endBank - startBank) + 1;
        lastCommandState_.memoryCaptureSize <<= 16;
        lastCommandState_.memory = new uint8_t[lastCommandState_.memoryCaptureSize];
        uint8_t *memoryOut = lastCommandState_.memory;
        for (; startBank <= endBank; ++startBank, memoryOut += 0x10000) {
            clemens_out_bin_data(machine, memoryOut, 0x10000, startBank, 0);
        }
        return fmt::format("OK:{} {},{}", cmd, params[2], params[3]);
    }

    return fmt::format("UNK:{}", cmd);
}

bool ClemensFrontend::cmdMessageLocal(std::string_view message) {
    auto [params, cmd, paramCount] = gatherMessageParams(message, true);
    auto setPos = cmd.find(':');
    std::string_view status;
    if (setPos != std::string_view::npos) {
        status = cmd.substr(0, setPos);
        cmd = cmd.substr(setPos + 1);
    }
    if (status == "UNK") {
        CLEM_TERM_COUT.format(TerminalLine::Error, "Message command '{}' failure.", cmd);
        return false;
    } else if (status == "FAIL") {
        std::string paramLine;
        for (auto &param : params) {
            paramLine += param;
            paramLine.push_back(',');
        }
        paramLine.pop_back();
        CLEM_TERM_COUT.format(TerminalLine::Error, "Message '{}' error with params {}.", cmd,
                              paramLine);
        return false;
    } else if (cmd == "dump") {
        bool isOk = true;
        auto outPath = std::filesystem::path(CLEM_HOST_TRACES_DIR) / params[0];
        std::ios_base::openmode flags = std::ios_base::out | std::ios_base::binary;
        std::ofstream outstream(outPath, flags);
        if (outstream.is_open()) {
            if (params[1] == "bin") {
                outstream.write((char *)lastCommandState_.memory,
                                lastCommandState_.memoryCaptureSize);
                outstream.close();
            } else {
                constexpr unsigned kHexByteCountPerLine = 64;
                char hexDump[kHexByteCountPerLine * 2 + 8 + 1];
                unsigned adrBegin = lastCommandState_.memoryCaptureAddress;
                unsigned adrEnd =
                    lastCommandState_.memoryCaptureAddress + lastCommandState_.memoryCaptureSize;
                uint8_t *memoryOut = lastCommandState_.memory;
                while (adrBegin < adrEnd) {
                    snprintf(hexDump, sizeof(hexDump), "%06X: ", adrBegin);
                    clemens_out_hex_data_from_memory(hexDump + 8, memoryOut,
                                                     kHexByteCountPerLine * 2, adrBegin);
                    hexDump[sizeof(hexDump) - 1] = '\n';
                    outstream.write(hexDump, sizeof(hexDump));
                    adrBegin += 0x40;
                    memoryOut += 0x40;
                }
                outstream.close();
            }
        }
        if (outstream.fail()) {
            CLEM_TERM_COUT.format(TerminalLine::Error, "Dump memory failed to open output file {}",
                                  outPath.string());
            isOk = false;
        } else {
            CLEM_TERM_COUT.format(TerminalLine::Info, "Dump memory to file {}", outPath.string());
        }
        delete[] lastCommandState_.memory;
        lastCommandState_.memory = nullptr;
        lastCommandState_.memoryCaptureAddress = 0;
        lastCommandState_.memoryCaptureSize = 0;
        return isOk;
    } else {
        CLEM_TERM_COUT.format(TerminalLine::Error, "Message command '{}' unrecogized.", cmd);
        return false;
    }
    return false;
}

void ClemensFrontend::cmdSave(std::string_view operand) {
    auto [params, cmd, paramCount] = gatherMessageParams(operand);
    if (paramCount != 1) {
        CLEM_TERM_COUT.print(TerminalLine::Error, "Save requires a filename.");
        return;
    }
    backend_->saveMachine(std::string(params[0]));
}

void ClemensFrontend::cmdLoad(std::string_view operand) {
    auto [params, cmd, paramCount] = gatherMessageParams(operand);
    if (paramCount != 1) {
        CLEM_TERM_COUT.print(TerminalLine::Error, "Load requires a filename.");
        return;
    }
    backend_->loadMachine(std::string(params[0]));
}

void ClemensFrontend::cmdGet(std::string_view operand) {
    if (operand.empty()) {
        CLEM_TERM_COUT.print(TerminalLine::Error, "Get requires a register name.");
        return;
    }
    if (operand == "irqs") {
        CLEM_TERM_COUT.format(TerminalLine::Info, "IRQ: {:08X}", frameReadState_.irqs);
        return;
    }
}

void ClemensFrontend::cmdADBMouse(std::string_view operand) {
    auto [params, cmd, paramCount] = gatherMessageParams(operand);
    ClemensInputEvent input;

    if (paramCount == 2) {
        int16_t dx, dy;
        if (std::from_chars(params[0].data(), params[0].data() + params[0].size(), dx).ec !=
            std::errc{}) {
            CLEM_TERM_COUT.print(TerminalLine::Error, "ADBMouse delta X could not be parsed.");
            return;
        }
        if (std::from_chars(params[1].data(), params[1].data() + params[1].size(), dy).ec !=
            std::errc{}) {
            CLEM_TERM_COUT.print(TerminalLine::Error, "ADBMouse delta Y could not be parsed.");
            return;
        }
        input.type = kClemensInputType_MouseMove;
        input.value_a = dx;
        input.value_b = dy;
    } else if (paramCount == 1) {
        int btn;
        if (std::from_chars(params[0].data(), params[0].data() + params[0].size(), btn).ec !=
            std::errc{}) {
            CLEM_TERM_COUT.print(TerminalLine::Error, "ADBMouse button state could not be parsed.");
            return;
        }
        input.type = btn == 0 ? kClemensInputType_MouseButtonDown : kClemensInputType_MouseButtonUp;
        input.value_a = 0x01;
        input.value_b = 0x01;
    } else {
        CLEM_TERM_COUT.print(TerminalLine::Error, "ADBMouse invalid parameters.");
        return;
    }
    backend_->inputEvent(input);
    CLEM_TERM_COUT.print(TerminalLine::Info, "Input sent.");
}
