//  Boo...
#include <algorithm>
#define _USE_MATH_DEFINES

#include "clem_front.hpp"

#include "clem_backend.hpp"

#include "cinek/fixedstack.hpp"
#include "clem_assets.hpp"
#include "clem_backend.hpp"
#include "clem_defs.h"
#include "clem_disk.h"
#include "clem_host_platform.h"
#include "clem_host_shared.hpp"
#include "clem_host_utils.hpp"
#include "clem_imgui.hpp"
#include "clem_l10n.hpp"
#include "clem_mem.h"
#include "clem_mmio_defs.h"
#include "clem_mmio_types.h"
#include "core/clem_apple2gs_config.hpp"
#include "core/clem_disk_asset.hpp"
#include "core/clem_disk_status.hpp"
#include "core/clem_disk_utils.hpp"
#include "emulator.h"
#include "emulator_mmio.h"
#include "imgui.h"
#include "imgui_internal.h"
#include "spdlog/spdlog.h"
#include "version.h"

#include "cinek/encode.h"
#include "fmt/format.h"
#include "imgui_filedialog/ImGuiFileDialog.h"

#include <cfloat>
#include <charconv>
#include <cmath>
#include <filesystem>
#include <memory>
#include <mutex>
#include <optional>
#include <tuple>

//  Style
namespace ClemensHostStyle {

static constexpr float kSideBarMinWidth = 200.0f;
static constexpr float kMachineStateViewMinWidth = 480.0f;

// monochromatic "platinum" classic CACAC8
//                          middle  969695
// monochromatic "platinum" dark    4A4A33
static ImU32 kDarkFrameColor = IM_COL32(0x4a, 0x4a, 0x33, 0xff);
static ImU32 kDarkInsetColor = IM_COL32(0x3a, 0x3a, 0x22, 0xff);
static ImU32 kWidgetToggleOnColor = IM_COL32(0xFF, 0xFF, 0xFF, 0xff);
static ImU32 kWidgetActiveColor = IM_COL32(0xCA, 0xCA, 0xC8, 0xff);
static ImU32 kWidgetHoverColor = IM_COL32(0xAA, 0xAA, 0xA8, 0xff);
static ImU32 kWidgetColor = IM_COL32(0x96, 0x96, 0x95, 0xff);
static ImU32 kWidgetToggleOffColor = IM_COL32(0x22, 0x22, 0x22, 0xff);

ImU32 getFrameColor(const ClemensFrontend &) { return kDarkFrameColor; }
ImU32 getInsetColor(const ClemensFrontend &) { return kDarkInsetColor; }
ImU32 getWidgetColor(const ClemensFrontend &) { return kWidgetColor; }
ImU32 getWidgetHoverColor(const ClemensFrontend &) { return kWidgetHoverColor; }
ImU32 getWidgetActiveColor(const ClemensFrontend &) { return kWidgetActiveColor; }
ImU32 getWidgetToggleOnColor(const ClemensFrontend &) { return kWidgetToggleOnColor; }
ImU32 getWidgetToggleOffColor(const ClemensFrontend &) { return kWidgetToggleOffColor; }

ImTextureID getImTextureOfAsset(ClemensHostAssets::ImageId id) {
    return (ImTextureID)(ClemensHostAssets::getImage(id));
}
ImVec2 getScaledImageSize(ClemensHostAssets::ImageId id, float size) {
    return ImVec2(ClemensHostAssets::getImageAspect(id) * size, size);
}
} // namespace ClemensHostStyle

//  TODO: clean up disk selection GUI
//  TODO: blank disk gui selection for disk set (selecting combo create will
//        enable another input widget, unselecting will gray out that widget.)
//  TODO: help section in user-mode
//  TODO: allow font selection/theme for GUI
//  TODO: Insert card to slot (gui and non-gui)
//  TODO: Mouse x,y scaling based on display view size vs desktop size
//  TODO: preroll audio for some buffer on to handle sound clipping on lower end
//        systems

//  DONE: Platform specific user data directory (ROM, disk images, traces, etc)
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

//  NTSC visual "resolution"
constexpr int kClemensScreenWidth = 720;
constexpr int kClemensScreenHeight = 480;
constexpr float kClemensAspectRatio = float(kClemensScreenWidth) / kClemensScreenHeight;

//  Delay user-initiated reboot after reset by this amount in seconds
//  This was done in response to a bug related to frequent reboots that appears fixed
//  with the addition of GUIMode::StartingEmulator and delaying the switch to GUIMode::Emulator
//  until the first machine frame runs.
//  Kept this value here since I think it's better UX
constexpr float kClemensRebootDelayDuration = 1.0f;

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

static constexpr size_t kFrameMemorySize = 4 * 1024 * 1024;

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
    case ClemensBackendCommand::Terminate:
        return "Terminate";
    default:
        return "Command";
    }
}

void ClemensFrontend::IWMStatus::copyFrom(ClemensMMIO &mmio, const ClemensDeviceIWM &iwm) {
    const ClemensDrive *iwmDrive = nullptr;
    status = 0;
    if (iwm.io_flags & CLEM_IWM_FLAG_DRIVE_ON) {
        status |= kIWMStatusDriveOn;
    }
    if (iwm.io_flags & CLEM_IWM_FLAG_DRIVE_35) {
        status |= kIWMStatusDrive35;
        iwmDrive = clemens_drive_get(&mmio, kClemensDrive_3_5_D1);
    } else {
        iwmDrive = clemens_drive_get(&mmio, kClemensDrive_5_25_D1);
    }
    if (iwm.io_flags & CLEM_IWM_FLAG_DRIVE_2) {
        status |= kIWMStatusDriveAlt;
        ++iwmDrive; // HACKY!
    }
    if (iwm.q6_switch) {
        status |= kIWMStatusIWMQ6;
    }
    if (iwm.q7_switch) {
        status |= kIWMStatusIWMQ7;
    }
    if (iwmDrive->is_spindle_on) {
        status |= kIWMStatusDriveSpin;
    }

    qtr_track_index = iwmDrive->qtr_track_index;
    track_byte_index = iwmDrive->track_byte_index;
    track_bit_shift = iwmDrive->track_bit_shift;
    track_bit_length = iwmDrive->track_bit_length;
    data = iwm.data_r;
    latch = iwm.latch;
    ph03 = (uint8_t)(iwm.out_phase & 0xff);

    constexpr auto diskBufferSize = sizeof(buffer);
    memset(buffer, 0, diskBufferSize);

    const uint8_t *diskBits = iwmDrive->disk.bits_data;
    unsigned diskTrackIndex = qtr_track_index;

    if (iwmDrive->disk.meta_track_map[diskTrackIndex] != 0xff) {
        diskTrackIndex = iwmDrive->disk.meta_track_map[diskTrackIndex];

        if (iwmDrive->disk.track_initialized[diskTrackIndex]) {
            unsigned trackByteCount = iwmDrive->disk.track_byte_count[diskTrackIndex];
            unsigned left, right;
            if (iwmDrive->track_byte_index > 0) {
                left = iwmDrive->track_byte_index - 1;
            } else {
                left = trackByteCount - 1;
            }
            right = left + diskBufferSize - 1;
            if (right >= trackByteCount) {
                right = right - trackByteCount;
            }
            diskBits += iwmDrive->disk.track_byte_offset[diskTrackIndex];
            unsigned bufferIndex = 0;
            if (left > right) {
                for (; left < trackByteCount && bufferIndex < 4; ++left, ++bufferIndex) {
                    buffer[bufferIndex] = diskBits[left];
                }
                left = 0;
            }
            //  TODO: Buggy - seems that using a blank disk and then inserting a non blank disk
            //  causes a crash
            for (; left <= right; ++left, ++bufferIndex) {
                assert(bufferIndex < 4);
                buffer[bufferIndex] = diskBits[left];
            }
        }
    }
}

void ClemensFrontend::DOCStatus::copyFrom(const ClemensDeviceEnsoniq &doc) {
    memcpy(voice, doc.voice, sizeof(voice));
    memcpy(reg, doc.reg, sizeof(reg));
    memcpy(acc, doc.acc, sizeof(acc));
    memcpy(ptr, doc.ptr, sizeof(ptr));
    memcpy(osc_flags, doc.osc_flags, sizeof(osc_flags));
}

void ClemensFrontend::ADBStatus::copyFrom(ClemensMMIO &mmio) {
    mod_states = clemens_get_adb_key_modifier_states(&mmio);
    for (int i = 0; i < 4; ++i) {
        mouse_reg[i] = mmio.dev_adb.mouse_reg[i];
    }
}

const uint64_t ClemensFrontend::kFrameSeqNoInvalid = std::numeric_limits<uint64_t>::max();

ClemensFrontend::ClemensFrontend(ClemensConfiguration config,
                                 const cinek::ByteBuffer &systemFontLoBuffer,
                                 const cinek::ByteBuffer &systemFontHiBuffer)
    : config_(config), displayProvider_(systemFontLoBuffer, systemFontHiBuffer),
      display_(displayProvider_), audio_(), logLevel_(CLEM_DEBUG_LOG_INFO), uiFrameTimeDelta_(0.0),
      frameSeqNo_(kFrameSeqNoInvalid), frameLastSeqNo_(kFrameSeqNoInvalid),
      frameWriteMemory_(kFrameMemorySize, malloc(kFrameMemorySize)),
      frameReadMemory_(kFrameMemorySize, malloc(kFrameMemorySize)), lastFrameCPUPins_{},
      lastFrameCPURegs_{}, lastFrameIWM_{}, lastFrameIRQs_(0), lastFrameNMIs_(0),
      emulatorHasKeyboardFocus_(true), emulatorHasMouseFocus_(false), terminalChanged_(false),
      consoleChanged_(false), terminalMode_(TerminalMode::Command), debugIOMode_(DebugIOMode::Core),
      vgcDebugMinScanline_(0), vgcDebugMaxScanline_(0), joystickSlotCount_(0),
      guiMode_(GUIMode::None), guiPrevMode_(GUIMode::None), appTime_(0.0),
      nextUIFlashCycleAppTime_(0.0), uiFlashAlpha_(1.0f), settingsView_(config_) {

    ClemensTraceExecutedInstruction::initialize();

    initDebugIODescriptors();
    clem_joystick_open_devices(CLEM_HOST_JOYSTICK_PROVIDER_DEFAULT);

    audio_.start();
    if (config_.gs.audioSamplesPerSecond == 0) {
        config_.gs.audioSamplesPerSecond = audio_.getAudioFrequency();
    }

    auto audioBufferSize = config_.gs.audioSamplesPerSecond * audio_.getBufferStride() / 2;
    lastCommandState_.audioBuffer =
        cinek::ByteBuffer(new uint8_t[audioBufferSize], audioBufferSize);
    thisFrameAudioBuffer_ = cinek::ByteBuffer(new uint8_t[audioBufferSize], audioBufferSize);

    snapshotRootPath_ =
        (std::filesystem::path(config_.dataDirectory) / CLEM_HOST_SNAPSHOT_DIR).string();

    debugMemoryEditor_.ReadFn = &ClemensFrontend::imguiMemoryEditorRead;
    debugMemoryEditor_.WriteFn = &ClemensFrontend::imguiMemoryEditorWrite;
    CLEM_TERM_COUT.format(TerminalLine::Info, "Welcome to the Clemens IIgs Emulator {}.{}",
                          CLEM_HOST_VERSION_MAJOR, CLEM_HOST_VERSION_MINOR);
    spdlog::info("Clemens IIGS Emulator {}.{}", CLEM_HOST_VERSION_MAJOR, CLEM_HOST_VERSION_MINOR);

    if (config_.poweredOn) {
        setGUIMode(GUIMode::RebootEmulator);
    } else {
        setGUIMode(GUIMode::Setup);
    }
}

ClemensFrontend::~ClemensFrontend() {
    stopBackend();
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
        constexpr float kMouseScalar = 1.25f;
        if (input.type == kClemensInputType_MouseMove) {
            input.value_a =
                std::clamp(int16_t(input.value_a * kMouseScalar), (int16_t)(-63), (int16_t)(63));
            input.value_b =
                std::clamp(int16_t(input.value_b * kMouseScalar), (int16_t)(-63), (int16_t)(63));
        }
        backendQueue_.inputEvent(input);
    }
}

void ClemensFrontend::lostFocus() {
    emulatorHasMouseFocus_ = false;
    emulatorHasKeyboardFocus_ = false;
}

void ClemensFrontend::startBackend() {
    auto romPath = config_.romFilename;

    ClemensBackendConfig backendConfig{};

    backendConfig.dataRootPath = config_.dataDirectory;
    backendConfig.snapshotRootPath = snapshotRootPath_;
    backendConfig.traceRootPath =
        (std::filesystem::path(config_.dataDirectory) / CLEM_HOST_TRACES_DIR).string();
    backendConfig.enableFastEmulation = config_.fastEmulationEnabled;
    backendConfig.logLevel = logLevel_;
    backendConfig.type = ClemensBackendConfig::Type::Apple2GS;
    backendConfig.breakpoints = breakpoints_;

    backendConfig.GS = config_.gs;

    spdlog::info("Starting new emulator backend");
    config_.poweredOn = true;
    auto backend = std::make_unique<ClemensBackend>(romPath, backendConfig);
    backendThread_ = std::thread(&ClemensFrontend::runBackend, this, std::move(backend));

    uiFrameTimeDelta_ = 0.0;
    backendQueue_.reset();
    backendQueue_.run();
}

void ClemensFrontend::runBackend(std::unique_ptr<ClemensBackend> backend) {
    ClemensBackendState state;

    ClemensCommandQueue::DispatchResult results{};
    // double frameTimeLag = 0.0f;

    // results = backend->main(commands, state)).second
    while (!results.second) {
        //  TODO: need to account for lag!!!

        results = backend->main(state, results.first,
                                [this](ClemensCommandQueue &commands,
                                       const ClemensCommandQueue::ResultBuffer &lastFrameResults,
                                       const ClemensBackendState &state) {
                                    std::unique_lock<std::mutex> lk(frameMutex_);
                                    readyForFrame_.wait(lk, [this]() {
                                        return (uiFrameTimeDelta_ > 0.0) ||
                                               !stagedBackendQueue_.isEmpty();
                                    });
                                    // CK_TODO("Need to account for lag!!!")
                                    frameSeqNo_++;
                                    backendStateDelegate(state, lastFrameResults);
                                    commands.queue(stagedBackendQueue_);
                                    // frameTimeLag += uiFrameTimeDelta_;
                                    uiFrameTimeDelta_ = 0.0;
                                });
        if (results.second) {
            std::lock_guard<std::mutex> lk(frameMutex_);
            ClemensAppleIIGSConfig config;
            if (backend->queryConfig(config)) {
                lastCommandState_.gsConfig = config;
            } else {
                spdlog::warn("Configuration was not retrieved on termination of emulated machine!");
            }
        }
        //  TODO: check for config change
        //        mutex configuration
        //        save it!
    }
}

