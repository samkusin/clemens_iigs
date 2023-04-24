#include "clem_command_queue.hpp"
#include "disklib/clem_disk_utils.hpp"

#include "cinek/ckdebug.h"

#include "clem_host_shared.hpp"
#include "fmt/core.h"

#include <cassert>
#include <charconv>
#include <utility>

//  State affected to translate from the old main() to the new main()
//  bool isRunning = !stepsRemaining.has_value() || *stepsRemaining > 0;
//      - dispatchAll can alter this equation - so just set isRunning again after dispatchAll()
//      - if this isRunning has changed to ON from OFF after the call to dispatchAll, reset the run
//      sampler.

void ClemensCommandQueue::queue(ClemensCommandQueue &other) {
    while (!queue_.isFull() && !other.queue_.isEmpty()) {
        Command cmd;
        other.queue_.pop(cmd);
        queue_.push(cmd);
    }
}

auto ClemensCommandQueue::dispatchAll(ClemensCommandQueueListener &listener) -> DispatchResult {
    DispatchResult result;
    result.second = false;

    bool commandFailed = false;
    while (!queue_.isEmpty() && !result.second) {
        Command cmd;
        queue_.pop(cmd);

        switch (cmd.type) {
        case Command::Terminate:
            result.second = true;
            break;
        case Command::ResetMachine:
            listener.onCommandReset();
            break;
        case Command::RunMachine:
            listener.onCommandRun();
            break;
        case Command::StepMachine: {
            unsigned count;
            if (std::from_chars(cmd.operand.data(), cmd.operand.data() + cmd.operand.size(), count)
                    .ec != std::errc{}) {
                count = 0;
            }
            listener.onCommandStep(count);
        } break;
        case Command::InsertDisk:
            if (!insertDisk(listener, cmd.operand, false))
                commandFailed = true;
            break;
        case Command::InsertBlankDisk:
            if (!insertDisk(listener, cmd.operand, true))
                commandFailed = true;
            break;
        case Command::EjectDisk:
            ejectDisk(listener, cmd.operand);
            break;
        case Command::InsertSmartPortDisk:
            if (!insertSmartPortDisk(listener, cmd.operand, false))
                commandFailed = true;
            break;
        case Command::InsertBlankSmartPortDisk:
            if (!insertSmartPortDisk(listener, cmd.operand, true))
                commandFailed = true;
            break;
        case Command::EjectSmartPortDisk:
            ejectSmartPortDisk(listener, cmd.operand);
            break;
        case Command::WriteProtectDisk:
            writeProtectDisk(listener, cmd.operand);
            break;
        case Command::Input:
            inputMachine(listener, cmd.operand);
            break;
        case Command::Break:
            listener.onCommandBreakExecution();
            break;
        case Command::AddBreakpoint:
            if (!addBreakpoint(listener, cmd.operand))
                commandFailed = true;
            break;
        case Command::DelBreakpoint:
            if (!delBreakpoint(listener, cmd.operand))
                commandFailed = true;
            break;
        case Command::DebugMemoryPage:
            listener.onCommandDebugMemoryPage((uint8_t)(std::stoul(cmd.operand)));
            break;
        case Command::WriteMemory:
            writeMemory(listener, cmd.operand);
            break;
        case Command::DebugLogLevel:
            listener.onCommandDebugLogLevel((int)(std::stol(cmd.operand)));
            break;
        case Command::DebugProgramTrace:
            if (!programTrace(listener, cmd.operand))
                commandFailed = true;
            break;
        case Command::SaveMachine:
            if (!listener.onCommandSaveMachine(cmd.operand))
                commandFailed = true;
            break;
        case Command::LoadMachine:
            if (!listener.onCommandLoadMachine(cmd.operand)) {
                //  TODO: this should force a restart - hopefully the
                //        frontend will save the current state prior to loading
                //        a new one to avoid data loss.
                commandFailed = true;
            }
            break;
        case Command::RunScript:
            if (!listener.onCommandRunScript(cmd.operand)) {
                commandFailed = true;
            }
            break;
        case Command::FastDiskEmulation: {
            int value = 0;
            if (std::from_chars(cmd.operand.data(), cmd.operand.data() + cmd.operand.size(), value)
                    .ec != std::errc{}) {
                commandFailed = true;
            } else {
                listener.onCommandFastDiskEmulation(value == 1);
            }
            break;
        }
        case Command::DebugMessage: {
            auto msgResponse = listener.onCommandDebugMessage(cmd.operand);
            if (msgResponse.substr(0, 2) != "OK") {
                commandFailed = true;
            }
            cmd.operand = msgResponse;
            break;
        }
        case Command::Undefined:
            break;
        }

        if (cmd.type != Command::Input) {
            //   queue command result
            ClemensBackendResult commandResult;
            commandResult.cmd = std::move(cmd);
            commandResult.type =
                commandFailed ? ClemensBackendResult::Failed : ClemensBackendResult::Succeeded;
            result.first.emplace_back(commandResult);
        }
    }
    queue_.clear();

    return result;
}

