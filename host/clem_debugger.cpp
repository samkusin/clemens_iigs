#include "clem_debugger.hpp"
#include "clem_command_queue.hpp"
#include "clem_frame_state.hpp"
#include "clem_host_utils.hpp"
#include "clem_l10n.hpp"
#include "core/clem_disk_utils.hpp"

#include "clem_types.h"

#include "fmt/core.h"
#include "imgui.h"

#include <algorithm>
#include <charconv>
#include <cmath>
#include <cstdio>
#include <filesystem>
#include <string>
#include <string_view>
#include <system_error>
#include <tuple>

template <typename TBufferType> struct FormatView2 {
    using BufferType = TBufferType;
    using StringType = typename BufferType::ValueType;
    using LevelType = typename StringType::Type;

    BufferType &buffer_;
    bool &viewChanged_;
    FormatView2(BufferType &buffer, bool &viewChanged)
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

#define CLEM_HOST_GUI_CPU_PINS_COLOR(_field_, _last_field_)                                        \
    _last_field_ != _field_ ? getModifiedColor(_field_, frameState_->isRunning)                    \
                            : getDefaultColor(_field_)

#define CLEM_HOST_GUI_CPU_PINS_INV_COLOR(_field_, _last_field_)                                    \
    _last_field_ != _field_ ? getModifiedColor(!_field_, frameState_->isRunning)                   \
                            : getDefaultColor(!_field_)

#define CLEM_HOST_GUI_CPU_FIELD_COLOR(_field_, _last_field_)                                       \
    _last_field_ != _field_ ? getModifiedColor(true, frameState_->isRunning) : getDefaultColor(true)

#define CLEM_TERM_COUT                                                                             \
    FormatView2<decltype(ClemensDebugger::consoleLines_)>(consoleLines_, consoleChanged_)

static void displayViewNotAvailable(const char *label) {
    ImVec2 contentRegion = ImGui::GetContentRegionAvail();
    auto labelSize = ImGui::CalcTextSize(label);
    ImVec2 pos((contentRegion.x - labelSize.x) * 0.5f, (contentRegion.y - labelSize.y) * 0.5f);
    ImGui::SetCursorPos(pos);
    ImGui::TextUnformatted(label);
}

////////////////////////////////////////////////////////////////////////////////

ClemensDebugger::ClemensDebugger(ClemensCommandQueue &commandQueue,
                                 ClemensDebuggerListener &listener)
    : commandQueue_(commandQueue), listener_(listener), frameState_(nullptr),
      consoleChanged_(false) {

    consoleInputLineBuf_[0] = '\0';
    iwmDiskBitSlip_ = 0;

    debugMemoryEditor_.ReadFn = &ClemensDebugger::imguiMemoryEditorRead;
    debugMemoryEditor_.WriteFn = &ClemensDebugger::imguiMemoryEditorWrite;
}

void ClemensDebugger::lastFrame(const ClemensFrame::FrameState &lastFrameState) {
    lastFrameCPURegs_ = lastFrameState.cpu.regs;
    lastFrameCPUPins_ = lastFrameState.cpu.pins;
    lastFrameIRQs_ = lastFrameState.irqs;
    lastFrameNMIs_ = lastFrameState.nmis;
    lastFrameIWM_ = lastFrameState.iwm;
    lastFrameADBStatus_ = lastFrameState.adb;
    if (lastFrameState.ioPage) {
        memcpy(lastFrameIORegs_, lastFrameState.ioPage, 256);
    }
}

bool ClemensDebugger::thisFrame(ClemensFrame::LastCommandState &lastCommandState,
                                const ClemensFrame::FrameState &frameState) {
    bool hitBreakpoint = false;
    frameState_ = &frameState;

    //  display log lines
    auto *logNode = lastCommandState.logNode;
    while (logNode) {
        if (consoleLines_.isFull()) {
            consoleLines_.pop();
        }
        TerminalLine logLine;
        logLine.text.assign((char *)logNode + sizeof(*logNode), logNode->sz);
        switch (logNode->logLevel) {
        case CLEM_DEBUG_LOG_DEBUG:
            logLine.type = Debug;
            break;
        case CLEM_DEBUG_LOG_INFO:
            logLine.type = Info;
            break;
        case CLEM_DEBUG_LOG_WARN:
            logLine.type = Warn;
            break;
        case CLEM_DEBUG_LOG_FATAL:
        case CLEM_DEBUG_LOG_UNIMPL:
            logLine.type = Error;
            break;
        default:
            logLine.type = Info;
            break;
        }
        consoleLines_.push(std::move(logLine));
        logNode = logNode->next;
        consoleChanged_ = true;
    }
    lastCommandState.logNode = lastCommandState.logNodeTail = nullptr;

    //  display last few log instructions
    auto *logInstructionNode = lastCommandState.logInstructionNode;
    while (logInstructionNode) {
        ClemensBackendExecutedInstruction *execInstruction = logInstructionNode->begin;
        ClemensTraceExecutedInstruction instruction;
        while (execInstruction != logInstructionNode->end) {
            instruction.fromInstruction(execInstruction->data, execInstruction->operand);
            CLEM_TERM_COUT.format(Opcode, "({}) {:02X}/{:04X} {} {}", instruction.cycles_spent,
                                  instruction.pc >> 16, instruction.pc & 0xffff, instruction.opcode,
                                  instruction.operand);
            ++execInstruction;
        }
        logInstructionNode = logInstructionNode->next;
    }
    lastCommandState.logInstructionNode = lastCommandState.logInstructionNodeTail = nullptr;

    breakpoints_.clear();
    for (unsigned bpIndex = 0; bpIndex < frameState_->breakpointCount; ++bpIndex) {
        breakpoints_.emplace_back(frameState_->breakpoints[bpIndex]);
    }

    if (lastCommandState.hitBreakpoint.has_value()) {
        unsigned bpIndex = *lastCommandState.hitBreakpoint;
        CLEM_TERM_COUT.format(Info, "Breakpoint {} hit {:02X}/{:04X}.", bpIndex,
                              (breakpoints_[bpIndex].address >> 16) & 0xff,
                              breakpoints_[bpIndex].address & 0xffff);
        hitBreakpoint = true;
        lastCommandState.hitBreakpoint = std::nullopt;
    }
    return hitBreakpoint;
}

std::vector<ClemensBackendBreakpoint> ClemensDebugger::copyBreakpoints() const {
    return breakpoints_;
}

void ClemensDebugger::cpuStatRow16(const char *label, const char *attrName, uint16_t value,
                                   float labelWidth, const ImVec4 &color) {
    char buf[16];
    char idname[16];
    ImGui::TableNextColumn();
    ImGui::AlignTextToFramePadding();
    ImGui::TextUnformatted(label);
    ImGui::SameLine(labelWidth);
    ImGui::PushStyleColor(ImGuiCol_Text, color);
    ImGui::PushItemWidth(-1);
    snprintf(idname, sizeof(idname), "##r%s", attrName);
    ImGui::InputScalar(idname, ImGuiDataType_U16, &value, NULL, NULL, "%04X",
                       ImGuiInputTextFlags_CharsHexadecimal | ImGuiInputTextFlags_CharsNoBlank);
    if (ImGui::IsItemDeactivatedAfterEdit()) {
        snprintf(buf, sizeof(buf), ".%s = %04X", attrName, value);
        commandQueue_.runScript(buf);
    }
    ImGui::PopItemWidth();
    ImGui::PopStyleColor();
}

void ClemensDebugger::cpuStatRow8(const char *label, const char *attrName, uint8_t value,
                                  float labelWidth, const ImVec4 &color) {
    char buf[16];
    char idname[16];
    ImGui::TableNextColumn();
    ImGui::AlignTextToFramePadding();
    ImGui::TextUnformatted(label);
    ImGui::SameLine(labelWidth);
    ImGui::PushStyleColor(ImGuiCol_Text, color);
    ImGui::PushItemWidth(-1);
    snprintf(idname, sizeof(idname), "##r%s", attrName);
    ImGui::InputScalar(idname, ImGuiDataType_U8, &value, NULL, NULL, "%02X",
                       ImGuiInputTextFlags_CharsHexadecimal | ImGuiInputTextFlags_CharsNoBlank |
                           ImGuiInputTextFlags_EnterReturnsTrue);
    if (ImGui::IsItemDeactivatedAfterEdit()) {
        snprintf(buf, sizeof(buf), ".%s = %04X", attrName, value);
        commandQueue_.runScript(buf);
    }
    ImGui::PopItemWidth();
    ImGui::PopStyleColor();
}

void ClemensDebugger::cpuProcessorFlag(const char *label, uint8_t flag) {
    char buf[16];
    ImVec4 color = flag ? getStatusFieldColor<uint8_t>(lastFrameCPURegs_.P, frameState_->cpu.regs.P,
                                                       flag, frameState_->isRunning)
                              .Value
                        : ImGui::GetStyleColorVec4(ImGuiCol_TextDisabled);

    ImGui::PushStyleColor(ImGuiCol_Text, (ImVec4)color);
    if (ImGui::Button(label) && flag) {
        snprintf(buf, sizeof(buf), ".p = %02X", frameState_->cpu.regs.P ^ flag);
        commandQueue_.runScript(buf);
    }
    ImGui::PopStyleColor();
}

void ClemensDebugger::layoutConsoleLines(ImVec2 dimensions) {
    ImGui::BeginChild("##ConsoleLines", dimensions);
    for (size_t index = 0; index < consoleLines_.size(); ++index) {
        auto &line = consoleLines_.at(index);
        switch (line.type) {
        case Debug:
            ImGui::PushStyleColor(ImGuiCol_Text, (ImU32)ImColor(192, 192, 192, 255));
            break;
        case Warn:
            ImGui::PushStyleColor(ImGuiCol_Text, (ImU32)ImColor(255, 255, 0, 255));
            break;
        case Error:
            ImGui::PushStyleColor(ImGuiCol_Text, (ImU32)ImColor(255, 0, 192, 255));
            break;
        case Command:
            ImGui::PushStyleColor(ImGuiCol_Text, (ImU32)ImColor(0, 255, 255, 255));
            break;
        case Opcode:
            ImGui::PushStyleColor(ImGuiCol_Text, (ImU32)ImColor(0, 255, 0, 255));
            break;
        default:
            ImGui::PushStyleColor(ImGuiCol_Text, (ImU32)ImColor(255, 255, 255, 255));
            break;
        }
        ImGui::TextUnformatted(consoleLines_.at(index).text.c_str());
        ImGui::PopStyleColor();
    }
    if (consoleChanged_) {
        ImGui::SetScrollHereY();
        consoleChanged_ = false;
    }
    ImGui::EndChild();
}

void ClemensDebugger::console(ImVec2 anchor, ImVec2 dimensions) {
    const float kLineSize = ImGui::GetTextLineHeight();
    const float kTextLineSize = ImGui::GetTextLineHeightWithSpacing();
    const ImGuiStyle &style = ImGui::GetStyle();

    ImGui::SetNextWindowPos(anchor);
    ImGui::SetNextWindowSize(dimensions);
    ImGui::Begin("DebuggerConsole", nullptr,
                 ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove);

    //  pass 1 - min layout
    ImVec2 contentRegion = ImGui::GetContentRegionAvail();
    const float kCollapsingHeaderHeight = 2 * style.FramePadding.y + kLineSize;
    float topY = style.FramePadding.y + kTextLineSize;
    float componentHeight = 2 * style.FramePadding.y + 8 * kTextLineSize;
    float consoleY = topY + kCollapsingHeaderHeight + componentHeight + style.ItemSpacing.y +
                     (style.FramePadding.y + style.ItemSpacing.y) + // tab border
                     (2 * style.ItemSpacing.y);                     // separator
    float consoleHeight = 2 * style.FramePadding.y + 4 * kTextLineSize;
    float inputY = consoleY + consoleHeight + style.ItemSpacing.y;
    float bottomY = inputY + 2 * style.FramePadding.y + kLineSize;

    //  pass 2 - resize component up to component max height if space is available
    //  then resize console to remainder size
    topY = bottomY;
    bottomY = contentRegion.y;
    float adjustHeight = 6 * kTextLineSize;
    float availHeight = bottomY - topY;
    adjustHeight = std::min(adjustHeight, availHeight);
    componentHeight += adjustHeight;
    availHeight -= adjustHeight;
    ImVec2 sectionSize(contentRegion.x, componentHeight);
    if (ImGui::CollapsingHeader("Systems", ImGuiTreeNodeFlags_DefaultOpen)) {
        if (ImGui::BeginTabBar("SystemsTabBar")) {
            if (ImGui::BeginTabItem("DOC")) {
                ImGui::BeginChild("##DOC", sectionSize);
                doMachineDebugDOCDisplay();
                ImGui::EndChild();
                ImGui::EndTabItem();
            }
            ImGui::EndTabBar();
        }
    } else {
        availHeight += componentHeight;
    }
    ImGui::Separator();
    sectionSize.y = consoleHeight + availHeight;
    layoutConsoleLines(sectionSize);
    ImGui::Separator();
    ImGui::AlignTextToFramePadding();
    ImGui::TextUnformatted(">");
    ImGui::SameLine();
    ImGui::PushItemWidth(-1);
    if (ImGui::InputText("##input", consoleInputLineBuf_, sizeof(consoleInputLineBuf_),
                         ImGuiInputTextFlags_EnterReturnsTrue)) {
        // allow regular input at the console
        CLEM_TERM_COUT.print(Info, consoleInputLineBuf_);
        executeCommand(consoleInputLineBuf_);
        consoleInputLineBuf_[0] = '\0';
        ImGui::SetKeyboardFocusHere(-1);
    }
    ImGui::SetItemDefaultFocus();
    ImGui::PopItemWidth();

    ImGui::End();
}

//  Draws the auxillary view
void ClemensDebugger::auxillary(ImVec2 anchor, ImVec2 dimensions) {
    ImGui::SetNextWindowPos(anchor);
    ImGui::SetNextWindowSize(dimensions);
    ImGui::Begin("DebuggerAuxillary", nullptr,
                 ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove |
                     ImGuiWindowFlags_NoBringToFrontOnFocus);
    if (ImGui::BeginTabBar("AuxTabBar")) {
        if (ImGui::BeginTabItem("Memory")) {
            if (frameState_ && frameState_->memoryView) {
                uint8_t bank = frameState_->memoryViewBank;
                if (ImGui::InputScalar("Bank", ImGuiDataType_U8, &bank, NULL, NULL, "%02X",
                                       ImGuiInputTextFlags_CharsHexadecimal)) {
                    commandQueue_.debugMemoryPage((uint8_t)(bank & 0xff));
                }
                debugMemoryEditor_.OptAddrDigitsCount = 4;
                debugMemoryEditor_.Cols = 16;
                debugMemoryEditor_.DrawContents(this, CLEM_IIGS_BANK_SIZE, (size_t)(bank) << 16);
            } else {
                displayViewNotAvailable(CLEM_L10N_LABEL(kDebugNotAvailableWhileRunning));
            }
            ImGui::EndTabItem();
        }
        if (ImGui::BeginTabItem("IWM")) {
            ImVec2 contentRegion = ImGui::GetContentRegionAvail();
            ImGui::BeginChild("##IWM", contentRegion);
            doMachineDebugIWMDisplay(frameState_ && !frameState_->isRunning);
            ImGui::EndChild();
            ImGui::EndTabItem();
        }
        ImGui::EndTabBar();
    }
    ImGui::End();
}

void ClemensDebugger::cpuStateTable(ImVec2 anchor, ImVec2 dimensions,
                                    const DebugDiagnostics &diagnostics) {
    const float kCharSize = ImGui::GetFont()->GetCharAdvance('A');
    const ImGuiStyle &style = ImGui::GetStyle();

    ImGui::SetNextWindowPos(anchor);
    ImGui::SetNextWindowSize(dimensions);
    ImGui::Begin("CPUAndMachineState", nullptr,
                 ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove |
                     ImGuiWindowFlags_NoBringToFrontOnFocus);
    ImGui::PushStyleVar(ImGuiStyleVar_CellPadding, ImVec2(kCharSize, 2.0f));
    if (frameState_) {
        auto &regs = frameState_->cpu.regs;
        auto &lastRegs = lastFrameCPURegs_;
        const float kTableCellWidth1 =
            (style.CellPadding.x + style.FramePadding.x) * 2 + kCharSize * 4;
        if (ImGui::BeginTable("CPUState", 1, ImGuiTableFlags_Borders)) {
            ImGui::TableNextRow();
            cpuStatRow16("PC", "pc", regs.PC, kTableCellWidth1,
                         CLEM_HOST_GUI_CPU_FIELD_COLOR(regs.PC, lastRegs.PC));
            ImGui::TableNextRow();
            cpuStatRow8("PBR", "pbr", regs.PBR, kTableCellWidth1,
                        CLEM_HOST_GUI_CPU_FIELD_COLOR(regs.PBR, lastRegs.PBR));
            ImGui::TableNextRow();
            cpuStatRow16("S", "s", regs.S, kTableCellWidth1,
                         CLEM_HOST_GUI_CPU_FIELD_COLOR(regs.S, lastRegs.S));
            ImGui::TableNextRow();
            cpuStatRow16("D", "d", regs.D, kTableCellWidth1,
                         CLEM_HOST_GUI_CPU_FIELD_COLOR(regs.D, lastRegs.D));
            ImGui::TableNextRow();
            cpuStatRow8("DBR", "dbr", regs.DBR, kTableCellWidth1,
                        CLEM_HOST_GUI_CPU_FIELD_COLOR(regs.DBR, lastRegs.DBR));
            ImGui::TableNextRow();
            cpuStatRow16("A", "a", regs.A, kTableCellWidth1,
                         CLEM_HOST_GUI_CPU_FIELD_COLOR(regs.A, lastRegs.A));
            ImGui::TableNextRow();
            cpuStatRow16("X", "x", regs.X, kTableCellWidth1,
                         CLEM_HOST_GUI_CPU_FIELD_COLOR(regs.X, lastRegs.X));
            ImGui::TableNextRow();
            cpuStatRow16("Y", "y", regs.Y, kTableCellWidth1,
                         CLEM_HOST_GUI_CPU_FIELD_COLOR(regs.Y, lastRegs.Y));
            ImGui::EndTable();
        }

        //  Processor Flags
        ImGui::Spacing();
        ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(2, 0));
        ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(2.0f, style.FramePadding.y));
        float flagsLineWidth =
            8 * (style.ItemSpacing.x + style.FramePadding.x + kCharSize) - style.ItemSpacing.x;
        ImGui::SetCursorPosX((ImGui::GetWindowContentRegionWidth() - flagsLineWidth) * 0.5f);
        cpuProcessorFlag("n", kClemensCPUStatus_Negative);
        ImGui::SameLine();
        cpuProcessorFlag("v", kClemensCPUStatus_Overflow);
        ImGui::SameLine();
        if (frameState_->cpu.pins.emulation) {
            cpuProcessorFlag("-", 0);
            ImGui::SameLine();
            cpuProcessorFlag("-", 0);
            ImGui::SameLine();
        } else {
            cpuProcessorFlag("m", kClemensCPUStatus_MemoryAccumulator);
            ImGui::SameLine();
            cpuProcessorFlag("x", kClemensCPUStatus_Index);
            ImGui::SameLine();
        }
        cpuProcessorFlag("d", kClemensCPUStatus_Decimal);
        ImGui::SameLine();
        cpuProcessorFlag("i", kClemensCPUStatus_IRQDisable);
        ImGui::SameLine();
        cpuProcessorFlag("z", kClemensCPUStatus_Zero);
        ImGui::SameLine();
        cpuProcessorFlag("c", kClemensCPUStatus_Carry);
        ImGui::PopStyleVar(2);
        ImGui::NewLine();

        // Pins
        ImGui::Separator();

        auto &pins = frameState_->cpu.pins;
        auto &lastPins = lastFrameCPUPins_;

        if (ImGui::BeginTable("CPUPins", 1, ImGuiTableFlags_Borders)) {
            ImGui::TableNextColumn();
            ImGui::TextColored(CLEM_HOST_GUI_CPU_PINS_COLOR(lastPins.readyOut, pins.readyOut),
                               "RDY");
            ImGui::TableNextColumn();
            ImGui::TextColored(CLEM_HOST_GUI_CPU_PINS_COLOR(lastPins.resbIn, pins.resbIn), "RESB");
            ImGui::TableNextColumn();
            ImGui::TextColored(CLEM_HOST_GUI_CPU_PINS_COLOR(lastPins.emulation, pins.emulation),
                               "E");
            ImGui::TableNextColumn();
            ImGui::TextColored(CLEM_HOST_GUI_CPU_PINS_INV_COLOR(lastPins.irqbIn, pins.irqbIn),
                               "IRQ");
            ImGui::TableNextColumn();
            ImGui::TextColored(CLEM_HOST_GUI_CPU_PINS_INV_COLOR(lastPins.nmibIn, pins.nmibIn),
                               "NMI");
            ImGui::EndTable();
        }
        // TODO: IRQ Mask/Flags

        ImGui::Separator();
        diagnosticTables(diagnostics);
    }

    ImGui::PopStyleVar();
    ImGui::End();
}

