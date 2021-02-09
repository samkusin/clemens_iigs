#include <cstdint>

namespace clements
{

struct CPU65C816
{
  enum {
    kProcessorStatus_Carry              = (1 << 0),     // C
    kProcessorStatus_Zero               = (1 << 1),     // Z
    kProcessorStatus_IRQDisable         = (1 << 2),     // I
    kProcessorStatus_Decimal            = (1 << 3),     // D
    kProcessorStatus_Index              = (1 << 4),     // X,
    kProcessorStatus_MemoryAccumulator  = (1 << 5),     // M,
    kProcessorStatus_Overflow           = (1 << 6),     // V
    kProcessorStatus_Negative           = (1 << 7)      // N
  };
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
    Reset,
    FetchExecute
  };
  State state;
  unsigned stateCycles; // Number of cycles the state has been in.
  bool emulation;
  bool brk;


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


//  Concepts
//    Interrupts:
//      - Value of PC is either read from (RESET) or written to the stack
//      (EVERYTHING ELSE) 2 memory cycles
//      - Value of P is again either read or written to the stack (1 memory
//        cycle)
//      - Read two bytes of the interrupt vector from the vector address
//

//  Components
//    EMUBUS <-> CPU
//    EMUBUS <-> FPI
//    EMUBUS <-> MEGA2
//
//  Emulation
//    emulate(EMUBUS)
//
//  Idea, the EmulatorBuf runs all of the components CPU, FPI, MEGA2
//
//  EmulatorBus
/*
uint32_t emulate(EmulatorBus& emuBus, CPU65C816& cpu, ComponentFPI& fpi, ComponentMega2& mega2)
{

}
*/

uint32_t emulate(CPU65C816& cpu, EmulationBus& emubus)
{
  const uint32_t kClockStep = 1000;   // 1hz = 1000 units
  uint32_t clocksSpent = 0;
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
      //  Only M=1, X=1, D=0, I, C/E=1 are set
      cpu.regs.P = cpu.regs.P & ~(
        CPU65C816::kProcessorStatus_MemoryAccumulator |
        CPU65C816::kProcessorStatus_Index |
        CPU65C816::kProcessorStatus_Decimal |
        CPU65C816::kProcessorStatus_IRQDisable |
        CPU65C816::kProcessorStatus_Carry);
      cpu.regs.P |= (
        CPU65C816::kProcessorStatus_MemoryAccumulator |
        CPU65C816::kProcessorStatus_Index |
        CPU65C816::kProcessorStatus_IRQDisable |
        CPU65C816::kProcessorStatus_Carry);
      cpu.emulation = true;
      // TODO: The eponimous B flag... assumption here
      //  http://visual6502.org/wiki/index.php?title=6502_BRK_and_B_bit
      cpu.brk = false;
      cpu.pins.emulationOut = true;
      cpu.pins.memIdxSelOut = true;
      cpu.pins.rwbOut = true;
      cpu.pins.vpbOut = true;
      cpu.pins.vdaOut = false;
      cpu.pins.vpaOut = false;
      clocksSpent += kClockStep;
    }
  } else {
    if (cpu.state == CPU65C816::State::Reset) {
      //  resbIn back to 1 - initiate reset and interrupt sequence
      //  TODO: clear STP and WAI
      //        RWB high during stack address cycle
      //        Set PC to the contents of Reset vector address 0xFFFC,D

      //  Normally we push the PC and P registers to the stack
      //  RESET is an exceptional case, as we are reading values while
      //    incrementing the stack pointer.  RESET doesn't use these values any
      //    way, but we include the microcode so we can at least accumalate the
      //    cycles, and for educational purposes
      uint16_t SP = cpu.regs.S;
      emubus.read(cpu, SP, 0x00);
      if (cpu.emulation) {
        SP = (SP & 0xff00) | ((SP & 0x00ff) - 1);
      } else {
        ++SP;
      }
      emubus.read(SP, 0x00);
      cpu.decS2();
      SP = cpu.regs.S;
      emubus.read(SP, 0x00);
      cpu.decS();
      cpu.pins.vpbOut = true;
      cpu.regs.PC = emubus.read(0xFFFC, 0x00) | (
        (uint16_t)emubus.read(0xFFFD, 0x00) << 8);
      cpu.pins.vpbOut = false;
      cpu.state = CPU65C816::State::FetchExecute;
    }
  }

  return clocksSpent;
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
  uint32_t clocksCounter = 0;
  uint32_t stepCounter = 0;
  cpu.state = clements::CPU65C816::State::None;

  //  Assuming systemPowerOn is true
  cpu.pins.resbIn = false;    // active low

  while (systemPowerOn) {
    clocksCounter += clements::emulate(cpu);
    ++stepCounter;
    if (clocksCounter >= 1000) {
      break;
    }
  }

  return 0;
}
