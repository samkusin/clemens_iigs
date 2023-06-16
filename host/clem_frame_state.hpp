#ifndef CLEM_HOST_FRAME_STATE_HPP
#define CLEM_HOST_FRAME_STATE_HPP

#include "clem_host_shared.hpp"
#include "core/clem_apple2gs_config.hpp"

#include "cinek/buffer.hpp"
#include "cinek/fixedstack.hpp"

#include <optional>
#include <vector>

struct ClemensBackendState {
    ClemensMachine *machine;
    ClemensMMIO *mmio;
    double fps;
    bool isRunning;
    bool isTracing;
    bool isIWMTracing;
    bool mmioWasInitialized;

    ClemensAppleIIGSFrame *frame;

    unsigned hostCPUID;
    int logLevel;
    const ClemensBackendOutputText *logBufferStart;
    const ClemensBackendOutputText *logBufferEnd;
    const ClemensBackendBreakpoint *bpBufferStart;
    const ClemensBackendBreakpoint *bpBufferEnd;
    std::optional<unsigned> bpHitIndex;
    const ClemensBackendExecutedInstruction *logInstructionStart;
    const ClemensBackendExecutedInstruction *logInstructionEnd;

    uint8_t ioPageValues[256]; // 0xc000 - 0xc0ff
    uint8_t debugMemoryPage;

    float machineSpeedMhz;
    float avgVBLsPerFrame;
    bool fastEmulationOn;

    // valid if a debugMessage() command was issued from the frontend
    std::optional<std::string> message;
    // valid if the configuration changed (i.e BRAM, disk status)
    std::optional<ClemensAppleIIGSConfig> config;

    void reset();
};

namespace ClemensFrame {

constexpr uint8_t kIWMStatusDriveSpin = 0x01;
constexpr uint8_t kIWMStatusDrive35 = 0x02;
constexpr uint8_t kIWMStatusDriveAlt = 0x04;
constexpr uint8_t kIWMStatusDriveOn = 0x08;
constexpr uint8_t kIWMStatusDriveWP = 0x10;
constexpr uint8_t kIWMStatusDriveSel = 0x20;
constexpr uint8_t kIWMStatusIWMQ6 = 0x40;
constexpr uint8_t kIWMStatusIWMQ7 = 0x80;

//  Toggle between frame memory buffers so that backendStateDelegate and frame
//  write to and read from separate buffers, to minimize the time we need to
//  keep the frame mutex between the two threads.
struct LogOutputNode {
    int logLevel;
    unsigned sz; // size of log text following this struct in memory
    LogOutputNode *next;
};

struct LogInstructionNode {
    ClemensBackendExecutedInstruction *begin;
    ClemensBackendExecutedInstruction *end;
    LogInstructionNode *next;
};

struct IWMStatus {
    int qtr_track_index;
    unsigned track_byte_index;
    unsigned track_bit_shift;
    unsigned track_bit_length;
    uint8_t buffer[4];
    uint8_t data;
    uint8_t latch;
    uint8_t status;
    uint8_t ph03;

    void copyFrom(ClemensMMIO &mmio, const ClemensDeviceIWM &iwm);
};

struct DOCStatus {
    /** PCM output (floating point per channel) */
    float voice[16];

    uint8_t reg[256];      /**< DOC register values */
    unsigned acc[32];      /**< Oscillator running accumulator */
    uint16_t ptr[32];      /**< Stored pointer from last cycle */
    uint8_t osc_flags[32]; /**< IRQ flagged */

    void copyFrom(const ClemensDeviceEnsoniq &doc);
};

struct ADBStatus {
    unsigned mod_states;
    uint16_t mouse_reg[4];

    void copyFrom(ClemensMMIO &mmio);
};

//  This state sticks around until processed by the UI frame - a hacky solution
//  to the problem mentioned with FrameState.  In some cases we want to know
//  when an event happened (command failed, termination, breakpoint hit, etc)
struct LastCommandState {
    std::vector<ClemensBackendResult> results;
    std::optional<unsigned> hitBreakpoint;
    std::optional<std::string> message;
    std::optional<ClemensAppleIIGSConfig> gsConfig;
    LogOutputNode *logNode = nullptr;
    LogOutputNode *logNodeTail = nullptr;
    LogInstructionNode *logInstructionNode = nullptr;
    LogInstructionNode *logInstructionNodeTail = nullptr;
    cinek::ByteBuffer audioBuffer;
    bool isFastEmulationOn;
};

// This state comes in for any update to the emulator per frame.  As such
// its possible to "lose" state if the emulator runs faster than the UI.
// This is OK in most cases as the UI will only present this data per frame
struct FrameState {
    ClemensClock emulatorClock;
    Clemens65C816 cpu;
    ClemensAppleIIGSFrame frame;
    std::array<std::string, CLEM_CARD_SLOT_COUNT> cards;

    IWMStatus iwm;
    DOCStatus doc;
    ADBStatus adb;

    uint8_t *memoryView = nullptr;
    uint8_t *docRAM = nullptr;
    uint8_t *ioPage = nullptr;
    uint8_t *bram = nullptr;
    uint8_t *e0bank = nullptr;
    uint8_t *e1bank = nullptr;

    ClemensBackendBreakpoint *breakpoints = nullptr;
    unsigned breakpointCount = 0;
    int logLevel;

    float machineSpeedMhz;
    float avgVBLsPerFrame;
    uint32_t vgcModeFlags;
    uint32_t irqs, nmis;
    uint8_t memoryViewBank = 0;

    unsigned backendCPUID;
    float fps;
    bool mmioWasInitialized = false;
    bool isTracing = false;
    bool isIWMTracing = false;
    bool isRunning = false;

    void copyState(const ClemensBackendState &state, LastCommandState &commandState,
                   cinek::FixedStack &frameMemory);
};
} // namespace ClemensFrame

#endif