void ClemensCommandQueue::terminate() { queue(Command{Command::Terminate}); }

void ClemensCommandQueue::reset() { queue(Command{Command::ResetMachine}); }

void ClemensCommandQueue::run() { queue(Command{Command::RunMachine}); }

void ClemensCommandQueue::step(unsigned count) {
    queue(Command{Command::StepMachine, fmt::format("{}", count)});
}

void ClemensCommandQueue::insertDisk(ClemensDriveType driveType, std::string diskPath) {
    queue(Command{Command::InsertDisk,
                  fmt::format("{}={}", ClemensDiskUtilities::getDriveName(driveType), diskPath)});
}

void ClemensCommandQueue::insertBlankDisk(ClemensDriveType driveType, std::string diskPath) {
    queue(Command{Command::InsertBlankDisk,
                  fmt::format("{}={}", ClemensDiskUtilities::getDriveName(driveType), diskPath)});
}

bool ClemensCommandQueue::insertDisk(ClemensCommandQueueListener &listener,
                                     const std::string_view &inputParam, bool allowBlank) {
    auto sepPos = inputParam.find('=');
    if (sepPos == std::string_view::npos)
        return false;
    auto driveName = inputParam.substr(0, sepPos);
    auto imagePath = inputParam.substr(sepPos + 1);
    auto driveType = ClemensDiskUtilities::getDriveType(driveName);
    if (driveType == kClemensDrive_Invalid)
        return false;

    return allowBlank ? listener.onCommandInsertBlankDisk(driveType, std::string(imagePath))
                      : listener.onCommandInsertDisk(driveType, std::string(imagePath));
}

void ClemensCommandQueue::ejectDisk(ClemensDriveType driveType) {
    queue(Command{Command::EjectDisk, std::string(ClemensDiskUtilities::getDriveName(driveType))});
}

void ClemensCommandQueue::ejectDisk(ClemensCommandQueueListener &listener,
                                    const std::string_view &inputParam) {
    auto driveType = ClemensDiskUtilities::getDriveType(inputParam);
    listener.onCommandEjectDisk(driveType);
}

void ClemensCommandQueue::writeProtectDisk(ClemensDriveType driveType, bool wp) {
    queue(Command{Command::WriteProtectDisk,
                  fmt::format("{},{}", ClemensDiskUtilities::getDriveName(driveType), wp ? 1 : 0)});
}

bool ClemensCommandQueue::writeProtectDisk(ClemensCommandQueueListener &listener,
                                           const std::string_view &inputParam) {
    auto sepPos = inputParam.find(',');
    if (sepPos == std::string_view::npos)
        return false;
    auto driveParam = inputParam.substr(0, sepPos);
    auto driveType = ClemensDiskUtilities::getDriveType(driveParam);
    auto enableParam = inputParam.substr(sepPos + 1);

    return listener.onCommandWriteProtectDisk(driveType, enableParam == "1");
}

void ClemensCommandQueue::insertSmartPortDisk(unsigned driveIndex, std::string diskPath) {
    queue(Command{Command::InsertSmartPortDisk, fmt::format("{}={}", driveIndex, diskPath)});
}

void ClemensCommandQueue::insertBlankSmartPortDisk(unsigned driveIndex, std::string diskPath) {
    queue(Command{Command::InsertBlankSmartPortDisk, fmt::format("{}={}", driveIndex, diskPath)});
}

bool ClemensCommandQueue::insertSmartPortDisk(ClemensCommandQueueListener &listener,
                                              const std::string_view &inputParam, bool allowBlank) {
    auto sepPos = inputParam.find('=');
    if (sepPos == std::string_view::npos)
        return false;
    auto driveIndexLabel = inputParam.substr(0, sepPos);
    auto imagePath = inputParam.substr(sepPos + 1);
    unsigned driveIndex;
    if (std::from_chars(driveIndexLabel.data(), driveIndexLabel.data() + driveIndexLabel.size(),
                        driveIndex)
            .ec != std::errc{})
        return false;

    return allowBlank
               ? listener.onCommandInsertBlankSmartPortDisk(driveIndex, std::string(imagePath))
               : listener.onCommandInsertSmartPortDisk(driveIndex, std::string(imagePath));
}

