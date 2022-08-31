#include "clem_backend.hpp"
#include "emulator.h"

#include <cstdlib>
#include <fstream>
#include <optional>

#include "fmt/format.h"

static constexpr unsigned kSlabMemorySize = 32 * 1024 * 1024;

ClemensBackend::ClemensBackend(std::string romPathname, const Config& config) :
  config_(config),
  slabMemory_(kSlabMemorySize, malloc(kSlabMemorySize)),
  emulatorStepsSinceReset_(0) {
  runner_ = std::thread(&ClemensBackend::run, this);
  romBuffer_ = loadROM(romPathname.c_str());

  clemens_host_setup(&machine_, &ClemensBackend::emulatorLog, this);

  switch (config_.type) {
    case Type::Apple2GS:
      initApple2GS();
      break;
  }

  //  TODO: Only use this when opcode debugging is enabled
  clemens_opcode_callback(&machine_, &ClemensBackend::emulatorOpcodeCallback);
}

ClemensBackend::~ClemensBackend() {
  terminate();
  runner_.join();

  free(slabMemory_.getHead());
}

void ClemensBackend::terminate() {
  queueToFront(Command{Command::Terminate});
}

void ClemensBackend::reset() {
  queue(Command{Command::ResetMachine});
}

void ClemensBackend::setRefreshFrequency(unsigned hz) {
  queue(Command{Command::SetHostUpdateFrequency, fmt::format("{}", hz)});
}

void ClemensBackend::queue(const Command& cmd) {
  {
    std::lock_guard<std::mutex> queuelock(commandQueueMutex_);
    commandQueue_.emplace_back(cmd);
  }
  commandQueueCondition_.notify_one();
}

void ClemensBackend::queueToFront(const Command& cmd) {
  {
    std::lock_guard<std::mutex> queuelock(commandQueueMutex_);
    commandQueue_.emplace_front(cmd);
  }
  commandQueueCondition_.notify_one();
}

static int64_t calculateClocksPerTimeslice(unsigned hz) {
    return CLEM_CLOCKS_MEGA2_CYCLE * (int64_t)(CLEM_MEGA2_CYCLES_PER_SECOND) / hz;
}

void ClemensBackend::run() {
  int64_t clocksPerTimeslice = calculateClocksPerTimeslice(60);
  int64_t clocksRemainingInTimeslice = 0;
  std::optional<int> stepsRemaining;
  bool isActive = true;
  bool isMachineReady = false;

  while (isActive) {
    //  Processes all incoming commands from the frontend to control the
    //  emulator.
    std::unique_lock<std::mutex> queuelock(commandQueueMutex_);
    commandQueueCondition_.wait(queuelock,
      [this, &stepsRemaining] {
        //  process commands, or we are in run mode, or in active step mode
        return !commandQueue_.empty() ||
               (stepsRemaining.has_value() && *stepsRemaining > 0);
      });

    while (!commandQueue_.empty() && isActive) {
      auto command = commandQueue_.front();
      commandQueue_.pop_front();
      switch (command.type) {
        case Command::Terminate:
          isActive = false;
          break;
        case Command::ResetMachine:
          resetMachine();
          isMachineReady = true;
          break;
        case Command::SetHostUpdateFrequency:
          clocksPerTimeslice = calculateClocksPerTimeslice(std::stoul(command.operand));
          break;
      }
    }
    queuelock.unlock();

    if (!isActive || !isMachineReady) continue;

    //  Run the emulator in either 'step' or 'run' mode.
    //
    //  RUN MODE executes several instructions in time slices to maximize
    //  performance while providing feedback to the frontend.
    //
    //  STEP MODE executes a single instruction and decrements a 'step' counter
    //
    //  If neither mode is applicable, the emulator holds and this loop will
    //  wait for commands from the frontend
    //
    clocksRemainingInTimeslice += clocksPerTimeslice;
    while (clocksRemainingInTimeslice > 0 &&
           (!stepsRemaining.has_value() || *stepsRemaining > 0)) {

      if (emulatorStepsSinceReset_ >= 2) {
        machine_.cpu.pins.resbIn = true;
      }

      clem_clocks_time_t pre_emulate_time = machine_.clocks_spent;
      clemens_emulate(&machine_);
      clem_clocks_duration_t emulate_step_time = machine_.clocks_spent - pre_emulate_time;

      clocksRemainingInTimeslice -= emulate_step_time;
      if (stepsRemaining.has_value()) {
        *stepsRemaining -= 1;
      }
      ++emulatorStepsSinceReset_;

      //  TODO: MMIO bypass

      //  TODO: check breakpoints, etc

    } // clocksRemainingInTimeslice

    if (stepsRemaining.has_value() && *stepsRemaining == 0) {
      //  if we've finished stepping through code, we are also done with our
      //  timeslice and will wait for a new step/run request
      clocksRemainingInTimeslice = 0;
    }
  } // isActive
}