void ClemensDebugger::diagnosticTables(const DebugDiagnostics &diagnostics) {
    if (!frameState_)
        return;
    const float kCharSize = ImGui::GetFont()->GetCharAdvance('A');
    if (ImGui::CollapsingHeader("Stats", ImGuiTreeNodeFlags_DefaultOpen)) {
        if (ImGui::BeginTable("##Stats", 2)) {
            ImGui::TableSetupColumn("##Label", ImGuiTableColumnFlags_WidthFixed, kCharSize * 4);
            ImGui::TableSetupColumn("##Label", ImGuiTableColumnFlags_WidthStretch);
            ImGui::TableNextRow();
            ImGui::TableNextColumn();
            ImGui::TextUnformatted("EMU");
            ImGui::TableNextColumn();
            ImGui::Text("%5.2f fps", frameState_->fps);
            ImGui::TableNextRow();
            ImGui::TableNextColumn();
            ImGui::TextUnformatted("GUI");
            ImGui::TableNextColumn();
            ImGui::Text("%5.2f fps", ImGui::GetIO().Framerate);
            ImGui::TableNextRow();
            ImGui::TableNextColumn();
            ImGui::TextUnformatted("Time");
            ImGui::TableNextColumn();
            uint64_t emulatorTime = 0;
            // if (frameSeqNo_ != kFrameSeqNoInvalid) {
            emulatorTime =
                (uint64_t)(clem_calc_secs_from_clocks(&frameState_->emulatorClock) * 1000);
            //}
            unsigned hours = emulatorTime / 3600000;
            unsigned minutes = (emulatorTime % 3600000) / 60000;
            unsigned seconds = ((emulatorTime % 3600000) % 60000) / 1000;
            unsigned milliseconds = ((emulatorTime % 3600000) % 60000) % 1000;
            ImGui::Text("%02u:%02u:%02u.%01u", hours, minutes, seconds, milliseconds / 100);
            ImGui::EndTable();
        }
    }

    if (ImGui::CollapsingHeader("Mouse", ImGuiTreeNodeFlags_DefaultOpen)) {
        auto *state = frameState_->e1bank;
        if (ImGui::BeginTable("##Mouse", 2)) {
            ImGui::TableSetupColumn("##Label", ImGuiTableColumnFlags_WidthFixed, kCharSize * 4);
            ImGui::TableSetupColumn("##Label", ImGuiTableColumnFlags_WidthStretch);
            ImGui::TableNextRow();
            ImGui::TableNextColumn();
            ImGui::TextUnformatted("Host");
            ImGui::TableNextColumn();
            ImGui::Text("%d,%d", diagnostics.mouseX, diagnostics.mouseY);
            if (state) {
                ImGui::TableNextRow();
                ImGui::TableNextColumn();
                ImGui::TextUnformatted("ROM");
                ImGui::TableNextColumn();
                ImGui::Text("%u,%u", ((uint16_t)(state[0x192]) << 8) | state[0x190],
                            ((uint16_t)(state[0x193]) << 8) | state[0x191]);
            }
            ImGui::EndTable();
        }
    }
}