void ClemensCommandQueue::ejectSmartPortDisk(unsigned driveIndex) {
    queue(Command{Command::EjectSmartPortDisk, std::to_string(driveIndex)});
}

void ClemensCommandQueue::ejectSmartPortDisk(ClemensCommandQueueListener &listener,
                                             const std::string_view &inputParam) {
    unsigned driveIndex;
    if (std::from_chars(inputParam.data(), inputParam.data() + inputParam.size(), driveIndex).ec !=
        std::errc{})
        return;

    listener.onCommandEjectSmartPortDisk(driveIndex);
}

void ClemensCommandQueue::debugMemoryPage(uint8_t page) {
    queue(Command{Command::DebugMemoryPage, std::to_string(page)});
}

void ClemensCommandQueue::debugMemoryWrite(uint16_t addr, uint8_t value) {
    queue(Command{Command::WriteMemory, fmt::format("{}={}", addr, value)});
}

void ClemensCommandQueue::writeMemory(ClemensCommandQueueListener &listener,
                                      const std::string_view &inputParam) {
    auto sepPos = inputParam.find('=');
    if (sepPos == std::string_view::npos) {
        return;
    }
    uint16_t addr;
    if (std::from_chars(inputParam.data(), inputParam.data() + sepPos, addr).ec != std::errc{})
        return;
    auto valStr = inputParam.substr(sepPos + 1);
    uint8_t value;
    if (std::from_chars(valStr.data(), valStr.data() + valStr.size(), value).ec != std::errc{})
        return;

    listener.onCommandDebugMemoryWrite(addr, value);
}

void ClemensCommandQueue::debugLogLevel(int logLevel) {
    queue(Command{Command::DebugLogLevel, fmt::format("{}", logLevel)});
}

void ClemensCommandQueue::debugProgramTrace(std::string op, std::string path) {
    queue(Command{Command::DebugProgramTrace,
                  fmt::format("{},{}", op, path.empty() ? "#" : path.c_str())});
}

bool ClemensCommandQueue::programTrace(ClemensCommandQueueListener &listener,
                                       const std::string_view &inputParam) {
    auto sepPos = inputParam.find(',');
    auto op = inputParam.substr(0, sepPos);
    std::string_view param;
    if (sepPos != std::string_view::npos) {
        param = inputParam.substr(sepPos + 1);
        if (param == "#")
            param = std::string_view();
    } else {
        param = std::string_view();
    }
    auto path = param;
    return listener.onCommandDebugProgramTrace(op, path);
}

void ClemensCommandQueue::saveMachine(std::string path) {
    queue(Command{Command::SaveMachine, std::move(path)});
}

void ClemensCommandQueue::loadMachine(std::string path) {
    queue(Command{Command::LoadMachine, std::move(path)});
}

void ClemensCommandQueue::runScript(std::string command) {
    queue(Command{Command::RunScript, std::move(command)});
}

void ClemensCommandQueue::breakExecution() { queue(Command{Command::Break}); }

void ClemensCommandQueue::addBreakpoint(const ClemensBackendBreakpoint &breakpoint) {
    Command cmd;
    cmd.operand = fmt::format("{:s}:{:06X}",
                              breakpoint.type == ClemensBackendBreakpoint::DataRead ? "r"
                              : breakpoint.type == ClemensBackendBreakpoint::Write  ? "w"
                              : breakpoint.type == ClemensBackendBreakpoint::IRQ    ? "i"
                              : breakpoint.type == ClemensBackendBreakpoint::BRK    ? "b"
                                                                                    : "",
                              breakpoint.address);
    cmd.type = Command::AddBreakpoint;
    queue(std::move(cmd));
}

bool ClemensCommandQueue::addBreakpoint(ClemensCommandQueueListener &listener,
                                        const std::string_view &inputParam) {
    auto sepPos = inputParam.find(':');
    assert(sepPos != std::string_view::npos);
    auto type = inputParam.substr(0, sepPos);
    auto address = inputParam.substr(sepPos + 1);
    ClemensBackendBreakpoint bp;
    if (type == "r") {
        bp.type = ClemensBackendBreakpoint::DataRead;
    } else if (type == "w") {
        bp.type = ClemensBackendBreakpoint::Write;
    } else if (type == "i") {
        bp.type = ClemensBackendBreakpoint::IRQ;
    } else if (type == "b") {
        bp.type = ClemensBackendBreakpoint::BRK;
    } else {
        bp.type = ClemensBackendBreakpoint::Execute;
    }
    bp.address = (uint32_t)std::stoul(std::string(address), nullptr, 16);

    listener.onCommandAddBreakpoint(bp);

    return true;
}

