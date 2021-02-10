#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <assert.h>


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

#define CLEM_65816_RESET_VECTOR_LO_ADDR     (0xFFFC)
#define CLEM_65816_RESET_VECTOR_HI_ADDR     (0xFFFD)
#define CLEM_IIGS_BANK_SIZE                 (64 * 1024)
#define CLEM_IIGS_ROM3_SIZE                 (CLEM_IIGS_BANK_SIZE * 4)

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
    uint8_t databank;                   // Bank when clockHi, else data
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
    kClemensCPUStateType_Reset,
    kClemensCPUStateType_Execute
};

struct Clemens65C816 {
    struct ClemensCPUPins pins;
    struct ClemensCPURegs regs;
    enum ClemensCPUStateType state_type;
    uint32_t cycles_spent_in_frame;
    uint16_t PC_NEXT;
    bool emulation;
    bool intr_brk;
};


typedef struct {
    struct Clemens65C816 cpu;
    uint32_t clocks_step;
    uint32_t clocks_spent_in_frame;
    uint8_t* fpi_bank_map[256];
} ClemensMachine;


int clemens_init(
    ClemensMachine* machine,
    uint32_t clocks_step,
    void* rom,
    size_t romSize
) {
    machine->clocks_step = clocks_step;
    machine->clocks_spent_in_frame = 0;
    if (romSize != CLEM_IIGS_ROM3_SIZE) {
        return -1;
    }

    /* memory organization for the FPI */
    machine->fpi_bank_map[0xfc] = (uint8_t*)rom;
    machine->fpi_bank_map[0xfd] = (uint8_t*)rom + CLEM_IIGS_BANK_SIZE;
    machine->fpi_bank_map[0xfe] = (uint8_t*)rom + CLEM_IIGS_BANK_SIZE * 2;
    machine->fpi_bank_map[0xff] = (uint8_t*)rom + CLEM_IIGS_BANK_SIZE * 3;

    return 0;
}


static inline void _clem_nop(
    ClemensMachine* clem,
    uint32_t cycle_count
) {
    clem->clocks_spent_in_frame += clem->clocks_step * cycle_count;
}

static inline void _clem_mem_read(
    ClemensMachine* clem,
    uint8_t* data,
    uint16_t adr,
    uint8_t bank
) {
    clem->cpu.pins.adr = adr;
    clem->cpu.pins.databank = bank;
    if (clem->cpu.pins.emulationOut) {
        //  TODO: optimize
        if (adr >= 0xd000) {
            *data = clem->fpi_bank_map[0xff][adr];
        } else {
            *data = 0x00;
        }
    }

    clem->clocks_spent_in_frame += clem->clocks_step;
}

static inline void _clem_mem_write(
    ClemensMachine* clem,
    uint8_t data,
    uint16_t adr,
    uint8_t bank
) {
    clem->cpu.pins.adr = adr;
    clem->cpu.pins.databank = bank;
    clem->clocks_spent_in_frame += clem->clocks_step;
}

static inline void _cpu_sp_dec2(
    struct Clemens65C816* cpu
) {
    uint16_t tmp = cpu->regs.S - 2;
    if (cpu->emulation) {
        tmp = (cpu->regs.S & 0xff00) | (tmp & 0x00ff);
    }
    cpu->regs.S = tmp;
}

static inline void _cpu_sp_dec(
    struct Clemens65C816* cpu
) {
    uint16_t tmp = cpu->regs.S - 1;
    if (cpu->emulation) {
        tmp = (cpu->regs.S & 0xff00) | (tmp & 0x00ff);
    }
    cpu->regs.S = tmp;
}

void emulate(ClemensMachine* clem) {
    struct Clemens65C816* cpu = &clem->cpu;
    uint16_t tmp_addr;
    uint16_t tmp_reg;
    uint8_t tmp_data;
    uint8_t tmp_datahi;

    if (!cpu->pins.resbIn) {
        /*  the reset interrupt overrides any other state
            start in emulation mode, 65C02 stack, regs, etc.
        */
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
            _clem_nop(clem, 1);
        }
        _clem_nop(clem, 1);
        return;
    }
    //  RESB high during reset invokes our interrupt microcode
    if (cpu->state_type == kClemensCPUStateType_Reset) {
        _clem_mem_read(clem, &tmp_data, cpu->regs.S, 0x00);
        tmp_addr = cpu->regs.S - 1;
        if (cpu->emulation) {
            tmp_addr = (
                (cpu->regs.S & 0xff00) | (tmp_addr & 0x00ff));
        }
        _clem_mem_read(clem, &tmp_datahi, tmp_addr, 0x00);
        _cpu_sp_dec2(cpu);
        _clem_mem_read(clem, &tmp_data, cpu->regs.S, 0x00);
        _cpu_sp_dec(cpu);
        _clem_mem_read(clem, &tmp_data, CLEM_65816_RESET_VECTOR_LO_ADDR, 0x00);
        _clem_mem_read(clem, &tmp_datahi, CLEM_65816_RESET_VECTOR_HI_ADDR, 0x00);
        cpu->PC_NEXT = (uint16_t)(tmp_datahi << 8) | tmp_data;
        cpu->state_type = kClemensCPUStateType_Execute;
        return;
    }

    assert(cpu->state_type == kClemensCPUStateType_Execute);
    /*
        Execute all cycles of an instruction here
    */
    cpu->regs.PC = cpu->PC_NEXT;
    _clem_mem_read(clem, &tmp_data, cpu->regs.PC, cpu->regs.PBR);
    printf("%02x\n", tmp_data);
    ++cpu->regs.PC;
    _clem_mem_read(clem, &tmp_data, cpu->regs.PC, cpu->regs.PBR);
    printf("%02x\n", tmp_data);
    ++cpu->regs.PC;
    _clem_mem_read(clem, &tmp_data, cpu->regs.PC, cpu->regs.PBR);
    printf("%02x\n", tmp_data);
    ++cpu->regs.PC;
}

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



int main(int argc, char* argv[])
{
    ClemensMachine machine;

    /*  ROM 3 only */
    FILE* fp = fopen("gs_rom_3.rom", "rb");
    void* rom = NULL;
    if (fp == NULL) {
        fprintf(stderr, "No ROM\n");
        return 1;
    }
    rom = malloc(CLEM_IIGS_ROM3_SIZE);
    if (fread(rom, 1, CLEM_IIGS_ROM3_SIZE, fp) != CLEM_IIGS_ROM3_SIZE) {
        fprintf(stderr, "Bad ROM\n");
        return 1;
    }
    fclose(fp);

    memset(&machine, 0, sizeof(machine));
    clemens_init(&machine, 1000, rom, 256 * 1024);

    machine.cpu.pins.resbIn = false;
    emulate(&machine);
    machine.cpu.pins.resbIn = true;
    emulate(&machine);
    emulate(&machine);

    return 0;
}
