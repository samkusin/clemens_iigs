#include <stdbool.h>
#include <stdint.h>


/*
 * The Clemens Emulator
 * =====================
 * The Emulation Layer facilities practical I/O between a host application and
 * the "internals" of the machine (CPU, FPI, MEGA2, I/O state.)
 *
 * "Practical I/O" comes from and is accessed by the 'Host' application.  Input
 * includes keyboard, mouse and gamepad events, disk images.  Output includes
 * video, speaker and other devices (TBD.)   The emulator provides the
 * controlling components for this I/O.
 *
 *
 * Emulation
 * ---------
 * There are three major components executed in the emulation loop: the CPU,
 * FPI and MEGA2.  Wrapping these components is a 'bus controller' plus RAM and
 * ROM units.
 *
 * The MEGA2, following the IIgs firmware/hardware references acts as a
 * 'frontend' for the machine's I/O.  Since Apple II uses memory mapped I/O to
 * control devices, this mostly abstracts the I/O layer from the emulation
 * loop.
 *
 * The loop performs the following:
 *  - execute CPU for a time slice until
 *      - a set number of clocks passes
 *      - a memory access occurs
 *      - ???
 *  - interrupts are checked per time-slice,
 *      - if triggered, set the CPU state accordingly
 *      - ???
 */


enum {
    kClemensCPUStatus_Carry              = (1 << 0),     // C
    kClemensCPUStatus_Zero               = (1 << 1),     // Z
    kClemensCPUStatus_IRQDisable         = (1 << 2),     // I
    kClemensCPUStatus_Decimal            = (1 << 3),     // D
    kClemensCPUStatus_Index              = (1 << 4),     // X,
    kClemensCPUStatus_MemoryAccumulator  = (1 << 5),     // M,
    kClemensCPUStatus_Overflow           = (1 << 6),     // V
    kClemensCPUStatus_Negative           = (1 << 7)      // N
};

struct ClemensCPURegs {
    uint16_t A;
    uint16_t X;
    uint16_t Y;
    uint16_t D;                         // Direct
    uint16_t S;                         // Stack
    uint16_t PC;                        // Program Counter
    uint8_t IR;                         // Instruction Register
    uint8_t P;                          // Processor Status
    uint8_t DBR;                        // Data Bank (Memory)
    uint8_t PBR;                        // Program Bank (Memory)
};

struct ClemensCPUPins {
    uint16_t adr;                       // A0-A15 Address
    uint8_t data;                       // Data (8-bit)
    bool abortIn;                       // ABORTB In
    bool busEnableIn;                   // Bus Enable
    bool emulationOut;                  // Emulation Status
    bool irqIn;                         // Interrupt Request
    bool memLockOut;                    // Memory Lock
    bool memIdxSelOut;                  // Memory/Index Select
    bool nmiIn;                         // Non-Maskable Interrupt
    bool rwbOut;                        // Read/Write byte
    bool readyInOut;                    // Ready CPU
    bool resbIn;                        // RESET
    bool vdaOut;                        // Valid Data Address
    bool vpaOut;                        // Valid Program Address
    bool vpbOut;                        // Vector Pull
};

enum ClemensCPUStateType {
    kClemensCPUStateType_None,
    kClemensCPUStateType_Reset
};

struct ClemensCPU65C816 {
    struct ClemensCPURegs regs;
    struct ClemensCPUPins pins;
    enum ClemensCPUStateType state_type;
    bool emulation;
    bool intr_brk;
};


typedef struct {
    struct ClemensCPU65C816 cpu;
    uint32_t clocks_step;
    bool interrupt_reset_b;             // signal to CPU: resb
} ClemensMachine;


static


uint32_t emulate(
    struct ClemensCPU65C816* cpu,
    uint32_t clocks_to_run,
    uint32_t clocks_step
) {
    uint32_t clocks_used = 0;

    if (!cpu->pins.resbIn) {
        //  the reset interrupt overrides any other state
        //  start in emulation mode, 65C02 stack, regs, etc.
        if (cpu->state_type != kClemensCPUStateType_Reset) {
            cpu->state_type = kClemensCPUStateType_Reset;
            cpu->regs.D = 0x0000;
            cpu->regs.DBR = 0x00;
            cpu->regs.PBR = 0x00;
            cpu->regs.S &= 0x00ff;  cpu->regs.S |= 0x0100;
            cpu->regs.X &= 0x00ff;
            cpu->regs.Y &= 0x00ff;

            cpu->regs.P = cpu->regs.P & ~(
                kClemensCPUStatus_MemoryAccumulator |
                kClemensCPUStatus_Index |
                kClemensCPUStatus_Decimal |
                kClemensCPUStatus_IRQDisable |
                kClemensCPUStatus_Carry);
            cpu->regs.P |= (
                kClemensCPUStatus_MemoryAccumulator |
                kClemensCPUStatus_Index |
                kClemensCPUStatus_IRQDisable |
                kClemensCPUStatus_Carry);
            cpu->intr_brk = false;
            cpu->emulation = true;
            cpu->pins.emulationOut = true;
            cpu->pins.memIdxSelOut = true;
            cpu->pins.rwbOut = true;
            cpu->pins.vpbOut = true;
            cpu->pins.vdaOut = false;
            cpu->pins.vpaOut = false;
            clocks_used += clocks_step * 2;
        }
    } else {
        //  RESB high during reset invokes our interrupt microcode
        if (cpu->state_type == kClemensCPUStateType_Reset) {

        }
    }


}