void ClemensFrontend::stopBackend() {
    if (backendThread_.joinable()) {
        backendQueue_.terminate();
        {
            std::lock_guard<std::mutex> lk(frameMutex_);
            stagedBackendQueue_.queue(backendQueue_);
        }
        readyForFrame_.notify_one();
        backendThread_.join();
    }
    backendThread_ = std::thread();
    if (lastCommandState_.gsConfig.has_value()) {
        config_.gs = *lastCommandState_.gsConfig;
        config_.setDirty();
    }
    config_.poweredOn = false;
    spdlog::info("Terminated emulator backend");
}

bool ClemensFrontend::isBackendRunning() const { return backendThread_.joinable(); }

void ClemensFrontend::backendStateDelegate(const ClemensBackendState &state,
                                           const ClemensCommandQueue::ResultBuffer &results) {

    //  prevent reallocation of the results vector
    std::copy(results.begin(), results.end(), std::back_inserter(lastCommandState_.results));

    frameWriteState_.copyState(state, lastCommandState_, frameWriteMemory_);
}

void ClemensFrontend::FrameState::copyState(const ClemensBackendState &state,
                                            LastCommandState &commandState,
                                            cinek::FixedStack &frameMemory) {
    frameMemory.reset();

    emulatorClock.ts = state.machine->tspec.clocks_spent;
    emulatorClock.ref_step = CLEM_CLOCKS_PHI0_CYCLE;
    cpu = state.machine->cpu;

    ////////////////////////////////////////////////////////////////////////////
    //  Mega 2 state replicated from the backend
    //      This code could be moved into a separate module just to make this
    //      rather lengthy block of code more maintainable.
    //
    frame.monitor = state.frame->monitor;
    //  copy scanlines as this data may become invalid on a frame-to-frame
    //  basis
    frame.text = state.frame->text;
    if (frame.text.format != kClemensVideoFormat_None) {
        frame.text.scanlines =
            frameMemory.allocateArray<ClemensScanline>(state.frame->text.scanline_limit);
        memcpy(frame.text.scanlines, state.frame->text.scanlines,
               sizeof(ClemensScanline) * state.frame->text.scanline_limit);
    }
    frame.graphics = state.frame->graphics;
    if (frame.graphics.format != kClemensVideoFormat_None) {
        frame.graphics.scanlines =
            frameMemory.allocateArray<ClemensScanline>(state.frame->graphics.scanline_limit);
        memcpy(frame.graphics.scanlines, state.frame->graphics.scanlines,
               sizeof(ClemensScanline) * state.frame->graphics.scanline_limit);
        // need to save off the rgb color buffer since the original memory belongs
        // to the backend
        frame.graphics.rgb = frameMemory.allocateArray<uint16_t>(frame.graphics.rgb_buffer_size);
        memcpy(frame.graphics.rgb, state.frame->graphics.rgb,
               state.frame->graphics.rgb_buffer_size);
    }
    frame.diskDriveStatuses = state.frame->diskDriveStatuses;
    frame.smartPortStatuses = state.frame->smartPortStatuses;

    e0bank = frameMemory.allocateArray<uint8_t>(CLEM_IIGS_BANK_SIZE);
    clemens_out_bin_data(state.machine, e0bank, CLEM_IIGS_BANK_SIZE, 0xe0, 0x0000);
    e1bank = frameMemory.allocateArray<uint8_t>(CLEM_IIGS_BANK_SIZE);
    clemens_out_bin_data(state.machine, e1bank, CLEM_IIGS_BANK_SIZE, 0xe1, 0x0000);

    //  audio data - note, that the actual buffer and some fixed attributes like
    //  stride are all that's needed by the frontend to render audio
    //  frames are accumulated into an audio buffer residing in "last command state"
    //  just in case the front-end refresh rate ends up being slower than the
    //  backend
    frame.audio = state.frame->audio;
    if (frame.audio.data) {
        auto audioBufferSize = int32_t(frame.audio.frame_count * frame.audio.frame_stride);
        auto audioBufferRange = commandState.audioBuffer.forwardSize(audioBufferSize, false);
        memcpy(audioBufferRange.first, frame.audio.data, cinek::length(audioBufferRange));
        frame.audio.data = NULL;
    } else {
        commandState.audioBuffer.reset();
    }
    //  replicate card states needed by the frontend
    for (unsigned slotIndex = 0; slotIndex < CLEM_CARD_SLOT_COUNT; ++slotIndex) {
        if (state.mmio->card_slot[slotIndex]) {
            const char *cardName = state.mmio->card_slot[slotIndex]->io_name(
                state.mmio->card_slot[slotIndex]->context);
            cards[slotIndex] = cardName;
        } else {
            cards[slotIndex].clear();
        }
    }

    //  Mega 2 Component subsystems
    iwm.copyFrom(*state.mmio, state.mmio->dev_iwm);
    doc.copyFrom(state.mmio->dev_audio.doc);
    adb.copyFrom(*state.mmio);

    ////////////////////////////////////////////////////////////////////////////
    //  Memory buffers to inspect
    //
    memoryViewBank = state.debugMemoryPage;
    if (!state.isRunning && state.mmioWasInitialized) {
        memoryView = (uint8_t *)frameMemory.allocate(CLEM_IIGS_BANK_SIZE);
        //  read every byte from the memory controller - which can be 'slow' enough
        //  to effect framerate on some systems.   so we only update memory state
        //  when the emulator isn't actively running instructions
        for (unsigned addr = 0; addr < 0x10000; ++addr) {
            clem_read(state.machine, &memoryView[addr], addr, state.debugMemoryPage,
                      CLEM_MEM_FLAG_NULL);
        }
        constexpr size_t kDOCRAMSize = sizeof(state.mmio->dev_audio.doc.sound_ram);
        docRAM = (uint8_t *)frameMemory.allocate(kDOCRAMSize);
        memcpy(docRAM, &state.mmio->dev_audio.doc.sound_ram, kDOCRAMSize);
    } else {
        memoryView = nullptr;
        docRAM = nullptr;
    }
    ioPage = (uint8_t *)frameMemory.allocate(256);
    memcpy(ioPage, state.ioPageValues, 256);
    bram = (uint8_t *)frameMemory.allocate(CLEM_RTC_BRAM_SIZE);
    memcpy(bram, state.mmio->dev_rtc.bram, CLEM_RTC_BRAM_SIZE);

    breakpointCount = (unsigned)(state.bpBufferEnd - state.bpBufferStart);
    breakpoints = frameMemory.allocateArray<ClemensBackendBreakpoint>(breakpointCount);

    auto *bpDest = breakpoints;
    for (auto *bpCur = state.bpBufferStart; bpCur != state.bpBufferEnd; ++bpCur, ++bpDest) {
        *bpDest = *bpCur;
        if (state.bpHitIndex.has_value() && !commandState.hitBreakpoint.has_value()) {
            if (unsigned(bpCur - state.bpBufferStart) == *state.bpHitIndex) {
                commandState.hitBreakpoint = *state.bpHitIndex;
            }
        }
    }

    logLevel = state.logLevel;
    for (auto *logItem = state.logBufferStart; logItem != state.logBufferEnd; ++logItem) {
        LogOutputNode *logMemory = reinterpret_cast<LogOutputNode *>(frameMemory.allocate(
            sizeof(LogOutputNode) + CK_ALIGN_SIZE_TO_ARCH(logItem->text.size())));
        logMemory->logLevel = logItem->level;
        logMemory->sz = unsigned(logItem->text.size());
        logItem->text.copy(reinterpret_cast<char *>(logMemory) + sizeof(LogOutputNode),
                           std::string::npos);
        logMemory->next = nullptr;
        if (!commandState.logNode) {
            commandState.logNode = logMemory;
        } else {
            commandState.logNodeTail->next = logMemory;
        }
        commandState.logNodeTail = logMemory;
    }

    if (state.logInstructionStart != state.logInstructionEnd) {
        size_t instructionCount = state.logInstructionEnd - state.logInstructionStart;
        LogInstructionNode *logInstMemory = reinterpret_cast<LogInstructionNode *>(
            frameMemory.allocate(sizeof(LogInstructionNode)));
        logInstMemory->begin =
            frameMemory.allocateArray<ClemensBackendExecutedInstruction>(instructionCount);
        logInstMemory->end = logInstMemory->begin + instructionCount;
        logInstMemory->next = nullptr;
        memcpy(logInstMemory->begin, state.logInstructionStart,
               instructionCount * sizeof(ClemensBackendExecutedInstruction));
        if (!commandState.logInstructionNode) {
            commandState.logInstructionNode = logInstMemory;
        } else {
            commandState.logInstructionNodeTail->next = logInstMemory;
        }
        commandState.logInstructionNodeTail = logInstMemory;
    }

    commandState.isFastEmulationOn = state.fastEmulationOn;

    machineSpeedMhz = state.machineSpeedMhz;
    avgVBLsPerFrame = state.avgVBLsPerFrame;
    vgcModeFlags = state.mmio->vgc.mode_flags;
    irqs = state.mmio->irq_line;
    nmis = state.mmio->nmi_line;

    backendCPUID = state.hostCPUID;
    fps = state.fps;
    mmioWasInitialized = state.mmioWasInitialized;
    isTracing = state.isTracing;
    isRunning = state.isRunning;

    if (state.message.has_value()) {
        fmt::print("debug message: {}\n", *state.message);
    }
    if (state.config.has_value()) {
        commandState.gsConfig = state.config;
    }
}

void ClemensFrontend::pollJoystickDevices() {
    std::array<ClemensHostJoystick, CLEM_HOST_JOYSTICK_LIMIT> joysticks;
    unsigned deviceCount = clem_joystick_poll(joysticks.data());
    unsigned joystickCount = 0;
    ClemensInputEvent inputs[2];
    constexpr int32_t kGameportAxisMagnitude = CLEM_GAMEPORT_PADDLE_AXIS_VALUE_MAX;
    constexpr int32_t kHostAxisMagnitude = CLEM_HOST_JOYSTICK_AXIS_DELTA * 2;
    unsigned index;

    if (!isBackendRunning()) {
        return;
    }

    for (index = 0; index < (unsigned)joysticks.size(); ++index) {
        if (index >= deviceCount || joystickCount >= 2)
            break;
        auto &input = inputs[joystickCount];
        if (joysticks[index].isConnected) {
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
            /*
            printf("SKS: [%u] - A:(%d,%d)  B:(%d,%d)  BTN:%08X\n", index, joysticks[index].x[0],
                    joysticks[index].y[0], joysticks[index].x[1], joysticks[index].y[1],
                    joysticks[index].buttons);
            */
            joysticks_[index] = joysticks[index];
            joystickCount++;
        } else if (joysticks_[index].isConnected) {
            input.type = kClemensInputType_PaddleDisconnected;
            input.gameport_button_mask = 0;
            joysticks_[index].isConnected = false;
            joystickCount++;
        }
        if (joystickCount == 1) {
            input.gameport_button_mask |= CLEM_GAMEPORT_BUTTON_MASK_JOYSTICK_0;
        }
        if (joystickCount == 2) {
            input.gameport_button_mask |= CLEM_GAMEPORT_BUTTON_MASK_JOYSTICK_1;
        }
    }
    joystickSlotCount_ = std::max(joystickSlotCount_, joystickCount);
    if (joystickCount < 1)
        return;
    backendQueue_.inputEvent(inputs[0]);
    if (joystickCount < 2)
        return;
    backendQueue_.inputEvent(inputs[1]);
}

