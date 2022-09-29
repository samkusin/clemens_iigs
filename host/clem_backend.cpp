#include "clem_backend.hpp"
#include "clem_disk_utils.hpp"
#include "clem_host_platform.h"
#include "emulator.h"
#include "clem_mem.h"

#include <charconv>
#include <chrono>
#include <cstdarg>
#include <cstdlib>
#include <fstream>
#include <optional>

#include "fmt/format.h"

static constexpr unsigned kSlabMemorySize = 32 * 1024 * 1024;
static constexpr unsigned kLogOutputLineLimit = 1024;

struct ClemensRunSampler {
  //  our method of keeping the simulation in sync with real time is to rely
  //  on two counters
  //    - sDT = simulation counter that ticks at a fixed rate, and
  //    - aDT = real-time timer that counts the actual time passed
  //
  //  if aDT < sDT, then sleep the amount of time needed to catch up with sDT
  //  if aDT >= sDT, then clamp aDT to sDT
  std::chrono::microseconds fixedTimeInterval;
  std::chrono::microseconds actualTimeInterval;
  std::chrono::microseconds sampledFrameTime;
  unsigned sampledFrameCount;
  double sampledFramesPerSecond;

  ClemensRunSampler() {
    reset();
  }

  void reset() {
    fixedTimeInterval = std::chrono::microseconds::zero();
    actualTimeInterval = std::chrono::microseconds::zero();
    sampledFrameTime = std::chrono::microseconds::zero();
    sampledFrameCount = 0;
    sampledFramesPerSecond = 0.0f;
  }

  void update(std::chrono::microseconds fixedFrameInterval,
              std::chrono::microseconds actualFrameInterval) {
    fixedTimeInterval += fixedFrameInterval;
    actualTimeInterval += actualFrameInterval;
    if (actualTimeInterval < fixedTimeInterval) {
      std::this_thread::sleep_for(fixedTimeInterval - actualTimeInterval);
      fixedTimeInterval -= actualTimeInterval;
      actualTimeInterval = std::chrono::microseconds::zero();
    } else {
      std::this_thread::yield();
      if (actualTimeInterval - fixedFrameInterval > std::chrono::microseconds(500000)) {
        actualTimeInterval = fixedTimeInterval = std::chrono::microseconds::zero();
      }
    }

    sampledFrameTime += actualFrameInterval;
    ++sampledFrameCount;

    if (sampledFrameTime >= std::chrono::microseconds(500000)) {
      sampledFramesPerSecond = sampledFrameCount * 1e6 / sampledFrameTime.count();
      sampledFrameTime = std::chrono::microseconds::zero();
      sampledFrameCount = 0;
    }
  }
};

template<typename ...Args>
void ClemensBackend::localLog(int log_level, const char* msg, Args... args) {
  if (logOutput_.size() >= kLogOutputLineLimit) return;
  ClemensBackendOutputText logLine{log_level,};
  logLine.text = fmt::format(msg, args...);
  logOutput_.emplace_back(logLine);
}

ClemensBackend::ClemensBackend(std::string romPathname, const Config& config,
                               PublishStateDelegate publishDelegate) :
  config_(config),
  slabMemory_(kSlabMemorySize, malloc(kSlabMemorySize)),
  logLevel_(CLEM_DEBUG_LOG_INFO),
  debugMemoryPage_(0x00),
  areInstructionsLogged_(false) {

  loggedInstructions_.reserve(10000);

  initEmulatedDiskLocalStorage();

  romBuffer_ = loadROM(romPathname.c_str());

  memset(&machine_, 0, sizeof(machine_));
  clemens_host_setup(&machine_, &ClemensBackend::emulatorLog, this);

  switch (config_.type) {
    case ClemensBackendConfig::Type::Apple2GS:
      initApple2GS();
      break;
  }

  //  TODO: Only use this when opcode debugging is enabled
  clemens_opcode_callback(&machine_, &ClemensBackend::emulatorOpcodeCallback);

  for (size_t driveIndex = 0; driveIndex < diskDrives_.size(); ++driveIndex) {
    if (diskDrives_[driveIndex].imagePath.empty()) continue;
    auto driveType = static_cast<ClemensDriveType>(driveIndex);
    if (!loadDisk(driveType)) {
      fmt::print("Failed to load image '{}' into drive {}\n",
                 diskDrives_[driveIndex].imagePath,
                 ClemensDiskUtilities::getDriveName(driveType));
    } else {
      printf("Failed to load image '{}' into drive {}\n",
             diskDrives_[driveIndex].imagePath,
             ClemensDiskUtilities::getDriveName(driveType));
    }
  }

  //  Everything is ready for the main thread
  runner_ = std::thread(&ClemensBackend::main, this, std::move(publishDelegate));
}