//  Issues a soft reset to the machine.   Pushes the RESB pin low on the 65816
void ClemensBackend::resetMachine() {
  machine_.cpu.pins.resbIn = false;
  emulatorStepsSinceReset_ = 0;
}


cinek::CharBuffer ClemensBackend::loadROM(const char* romPathname) {
  cinek::CharBuffer romBuffer;
  std::ifstream romFileStream(romPathname, std::ios::binary | std::ios::ate);
  unsigned romMemorySize = 0;

  if (romFileStream.is_open()) {
    romMemorySize = unsigned(romFileStream.tellg());
    romBuffer = cinek::CharBuffer(
      (char *)slabMemory_.allocate(romMemorySize), romMemorySize);
    romFileStream.seekg(0, std::ios::beg);
    romFileStream.read(romBuffer.forwardSize(romMemorySize).first, romMemorySize);
    romFileStream.close();
  }
  return romBuffer;
}

void ClemensBackend::initApple2GS() {
  const unsigned kFPIBankCount = CLEM_IIGS_FPI_MAIN_RAM_BANK_COUNT;
  const uint32_t kClocksPerFastCycle = CLEM_CLOCKS_FAST_CYCLE;
  const uint32_t kClocksPerSlowCycle = CLEM_CLOCKS_MEGA2_CYCLE;
  clemens_init(&machine_, kClocksPerSlowCycle, kClocksPerFastCycle,
               romBuffer_.getHead(), romBuffer_.getSize(),
               slabMemory_.allocate(CLEM_IIGS_BANK_SIZE),
               slabMemory_.allocate(CLEM_IIGS_BANK_SIZE),
               slabMemory_.allocate(CLEM_IIGS_BANK_SIZE * kFPIBankCount),
               slabMemory_.allocate(2048 * 7),    //  TODO: placeholder
               kFPIBankCount);
  loadBRAM();

  //  TODO: It seems the internal audio code expects 2 channel float PCM,
  //        so we only need buffer size and frequency.
  ClemensAudioMixBuffer audioMixBuffer;
  audioMixBuffer.frames_per_second = config_.audioSamplesPerSecond;
  audioMixBuffer.stride = 2 * sizeof(float);
  audioMixBuffer.frame_count = audioMixBuffer.frames_per_second / 4;
  audioMixBuffer.data = (uint8_t*)(
    slabMemory_.allocate(audioMixBuffer.frame_count * audioMixBuffer.stride)
  );
  clemens_assign_audio_mix_buffer(&machine_, &audioMixBuffer);
}

void ClemensBackend::saveBRAM() {
  bool isDirty = false;
  const uint8_t *bram = clemens_rtc_get_bram(&machine_, &isDirty);
  if (!isDirty)
    return;

  std::ofstream bramFile("clem.bram", std::ios::binary);
  if (bramFile.is_open()) {
    bramFile.write((char *)machine_.mmio.dev_rtc.bram, CLEM_RTC_BRAM_SIZE);
  } else {
    //  TODO: display error?
  }
}

void ClemensBackend::loadBRAM() {
  std::ifstream bramFile("clem.bram", std::ios::binary);
  if (bramFile.is_open()) {
    bramFile.read((char *)machine_.mmio.dev_rtc.bram, CLEM_RTC_BRAM_SIZE);
  } else {
    //  TODO: display warning?
  }
}

void ClemensBackend::emulatorLog(int log_level, ClemensMachine *machine,
                                 const char *msg) {
  auto *host = reinterpret_cast<ClemensBackend *>(machine->debug_user_ptr);
}

//  If enabled, this emulator issues this callback per instruction
//  This is great for debugging but should be disabled otherwise
void ClemensBackend::emulatorOpcodeCallback(struct ClemensInstruction *inst,
                                            const char *operand, void *this_ptr) {
  auto *host = reinterpret_cast<ClemensBackend *>(this_ptr);
}
