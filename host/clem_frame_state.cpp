#include "clem_frame_state.hpp"

#include "clem_defs.h"
#include "clem_mem.h"
#include "core/clem_apple2gs_config.hpp"
#include "emulator.h"
#include "emulator_mmio.h"

#include "fmt/core.h"

#include <algorithm>
#include <cstring>

void ClemensBackendState::reset() {
    config.reset();
    message.reset();
    frame = ClemensAppleIIGSFrame();
}

namespace ClemensFrame {

void IWMStatus::copyFrom(ClemensMMIO &mmio, const ClemensDeviceIWM &iwm, bool detailed) {
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
    if (iwm.io_flags & CLEM_IWM_FLAG_HEAD_SEL) {
        status |= kIWMStatusDriveSel;
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
    if (iwm.io_flags & CLEM_IWM_FLAG_WRPROTECT_SENSE) {
        status |= kIWMStatusDriveWP;
    }
    data = iwm.data_r;
    data_w = iwm.data_w;
    latch = iwm.latch;
    async_mode = iwm.async_mode ? 1 : 0;
    cell_time = iwm.fast_mode ? 2 : 4;
    has_disk = iwmDrive->has_disk ? 1 : 0;

    ph03 = (uint8_t)(iwm.out_phase & 0xff);
    if (!iwmDrive) {
        qtr_track_index = -1; //  no disk
        return;
    }
    if (iwmDrive->is_spindle_on) {
        status |= kIWMStatusDriveSpin;
    }
    qtr_track_index = iwmDrive->qtr_track_index;
    track_byte_index = iwmDrive->track_byte_index;
    track_bit_shift = iwmDrive->track_bit_shift;
    track_bit_length = iwmDrive->track_bit_length;

    if (!iwmDrive->has_disk || !detailed ||
        iwmDrive->disk.meta_track_map[qtr_track_index] == 0xff) {
        if (detailed) {
            memset(buffer, 0, sizeof(buffer));
        }
        return;
    }
    unsigned diskTrackIndex = iwmDrive->disk.meta_track_map[qtr_track_index];
    if (!iwmDrive->disk.track_initialized[diskTrackIndex]) {
        memset(buffer, 0, sizeof(buffer));
        return;
    }

    //  copy window from disk to buffer
    //  step 1 - bytes from disk buffer.
    //  buffer_mid = sizeof(buffer) / 2;
    //  window = [track_byte_index - buffer_mid : sizeof(buffer)]
    //  if window.start < 0:
    //      window = [track_byte_count + window.start, -window.start]
    //      buffer_bit_start_index = window.start * 8;
    //      do_copy_window(window)
    //      window = [0, sizeof(buffer) - window.start]
    //  else:
    //      buffer_bit_start_index = window.start * 8
    //  do_copy_window(window)
    constexpr auto diskBufferSize = sizeof(buffer);
    constexpr auto diskBufferMid = diskBufferSize / 2;
    const uint8_t *diskBits =
        iwmDrive->disk.bits_data + iwmDrive->disk.track_byte_offset[diskTrackIndex];
    unsigned trackByteCount = (iwmDrive->disk.track_bits_count[diskTrackIndex] + 7) / 8;
    int startIndex = int(track_byte_index - diskBufferMid);
    int runLength;
    int runCount = 0;
    if (startIndex < 0) {
        //  split window
        runLength = -startIndex;
        startIndex = trackByteCount - runLength;
        buffer_bit_start_index = startIndex * 8;
        assert(runCount + runLength <= (int)diskBufferSize);
        memcpy(buffer + runCount, diskBits + startIndex, runLength);
        runCount += runLength;
        runLength = diskBufferSize - runLength;
        startIndex = 0;
    } else {
        //  single window
        runLength = int(diskBufferSize);
        runCount = 0;
        buffer_bit_start_index = startIndex * 8;
    }
    assert(runCount + runLength <= (int)diskBufferSize);
    memcpy(buffer + runCount, diskBits + startIndex, runLength);
}

void DOCStatus::copyFrom(const ClemensDeviceEnsoniq &doc) {
    memcpy(voice, doc.voice, sizeof(voice));
    memcpy(reg, doc.reg, sizeof(reg));
    memcpy(acc, doc.acc, sizeof(acc));
    memcpy(ptr, doc.ptr, sizeof(ptr));
    memcpy(osc_flags, doc.osc_flags, sizeof(osc_flags));
}

void ADBStatus::copyFrom(ClemensMMIO &mmio) {
    mod_states = clemens_get_adb_key_modifier_states(&mmio);
    for (int i = 0; i < 4; ++i) {
        mouse_reg[i] = mmio.dev_adb.mouse_reg[i];
    }
}

void FrameState::copyState(const ClemensBackendState &state, LastCommandState &commandState,
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
    frame.monitor = state.frame.monitor;
    //  copy scanlines as this data may become invalid on a frame-to-frame
    //  basis
    frame.text = state.frame.text;
    if (frame.text.format != kClemensVideoFormat_None) {
        frame.text.scanlines =
            frameMemory.allocateArray<ClemensScanline>(state.frame.text.scanline_limit);
        memcpy(frame.text.scanlines, state.frame.text.scanlines,
               sizeof(ClemensScanline) * state.frame.text.scanline_limit);
    }
    frame.graphics = state.frame.graphics;
    if (frame.graphics.format != kClemensVideoFormat_None) {
        frame.graphics.scanlines =
            frameMemory.allocateArray<ClemensScanline>(state.frame.graphics.scanline_limit);
        memcpy(frame.graphics.scanlines, state.frame.graphics.scanlines,
               sizeof(ClemensScanline) * state.frame.graphics.scanline_limit);
        // need to save off the rgb color buffer since the original memory belongs
        // to the backend
        if (state.frame.graphics.rgb_buffer_size > 0) {
            frame.graphics.rgb_buffer_size = state.frame.graphics.rgb_buffer_size;
            frame.graphics.rgb =
                frameMemory.allocateArray<uint16_t>(frame.graphics.rgb_buffer_size);
            memcpy(frame.graphics.rgb, state.frame.graphics.rgb,
                   state.frame.graphics.rgb_buffer_size);
        }
    }
    frame.diskDriveStatuses = state.frame.diskDriveStatuses;
    frame.smartPortStatuses = state.frame.smartPortStatuses;

    e0bank = frameMemory.allocateArray<uint8_t>(CLEM_IIGS_BANK_SIZE);
    clemens_out_bin_data(state.machine, e0bank, CLEM_IIGS_BANK_SIZE, 0xe0, 0x0000);
    e1bank = frameMemory.allocateArray<uint8_t>(CLEM_IIGS_BANK_SIZE);
    clemens_out_bin_data(state.machine, e1bank, CLEM_IIGS_BANK_SIZE, 0xe1, 0x0000);

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
    iwm.copyFrom(*state.mmio, state.mmio->dev_iwm, !state.isRunning);
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
        for (unsigned addr = 0; addr < CLEM_IIGS_BANK_SIZE; ++addr) {
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
    commandState.isFastModeOn = state.fastModeEnabled;

    machineSpeedMhz = state.machineSpeedMhz;
    emulationSpeedMhz = state.emulationSpeedMhz;
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

} // namespace ClemensFrame