ClemensBackend::~ClemensBackend() {
  terminate();
  runner_.join();

  saveBRAM();

  free(slabMemory_.getHead());
}

void ClemensBackend::terminate() {
  queueToFront(Command{Command::Terminate});
}

void ClemensBackend::reset() {
  queue(Command{Command::ResetMachine});
}

void ClemensBackend::resetMachine() {
  machine_.cpu.pins.resbIn = false;
  machine_.resb_counter = 3;
}

void ClemensBackend::setRefreshFrequency(unsigned hz) {
  queue(Command{Command::SetHostUpdateFrequency, fmt::format("{}", hz)});
}

void ClemensBackend::run() {
  queue(Command{Command::RunMachine});
}

void ClemensBackend::step(unsigned count) {
  queue(Command{Command::StepMachine, fmt::format("{}", count)});
}

unsigned ClemensBackend::stepMachine(const std::string_view &inputParam) {
  unsigned count;
  if (std::from_chars(inputParam.data(), inputParam.data() + inputParam.size(),
                      count).ec != std::errc{}) {
    return 0;
  }
  return count;
}

void ClemensBackend::publish() {
  queue(Command{Command::Publish});
}

void ClemensBackend::insertDisk(ClemensDriveType driveType, std::string diskPath) {
  queue(Command{Command::InsertDisk,
                fmt::format("{}={}", ClemensDiskUtilities::getDriveName(driveType), diskPath)});
}

bool ClemensBackend::insertDisk(const std::string_view& inputParam) {
  auto sepPos = inputParam.find('=');
  if (sepPos == std::string_view::npos) return false;
  auto driveName = inputParam.substr(0, sepPos);
  auto imagePath = inputParam.substr(sepPos + 1);
  auto driveType = ClemensDiskUtilities::getDriveType(driveName);
  if (driveType == kClemensDrive_Invalid) return false;

  diskDrives_[driveType].imagePath = imagePath;
  return loadDisk(driveType);
}

void ClemensBackend::ejectDisk(ClemensDriveType driveType) {
  queue(Command{Command::EjectDisk,
                std::string(ClemensDiskUtilities::getDriveName(driveType))});
}

void ClemensBackend::ejectDisk(const std::string_view& inputParam) {
  auto driveType = ClemensDiskUtilities::getDriveType(inputParam);
  diskDrives_[driveType].isEjecting = true;
}

void ClemensBackend::writeProtectDisk(ClemensDriveType driveType, bool wp) {
  queue(Command{Command::WriteProtectDisk,
                fmt::format("{},{}", ClemensDiskUtilities::getDriveName(driveType),
                                     wp ? 1 : 0)});
}

bool ClemensBackend::writeProtectDisk(const std::string_view& inputParam) {
  auto sepPos = inputParam.find(',');
  if (sepPos == std::string_view::npos) return false;
  auto driveParam = inputParam.substr(0, sepPos);
  auto driveType = ClemensDiskUtilities::getDriveType(driveParam);
  auto* drive = clemens_drive_get(&machine_, driveType);
  if (!drive || !drive->has_disk) return false;

  auto enableParam = inputParam.substr(sepPos+1);
  drive->disk.is_write_protected = enableParam == "1";

  return true;
}

void ClemensBackend::debugMemoryPage(uint8_t page) {
  queue(Command{Command::DebugMemoryPage, std::to_string(page)});
}

void ClemensBackend::debugMemoryWrite(uint16_t addr, uint8_t value) {
  queue(Command{Command::WriteMemory, fmt::format("{}={}", addr, value)});
}

void ClemensBackend::writeMemory(const std::string_view& inputParam) {
  auto sepPos = inputParam.find('=');
  if (sepPos == std::string_view::npos) {
    return;
  }
  uint16_t addr;
  if (std::from_chars(inputParam.data(), inputParam.data() + sepPos, addr).ec != std::errc{})
    return;
  auto valStr = inputParam.substr(sepPos+1);
  uint8_t value;
  if (std::from_chars(valStr.data(), valStr.data() + valStr.size(), value).ec != std::errc{})
    return;

  clem_write(&machine_, value, addr, debugMemoryPage_, CLEM_MEM_FLAG_NULL);
}

