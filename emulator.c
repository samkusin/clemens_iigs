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

/*  For historical reasons, these opcodes are ordered by the approximate time
    when they were implemented/discovered by the emulator
*/
#define CLEM_OPC_LDA_MODE_01                (0xA0)
#define CLEM_OPC_TSB_ABS                    (0x0C)
#define CLEM_OPC_SEI                        (0x78)
#define CLEM_OPC_CLC                        (0x18)
#define CLEM_OPC_TCS                        (0x1B)
#define CLEM_OPC_CLD                        (0xD8)
#define CLEM_OPC_XCE                        (0xFB)
#define CLEM_OPC_SEC                        (0x38)
#define CLEM_OPC_JSR                        (0x20)
#define CLEM_OPC_RTS                        (0x60)
#define CLEM_OPC_JSL                        (0x22)
#define CLEM_OPC_RTL                        (0x6B)

#define CLEM_ADR_MODE_01_IMMEDIATE          (0x08)

/* Attempt to mimic VDA and VPA per memory access */
#define CLEM_MEM_FLAG_OPCODE_FETCH          (0x3)
#define CLEM_MEM_FLAG_DATA                  (0x2)
#define CLEM_MEM_FLAG_PROGRAM               (0x1)
#define CLEM_MEM_FLAG_NULL                  (0x0)

#define CLEM_UTIL_set16_lo(_v16_, _v8_) \
    (((_v16_) & 0xff00) | ((_v8_) & 0xff))


#define CLEM_I_PRINTF(_clem_, _fmt_, ...) \
    printf("%02X:%04X " _fmt_ "\n", \
        (_clem_)->cpu.regs.PBR, (_clem_)->cpu. regs.PC, ##__VA_ARGS__)


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
    bool memIdxSelOut;                  // Memory/Index Select
    bool nmiIn;                         // Non-Maskable Interrupt
    bool readyInOut;                    // Ready CPU
    bool resbIn;                        // RESET
    bool vpbOut;                        // Vector Pull
    /*
    bool rwbOut;                        // Read/Write byte
    bool vdaOut;                        // Valid Data Address
    bool vpaOut;                        // Valid Program Address
    bool mlbOut;                        // Memory Lock
    */
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
    uint32_t cycles_spent;
    bool emulation;
    bool intr_brk;
};


typedef struct {
    struct Clemens65C816 cpu;
    uint32_t clocks_step;
    uint32_t clocks_spent;
    uint8_t* fpi_bank_map[256];
} ClemensMachine;


int clemens_init(
    ClemensMachine* machine,
    uint32_t clocks_step,
    void* rom,
    size_t romSize
) {
    machine->clocks_step = clocks_step;
    machine->clocks_spent = 0;
    if (romSize != CLEM_IIGS_ROM3_SIZE) {
        return -1;
    }

    /* memory organization for the FPI */
    machine->fpi_bank_map[0xfc] = (uint8_t*)rom;
    machine->fpi_bank_map[0xfd] = (uint8_t*)rom + CLEM_IIGS_BANK_SIZE;
    machine->fpi_bank_map[0xfe] = (uint8_t*)rom + CLEM_IIGS_BANK_SIZE * 2;
    machine->fpi_bank_map[0xff] = (uint8_t*)rom + CLEM_IIGS_BANK_SIZE * 3;

    /* TODO: clear memory according to spec 0x00, 0xff, etc (look it up) */
    machine->fpi_bank_map[0x00] = (uint8_t*)malloc(CLEM_IIGS_BANK_SIZE);
    memset(machine->fpi_bank_map[0x00], 0, CLEM_IIGS_BANK_SIZE);
    machine->fpi_bank_map[0x01] = (uint8_t*)malloc(CLEM_IIGS_BANK_SIZE);
    memset(machine->fpi_bank_map[0x01], 0, CLEM_IIGS_BANK_SIZE);
    machine->fpi_bank_map[0x02] = (uint8_t*)malloc(CLEM_IIGS_BANK_SIZE);
    memset(machine->fpi_bank_map[0x02], 0, CLEM_IIGS_BANK_SIZE);
    machine->fpi_bank_map[0x03] = (uint8_t*)malloc(CLEM_IIGS_BANK_SIZE);
    memset(machine->fpi_bank_map[0x03], 0, CLEM_IIGS_BANK_SIZE);

    return 0;
}


