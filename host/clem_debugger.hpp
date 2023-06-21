#ifndef CLEM_HOST_DEBUGGER_HPP
#define CLEM_HOST_DEBUGGER_HPP

#include "clem_frame_state.hpp"
#include "clem_host_shared.hpp"

#include "clem_types.h"

#include "cinek/circular_buffer.hpp"
#include "imgui.h"
#include "imgui_memory_editor.h"

#include <vector>

class ClemensCommandQueue;
class ClemensDebuggerListener;

class ClemensDebugger {

  public:
    ClemensDebugger(ClemensCommandQueue &commandQueue, ClemensDebuggerListener &listener);

    void lastFrame(const ClemensFrame::FrameState &lastFrameState);
    bool thisFrame(ClemensFrame::LastCommandState &lastCommandState,
                   const ClemensFrame::FrameState &frameState);

    std::vector<ClemensBackendBreakpoint> copyBreakpoints() const;

    //  These should all be gui elements that assume the caller is setting layout
    //  values (Item Width, window size, etc
    //  various parts of the frontend FrameState to display.

    //  Draws the main debugger console
    void console(ImVec2 anchor, ImVec2 dimensions);
    //  Draws a column of CPU state entries
    void cpuStateTable(ImVec2 anchor, ImVec2 dimensions);
    //  Draws the auxillary view
    void auxillary(ImVec2 anchor, ImVec2 dimensions);

    enum LogLineType { Debug, Info, Warn, Error, Command, Opcode };

    void print(LogLineType type, const char *str);

  private:
    ClemensCommandQueue &commandQueue_;
    ClemensDebuggerListener &listener_;

    const ClemensFrame::FrameState *frameState_;
    char consoleInputLineBuf_[120];

    struct TerminalLine {
        using Type = LogLineType;
        std::string text;
        Type type;
    };
    // TODO: this may need to be dynamic...
    template <size_t N> using TerminalBuffer = cinek::CircularBuffer<TerminalLine, N>;
    TerminalBuffer<1024> consoleLines_;
    bool consoleChanged_;

    void layoutConsoleLines(ImVec2 dimensions);
    template <typename TBufferType> friend struct FormatView2;

  private:
    MemoryEditor debugMemoryEditor_;

    static ImU8 imguiMemoryEditorRead(const ImU8 *mem_ptr, size_t off);
    static void imguiMemoryEditorWrite(ImU8 *mem_ptr, size_t off, ImU8 value);

    void doMachineDebugIORegister(uint8_t *ioregsold, uint8_t *ioregs, uint8_t reg);

  private:
    ClemensCPUPins lastFrameCPUPins_;
    ClemensCPURegs lastFrameCPURegs_;
    ClemensFrame::ADBStatus lastFrameADBStatus_;
    ClemensFrame::IWMStatus lastFrameIWM_;
    uint32_t lastFrameIRQs_, lastFrameNMIs_;
    uint8_t lastFrameIORegs_[256];

    std::vector<ClemensBackendBreakpoint> breakpoints_;

    void cpuStatRow16(const char *label, const char *attrName, uint16_t value, float labelWidth,
                      const ImVec4 &color);
    void cpuStatRow8(const char *label, const char *attrName, uint8_t value, float labelWidth,
                     const ImVec4 &color);
    void cpuProcessorFlag(const char *label, uint8_t flag);

    void executeCommand(std::string_view command);

    void cmdHelp(std::string_view operand);
    void cmdBreak(std::string_view operand);
    void cmdRun(std::string_view operand);
    void cmdReboot(std::string_view operand);
    void cmdShutdown(std::string_view operand);
    void cmdReset(std::string_view operand);
    void cmdDisk(std::string_view operand);
    void cmdStep(std::string_view operand);
    void cmdLog(std::string_view operand);
    void cmdTrace(std::string_view operand);
    void cmdSave(std::string_view operand);
    void cmdLoad(std::string_view operand);
    void cmdDump(std::string_view operand);
};

class ClemensDebuggerListener {
  public:
    virtual ~ClemensDebuggerListener() = default;

    virtual void onDebuggerCommandReboot() = 0;
    virtual void onDebuggerCommandShutdown() = 0;
    virtual void onDebuggerCommandPaste() = 0;
};

#endif
