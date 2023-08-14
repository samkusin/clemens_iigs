//  Boo...
#include "clem_configuration.hpp"
#include "clem_host.hpp"

#include <algorithm>
#include <cstdint>
#define _USE_MATH_DEFINES

#include "clem_front.hpp"

#include "clem_backend.hpp"

#include "cinek/fixedstack.hpp"
#include "clem_assets.hpp"
#include "clem_backend.hpp"
#include "clem_disk.h"
#include "clem_host_platform.h"
#include "clem_host_shared.hpp"
#include "clem_host_utils.hpp"
#include "clem_imgui.hpp"
#include "clem_l10n.hpp"
#include "clem_mmio_defs.h"
#include "clem_mmio_types.h"
#include "core/clem_apple2gs_config.hpp"
#include "core/clem_disk_asset.hpp"
#include "core/clem_disk_status.hpp"
#include "core/clem_disk_utils.hpp"
#include "emulator_mmio.h"
#include "imgui.h"
#include "spdlog/spdlog.h"
#include "version.h"

#define IMGUI_DEFINE_MATH_OPERATORS
#include "imgui_internal.h"

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

#include "cinek/equation.inl"

// #define CLEMENS_HOST_PROFILE_ENABLED

/*
namespace cinek {
template <>
void tweenProperty<ImVec2>(ImVec2 &out, const keyframe<ImVec2> &left, const keyframe<ImVec2> &right,
                           double scalar) {
    out.x = left.prop.x + (right.prop.x - left.prop.x) * scalar;
    out.y = left.prop.y + (right.prop.y - left.prop.y) * scalar;
}
} // namespace cinek
*/
//  Style
namespace ClemensHostStyle {

// monochromatic "platinum" classic CACAC8
//                          middle  969695
// monochromatic "platinum" dark    4A4A33
static ImU32 kDarkFrameColor = IM_COL32(0x4a, 0x4a, 0x33, 0xff);
static ImU32 kDarkInsetColor = IM_COL32(0x3a, 0x3a, 0x22, 0xff);
static ImU32 kWidgetToggleOnColor = IM_COL32(0xFF, 0xFF, 0xFF, 0xff);
static ImU32 kWidgetActiveColor = IM_COL32(0xBA, 0xBA, 0xB8, 0xff);
static ImU32 kWidgetHoverColor = IM_COL32(0xAA, 0xAA, 0xA8, 0xff);
static ImU32 kWidgetColor = IM_COL32(0x96, 0x96, 0x95, 0xff);
static ImU32 kWidgetToggleOffColor = IM_COL32(0x22, 0x22, 0x22, 0xff);
static ImU32 kMenuColor = IM_COL32(0x4a, 0x4a, 0x33, 0xff);

ImU32 getFrameColor(const ClemensFrontend &) { return kDarkFrameColor; }
ImU32 getInsetColor(const ClemensFrontend &) { return kDarkInsetColor; }
ImU32 getWidgetColor(const ClemensFrontend &) { return kWidgetColor; }
ImU32 getWidgetHoverColor(const ClemensFrontend &) { return kWidgetHoverColor; }
ImU32 getWidgetActiveColor(const ClemensFrontend &) { return kWidgetActiveColor; }
ImU32 getWidgetToggleOnColor(const ClemensFrontend &) { return kWidgetToggleOnColor; }
ImU32 getWidgetToggleOffColor(const ClemensFrontend &) { return kWidgetToggleOffColor; }
ImU32 getMenuColor(const ClemensFrontend &) { return kMenuColor; }

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

namespace {

//  NTSC visual "resolution"
constexpr int kClemensScreenWidth = ClemensHostStyle::kScreenWidth;
constexpr int kClemensScreenHeight = ClemensHostStyle::kScreenHeight;
constexpr float kClemensAspectRatio = float(kClemensScreenWidth) / kClemensScreenHeight;
constexpr double kMachineFrameDuration = 1.0 / 60.00;

//  Delay user-initiated reboot after reset by this amount in seconds
//  This was done in response to a bug related to frequent reboots that appears fixed
//  with the addition of GUIMode::StartingEmulator and delaying the switch to GUIMode::Emulator
//  until the first machine frame runs.
//  Kept this value here since I think it's better UX
constexpr float kClemensRebootDelayDuration = 1.0f;

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

const uint64_t ClemensFrontend::kFrameSeqNoInvalid = std::numeric_limits<uint64_t>::max();

extern "C" void clem_temp_generate_ascii_to_adb_table();

ClemensFrontend::ClemensFrontend(ClemensConfiguration &config,
                                 const cinek::ByteBuffer &systemFontLoBuffer,
                                 const cinek::ByteBuffer &systemFontHiBuffer)
    : config_(config), displayProvider_(systemFontLoBuffer, systemFontHiBuffer),
      display_(displayProvider_), audio_(), logLevel_(CLEM_DEBUG_LOG_INFO),
      dtEmulatorNextUpdateInterval_(0.0), frameSeqNo_(kFrameSeqNoInvalid),
      frameLastSeqNo_(kFrameSeqNoInvalid),
      frameReadMemory_(kFrameMemorySize, malloc(kFrameMemorySize)), lastFrameCPUPins_{},
      lastFrameCPURegs_{}, lastFrameIWM_{}, lastFrameIRQs_(0), lastFrameNMIs_(0),
      emulatorShouldHaveKeyboardFocus_(false), emulatorHasKeyboardFocus_(false),
      emulatorHasMouseFocus_(false), mouseInEmulatorScreen_(false),
      pasteClipboardToEmulator_(false), debugIOMode_(DebugIOMode::Core), vgcDebugMinScanline_(0),
      vgcDebugMaxScanline_(0), joystickSlotCount_(0), guiMode_(GUIMode::None),
      guiPrevMode_(GUIMode::None), appTime_(0.0), nextUIFlashCycleAppTime_(0.0),
      uiFlashAlpha_(1.0f), debugger_(backendQueue_, *this), settingsView_(config_) {

    ClemensTraceExecutedInstruction::initialize();

    clem_temp_generate_ascii_to_adb_table();

    initDebugIODescriptors();
    clem_joystick_open_devices(CLEM_HOST_JOYSTICK_PROVIDER_DEFAULT);

    audio_.start();
    if (config_.gs.audioSamplesPerSecond == 0) {
        config_.gs.audioSamplesPerSecond = audio_.getAudioFrequency();
    }

    snapshotRootPath_ =
        (std::filesystem::path(config_.dataDirectory) / CLEM_HOST_SNAPSHOT_DIR).string();

    spdlog::info("Clemens IIGS Emulator {}.{}", CLEM_HOST_VERSION_MAJOR, CLEM_HOST_VERSION_MINOR);

    if (config_.poweredOn) {
        rebootInternal(true);
    } else {
        setGUIMode(GUIMode::Setup);
    }
}

ClemensFrontend::~ClemensFrontend() {
    stopBackend();
    audio_.stop();
    clem_joystick_close_devices();

    config_.save();

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

bool ClemensFrontend::emulatorHasFocus() const {
    return emulatorHasMouseFocus_ || emulatorHasKeyboardFocus_;
}

void ClemensFrontend::pasteText(const char *utf8, unsigned textSizeLimit) {
    size_t textSize = strnlen(utf8, textSizeLimit);
    std::string clipboardText(utf8, textSize);
    backendQueue_.sendText(std::move(clipboardText));
}

void ClemensFrontend::lostFocus() {}

void ClemensFrontend::gainFocus() {
    emulatorHasMouseFocus_ = false;
    emulatorHasKeyboardFocus_ = false;
    if (guiMode_ == GUIMode::Emulator) {
        emulatorShouldHaveKeyboardFocus_ = true;
    }
}

void ClemensFrontend::startBackend() {
    auto romPath = config_.romFilename;

    ClemensBackendConfig backendConfig{};

    backendConfig.dataRootPath = config_.dataDirectory;
    backendConfig.imageRootPath =
        (std::filesystem::path(config_.dataDirectory) / CLEM_HOST_LIBRARY_DIR).string();
    backendConfig.snapshotRootPath = snapshotRootPath_;
    backendConfig.traceRootPath =
        (std::filesystem::path(config_.dataDirectory) / CLEM_HOST_TRACES_DIR).string();
    backendConfig.enableFastEmulation = config_.fastEmulationEnabled;
    backendConfig.logLevel = logLevel_;
    backendConfig.type = ClemensBackendConfig::Type::Apple2GS;
    backendConfig.breakpoints = debugger_.copyBreakpoints();

    backendConfig.GS = config_.gs;

    spdlog::info("Starting new emulator backend");
    config_.poweredOn = true;
    auto backend = std::make_unique<ClemensBackend>(romPath, backendConfig);
    dtEmulatorNextUpdateInterval_ = 0.0;
    backendThread_ = std::thread(&ClemensFrontend::runBackend, this, std::move(backend));
    backendQueue_.reset();
    backendQueue_.run();
}

void ClemensFrontend::runBackend(std::unique_ptr<ClemensBackend> backend) {
    ClemensCommandQueue::DispatchResult results{};
    backendState_.reset();

#ifdef CLEMENS_HOST_PROFILE_ENABLED
    std::chrono::high_resolution_clock::time_point f_start;
    std::chrono::high_resolution_clock::time_point f_end;
    auto f_cumulative = std::chrono::high_resolution_clock::duration::zero();
    unsigned f_count = 0;
#endif

    while (!results.second) {
        std::unique_lock<std::mutex> lk(frameMutex_);
        backend->post(backendState_);
        std::copy(results.first.begin(), results.first.end(),
                  std::back_inserter(lastCommandState_.results));
        frameSeqNo_++;
        readyForFrame_.wait(lk, [this]() { return frameLastSeqNo_ == frameSeqNo_; });
        lk.unlock();
#ifdef CLEMENS_HOST_PROFILE_ENABLED
        f_start = std::chrono::high_resolution_clock::now();
#endif
        results = backend->step(stagedBackendQueue_);
#ifdef CLEMENS_HOST_PROFILE_ENABLED
        f_end = std::chrono::high_resolution_clock::now();
        f_cumulative += (f_end - f_start);
        f_count++;
        if (f_count == 60) {
            auto f_avg_time = f_cumulative / f_count;
            spdlog::info("SKS: f_avg_time = {} us",
                         std::chrono::duration_cast<std::chrono::microseconds>(f_avg_time).count());

            f_cumulative = std::chrono::high_resolution_clock::duration::zero();
            f_count = 0;
        }
#endif

        // sync audio
        auto audioFrame = backend->renderAudioFrame();
        audio_.queue(audioFrame.first, audioFrame.second);
    }

    std::lock_guard<std::mutex> lk(frameMutex_);
    ClemensAppleIIGSConfig config;
    if (backend->queryConfig(config)) {
        lastCommandState_.gsConfig = config;
    } else {
        spdlog::warn("Configuration was not retrieved on termination of emulated machine!");
    }
}

void ClemensFrontend::stopBackend() {
    if (backendThread_.joinable()) {
        backendQueue_.terminate();
        syncBackend(false);
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

void ClemensFrontend::pollJoystickDevices() {
    std::array<ClemensHostJoystick, CLEM_HOST_JOYSTICK_LIMIT> joysticks;
    unsigned deviceCount = clem_joystick_poll(joysticks.data());
    unsigned joystickCount = 0;
    ClemensInputEvent inputs[2];
    constexpr int32_t kGameportAxisMagnitude = CLEM_GAMEPORT_PADDLE_AXIS_VALUE_MAX;
    constexpr int32_t kHostAxisMagnitude = CLEM_HOST_JOYSTICK_AXIS_DELTA * 2;
    unsigned index;

    for (index = 0; index < (unsigned)joysticks.size(); ++index) {
        if (index >= deviceCount || joystickCount >= 2)
            break;
        auto &input = inputs[joystickCount];
        auto &bindings = config_.joystickBindings[index];
        if (joysticks[index].isConnected) {
            //  TODO: select (x,y)[0] or (x,y)[1] based on user preference for
            //        console style controllers (left or right stick, left always for now)
            input.type = kClemensInputType_Paddle;
            joysticks[index].x[0] += bindings.axisAdj[0] * 16;
            joysticks[index].y[0] += bindings.axisAdj[1] * 16;
            joysticks[index].x[0] =
                (int16_t)std::clamp(-CLEM_HOST_JOYSTICK_AXIS_DELTA, (int32_t)joysticks[index].x[0],
                                    CLEM_HOST_JOYSTICK_AXIS_DELTA);
            joysticks[index].y[0] =
                (int16_t)std::clamp(-CLEM_HOST_JOYSTICK_AXIS_DELTA, (int32_t)joysticks[index].y[0],
                                    CLEM_HOST_JOYSTICK_AXIS_DELTA);

            input.value_a =
                (int16_t)((int32_t)(joysticks[index].x[0] + CLEM_HOST_JOYSTICK_AXIS_DELTA) *
                          kGameportAxisMagnitude / kHostAxisMagnitude);
            input.value_b =
                (int16_t)((int32_t)(joysticks[index].y[0] + CLEM_HOST_JOYSTICK_AXIS_DELTA) *
                          kGameportAxisMagnitude / kHostAxisMagnitude);

            input.gameport_button_mask = 0;
            if (bindings.button[0] != UINT32_MAX &&
                joysticks[index].buttons & (1 << bindings.button[0])) {
                input.gameport_button_mask |= 1;
            }
            if (bindings.button[1] != UINT32_MAX &&
                joysticks[index].buttons & (1 << bindings.button[1])) {
                input.gameport_button_mask |= 2;
            }
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
    diagnostics_.joyCount = joystickSlotCount_;
    for (index = 0; index < diagnostics_.joyCount; ++index) {
        diagnostics_.joyX[index] = joysticks_[index].x[0];
        diagnostics_.joyY[index] = joysticks_[index].y[0];
    }

    if (!isBackendRunning() || guiMode_ == GUIMode::JoystickConfig) {
        return;
    }
    if (joystickCount < 1)
        return;
    backendQueue_.inputEvent(inputs[0]);
    if (joystickCount < 2)
        return;
    backendQueue_.inputEvent(inputs[1]);
}

auto ClemensFrontend::frame(int width, int height, double deltaTime, ClemensHostInterop &interop)
    -> ViewType {
    //  send commands to emulator thread
    //  get results from emulator thread
    //    video, audio, machine state, etc
    if (interop.exitApp)
        return getViewType();

    interop.minWindowWidth = 0;
    interop.minWindowHeight = 0;

    pollJoystickDevices();

    bool isNewFrame = false;
    dtEmulatorNextUpdateInterval_ -= deltaTime;
    if (dtEmulatorNextUpdateInterval_ <= 0.0) {
        isNewFrame = syncBackend(true);
        readyForFrame_.notify_one();
        dtEmulatorNextUpdateInterval_ =
            std::max(0.0, dtEmulatorNextUpdateInterval_ + kMachineFrameDuration);
    }

    constexpr double kUIFlashCycleTime = 1.0;
    appTime_ += deltaTime;

    if (appTime_ >= nextUIFlashCycleAppTime_) {
        nextUIFlashCycleAppTime_ = std::floor(appTime_ / kUIFlashCycleTime) * kUIFlashCycleTime;
        nextUIFlashCycleAppTime_ += kUIFlashCycleTime;
    }
    uiFlashAlpha_ = std::sin((nextUIFlashCycleAppTime_ - appTime_) * M_PI / kUIFlashCycleTime);

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

    ImVec2 interfaceAnchor(0.0f, 0.0f);
    interop.allowConfigureJoystick =
        (guiMode_ == GUIMode::Emulator || guiMode_ == GUIMode::Setup) &&
        !browseDriveType_.has_value() && !browseSmartDriveIndex_.has_value();
    if (!interop.nativeMenu) {
        doMainMenu(interfaceAnchor, interop);
        interop.minWindowHeight +=
            ImGui::GetTextLineHeightWithSpacing() + ImGui::GetStyle().WindowBorderSize;
    }
    switch (interop.action) {
    case ClemensHostInterop::About:
        setGUIMode(GUIMode::Help);
        break;
    case ClemensHostInterop::Help:
        setGUIMode(GUIMode::HelpShortcuts);
        break;
    case ClemensHostInterop::DiskHelp:
        setGUIMode(GUIMode::HelpDisk);
        break;
    case ClemensHostInterop::LoadSnapshot:
        if (isBackendRunning()) {
            if (!isEmulatorStarting()) {
                setGUIMode(GUIMode::LoadSnapshot);
            }
        } else {
            setGUIMode(GUIMode::LoadSnapshotAfterPowerOn);
        }
        break;
    case ClemensHostInterop::SaveSnapshot:
        if (isBackendRunning() && !isEmulatorStarting()) {
            setGUIMode(GUIMode::SaveSnapshot);
        }
        break;
    case ClemensHostInterop::PasteFromClipboard:
        pasteClipboardToEmulator_ = true;
        break;
    case ClemensHostInterop::Power:
        rebootInternal(true);
        break;
    case ClemensHostInterop::Reboot:
        rebootInternal(true);
        break;
    case ClemensHostInterop::Shutdown:
        setGUIMode(GUIMode::ShutdownEmulator);
        break;
    case ClemensHostInterop::Debugger:
        config_.hybridInterfaceEnabled = true;
        break;
    case ClemensHostInterop::Standard:
        config_.hybridInterfaceEnabled = false;
        break;
    case ClemensHostInterop::AspectView:
        // TODO: set view width and height in interop
        break;
    case ClemensHostInterop::JoystickConfig:
        setGUIMode(GUIMode::JoystickConfig);
        break;
    case ClemensHostInterop::MouseLock:
        emulatorHasMouseFocus_ = true;
        break;
    case ClemensHostInterop::MouseUnlock:
        emulatorHasMouseFocus_ = false;
        break;
    case ClemensHostInterop::PauseExecution:
        backendQueue_.breakExecution();
        break;
    case ClemensHostInterop::ContinueExecution:
        backendQueue_.run();
        break;
    case ClemensHostInterop::DisableFastMode:
        backendQueue_.fastMode(false);
        break;
    case ClemensHostInterop::EnableFastMode:
        backendQueue_.fastMode(true);
        break;
    case ClemensHostInterop::None:
        break;
    }
    emulatorHasMouseFocus_ = emulatorHasMouseFocus_ && isBackendRunning();
    doEmulatorInterface(interfaceAnchor, ImVec2(width, height), interop, viewToMonitor, deltaTime);

    switch (guiMode_) {
    case GUIMode::None:
        if (isBackendRunning()) {
            setGUIMode(GUIMode::Emulator);
        } else {
            setGUIMode(GUIMode::Setup);
        }
        break;
    case GUIMode::Setup:
        //  this is handled above
        break;
    case GUIMode::SaveSnapshot:
        if (!saveSnapshotMode_.isStarted()) {
            saveSnapshotMode_.start(backendQueue_, frameReadState_.isRunning);
        }
        if (saveSnapshotMode_.frame(width, height, display_, backendQueue_)) {
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
    case GUIMode::HelpShortcuts:
    case GUIMode::HelpDisk:
        doHelpScreen(width, height);
        break;
    case GUIMode::JoystickConfig:
        // this is handled above
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
    if (guiMode_ != GUIMode::Emulator) {
        lostFocus();
    }

    config_.save();

    if (pasteClipboardToEmulator_) {
        interop.action = ClemensHostInterop::PasteFromClipboard;
        pasteClipboardToEmulator_ = false;
    }
    interop.poweredOn = isBackendRunning();
    interop.mouseLock = emulatorHasMouseFocus_;
    interop.mouseShow =
        !mouseInEmulatorScreen_ || ImGui::IsPopupOpen(nullptr, ImGuiPopupFlags_AnyPopup);
    interop.debuggerOn = config_.hybridInterfaceEnabled;
    interop.running = frameReadState_.isRunning;
    interop.fastMode = lastCommandState_.isFastModeOn;

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
                                          "HelpShortcuts",
                                          "HelpDisk",
                                          "JoystickConfig",
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

bool ClemensFrontend::syncBackend(bool copyState) {
    //  check if a new frame of data is available.  if none available, we'll
    //  wait another UI frame (slow emulator)
    bool newFrame = false;
    std::lock_guard<std::mutex> frameLock(frameMutex_);
    if (frameLastSeqNo_ != frameSeqNo_ && copyState) {
        //  new frame data located in the write frame, obtained from the
        //  backendState_ which is guaranteed to remain untouched until
        //  the last frame sequence number is updated.
        debugger_.lastFrame(frameReadState_);

        lastFrameCPURegs_ = frameReadState_.cpu.regs;
        lastFrameCPUPins_ = frameReadState_.cpu.pins;
        lastFrameIRQs_ = frameReadState_.irqs;
        lastFrameNMIs_ = frameReadState_.nmis;
        lastFrameIWM_ = frameReadState_.iwm;
        lastFrameADBStatus_ = frameReadState_.adb;
        if (frameReadState_.ioPage) {
            memcpy(lastFrameIORegs_, frameReadState_.ioPage, 256);
        }

        frameReadState_.copyState(backendState_, lastCommandState_, frameReadMemory_);
        backendState_.reset();

        if (debugger_.thisFrame(lastCommandState_, frameReadState_)) {
            emulatorHasMouseFocus_ = false;
        }

        logLevel_ = frameReadState_.logLevel;

        if (!lastCommandState_.results.empty()) {
            for (auto &result : lastCommandState_.results) {
                processBackendResult(result);
            }
            lastCommandState_.results.clear();
        }

        if (lastCommandState_.gsConfig.has_value()) {
            config_.gs = *lastCommandState_.gsConfig;
            lastCommandState_.gsConfig = std::nullopt;
            config_.setDirty();
        }
        newFrame = true;
    }

    stagedBackendQueue_.queue(backendQueue_);
    frameLastSeqNo_ = frameSeqNo_;
    return newFrame;
}

void ClemensFrontend::processBackendResult(const ClemensBackendResult &result) {
    bool succeeded = false;
    switch (result.type) {
    case ClemensBackendResult::Failed:
        debugger_.print(ClemensDebugger::Error,
                        fmt::format("{} Failed.", getCommandTypeName(result.cmd.type)).c_str());
        break;
    case ClemensBackendResult::Succeeded:
        debugger_.print(ClemensDebugger::Info, "Ok.");
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

void ClemensFrontend::doEmulatorInterface(ImVec2 anchor, ImVec2 dimensions,
                                          ClemensHostInterop &interop,
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

    interop.minWindowWidth += ClemensHostStyle::kSideBarMinWidth;
    interop.minWindowWidth += ClemensHostStyle::kScreenWidth;
    interop.minWindowHeight += ClemensHostStyle::kScreenHeight;
    interop.minWindowHeight += kInfoStatusSize.y;

    kMonitorViewSize.y -= (kInfoStatusSize.y + anchor.y);
    kMonitorViewSize.x =
        std::min(dimensions.x - anchor.x, kMonitorViewSize.y * kClemensAspectRatio);

    ImVec2 kSideBarSize(ClemensHostStyle::kSideBarMinWidth,
                        dimensions.y - kInfoStatusSize.y - anchor.y);
    kMonitorViewSize.x = dimensions.x - kSideBarSize.x;

    ImVec2 kSideBarAnchor = anchor;
    ImVec2 kMonitorViewAnchor(anchor.x + kSideBarSize.x, anchor.y);
    ImVec2 kInfoStatusAnchor(anchor.x, anchor.y + kSideBarSize.y);

    ImGui::PushStyleColor(ImGuiCol_WindowBg, ClemensHostStyle::getFrameColor(*this));
    ImGui::PushStyleColor(ImGuiCol_ButtonActive, ClemensHostStyle::getWidgetActiveColor(*this));
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ClemensHostStyle::getWidgetHoverColor(*this));
    ImGui::PushStyleColor(ImGuiCol_Button, ClemensHostStyle::getWidgetColor(*this));
    ImGui::PushStyleColor(ImGuiCol_FrameBgActive, ClemensHostStyle::getWidgetActiveColor(*this));
    ImGui::PushStyleColor(ImGuiCol_FrameBgHovered, ClemensHostStyle::getWidgetHoverColor(*this));
    ImGui::PushStyleColor(ImGuiCol_FrameBg, ClemensHostStyle::getWidgetToggleOffColor(*this));

    if (browseDriveType_.has_value()) {
        if (guiMode_ == GUIMode::JoystickConfig)
            guiMode_ = GUIMode::None;
        doMachineDiskBrowserInterface(kMonitorViewAnchor, kMonitorViewSize);
    } else if (browseSmartDriveIndex_.has_value()) {
        if (guiMode_ == GUIMode::JoystickConfig)
            guiMode_ = GUIMode::None;
        doMachineSmartDiskBrowserInterface(kMonitorViewAnchor, kMonitorViewSize);
    } else if (guiMode_ == GUIMode::Setup) {
        doSetupUI(kMonitorViewAnchor, kMonitorViewSize);
    } else if (guiMode_ == GUIMode::JoystickConfig) {
        doJoystickConfig(kMonitorViewAnchor, kMonitorViewSize);
    } else {
        if (config_.hybridInterfaceEnabled) {
            doDebuggerLayout(kMonitorViewAnchor, kMonitorViewSize, viewToMonitor);
        } else {
            doMachineViewLayout(kMonitorViewAnchor, kMonitorViewSize, viewToMonitor);
        }
    }
    doSidePanelLayout(kSideBarAnchor, kSideBarSize);
    doInfoStatusLayout(kInfoStatusAnchor, kInfoStatusSize, kMonitorViewAnchor.x);
    ImGui::PopStyleColor(7);
}

void ClemensFrontend::doDebuggerLayout(ImVec2 anchor, ImVec2 dimensions,
                                       const ViewToMonitorTranslation &viewToMonitor) {
    const ImGuiStyle &style = ImGui::GetStyle();
    bool mode80 = true;

    // Supports a tiled window debug mode that contains
    //  CPU State (always docked to the right of the screen)
    //  Main View (always docked to the left or right of debugger)
    //  Aux View (always docked below the main view)
    //  Debugger (always docked to the left of right of main view )
    constexpr unsigned kConsoleTile = 0;
    constexpr unsigned kViewTile = 1;
    constexpr unsigned kAuxViewTile = 2;
    constexpr unsigned kCPUTile = 3;

    ImVec2 childAnchor[4];
    ImVec2 childSize[4];

    ImGui::PushFont(mode80 ? ClemensHostImGui::Get80ColumnFont() : NULL);
    float kCharSize = ImGui::GetFont()->GetCharAdvance('A');

    //  CPU tile always fixed for now

    childSize[kCPUTile].x =
        (kCharSize * 15.0f) +
        (style.WindowPadding.x + style.WindowBorderSize + style.FramePadding.x) * 2;
    childSize[kCPUTile].y = dimensions.y;
    childAnchor[kCPUTile].x = anchor.x + dimensions.x - childSize[kCPUTile].x;
    childAnchor[kCPUTile].y = anchor.y;
    dimensions.x -= childSize[kCPUTile].x;

    //  These tiles can be resized based off the anchor tile, which will be
    //  the view tile (emulator screen).
    childAnchor[kViewTile] = anchor;
    childSize[kViewTile].x = dimensions.x * 0.55f;
    childSize[kViewTile].y = dimensions.y * 0.55f;

    childAnchor[kConsoleTile].x = childAnchor[kViewTile].x + childSize[kViewTile].x;
    childAnchor[kConsoleTile].y = childAnchor[kViewTile].y;
    childSize[kConsoleTile].x = dimensions.x - childSize[kViewTile].x;
    childSize[kConsoleTile].y = dimensions.y;

    childAnchor[kAuxViewTile].x = childAnchor[kViewTile].x;
    childAnchor[kAuxViewTile].y = childAnchor[kViewTile].y + childSize[kViewTile].y;
    childSize[kAuxViewTile].x = childSize[kViewTile].x;
    childSize[kAuxViewTile].y = dimensions.y - childSize[kViewTile].y;

    doMachineViewLayout(childAnchor[kViewTile], childSize[kViewTile], viewToMonitor);
    debugger_.auxillary(childAnchor[kAuxViewTile], childSize[kAuxViewTile]);
    debugger_.console(childAnchor[kConsoleTile], childSize[kConsoleTile]);
    debugger_.cpuStateTable(childAnchor[kCPUTile], childSize[kCPUTile], diagnostics_);

    ImGui::PopFont();
}

void ClemensFrontend::doMainMenu(ImVec2 &anchor, ClemensHostInterop &interop) {
    ImGui::PushStyleColor(ImGuiCol_MenuBarBg, ClemensHostStyle::getMenuColor(*this));
    // ImGui::PushStyleColor(ImGuiCol_Text, IM_COL32_BLACK);
    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(ImGui::GetStyle().FramePadding.x,
                                                           ImGui::GetTextLineHeight() * 0.5f));
    if (ImGui::BeginMainMenuBar()) {
        if (ImGui::BeginMenu("File")) {
            if (ImGui::MenuItem("Load Snapshot", NULL, false, !isEmulatorStarting())) {
                interop.action = ClemensHostInterop::LoadSnapshot;
            }
            if (ImGui::MenuItem("Save Snapshot", NULL, false,
                                isBackendRunning() && !isEmulatorStarting())) {
                interop.action = ClemensHostInterop::SaveSnapshot;
            }
            ImGui::Separator();
            if (ImGui::MenuItem("Quit", NULL)) {
                interop.exitApp = true;
            }
            ImGui::EndMenu();
        }
        if (ImGui::BeginMenu("Edit")) {
            // if (ImGui::MenuItem("Create Screenshot", NULL)) {
            //}
            // ImGui::Separator();
            if (ImGui::MenuItem("Paste Text Input", NULL)) {
                interop.action = ClemensHostInterop::PasteFromClipboard;
            }
            ImGui::EndMenu();
        }
        if (ImGui::BeginMenu("View")) {
            if (config_.hybridInterfaceEnabled) {
                if (ImGui::MenuItem("User Mode", CLEM_L10N_LABEL(kHybridModeShortcutText))) {
                    config_.hybridInterfaceEnabled = false;
                }
            } else {
                if (ImGui::MenuItem("Debugger Mode", CLEM_L10N_LABEL(kHybridModeShortcutText))) {
                    config_.hybridInterfaceEnabled = true;
                }
            }
            if (interop.mouseLock) {
                if (ImGui::MenuItem("Unlock Mouse", CLEM_L10N_LABEL(kLockMouseShortcutText))) {
                    interop.action = ClemensHostInterop::MouseUnlock;
                }
            } else {
                if (ImGui::MenuItem("Lock Mouse", CLEM_L10N_LABEL(kLockMouseShortcutText))) {
                    interop.action = ClemensHostInterop::MouseLock;
                }
            }
            ImGui::EndMenu();
        }
        if (ImGui::BeginMenu("Machine")) {
            if (guiMode_ == GUIMode::Setup) {
                if (ImGui::MenuItem("Power", NULL)) {
                    interop.action = ClemensHostInterop::Power;
                }
            } else {
                if (frameReadState_.isRunning) {
                    if (ImGui::MenuItem("Pause", CLEM_L10N_LABEL(kTogglePauseShortcutText), false,
                                        !isEmulatorStarting())) {
                        interop.action = ClemensHostInterop::PauseExecution;
                    }
                } else {
                    if (ImGui::MenuItem("Run", CLEM_L10N_LABEL(kTogglePauseShortcutText), false,
                                        !isEmulatorStarting())) {
                        interop.action = ClemensHostInterop::ContinueExecution;
                    }
                }
                if (ImGui::MenuItem("Reboot", NULL, false,
                                    isBackendRunning() && !isEmulatorStarting())) {
                    interop.action = ClemensHostInterop::Reboot;
                }
                if (ImGui::MenuItem("Shutdown", NULL)) {
                    interop.action = ClemensHostInterop::Shutdown;
                }
            }
            ImGui::Separator();
            if (ImGui::MenuItem("Fast Mode", CLEM_L10N_LABEL(kFastModeShortCutText), interop.fastMode, isBackendRunning())) {
                if (interop.fastMode) {
                    interop.action = ClemensHostInterop::DisableFastMode;
                } else {
                    interop.action = ClemensHostInterop::EnableFastMode;    
                }
            }
            if (ImGui::MenuItem("Configure Joystick", NULL, false,
                                interop.allowConfigureJoystick)) {
                interop.action = ClemensHostInterop::JoystickConfig;
            }
            ImGui::EndMenu();
        }
        if (ImGui::BeginMenu("Help")) {
            if (ImGui::MenuItem("Keyboard Shortcuts")) {
                interop.action = ClemensHostInterop::Help;
            }
            if (ImGui::MenuItem("Disk Selection")) {
                interop.action = ClemensHostInterop::DiskHelp;
            }
            ImGui::Separator();
            if (ImGui::MenuItem("About")) {
                interop.action = ClemensHostInterop::About;
            }
            ImGui::EndMenu();
        }
        anchor.y += ImGui::GetWindowSize().y;
        ImGui::EndMainMenuBar();
    }
    ImGui::PopStyleVar();
    // ImGui::PopStyleColor();
    ImGui::PopStyleColor();

    //  ImGui hotkeys are not handled by the ImGui system, so handle the keypresses here
    if (guiMode_ == GUIMode::Emulator) {
        if (ImGui::IsKeyDown(ImGuiKey_LeftAlt)) {
            if (ImGui::IsKeyDown(ImGuiKey_LeftCtrl) || ImGui::IsKeyDown(ImGuiKey_RightCtrl)) {
                if (ImGui::IsKeyPressed(ImGuiKey_F10)) {
                    if (interop.mouseLock) {
                        interop.action = ClemensHostInterop::MouseUnlock;
                    } else {
                        interop.action = ClemensHostInterop::MouseLock;
                    }
                } else if (ImGui::IsKeyPressed(ImGuiKey_F11)) {
                    config_.hybridInterfaceEnabled = !config_.hybridInterfaceEnabled;
                    config_.setDirty();
                } else if (ImGui::IsKeyPressed(ImGuiKey_F5)) {
                    if (frameReadState_.isRunning) {
                        interop.action = ClemensHostInterop::PauseExecution;
                    } else {
                        interop.action = ClemensHostInterop::ContinueExecution;
                    }
                } else if (ImGui::IsKeyPressed(ImGuiKey_F8)) {
                    if (lastCommandState_.isFastModeOn) {
                        interop.action = ClemensHostInterop::DisableFastMode;
                    } else {
                        interop.action = ClemensHostInterop::EnableFastMode;
                    }
                }
            }
        }
    }
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

    if (ImGui::Begin("SidePanel", nullptr,
                     ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoBringToFrontOnFocus)) {
        if (ImGui::BeginChild("DiskTray", ImVec2(0.0f, ClemensHostStyle::kDiskTrayHeight))) {
            doMachineDiskDisplay(sidebarSize.x);
        }
        ImGui::EndChild();
        if (ImGui::BeginChild("PeripheralsAndCards")) {
            doMachinePeripheralDisplay(sidebarSize.x);
        }
        ImGui::EndChild();
        ImGui::End();
    }
    anchor.y += sidebarSize.y;
    ImGui::SetNextWindowPos(anchor);
    ImGui::SetNextWindowSize(quickBarSize);
    if (ImGui::Begin("Quickbar", nullptr,
                     ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoBringToFrontOnFocus)) {
        doDebuggerQuickbar(quickBarSize.x);
        ImGui::End();
    }
}

void ClemensFrontend::doMachinePeripheralDisplay(float /*width */) {
    const ImGuiStyle &drawStyle = ImGui::GetStyle();
    ImDrawList *drawList = ImGui::GetWindowDrawList();
    if (ImGui::CollapsingHeader("System", ImGuiTreeNodeFlags_DefaultOpen)) {
        ImGui::Indent();
        ImGui::Text("%uK", config_.gs.memory);
        ImGui::Unindent();
        ImGui::Separator();
    }
    if (ImGui::CollapsingHeader("Devices", ImGuiTreeNodeFlags_DefaultOpen)) {
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
                    buttonText[0] = '0' + buttonIndex;
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
    if (!config_.hybridInterfaceEnabled) {
        debugger_.diagnosticTables(diagnostics_, false);
        ImGui::Separator();
    }
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

    const auto &style = ImGui::GetStyle();

    float resetStatusWidth =
        (style.FrameBorderSize + style.FramePadding.x) * 2 +
        ImGui::GetFont()->CalcTextSizeA(ImGui::GetFontSize(), FLT_MAX, 0.0f, "RESET0").x;
    float fpsStatusWidth =
        (style.FrameBorderSize + style.FramePadding.x) * 2 +
        ImGui::GetFont()->CalcTextSizeA(ImGui::GetFontSize(), FLT_MAX, 0.0f, "FPS:999.9").x;
    ImVec2 inputInstructionSize = ImVec2(dimensions.x - ImGui::GetCursorPos().x - resetStatusWidth -
                                             fpsStatusWidth - style.ItemSpacing.x,
                                         ImGui::GetTextLineHeight() + statusItemPaddingHeight);
    doViewInputInstructions(inputInstructionSize);

    ImGui::SameLine(anchor.x + dimensions.x - resetStatusWidth - fpsStatusWidth -
                    style.ItemSpacing.x);
    float runSpeedMhz = 0.0f;
    if (isBackendRunning()) {
        if (lastCommandState_.isFastEmulationOn) {
            runSpeedMhz = frameReadState_.emulationSpeedMhz;
        } else {
            runSpeedMhz = frameReadState_.machineSpeedMhz;
        }
    }
    ClemensHostImGui::StatusBarField(isResetDown ? ClemensHostImGui::StatusBarFlags_Active
                                                 : ClemensHostImGui::StatusBarFlags_Inactive,
                                     "%5.2f mhz",runSpeedMhz);

    ImGui::SameLine(anchor.x + dimensions.x - resetStatusWidth);
    ClemensHostImGui::StatusBarField(isResetDown ? ClemensHostImGui::StatusBarFlags_Active
                                                 : ClemensHostImGui::StatusBarFlags_Inactive,
                                     "RESET");
    ImGui::PopStyleVar();
    ImGui::PopStyleColor(2);

    ImGui::End();
}

void ClemensFrontend::doDebuggerQuickbar(float /*width */) {
    float lineHeight = ImGui::GetTextLineHeight() * 1.25f;
    const ImVec2 kIconSize(lineHeight, lineHeight);
    if (isBackendRunning()) {
        ClemensHostImGui::PushStyleButtonEnabled();
        ImGui::SameLine();

        if (ClemensHostImGui::IconButton(
                "Power", ClemensHostStyle::getImTextureOfAsset(ClemensHostAssets::kPowerButton),
                kIconSize)) {
            setGUIMode(GUIMode::ShutdownEmulator);
        }
        if (ImGui::IsItemHovered(ImGuiHoveredFlags_DelayNormal)) {
            ImGui::SetTooltip("Shutdown");
        }
        ImGui::SameLine();
        if (ClemensHostImGui::IconButton(
                "Reboot", ClemensHostStyle::getImTextureOfAsset(ClemensHostAssets::kPowerCycle),
                kIconSize)) {
            rebootInternal(true);
        }
        if (ImGui::IsItemHovered(ImGuiHoveredFlags_DelayNormal)) {
            ImGui::SetTooltip("Reboot");
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
            config_.hybridInterfaceEnabled = !config_.hybridInterfaceEnabled;
            config_.setDirty();
        }
        if (ImGui::IsItemHovered(ImGuiHoveredFlags_DelayNormal)) {
            if (config_.hybridInterfaceEnabled) {
                ImGui::SetTooltip("Enter standard mode");
            } else {
                ImGui::SetTooltip("Enter debugger mode");
            }
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

/*
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
*/

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
    doMachineSmartDriveStatus(1, "s7d1", true, true);
    ImGui::Separator();
    doMachineSmartDriveStatus(2, "s7d2", true, true);
    ImGui::Separator();
    //  TODO: Support IWM smartport devices once the ProDOSHDD32 can be removeable like
    //        it's implemented in the hddcard interface
    // doMachineSmartDriveStatus(0, "s2d1", !isBackendRunning(), false);
    ImGui::Separator();
    ImGui::Spacing();

    ImGui::PopStyleColor(3);
}

void ClemensFrontend::doMachineDiskStatus(ClemensDriveType driveType, float /*width */) {
    ClemensDiskDriveStatus driveStatus{};
    auto driveName = ClemensDiskUtilities::getDriveName(driveType);

    if (isBackendRunning()) {
        driveStatus = frameReadState_.frame.diskDriveStatuses[driveType];
    } else {
        driveStatus.assetPath = config_.gs.diskImagePaths[driveType];
    }

    const ImGuiStyle &style = ImGui::GetStyle();
    const float charSize = ImGui::GetFont()->GetCharAdvance('A');
    const float charHeight = ImGui::GetTextLineHeight();

    ImVec2 iconSize(32.0f, 32.0f);
    const auto clemensDiskIcon =
        (driveType == kClemensDrive_3_5_D1 || driveType == kClemensDrive_3_5_D2)
            ? ClemensHostAssets::kDisk35
            : ClemensHostAssets::kDisk525;
    const float widgetHeight = iconSize.y + charHeight * 0.5f;
    const float widgetWidthA = widgetHeight + charSize * 4 + (style.FramePadding.x * 2);
    ImGuiID diskWidgetId = ImGui::GetID(driveName.data(), driveName.data() + driveName.size());
    ImVec2 diskWidgetSize(widgetWidthA, widgetHeight);
#if 0
    const float widgetWidthB = widgetWidthA + charSize * 12;

    //  setup animations!


    const double animationTime = 0.3333f;
    auto &animation = diskWidgetAnimations_[driveType];
    animation.a = Animation::Keyframe(diskWidgetSize, 0.0);
    animation.b = Animation::Keyframe(ImVec2(widgetWidthB, widgetHeight), animationTime);
    animation.transition = cinek::equation<ImVec2>(cinek::transition::kEaseIn);
    switch (animation.mode) {
    case Animation::Mode::A:
        diskWidgetSize = animation.a.prop;
        animation.t = 0.0;
        break;
    case Animation::Mode::B:
        diskWidgetSize = animation.b.prop;
        animation.t = animationTime;
        break;
    case Animation::Mode::AToB:
        animation.t = std::min(animation.t + ImGui::GetIO().DeltaTime, animationTime);
        animation.transition.calc(diskWidgetSize, animation.a, animation.b, animation.t);
        if (animation.t >= animationTime)
            animation.mode = Animation::Mode::B;
        break;
    case Animation::Mode::BToA:
        animation.t = std::max(animation.t - ImGui::GetIO().DeltaTime, 0.0);
        animation.transition.calc(diskWidgetSize, animation.a, animation.b, animation.t);
        if (animation.t <= 0.0)
            animation.mode = Animation::Mode::A;
        break;
    }
#endif
    ImGui::PushID(diskWidgetId);
    ImGui::BeginGroup();
    {
        ImVec2 diskIndicatorSize(6.0f, widgetHeight);
        ImVec2 controlsWidgetSize(16.0f, widgetHeight);
        ImVec2 cursorPos = ImGui::GetCursorScreenPos();
        ImVec2 diskWidgetPos =
            cursorPos + ImVec2(controlsWidgetSize.x + style.ItemInnerSpacing.x +
                                   diskIndicatorSize.x + style.ItemInnerSpacing.x,
                               0.0f);
        ImVec2 bottomRightPos = diskWidgetPos + ImVec2(diskWidgetSize.x, widgetHeight);

        ImGui::PushClipRect(cursorPos, bottomRightPos, false);
        if (driveStatus.isMounted()) {
            ImVec2 buttonAnchor = cursorPos + ImVec2(0.0f, style.FramePadding.y);
            ImVec2 buttonSize(controlsWidgetSize.x, controlsWidgetSize.x);
            ImGui::SetCursorScreenPos(buttonAnchor);
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
            buttonAnchor.y = bottomRightPos.y - buttonSize.y - style.FramePadding.y;
            if (isBackendRunning()) {
                //  TODO: we don't persist write protected status in the configuration, so
                //  there's
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

                ImGui::SetCursorScreenPos(buttonAnchor);
                if (ClemensHostImGui::IconButton(
                        "Lock", ClemensHostStyle::getImTextureOfAsset(ClemensHostAssets::kLockDisk),
                        buttonSize)) {
                    backendQueue_.writeProtectDisk(driveType, !driveStatus.isWriteProtected);
                }

                ImGui::PopStyleColor();
            }
        }
        doMachineDiskMotorStatus(diskWidgetPos -
                                     ImVec2(style.ItemInnerSpacing.x + diskIndicatorSize.x, 0.0f),
                                 diskIndicatorSize, driveStatus.isSpinning);
        ImGui::SetCursorScreenPos(diskWidgetPos);
        if (ImGui::InvisibleButton("disk", diskWidgetSize)) {
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
                } else if (!savedDiskBrowsePaths_[driveType].empty()) {
                    assetBrowser_.setCurrentDirectory(savedDiskBrowsePaths_[driveType]);
                }
                assetBrowser_.setDiskType(ClemensDiskAsset::diskTypeFromDriveType(driveType));
            }
        }
        ImVec4 uiColor;
        if (ImGui::IsItemActive() || ImGui::IsItemHovered() || ImGui::IsItemFocused()) {
            if (ImGui::IsItemActive()) {
                uiColor = style.Colors[ImGuiCol_ButtonActive];
            } else if (ImGui::IsItemHovered() || ImGui::IsItemFocused()) {
                uiColor = style.Colors[ImGuiCol_NavHighlight];
            }
#if 0
            if (animation.mode == Animation::Mode::A) {
                animation.mode = Animation::Mode::AToB;
            }
#endif
        } else {
            uiColor = style.Colors[ImGuiCol_Button];
#if 0
            if (animation.mode != Animation::Mode::A) {
                animation.mode = Animation::Mode::BToA;
            }
#endif
        }
        if (browseDriveType_.has_value() && *browseDriveType_ == driveType) {
            uiColor.w = uiFlashAlpha_;
        }
        ImDrawList *drawList = ImGui::GetWindowDrawList();
        drawList->AddRectFilled(diskWidgetPos, bottomRightPos, (ImU32)ImColor(uiColor));
        //  icon
        cursorPos =
            diskWidgetPos + ImVec2(style.FramePadding.x, (widgetHeight - iconSize.y) * 0.5f);
        ImVec4 iconColor;
        if (driveStatus.isMounted()) {
            uiColor = ImVec4(1.0f, 1.0f, 1.0f, 1.0f);
            iconColor = uiColor;
        } else {
            uiColor = ImVec4(0.0f, 0.0f, 0.0f, 1.0f);
            iconColor = ImVec4(0.5f, 0.5f, 0.5f, 1.0f);
        }

        auto texId = ClemensHostStyle::getImTextureOfAsset(clemensDiskIcon);
        drawList->AddImage(texId, cursorPos, cursorPos + iconSize, ImVec2(0.0f, 0.0f),
                           ImVec2(1.0f, 1.0f), ImColor(iconColor));

        cursorPos.x += (iconSize.x + style.ItemInnerSpacing.x);
        cursorPos.y += ((iconSize.y - charHeight) * 0.5f);
        ImGui::SetCursorScreenPos(cursorPos);
        ImGui::PushStyleColor(ImGuiCol_Text, uiColor);
        ImGui::TextUnformatted(ClemensDiskUtilities::getDriveName(driveType).data());
        ImGui::PopStyleColor();
        ImGui::PopClipRect();
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

void ClemensFrontend::doMachineSmartDriveStatus(unsigned driveIndex, const char *label,
                                                bool allowSelect, bool allowHotswap) {
    ClemensDiskDriveStatus driveStatus{};

    if (isBackendRunning()) {
        driveStatus = frameReadState_.frame.smartPortStatuses[driveIndex];
    } else {
        driveStatus.assetPath = config_.gs.smartPortImagePaths[driveIndex];
    }
    bool isDisabled = !allowHotswap && isBackendRunning() && !driveStatus.isMounted();
    float widgetAlpha = isDisabled ? 0.2f : 1.0f;

    const ImGuiStyle &style = ImGui::GetStyle();
    const float charSize = ImGui::GetFont()->GetCharAdvance('A');
    const float charHeight = ImGui::GetTextLineHeight();

    char driveName[32];
    strncpy(driveName, label, sizeof(driveName) - 1);
    driveName[31] = '\0';

    ImVec2 iconSize(32.0f, 32.0f);
    const auto clemensDiskIcon = ClemensHostAssets::kDiskHDD;
    const float widgetHeight = iconSize.y + charHeight * 0.5f;
    const float widgetWidthA = widgetHeight + charSize * 4 + (style.FramePadding.x * 2);
    ImGuiID diskWidgetId = ImGui::GetID(driveName);
    ImVec2 diskWidgetSize(widgetWidthA, widgetHeight);

    ImGui::PushID(diskWidgetId);
    ImGui::BeginGroup();
    {
        ImVec2 diskIndicatorSize(6.0f, widgetHeight);
        ImVec2 controlsWidgetSize(16.0f, widgetHeight);
        ImVec2 cursorPos = ImGui::GetCursorScreenPos();
        ImVec2 diskWidgetPos =
            cursorPos + ImVec2(controlsWidgetSize.x + style.ItemInnerSpacing.x +
                                   diskIndicatorSize.x + style.ItemInnerSpacing.x,
                               0.0f);
        ImVec2 bottomRightPos = diskWidgetPos + ImVec2(diskWidgetSize.x, widgetHeight);

        ImGui::PushClipRect(cursorPos, bottomRightPos, false);
        if (driveStatus.isMounted() && allowSelect) {
            ImVec2 buttonAnchor = cursorPos + ImVec2(0.0f, style.FramePadding.y);
            ImVec2 buttonSize(controlsWidgetSize.x, controlsWidgetSize.x);
            ImGui::SetCursorScreenPos(buttonAnchor);
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
            /*
            buttonAnchor.y = bottomRightPos.y - buttonSize.y - style.FramePadding.y;
            if (isBackendRunning()) {
                //  TODO: we don't persist write protected status in the configuration, so
            there's
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

                ImGui::SetCursorScreenPos(buttonAnchor);
                if (ClemensHostImGui::IconButton(
                        "Lock",
            ClemensHostStyle::getImTextureOfAsset(ClemensHostAssets::kLockDisk), buttonSize)) {
                    backendQueue_.writeProtectDisk(driveType, !driveStatus.isWriteProtected);
                }

                ImGui::PopStyleColor();
            }
            */
        }
        doMachineDiskMotorStatus(diskWidgetPos -
                                     ImVec2(style.ItemInnerSpacing.x + diskIndicatorSize.x, 0.0f),
                                 diskIndicatorSize, driveStatus.isSpinning);
        ImGui::SetCursorScreenPos(diskWidgetPos);
        if (browseSmartDriveIndex_.has_value() || !allowSelect) {
            ImGui::Dummy(diskWidgetSize);
        } else if (ImGui::InvisibleButton("disk", diskWidgetSize)) {
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
                } else if (!savedSmartDiskBrowsePaths_[driveIndex].empty()) {
                    assetBrowser_.setCurrentDirectory(savedSmartDiskBrowsePaths_[driveIndex]);
                }
                assetBrowser_.setDiskType(ClemensDiskAsset::DiskHDD);
            }
        }
        ImVec4 uiColor;
        if (allowSelect &&
            (ImGui::IsItemActive() || ImGui::IsItemHovered() || ImGui::IsItemFocused())) {
            if (ImGui::IsItemActive()) {
                uiColor = style.Colors[ImGuiCol_ButtonActive];
            } else if (ImGui::IsItemHovered() || ImGui::IsItemFocused()) {
                uiColor = style.Colors[ImGuiCol_NavHighlight];
            }
        } else {
            uiColor = style.Colors[ImGuiCol_Button];
        }
        uiColor.w = widgetAlpha;
        if (browseSmartDriveIndex_.has_value() && *browseSmartDriveIndex_ == driveIndex) {
            uiColor.w *= uiFlashAlpha_;
        }
        ImDrawList *drawList = ImGui::GetWindowDrawList();
        drawList->AddRectFilled(diskWidgetPos, bottomRightPos, (ImU32)ImColor(uiColor));
        //  icon
        cursorPos =
            diskWidgetPos + ImVec2(style.FramePadding.x, (widgetHeight - iconSize.y) * 0.5f);

        ImVec4 iconColor;
        if (driveStatus.isMounted()) {
            uiColor = ImVec4(1.0f, 1.0f, 1.0f, widgetAlpha);
            iconColor = uiColor;
        } else {
            uiColor = ImVec4(0.0f, 0.0f, 0.0f, widgetAlpha);
            iconColor = ImVec4(0.5f, 0.5f, 0.5f, widgetAlpha);
        }

        auto texId = ClemensHostStyle::getImTextureOfAsset(clemensDiskIcon);
        drawList->AddImage(texId, cursorPos, cursorPos + iconSize, ImVec2(0.0f, 0.0f),
                           ImVec2(1.0f, 1.0f), ImColor(iconColor));

        cursorPos.x += (iconSize.x + style.ItemInnerSpacing.x);
        cursorPos.y += ((iconSize.y - charHeight) * 0.5f);
        ImGui::SetCursorScreenPos(cursorPos);
        ImGui::PushStyleColor(ImGuiCol_Text, uiColor);
        ImGui::TextUnformatted(driveName);
        ImGui::PopStyleColor();
        ImGui::PopClipRect();
    }
    ImGui::EndGroup();
    ImGui::PopID();
    if (ImGui::IsItemHovered(ImGuiHoveredFlags_DelayNormal)) {
        if (driveStatus.isMounted()) {
            ImGui::SetTooltip("%s (%s)", label, driveStatus.assetPath.c_str());
        } else {
            ImGui::SetTooltip("%s", label);
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
        auto assetPath = assetBrowser_.getCurrentPathname();
        if (assetBrowser_.isSelected()) {
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
        savedDiskBrowsePaths_[*browseDriveType_] =
            std::filesystem::path(assetPath).parent_path().string();
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
        auto assetPath = assetBrowser_.getCurrentPathname();
        if (assetBrowser_.isSelected()) {
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
        savedSmartDiskBrowsePaths_[*browseSmartDriveIndex_] =
            std::filesystem::path(assetPath).parent_path().string();
        browseSmartDriveIndex_ = std::nullopt;
    }
    ImGui::End();
}

template <typename... Args> void ClemensFrontend::doModalError(const char *msg, Args... args) {
    messageModalString_ = fmt::format(msg, args...);
    spdlog::error("ClemensFrontend - doModalError() {}\n", messageModalString_);
}

/*

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

    bool driveOn = (iwmState.status & ClemensFrame::kIWMStatusDriveOn);
    bool driveSpin = (iwmState.status & ClemensFrame::kIWMStatusDriveSpin);
    bool drive35 = (iwmState.status & ClemensFrame::kIWMStatusDrive35);
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
    fieldColor = CLEM_HOST_GUI_IWM_STATUS_CHANGED(ClemensFrame::kIWMStatusDrive35)
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
    fieldColor = CLEM_HOST_GUI_IWM_STATUS_CHANGED(ClemensFrame::kIWMStatusDriveAlt)
                     ? getModifiedColor(driveOn, frameReadState_.isRunning)
                     : getDefaultColor(driveOn);
    ImGui::PushStyleColor(ImGuiCol_Text, (ImU32)fieldColor);
    ImGui::TextUnformatted("Drive");
    ImGui::TableNextColumn();
    if (driveOn) {
        if (iwmState.status & ClemensFrame::kIWMStatusDriveAlt) {
            ImGui::TextUnformatted("2");
        } else {
            ImGui::TextUnformatted("1");
        }
    }
    ImGui::PopStyleColor();
    ImGui::TableNextRow();
    ImGui::TableNextColumn();
    fieldColor = CLEM_HOST_GUI_IWM_STATUS_CHANGED(ClemensFrame::kIWMStatusIWMQ6)
                     ? getModifiedColor(true, frameReadState_.isRunning)
                     : getDefaultColor(true);
    ImGui::TextColored(fieldColor, "Q6");
    ImGui::TableNextColumn();
    ImGui::TextColored(fieldColor, "%d", (iwmState.status & ClemensFrame::kIWMStatusIWMQ6) ? 1 : 0);
    ImGui::TableNextRow();
    ImGui::TableNextColumn();
    fieldColor = CLEM_HOST_GUI_IWM_STATUS_CHANGED(ClemensFrame::kIWMStatusIWMQ7)
                     ? getModifiedColor(true, frameReadState_.isRunning)
                     : getDefaultColor(true);
    ImGui::TextColored(fieldColor, "Q7");
    ImGui::TableNextColumn();
    ImGui::TextColored(fieldColor, "%d", (iwmState.status & ClemensFrame::kIWMStatusIWMQ7) ? 1 : 0);
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
    fieldColor = CLEM_HOST_GUI_IWM_STATUS_CHANGED(ClemensFrame::kIWMStatusDriveSpin)
                     ? getModifiedColor(driveOn, frameReadState_.isRunning)
                     : getDefaultColor(driveOn);
    ImGui::TextColored(fieldColor, "Motor");
    ImGui::TableNextColumn();
    ImGui::TextColored(fieldColor, "%s",
                       (iwmState.status & ClemensFrame::kIWMStatusDriveSpin) ? "on" : "off");
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
    fieldColor = CLEM_HOST_GUI_IWM_STATUS_CHANGED(ClemensFrame::kIWMStatusDriveSel)
                     ? getModifiedColor(driveOn, frameReadState_.isRunning)
                     : getDefaultColor(driveOn);
    ImGui::PushStyleColor(ImGuiCol_Text, (ImU32)fieldColor);
    ImGui::TableNextColumn();
    ImGui::TextUnformatted("HeadSel");
    ImGui::TableNextColumn();
    ImGui::Text("%d", iwmState.status & ClemensFrame::kIWMStatusDriveSel ? 1 : 0);
    ImGui::PopStyleColor();
    ImGui::TableNextRow();
    fieldColor = CLEM_HOST_GUI_IWM_STATUS_CHANGED(ClemensFrame::kIWMStatusDriveWP)
                     ? getModifiedColor(driveOn, frameReadState_.isRunning)
                     : getDefaultColor(driveOn);
    ImGui::PushStyleColor(ImGuiCol_Text, (ImU32)fieldColor);
    ImGui::TableNextColumn();
    ImGui::TextUnformatted("Sense");
    ImGui::TableNextColumn();
    ImGui::Text("%d", iwmState.status & ClemensFrame::kIWMStatusDriveWP ? 1 : 0);
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
*/

void ClemensFrontend::doSetupUI(ImVec2 anchor, ImVec2 dimensions) {
    ImGui::SetNextWindowPos(anchor);
    ImGui::SetNextWindowSize(dimensions);
    ImGui::Begin("Settings", NULL,
                 ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoCollapse);
    ImGui::Spacing();
    if (settingsView_.frame()) {
        rebootInternal(true);
    }
    ImGui::End();
}

void ClemensFrontend::doMachineViewLayout(ImVec2 rootAnchor, ImVec2 rootSize,
                                          const ViewToMonitorTranslation &viewToMonitor) {
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
    ImGui::SetNextWindowPos(rootAnchor);
    ImGui::SetNextWindowSize(rootSize);
    if (ImGui::Begin("Display", nullptr,
                     ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoBringToFrontOnFocus |
                         ImGuiWindowFlags_NoMove)) {
        if (emulatorShouldHaveKeyboardFocus_) {
            // Focus for the next frame will be evaluated inside the window block
            // Here we want the emulator to intercept all keyboard input
            ImGui::SetNextWindowFocus();
            if (guiMode_ == GUIMode::Emulator) {
                emulatorShouldHaveKeyboardFocus_ = false;
            }
        }
        if (ImGui::BeginChild("DisplayView")) {
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

            emulatorHasKeyboardFocus_ =
                emulatorHasMouseFocus_ || ImGui::IsWindowFocused() || ImGui::IsWindowHovered();

            //  translate mouse position to emulated screen coordinates
            auto mousePos = ImGui::GetMousePos();
            float viewToMonitorScalarX = viewToMonitor.size.x / monitorSize.x;
            float viewToMonitorScalarY = viewToMonitor.size.y / monitorSize.y;
            float mouseX = (mousePos.x - monitorAnchor.x) * viewToMonitorScalarX;
            float mouseY = (mousePos.y - monitorAnchor.y) * viewToMonitorScalarY;
            float mouseToViewDX = (viewToMonitor.size.x - viewToMonitor.workSize.x) * 0.5f;
            float mouseToViewDY = (viewToMonitor.size.y - viewToMonitor.workSize.y) * 0.5f;

            bool mouseInDeviceScreen = true;
            diagnostics_.mouseX = std::floor(mouseX - mouseToViewDX);
            diagnostics_.mouseY = std::floor(mouseY - mouseToViewDY);

            if (diagnostics_.mouseX < 0 || diagnostics_.mouseX >= viewToMonitor.workSize.x)
                mouseInDeviceScreen = false;
            else if (diagnostics_.mouseY < 0 || diagnostics_.mouseY >= viewToMonitor.workSize.y)
                mouseInDeviceScreen = false;

            //
            //` super-hires tracking: x /= 2 if in 320 mode (any scanlines that are 640 pixel,
            //```means 640 mode)
            //  what to do about legacy graphics modes (use screen holes, but what do apps expect
            //  in ranges from firmware?)
            if (!emulatorHasMouseFocus_) {
                mouseInEmulatorScreen_ = mouseInDeviceScreen;
                int16_t clampedMouseX =
                    std::clamp(diagnostics_.mouseX, (int16_t)0, (int16_t)viewToMonitor.workSize.x);
                int16_t clampedMouseY =
                    std::clamp(diagnostics_.mouseY, (int16_t)0, (int16_t)viewToMonitor.workSize.y);
                ClemensInputEvent mouseEvt;

                int16_t a2screenX, a2screenY;

                //  TODO: maybe only run this for super-hires mode?
                clemens_monitor_to_video_coordinates(&frameReadState_.frame.monitor,
                                                     &frameReadState_.frame.graphics, &a2screenX,
                                                     &a2screenY, clampedMouseX, clampedMouseY);

                mouseEvt.type = kClemensInputType_MouseMoveAbsolute;
                mouseEvt.value_a = int16_t(a2screenX);
                mouseEvt.value_b = int16_t(a2screenY);
                backendQueue_.inputEvent(mouseEvt);
                if (mouseInEmulatorScreen_) {
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
            } else {
                mouseInEmulatorScreen_ = false;
            }
        }
        ImGui::EndChild();
        ImGui::End();
    }
    ImGui::PopStyleVar();
}

void ClemensFrontend::doViewInputInstructions(ImVec2 dimensions) {
    const char *infoText = nullptr;
    if (emulatorHasMouseFocus_) {
        infoText = ClemensL10N::kMouseUnlock[ClemensL10N::kLanguageDefault];
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
            if (ImGui::BeginTabItem(
                    "Hotkeys", NULL,
                    guiMode_ == GUIMode::HelpShortcuts ? ImGuiTabItemFlags_SetSelected : 0)) {
                ClemensHostImGui::Markdown(CLEM_L10N_LABEL(kGSKeyboardCommands));
                ImGui::EndTabItem();
            }
            if (ImGui::BeginTabItem("Disk Selection", NULL,
                                    guiMode_ == GUIMode::HelpDisk ? ImGuiTabItemFlags_SetSelected
                                                                  : 0)) {
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
    if (guiMode_ == GUIMode::HelpShortcuts || guiMode_ == GUIMode::HelpDisk) {
        //  hacky method to automatically trigger the shortcuts tab from the main menu
        setGUIMode(GUIMode::Help);
    }
    if (!isOpen) {
        setGUIMode(GUIMode::Emulator);
    }
}

void ClemensFrontend::doJoystickConfig(ImVec2 anchor, ImVec2 dimensions) {
    ImGui::SetNextWindowPos(anchor);
    ImGui::SetNextWindowSize(dimensions);
    ImGui::Begin(CLEM_L10N_LABEL(kTitleJoystickConfiguration), NULL,
                 ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoCollapse);
    ImGui::Spacing();

    ImVec2 contentAvail = ImGui::GetContentRegionAvail();
    ImVec2 modalAnchor;
    ImVec2 modalDimensions;
    std::optional<unsigned> joystickModalIndex;
    ImVec2 kFooterSize(contentAvail.x, ImGui::GetFrameHeightWithSpacing());
    ImVec2 childPos = ImGui::GetCursorPos();
    if (ImGui::BeginChild("##Joysticks", ImVec2(contentAvail.x, contentAvail.y - kFooterSize.y))) {
        contentAvail = ImGui::GetContentRegionAvail();
        if (joystickSlotCount_ == 0) {
            ImVec2 textSize = ImGui::CalcTextSize(CLEM_L10N_LABEL(kLabelJoystickNone));
            ImGui::SetCursorPosX((contentAvail.x - textSize.x) * 0.5f);
            ImGui::SetCursorPosY((contentAvail.y - textSize.y) * 0.5f);
            ImGui::PushStyleColor(ImGuiCol_Text, IM_COL32(255, 255, 0, 255));
            ImGui::TextUnformatted(CLEM_L10N_LABEL(kLabelJoystickNone));
            ImGui::PopStyleColor();
        } else {
            const float kGridSize = ImGui::GetTextLineHeightWithSpacing() * 12;
            const float kInnerGridSize = ImGui::GetTextLineHeightWithSpacing() * 11;
            const float kRowSize = kGridSize + 2 * ImGui::GetTextLineHeightWithSpacing();
            const float kBallSize = ImGui::GetFont()->FontSize * 0.75f;
            const float kCharSize = ImGui::GetFont()->FontSize;
            const float kContentHeight =
                (kRowSize + ImGui::GetTextLineHeight()) * joystickSlotCount_;
            ImGui::SetCursorPosY((contentAvail.y - kContentHeight) * 0.5f);
            //  the joystick position where 0,0 is the center
            //
            //  identifying label for the joystick
            //  two rows, one per joystick
            //  buttons 1 and 2 require mapping controls
            for (unsigned joystickIndex = 0; joystickIndex < joystickSlotCount_; joystickIndex++) {
                char joyId[8];
                snprintf(joyId, sizeof(joyId) - 1, "joy%u", joystickIndex);
                auto &bindings = config_.joystickBindings[joystickIndex];
                auto &joystick = joysticks_[joystickIndex];
                constexpr unsigned axisIndex = 0; // TODO: this will be configurable
                modalAnchor = ImGui::GetCursorPos();
                ImGui::BeginGroup();
                if (ImGui::BeginTable(joyId, 3)) {
                    ImGui::BeginDisabled(!joystick.isConnected);
                    ImGui::TableSetupColumn("##Name", ImGuiTableColumnFlags_WidthStretch);
                    ImGui::TableSetupColumn("##Axis", ImGuiTableColumnFlags_WidthFixed,
                                            kGridSize + 6 * kCharSize);
                    ImGui::TableSetupColumn("##Button");
                    ImGui::TableNextRow();
                    ImGui::TableNextColumn();
                    ImGui::Text(CLEM_L10N_LABEL(kLabelJoystickId), joystickIndex);
                    ImGui::Spacing();
                    ImGui::Separator();
                    ImGui::Spacing();
                    ImGui::PushStyleColor(ImGuiCol_Text, IM_COL32(255, 255, 0, 255));
                    ImGui::PushTextWrapPos();
                    ImGui::TextUnformatted(CLEM_L10N_LABEL(kLabelJoystickHelp));
                    ImGui::PopTextWrapPos();
                    ImGui::PopStyleColor();
                    ImGui::TableNextColumn();
                    ImGui::SetNextItemWidth(kGridSize);
                    ImGui::SliderInt("Adjust", &bindings.axisAdj[0], -9, 9);
                    ImGui::Dummy(ImVec2(kGridSize, kGridSize));
                    {
                        ImDrawList *drawList = ImGui::GetWindowDrawList();
                        auto leftTop = ImGui::GetItemRectMin();
                        auto rightBottom = ImGui::GetItemRectMax();
                        drawList->AddRectFilled(leftTop, rightBottom,
                                                ClemensHostStyle::getInsetColor(*this));
                        auto ballPos =
                            ImVec2(((joystick.x[axisIndex] + CLEM_HOST_JOYSTICK_AXIS_DELTA) *
                                    kInnerGridSize / CLEM_HOST_JOYSTICK_AXIS_DELTA),
                                   ((joystick.y[axisIndex] + CLEM_HOST_JOYSTICK_AXIS_DELTA) *
                                    kInnerGridSize / CLEM_HOST_JOYSTICK_AXIS_DELTA));
                        ballPos.x =
                            (ballPos.x * 0.5f) + leftTop.x + (kGridSize - kInnerGridSize) * 0.5f;
                        ballPos.y =
                            (ballPos.y * 0.5f) + leftTop.y + (kGridSize - kInnerGridSize) * 0.5f;

                        drawList->AddCircleFilled(ballPos, kBallSize,
                                                  ClemensHostStyle::getWidgetColor(*this));
                    }
                    ImGui::SameLine();
                    ImGui::VSliderInt("##YAdj", ImVec2(kBallSize * 2, kGridSize),
                                      &bindings.axisAdj[1], 9, -9);
                    ImGui::TableNextColumn();
                    char btnLabel[16];
                    auto &style = ImGui::GetStyle();
                    auto buttonColor = (joystick.buttons & (1 << bindings.button[0]))
                                           ? ImVec4(0.0f, 1.0f, 0.0f, 1.0f)
                                           : style.Colors[ImGuiCol_Button];
                    ImGui::PushStyleColor(ImGuiCol_Button, buttonColor);
                    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, buttonColor);
                    if (bindings.button[0] != UINT32_MAX) {
                        snprintf(btnLabel, sizeof(btnLabel) - 1, "B%u", bindings.button[0]);
                    } else {
                        strncpy(btnLabel, "B?", sizeof(btnLabel) - 1);
                    }
                    if (ImGui::Button(btnLabel, ImVec2(kCharSize * 6, 0.0f))) {
                        joystickModalIndex = joystickIndex;
                        joystickButtonConfigIndex_ = 0;
                    }
                    ImGui::PopStyleColor(2);
                    ImGui::SameLine();
                    ImGui::TextUnformatted(CLEM_L10N_LABEL(kButtonJoystickButton1));
                    buttonColor = (joystick.buttons & (1 << bindings.button[1]))
                                      ? ImVec4(0.0f, 1.0f, 0.0f, 1.0f)
                                      : style.Colors[ImGuiCol_Button];
                    ImGui::PushStyleColor(ImGuiCol_Button, buttonColor);
                    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, buttonColor);
                    if (bindings.button[1] != UINT32_MAX) {
                        snprintf(btnLabel, sizeof(btnLabel) - 1, "B%u", bindings.button[1]);
                    } else {
                        strncpy(btnLabel, "B?", sizeof(btnLabel) - 1);
                    }
                    if (ImGui::Button(btnLabel, ImVec2(kCharSize * 6, 0.0f))) {
                        joystickModalIndex = joystickIndex;
                        joystickButtonConfigIndex_ = 1;
                    }
                    ImGui::PopStyleColor(2);
                    ImGui::SameLine();
                    ImGui::TextUnformatted(CLEM_L10N_LABEL(kButtonJoystickButton2));
                    ImGui::Spacing();
                    ImGui::Separator();
                    ImGui::Spacing();
                    ImGui::PushStyleColor(ImGuiCol_Text, IM_COL32(255, 255, 0, 255));
                    ImGui::PushTextWrapPos();
                    ImGui::TextUnformatted(CLEM_L10N_LABEL(kLabelJoystick2Help));
                    ImGui::PopTextWrapPos();
                    ImGui::PopStyleColor();
                    ImGui::EndDisabled();
                    ImGui::EndTable();
                }
                ImGui::EndGroup();
                if (joystickModalIndex_.has_value()) {
                    if (joystickIndex == joystickModalIndex_) {
                        modalDimensions =
                            ImVec2(contentAvail.x, ImGui::GetCursorPosY() - modalAnchor.y);
                    }
                }
                ImGui::Separator();
            }
        }
    }
    ImGui::EndChild();
    if (ImGui::Button(CLEM_L10N_LABEL(kLabelJoystickConfirm))) {
        if (isBackendRunning()) {
            setGUIMode(GUIMode::Emulator);
        } else {
            setGUIMode(GUIMode::Setup);
        }
    }
    ImGui::End();

    if (joystickModalIndex.has_value()) {
        ImGui::OpenPopup("##ButtonInputModal");
        joystickModalIndex_ = joystickModalIndex;
    }
    if (ImGui::IsPopupOpen("##ButtonInputModal")) {
        assert(joystickModalIndex_.has_value());
        unsigned joystickIndex = *joystickModalIndex_;
        unsigned joystickButtonIndex = *joystickButtonConfigIndex_;
        modalAnchor.x += (anchor.x + childPos.x);
        modalAnchor.y += (anchor.y + childPos.y);
        ImGui::SetNextWindowPos(modalAnchor);
        ImGui::SetNextWindowSize(modalDimensions);
        if (ImGui::BeginPopupModal("##ButtonInputModal", NULL,
                                   ImGuiWindowFlags_Modal | ImGuiWindowFlags_NoScrollbar |
                                       ImGuiWindowFlags_NoTitleBar)) {
            auto &bindings = config_.joystickBindings[joystickIndex];
            auto contentAvail = ImGui::GetContentRegionAvail();
            auto textSize = ImGui::CalcTextSize(CLEM_L10N_LABEL(kLabelJoystickButtonBinding));
            ImGui::SetCursorPos(
                ImVec2((contentAvail.x - textSize.x) * 0.5f, (contentAvail.y - textSize.y) * 0.5f));
            ImGui::PushTextWrapPos();
            ImGui::Text(CLEM_L10N_LABEL(kLabelJoystickButtonBinding), joystickButtonIndex);
            ImGui::PopTextWrapPos();
            if (joysticks_[joystickIndex].buttons & CLEM_HOST_JOYSTICK_BUTTON_A) {
                for (unsigned buttonBindingIndex = 0; buttonBindingIndex < 2;
                     ++buttonBindingIndex) {
                    if (bindings.button[buttonBindingIndex] == 0) {
                        bindings.button[buttonBindingIndex] = UINT32_MAX;
                    }
                }
                bindings.button[joystickButtonIndex] = 0;
                ImGui::CloseCurrentPopup();
            } else if (joysticks_[joystickIndex].buttons & CLEM_HOST_JOYSTICK_BUTTON_B) {
                for (unsigned buttonBindingIndex = 0; buttonBindingIndex < 2;
                     ++buttonBindingIndex) {
                    if (bindings.button[buttonBindingIndex] == 1) {
                        bindings.button[buttonBindingIndex] = UINT32_MAX;
                    }
                }
                bindings.button[joystickButtonIndex] = 1;
                ImGui::CloseCurrentPopup();
            } else if (joysticks_[joystickIndex].buttons & CLEM_HOST_JOYSTICK_BUTTON_X) {
                for (unsigned buttonBindingIndex = 0; buttonBindingIndex < 2;
                     ++buttonBindingIndex) {
                    if (bindings.button[buttonBindingIndex] == 2) {
                        bindings.button[buttonBindingIndex] = UINT32_MAX;
                    }
                }
                bindings.button[joystickButtonIndex] = 2;
                ImGui::CloseCurrentPopup();
            } else if (joysticks_[joystickIndex].buttons & CLEM_HOST_JOYSTICK_BUTTON_Y) {
                for (unsigned buttonBindingIndex = 0; buttonBindingIndex < 2;
                     ++buttonBindingIndex) {
                    if (bindings.button[buttonBindingIndex] == 3) {
                        bindings.button[buttonBindingIndex] = UINT32_MAX;
                    }
                }
                bindings.button[joystickButtonIndex] = 3;
                ImGui::CloseCurrentPopup();
            } else if (joysticks_[joystickIndex].buttons & CLEM_HOST_JOYSTICK_BUTTON_L) {
                for (unsigned buttonBindingIndex = 0; buttonBindingIndex < 2;
                     ++buttonBindingIndex) {
                    if (bindings.button[buttonBindingIndex] == 4) {
                        bindings.button[buttonBindingIndex] = UINT32_MAX;
                    }
                }
                bindings.button[joystickButtonIndex] = 4;
                ImGui::CloseCurrentPopup();
            } else if (joysticks_[joystickIndex].buttons & CLEM_HOST_JOYSTICK_BUTTON_R) {
                for (unsigned buttonBindingIndex = 0; buttonBindingIndex < 2;
                     ++buttonBindingIndex) {
                    if (bindings.button[buttonBindingIndex] == 5) {
                        bindings.button[buttonBindingIndex] = UINT32_MAX;
                    }
                }
                bindings.button[joystickButtonIndex] = 5;
                ImGui::CloseCurrentPopup();
            }
            if (ImGui::IsKeyPressed(ImGuiKey_Escape)) {
                ImGui::CloseCurrentPopup();
            }
            ImGui::EndPopup();
        }
        if (!ImGui::IsPopupOpen("##ButtonInputModal")) {
            config_.setDirty();
            joystickModalIndex_ = std::nullopt;
        }
    }
}

bool ClemensFrontend::isEmulatorStarting() const {
    return guiMode_ == GUIMode::RebootEmulator || guiMode_ == GUIMode::StartingEmulator;
}

bool ClemensFrontend::isEmulatorActive() const { return !isEmulatorStarting(); }

void ClemensFrontend::rebootInternal(bool keyfocus) {
    setGUIMode(GUIMode::RebootEmulator);
    delayRebootTimer_ = 0.0f;
    if (isBackendRunning()) {
        stopBackend();
        debugger_.print(ClemensDebugger::Info, "Rebooting machine...");
    } else {
        debugger_.print(ClemensDebugger::Info, "Powering machine...");
    }
    if (keyfocus) {
        spdlog::info("Emulator view keyboard focus requested");
        emulatorShouldHaveKeyboardFocus_ = true;
    }
}

/*
void ClemensFrontend::dispatchClipboardToEmulator() {
    //  characters from clipboard are guaranteed to be iso latin 1 compliant
    //  translate to ADB keystrokes
    //  all ascii characters can derive from a keyboard input
    if (clipboardTextAscii_.empty())
        return;
    uint16_t adbCode = clem_iso_latin_1_to_adb_key_and_modifier(clipboardTextAscii_.front(), 0);
    if (adbCode) {
        uint16_t adbMod = adbCode >> 8;
        ClemensInputEvent evt{};
        evt.type = kClemensInputType_KeyDown;
        if (adbMod & CLEM_ADB_KEY_MOD_STATE_SHIFT) {
            evt.value_a = CLEM_ADB_KEY_LSHIFT;
            backendQueue_.inputEvent(evt);
        }
        if (adbMod & CLEM_ADB_KEY_MOD_STATE_CTRL) {
            evt.value_a = CLEM_ADB_KEY_LCTRL;
            backendQueue_.inputEvent(evt);
        }
        evt.value_a = adbCode & 0xff;
        backendQueue_.inputEvent(evt);

        evt.type = kClemensInputType_KeyUp;
        evt.value_a = adbCode & 0xff;
        backendQueue_.inputEvent(evt);
        if (adbMod & CLEM_ADB_KEY_MOD_STATE_SHIFT) {
            evt.value_a = CLEM_ADB_KEY_LSHIFT;
            backendQueue_.inputEvent(evt);
        }
        if (adbMod & CLEM_ADB_KEY_MOD_STATE_CTRL) {
            evt.value_a = CLEM_ADB_KEY_LCTRL;
            backendQueue_.inputEvent(evt);
        }
    }
    clipboardTextAscii_.pop_front();
}
*/

void ClemensFrontend::onDebuggerCommandReboot() { rebootInternal(false); }

void ClemensFrontend::onDebuggerCommandShutdown() { setGUIMode(GUIMode::ShutdownEmulator); }

void ClemensFrontend::onDebuggerCommandPaste() { pasteClipboardToEmulator_ = true; }