static inline void _clem_cycle(
    ClemensMachine* clem,
    uint32_t cycle_count
) {
    clem->clocks_spent += clem->clocks_step * cycle_count;
    ++clem->cpu.cycles_spent;
}

/*  Memory Reads and Writes:
    Requirements:
        Handle FPI access to ROM
        Handle FPI and MEGA2 fast and slow accesses to RAM
        Handle Access based on the Shadow Register


*/


static inline void _clem_mem_read(
    ClemensMachine* clem,
    uint8_t* data,
    uint16_t adr,
    uint8_t bank,
    uint8_t flags
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

    clem->clocks_spent += clem->clocks_step;
    ++clem->cpu.cycles_spent;
}

static inline void _clem_mem_write(
    ClemensMachine* clem,
    uint8_t data,
    uint16_t adr,
    uint8_t bank,
    uint8_t flags
) {
    clem->cpu.pins.adr = adr;
    clem->cpu.pins.databank = bank;
    clem->fpi_bank_map[bank][adr] = data;
    clem->clocks_spent += clem->clocks_step;
    ++clem->cpu.cycles_spent;
}

static inline void _cpu_sp_dec3(
    struct Clemens65C816* cpu
) {
    uint16_t tmp = cpu->regs.S - 3;
    if (cpu->emulation) {
        tmp = (cpu->regs.S & 0xff00) | (tmp & 0x00ff);
    }
    cpu->regs.S = tmp;
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

static inline void _cpu_sp_inc3(
    struct Clemens65C816* cpu
) {
    uint16_t tmp = cpu->regs.S + 3;
    if (cpu->emulation) {
        tmp = (cpu->regs.S & 0xff00) | (tmp & 0x00ff);
    }
    cpu->regs.S = tmp;
}

static inline void _cpu_sp_inc2(
    struct Clemens65C816* cpu
) {
    uint16_t tmp = cpu->regs.S + 2;
    if (cpu->emulation) {
        tmp = (cpu->regs.S & 0xff00) | (tmp & 0x00ff);
    }
    cpu->regs.S = tmp;
}

static inline void _cpu_sp_inc(
    struct Clemens65C816* cpu
) {
    uint16_t tmp = cpu->regs.S + 1;
    if (cpu->emulation) {
        tmp = (cpu->regs.S & 0xff00) | (tmp & 0x00ff);
    }
    cpu->regs.S = tmp;
}

void cpu_execute(struct Clemens65C816* cpu, ClemensMachine* clem) {
    uint16_t tmp_addr;
    uint16_t tmp_value;
    uint16_t tmp_pc;
    uint16_t tmp_pc_operand;
    uint8_t tmp_data;
    uint8_t tmp_datahi;
    uint8_t IR;
    uint8_t IR_cc;
    uint8_t IR_aaa;
    uint8_t IR_bbb;
    bool m_status;
    bool x_status;
    assert(cpu->state_type == kClemensCPUStateType_Execute);
    /*
        Execute all cycles of an instruction here
    */
    cpu->pins.vpbOut = true;
    tmp_pc = cpu->regs.PC;
    _clem_mem_read(clem, &cpu->regs.IR, tmp_pc++, cpu->regs.PBR,
                   CLEM_MEM_FLAG_OPCODE_FETCH);
    tmp_pc_operand = tmp_pc;

    /*
        65xxx opcoes seem to follow an 'aaabbbcc' bit pattern
        reference: http://nparker.llx.com/a2/opcodes.html
    */
    IR = cpu->regs.IR;
    IR_aaa = IR & 0xE0;
    IR_bbb = IR & 0x1C;
    IR_cc = IR & 0x03;
    m_status = (cpu->regs.P & kClemensCPUStatus_MemoryAccumulator) != 0;
    x_status = (cpu->regs.P & kClemensCPUStatus_Index) != 0;

    if (IR_cc == 0x01) {
        //  6502 opcodes, IR_bbb indicates an addressing mode
        switch (IR_bbb) {
            case CLEM_ADR_MODE_01_IMMEDIATE:
                _clem_mem_read(clem, &tmp_data, tmp_pc++, cpu->regs.PBR,
                               CLEM_MEM_FLAG_PROGRAM);
                tmp_value = tmp_data;
                if (!m_status) {
                    _clem_mem_read(clem, &tmp_data, tmp_pc++, cpu->regs.PBR,
                                   CLEM_MEM_FLAG_PROGRAM);
                    tmp_value = ((uint16_t)tmp_data << 8) | tmp_value;
                    CLEM_I_PRINTF(clem, "LDA #$%04X", tmp_value);
                } else {
                    CLEM_I_PRINTF(clem, "LDA #$%02X", tmp_value);
                }
                if (IR_aaa == CLEM_OPC_LDA_MODE_01) {
                    if (m_status) {
                        cpu->regs.A = (
                            (cpu->regs.A & 0xff00) | (tmp_value & 0x00ff));
                    } else {
                        cpu->regs.A = tmp_value;
                    }
                }
                break;
        }
    }
    if (tmp_pc == tmp_pc_operand) {
        /*  all operands are one-byte or otherwise unhandled from the above
            shortcut methods of parsing opcodes
        */
        switch (cpu->regs.IR) {
            case CLEM_OPC_TSB_ABS:
                //  Test and Set value in memory against accumulator
                _clem_mem_read(clem, &tmp_data, tmp_pc++, cpu->regs.PBR,
                               CLEM_MEM_FLAG_PROGRAM);
                tmp_addr = tmp_data;
                _clem_mem_read(clem, &tmp_data, tmp_pc++, cpu->regs.PBR,
                               CLEM_MEM_FLAG_PROGRAM);
                tmp_addr = ((uint16_t)tmp_data << 8) | tmp_addr;
                CLEM_I_PRINTF(clem, "TSB $%04X", tmp_addr);
                _clem_mem_read(clem, &tmp_data, tmp_addr, cpu->regs.DBR,
                               CLEM_MEM_FLAG_DATA);
                tmp_value = tmp_data;
                if (!m_status) {
                    _clem_mem_read(clem, &tmp_data, tmp_addr + 1,
                                   cpu->regs.DBR, CLEM_MEM_FLAG_DATA);
                    tmp_value = ((uint16_t)tmp_data << 8) | tmp_value;
                }
                tmp_value |= cpu->regs.A;
                if (!(tmp_value & cpu->regs.A)) {
                    cpu->regs.P |= kClemensCPUStatus_Zero;
                } else {
                    cpu->regs.P &= ~kClemensCPUStatus_Zero;
                }
                _clem_cycle(clem, 1);
                if (!m_status) {
                    _clem_mem_write(clem, tmp_value >> 8, tmp_addr + 1,
                                    cpu->regs.DBR, CLEM_MEM_FLAG_DATA);
                }
                _clem_mem_write(clem, (uint8_t)tmp_value, tmp_addr,
                                cpu->regs.DBR, CLEM_MEM_FLAG_DATA);

                break;
            case CLEM_OPC_TCS:
                CLEM_I_PRINTF(clem, "TSC");
                if (cpu->emulation) {
                    cpu->regs.S = CLEM_UTIL_set16_lo(cpu->regs.S, cpu->regs.A);
                } else {
                    cpu->regs.S = cpu->regs.A;
                }
                _clem_cycle(clem, 1);
                break;
            case CLEM_OPC_SEI:
                CLEM_I_PRINTF(clem, "SEI");
                cpu->regs.P |= kClemensCPUStatus_IRQDisable;
                _clem_cycle(clem, 1);
                break;
            case CLEM_OPC_CLC:
                CLEM_I_PRINTF(clem, "CLC");
                cpu->regs.P &= ~kClemensCPUStatus_Carry;
                _clem_cycle(clem, 1);
                break;
            case CLEM_OPC_SEC:
                CLEM_I_PRINTF(clem, "SEC");
                cpu->regs.P |= kClemensCPUStatus_Carry;
                _clem_cycle(clem, 1);
                break;
            case CLEM_OPC_CLD:
                CLEM_I_PRINTF(clem, "CLD");
                cpu->regs.P &= ~kClemensCPUStatus_Decimal;
                _clem_cycle(clem, 1);
                break;
            case CLEM_OPC_XCE:
                CLEM_I_PRINTF(clem, "XCE");
                tmp_value = cpu->emulation;
                cpu->emulation = (cpu->regs.P & kClemensCPUStatus_Carry) != 0;
                if (tmp_value != cpu->emulation) {
                    cpu->regs.P |= kClemensCPUStatus_Index;
                    cpu->regs.P |= kClemensCPUStatus_MemoryAccumulator;
                    if (tmp_value) {
                        // switch to native, sets M and X to 8-bits (1)
                        // TODO: log internally
                    } else {
                        // switch to emulation, and emulation stack
                        cpu->regs.S = CLEM_UTIL_set16_lo(0x0100, cpu->regs.S);
                    }
                }
                if (tmp_value) {
                    cpu->regs.P |= kClemensCPUStatus_Carry;
                } else {
                    cpu->regs.P &= ~kClemensCPUStatus_Carry;
                }
                _clem_cycle(clem, 1);
                break;
            case CLEM_OPC_JSR:
                // Stack [PCH, PCL]
                _clem_mem_read(clem, &tmp_data, tmp_pc++, cpu->regs.PBR,
                               CLEM_MEM_FLAG_PROGRAM);
                tmp_addr = tmp_data;
                _clem_mem_read(clem, &tmp_data, tmp_pc, cpu->regs.PBR,
                               CLEM_MEM_FLAG_PROGRAM);
                tmp_addr = ((uint16_t)tmp_data << 8) | tmp_addr;
                CLEM_I_PRINTF(clem, "JSR %04X", tmp_addr);
                _clem_cycle(clem, 1);
                //  stack receives last address of operand
                _clem_mem_write(clem, (uint8_t)(tmp_pc >> 8), cpu->regs.S,
                                0x00, CLEM_MEM_FLAG_DATA);
                tmp_value = cpu->regs.S - 1;
                if (cpu->emulation) {
                    tmp_value = CLEM_UTIL_set16_lo(cpu->regs.S, tmp_value);
                }
                _clem_mem_write(clem, (uint8_t)tmp_pc, tmp_value, 0x00,
                                CLEM_MEM_FLAG_DATA);
                tmp_pc = tmp_addr;      // set next PC to the JSR routine
                break;
            case CLEM_OPC_JSL:
                // Stack [PBR, PCH, PCL]
                _clem_mem_read(clem, &tmp_data, tmp_pc++, cpu->regs.PBR,
                               CLEM_MEM_FLAG_PROGRAM);
                tmp_addr = tmp_data;
                _clem_mem_read(clem, &tmp_data, tmp_pc++, cpu->regs.PBR,
                               CLEM_MEM_FLAG_PROGRAM);
                tmp_addr = ((uint16_t)tmp_data << 8) | tmp_addr;
                //  push old PBR
                _clem_mem_write(clem, cpu->regs.PBR, cpu->regs.S, 0x00,
                                CLEM_MEM_FLAG_DATA);
                _clem_cycle(clem, 1);
                //  new PBR
                _clem_mem_read(clem, &tmp_data, tmp_pc, cpu->regs.PBR,
                               CLEM_MEM_FLAG_PROGRAM);
                CLEM_I_PRINTF(clem, "JSL %02X%04X", tmp_data, tmp_addr);
                cpu->regs.PBR = tmp_data;
                //  JSL stack overrun will not wrap to 0x1ff (65816 quirk)
                //  SP will still wrap
                //  tmp_pc will be tha address of the last operand
                _clem_mem_write(clem, (uint8_t)(tmp_pc >> 8), cpu->regs.S - 1,
                                0x00, CLEM_MEM_FLAG_DATA);
                tmp_value = cpu->regs.S - 1;
                _clem_mem_write(clem, (uint8_t)tmp_pc, cpu->regs.S - 2, 0x00,
                                CLEM_MEM_FLAG_DATA);
                _cpu_sp_dec3(cpu);
                tmp_pc = tmp_addr;      // set next PC to the JSL routine
                break;
            case CLEM_OPC_RTS:
                //  Stack [PCH, PCL]
                CLEM_I_PRINTF(clem, "RTS");
                _clem_cycle(clem, 2);
                tmp_value = cpu->regs.S + 1;
                if (cpu->emulation) {
                    tmp_value = CLEM_UTIL_set16_lo(cpu->regs.S, tmp_value);
                }
                _clem_mem_read(clem, &tmp_data, tmp_value, 0x00,
                               CLEM_MEM_FLAG_DATA);
                tmp_addr = tmp_data;
                ++tmp_value;
                if (cpu->emulation) {
                    tmp_value = CLEM_UTIL_set16_lo(cpu->regs.S, tmp_value);
                }
                _clem_mem_read(clem, &tmp_data, tmp_value, 0x00,
                               CLEM_MEM_FLAG_DATA);
                tmp_addr = ((uint16_t)tmp_data << 8) | tmp_addr;
                _clem_cycle(clem, 1);
                _cpu_sp_inc2(cpu);
                tmp_pc = tmp_addr + 1;  //  point to next instruction
                break;
            case CLEM_OPC_RTL:
                CLEM_I_PRINTF(clem, "RTL");
                _clem_cycle(clem, 2);
                //  again, 65816 quirk where RTL will read from over the top
                //  in emulation mode even
                _clem_mem_read(clem, &tmp_data, cpu->regs.S + 1, 0x00,
                               CLEM_MEM_FLAG_DATA);
                tmp_addr = tmp_data;
                _clem_mem_read(clem, &tmp_data, cpu->regs.S + 2, 0x00,
                               CLEM_MEM_FLAG_DATA);
                tmp_addr = ((uint16_t)tmp_data << 8) | tmp_addr;
                _clem_mem_read(clem, &tmp_data, cpu->regs.S + 3, 0x00,
                               CLEM_MEM_FLAG_DATA);
                cpu->regs.PBR = tmp_data;
                _cpu_sp_inc3(cpu);
                tmp_pc = tmp_addr + 1;
                break;
        }
    }

    cpu->regs.PC = tmp_pc;
}

void emulate(ClemensMachine* clem) {
    struct Clemens65C816* cpu = &clem->cpu;

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
            _clem_cycle(clem, 1);
        }
        _clem_cycle(clem, 1);
        return;
    }
    //  RESB high during reset invokes our interrupt microcode
    if (cpu->state_type == kClemensCPUStateType_Reset) {
        uint16_t tmp_addr;
        uint16_t tmp_value;
        uint8_t tmp_data;
        uint8_t tmp_datahi;

        _clem_mem_read(clem, &tmp_data, cpu->regs.S, 0x00, CLEM_MEM_FLAG_NULL);
        tmp_addr = cpu->regs.S - 1;
        if (cpu->emulation) {
            tmp_addr = CLEM_UTIL_set16_lo(cpu->regs.S, tmp_addr);
        }
        _clem_mem_read(clem, &tmp_datahi, tmp_addr, 0x00, CLEM_MEM_FLAG_NULL);
        _cpu_sp_dec2(cpu);
        _clem_mem_read(clem, &tmp_data, cpu->regs.S, 0x00, CLEM_MEM_FLAG_NULL);
        _cpu_sp_dec(cpu);

        // vector pull low signal while the PC is being loaded
        cpu->pins.vpbOut = false;
        _clem_mem_read(clem, &tmp_data, CLEM_65816_RESET_VECTOR_LO_ADDR, 0x00,
                       CLEM_MEM_FLAG_PROGRAM);
        _clem_mem_read(clem, &tmp_datahi, CLEM_65816_RESET_VECTOR_HI_ADDR,
                       0x00, CLEM_MEM_FLAG_NULL);
        cpu->regs.PC = (uint16_t)(tmp_datahi << 8) | tmp_data;

        cpu->state_type = kClemensCPUStateType_Execute;
        return;
    }

    cpu_execute(cpu, clem);
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

    while (machine.cpu.cycles_spent < 256) {
        emulate(&machine);
    }

    return 0;
}