ImU8 ClemensDebugger::imguiMemoryEditorRead(const ImU8 *mem_ptr, size_t off) {
    const auto *self = reinterpret_cast<const ClemensDebugger *>(mem_ptr);
    if (!self->frameState_->memoryView)
        return 0x00;
    return self->frameState_->memoryView[off & 0xffff];
}

void ClemensDebugger::imguiMemoryEditorWrite(ImU8 *mem_ptr, size_t off, ImU8 value) {
    auto *self = reinterpret_cast<ClemensDebugger *>(mem_ptr);
    self->commandQueue_.debugMemoryWrite((uint16_t)(off & 0xffff), value);
}

void ClemensDebugger::doMachineDebugIORegister(uint8_t * /*ioregsold */, uint8_t * /*ioregs */,
                                               uint8_t /*reg*/) {
    /*
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
    */
}

void ClemensDebugger::doMachineDebugDOCDisplay() {
    if (!frameState_)
        return;

    auto &doc = frameState_->doc;
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
        ImColor oscActiveColor(255, 255, 255);
        ImColor oscHalted(160, 160, 160);
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

void ClemensDebugger::doMachineDebugIWMDisplay(bool detailed) {
    if (!frameState_)
        return;
    auto &iwm = frameState_->iwm;
    static const ImVec4 kOffColor(0.50f, 0.50f, 0.50f, 1.0f);
    static const ImVec4 kLoColor(0.75f, 0.75f, 0.75f, 1.0f);
    static const ImVec4 kHiColor(1.0f, 1.0f, 1.0f, 1.0f);
    static const char *q6q7motor[] = {
        "----", // q60,q70,off
        "STAT", // q61,q70,off
        "HAND", // q60,q71,off
        "MODE", // q61,q71,off
        "READ", // q60,q70,on
        "STAT", // q61,q70,on
        "HAND", // q60,q71,on
        "WRIT"  // q61,q71,on
    };

    // First Section:   IWM State Machine
    // PHI0-3, 3.5?, Dx, ENBL, Q6, Q7, SENSE, Latch

    if (ImGui::BeginTable("IWM_State", 9)) {
        bool driveOn = (iwm.status & ClemensFrame::kIWMStatusDriveOn) != 0; // TODO SmartPort
        unsigned stateFlags = ((iwm.status & ClemensFrame::kIWMStatusIWMQ6) ? 0x1 : 0) |
                              ((iwm.status & ClemensFrame::kIWMStatusIWMQ7) ? 0x2 : 0) |
                              (driveOn ? 0x4 : 0);
        ImGui::TableSetupColumn("Type");
        ImGui::TableSetupColumn("Num");
        ImGui::TableSetupColumn("Phase");
        ImGui::TableSetupColumn("Sense");
        ImGui::TableSetupColumn("Q6Q7");
        ImGui::TableSetupColumn("Read");
        ImGui::TableSetupColumn("Writ");
        ImGui::TableSetupColumn("Ltch");
        ImGui::TableSetupColumn("Cell");
        ImGui::TableHeadersRow();
        ImGui::PushStyleColor(ImGuiCol_Text, driveOn ? kHiColor : kLoColor);
        ImGui::TableNextColumn(); // 0
        if (driveOn) {
            ImGui::TextUnformatted((iwm.status & ClemensFrame::kIWMStatusDrive35) != 0 ? "3.5 "
                                                                                       : "5.25");
        } else {
            ImGui::TextUnformatted("----");
        }
        ImGui::TableNextColumn(); // 1
        ImGui::TextUnformatted(iwm.status & ClemensFrame::kIWMStatusDriveAlt ? "D2" : "D1");
        ImGui::TableNextColumn(); // 2
        ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(0, 0));
        ImGui::TextColored(iwm.ph03 & 1 ? kHiColor : kOffColor, "0");
        ImGui::SameLine();
        ImGui::TextColored(iwm.ph03 & 2 ? kHiColor : kOffColor, "1");
        ImGui::SameLine();
        ImGui::TextColored(iwm.ph03 & 4 ? kHiColor : kOffColor, "2");
        ImGui::SameLine();
        ImGui::TextColored(iwm.ph03 & 8 ? kHiColor : kOffColor, "3");
        ImGui::PopStyleVar();
        ImGui::TableNextColumn(); // 3
        ImGui::TextUnformatted(iwm.status & ClemensFrame::kIWMStatusDriveWP ? "HI" : "LO");
        ImGui::TableNextColumn(); // 4
        ImGui::PushStyleColor(ImGuiCol_Text, stateFlags ? kHiColor : kOffColor);
        ImGui::TextUnformatted(q6q7motor[stateFlags & 0x7]);
        ImGui::PopStyleColor();
        ImGui::TableNextColumn(); // 5
        ImGui::Text("%02X", iwm.data);
        ImGui::TableNextColumn(); // 6
        ImGui::Text("%02X", iwm.data_w);
        ImGui::TableNextColumn(); // 7
        ImGui::Text("%02X", iwm.latch);
        ImGui::TableNextColumn(); // 8
        if (stateFlags) {
            ImGui::Text("%uus", iwm.cell_time);
        } else {
            ImGui::TextUnformatted("   ");
        }
        ImGui::PopStyleColor();
        ImGui::EndTable();
        ImGui::Separator();
    }

    // Second Section:  Active Disk
    // QtrTrack, "Real Track", Bitpos
    bool showBuffer = detailed && iwm.has_disk && iwm.track_bit_length;

    if (ImGui::BeginTable("IWM_Head", 3)) {

        ImGui::TableNextRow();
        ImGui::TableNextColumn();
        ImGui::TextColored(iwm.has_disk ? kHiColor : kLoColor, "QtrTrk: %03d", iwm.qtr_track_index);
        ImGui::TableNextColumn();
        ImGui::TextColored(iwm.has_disk ? kHiColor : kLoColor, "Head: %06d/%06d",
                           iwm.track_byte_index * 8 + (7 - iwm.track_bit_shift),
                           iwm.track_bit_length);
        ImGui::TableNextColumn();
        if (showBuffer) {
            ImGui::SliderInt("Shift", &iwmDiskBitSlip_, 0, 7, "%d", ImGuiSliderFlags_AlwaysClamp);
        }
        ImGui::EndTable();
        ImGui::Separator();
    }

    if (frameState_->isRunning) {
        displayViewNotAvailable(CLEM_L10N_LABEL(kDebugNotAvailableWhileRunning));
    } else if (!iwm.has_disk || !iwm.track_bit_length) {
        displayViewNotAvailable(CLEM_L10N_LABEL(kDebugDiskNoTrackData));
    }
    if (showBuffer) {
        // 40 byte window disk head 8 bits * 8 bytes per row (8 rows)
        // Draw in bit order where we mark starting bytes and the current head
        // position.
        // bit_count = 32 bytes * 8 = 256 bits
        // head_position = bit_count / 2
        // draw bits left to right,;

        constexpr unsigned kRowLimit = 5;
        unsigned rowIndex = 0;

        constexpr unsigned kBitsPerRow = (40 / kRowLimit) * 8;
        constexpr unsigned kBitWindowCount = 40 * 8;

        unsigned absBitHead = iwm.track_byte_index * 8 + 7 - iwm.track_bit_shift;
        unsigned absBitLeft;
        if (absBitHead - kBitWindowCount / 2 > absBitHead) {
            absBitLeft = iwm.track_bit_length - kBitWindowCount / 2 + absBitHead;
        } else {
            absBitLeft = absBitHead - kBitWindowCount / 2;
        }
        unsigned bitOffset;
        if (absBitLeft - iwm.buffer_bit_start_index > iwm.buffer_bit_start_index) {
            bitOffset = iwm.track_bit_length - iwm.buffer_bit_start_index + absBitLeft;
        } else {
            bitOffset = absBitLeft - iwm.buffer_bit_start_index;
        }
        //  once here, we are assured that the bit window (if smaller than the
        //  acquired buffer in IWMStatus, and it should be, is continguous -
        //  so wrapping is not of concern here.)
        //  basically bitOffset is a bit offset relative to the IWMStatus buffer
        //  instead of an absolute index - use absBitIndex to get the track relative
        //  bit offset.

        unsigned bitOffsetEnd = bitOffset + kBitWindowCount;
        unsigned bitOffsetCur = bitOffset;
        unsigned bitSlip = iwmDiskBitSlip_;
        //  bits 15-0 = bitOffset to bitOffset + 15, since disk bytes are 'big' bit-endian
        uint16_t shiftreg = (uint16_t)(iwm.buffer[bitOffsetCur / 8]) << 8;
        shiftreg |= iwm.buffer[(bitOffsetCur / 8) + 1];

        uint16_t slippedData = 0xffff;

        auto &style = ImGui::GetStyle();
        ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(1, style.ItemSpacing.y));

        //  not using a table... seems overkill?  but would like to control the
        //  rendering of this buffer to minimize screen space
        ImVec2 cursorPos = ImGui::GetCursorPos();
        ImVec2 kCellSize(ImGui::GetFont()->GetCharAdvance('0') + style.ItemSpacing.x,
                         ImGui::GetTextLineHeightWithSpacing());

        ImVec2 contentRegionAvail = ImGui::GetContentRegionAvail();

        /* kBitsPerRow + 2 to account for byte hex which may overflow the bits row*/
        const float kTableInnerWidth =
            std::max(kCellSize.x * 6 + (kBitsPerRow + 3) * kCellSize.x, contentRegionAvail.x);
        ImDrawList *drawList = ImGui::GetWindowDrawList();

        if (ImGui::BeginTable("IWM_BufferTable", 2,
                              ImGuiTableFlags_ScrollX | ImGuiTableFlags_ScrollY |
                                  ImGuiTableFlags_RowBg,
                              contentRegionAvail, kTableInnerWidth)) {

            ImGui::TableSetupColumn("Offset", ImGuiTableColumnFlags_WidthFixed, kCellSize.x * 6);
            ImGui::TableSetupColumn("Data", ImGuiTableColumnFlags_WidthStretch);
            ImGui::TableHeadersRow();

            ImGui::PushStyleColor(ImGuiCol_HeaderActive, IM_COL32_WHITE);
            ImGui::PushStyleColor(ImGuiCol_HeaderHovered, IM_COL32_WHITE);
            ImGui::PushStyleColor(ImGuiCol_Header, IM_COL32(192, 192, 192, 255));

            ImVec2 startCursorPos;
            unsigned byteColumnIndex = 0;

            while (bitOffset < bitOffsetEnd) {
                unsigned absBitIndex =
                    (iwm.buffer_bit_start_index + bitOffsetCur) % iwm.track_bit_length;
                if (bitOffset == bitOffsetCur) {
                    if (!(rowIndex & 1)) {
                        ImGui::TableNextColumn();
                        //  start of line
                        ImGui::TextColored(kLoColor, "%06u:", absBitIndex);
                        ImGui::TableNextColumn();
                        startCursorPos = ImGui::GetCursorPos();
                        cursorPos = startCursorPos;
                    } else {
                        cursorPos.x = startCursorPos.x;
                        cursorPos.y = startCursorPos.y + kCellSize.y;
                    }
                } else {
                    ImGui::SetCursorPos(cursorPos);
                }
                bool onByteBoundary = ((bitOffsetCur - bitSlip) % 8) == 0;
                if (onByteBoundary) {
                    slippedData = (uint8_t)((shiftreg << bitSlip) >> 8);

                    if (!(byteColumnIndex % 2)) {
                        ImVec2 ltanchor = ImGui::GetCursorScreenPos();
                        ImVec2 rbanchor = ltanchor;
                        rbanchor.x += 8 * kCellSize.x;
                        rbanchor.y += 2 * kCellSize.y;

                        drawList->AddRectFilled(ltanchor, rbanchor, IM_COL32_BLACK);
                    }
                    ++byteColumnIndex;
                }
                if ((rowIndex & 1)) {
                    //  byte row
                    if (onByteBoundary) {
                        ImGui::Text("^%02X", (uint8_t)(slippedData));
                        ImGui::SameLine();
                    }
                } else {
                    //  bit row
                    //  bit 15 = high bit of current byte, big bit-endian order
                    uint16_t bitValue = (shiftreg >> (15 - (bitOffsetCur % 8))) & 1;

                    if (absBitIndex == absBitHead) {
                        ImGui::PushStyleColor(ImGuiCol_Text, IM_COL32_BLACK);
                        ImGui::Selectable(bitValue ? "1" : "0", true, 0,
                                          kCellSize); // needs to be sized
                        ImGui::SameLine();
                        ImGui::PopStyleColor();
                    } else if (bitValue) {
                        ImGui::TextColored(kHiColor, "1");
                    } else {
                        ImGui::TextColored(kLoColor, "0");
                    }
                    if (ImGui::IsItemHovered(ImGuiHoveredFlags_DelayNormal)) {
                        if (slippedData != 0xffff) {
                            ImGui::SetTooltip("%s @ %u, BYTE:%02X", bitValue ? "HI" : "LO",
                                              absBitIndex, (uint8_t)(slippedData));
                        } else {
                            ImGui::SetTooltip("%s @ %u, BYTE:--", bitValue ? "HI" : "LO",
                                              absBitIndex);
                        }
                    }
                }
                ++bitOffsetCur;
                if ((bitOffsetCur % 8) == 0) {
                    shiftreg <<= 8;
                    shiftreg |= iwm.buffer[(bitOffsetCur / 8) + 1];
                }
                cursorPos.x += kCellSize.x;
                if (!((bitOffsetCur - bitOffset) % kBitsPerRow)) {
                    //  do the bit row or the byte value row
                    ++rowIndex;
                    if (rowIndex & 1) {
                        bitOffsetCur = bitOffset;
                    } else {
                        bitOffset = bitOffsetCur;
                        cursorPos.x += (kCellSize.x * 3); /* byte hex overflow */
                        ImGui::SetCursorPos(startCursorPos);
                        ImGui::Dummy(
                            ImVec2(cursorPos.x - startCursorPos.x, cursorPos.y - startCursorPos.y));
                        ImGui::TableNextRow();
                    }
                    shiftreg = (uint16_t)(iwm.buffer[bitOffsetCur / 8]) << 8;
                    shiftreg |= iwm.buffer[(bitOffsetCur / 8) + 1];
                }
            }
            ImGui::PopStyleColor(3);
            ImGui::EndTable();
        }
        ImGui::PopStyleVar();
    }
}