void ClemensCommandQueue::removeBreakpoint(unsigned index) {
    queue(Command{Command::DelBreakpoint, std::to_string(index)});
}

bool ClemensCommandQueue::delBreakpoint(ClemensCommandQueueListener &listener,
                                        const std::string_view &inputParam) {
    if (inputParam.empty()) {
        return listener.onCommandRemoveBreakpoint(-1);
    }

    unsigned index = (unsigned)std::stoul(std::string(inputParam));
    return listener.onCommandRemoveBreakpoint(index);
}

#if defined(__GNUC__)
#if !defined(__clang__)
//  NOTE GCC warning seems spurious for *some* std::string_view::find() cases
//       have added plenty of guards around the line below to no avail.
//            commaPos = !inputValueB.empty() ? inputValueB.find(',') : std::string_view::npos;
//  ref: https://gcc.gnu.org/bugzilla/show_bug.cgi?id=91397
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wstringop-overflow"
#endif
#endif
static const char *sInputKeys[] = {"",      "keyD", "keyU",   "mouseD", "mouseU",
                                   "mouse", "padl", "nopadl", NULL};

void ClemensCommandQueue::inputEvent(const ClemensInputEvent &input) {
    CK_ASSERT_RETURN(*sInputKeys[input.type] != '\0');
    Command cmd{Command::Input};
    if (input.type == kClemensInputType_Paddle ||
        input.type == kClemensInputType_PaddleDisconnected) {
        cmd.operand = fmt::format("{}={},{},{}", sInputKeys[input.type], input.value_a,
                                  input.value_b, input.gameport_button_mask);
    } else {
        cmd.operand = fmt::format("{}={},{},{}", sInputKeys[input.type], input.value_a,
                                  input.value_b, input.adb_key_toggle_mask);
    }
    queue(cmd);
}

void ClemensCommandQueue::inputMachine(ClemensCommandQueueListener &listener,
                                       const std::string_view &inputParam) {
    auto equalsTokenPos = inputParam.find('=');
    auto name = inputParam.substr(0, equalsTokenPos);
    auto value = inputParam.substr(equalsTokenPos + 1);
    for (const char **keyName = &sInputKeys[0]; *keyName != NULL; ++keyName) {
        if (name == *keyName) {
            ClemensInputEvent inputEvent{};
            inputEvent.type = (ClemensInputType)((int)(keyName - &sInputKeys[0]));
            //  oh why, oh why no straightforward C++ conversion from std::string_view
            //  to number
            auto commaPos = value.find(',');
            auto inputValueA = value.substr(0, commaPos);
            inputEvent.value_a = (int16_t)std::stol(std::string(inputValueA));
            if (commaPos != std::string_view::npos) {
                std::string_view inputModifiers;
                auto inputValueB = value.substr(commaPos + 1);
                commaPos = !inputValueB.empty() ? inputValueB.find(',') : std::string_view::npos;
                if (commaPos != std::string_view::npos) {
                    inputModifiers = inputValueB.substr(commaPos + 1);
                    inputValueB = inputValueB.substr(0, commaPos);
                }
                inputEvent.value_b = (int16_t)std::stol(std::string(inputValueB));
                if (inputEvent.type == kClemensInputType_Paddle ||
                    inputEvent.type == kClemensInputType_PaddleDisconnected) {
                    inputEvent.gameport_button_mask = std::stoul(std::string(inputModifiers));
                } else {
                    inputEvent.adb_key_toggle_mask = std::stoul(std::string(inputModifiers));
                }
            }
            listener.onCommandInputEvent(inputEvent);
        }
    }
}
#if defined(__GNUC__)
#if !defined(__clang__)
#pragma GCC diagnostic pop
#endif
#endif

void ClemensCommandQueue::enableFastDiskEmulation(bool enable) {
    queue(Command{Command::FastDiskEmulation, fmt::format("{}", enable ? 1 : 0)});
}

void ClemensCommandQueue::debugMessage(std::string msg) {
    queue(Command{Command::DebugMessage, std::move(msg)});
}

void ClemensCommandQueue::queue(const Command &cmd) { queue_.push(cmd); }