auto ClemensFrontend::frame(int width, int height, double deltaTime, FrameAppInterop &interop)
    -> ViewType {
    //  send commands to emulator thread
    //  get results from emulator thread
    //    video, audio, machine state, etc
    constexpr double kUIFlashCycleTime = 1.0;
    appTime_ += deltaTime;

    if (appTime_ >= nextUIFlashCycleAppTime_) {
        nextUIFlashCycleAppTime_ = std::floor(appTime_ / kUIFlashCycleTime) * kUIFlashCycleTime;
        nextUIFlashCycleAppTime_ += kUIFlashCycleTime;
    }

    if (interop.exitApp)
        return getViewType();

    uiFlashAlpha_ = std::sin((nextUIFlashCycleAppTime_ - appTime_) * M_PI / kUIFlashCycleTime);

    pollJoystickDevices();

    bool isNewFrame = false;
    bool isBackendTerminated = false;
    {
        std::lock_guard<std::mutex> frameLock(frameMutex_);
        isNewFrame = frameLastSeqNo_ != frameSeqNo_;
        if (isNewFrame) {
            lastFrameCPURegs_ = frameReadState_.cpu.regs;
            lastFrameCPUPins_ = frameReadState_.cpu.pins;
            lastFrameIRQs_ = frameReadState_.irqs;
            lastFrameNMIs_ = frameReadState_.nmis;
            lastFrameIWM_ = frameReadState_.iwm;
            lastFrameADBStatus_ = frameReadState_.adb;
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
            logLevel_ = frameReadState_.logLevel;
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
            lastCommandState_.logInstructionNode = lastCommandState_.logInstructionNodeTail =
                nullptr;

            breakpoints_.clear();
            for (unsigned bpIndex = 0; bpIndex < frameReadState_.breakpointCount; ++bpIndex) {
                breakpoints_.emplace_back(frameReadState_.breakpoints[bpIndex]);
            }

            if (!lastCommandState_.results.empty()) {
                for (auto &result : lastCommandState_.results) {
                    processBackendResult(result);
                }
                lastCommandState_.results.clear();
            }
            if (lastCommandState_.hitBreakpoint.has_value()) {
                unsigned bpIndex = *lastCommandState_.hitBreakpoint;
                CLEM_TERM_COUT.format(TerminalLine::Info, "Breakpoint {} hit {:02X}/{:04X}.",
                                      bpIndex, (breakpoints_[bpIndex].address >> 16) & 0xff,
                                      breakpoints_[bpIndex].address & 0xffff);
                emulatorHasMouseFocus_ = false;
                lastCommandState_.hitBreakpoint = std::nullopt;
            }

            std::swap(lastCommandState_.audioBuffer, thisFrameAudioBuffer_);
        }
        uiFrameTimeDelta_ = deltaTime;

        if (lastCommandState_.gsConfig.has_value()) {
            config_.gs = *lastCommandState_.gsConfig;
            lastCommandState_.gsConfig = std::nullopt;
            config_.setDirty();
        }

        stagedBackendQueue_.queue(backendQueue_);
    }
    readyForFrame_.notify_one();

    //  render video
    //  video is rendered to a texture and the UVs of the display on the virtual
    //  monitor are stored in screenUVs
    float screenUVs[2]{0.0f, 0.0f};
    ViewToMonitorTranslation viewToMonitor;
    if (frameReadState_.mmioWasInitialized && isEmulatorActive()) {
        const uint8_t *e0mem = frameReadState_.e0bank;
        const uint8_t *e1mem = frameReadState_.e1bank;
        bool altCharSet = frameReadState_.vgcModeFlags & CLEM_VGC_ALTCHARSET;
        bool text80col = frameReadState_.vgcModeFlags & CLEM_VGC_80COLUMN_TEXT;
        display_.start(frameReadState_.frame.monitor, kClemensScreenWidth, kClemensScreenHeight);
        display_.renderTextGraphics(frameReadState_.frame.text, frameReadState_.frame.graphics,
                                    e0mem, e1mem, text80col, altCharSet);
        if (frameReadState_.frame.graphics.format == kClemensVideoFormat_Double_Hires) {
            display_.renderDoubleHiresGraphics(frameReadState_.frame.graphics, e0mem, e1mem);
        } else if (frameReadState_.frame.graphics.format == kClemensVideoFormat_Hires) {
            display_.renderHiresGraphics(frameReadState_.frame.graphics, e0mem);
        } else if (frameReadState_.frame.graphics.format == kClemensVideoFormat_Super_Hires) {
            display_.renderSuperHiresGraphics(frameReadState_.frame.graphics, e1mem);
        }
        display_.finish(screenUVs);
        viewToMonitor.screenUVs.x = screenUVs[0];
        viewToMonitor.screenUVs.y = screenUVs[1];
        viewToMonitor.size.x = kClemensScreenWidth;
        viewToMonitor.size.y = kClemensScreenHeight;
        viewToMonitor.workSize.x = frameReadState_.frame.monitor.width;
        viewToMonitor.workSize.y = frameReadState_.frame.monitor.height;
    }

    // render audio
    ClemensAudio audioFrame;
    unsigned numBlankAudioFrames = 0;
    unsigned minAudioFrames = unsigned(audio_.getAudioFrequency() * deltaTime);

    audioFrame.data = thisFrameAudioBuffer_.getHead();
    audioFrame.frame_start = 0;
    audioFrame.frame_stride = audio_.getBufferStride();
    audioFrame.frame_count = thisFrameAudioBuffer_.getSize() / audioFrame.frame_stride;
    if (audioFrame.frame_count > 0) {
        if (audioFrame.frame_count < minAudioFrames) {
            numBlankAudioFrames = minAudioFrames - audioFrame.frame_count;
        }
    } else {
        //  this prevents looped playback of the last samples added to the buffer
        numBlankAudioFrames = minAudioFrames;
    }
    //  fill in silence if necessary
    if (numBlankAudioFrames > 0) {
        if (numBlankAudioFrames >=
            (unsigned)(thisFrameAudioBuffer_.getRemaining() / audioFrame.frame_stride)) {
            numBlankAudioFrames = thisFrameAudioBuffer_.getRemaining() / audioFrame.frame_stride;
        }
        auto audioFrames =
            thisFrameAudioBuffer_.forwardSize(numBlankAudioFrames * audioFrame.frame_stride, false);
        memset(audioFrames.first, 0, cinek::length(audioFrames));
        audioFrame.frame_count += numBlankAudioFrames;
    }
    audioFrame.frame_total = audioFrame.frame_count;
    audio_.queue(audioFrame, deltaTime);
    thisFrameAudioBuffer_.reset();

    if (config_.hybridInterfaceEnabled) {
        doDebuggerInterface(ImVec2(width, height), viewToMonitor, deltaTime);
    } else {
        doEmulatorInterface(ImVec2(width, height), viewToMonitor, deltaTime);
    }

    if (isBackendTerminated) {
        fflush(stdout);
    }

    switch (guiMode_) {
    case GUIMode::Setup:
        //  this is handled above
        break;
    case GUIMode::SaveSnapshot:
        if (!saveSnapshotMode_.isStarted()) {
            saveSnapshotMode_.start(backendQueue_, frameReadState_.isRunning);
        }
        if (saveSnapshotMode_.frame(width, height, backendQueue_)) {
            saveSnapshotMode_.stop(backendQueue_);
            setGUIMode(GUIMode::Emulator);
        }
        break;
    case GUIMode::LoadSnapshot:
        if (!loadSnapshotMode_.isStarted()) {
            loadSnapshotMode_.start(backendQueue_, snapshotRootPath_);
        }
        if (loadSnapshotMode_.frame(width, height, backendQueue_)) {
            loadSnapshotMode_.stop(backendQueue_);
            setGUIMode(GUIMode::Emulator);
        }
        break;
    case GUIMode::LoadSnapshotAfterPowerOn:
        if (!isBackendRunning()) {
            startBackend();
        } else {
            setGUIMode(GUIMode::LoadSnapshot);
        }
        break;
    case GUIMode::Help:
        doHelpScreen(width, height);
        break;
    case GUIMode::RebootEmulator:
        if (!isBackendRunning()) {
            startBackend();
            setGUIMode(GUIMode::StartingEmulator);
        }
        break;
    case GUIMode::StartingEmulator:
        if (isNewFrame) {
            setGUIMode(GUIMode::Emulator);
        }
        break;
    case GUIMode::ShutdownEmulator:
        stopBackend();
        setGUIMode(GUIMode::Setup);
        break;
    default:
        break;
    }

    guiPrevMode_ = guiMode_;

    if (delayRebootTimer_.has_value()) {
        delayRebootTimer_ = *delayRebootTimer_ + (float)deltaTime;
        if (*delayRebootTimer_ > kClemensRebootDelayDuration) {
            delayRebootTimer_.reset();
        }
    }

    //  message modals at the top of everything else
    if (!messageModalString_.empty()) {
        if (!ImGui::IsPopupOpen("Error")) {
            ImGui::OpenPopup("Error");
        }
        ImVec2 center = ImGui::GetMainViewport()->GetCenter();
        ImGui::SetNextWindowPos(center, ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
        ImGui::SetNextWindowSize(ImVec2(std::max(480.0f, width * 0.66f), 0.0f));
        if (ImGui::BeginPopupModal("Error", NULL,
                                   ImGuiWindowFlags_Modal | ImGuiWindowFlags_AlwaysAutoResize)) {
            ImGui::Spacing();
            ImGui::PushTextWrapPos();
            ImGui::TextUnformatted(messageModalString_.c_str());
            ImGui::PopTextWrapPos();
            ImGui::Spacing();
            ImGui::Separator();
            if (ImGui::Button("Ok") || ImGui::IsKeyPressed(ImGuiKey_Enter)) {
                ImGui::CloseCurrentPopup();
                messageModalString_.clear();
            }
            ImGui::EndPopup();
        }
    }

    //  handle emulator controls
    if (guiMode_ == GUIMode::Emulator) {
        if (ImGui::IsKeyDown(ImGuiKey_LeftAlt)) {
            if (ImGui::IsKeyDown(ImGuiKey_LeftCtrl) || ImGui::IsKeyDown(ImGuiKey_RightCtrl)) {
                if (ImGui::IsKeyPressed(ImGuiKey_F10)) {
                    emulatorHasMouseFocus_ = !emulatorHasMouseFocus_;
                } else if (ImGui::IsKeyPressed(ImGuiKey_F11)) {
                    config_.hybridInterfaceEnabled = !config_.hybridInterfaceEnabled;
                    config_.setDirty();
                }
            }
        }
    } else {
        lostFocus();
    }

    config_.save();

    interop.mouseLock = emulatorHasMouseFocus_;

    return getViewType();
}

void ClemensFrontend::setGUIMode(GUIMode guiMode) {
    static const char *kGUIModeNames[] = {"None",
                                          "Preamble",
                                          "Setup",
                                          "Emulator",
                                          "LoadSnapshot",
                                          "LoadSnapshotAfterPowerOn",
                                          "SaveSnapshot",
                                          "Help",
                                          "RebootEmulator",
                                          "StartingEmulator",
                                          "ShutdownEmulator"};
    if (guiMode_ == guiMode)
        return;

    switch (guiMode) {
    case GUIMode::Setup:
        settingsView_.start();
        break;
    default:
        break;
    }
    guiMode_ = guiMode;
    spdlog::info("ClemensFrontend - setGUIMode() {}", kGUIModeNames[static_cast<int>(guiMode_)]);
}

void ClemensFrontend::processBackendResult(const ClemensBackendResult &result) {
    bool succeeded = false;
    switch (result.type) {
    case ClemensBackendResult::Failed:
        CLEM_TERM_COUT.format(TerminalLine::Error, "{} Failed.",
                              getCommandTypeName(result.cmd.type));
        break;
    case ClemensBackendResult::Succeeded:
        CLEM_TERM_COUT.print(TerminalLine::Info, "Ok.");
        succeeded = true;
        break;
    default:
        CK_ASSERT(succeeded);
        break;
    }

    //  let GUI modes handle command results
    switch (guiMode_) {
    case GUIMode::SaveSnapshot:
        if (result.cmd.type == ClemensBackendCommand::Type::SaveMachine) {
            if (succeeded)
                saveSnapshotMode_.succeeded();
            else
                saveSnapshotMode_.fail();
        }
        break;
    case GUIMode::LoadSnapshot:
        if (result.cmd.type == ClemensBackendCommand::Type::LoadMachine) {
            if (succeeded)
                loadSnapshotMode_.succeeded();
            else
                loadSnapshotMode_.fail();
        }
        break;
    default:
        break;
    }
}

void ClemensFrontend::doDebuggerInterface(ImVec2 dimensions,
                                          const ViewToMonitorTranslation &viewToMonitor,
                                          double /*deltaTime*/) {
    const ImGuiStyle &kMainStyle = ImGui::GetStyle();
    const ImVec2 kWindowBoundary(kMainStyle.WindowBorderSize + kMainStyle.WindowPadding.x,
                                 kMainStyle.WindowBorderSize + kMainStyle.WindowPadding.y);
    const float kLineSpacing = ImGui::GetTextLineHeightWithSpacing();
    const float kMachineStateViewWidth =
        std::max(dimensions.x * 0.40f, ClemensHostStyle::kMachineStateViewMinWidth);
    const float kMonitorX = kMachineStateViewWidth;
    const float kMonitorViewWidth = dimensions.x - kMonitorX;
    const float kInfoBarHeight = kLineSpacing + kWindowBoundary.y * 2;
    const float kTerminalViewHeight =
        std::max(kLineSpacing * 6 + kWindowBoundary.y * 2, dimensions.y * 0.33f) - kInfoBarHeight;
    const float kMonitorViewHeight = dimensions.y - kInfoBarHeight - kTerminalViewHeight;
    ImVec2 monitorSize(kMonitorViewWidth, kMonitorViewHeight);

    doMachineStateLayout(ImVec2(0, 0), ImVec2(kMachineStateViewWidth, dimensions.y));
    doMachineViewLayout(ImVec2(kMonitorX, 0), monitorSize, viewToMonitor);
    doMachineInfoBar(ImVec2(kMonitorX, monitorSize.y), ImVec2(kMonitorViewWidth, kInfoBarHeight));
    doMachineTerminalLayout(ImVec2(kMonitorX, monitorSize.y + kInfoBarHeight),
                            ImVec2(kMonitorViewWidth, kTerminalViewHeight));
}

void ClemensFrontend::doEmulatorInterface(ImVec2 dimensions,
                                          const ViewToMonitorTranslation &viewToMonitor,
                                          double /*deltaTime*/) {
    const ImGuiStyle &kMainStyle = ImGui::GetStyle();
    const ImVec2 kWindowBoundary(kMainStyle.WindowBorderSize + kMainStyle.WindowPadding.x,
                                 kMainStyle.WindowBorderSize + kMainStyle.WindowPadding.y);
    const float kLineSpacing = ImGui::GetTextLineHeight() + ImGui::GetFontSize() / 2;

    //  The monitor view should be the largest possible given the input viewport
    //  Bottom Info Bar
    ImVec2 kMonitorViewSize(dimensions.x, dimensions.y);
    ImVec2 kInfoStatusSize(dimensions.x, kLineSpacing + kWindowBoundary.y * 2);

    kMonitorViewSize.y -= kInfoStatusSize.y;
    kMonitorViewSize.x = std::min(dimensions.x, kMonitorViewSize.y * kClemensAspectRatio);

    ImVec2 kSideBarSize(ClemensHostStyle::kSideBarMinWidth, dimensions.y - kInfoStatusSize.y);
    kMonitorViewSize.x = dimensions.x - kSideBarSize.x;

    ImVec2 kSideBarAnchor(0.0f, 0.0f);
    ImVec2 kMonitorViewAnchor(kSideBarSize.x, 0.0f);
    ImVec2 kInfoSizeAnchor(0.0f, kSideBarSize.y);
    ImGui::PushStyleColor(ImGuiCol_WindowBg, ClemensHostStyle::getFrameColor(*this));
    doSidePanelLayout(kSideBarAnchor, kSideBarSize);

    if (browseDriveType_.has_value()) {
        doMachineDiskBrowserInterface(kMonitorViewAnchor, kMonitorViewSize);
    } else if (browseSmartDriveIndex_.has_value()) {
        doMachineSmartDiskBrowserInterface(kMonitorViewAnchor, kMonitorViewSize);
    } else if (guiMode_ == GUIMode::Setup) {
        doSetupUI(kMonitorViewAnchor, kMonitorViewSize);
    } else {
        doMachineViewLayout(kMonitorViewAnchor, kMonitorViewSize, viewToMonitor);
        ImVec2 debugViewAnchor;
        ImVec2 debugViewSize;
        debugViewSize.x = ImGui::GetFont()->GetCharAdvance('A') * 24;
        debugViewSize.y = ImGui::GetFontSize() * 8;
        debugViewAnchor.x = kMonitorViewAnchor.x + kMonitorViewSize.x - debugViewSize.x;
        debugViewAnchor.y = 0;
        doDebugView(debugViewAnchor, debugViewSize);
    }
    doInfoStatusLayout(kInfoSizeAnchor, kInfoStatusSize, kMonitorViewAnchor.x);
    ImGui::PopStyleColor();
}

void ClemensFrontend::doSidePanelLayout(ImVec2 anchor, ImVec2 dimensions) {
    //  Display Power Status
    //  Display S5,D1
    //  Display S5,D2
    //  Display S6,D1
    //  Display S5,D1
    //  Display SmartPort HDD
    //  Separator
    //  Display Joystick 1 and 2 status
    //  Display OS specific instructions
    const ImGuiStyle &style = ImGui::GetStyle();
    ImVec2 quickBarSize(dimensions.x, 24.0f + (style.FramePadding.y + style.FrameBorderSize +
                                               style.WindowPadding.y + style.WindowBorderSize) *
                                                  2);
    ImVec2 sidebarSize(dimensions.x, dimensions.y - quickBarSize.y);
    ImGui::SetNextWindowPos(anchor);
    ImGui::SetNextWindowSize(sidebarSize);
    sidebarSize.x -= 2 * style.FramePadding.x;

    ImGui::Begin("SidePanel", nullptr,
                 ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoBringToFrontOnFocus);
    doUserMenuDisplay(sidebarSize.x);
    doMachineDiskDisplay(sidebarSize.x);
    doMachinePeripheralDisplay(sidebarSize.x);
    ImGui::End();
    anchor.y += sidebarSize.y;
    ImGui::SetNextWindowPos(anchor);
    ImGui::SetNextWindowSize(quickBarSize);
    ImGui::Begin("Quickbar", nullptr,
                 ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoBringToFrontOnFocus);
    doDebuggerQuickbar(quickBarSize.x);
    ImGui::End();
}

void ClemensFrontend::doUserMenuDisplay(float /* width */) {
    const ImGuiStyle &style = ImGui::GetStyle();
    const ImVec2 kIconSize(24.0f, 24.0f);
    ImGui::Spacing();
    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing,
                        ImVec2(style.ItemSpacing.x * 1.25f, style.ItemSpacing.y));
    if (isBackendRunning()) {
        ImGui::PushStyleColor(ImGuiCol_Button, IM_COL32(0, 255, 0, 192));
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, IM_COL32(128, 255, 128, 255));
        ImGui::PushStyleColor(ImGuiCol_ButtonActive, IM_COL32(255, 255, 255, 255));
    } else {
        ImGui::PushStyleColor(ImGuiCol_Button, IM_COL32(255, 0, 0, 192));
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, IM_COL32(255, 128, 128, 255));
        ImGui::PushStyleColor(ImGuiCol_ButtonActive, IM_COL32(255, 255, 255, 255));
    }
    //  Button Group 1
    if (ClemensHostImGui::IconButton(
            "Power", ClemensHostStyle::getImTextureOfAsset(ClemensHostAssets::kPowerButton),
            kIconSize)) {
        if (guiMode_ == GUIMode::Setup) {
            setGUIMode(GUIMode::RebootEmulator);
        } else {
            setGUIMode(GUIMode::ShutdownEmulator);
        }
    }
    if (ImGui::IsItemHovered(ImGuiHoveredFlags_DelayNormal)) {
        ImGui::SetTooltip("Power");
    }
    ImGui::PopStyleColor(3);

    //  Button Group 2
    ImGui::SameLine();
    if (!isEmulatorStarting()) {
        ClemensHostImGui::PushStyleButtonEnabled();
    } else {
        ClemensHostImGui::PushStyleButtonDisabled();
    }
    ImGui::SeparatorEx(ImGuiSeparatorFlags_Vertical);
    ImGui::SameLine();
    if (ClemensHostImGui::IconButton(
            "Load Snapshot", ClemensHostStyle::getImTextureOfAsset(ClemensHostAssets::kLoad),
            kIconSize)) {
        if (isBackendRunning()) {
            if (!isEmulatorStarting()) {
                setGUIMode(GUIMode::LoadSnapshot);
            }
        } else {
            setGUIMode(GUIMode::LoadSnapshotAfterPowerOn);
        }
    }
    ClemensHostImGui::PopStyleButton();
    if (ImGui::IsItemHovered(ImGuiHoveredFlags_DelayNormal)) {
        ImGui::SetTooltip("Load Snapshot");
    }
    ImGui::SameLine();
    if (isBackendRunning() && !isEmulatorStarting()) {
        ClemensHostImGui::PushStyleButtonEnabled();
    } else {
        ClemensHostImGui::PushStyleButtonDisabled();
    }
    if (ClemensHostImGui::IconButton(
            "Save Snapshot", ClemensHostStyle::getImTextureOfAsset(ClemensHostAssets::kSave),
            kIconSize)) {
        if (isBackendRunning() && !isEmulatorStarting()) {
            setGUIMode(GUIMode::SaveSnapshot);
        }
    }
    ClemensHostImGui::PopStyleButton();
    if (ImGui::IsItemHovered(ImGuiHoveredFlags_DelayNormal)) {
        ImGui::SetTooltip("Save Snapshot");
    }

    ImGui::SameLine();
    ImGui::SeparatorEx(ImGuiSeparatorFlags_Vertical);
    ImGui::SameLine();
    ClemensHostImGui::PushStyleButtonEnabled();
    if (ClemensHostImGui::IconButton(
            "Help", ClemensHostStyle::getImTextureOfAsset(ClemensHostAssets::kHelp), kIconSize)) {
        setGUIMode(GUIMode::Help);
    }
    if (ImGui::IsItemHovered(ImGuiHoveredFlags_DelayNormal)) {
        ImGui::SetTooltip("Help");
    }
    ClemensHostImGui::PopStyleButton();

    ImGui::PopStyleVar();
    ImGui::Spacing();
    ImGui::Separator();
}

