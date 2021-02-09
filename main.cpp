#include <cstdint>

namespace clements
{

struct CPU65C816
{
  struct {
    uint16_t A;
    uint16_t X;
    uint16_t Y;
    uint16_t D;         // Direct
    uint16_t S;         // Stack
    uint16_t PC;        // Program Counter
    uint8_t IR;         // Instruction Register
    uint8_t P;          // Processor Status
    uint8_t DBR;        // Data Bank (Memory)
    uint8_t PBR;        // Program Bank (Memory)
  } regs;

  struct {
    uint16_t adr;       // A0-A15 Address
    uint8_t data;       // Data (8-bit)
    uint8_t phi2EmuOut; // The embedded-phased clock signal (emulating a PHI2)
    bool abortIn;       // ABORTB In
    bool busEnableIn;   // Bus Enable
    bool emulationOut;  // Emulation Status
    bool irqIn;         // Interrupt Request
    bool memLockOut;    // Memory Lock
    bool memIdxSelOut;  // Memory/Index Select
    bool nmiIn;         // Non-Maskable Interrupt
    bool rwbOut;        // Read/Write byte
    bool readyInOut;    // Ready CPU
    bool resbIn;        // RESET
    bool vdaOut;        // Valid Data Address
    bool vpaOut;        // Valid Program Address
    bool vpbOut;        // Vector Pull
  } pins;

  //  CPU State
  enum class State {
    None,
    Reset
  };
  State state;
  unsigned stateCycles;         // Number of cycles the state has been in.

  State setState(State s) {
    state = s;
    stateCycles = 0;
    return state;
  }
};

uint16_t set_reg_hi8(uint16_t hi)
{
  uint16_t reg;
  reg &= 0x00ff;
  reg |= hi;
  return reg;
}


uint32_t emulate(CPU65C816& cpu, uint32_t cycleCounter)
{
  // interpret input pins
  if (!cpu.pins.resbIn) {
    //  RESB must be set at least two cycles before releasing for a valid CPU
    //  reset.
    if (cpu.state != CPU65C816::State::Reset) {
      cpu.state = cpu.setState(CPU65C816::State::Reset);
      cpu.regs.D = 0x0000;
      cpu.regs.DBR = 0x00;
      cpu.regs.PBR = 0x00;
      cpu.regs.S = set_reg_hi8(0x0100);
      cpu.regs.X = set_reg_hi8(0x0000);
      cpu.regs.Y = set_reg_hi8(0x0000);

    } else if (cpu.stateCycles >= 2) {


    }
  }
}

} // namespace clements


/**
 *  The Apple //gs Clements Emulator
 *
 *  CPU
 *  Mega II emulation
 *  Memory
 *    ROM
 *    RAM
 *  I/O
 *    IWM
 *    ADB (keyboard + mouse)
 *    Ports 1-7
 *    Ensoniq
 *
 * Approach:
 *
 */



int main(int argc, const char* argv[])
{
  clements::CPU65C816 cpu;
  bool systemPowerOn = true;
  uint32_t cycleCounter = 0;

  cpu.state = clements::CPU65C816::State::None;

  //  Assuming systemPowerOn is true
  cpu.pins.resbIn = false;    // active low

  while (systemPowerOn) {
    cycleCounter = clements::emulate(cpu, cycleCounter);
    if (cycleCounter >= 1000) {
      break;
    }
  }

  return 0;
}