void ClemensBackend::debugLogLevel(int logLevel) {
  queue(Command{Command::DebugLogLevel, fmt::format("{}", logLevel)});
}

void ClemensBackend::debugMessage(std::string msg) {
  queue(Command{Command::DebugMessage, std::move(msg)});
}

bool ClemensBackend::loadDisk(ClemensDriveType driveType) {
  diskBuffer_.reset();

  std::ifstream input(diskDrives_[driveType].imagePath,
                      std::ios_base::in | std::ios_base::binary);
  if (input.is_open()) {
    input.seekg(0, std::ios_base::end);
    size_t inputImageSize = input.tellg();
    if (inputImageSize <= diskBuffer_.getCapacity()) {
      auto bits = diskBuffer_.forwardSize(inputImageSize);
      input.seekg(0);
      input.read((char *)bits.first, inputImageSize);
      if (input.good()) {
        diskContainers_[driveType].nib = &disks_[driveType];
        auto parseBuffer = cinek::ConstCastRange<uint8_t>(bits);
        if (ClemensDiskUtilities::parseWOZ(&diskContainers_[driveType], parseBuffer)) {
          if (clemens_assign_disk(&machine_, driveType, &disks_[driveType])) {
            return true;
          }
        }
      }
    }
  }
  diskDrives_[driveType].imagePath.clear();
  return false;
}

bool ClemensBackend::saveDisk(ClemensDriveType driveType) {
  diskBuffer_.reset();
  auto writeOut = diskBuffer_.forwardSize(diskBuffer_.getCapacity());

  size_t writeOutCount = cinek::length(writeOut);
  if (!clem_woz_serialize(&diskContainers_[driveType], writeOut.first, &writeOutCount)) {
    return false;
  }
  std::ofstream out(diskDrives_[driveType].imagePath,
                    std::ios_base::out | std::ios_base::binary);
  if (out.fail()) return false;
  out.write((char*)writeOut.first, writeOutCount);
  if (out.fail() || out.bad()) return false;
  return true;
}

static const char* sInputKeys[] = {
  "",
  "keyD",
  "keyU",
  "mouseD",
  "mouseU",
  NULL
};

void ClemensBackend::inputEvent(const ClemensInputEvent& input) {
  CK_ASSERT_RETURN(*sInputKeys[input.type] != '\0');
  queue(Command{
    Command::Input,
    fmt::format("{}={},{}", sInputKeys[input.type], input.value,
                input.adb_key_toggle_mask)}
  );
}

void ClemensBackend::inputMachine(const std::string_view& inputParam) {
  if (!clemens_is_mmio_initialized(&machine_)) {
    return;
  }
  auto equalsTokenPos = inputParam.find('=');
  auto name = inputParam.substr(0, equalsTokenPos);
  auto value = inputParam.substr(equalsTokenPos+1);
  for (const char** keyName = &sInputKeys[0]; *keyName != NULL; ++keyName) {
    if (name == *keyName) {
      ClemensInputEvent inputEvent;
      inputEvent.type = (ClemensInputType)((int)(keyName - &sInputKeys[0]));
      //  oh why, oh why no straightforward C++ conversion from std::string_view
      //  to number
      auto commaPos = value.find(',');
      auto inputValue = value.substr(0, commaPos);
      auto inputModifiers = value.substr(commaPos + 1);
      inputEvent.value = std::stoul(std::string(inputValue));
      inputEvent.adb_key_toggle_mask = std::stoul(std::string(inputModifiers));
      clemens_input(&machine_, &inputEvent);
    }
  }
}

void ClemensBackend::breakExecution() {
  queue(Command{Command::Break});
}

void ClemensBackend::addBreakpoint(const ClemensBackendBreakpoint& breakpoint) {
  Command cmd;
  cmd.operand = fmt::format("{:s}:{:06X}",
    breakpoint.type == ClemensBackendBreakpoint::DataRead ? "r" :
    breakpoint.type == ClemensBackendBreakpoint::Write ? "w" : "", breakpoint.address);
  cmd.type = Command::AddBreakpoint;
  queue(std::move(cmd));
}