void ClemensFrontend::doMachinePeripheralDisplay(float /*width */) {
    ImGui::BeginChild("PeripheralsAndCards");
    const ImGuiStyle &drawStyle = ImGui::GetStyle();
    ImDrawList *drawList = ImGui::GetWindowDrawList();
    if (ImGui::CollapsingHeader("Motherboard", ImGuiTreeNodeFlags_DefaultOpen)) {
        ImGui::Indent();
        ImGui::Text("RAM: %uK", config_.gs.memory);
        ImGui::Unindent();
    }
    ImGui::Separator();
    if (ImGui::CollapsingHeader("Peripherals", ImGuiTreeNodeFlags_DefaultOpen)) {
        for (unsigned slot = 0; slot < joystickSlotCount_; ++slot) {
            ImColor color = joysticks_[slot].isConnected ? ImColor(255, 255, 255, 255)
                                                         : ImColor(128, 128, 128, 255);
            ImGui::Spacing();
            ImGui::Image(ClemensHostStyle::getImTextureOfAsset(ClemensHostAssets::kJoystick),
                         ImVec2(32.0f, 32.0f), ImVec2(0, 0), ImVec2(1, 1), color.Value);
            ImGui::SameLine();
            ImGui::BeginGroup();
            {
                constexpr auto kButtonOffColor = IM_COL32(128, 128, 128, 255);
                constexpr auto kButtonOffText = IM_COL32(0, 0, 0, 255);
                constexpr auto kButtonOnColor = IM_COL32(192, 0, 0, 255);
                constexpr auto kButtonOnText = IM_COL32(255, 255, 255, 255);
                constexpr ImU32 kAxisOnColor = IM_COL32(128, 128, 128, 255);
                constexpr ImU32 kAxisOffColor = IM_COL32(64, 64, 64, 255);
                const ImU32 dotColor = IM_COL32(0, 192, 0, 255);
                ImGui::TextColored(color, "Joystick %u", slot);
                ImGui::Spacing();
                ImVec2 buttonSize(ImGui::GetFont()->GetCharAdvance('X') +
                                      2 * (drawStyle.FrameBorderSize + drawStyle.FramePadding.x),
                                  ImGui::GetTextLineHeight() +
                                      2 * (drawStyle.FrameBorderSize + drawStyle.FramePadding.y));
                ImVec2 anchorLT = ImGui::GetCursorScreenPos();
                ImVec2 anchorRB(anchorLT.x + buttonSize.x, anchorLT.y + buttonSize.y);

                drawList->AddRectFilled(anchorLT, anchorRB, kAxisOffColor, 4.0f);
                drawList->AddRectFilled(ImVec2(anchorLT.x + 4.0f, anchorLT.y + 4.0f),
                                        ImVec2(anchorRB.x - 4.0f, anchorRB.y - 4.0f), kAxisOnColor,
                                        4.0f);

                if (joysticks_[slot].isConnected) {
                    float xMid = buttonSize.x * 0.5f;
                    float yMid = buttonSize.y * 0.5f;
                    float xAxis = ((float)joysticks_[0].x[0] / CLEM_HOST_JOYSTICK_AXIS_DELTA) *
                                  (buttonSize.x - 4.0f) * 0.5f;
                    float yAxis = ((float)joysticks_[0].y[0] / CLEM_HOST_JOYSTICK_AXIS_DELTA) *
                                  (buttonSize.y - 4.0f) * 0.5f;

                    drawList->AddCircleFilled(
                        ImVec2(anchorLT.x + xMid + xAxis, anchorLT.y + yMid + yAxis), 4.0f,
                        dotColor);
                }

                ImGui::Dummy(buttonSize);
                ImGui::SameLine();

                char buttonText[2];
                buttonText[1] = '\0';
                for (unsigned buttonIndex = 0; buttonIndex < 2; ++buttonIndex) {
                    if (buttonIndex > 0) {
                        ImGui::SameLine();
                    }
                    anchorLT = ImGui::GetCursorScreenPos();
                    anchorRB = ImVec2(anchorLT.x + buttonSize.x, anchorLT.y + buttonSize.y);
                    buttonText[0] = '1' + buttonIndex;
                    ImU32 backColor;
                    ImU32 textColor;
                    if (joysticks_[slot].buttons & (1 << buttonIndex)) {
                        backColor = kButtonOnColor;
                        textColor = kButtonOnText;
                    } else {
                        backColor = kButtonOffColor;
                        textColor = kButtonOffText;
                    }
                    drawList->AddRectFilled(anchorLT, anchorRB, backColor, 4.0f);
                    anchorLT.x += drawStyle.FramePadding.x;
                    anchorLT.y += drawStyle.FramePadding.y;
                    drawList->AddText(anchorLT, textColor, buttonText);
                    ImGui::Dummy(buttonSize);
                }
            }
            ImGui::EndGroup();
            ImGui::Spacing();
            ImGui::Separator();
        }
    }
    if (ImGui::CollapsingHeader("Cards", ImGuiTreeNodeFlags_DefaultOpen)) {
        for (unsigned slot = 0; slot < CLEM_CARD_SLOT_COUNT; ++slot) {
            if (isBackendRunning()) {
                if (frameReadState_.cards[slot].empty())
                    continue;
            } else {
                if (config_.gs.cardNames[slot].empty())
                    continue;
            }
            ImGui::Spacing();
            ImGui::Image(ClemensHostStyle::getImTextureOfAsset(ClemensHostAssets::kCard),
                         ImVec2(32.0f, 32.0f));
            ImGui::SameLine();
            ImGui::BeginGroup();
            ImGui::Text("Slot %u", slot + 1);
            ImGui::Spacing();
            ImGui::TextUnformatted(frameReadState_.cards[slot].c_str());
            ImGui::EndGroup();
            ImGui::Separator();
        }
    }
    ImGui::EndChild();
}

void ClemensFrontend::doInfoStatusLayout(ImVec2 anchor, ImVec2 dimensions, float dividerXPos) {
    //  Display 1Mhz vs Fast
    //  Display Mouse Lock/Focus
    //  Display Key Focus
    //  Display Option, Open Apple, Ctrl, Reset, Escape, Caps lock in abbrev/icons
    ImGui::SetNextWindowPos(anchor);
    ImGui::SetNextWindowSize(dimensions);
    ImGui::Begin("InfoStatus", nullptr,
                 ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoBringToFrontOnFocus |
                     ImGuiWindowFlags_NoMove);

    float statusItemHeight =
        dimensions.y - (ImGui::GetStyle().WindowBorderSize + ImGui::GetStyle().WindowPadding.y) * 2;
    float statusItemPaddingHeight =
        statusItemHeight - ImGui::GetTextLineHeight() - (ImGui::GetStyle().FrameBorderSize * 2);

    ImVec2 framePadding(6.0f, statusItemPaddingHeight / 2);
    ImGui::PushStyleColor(ImGuiCol_Border, ClemensHostStyle::getInsetColor(*this));
    ImGui::PushStyleColor(ImGuiCol_TableRowBg, ClemensHostStyle::getInsetColor(*this));
    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, framePadding);

    const uint8_t *bram = frameReadState_.bram;
    bool is1MhzMode = (bram && bram[CLEM_RTC_BRAM_SYSTEM_SPEED] == 0x00);
    bool isEscapeDown = (frameReadState_.adb.mod_states & CLEM_ADB_KEY_MOD_STATE_ESCAPE) != 0;
    bool isCtrlDown = (frameReadState_.adb.mod_states & CLEM_ADB_KEY_MOD_STATE_CTRL) != 0;
    bool isOptionDown = (frameReadState_.adb.mod_states & CLEM_ADB_KEY_MOD_STATE_OPTION) != 0;
    bool isOpenAppleDown = (frameReadState_.adb.mod_states & CLEM_ADB_KEY_MOD_STATE_APPLE) != 0;
    bool isResetDown = (frameReadState_.adb.mod_states & CLEM_ADB_KEY_MOD_STATE_RESET) != 0;

    ClemensHostImGui::StatusBarField(is1MhzMode ? ClemensHostImGui::StatusBarFlags_Active
                                                : ClemensHostImGui::StatusBarFlags_Inactive,
                                     "1 Mhz");

    ImGui::SameLine();
    ClemensHostImGui::StatusBarField(
        lastCommandState_.isFastEmulationOn ? ClemensHostImGui::StatusBarFlags_Active
                                            : ClemensHostImGui::StatusBarFlags_Inactive,
        ClemensHostStyle::getImTextureOfAsset(ClemensHostAssets::kFastEmulate),
        ClemensHostStyle::getScaledImageSize(ClemensHostAssets::kFastEmulate,
                                             ImGui::GetTextLineHeight()));

    ImGui::SameLine(dividerXPos);
    ImGui::SeparatorEx(ImGuiSeparatorFlags_Vertical);
    ImGui::SameLine();
    ClemensHostImGui::StatusBarField(isCtrlDown ? ClemensHostImGui::StatusBarFlags_Active
                                                : ClemensHostImGui::StatusBarFlags_Inactive,
                                     "CTRL");
    ImGui::SameLine();
    ClemensHostImGui::StatusBarField(isEscapeDown ? ClemensHostImGui::StatusBarFlags_Active
                                                  : ClemensHostImGui::StatusBarFlags_Inactive,
                                     "ESC");
    ImGui::SameLine();
    ClemensHostImGui::StatusBarField(isOptionDown ? ClemensHostImGui::StatusBarFlags_Active
                                                  : ClemensHostImGui::StatusBarFlags_Inactive,
                                     "OPT");
    ImGui::SameLine();
    ClemensHostImGui::StatusBarField(isOpenAppleDown ? ClemensHostImGui::StatusBarFlags_Active
                                                     : ClemensHostImGui::StatusBarFlags_Inactive,
                                     " " CLEM_HOST_OPEN_APPLE_UTF8 " ");
    ImGui::SameLine(0.0f, ImGui::GetFont()->GetCharAdvance('A') * 2);

    float rightSideWidth =
        (ImGui::GetStyle().FrameBorderSize + ImGui::GetStyle().FramePadding.x) * 2 +
        ImGui::GetFont()->CalcTextSizeA(ImGui::GetFontSize(), FLT_MAX, 0.0f, "RESET0").x;

    ImVec2 inputInstructionSize = ImVec2(dimensions.x - ImGui::GetCursorPos().x - rightSideWidth,
                                         ImGui::GetTextLineHeight() + statusItemPaddingHeight);
    doViewInputInstructions(inputInstructionSize);

    ImGui::SameLine(anchor.x + dimensions.x - rightSideWidth);
    ClemensHostImGui::StatusBarField(isResetDown ? ClemensHostImGui::StatusBarFlags_Active
                                                 : ClemensHostImGui::StatusBarFlags_Inactive,
                                     "RESET");
    ImGui::PopStyleVar();
    ImGui::PopStyleColor(2);

    ImGui::End();
}