////////////////////////////////////////////////////////////////////////////////

void ClemensDebugger::executeCommand(std::string_view command) {
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
        listener_.onDebuggerCommandReboot();
    } else if (action == "shutdown") {
        listener_.onDebuggerCommandShutdown();
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
    } else if (action == "paste") {
        listener_.onDebuggerCommandPaste();
    } else if (action == "bsave") {
        cmdBsave(operand);
    } else if (action == "bload") {
        cmdBload(operand);
    } else if (action == "cwd") {
        cmdPwd(operand);
    } else {
        commandQueue_.runScript(std::string(command));
    }
}

void ClemensDebugger::cmdBreak(std::string_view operand) {
    //  parse [r|w]<address>
    auto sepPos = operand.find(',');
    if (sepPos != std::string_view::npos) {
        //  multiple parameter breakpoint expression
        auto param = trimToken(operand, sepPos + 1);
        operand = trimToken(operand, 0, sepPos);
        if (operand == "erase") {
            int index = -1;
            if (!parseInt(param, index)) {
                CLEM_TERM_COUT.format(Error, "Invalid index specified {}", param);
                return;
            } else if (index < 0 || index >= int(breakpoints_.size())) {
                CLEM_TERM_COUT.format(Error, "Breakpoint {} doesn't exist", index);
                return;
            }
            commandQueue_.removeBreakpoint(index);
        }
        return;
    }
    if (operand == "list") {
        //  TODO: granular listing based on operand
        static const char *bpType[] = {"unknown", "execute", "data-read", "write", "IRQ", "BRK"};
        if (breakpoints_.empty()) {
            CLEM_TERM_COUT.print(Info, "No breakpoints defined.");
            return;
        }
        for (size_t i = 0; i < breakpoints_.size(); ++i) {
            auto &bp = breakpoints_[i];
            if (bp.type == ClemensBackendBreakpoint::IRQ ||
                bp.type == ClemensBackendBreakpoint::BRK) {
                CLEM_TERM_COUT.format(Info, "bp #{}: {}", i, bpType[bp.type]);
            } else {
                CLEM_TERM_COUT.format(Info, "bp #{}: {:02X}/{:04X} {}", i,
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
            CLEM_TERM_COUT.format(Error, "Breakpoint type {} is invalid.", typeStr);
            return;
        }
        operand = trimToken(operand, sepPos + 1);
        if (operand.empty()) {
            CLEM_TERM_COUT.format(Error, "Breakpoint type {} is invalid.", typeStr);
            return;
        }
    } else if (operand == "irq") {
        breakpoint.type = ClemensBackendBreakpoint::IRQ;
        breakpoint.address = 0x0;
        commandQueue_.addBreakpoint(breakpoint);
        return;
    } else if (operand == "brk") {
        breakpoint.type = ClemensBackendBreakpoint::BRK;
        breakpoint.address = 0x0;
        commandQueue_.addBreakpoint(breakpoint);
        return;
    } else {
        breakpoint.type = ClemensBackendBreakpoint::Execute;
    }

    char address[16];
    auto bankSepPos = operand.find('/');
    if (bankSepPos == std::string_view::npos) {
        if (operand.size() >= 2) {
            snprintf(address, sizeof(address), "%02X", frameState_->cpu.regs.PBR);
            operand.copy(address + 2, 4, 2);
        } else if (!operand.empty()) {
            CLEM_TERM_COUT.format(Error, "Address {} is invalid.", operand);
            return;
        }
    } else if (bankSepPos == 2 && operand.size() > bankSepPos) {
        operand.copy(address, bankSepPos, 0);
        operand.copy(address + bankSepPos, operand.size() - (bankSepPos + 1), bankSepPos + 1);
    } else {
        CLEM_TERM_COUT.format(Error, "Address {} is invalid.", operand);
        return;
    }
    if (operand.empty()) {
        commandQueue_.breakExecution();
    } else {
        address[6] = '\0';
        char *addressEnd = NULL;
        breakpoint.address = std::strtoul(address, &addressEnd, 16);
        if (addressEnd != address + 6) {
            CLEM_TERM_COUT.format(Error, "Address format is invalid read from '{}'", operand);
            return;
        }
        commandQueue_.addBreakpoint(breakpoint);
    }
}

void ClemensDebugger::cmdRun(std::string_view /*operand*/) { commandQueue_.run(); }

void ClemensDebugger::cmdStep(std::string_view operand) {
    unsigned count = 1;
    if (!operand.empty()) {
        if (std::from_chars(operand.data(), operand.data() + operand.size(), count).ec !=
            std::errc{}) {
            CLEM_TERM_COUT.format(Error, "Couldn't parse a number from '{}' for step", operand);
            return;
        }
    }
    commandQueue_.step(count);
}

void ClemensDebugger::cmdLog(std::string_view operand) {
    static std::array<const char *, 5> logLevelNames = {"DEBUG", "INFO", "WARN", "UNIMPL", "FATAL"};
    if (operand.empty()) {
        CLEM_TERM_COUT.format(Info, "Log level set to {}.", logLevelNames[frameState_->logLevel]);
        return;
    }
    auto levelName = std::find_if(logLevelNames.begin(), logLevelNames.end(),
                                  [&operand](const char *name) { return operand == name; });
    if (levelName == logLevelNames.end()) {
        CLEM_TERM_COUT.format(Error,
                              "Log level '{}' is not one of the following: DEBUG, INFO, "
                              "WARN, UNIMPL or FATAL",
                              operand);
        return;
    }
    commandQueue_.debugLogLevel(int(levelName - logLevelNames.begin()));
}

void ClemensDebugger::cmdReset(std::string_view /*operand*/) { commandQueue_.reset(); }

void ClemensDebugger::cmdTrace(std::string_view operand) {
    auto [params, cmd, paramCount] = gatherMessageParams(operand);
    if (paramCount > 2) {
        CLEM_TERM_COUT.format(Error, "Trace command doesn't recognize parameter {}",
                              params[paramCount]);
        return;
    }
    if (paramCount == 0) {
        CLEM_TERM_COUT.format(Info, "Trace is {}", frameState_->isTracing ? "active" : "inactive");
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
        if (!frameState_->isTracing) {
            if (!enable) {
                CLEM_TERM_COUT.print(Info, "Not tracing.");
            } else {
                CLEM_TERM_COUT.print(Info, "Enabling trace.");
            }
        } else {
            if (!enable) {
                if (path.empty()) {
                    CLEM_TERM_COUT.print(
                        Warn, "Trace will be lost as tracing was active but no output file"
                              " was specified");
                }
            }
            if (!path.empty()) {
                CLEM_TERM_COUT.format(Info, "Trace will be saved to {}", path);
            }
        }
    } else if (frameState_->isTracing) {
        if (params[0] == "iwm") {
            if (frameState_->isIWMTracing) {
                CLEM_TERM_COUT.print(Info, "IWM tracing deactivated");
            } else {
                CLEM_TERM_COUT.print(Info, "IWM tracing activated");
            }
        } else {
            CLEM_TERM_COUT.format(Error, "Invalid tracing option '{}'", params[0]);
        }
    } else {
        CLEM_TERM_COUT.print(Error, "Operation only allowed while tracing is active.");
    }
    commandQueue_.debugProgramTrace(std::string(params[0]), path);
}

void ClemensDebugger::cmdSave(std::string_view operand) {
    auto [params, cmd, paramCount] = gatherMessageParams(operand);
    if (paramCount != 1) {
        CLEM_TERM_COUT.print(Error, "Save requires a filename.");
        return;
    }
    commandQueue_.saveMachine(std::string(params[0]), nullptr);
}

void ClemensDebugger::cmdLoad(std::string_view operand) {
    auto [params, cmd, paramCount] = gatherMessageParams(operand);
    if (paramCount != 1) {
        CLEM_TERM_COUT.print(Error, "Load requires a filename.");
        return;
    }
    commandQueue_.loadMachine(std::string(params[0]));
}

void ClemensDebugger::cmdDump(std::string_view operand) {
    //  parse out parameters <start>, <end>, <filename>, <format>
    //  if format is absent, dumps to binary
    auto [params, cmd, paramIdx] = gatherMessageParams(operand);
    if (paramIdx < 3) {
        CLEM_TERM_COUT.print(Error, "Command requires <start_bank>, <end_bank>, <filename>");
        return;
    }
    uint8_t bankl, bankr;
    if (std::from_chars(params[0].data(), params[0].data() + params[0].size(), bankl, 16).ec !=
        std::errc{}) {
        CLEM_TERM_COUT.format(Error, "Command start bank '{}' is invalid", params[0]);
        return;
    }
    if (std::from_chars(params[1].data(), params[1].data() + params[1].size(), bankr, 16).ec !=
            std::errc{} ||
        bankl > bankr) {
        CLEM_TERM_COUT.format(Error, "Command end bank '{}' is invalid", params[1]);
        return;
    }
    if (paramIdx == 3) {
        params[3] = "bin";
    }
    if (paramIdx == 4 && params[3] != "hex" && params[3] != "bin") {
        CLEM_TERM_COUT.print(Error, "Command format type must be 'hex' or 'bin'");
        return;
    }
    std::string message = "dump ";
    for (auto &param : params) {
        message += param;
        message += ",";
    }
    message.pop_back();
    // CK_TODO(DebugMessage : Send Message to thread so it can fill in the data)
    commandQueue_.debugMessage(std::move(message));
}

void ClemensDebugger::cmdDisk(std::string_view operand) {
    // disk
    // disk <drive>,file=<image>
    // disk <drive>,wprot=off|on
    if (operand.empty()) {
        for (auto it = frameState_->frame.diskDriveStatuses.begin();
             it != frameState_->frame.diskDriveStatuses.end(); ++it) {
            auto driveType =
                static_cast<ClemensDriveType>(it - frameState_->frame.diskDriveStatuses.begin());
            CLEM_TERM_COUT.format(Info, "{} {}: {}", it->isWriteProtected ? "wp" : "  ",
                                  ClemensDiskUtilities::getDriveName(driveType),
                                  it->assetPath.empty() ? "<none>" : it->assetPath);
        }
        return;
    }
    auto sepPos = operand.find(',');
    auto driveType = ClemensDiskUtilities::getDriveType(trimToken(operand, 0, sepPos));
    if (driveType == kClemensDrive_Invalid) {
        CLEM_TERM_COUT.format(Error, "Invalid drive name {} specified.", operand);
        return;
    }
    auto &driveInfo = frameState_->frame.diskDriveStatuses[driveType];
    std::string_view diskOpExpr;
    if (sepPos == std::string_view::npos || (diskOpExpr = trimToken(operand, sepPos + 1)).empty()) {

        CLEM_TERM_COUT.format(Info, "{} {}: {}", driveInfo.isWriteProtected ? "wp" : "  ",
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
            commandQueue_.ejectDisk(driveType);
        } else {
            validOp = false;
        }
    } else {
        diskOpValue = trimToken(diskOpExpr, sepPos + 1);
        if (diskOpType == "file") {
            commandQueue_.insertDisk(driveType, std::string(diskOpValue));
        } else if (diskOpType == "wprot") {
            bool wprot;
            if (parseBool(diskOpValue, wprot)) {
                commandQueue_.writeProtectDisk(driveType, wprot);
            } else {
                validValue = false;
            }
        } else {
            validOp = false;
        }
    }
    if (!validValue) {
        CLEM_TERM_COUT.format(Error, "Invalid value {} in expression.", diskOpValue);
        return;
    } else if (!validOp) {
        CLEM_TERM_COUT.format(Error, "Invalid or unsupported operation {}.", diskOpExpr);
        return;
    }
}

void ClemensDebugger::cmdBload(std::string_view operand) {
    auto [params, cmd, paramCount] = gatherMessageParams(operand);
    if (paramCount != 2) {
        CLEM_TERM_COUT.format(Error, "usage: bload <pathname>, <address>");
        return;
    }
    //  assumed hex numbers
    unsigned address;
    if (std::from_chars(params[1].data(), params[1].data() + params[1].size(), address, 16).ec !=
        std::errc{}) {
        CLEM_TERM_COUT.format(Error, "Address must be a hexadecimal integer");
        return;
    }

    commandQueue_.bload(std::string(params[0]), address);
}

void ClemensDebugger::cmdBsave(std::string_view operand) {
    auto [params, cmd, paramCount] = gatherMessageParams(operand);
    if (paramCount != 3) {
        CLEM_TERM_COUT.format(Error, "usage: bsave <pathname>, <address>, <length>");
        return;
    }
    //  assumed hex numbers
    unsigned address, length;
    if (std::from_chars(params[1].data(), params[1].data() + params[1].size(), address, 16).ec !=
        std::errc{}) {
        CLEM_TERM_COUT.format(Error, "Address must be a hexadecimal integer");
        return;
    }
    if (std::from_chars(params[2].data(), params[2].data() + params[2].size(), length, 16).ec !=
        std::errc{}) {
        CLEM_TERM_COUT.format(Error, "Length must be a hexadecimal integer");
        return;
    }

    commandQueue_.bsave(std::string(params[0]), address, length);
}

void ClemensDebugger::cmdPwd(std::string_view) {
    std::error_code ec{};
    auto curPath = std::filesystem::current_path(ec);
    if (ec) {
        CLEM_TERM_COUT.format(Error, "pwd error {}", ec.message());
    } else {
        CLEM_TERM_COUT.print(Info, curPath.string());
    }
}

void ClemensDebugger::cmdHelp(std::string_view operand) {
    if (!operand.empty()) {
        CLEM_TERM_COUT.print(Warn, "Command specific help not yet supported.");
    }
    CLEM_TERM_COUT.print(Info, "shutdown                    - exit the emulator");
    CLEM_TERM_COUT.print(Info, "reset                       - soft reset of the machine");
    CLEM_TERM_COUT.print(Info, "reboot                      - hard reset of the machine");
    CLEM_TERM_COUT.print(Info, "disk                        - disk information");
    CLEM_TERM_COUT.print(Info, "disk <drive>,file=<image>   - insert disk");
    CLEM_TERM_COUT.print(Info, "disk <drive>,wprot=<off|on> - write protect");
    CLEM_TERM_COUT.print(Info, "disk <drive>,eject          - eject disk");
    CLEM_TERM_COUT.print(Info, "r]un                        - execute emulator until break");
    CLEM_TERM_COUT.print(Info, "s]tep                       - steps one instruction");
    CLEM_TERM_COUT.print(Info, "s]tep <count>               - step 'count' instructions");
    CLEM_TERM_COUT.print(Info, "b]reak                      - break execution at current PC");
    CLEM_TERM_COUT.print(Info, "b]reak <address>            - break execution at address");
    CLEM_TERM_COUT.print(Info, "b]reak r:<address>          - break on data read from address");
    CLEM_TERM_COUT.print(Info, "b]reak w:<address>          - break on write to address");
    CLEM_TERM_COUT.print(Info, "b]reak erase,<index>        - remove breakpoint with index");
    CLEM_TERM_COUT.print(Info, "b]reak irq                  - break on IRQ");
    CLEM_TERM_COUT.print(Info, "b]reak brk                  - break on IRQ");
    CLEM_TERM_COUT.print(Info, "b]reak list                 - list all breakpoints");
    CLEM_TERM_COUT.print(Info, "log {DEBUG|INFO|WARN|UNIMPL}- set the emulator log level");
    CLEM_TERM_COUT.print(Info, "dump <bank_begin>,          - dump memory from selected banks\n"
                               "     <bank_end>,              to a file with the specified\n"
                               "     <filename>, {bin|hex}    output format");
    CLEM_TERM_COUT.print(Info,
                         "trace {on|off},<pathname>   - toggle program tracing and output to file");
    CLEM_TERM_COUT.print(
        Info, "save <pathname>             - saves a snapshot into the snapshots folder");
    CLEM_TERM_COUT.print(
        Info, "load <pathname>             - loads a snapshot into the snapshots folder");
    CLEM_TERM_COUT.print(
        Info,
        "bsave <pathname>,<address>,<length>  - saves binary to file from location in memory");
    CLEM_TERM_COUT.print(Info, "bload <pathname>,<address>             - loads binary to address");
    CLEM_TERM_COUT.print(Info, "pwd                         - current working directory");

    CLEM_TERM_COUT.newline();
}

void ClemensDebugger::print(LogLineType type, const char *str) { CLEM_TERM_COUT.print(type, str); }