bool ClemensBackend::addBreakpoint(const std::string_view& inputParam) {
  auto sepPos = inputParam.find(':');
  assert(sepPos != std::string_view::npos);
  auto type = inputParam.substr(0, sepPos);
  auto address = inputParam.substr(sepPos+1);
  ClemensBackendBreakpoint bp;
  if (type == "r") {
    bp.type = ClemensBackendBreakpoint::DataRead;
  } else if (type == "w") {
    bp.type = ClemensBackendBreakpoint::Write;
  } else {
    bp.type = ClemensBackendBreakpoint::Execute;
  }
  bp.address = (uint32_t)std::stoul(std::string(address), nullptr, 16);
  auto range = std::equal_range(breakpoints_.begin(), breakpoints_.end(), bp,
    [](const ClemensBackendBreakpoint& bp, const ClemensBackendBreakpoint& newBp) {
      return bp.address < newBp.address;
    });
  while (range.first != range.second) {
    if (range.first->type == bp.type) {
      break;
    }
    range.first++;
  }
  if (range.first == range.second) {
    breakpoints_.emplace(range.second, bp);
  }
  return true;
}

void ClemensBackend::removeBreakpoint(unsigned index) {
  queue(Command{Command::DelBreakpoint, std::to_string(index)});
}

bool ClemensBackend::delBreakpoint(const std::string_view& inputParam) {
  if (inputParam.empty()) {
    breakpoints_.clear();
    return true;
  }
  unsigned index = (unsigned)std::stoul(std::string(inputParam));
  if (index >= breakpoints_.size()) {
    return false;
  }
  breakpoints_.erase(breakpoints_.begin() + index);
  return true;
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

static int64_t calculateClocksPerTimeslice(ClemensMachine* machine, unsigned hz) {
  bool is_machine_slow;
  return int64_t(clemens_clocks_per_second(machine, &is_machine_slow) / hz);
}

void ClemensBackend::main(PublishStateDelegate publishDelegate) {
  int64_t clocksRemainingInTimeslice = 0;
  std::optional<int> stepsRemaining = 0;
  bool isTerminated = false;
  bool isMachineReady = false;

  ClemensRunSampler runSampler;


  uint64_t publishSeqNo = 0;
  unsigned emulatorRefreshFrequency = 60;
  auto fixedFrameInterval =
    std::chrono::microseconds((long)std::floor(1e6/emulatorRefreshFrequency));
  auto lastFrameTimePoint = std::chrono::high_resolution_clock::now();
  while (!isTerminated) {
    bool isRunning = !stepsRemaining.has_value() || *stepsRemaining > 0;
    bool publishState = false;
    bool updateSeqNo = false;
    std::optional<bool> commandFailed;
    std::optional<Command::Type> commandType;
    std::optional<std::string> debugMessage;

    std::unique_lock<std::mutex> queuelock(commandQueueMutex_);
    if (!isRunning) {
      //  waiting for commands
      commandQueueCondition_.wait(queuelock,
        [this, &stepsRemaining] {
          return !commandQueue_.empty();
        });
    }
    //  TODO: we may just be able to use a vector for the command queue and
    //        create a local copy of the queue to minimize time spent executing
    //        commands.   the mutex seems to be used just for adds to the
    //        command queue.
    while (!commandQueue_.empty() && !isTerminated) {
      auto command = commandQueue_.front();
      commandQueue_.pop_front();
      if (command.type != Command::Publish && command.type != Command::Input) {
        if (!commandFailed.has_value()) {
          commandFailed = false;
        }
      }
      switch (command.type) {
        case Command::Terminate:
          isTerminated = true;
          break;
        case Command::ResetMachine:
          resetMachine();
          isMachineReady = true;
          break;
        case Command::SetHostUpdateFrequency:
          emulatorRefreshFrequency = std::stoul(command.operand);
          if (emulatorRefreshFrequency) {
            fixedFrameInterval =
              std::chrono::microseconds((long)std::floor(1e6/emulatorRefreshFrequency));
          } else {
            fixedFrameInterval = std::chrono::microseconds::zero();
          }
          break;
        case Command::RunMachine:
          stepsRemaining = std::nullopt;
          isRunning = true;
          runSampler.reset();
          break;
        case Command::StepMachine:
          *stepsRemaining = stepMachine(command.operand);
          isRunning = true;
          runSampler.reset();
          break;
        case Command::Publish:
          publishState = true;
          break;
        case Command::InsertDisk:
          if (!insertDisk(command.operand)) commandFailed = true;
          break;
        case Command::EjectDisk:
          ejectDisk(command.operand);
          break;
        case Command::WriteProtectDisk:
          writeProtectDisk(command.operand);
          break;
        case Command::Input:
          inputMachine(command.operand);
          break;
        case Command::Break:
          stepsRemaining = 0;
          break;
        case Command::AddBreakpoint:
          if (!addBreakpoint(command.operand)) commandFailed = true;
          break;
        case Command::DelBreakpoint:
          if (!delBreakpoint(command.operand)) commandFailed = true;
          break;
        case Command::DebugMemoryPage:
          debugMemoryPage_ = (uint8_t)(std::stoul(command.operand));
          break;
        case Command::WriteMemory:
          writeMemory(command.operand);
          break;
        case Command::DebugLogLevel:
          logLevel_ = (int)(std::stol(command.operand));
          break;
        case Command::DebugMessage:
          debugMessage = std::move(command.operand);
          break;
      }
      if (commandFailed.has_value() && *commandFailed == true && !commandType.has_value()) {
        commandType = command.type;
      }
    }
    queuelock.unlock();

    //  TODO: these edge cases seem sloppy - but we'll need to prevent the
    //        thread from spinning if the machine will not run
    if (!isMachineReady || emulatorRefreshFrequency == 0) {
      std::this_thread::sleep_for(std::chrono::milliseconds(15));
      continue;
    }
    //  if isRunning is false, we use a condition var/wait to hold the thread
    std::optional<unsigned> hitBreakpoint;
    if (isRunning && !isTerminated) {
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
      areInstructionsLogged_ = stepsRemaining.has_value() && (*stepsRemaining > 0);

      int64_t clocksPerTimeslice =
        calculateClocksPerTimeslice(&machine_, emulatorRefreshFrequency);
      clocksRemainingInTimeslice += clocksPerTimeslice;
      while (clocksRemainingInTimeslice > 0 &&
            (!stepsRemaining.has_value() || *stepsRemaining > 0)) {
        clem_clocks_time_t pre_emulate_time = machine_.clocks_spent;
        clemens_emulate(&machine_);
        clem_clocks_duration_t emulate_step_time = machine_.clocks_spent - pre_emulate_time;
        clocksRemainingInTimeslice -= emulate_step_time;
        if (stepsRemaining.has_value()) {
          stepsRemaining = *stepsRemaining - 1;
        }

        //  TODO: MMIO bypass

        //  TODO: check breakpoints, etc

        if (!breakpoints_.empty()) {
          if ((hitBreakpoint = checkHitBreakpoint()).has_value()) {
            stepsRemaining = 0;
            break;
          }
        }
      } // clocksRemainingInTimeslice


      if (stepsRemaining.has_value() && *stepsRemaining == 0) {
        //  if we've finished stepping through code, we are also done with our
        //  timeslice and will wait for a new step/run request
        clocksRemainingInTimeslice = 0;
        isRunning = false;
        areInstructionsLogged_ = false;
      }

      auto currentFrameTimePoint = std::chrono::high_resolution_clock::now();
      auto actualFrameInterval = std::chrono::duration_cast<std::chrono::microseconds>(
        currentFrameTimePoint - lastFrameTimePoint);
      lastFrameTimePoint = currentFrameTimePoint;

      runSampler.update(fixedFrameInterval, actualFrameInterval);

      for (auto diskDriveIt = diskDrives_.begin(); diskDriveIt != diskDrives_.end(); ++diskDriveIt) {
        auto& diskDrive = *diskDriveIt;
        auto driveIndex = unsigned(diskDriveIt - diskDrives_.begin());
        auto driveType = static_cast<ClemensDriveType>(driveIndex);
        auto* clemensDrive = clemens_drive_get(&machine_, driveType);
        diskDrive.isSpinning = clemensDrive->is_spindle_on;
        diskDrive.isWriteProtected = clemensDrive->disk.is_write_protected;
        diskDrive.saveFailed = false;
        if (diskDrive.isEjecting) {
          if (clemens_eject_disk_async(&machine_, driveType, &disks_[driveIndex])) {
            diskDrive.isEjecting = false;
            if (!saveDisk(driveType)) diskDrive.saveFailed = true;
            diskDrive.imagePath.clear();
          }
        }
      }
      updateSeqNo = true;
    }

    if (isTerminated) {
      //  eject and save all disks
      for (auto diskDriveIt = diskDrives_.begin(); diskDriveIt != diskDrives_.end(); ++diskDriveIt) {
        auto& diskDrive = *diskDriveIt;
        if (diskDrive.imagePath.empty()) continue;

        auto driveIndex = unsigned(diskDriveIt - diskDrives_.begin());
        auto driveType = static_cast<ClemensDriveType>(driveIndex);
        auto* clemensDrive = clemens_drive_get(&machine_, driveType);
        if (clemens_eject_disk_async(&machine_, driveType, &disks_[driveIndex])) {
          saveDisk(driveType);
          localLog(CLEM_DEBUG_LOG_INFO, "Saved {}", diskDrive.imagePath);
        }
      }
      publishState = true;
      updateSeqNo = true;
    }

    updateSeqNo = updateSeqNo || commandFailed.has_value();

    if (updateSeqNo) {
      publishSeqNo++;
    }

    //  TODO: publish emulator state using a callback
    //        the recipient will synchronize with UI accordingly with the
    //        assumption that once the callback returns, we can alter the state
    //        again as needed next timeslice.
    if (publishState) {
      ClemensBackendState publishedState {};
      publishedState.mmio_was_initialized = clemens_is_mmio_initialized(&machine_);
      if (publishedState.mmio_was_initialized) {
        clemens_get_monitor(&publishedState.monitor, &machine_);
        clemens_get_text_video(&publishedState.text, &machine_);
        clemens_get_graphics_video(&publishedState.graphics, &machine_);
        clemens_get_audio(&publishedState.audio, &machine_);
      }
      publishedState.isRunning = isRunning;
      publishedState.machine = &machine_;
      publishedState.seqno = publishSeqNo;
      publishedState.fps = runSampler.sampledFramesPerSecond;
      publishedState.hostCPUID = clem_host_get_processor_number();
      publishedState.logLevel = logLevel_;
      publishedState.logBufferStart = logOutput_.data();
      publishedState.logBufferEnd = logOutput_.data() + logOutput_.size();
      publishedState.logInstructionStart = loggedInstructions_.data();
      publishedState.logInstructionEnd = loggedInstructions_.data() + loggedInstructions_.size();
      publishedState.bpBufferStart = breakpoints_.data();
      publishedState.bpBufferEnd = breakpoints_.data() + breakpoints_.size();
      if (hitBreakpoint.has_value()) {
        publishedState.bpHitIndex = *hitBreakpoint;
      }
      publishedState.diskDrives = diskDrives_.data();
      publishedState.commandFailed = std::move(commandFailed);
      publishedState.commandType = std::move(commandType);
      publishedState.message = std::move(debugMessage);
      if (isTerminated) {
        publishedState.terminated = isTerminated;
      }
      //  read IO memory from bank 0xe0 which ignores memory shadow settings
      for (uint16_t ioAddr = 0xc000; ioAddr < 0xc0ff; ++ioAddr) {
        clem_read(&machine_, &publishedState.ioPageValues[ioAddr - 0xc000],
                  ioAddr, 0xe0, CLEM_MEM_FLAG_NULL);
      }
      publishedState.debugMemoryPage = debugMemoryPage_;

      publishDelegate(publishedState);
      if (publishedState.mmio_was_initialized) {
        clemens_audio_next_frame(&machine_, publishedState.audio.frame_count);
      }
      logOutput_.clear();
      loggedInstructions_.clear();
    }
  } // !isTerminated

  printf("Terminated backend refresh thread.\n");
}

std::optional<unsigned> ClemensBackend::checkHitBreakpoint() {
  for (auto it = breakpoints_.begin(); it != breakpoints_.end(); ++it) {
    uint16_t b_adr = (uint16_t)(it->address & 0xffff);
    uint8_t b_bank = (uint8_t)(it->address >> 16);
    unsigned index = (unsigned)(it - breakpoints_.begin());
    switch (it->type) {
      case ClemensBackendBreakpoint::Execute:
        if (machine_.cpu.regs.PBR == b_bank && machine_.cpu.regs.PC == b_adr) {
          return index;
        }
        break;
      case ClemensBackendBreakpoint::DataRead:
      case ClemensBackendBreakpoint::Write:
        if (machine_.cpu.regs.DBR == b_bank && machine_.cpu.pins.adr == b_adr) {
          if (machine_.cpu.pins.vdaOut) {
            if (it->type == ClemensBackendBreakpoint::DataRead &&
                machine_.cpu.pins.rwbOut) {
              return index;
            } else if (it->type == ClemensBackendBreakpoint::Write &&
                       !machine_.cpu.pins.rwbOut) {
              return index;
            }
          }
        }
        break;
    }
  }
  return std::nullopt;
}

void ClemensBackend::initEmulatedDiskLocalStorage() {
  diskBuffer_ =
    cinek::ByteBuffer((uint8_t*)slabMemory_.allocate(CLEM_DISK_35_MAX_DATA_SIZE),
                      CLEM_DISK_35_MAX_DATA_SIZE + 4096);
  disks_[kClemensDrive_3_5_D1].bits_data =
    (uint8_t*)slabMemory_.allocate(CLEM_DISK_35_MAX_DATA_SIZE);
  disks_[kClemensDrive_3_5_D1].bits_data_end =
    disks_[kClemensDrive_3_5_D1].bits_data + CLEM_DISK_35_MAX_DATA_SIZE;
  disks_[kClemensDrive_3_5_D2].bits_data =
    (uint8_t*)slabMemory_.allocate(CLEM_DISK_35_MAX_DATA_SIZE);
  disks_[kClemensDrive_3_5_D2].bits_data_end =
    disks_[kClemensDrive_3_5_D2].bits_data + CLEM_DISK_35_MAX_DATA_SIZE;

  disks_[kClemensDrive_5_25_D1].bits_data =
    (uint8_t*)slabMemory_.allocate(CLEM_DISK_525_MAX_DATA_SIZE);
  disks_[kClemensDrive_5_25_D1].bits_data_end =
    disks_[kClemensDrive_5_25_D1].bits_data + CLEM_DISK_525_MAX_DATA_SIZE;
  disks_[kClemensDrive_5_25_D2].bits_data =
    (uint8_t*)slabMemory_.allocate(CLEM_DISK_525_MAX_DATA_SIZE);
  disks_[kClemensDrive_5_25_D2].bits_data_end =
    disks_[kClemensDrive_5_25_D2].bits_data + CLEM_DISK_525_MAX_DATA_SIZE;

  //  some sanity values to initialize
  for (size_t driveIndex = 0; driveIndex < diskDrives_.size(); ++driveIndex) {
    auto& diskDrive = diskDrives_[driveIndex];
    diskDrive.isEjecting = false;
    diskDrive.isSpinning = false;
    diskDrive.isWriteProtected = true;
    diskDrive.saveFailed = false;
    diskDrive.imagePath = config_.diskDriveStates[driveIndex].imagePath;
  }
}

cinek::ByteBuffer ClemensBackend::loadROM(const char* romPathname) {
  cinek::ByteBuffer romBuffer;
  std::ifstream romFileStream(romPathname, std::ios::binary | std::ios::ate);
  unsigned romMemorySize = 0;

  if (romFileStream.is_open()) {
    romMemorySize = unsigned(romFileStream.tellg());
    romBuffer = cinek::ByteBuffer(
      (uint8_t *)slabMemory_.allocate(romMemorySize), romMemorySize);
    romFileStream.seekg(0, std::ios::beg);
    romFileStream.read((char*)romBuffer.forwardSize(romMemorySize).first, romMemorySize);
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
  if (host->logLevel_ > log_level) return;
  if (host->logOutput_.size() >= kLogOutputLineLimit) return;


  host->logOutput_.emplace_back(ClemensBackendOutputText{log_level, msg});
}

//  If enabled, this emulator issues this callback per instruction
//  This is great for debugging but should be disabled otherwise
void ClemensBackend::emulatorOpcodeCallback(struct ClemensInstruction *inst,
                                            const char *operand, void *this_ptr) {
  auto *host = reinterpret_cast<ClemensBackend *>(this_ptr);
  if (!host->areInstructionsLogged_) return;
  host->loggedInstructions_.emplace_back();
  auto& loggedInst = host->loggedInstructions_.back();
  loggedInst.data = *inst;
  strncpy(loggedInst.operand, operand, sizeof(loggedInst.operand));
}