void ClemensFrontend::doDebuggerQuickbar(float /*width */) {
    const ImGuiStyle &style = ImGui::GetStyle();
    float lineHeight = ImGui::GetWindowHeight() - (style.FramePadding.y + style.FrameBorderSize -
                                                   style.WindowBorderSize + style.WindowPadding.y) *
                                                      2;
    const ImVec2 kIconSize(lineHeight, lineHeight);
    if (isBackendRunning()) {
        ClemensHostImGui::PushStyleButtonEnabled();
        ImGui::SameLine();
        if (ClemensHostImGui::IconButton(
                "CycleButton",
                ClemensHostStyle::getImTextureOfAsset(ClemensHostAssets::kPowerCycle), kIconSize)) {
            if (isBackendRunning() && !isEmulatorStarting()) {
                rebootInternal();
            }
        }
        if (ImGui::IsItemHovered(ImGuiHoveredFlags_DelayNormal)) {
            ImGui::SetTooltip("Reboot (cycle power)");
        }
        ImGui::SameLine();
        if (frameReadState_.isRunning) {
            if (ClemensHostImGui::IconButton(
                    "Continue",
                    ClemensHostStyle::getImTextureOfAsset(ClemensHostAssets::kStopMachine),
                    kIconSize)) {
                backendQueue_.breakExecution();
            }
            if (ImGui::IsItemHovered(ImGuiHoveredFlags_DelayNormal)) {
                ImGui::SetTooltip("Stop execution");
            }
        } else {
            if (ClemensHostImGui::IconButton(
                    "Continue",
                    ClemensHostStyle::getImTextureOfAsset(ClemensHostAssets::kRunMachine),
                    kIconSize)) {
                backendQueue_.run();
            }
            if (ImGui::IsItemHovered(ImGuiHoveredFlags_DelayNormal)) {
                ImGui::SetTooltip("Continue execution");
            }
        }
        ImGui::SameLine();
        if (ClemensHostImGui::IconButton(
                "Debugger", ClemensHostStyle::getImTextureOfAsset(ClemensHostAssets::kDebugger),
                kIconSize)) {
            config_.hybridInterfaceEnabled = true;
            config_.setDirty();
        }
        if (ImGui::IsItemHovered(ImGuiHoveredFlags_DelayNormal)) {
            ImGui::SetTooltip("Enter debugger mode");
        }
        ImGui::SameLine();
        if (ClemensHostImGui::IconButton(
                "Home Folder", ClemensHostStyle::getImTextureOfAsset(ClemensHostAssets::kFolder),
                kIconSize)) {
            open_system_folder_view(config_.dataDirectory.c_str());
        }
        if (ImGui::IsItemHovered(ImGuiHoveredFlags_DelayNormal)) {
            ImGui::SetTooltip("Open Assets Folder on Desktop");
        }
        ClemensHostImGui::PopStyleButton();
    }
}

static ImColor getDefaultColor(bool hi) {
    const ImColor kDefaultColor(255, 255, 255, hi ? 255 : 128);
    return kDefaultColor;
}
static ImColor getModifiedColor(bool hi, bool isRunning) {
    if (isRunning)
        return getDefaultColor(hi);
    const ImColor kModifiedColor(255, 0, 255, hi ? 255 : 128);
    return kModifiedColor;
}

template <typename T> static ImColor getStatusFieldColor(T a, T b, T statusMask, bool isRunning) {
    return (a & statusMask) != (b & statusMask) ? getModifiedColor(b & statusMask, isRunning)
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
    doMachineDiskDisplay(ImGui::GetWindowContentRegionWidth());
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
        if (ImGui::BeginTabItem("VGC")) {
            doMachineDebugVGCDisplay();
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
    if (lastCommandState_.isFastEmulationOn) {
        ImGui::Text("%1.3f (x%3.1f)", frameReadState_.machineSpeedMhz,
                    frameReadState_.avgVBLsPerFrame);

    } else {
        ImGui::Text("%1.3f mhz", frameReadState_.machineSpeedMhz);
    }
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
    ImGui::Text("%02u:%02u:%02u.%01u", hours, minutes, seconds, milliseconds);
    ImGui::EndTable();
}

static const char *sDriveDescriptionShort[] = {"3.5\" disk", "3.5\" disk", "5.25\" disk",
                                               "5.25\" disk"};
static const char *sDriveDescription[] = {"3.5 inch 800K", "3.5 inch 800K", "5.25 inch 140K",
                                          "5.25 inch 140K"};

void ClemensFrontend::doMachineDiskDisplay(float width) {
    ImGui::PushStyleColor(ImGuiCol_Button, ClemensHostStyle::getWidgetColor(*this));
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ClemensHostStyle::getWidgetHoverColor(*this));
    ImGui::PushStyleColor(ImGuiCol_ButtonActive, ClemensHostStyle::getWidgetActiveColor(*this));

    ImGui::Spacing();
    doMachineDiskStatus(kClemensDrive_3_5_D1, width);
    ImGui::Separator();
    ImGui::Spacing();
    doMachineDiskStatus(kClemensDrive_3_5_D2, width);
    ImGui::Separator();
    ImGui::Spacing();
    doMachineDiskStatus(kClemensDrive_5_25_D1, width);
    ImGui::Separator();
    ImGui::Spacing();
    doMachineDiskStatus(kClemensDrive_5_25_D2, width);
    ImGui::Separator();
    ImGui::Spacing();
    doMachineSmartDriveStatus(0, width, !isBackendRunning());
    ImGui::Separator();
    ImGui::Spacing();

    ImGui::PopStyleColor(3);
}

void ClemensFrontend::doMachineDiskStatus(ClemensDriveType driveType, float width) {
    ClemensDiskDriveStatus driveStatus{};
    auto driveName = ClemensDiskUtilities::getDriveName(driveType);

    if (isBackendRunning()) {
        driveStatus = frameReadState_.frame.diskDriveStatuses[driveType];
    } else {
        driveStatus.assetPath = config_.gs.diskImagePaths[driveType];
    }

    const ImGuiStyle &style = ImGui::GetStyle();
    const bool shortForm = !config_.hybridInterfaceEnabled;
    const float height = shortForm ? 48.0f : 32.0f;
    const float iconSize = shortForm ? 32.0f : 24.0f;
    const auto clemensDiskIcon =
        (driveType == kClemensDrive_3_5_D1 || driveType == kClemensDrive_3_5_D2)
            ? ClemensHostAssets::kDisk35
            : ClemensHostAssets::kDisk525;
    ImVec4 uiColor;

    ImGui::PushID(driveName.data(), driveName.data() + driveName.size());
    ImGui::BeginGroup();
    {
        const float rightButtonSize = 16;
        const float rightSectionWidth = (rightButtonSize + style.ItemInnerSpacing.x * 2);

        ImVec2 cursorPos = ImGui::GetCursorScreenPos();
        ImVec2 anchorCursorPos = cursorPos;
        ImVec2 tailCursorPos = cursorPos;

        // body
        if (shortForm) {
            tailCursorPos.x += width - style.FramePadding.x - rightSectionWidth;
        } else {
            tailCursorPos.x += width - style.FramePadding.x - rightSectionWidth * 2;
        }
        tailCursorPos.y += height;

        ImGui::PushClipRect(anchorCursorPos, tailCursorPos, false);
        ImVec2 buttonSize(tailCursorPos.x - anchorCursorPos.x, tailCursorPos.y - anchorCursorPos.y);
        if (ImGui::InvisibleButton("disk", buttonSize)) {
            if (!browseDriveType_.has_value() || *browseDriveType_ != driveType) {
                browseDriveType_ = driveType;
            } else {
                browseDriveType_ = std::nullopt;
            }
            browseSmartDriveIndex_ = std::nullopt;
            if (browseDriveType_.has_value()) {
                if (driveStatus.isMounted()) {
                    assetBrowser_.setCurrentDirectory(
                        std::filesystem::path(driveStatus.assetPath).parent_path().string());
                } else {
                    assetBrowser_.setCurrentDirectory(std::filesystem::current_path().string());
                }
                assetBrowser_.setDiskType(ClemensDiskAsset::diskTypeFromDriveType(driveType));
            }
        }
        ImDrawList *drawList = ImGui::GetWindowDrawList();

        if (ImGui::IsItemActive()) {
            uiColor = style.Colors[ImGuiCol_ButtonActive];
        } else if (ImGui::IsItemHovered()) {
            uiColor = style.Colors[ImGuiCol_ButtonHovered];
        } else {
            uiColor = style.Colors[ImGuiCol_Button];
        }
        if (browseDriveType_.has_value() && *browseDriveType_ == driveType) {
            uiColor.w = uiFlashAlpha_;
        }
        drawList->AddRectFilled(cursorPos, tailCursorPos, (ImU32)ImColor(uiColor));

        //  icon
        auto texId = ClemensHostStyle::getImTextureOfAsset(clemensDiskIcon);
        ImVec2 rbCursorPos;
        cursorPos.y += (height - iconSize) * 0.5f;
        cursorPos.x += style.FramePadding.x;
        rbCursorPos.x = cursorPos.x + iconSize;
        rbCursorPos.y = cursorPos.y + iconSize;
        if (driveStatus.isMounted()) {
            uiColor = ImVec4(1.0f, 1.0f, 1.0f, 1.0f);
        } else {
            uiColor = ImVec4(1.0f, 1.0f, 1.0f, 0.5f);
        }
        drawList->AddImage(texId, cursorPos, rbCursorPos, ImVec2(0.0f, 0.0f), ImVec2(1.0f, 1.0f),
                           IM_COL32_WHITE);

        cursorPos.x += iconSize + style.ItemInnerSpacing.x;
        doMachineDiskMotorStatus(cursorPos, ImVec2(6.0f, iconSize), driveStatus.isSpinning);
        auto title = driveStatus.isMounted()
                         ? std::filesystem::path(driveStatus.assetPath).filename().stem().string()
                         : "No Disk";

        cursorPos.x += style.ItemSpacing.x;
        cursorPos.y += (iconSize - ImGui::GetFontSize()) * 0.5f;

        drawList->AddText(cursorPos, (ImU32)ImColor(uiColor), title.c_str());

        ImGui::PopClipRect();

        if (driveStatus.isMounted()) {
            rbCursorPos = tailCursorPos;
            ImVec2 buttonSize(rightButtonSize, rightButtonSize);
            cursorPos.x = rbCursorPos.x + style.ItemInnerSpacing.x;
            cursorPos.y = anchorCursorPos.y + style.FramePadding.y;
            ImGui::SetCursorScreenPos(cursorPos);
            if (ClemensHostImGui::IconButton(
                    "Eject", ClemensHostStyle::getImTextureOfAsset(ClemensHostAssets::kEjectDisk),
                    buttonSize)) {
                if (isBackendRunning()) {
                    backendQueue_.ejectDisk(driveType);
                } else {
                    config_.gs.diskImagePaths[driveType].clear();
                    config_.setDirty();
                }
            }

            if (shortForm) {
                cursorPos.x = rbCursorPos.x + style.ItemInnerSpacing.x;
                cursorPos.y += rightButtonSize + style.ItemSpacing.y;
            } else {
                cursorPos.x += rightSectionWidth;
            }

            if (isBackendRunning()) {
                //  TODO: we don't persist write protected status in the configuration, so there's
                //        nowhere to persist this attribute until the machine is on.
                //        yes, 2IMG and WOZ do persist the write protect status but DSK,DO,PO do
                //        not.
                if (driveStatus.isWriteProtected) {
                    ImGui::PushStyleColor(ImGuiCol_Button,
                                          ClemensHostStyle::getWidgetToggleOnColor(*this));
                } else {
                    ImGui::PushStyleColor(ImGuiCol_Button,
                                          ClemensHostStyle::getWidgetToggleOffColor(*this));
                }

                ImGui::SetCursorScreenPos(cursorPos);
                if (ClemensHostImGui::IconButton(
                        "Lock", ClemensHostStyle::getImTextureOfAsset(ClemensHostAssets::kLockDisk),
                        buttonSize)) {
                    backendQueue_.writeProtectDisk(driveType, !driveStatus.isWriteProtected);
                }

                ImGui::PopStyleColor();
            }
        }
    }
    ImGui::EndGroup();
    ImGui::PopID();
    if (ImGui::IsItemHovered(ImGuiHoveredFlags_DelayNormal)) {
        if (driveStatus.isMounted()) {
            ImGui::SetTooltip("%s (%s)", sDriveDescriptionShort[driveType],
                              driveStatus.assetPath.c_str());
        } else {
            ImGui::SetTooltip("%s", sDriveDescription[driveType]);
        }
    }
}

void ClemensFrontend::doMachineSmartDriveStatus(unsigned driveIndex, float width,
                                                bool allowSelect) {
    // TODO: Crazy code duplication with doMachineDiskStatus
    //       The only difference is the source status object which comes from the
    //       driveIndex vs driveType.   Perhaps pass one or the other in to a
    //       common doMachineDriveStatus() call and handle smart port vs plain
    //       floppy disk tray UI
    ClemensDiskDriveStatus driveStatus{};

    if (isBackendRunning()) {
        driveStatus = frameReadState_.frame.smartPortStatuses[driveIndex];
    } else {
        driveStatus.assetPath = config_.gs.smartPortImagePaths[driveIndex];
    }
    bool isDisabled = isBackendRunning() && !driveStatus.isMounted();
    float widgetAlpha = isDisabled ? 0.2f : 1.0f;

    const ImGuiStyle &style = ImGui::GetStyle();
    const bool shortForm = !config_.hybridInterfaceEnabled;
    const float height = shortForm ? 48.0f : 32.0f;
    const float iconSize = shortForm ? 32.0f : 24.0f;
    const auto clemensDiskIcon = ClemensHostAssets::kDiskHDD;
    ImVec4 uiColor;

    char driveName[32];
    snprintf(driveName, sizeof(driveName), "SmartHDD%u", driveIndex);

    ImGui::PushID(driveName);
    ImGui::BeginGroup();
    {
        const float rightButtonSize = 16;
        const float rightSectionWidth = (rightButtonSize + style.ItemInnerSpacing.x * 2);

        ImVec2 cursorPos = ImGui::GetCursorScreenPos();
        ImVec2 anchorCursorPos = cursorPos;
        ImVec2 tailCursorPos = cursorPos;

        // body
        if (shortForm) {
            tailCursorPos.x += width - style.FramePadding.x - rightSectionWidth;
        } else {
            tailCursorPos.x += width - style.FramePadding.x - rightSectionWidth * 2;
        }
        tailCursorPos.y += height;

        ImVec2 buttonSize(tailCursorPos.x - anchorCursorPos.x, tailCursorPos.y - anchorCursorPos.y);

        if (browseSmartDriveIndex_.has_value() || !allowSelect) {
            ImGui::Dummy(buttonSize);
        } else if (ImGui::InvisibleButton("disk", buttonSize)) {
            if (!browseSmartDriveIndex_.has_value() || *browseSmartDriveIndex_ != driveIndex) {
                browseSmartDriveIndex_ = driveIndex;
            } else {
                browseSmartDriveIndex_ = std::nullopt;
            }
            browseDriveType_ = std::nullopt;
            if (browseSmartDriveIndex_.has_value()) {
                if (driveStatus.isMounted()) {
                    assetBrowser_.setCurrentDirectory(
                        std::filesystem::path(driveStatus.assetPath).parent_path().string());
                } else {
                    assetBrowser_.setCurrentDirectory(std::filesystem::current_path().string());
                }
                assetBrowser_.setDiskType(ClemensDiskAsset::DiskHDD);
            }
        }

        ImGui::PushClipRect(anchorCursorPos, tailCursorPos, false);

        ImDrawList *drawList = ImGui::GetWindowDrawList();

        if (ImGui::IsItemActive() && allowSelect) {
            uiColor = style.Colors[ImGuiCol_ButtonActive];
        } else if (ImGui::IsItemHovered() && allowSelect) {
            uiColor = style.Colors[ImGuiCol_ButtonHovered];
        } else {
            uiColor = style.Colors[ImGuiCol_Button];
        }
        uiColor.w = widgetAlpha;
        if (browseSmartDriveIndex_.has_value() && *browseSmartDriveIndex_ == driveIndex) {
            uiColor.w *= uiFlashAlpha_;
        }
        drawList->AddRectFilled(cursorPos, tailCursorPos, (ImU32)ImColor(uiColor));

        //  icon
        auto texId = ClemensHostStyle::getImTextureOfAsset(clemensDiskIcon);
        ImVec2 rbCursorPos;
        cursorPos.y += (height - iconSize) * 0.5f;
        cursorPos.x += style.FramePadding.x;
        rbCursorPos.x = cursorPos.x + iconSize;
        rbCursorPos.y = cursorPos.y + iconSize;

        uiColor = ImVec4(1.0f, 1.0f, 1.0f, widgetAlpha);
        drawList->AddImage(texId, cursorPos, rbCursorPos, ImVec2(0.0f, 0.0f), ImVec2(1.0f, 1.0f),
                           ImColor(uiColor));

        cursorPos.x += iconSize + style.ItemInnerSpacing.x;
        doMachineDiskMotorStatus(cursorPos, ImVec2(6.0f, iconSize), driveStatus.isSpinning);
        auto title = driveStatus.isMounted()
                         ? std::filesystem::path(driveStatus.assetPath).filename().stem().string()
                         : "No Disk";

        cursorPos.x += style.ItemSpacing.x;
        cursorPos.y += (iconSize - ImGui::GetFontSize()) * 0.5f;

        drawList->AddText(cursorPos, ImColor(uiColor), title.c_str());

        ImGui::PopClipRect();

        if (driveStatus.isMounted() && allowSelect) {
            rbCursorPos = tailCursorPos;
            ImVec2 buttonSize(rightButtonSize, rightButtonSize);
            cursorPos.x = rbCursorPos.x + style.ItemInnerSpacing.x;
            cursorPos.y = anchorCursorPos.y + style.FramePadding.y;
            ImGui::SetCursorScreenPos(cursorPos);
            if (ClemensHostImGui::IconButton(
                    "Eject", ClemensHostStyle::getImTextureOfAsset(ClemensHostAssets::kEjectDisk),
                    buttonSize)) {
                if (isBackendRunning()) {
                    backendQueue_.ejectSmartPortDisk(driveIndex);
                } else {
                    config_.gs.smartPortImagePaths[driveIndex].clear();
                    config_.setDirty();
                }
            }
        }
    }
    ImGui::EndGroup();
    ImGui::PopID();
    if (ImGui::IsItemHovered(ImGuiHoveredFlags_DelayNormal)) {
        if (driveStatus.isMounted()) {
            ImGui::SetTooltip("Smartport HDD (%u) %s", driveIndex, driveStatus.assetPath.c_str());
        } else {
            ImGui::SetTooltip("Smartport HDD");
        }
    }
}

void ClemensFrontend::doMachineDiskMotorStatus(const ImVec2 &pos, const ImVec2 &size,
                                               bool isSpinning) {
    const ImColor kRed(255, 0, 0, 255);
    const ImColor kDark(64, 64, 64, 255);
    auto lt = ImVec2(pos.x, pos.y);
    auto rb = ImVec2(pos.x + size.x, pos.y + size.y);
    ImDrawList *drawList = ImGui::GetWindowDrawList();
    if (isSpinning) {
        drawList->AddRectFilled(lt, rb, (ImU32)kRed, 2.0f);
    } else {
        drawList->AddRectFilled(lt, rb, (ImU32)kDark, 2.0f);
    }
}

void ClemensFrontend::doMachineDiskBrowserInterface(ImVec2 anchor, ImVec2 dimensions) {
    if (!browseDriveType_.has_value())
        return;

    //  all input focus goes to this interface
    lostFocus();

    char title[64];
    snprintf(title, sizeof(title), "Select %s (%s)", sDriveDescriptionShort[*browseDriveType_],
             ClemensDiskUtilities::getDriveName(*browseDriveType_).data());

    ImGui::SetNextWindowPos(anchor);
    ImGui::SetNextWindowSize(dimensions);
    ImGui::Begin(title, NULL,
                 ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoCollapse);
    ImGui::Spacing();
    assetBrowser_.frame(ImGui::GetContentRegionAvail());
    if (assetBrowser_.isDone()) {
        if (assetBrowser_.isSelected()) {
            auto assetPath = assetBrowser_.getCurrentPathname();
            bool success = true;
            if (assetBrowser_.isSelectedFilePathNewFile()) {
                std::vector<uint8_t> decodeBuffer;
                decodeBuffer.resize(4 * 1024 * 1024);
                //  generated target image
                auto imageBuffer = cinek::Range<uint8_t>(decodeBuffer.data(),
                                                         decodeBuffer.data() + decodeBuffer.size());
                success =
                    ClemensDiskUtilities::createDisk(imageBuffer, assetPath, *browseDriveType_);
                if (success) {
                    spdlog::info("ClemensFrontend - disk {}://{} created",
                                 ClemensDiskUtilities::getDriveName(*browseDriveType_), assetPath);
                } else {
                    doModalError("Unable to create disk at {}", assetPath);
                }
            }
            if (success) {
                if (isBackendRunning()) {
                    backendQueue_.insertDisk(*browseDriveType_, assetPath);
                } else {
                    config_.gs.diskImagePaths[*browseDriveType_] = assetPath;
                    config_.setDirty();
                }
            }
        }
        browseDriveType_ = std::nullopt;
    }
    ImGui::End();
}

void ClemensFrontend::doMachineSmartDiskBrowserInterface(ImVec2 anchor, ImVec2 dimensions) {
    if (!browseSmartDriveIndex_.has_value())
        return;

    //  all input focus goes to this interface
    lostFocus();

    char title[64];
    snprintf(title, sizeof(title), "Select Smartport Disk #%u", *browseSmartDriveIndex_);

    ImGui::SetNextWindowPos(anchor);
    ImGui::SetNextWindowSize(dimensions);
    ImGui::Begin("Select Disk", NULL,
                 ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoCollapse);
    ImGui::Spacing();
    assetBrowser_.frame(ImGui::GetContentRegionAvail());
    if (assetBrowser_.isDone()) {
        if (assetBrowser_.isSelected()) {
            auto assetPath = assetBrowser_.getCurrentPathname();
            bool success = true;
            if (assetBrowser_.isSelectedFilePathNewFile()) {
                auto blockCount = (assetBrowser_.getFileSize() + 511) / 512;
                success = blockCount > 0 && ClemensDiskUtilities::createProDOSHardDisk(
                                                assetPath, blockCount) == blockCount;
                if (success) {
                    spdlog::info("ClemensFrontend - smart {}://{} created", *browseSmartDriveIndex_,
                                 assetPath);
                } else {
                    doModalError("Unable to create disk at {}", assetPath);
                }
            }
            if (success) {
                if (isBackendRunning()) {
                    backendQueue_.insertSmartPortDisk(*browseSmartDriveIndex_, assetPath);
                } else {
                    config_.gs.smartPortImagePaths[*browseSmartDriveIndex_] = assetPath;
                    config_.setDirty();
                }
            }
        }
        browseSmartDriveIndex_ = std::nullopt;
    }
    ImGui::End();
}

template <typename... Args> void ClemensFrontend::doModalError(const char *msg, Args... args) {
    messageModalString_ = fmt::format(msg, args...);
    spdlog::error("ClemensFrontend - doModalError() {}\n", messageModalString_);
}

#define CLEM_HOST_GUI_CPU_PINS_COLOR(_field_)                                                      \
    lastFrameCPUPins_._field_ != frameReadState_.cpu.pins._field_                                  \
        ? getModifiedColor(frameReadState_.cpu.pins._field_, frameReadState_.isRunning)            \
        : getDefaultColor(frameReadState_.cpu.pins._field_)

#define CLEM_HOST_GUI_CPU_PINS_INV_COLOR(_field_)                                                  \
    lastFrameCPUPins_._field_ != frameReadState_.cpu.pins._field_                                  \
        ? getModifiedColor(!frameReadState_.cpu.pins._field_, frameReadState_.isRunning)           \
        : getDefaultColor(!frameReadState_.cpu.pins._field_)

#define CLEM_HOST_GUI_CPU_FIELD_COLOR(_field_)                                                     \
    lastFrameCPURegs_._field_ != frameReadState_.cpu.regs._field_                                  \
        ? getModifiedColor(true, frameReadState_.isRunning)                                        \
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
                                                    kClemensCPUStatus_Negative,
                                                    frameReadState_.isRunning),
                       "n");
    ImGui::SameLine(0.0f, 4.0f);
    ImGui::TextColored(getStatusFieldColor<uint8_t>(lastFrameCPURegs_.P, frameReadState_.cpu.regs.P,
                                                    kClemensCPUStatus_Overflow,
                                                    frameReadState_.isRunning),
                       "v");
    ImGui::SameLine(0.0f, 4.0f);
    ImGui::TextColored(getStatusFieldColor<uint8_t>(lastFrameCPURegs_.P, frameReadState_.cpu.regs.P,
                                                    kClemensCPUStatus_MemoryAccumulator,
                                                    frameReadState_.isRunning),
                       frameReadState_.cpu.pins.emulation ? " " : "m");
    ImGui::SameLine(0.0f, 4.0f);
    ImGui::TextColored(getStatusFieldColor<uint8_t>(lastFrameCPURegs_.P, frameReadState_.cpu.regs.P,
                                                    kClemensCPUStatus_Index,
                                                    frameReadState_.isRunning),
                       frameReadState_.cpu.pins.emulation ? " " : "x");
    ImGui::SameLine(0.0f, 4.0f);
    ImGui::TextColored(getStatusFieldColor<uint8_t>(lastFrameCPURegs_.P, frameReadState_.cpu.regs.P,
                                                    kClemensCPUStatus_Decimal,
                                                    frameReadState_.isRunning),
                       "d");
    ImGui::SameLine(0.0f, 4.0f);
    ImGui::TextColored(getStatusFieldColor<uint8_t>(lastFrameCPURegs_.P, frameReadState_.cpu.regs.P,
                                                    kClemensCPUStatus_IRQDisable,
                                                    frameReadState_.isRunning),
                       "i");
    ImGui::SameLine(0.0f, 4.0f);
    ImGui::TextColored(getStatusFieldColor<uint8_t>(lastFrameCPURegs_.P, frameReadState_.cpu.regs.P,
                                                    kClemensCPUStatus_Zero,
                                                    frameReadState_.isRunning),
                       "z");
    ImGui::SameLine(0.0f, 4.0f);
    ImGui::TextColored(getStatusFieldColor<uint8_t>(lastFrameCPURegs_.P, frameReadState_.cpu.regs.P,
                                                    kClemensCPUStatus_Carry,
                                                    frameReadState_.isRunning),
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
        backendQueue_.debugMemoryPage((uint8_t)(bank & 0xff));
    }
    debugMemoryEditor_.OptAddrDigitsCount = 4;
    debugMemoryEditor_.Cols = 8;
    debugMemoryEditor_.DrawContents(this, CLEM_IIGS_BANK_SIZE, (size_t)(bank) << 16);
}

ImU8 ClemensFrontend::imguiMemoryEditorRead(const ImU8 *mem_ptr, size_t off) {
    const auto *self = reinterpret_cast<const ClemensFrontend *>(mem_ptr);
    if (!self->frameReadState_.memoryView)
        return 0x00;
    return self->frameReadState_.memoryView[off & 0xffff];
}

void ClemensFrontend::imguiMemoryEditorWrite(ImU8 *mem_ptr, size_t off, ImU8 value) {
    auto *self = reinterpret_cast<ClemensFrontend *>(mem_ptr);
    self->backendQueue_.debugMemoryWrite((uint16_t)(off & 0xffff), value);
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
            ImGui::TextColored(col, "%c", (flags & CLEM_ENSONIQ_OSC_FLAG_CYCLE) ? 'C' : ' ');
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

void ClemensFrontend::doMachineDebugVGCDisplay() {
    //  display ClemensVideo for current video mode, which may be mixed
    //      inspect frameReadState.textFrame for lores, hires or text modes
    //      inspect frameReadState.graphicsFrame for the various modes
    //
    //  information to communicate
    //      1. Super Hires Mode
    //          - View Scanline Info from Scanline A to B (DragRangeInt2)
    //          - Draw table of scanline data where each row has the scanline info
    //            plus visual palette info
    //          - Row: IRQ, 640/320, ColorFill, Palette Index, Palette 0-15 RGB boxes

    auto &graphics = frameReadState_.frame.graphics;
    // auto &text = frameReadState_.textFrame;

    if (graphics.format == kClemensVideoFormat_Super_Hires) {
        ImGui::LabelText("Mode", "Super Hires");
        ImGui::DragIntRange2("scanlines", &vgcDebugMinScanline_, &vgcDebugMaxScanline_, 1,
                             graphics.scanline_start, graphics.scanline_count, "Start: %d",
                             "End: %d");
    }
}

void ClemensFrontend::doMachineDebugIORegister(uint8_t *ioregsold, uint8_t *ioregs, uint8_t reg) {
    auto &desc = sDebugIODescriptors[reg];
    bool changed = ioregsold[reg] != ioregs[reg];
    bool tooltip = false;
    ImGui::TableNextColumn();
    ImGui::PushStyleColor(ImGuiCol_Text,
                          (ImU32)(changed ? getModifiedColor(true, frameReadState_.isRunning)
                                          : getDefaultColor(true)));
    ImGui::TextUnformatted(desc.readLabel);
    ImGui::PopStyleColor();
    tooltip = tooltip || ImGui::IsItemHovered();
    ImGui::TableNextColumn();
    ImGui::TextColored(changed ? getModifiedColor(true, frameReadState_.isRunning)
                               : getDefaultColor(true),
                       "%04X", 0xc000 + reg);
    tooltip = tooltip || ImGui::IsItemHovered();
    ImGui::TableNextColumn();
    ImGui::TextColored(changed ? getModifiedColor(true, frameReadState_.isRunning)
                               : getDefaultColor(true),
                       "%02X", ioregs[reg]);
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
    fieldColor = CLEM_HOST_GUI_IWM_STATUS_CHANGED(kIWMStatusDrive35)
                     ? getModifiedColor(driveOn, frameReadState_.isRunning)
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
    fieldColor = CLEM_HOST_GUI_IWM_STATUS_CHANGED(kIWMStatusDriveAlt)
                     ? getModifiedColor(driveOn, frameReadState_.isRunning)
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
    fieldColor = CLEM_HOST_GUI_IWM_STATUS_CHANGED(kIWMStatusIWMQ6)
                     ? getModifiedColor(true, frameReadState_.isRunning)
                     : getDefaultColor(true);
    ImGui::TextColored(fieldColor, "Q6");
    ImGui::TableNextColumn();
    ImGui::TextColored(fieldColor, "%d", (iwmState.status & kIWMStatusIWMQ6) ? 1 : 0);
    ImGui::TableNextRow();
    ImGui::TableNextColumn();
    fieldColor = CLEM_HOST_GUI_IWM_STATUS_CHANGED(kIWMStatusIWMQ7)
                     ? getModifiedColor(true, frameReadState_.isRunning)
                     : getDefaultColor(true);
    ImGui::TextColored(fieldColor, "Q7");
    ImGui::TableNextColumn();
    ImGui::TextColored(fieldColor, "%d", (iwmState.status & kIWMStatusIWMQ7) ? 1 : 0);
    ImGui::TableNextRow();
    fieldColor = lastFrameIORegs_[CLEM_MMIO_REG_IWM_Q6_LO] != ioregs[CLEM_MMIO_REG_IWM_Q6_LO]
                     ? getModifiedColor(true, frameReadState_.isRunning)
                     : getDefaultColor(true);
    ImGui::TableNextColumn();
    ImGui::TextColored(fieldColor, "Read");
    ImGui::TableNextColumn();
    ImGui::TextColored(fieldColor, "%02X", ioregs[CLEM_MMIO_REG_IWM_Q6_LO]);
    ImGui::TableNextRow();
    fieldColor = lastFrameIWM_.data != iwmState.data
                     ? getModifiedColor(driveOn, frameReadState_.isRunning)
                     : getDefaultColor(driveOn);
    ImGui::TableNextColumn();
    ImGui::TextColored(fieldColor, "Data");
    ImGui::TableNextColumn();
    ImGui::TextColored(fieldColor, "%02X", iwmState.data);
    ImGui::TableNextRow();
    fieldColor = lastFrameIWM_.latch != iwmState.latch
                     ? getModifiedColor(driveOn, frameReadState_.isRunning)
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
    fieldColor = CLEM_HOST_GUI_IWM_STATUS_CHANGED(kIWMStatusDriveSpin)
                     ? getModifiedColor(driveOn, frameReadState_.isRunning)
                     : getDefaultColor(driveOn);
    ImGui::TextColored(fieldColor, "Motor");
    ImGui::TableNextColumn();
    ImGui::TextColored(fieldColor, "%s", (iwmState.status & kIWMStatusDriveSpin) ? "on" : "off");
    ImGui::TableNextRow();
    ImGui::TableNextColumn();
    fieldColor = lastFrameIWM_.ph03 != iwmState.ph03
                     ? getModifiedColor(driveOn, frameReadState_.isRunning)
                     : getDefaultColor(driveOn);
    ImGui::PushStyleColor(ImGuiCol_Text, (ImU32)fieldColor);
    ImGui::TextUnformatted("Phase");
    ImGui::TableNextColumn();
    ImGui::Text("%u%u%u%u", (iwmState.ph03 & 1) ? 1 : 0, (iwmState.ph03 & 2) ? 1 : 0,
                (iwmState.ph03 & 4) ? 1 : 0, (iwmState.ph03 & 8) ? 1 : 0);
    ImGui::PopStyleColor();
    ImGui::TableNextRow();
    fieldColor = CLEM_HOST_GUI_IWM_STATUS_CHANGED(kIWMStatusDriveSel)
                     ? getModifiedColor(driveOn, frameReadState_.isRunning)
                     : getDefaultColor(driveOn);
    ImGui::PushStyleColor(ImGuiCol_Text, (ImU32)fieldColor);
    ImGui::TableNextColumn();
    ImGui::TextUnformatted("HeadSel");
    ImGui::TableNextColumn();
    ImGui::Text("%d", iwmState.status & kIWMStatusDriveSel ? 1 : 0);
    ImGui::PopStyleColor();
    ImGui::TableNextRow();
    fieldColor = CLEM_HOST_GUI_IWM_STATUS_CHANGED(kIWMStatusDriveWP)
                     ? getModifiedColor(driveOn, frameReadState_.isRunning)
                     : getDefaultColor(driveOn);
    ImGui::PushStyleColor(ImGuiCol_Text, (ImU32)fieldColor);
    ImGui::TableNextColumn();
    ImGui::TextUnformatted("Sense");
    ImGui::TableNextColumn();
    ImGui::Text("%d", iwmState.status & kIWMStatusDriveWP ? 1 : 0);
    ImGui::PopStyleColor();
    ImGui::TableNextRow();
    fieldColor = lastFrameIWM_.qtr_track_index != iwmState.qtr_track_index
                     ? getModifiedColor(driveOn, frameReadState_.isRunning)
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
                     ? getModifiedColor(driveOn, frameReadState_.isRunning)
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

    ImVec4 fieldColor;
    ImGui::TableNextColumn();
    ImGui::BeginTable("PARAMS", 2, ImGuiTableFlags_SizingFixedFit);
    ImGui::TableSetupColumn("Name", ImGuiTableColumnFlags_WidthFixed, fontCharSize * 8);
    ImGui::TableSetupColumn("Value", ImGuiTableColumnFlags_WidthFixed, fontCharSize * 6);
    ImGui::TableHeadersRow();
    ImGui::TableNextColumn();
    if (lastFrameADBStatus_.mouse_reg[0] != frameReadState_.adb.mouse_reg[0]) {
        fieldColor = getModifiedColor(true, frameReadState_.isRunning);
    } else {
        fieldColor = getDefaultColor(true);
    }
    ImGui::TextColored(fieldColor, "ADB.M0");
    ImGui::TableNextColumn();
    ImGui::TextColored(fieldColor, "%04x", frameReadState_.adb.mouse_reg[0]);
    ImGui::TableNextRow();
    ImGui::TableNextColumn();
    if (lastFrameADBStatus_.mouse_reg[0] != frameReadState_.adb.mouse_reg[0]) {
        fieldColor = getModifiedColor(true, frameReadState_.isRunning);
    } else {
        fieldColor = getDefaultColor(true);
    }
    ImGui::TextColored(fieldColor, "ADB.M3");
    ImGui::TableNextColumn();
    ImGui::TextColored(fieldColor, "%04x", frameReadState_.adb.mouse_reg[3]);
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

void ClemensFrontend::doDebugView(ImVec2 anchor, ImVec2 size) {
    auto *state = frameReadState_.e1bank;
    ImGui::SetNextWindowPos(anchor, ImGuiCond_Once);
    ImGui::SetNextWindowSize(size);
    ImGui::PushStyleColor(ImGuiCol_WindowBg, IM_COL32(0, 0, 0, 128));
    if (ImGui::Begin("Debug View", NULL)) {

        ImGui::BeginTable("Diagnostics", 2);
        {
            ImGui::TableNextRow();
            ImGui::TableNextColumn();
            ImGui::TextUnformatted("Mouse.Host");
            ImGui::TableNextColumn();
            ImGui::Text("(%d,%d)", diagnostics_.mouseX, diagnostics_.mouseY);
            if (state) {
                ImGui::TableNextRow();
                ImGui::TableNextColumn();
                ImGui::TextUnformatted("Mouse.E1ROM");
                ImGui::TableNextColumn();
                ImGui::Text("(%u,%u)", ((uint16_t)(state[0x192]) << 8) | state[0x190],
                            ((uint16_t)(state[0x193]) << 8) | state[0x191]);
            }
        }
        ImGui::EndTable();
    }
    ImGui::End();
    ImGui::PopStyleColor();
}

void ClemensFrontend::doSetupUI(ImVec2 anchor, ImVec2 dimensions) {
    ImGui::SetNextWindowPos(anchor);
    ImGui::SetNextWindowSize(dimensions);
    ImGui::Begin("Settings", NULL,
                 ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoCollapse);
    ImGui::Spacing();
    settingsView_.frame();
    ImGui::End();
}

void ClemensFrontend::doMachineViewLayout(ImVec2 rootAnchor, ImVec2 rootSize,
                                          const ViewToMonitorTranslation &viewToMonitor) {
    ImGui::SetNextWindowPos(rootAnchor);
    ImGui::SetNextWindowSize(rootSize);
    if (emulatorHasKeyboardFocus_) {
        // Focus for the next frame will be evaluated inside the window block
        // Here we want the emulator to intercept all keyboard input
        ImGui::SetNextWindowFocus();
        emulatorHasKeyboardFocus_ = false;
    }
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
    ImGui::Begin("Display", nullptr,
                 ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoBringToFrontOnFocus |
                     ImGuiWindowFlags_NoMove);
    ImGui::BeginChild("DisplayView");

    ImTextureID texId{(void *)((uintptr_t)display_.getScreenTarget().id)};
    ImVec2 p = ImGui::GetCursorScreenPos();
    ImVec2 contentSize = ImGui::GetContentRegionAvail();
    ImVec2 monitorSize(contentSize.y * kClemensAspectRatio, contentSize.y);
    if (contentSize.x < monitorSize.x) {
        monitorSize.x = contentSize.x;
        monitorSize.y = contentSize.x / kClemensAspectRatio;
    }
    ImVec2 monitorAnchor(p.x + (contentSize.x - monitorSize.x) * 0.5f,
                         p.y + (contentSize.y - monitorSize.y) * 0.5f);
    float screenV0 = 0.0f, screenV1 = viewToMonitor.screenUVs.y;
#if defined(CK3D_BACKEND_GL)
    // Flip the texture coords so that the top V is 1.0f
    screenV0 = 1.0f;
    screenV1 = 1.0f - screenV1;
#endif

    ImVec4 tint_col = ImVec4(1.0f, 1.0f, 1.0f, 1.0f); // No tint
    ImGui::GetWindowDrawList()->AddImage(
        texId, ImVec2(monitorAnchor.x, monitorAnchor.y),
        ImVec2(monitorAnchor.x + monitorSize.x, monitorAnchor.y + monitorSize.y),
        ImVec2(0, screenV0), ImVec2(viewToMonitor.screenUVs.x, screenV1),
        ImGui::GetColorU32(tint_col));

    emulatorHasKeyboardFocus_ = emulatorHasMouseFocus_;
    if (ImGui::IsWindowFocused()) {
        emulatorHasKeyboardFocus_ = true;
    }
    emulatorHasKeyboardFocus_ = emulatorHasKeyboardFocus_ || ImGui::IsWindowHovered();

    //  translate mouse position to emulated screen coordinates
    auto mousePos = ImGui::GetMousePos();
    float viewToMonitorScalarX = viewToMonitor.size.x / monitorSize.x;
    float viewToMonitorScalarY = viewToMonitor.size.y / monitorSize.y;
    float mouseX = (mousePos.x - monitorAnchor.x) * viewToMonitorScalarX;
    float mouseY = (mousePos.y - monitorAnchor.y) * viewToMonitorScalarY;
    float mouseToViewDX = (viewToMonitor.size.x - viewToMonitor.workSize.x) * 0.5f;
    float mouseToViewDY = (viewToMonitor.size.y - viewToMonitor.workSize.y) * 0.5f;

    diagnostics_.mouseX =
        std::floor(std::clamp(mouseX - mouseToViewDX, 0.0f, viewToMonitor.workSize.x));
    diagnostics_.mouseY =
        std::floor(std::clamp(mouseY - mouseToViewDY, 0.0f, viewToMonitor.workSize.y));

    if (!emulatorHasMouseFocus_) {
        ClemensInputEvent mouseEvt;
        mouseEvt.type = kClemensInputType_MouseMoveAbsolute;
        mouseEvt.value_a = int16_t(diagnostics_.mouseX);
        mouseEvt.value_b = int16_t(diagnostics_.mouseY);
        backendQueue_.inputEvent(mouseEvt);
        if (ImGui::GetIO().MouseClicked[0] && ImGui::IsWindowHovered()) {
            mouseEvt.type = kClemensInputType_MouseButtonDown;
            mouseEvt.value_a = 0x01;
            mouseEvt.value_b = 0x01;
            backendQueue_.inputEvent(mouseEvt);
        } else if (ImGui::GetIO().MouseReleased[0]) {
            mouseEvt.type = kClemensInputType_MouseButtonUp;
            mouseEvt.value_a = 0x01;
            mouseEvt.value_b = 0x01;
            backendQueue_.inputEvent(mouseEvt);
        }        
    }

    ImGui::EndChild();
    ImGui::End();
    ImGui::PopStyleVar();
}

void ClemensFrontend::doMachineInfoBar(ImVec2 rootAnchor, ImVec2 rootSize) {
    //  Display Power Status, Disk Drive Status, SmartPort Status, MouseLock,
    //  Key Focus
    const ImColor kGreen(0, 255, 0, 255);
    const ImColor kDark(64, 64, 64, 255);
    const uint8_t *bram = frameReadState_.bram;

    ImGui::SetNextWindowPos(rootAnchor);
    ImGui::SetNextWindowSize(rootSize);
    ImGui::Begin("InfoBar", nullptr, ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove);
    ImDrawList *drawList = ImGui::GetWindowDrawList();
    ImVec2 screenPos;
    ImVec2 cursorPos;
    float lineHeight = ImGui::GetTextLineHeightWithSpacing();
    float circleHeight = ImGui::GetTextLineHeight() * 0.5f;
    ImGui::TextUnformatted("1Mhz: ");
    ImGui::SameLine(0.0f, 0.0f);
    screenPos = ImGui::GetCursorScreenPos();
    screenPos.y += lineHeight * 0.5f;
    if (!bram || bram[CLEM_RTC_BRAM_SYSTEM_SPEED] != 0x00) {
        drawList->AddCircleFilled(screenPos, circleHeight, kDark);
    } else {
        drawList->AddCircleFilled(screenPos, circleHeight, kGreen);
    }
    cursorPos.x = ImGui::GetCursorStartPos().x + rootSize.x * 0.20f;
    cursorPos.y = ImGui::GetCursorPosY();
    ImGui::SameLine(cursorPos.x, 0.0f);

    doViewInputInstructions(ImVec2(ImGui::GetWindowContentRegionWidth() - cursorPos.x, rootSize.y));

    ImGui::End();
}

void ClemensFrontend::doViewInputInstructions(ImVec2 dimensions) {
    const char *infoText = nullptr;
    if (emulatorHasMouseFocus_) {
        infoText = ClemensL10N::kMouseUnlock[ClemensL10N::kLanguageDefault];
    } else if (!emulatorHasKeyboardFocus_) {
        infoText = ClemensL10N::kViewInput[ClemensL10N::kLanguageDefault];
    } else {
        infoText = "";
    }

    ImVec2 anchor = ImGui::GetCursorScreenPos();
    ImColor color = ImGui::GetStyleColorVec4(ImGuiCol_Text);
    ImGui::Dummy(dimensions);
    ImVec2 size = ImGui::GetFont()->CalcTextSizeA(ImGui::GetFontSize(), FLT_MAX, 0.0f, infoText);
    // anchor.x += std::max(0.0f, (dimensions.x - size.x) * 0.5f);
    anchor.y += std::max(0.0f, (dimensions.y - size.y) * 0.5f);
    ImGui::GetWindowDrawList()->AddText(anchor, (ImU32)color, infoText);
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

void ClemensFrontend::doHelpScreen(int width, int height) {
    static bool isOpen = false;
    if (!ImGui::IsPopupOpen("Clemens IIGS Help")) {
        ImGui::OpenPopup("Clemens IIGS Help");
        isOpen = true;
    }
    ImVec2 center = ImGui::GetMainViewport()->GetCenter();
    ImGui::SetNextWindowPos(center, ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
    ImGui::SetNextWindowSize(
        ImVec2(std::max(720.0f, width * 0.80f), std::max(512.0f, height * 0.80f)));
    if (ImGui::BeginPopupModal("Clemens IIGS Help", &isOpen)) {
        if (ImGui::BeginTabBar("HelpSections")) {
            if (ImGui::BeginTabItem("Summary")) {
                ClemensHostImGui::Markdown(CLEM_L10N_LABEL(kEmulatorHelp));
                ImGui::EndTabItem();
            }
            if (ImGui::BeginTabItem("Hotkeys")) {
                ClemensHostImGui::Markdown(CLEM_L10N_LABEL(kGSKeyboardCommands));
                ImGui::EndTabItem();
            }
            if (ImGui::BeginTabItem("Disk Selection")) {
                ClemensHostImGui::Markdown(CLEM_L10N_LABEL(kDiskSelectionHelp));
                ImGui::EndTabItem();
            }
            if (ImGui::BeginTabItem("Debugger")) {
                ClemensHostImGui::Markdown(CLEM_L10N_LABEL(kDebuggerHelp));
                ImGui::EndTabItem();
            }
            ImGui::EndTabBar();
        }

        ImGui::EndPopup();
    }
    if (!isOpen) {
        setGUIMode(GUIMode::Emulator);
    }
}

bool ClemensFrontend::isEmulatorStarting() const {
    return guiMode_ == GUIMode::RebootEmulator || guiMode_ == GUIMode::StartingEmulator;
}

bool ClemensFrontend::isEmulatorActive() const { return !isEmulatorStarting(); }

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
    } else if (action == "shutdown") {
        cmdShutdown(operand);
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
    } else if (action == "minimode") {
        cmdMinimode(operand);
    } else {
        cmdScript(command);
    }
}

void ClemensFrontend::cmdHelp(std::string_view operand) {
    if (!operand.empty()) {
        CLEM_TERM_COUT.print(TerminalLine::Warn, "Command specific help not yet supported.");
    }
    CLEM_TERM_COUT.print(TerminalLine::Info, "shutdown                    - exit the emulator");
    CLEM_TERM_COUT.print(TerminalLine::Info,
                         "minimode                    - returns to user display mode");
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
    CLEM_TERM_COUT.print(TerminalLine::Info, "b]reak brk                  - break on IRQ");
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
    CLEM_TERM_COUT.print(
        TerminalLine::Info,
        ".{a|b|c|x|y|p|d|s|dbr|pbr|pc} = <value>      - sets a register value now\n"
        "                              = a0           - hex\n"
        "                              = #$a0         - hex alternate\n"
        "                              = 0800         - 16-bit hex\n"
        "                              = #128         - decimal\n"
        "                              = #-10         - decimal negative\n");
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
            backendQueue_.removeBreakpoint(index);
        }
        return;
    }
    if (operand == "list") {
        //  TODO: granular listing based on operand
        static const char *bpType[] = {"unknown", "execute", "data-read", "write", "IRQ", "BRK"};
        if (breakpoints_.empty()) {
            CLEM_TERM_COUT.print(TerminalLine::Info, "No breakpoints defined.");
            return;
        }
        for (size_t i = 0; i < breakpoints_.size(); ++i) {
            auto &bp = breakpoints_[i];
            if (bp.type == ClemensBackendBreakpoint::IRQ ||
                bp.type == ClemensBackendBreakpoint::BRK) {
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
        backendQueue_.addBreakpoint(breakpoint);
        return;
    } else if (operand == "brk") {
        breakpoint.type = ClemensBackendBreakpoint::BRK;
        breakpoint.address = 0x0;
        backendQueue_.addBreakpoint(breakpoint);
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
        backendQueue_.breakExecution();
    } else {
        address[6] = '\0';
        char *addressEnd = NULL;
        breakpoint.address = std::strtoul(address, &addressEnd, 16);
        if (addressEnd != address + 6) {
            CLEM_TERM_COUT.format(TerminalLine::Error, "Address format is invalid read from '{}'",
                                  operand);
            return;
        }
        backendQueue_.addBreakpoint(breakpoint);
    }
}

void ClemensFrontend::cmdRun(std::string_view /*operand*/) { backendQueue_.run(); }

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
    backendQueue_.step(count);
}

void ClemensFrontend::rebootInternal() {
    setGUIMode(GUIMode::RebootEmulator);
    delayRebootTimer_ = 0.0f;
    if (isBackendRunning()) {
        stopBackend();
        CLEM_TERM_COUT.print(TerminalLine::Info, "Rebooting machine...");
    } else {
        CLEM_TERM_COUT.print(TerminalLine::Info, "Powering machine...");
    }
}

void ClemensFrontend::cmdReboot(std::string_view /*operand*/) { rebootInternal(); }

void ClemensFrontend::cmdShutdown(std::string_view /*operand*/) {
    setGUIMode(GUIMode::ShutdownEmulator);
}

void ClemensFrontend::cmdReset(std::string_view /*operand*/) { backendQueue_.reset(); }

void ClemensFrontend::cmdDisk(std::string_view operand) {
    // disk
    // disk <drive>,file=<image>
    // disk <drive>,wprot=off|on
    if (operand.empty()) {
        for (auto it = frameReadState_.frame.diskDriveStatuses.begin();
             it != frameReadState_.frame.diskDriveStatuses.end(); ++it) {
            auto driveType =
                static_cast<ClemensDriveType>(it - frameReadState_.frame.diskDriveStatuses.begin());
            CLEM_TERM_COUT.format(TerminalLine::Info, "{} {}: {}",
                                  it->isWriteProtected ? "wp" : "  ",
                                  ClemensDiskUtilities::getDriveName(driveType),
                                  it->assetPath.empty() ? "<none>" : it->assetPath);
        }
        return;
    }
    auto sepPos = operand.find(',');
    auto driveType = ClemensDiskUtilities::getDriveType(trimToken(operand, 0, sepPos));
    if (driveType == kClemensDrive_Invalid) {
        CLEM_TERM_COUT.format(TerminalLine::Error, "Invalid drive name {} specified.", operand);
        return;
    }
    auto &driveInfo = frameReadState_.frame.diskDriveStatuses[driveType];
    std::string_view diskOpExpr;
    if (sepPos == std::string_view::npos || (diskOpExpr = trimToken(operand, sepPos + 1)).empty()) {

        CLEM_TERM_COUT.format(TerminalLine::Info, "{} {}: {}",
                              driveInfo.isWriteProtected ? "wp" : "  ",
                              ClemensDiskUtilities::getDriveName(driveType),
                              driveInfo.assetPath.empty() ? "<none>" : driveInfo.assetPath);
        return;
    }

    bool validOp = true;
    bool validValue = true;
    sepPos = diskOpExpr.find('=');
    auto diskOpType = trimToken(diskOpExpr, 0, sepPos);
    std::string_view diskOpValue;
    if (sepPos == std::string_view::npos) {
        if (diskOpType == "eject") {
            backendQueue_.ejectDisk(driveType);
        } else {
            validOp = false;
        }
    } else {
        diskOpValue = trimToken(diskOpExpr, sepPos + 1);
        if (diskOpType == "file") {
            backendQueue_.insertDisk(driveType, std::string(diskOpValue));
        } else if (diskOpType == "wprot") {
            bool wprot;
            if (parseBool(diskOpValue, wprot)) {
                backendQueue_.writeProtectDisk(driveType, wprot);
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
        CLEM_TERM_COUT.format(TerminalLine::Error,
                              "Log level '{}' is not one of the following: DEBUG, INFO, "
                              "WARN, UNIMPL or FATAL",
                              operand);
        return;
    }
    backendQueue_.debugLogLevel(int(levelName - logLevelNames.begin()));
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
    // CK_TODO(DebugMessage : Send Message to thread so it can fill in the data)
    backendQueue_.debugMessage(std::move(message));
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
    backendQueue_.debugProgramTrace(std::string(params[0]), path);
}

void ClemensFrontend::cmdSave(std::string_view operand) {
    auto [params, cmd, paramCount] = gatherMessageParams(operand);
    if (paramCount != 1) {
        CLEM_TERM_COUT.print(TerminalLine::Error, "Save requires a filename.");
        return;
    }
    backendQueue_.saveMachine(std::string(params[0]));
}

void ClemensFrontend::cmdLoad(std::string_view operand) {
    auto [params, cmd, paramCount] = gatherMessageParams(operand);
    if (paramCount != 1) {
        CLEM_TERM_COUT.print(TerminalLine::Error, "Load requires a filename.");
        return;
    }
    backendQueue_.loadMachine(std::string(params[0]));
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
    backendQueue_.inputEvent(input);
    CLEM_TERM_COUT.print(TerminalLine::Info, "Input sent.");
}

void ClemensFrontend::cmdMinimode(std::string_view operand) {
    auto [params, cmd, paramCount] = gatherMessageParams(operand);
    if (paramCount != 0) {
        CLEM_TERM_COUT.print(TerminalLine::Error, "Minimode innvalid parameters.");
        return;
    }
    config_.hybridInterfaceEnabled = false;
    config_.setDirty();
}

void ClemensFrontend::cmdScript(std::string_view command) {
    backendQueue_.runScript(std::string(command));
}
