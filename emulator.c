#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <assert.h>

#define ANSI_COLOR_RED     "\x1b[31m"
#define ANSI_COLOR_GREEN   "\x1b[32m"
#define ANSI_COLOR_YELLOW  "\x1b[33m"
#define ANSI_COLOR_BLUE    "\x1b[34m"
#define ANSI_COLOR_MAGENTA "\x1b[35m"
#define ANSI_COLOR_CYAN    "\x1b[36m"
#define ANSI_COLOR_RESET   "\x1b[0m"


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

#define CLEM_OPC_ADC_IMM                    (0x69)
#define CLEM_OPC_ADC_ABS                    (0x6D)
#define CLEM_OPC_ADC_ABSL                   (0x6F)
#define CLEM_OPC_ADC_DP                     (0x65)
#define CLEM_OPC_ADC_DP_INDIRECT            (0x72)
#define CLEM_OPC_ADC_DP_INDIRECTL           (0x67)
#define CLEM_OPC_ADC_ABS_IDX                (0x7D)
#define CLEM_OPC_ADC_ABSL_IDX               (0x7F)
#define CLEM_OPC_ADC_ABS_IDY                (0x79)
#define CLEM_OPC_ADC_DP_IDX                 (0x75)
#define CLEM_OPC_ADC_DP_IDX_INDIRECT        (0x61)
#define CLEM_OPC_ADC_DP_INDIRECT_IDY        (0x71)
#define CLEM_OPC_ADC_DP_INDIRECTL_IDY       (0x77)
#define CLEM_OPC_ADC_STACK_REL              (0x63)
#define CLEM_OPC_ADC_STACK_REL_INDIRECT_IDY (0x73)
#define CLEM_OPC_BRA                        (0x80)
#define CLEM_OPC_BNE                        (0xD0)
#define CLEM_OPC_CLC                        (0x18)
#define CLEM_OPC_CLD                        (0xD8)
#define CLEM_OPC_DEX                        (0xCA)
#define CLEM_OPC_DEY                        (0x88)
#define CLEM_OPC_INX                        (0xE8)
#define CLEM_OPC_INY                        (0xC8)
#define CLEM_OPC_JSL                        (0x22)
#define CLEM_OPC_JSR                        (0x20)
#define CLEM_OPC_LDA_IMM                    (0xA9)
#define CLEM_OPC_LDA_ABS                    (0xAD)
#define CLEM_OPC_LDA_ABSL                   (0xAF)
#define CLEM_OPC_LDA_DP                     (0xA5)
#define CLEM_OPC_LDA_DP_INDIRECT            (0xB2)
#define CLEM_OPC_LDA_DP_INDIRECTL           (0xA7)
#define CLEM_OPC_LDA_ABS_IDX                (0xBD)
#define CLEM_OPC_LDA_ABSL_IDX               (0xBF)
#define CLEM_OPC_LDA_ABS_IDY                (0xB9)
#define CLEM_OPC_LDA_DP_IDX                 (0xB5)
#define CLEM_OPC_LDA_DP_IDX_INDIRECT        (0xA1)
#define CLEM_OPC_LDA_DP_INDIRECT_IDY        (0xB1)
#define CLEM_OPC_LDA_DP_INDIRECTL_IDY       (0xB7)
#define CLEM_OPC_LDA_STACK_REL              (0xA3)
#define CLEM_OPC_LDA_STACK_REL_INDIRECT_IDY (0xB3)
#define CLEM_OPC_LDY_IMM                    (0xA0)
#define CLEM_OPC_LDX_IMM                    (0xA2)
#define CLEM_OPC_PHA                        (0x48)
#define CLEM_OPC_PHB                        (0x8B)
#define CLEM_OPC_PHD                        (0x0B)
#define CLEM_OPC_PHK                        (0x4B)
#define CLEM_OPC_PHP                        (0x08)
#define CLEM_OPC_PHX                        (0xDA)
#define CLEM_OPC_PHY                        (0x5A)
#define CLEM_OPC_PLA                        (0x68)
#define CLEM_OPC_PLB                        (0xAB)
#define CLEM_OPC_PLD                        (0x2B)
#define CLEM_OPC_PLP                        (0x28)
#define CLEM_OPC_PLX                        (0xFA)
#define CLEM_OPC_PLY                        (0x7A)
#define CLEM_OPC_REP                        (0xC2)
#define CLEM_OPC_RTL                        (0x6B)
#define CLEM_OPC_RTS                        (0x60)
#define CLEM_OPC_SEC                        (0x38)
#define CLEM_OPC_SEI                        (0x78)
#define CLEM_OPC_SEP                        (0xE2)
#define CLEM_OPC_STA_ABS                    (0x8D)
#define CLEM_OPC_STA_ABSL                   (0x8F)
#define CLEM_OPC_STA_DP                     (0x85)
#define CLEM_OPC_STA_DP_INDIRECT            (0x92)
#define CLEM_OPC_STA_DP_INDIRECTL           (0x87)
#define CLEM_OPC_STA_ABS_IDX                (0x9D)
#define CLEM_OPC_STA_ABSL_IDX               (0x9F)
#define CLEM_OPC_STA_ABS_IDY                (0x99)
#define CLEM_OPC_STA_DP_IDX                 (0x95)
#define CLEM_OPC_STA_DP_IDX_INDIRECT        (0x81)
#define CLEM_OPC_STA_DP_INDIRECT_IDY        (0x91)
#define CLEM_OPC_STA_DP_INDIRECTL_IDY       (0x97)
#define CLEM_OPC_STA_STACK_REL              (0x83)
#define CLEM_OPC_STA_STACK_REL_INDIRECT_IDY (0x93)
#define CLEM_OPC_TCS                        (0x1B)
#define CLEM_OPC_TSB_ABS                    (0x0C)
#define CLEM_OPC_XCE                        (0xFB)


/* Attempt to mimic VDA and VPA per memory access */
#define CLEM_MEM_FLAG_OPCODE_FETCH          (0x3)
#define CLEM_MEM_FLAG_DATA                  (0x2)
#define CLEM_MEM_FLAG_PROGRAM               (0x1)
#define CLEM_MEM_FLAG_NULL                  (0x0)

#define CLEM_UTIL_set16_lo(_v16_, _v8_) \
    (((_v16_) & 0xff00) | ((_v8_) & 0xff))

#define CLEM_UTIL_CROSSED_PAGE_BOUNDARY(_adr0_, _adr1_) \
    (((_adr0_) ^ (_adr1_)) & 0xff00)

#define CLEM_I_PRINT_STATS(_clem_) do {\
    struct Clemens65C816* _cpu_ = &(_clem_)->cpu; \
    uint8_t _P_ = _cpu_->regs.P; \
    if (_cpu_->emulation) { \
        printf(ANSI_COLOR_GREEN "Clocks.... Cycles...." ANSI_COLOR_RESET \
            " NV_BDIZC PC=%04X, PBR=%02X, DBR=%02X, S=%04X, D=%04X, " \
            "B=%02X A=%02X, X=%02X, Y=%02X\n", \
            _cpu_->regs.PC, _cpu_->regs.PBR, _cpu_->regs.DBR, _cpu_->regs.S, \
            _cpu_->regs.D, \
            (_cpu_->regs.A & 0xff00) >> 8, _cpu_->regs.A & 0x00ff, \
            _cpu_->regs.X & 0x00ff, _cpu_->regs.Y & 0x00ff); \
        printf(ANSI_COLOR_GREEN "%10.2f %10u" ANSI_COLOR_RESET \
            " %c%c%c%c%c%c%c%c\n", \
            (_clem_)->clocks_spent / (float)(_clem_)->clocks_step, \
            (_cpu_)->cycles_spent, \
            (_P_ & kClemensCPUStatus_Negative) ? '1' : '0', \
            (_P_ & kClemensCPUStatus_Overflow) ? '1' : '0', \
            (_P_ & kClemensCPUStatus_MemoryAccumulator) ? '1' : '0', \
            _cpu_->intr_brk ? '1' : '0', \
            (_P_ & kClemensCPUStatus_Decimal) ? '1' : '0', \
            (_P_ & kClemensCPUStatus_IRQDisable) ? '1' : '0', \
            (_P_ & kClemensCPUStatus_Zero) ? '1' : '0', \
            (_P_ & kClemensCPUStatus_Carry) ? '1' : '0'); \
    } else { \
        printf(ANSI_COLOR_GREEN "Clocks.... Cycles...." ANSI_COLOR_RESET \
            " NVMXDIZC PC=%04X, PBR=%02X, DBR=%02X, S=%04X, D=%04X, " \
            "A=%04X, X=%04X, Y=%04X\n", \
            _cpu_->regs.PC, _cpu_->regs.PBR, _cpu_->regs.DBR, _cpu_->regs.S, \
            _cpu_->regs.D, _cpu_->regs.A, _cpu_->regs.X, _cpu_->regs.Y); \
        printf(ANSI_COLOR_GREEN "%10.2f %10u" ANSI_COLOR_RESET \
            " %c%c%c%c%c%c%c%c\n", \
            (_clem_)->clocks_spent / (float)(_clem_)->clocks_step, \
            (_cpu_)->cycles_spent, \
            (_P_ & kClemensCPUStatus_Negative) ? '1' : '0', \
            (_P_ & kClemensCPUStatus_Overflow) ? '1' : '0', \
            (_P_ & kClemensCPUStatus_MemoryAccumulator) ? '1' : '0', \
            (_P_ & kClemensCPUStatus_Index) ? '1' : '0', \
            (_P_ & kClemensCPUStatus_Decimal) ? '1' : '0', \
            (_P_ & kClemensCPUStatus_IRQDisable) ? '1' : '0', \
            (_P_ & kClemensCPUStatus_Zero) ? '1' : '0', \
            (_P_ & kClemensCPUStatus_Carry) ? '1' : '0'); \
    } \
} while(0)


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

enum ClemensCPUAddrMode {
    kClemensCPUAddrMode_None,
    kClemensCPUAddrMode_Immediate,
    kClemensCPUAddrMode_Absolute,
    kClemensCPUAddrMode_AbsoluteLong,
    kClemensCPUAddrMode_DirectPage,
    kClemensCPUAddrMode_DirectPageIndirect,
    kClemensCPUAddrMode_DirectPageIndirectLong,
    kClemensCPUAddrMode_Absolute_X,
    kClemensCPUAddrMode_AbsoluteLong_X,
    kClemensCPUAddrMode_Absolute_Y,
    kClemensCPUAddrMode_DirectPage_X,
    kClemensCPUAddrMode_DirectPage_X_Indirect,
    kClemensCPUAddrMode_DirectPage_Indirect_Y,
    kClemensCPUAddrMode_DirectPage_IndirectLong_Y,
    kClemensCPUAddrMode_Stack_Relative,
    kClemensCPUAddrMode_Stack_Relative_Indirect_Y,
    kClemensCPUAddrMode_PCRelative,
    kClemensCPUAddrMode_PCRelativeLong
};

struct ClemensOpcodeDesc {
    enum ClemensCPUAddrMode addr_mode;
    char name[4];
};

static struct ClemensOpcodeDesc sOpcodeDescriptions[256];

struct ClemensInstruction {
    struct ClemensOpcodeDesc* desc;
    uint16_t value;
    uint8_t bank;
    bool opc_8;
};

static inline void _opcode_instruction_define(
    struct ClemensInstruction* instr,
    uint8_t opcode,
    uint16_t value,
    bool opc_8
) {
    instr->desc = &sOpcodeDescriptions[opcode];
    instr->bank = 0x00;
    instr->opc_8 = opc_8;
    instr->value = value;
}

static inline void _opcode_instruction_define_simple(
    struct ClemensInstruction* instr,
    uint8_t opcode
) {
    instr->desc = &sOpcodeDescriptions[opcode];
    instr->bank = 0x00;
    instr->opc_8 = false;
    instr->value = 0x0000;
}

static inline void _opcode_instruction_define_long(
    struct ClemensInstruction* instr,
    uint8_t opcode,
    uint8_t bank,
    uint16_t addr
) {
    instr->desc = &sOpcodeDescriptions[opcode];
    instr->bank = bank;
    instr->opc_8 = false;
    instr->value = addr;
}

static inline void _opcode_instruction_define_dp(
    struct ClemensInstruction* instr,
    uint8_t opcode,
    uint8_t offset
) {
    instr->desc = &sOpcodeDescriptions[opcode];
    instr->bank = 0x00;
    instr->opc_8 = false;
    instr->value = offset;
}

static void _opcode_description(
    uint8_t opcode,
    const char* name,
    enum ClemensCPUAddrMode addr_mode
) {
    strncpy(sOpcodeDescriptions[opcode].name, name, 3);
    sOpcodeDescriptions[opcode].name[3] = '\0';
    sOpcodeDescriptions[opcode].addr_mode = addr_mode;
}

static void _opcode_print(
    struct ClemensInstruction* inst,
    uint8_t pbr,
    uint16_t addr
) {
    printf(ANSI_COLOR_BLUE "%02X:%04X "
           ANSI_COLOR_CYAN "%s",
           pbr, addr, inst->desc->name);
    switch (inst->desc->addr_mode) {
        case kClemensCPUAddrMode_Immediate:
            if (inst->opc_8) {
                printf(ANSI_COLOR_YELLOW " #$%02X", (uint8_t)inst->value);
            } else {
                printf(ANSI_COLOR_YELLOW " #$%04X", inst->value);
            }
            break;
        case kClemensCPUAddrMode_Absolute:
            printf(ANSI_COLOR_YELLOW " $%04X", inst->value);
            break;
        case kClemensCPUAddrMode_AbsoluteLong:
            printf(ANSI_COLOR_YELLOW " $%02X%04X", inst->bank, inst->value);
            break;
        case kClemensCPUAddrMode_Absolute_X:
            printf(ANSI_COLOR_YELLOW " $%04X, X", inst->value);
            break;
        case kClemensCPUAddrMode_Absolute_Y:
            printf(ANSI_COLOR_YELLOW, " $%04X, Y", inst->value);
            break;
        case kClemensCPUAddrMode_AbsoluteLong_X:
            printf(ANSI_COLOR_YELLOW " $%02X%04X, X", inst->bank, inst->value);
            break;
        case kClemensCPUAddrMode_DirectPage:
            printf(ANSI_COLOR_YELLOW " $%02X", inst->value);
            break;
        case kClemensCPUAddrMode_DirectPage_X:
            printf(ANSI_COLOR_YELLOW " $%02X, X", inst->value);
            break;
        case kClemensCPUAddrMode_DirectPageIndirect:
            printf(ANSI_COLOR_YELLOW " ($%02X)", inst->value);
            break;
        case kClemensCPUAddrMode_DirectPageIndirectLong:
            printf(ANSI_COLOR_YELLOW " [$%02X]", inst->value);
            break;
        case kClemensCPUAddrMode_DirectPage_X_Indirect:
            printf(ANSI_COLOR_YELLOW " ($%02, X)", inst->value);
            break;
        case kClemensCPUAddrMode_DirectPage_Indirect_Y:
            printf(ANSI_COLOR_YELLOW " ($%02X), Y", inst->value);
            break;
        case kClemensCPUAddrMode_DirectPage_IndirectLong_Y:
            printf(ANSI_COLOR_YELLOW " [$%02X], Y", inst->value);
            break;
        case kClemensCPUAddrMode_PCRelative:
            printf(ANSI_COLOR_YELLOW " $%02X (%d)", inst->value, (int8_t)inst->value);
            break;
    }
    printf(ANSI_COLOR_RESET "\n");
}


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
    bool irqIn;                         // Interrupt Request
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
    uint32_t clocks_step_mega2;     /* typically FPI speed mhz * clocks_step */
    uint32_t clocks_spent;
    uint8_t* fpi_bank_map[256];     /* $00 - $ff */
    uint8_t* mega2_bank_map[2];     /* $e0 - $e1 */
} ClemensMachine;


int clemens_init(
    ClemensMachine* machine,
    uint32_t speed_factor,
    uint32_t clocks_step,
    void* rom,
    size_t romSize
) {
    machine->clocks_step = clocks_step;
    machine->clocks_step_mega2 = speed_factor;
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

    machine->mega2_bank_map[0x00] = (uint8_t*)malloc(CLEM_IIGS_BANK_SIZE);
    memset(machine->fpi_bank_map[0x00], 0, CLEM_IIGS_BANK_SIZE);
    machine->mega2_bank_map[0x01] = (uint8_t*)malloc(CLEM_IIGS_BANK_SIZE);
    memset(machine->mega2_bank_map[0x01], 0, CLEM_IIGS_BANK_SIZE);

    for (unsigned i = 0; i < 256; ++i) {
        _opcode_description((uint8_t)i, "...", kClemensCPUAddrMode_None);
    }
    _opcode_description(CLEM_OPC_ADC_IMM, "ADC", kClemensCPUAddrMode_Immediate);
    _opcode_description(CLEM_OPC_ADC_ABS, "ADC", kClemensCPUAddrMode_Absolute);
    _opcode_description(CLEM_OPC_ADC_ABSL, "ADC",
                        kClemensCPUAddrMode_AbsoluteLong);
    _opcode_description(CLEM_OPC_ADC_DP,  "ADC",
                        kClemensCPUAddrMode_DirectPage);
    _opcode_description(CLEM_OPC_ADC_DP_INDIRECT,  "ADC",
                        kClemensCPUAddrMode_DirectPageIndirect);
    _opcode_description(CLEM_OPC_ADC_DP_INDIRECTL,  "ADC",
                        kClemensCPUAddrMode_DirectPageIndirectLong);
    _opcode_description(CLEM_OPC_ADC_ABS_IDX,  "ADC",
                        kClemensCPUAddrMode_Absolute_X);
    _opcode_description(CLEM_OPC_ADC_ABSL_IDX,  "ADC",
                        kClemensCPUAddrMode_AbsoluteLong_X);
    _opcode_description(CLEM_OPC_ADC_ABS_IDY,  "ADC",
                        kClemensCPUAddrMode_Absolute_Y);
    _opcode_description(CLEM_OPC_ADC_DP_IDX,  "ADC",
                        kClemensCPUAddrMode_DirectPage_X);
    _opcode_description(CLEM_OPC_ADC_DP_IDX_INDIRECT,  "ADC",
                        kClemensCPUAddrMode_DirectPage_X_Indirect);
    _opcode_description(CLEM_OPC_ADC_DP_INDIRECT_IDY,  "ADC",
                        kClemensCPUAddrMode_DirectPage_Indirect_Y);
    _opcode_description(CLEM_OPC_ADC_DP_INDIRECTL_IDY,  "ADC",
                        kClemensCPUAddrMode_DirectPage_IndirectLong_Y);
    _opcode_description(CLEM_OPC_ADC_STACK_REL,  "ADC",
                        kClemensCPUAddrMode_Stack_Relative);
    _opcode_description(CLEM_OPC_ADC_STACK_REL_INDIRECT_IDY,  "ADC",
                        kClemensCPUAddrMode_Stack_Relative_Indirect_Y);

    _opcode_description(CLEM_OPC_BRA,     "BRA", kClemensCPUAddrMode_PCRelative);
    _opcode_description(CLEM_OPC_BNE,     "BNE", kClemensCPUAddrMode_PCRelative);

    _opcode_description(CLEM_OPC_CLC,     "CLC", kClemensCPUAddrMode_None);
    _opcode_description(CLEM_OPC_CLD,     "CLD", kClemensCPUAddrMode_None);
    _opcode_description(CLEM_OPC_DEX,     "DEX", kClemensCPUAddrMode_None);
    _opcode_description(CLEM_OPC_DEY,     "DEY", kClemensCPUAddrMode_None);
    _opcode_description(CLEM_OPC_INX,     "INX", kClemensCPUAddrMode_None);
    _opcode_description(CLEM_OPC_INY,     "INY", kClemensCPUAddrMode_None);
    _opcode_description(CLEM_OPC_JSL,     "JSL", kClemensCPUAddrMode_AbsoluteLong);
    _opcode_description(CLEM_OPC_JSR,     "JSR", kClemensCPUAddrMode_Absolute);

    _opcode_description(CLEM_OPC_LDA_IMM, "LDA", kClemensCPUAddrMode_Immediate);
    _opcode_description(CLEM_OPC_LDA_ABS, "LDA", kClemensCPUAddrMode_Absolute);
    _opcode_description(CLEM_OPC_LDA_DP,  "LDA",
                        kClemensCPUAddrMode_DirectPage);
    _opcode_description(CLEM_OPC_LDA_DP_INDIRECT,  "LDA",
                        kClemensCPUAddrMode_DirectPageIndirect);
    _opcode_description(CLEM_OPC_LDA_DP_INDIRECTL,  "LDA",
                        kClemensCPUAddrMode_DirectPageIndirectLong);
    _opcode_description(CLEM_OPC_LDA_ABS_IDX,  "LDA",
                        kClemensCPUAddrMode_Absolute_X);
    _opcode_description(CLEM_OPC_LDA_ABSL_IDX,  "LDA",
                        kClemensCPUAddrMode_AbsoluteLong_X);
    _opcode_description(CLEM_OPC_LDA_ABS_IDY,  "LDA",
                        kClemensCPUAddrMode_Absolute_Y);
    _opcode_description(CLEM_OPC_LDA_DP_IDX,  "LDA",
                        kClemensCPUAddrMode_DirectPage_X);
    _opcode_description(CLEM_OPC_LDA_DP_IDX_INDIRECT,  "LDA",
                        kClemensCPUAddrMode_DirectPage_X_Indirect);
    _opcode_description(CLEM_OPC_LDA_DP_INDIRECT_IDY,  "LDA",
                        kClemensCPUAddrMode_DirectPage_Indirect_Y);
    _opcode_description(CLEM_OPC_LDA_DP_INDIRECTL_IDY,  "LDA",
                        kClemensCPUAddrMode_DirectPage_IndirectLong_Y);
    _opcode_description(CLEM_OPC_LDA_STACK_REL,  "LDA",
                        kClemensCPUAddrMode_Stack_Relative);
    _opcode_description(CLEM_OPC_LDA_STACK_REL_INDIRECT_IDY,  "LDA",
                        kClemensCPUAddrMode_Stack_Relative_Indirect_Y);
    _opcode_description(CLEM_OPC_LDA_ABSL, "LDA", kClemensCPUAddrMode_AbsoluteLong);

    _opcode_description(CLEM_OPC_LDX_IMM, "LDX", kClemensCPUAddrMode_Immediate);
    _opcode_description(CLEM_OPC_LDY_IMM, "LDY", kClemensCPUAddrMode_Immediate);
    _opcode_description(CLEM_OPC_PHA,     "PHA", kClemensCPUAddrMode_None);
    _opcode_description(CLEM_OPC_PHB,     "PHB", kClemensCPUAddrMode_None);
    _opcode_description(CLEM_OPC_PHD,     "PHD", kClemensCPUAddrMode_None);
    _opcode_description(CLEM_OPC_PHK,     "PHK", kClemensCPUAddrMode_None);
    _opcode_description(CLEM_OPC_PHP,     "PHP", kClemensCPUAddrMode_None);
    _opcode_description(CLEM_OPC_PHX,     "PHX", kClemensCPUAddrMode_None);
    _opcode_description(CLEM_OPC_PHY,     "PHY", kClemensCPUAddrMode_None);
    _opcode_description(CLEM_OPC_PLA,     "PLA", kClemensCPUAddrMode_None);
    _opcode_description(CLEM_OPC_PLB,     "PLB", kClemensCPUAddrMode_None);
    _opcode_description(CLEM_OPC_PLD,     "PLD", kClemensCPUAddrMode_None);
    _opcode_description(CLEM_OPC_PLP,     "PLP", kClemensCPUAddrMode_None);
    _opcode_description(CLEM_OPC_PLX,     "PLX", kClemensCPUAddrMode_None);
    _opcode_description(CLEM_OPC_PLY,     "PLY", kClemensCPUAddrMode_None);
    _opcode_description(CLEM_OPC_REP,     "REP", kClemensCPUAddrMode_Immediate);
    _opcode_description(CLEM_OPC_RTL,     "RTL", kClemensCPUAddrMode_None);
    _opcode_description(CLEM_OPC_RTS,     "RTS", kClemensCPUAddrMode_None);
    _opcode_description(CLEM_OPC_SEC,     "SCE", kClemensCPUAddrMode_None);
    _opcode_description(CLEM_OPC_SEI,     "SEI", kClemensCPUAddrMode_None);
    _opcode_description(CLEM_OPC_SEP,     "SEP", kClemensCPUAddrMode_Immediate);

    _opcode_description(CLEM_OPC_STA_ABS, "STA", kClemensCPUAddrMode_Absolute);
    _opcode_description(CLEM_OPC_STA_ABSL, "STA", kClemensCPUAddrMode_AbsoluteLong);
    _opcode_description(CLEM_OPC_STA_DP,  "STA",
                        kClemensCPUAddrMode_DirectPage);
    _opcode_description(CLEM_OPC_STA_DP_INDIRECT,  "STA",
                        kClemensCPUAddrMode_DirectPageIndirect);
    _opcode_description(CLEM_OPC_STA_DP_INDIRECTL,  "STA",
                        kClemensCPUAddrMode_DirectPageIndirectLong);
    _opcode_description(CLEM_OPC_STA_ABS_IDX,  "STA",
                        kClemensCPUAddrMode_Absolute_X);
    _opcode_description(CLEM_OPC_STA_ABSL_IDX,  "STA",
                        kClemensCPUAddrMode_AbsoluteLong_X);
    _opcode_description(CLEM_OPC_STA_ABS_IDY,  "STA",
                        kClemensCPUAddrMode_Absolute_Y);
    _opcode_description(CLEM_OPC_STA_DP_IDX,  "STA",
                        kClemensCPUAddrMode_DirectPage_X);
    _opcode_description(CLEM_OPC_STA_DP_IDX_INDIRECT,  "STA",
                        kClemensCPUAddrMode_DirectPage_X_Indirect);
    _opcode_description(CLEM_OPC_STA_DP_INDIRECT_IDY,  "STA",
                        kClemensCPUAddrMode_DirectPage_Indirect_Y);
    _opcode_description(CLEM_OPC_STA_DP_INDIRECTL_IDY,  "STA",
                        kClemensCPUAddrMode_DirectPage_IndirectLong_Y);
    _opcode_description(CLEM_OPC_STA_STACK_REL,  "STA",
                        kClemensCPUAddrMode_Stack_Relative);
    _opcode_description(CLEM_OPC_STA_STACK_REL_INDIRECT_IDY,  "STA",
                        kClemensCPUAddrMode_Stack_Relative_Indirect_Y);

    _opcode_description(CLEM_OPC_TSB_ABS, "TSB", kClemensCPUAddrMode_Absolute);
    _opcode_description(CLEM_OPC_TCS,     "TCS", kClemensCPUAddrMode_None);
    _opcode_description(CLEM_OPC_XCE,     "XCE", kClemensCPUAddrMode_None);

    return 0;
}


static inline void _cpu_p_flags_n_z_data(
    struct Clemens65C816* cpu,
    uint8_t data
) {
    if (data & 0x80) {
        cpu->regs.P |= kClemensCPUStatus_Negative;
    } else {
        cpu->regs.P &= ~kClemensCPUStatus_Negative;
    }
    if (data) {
        cpu->regs.P &= ~kClemensCPUStatus_Zero;
    } else {
        cpu->regs.P |= kClemensCPUStatus_Zero;
    }
}

static inline void _cpu_p_flags_n_z_data_16(
    struct Clemens65C816* cpu,
    uint16_t data
) {
    if (data & 0x8000) {
        cpu->regs.P |= kClemensCPUStatus_Negative;
    } else {
        cpu->regs.P &= ~kClemensCPUStatus_Negative;
    }
    if (data) {
        cpu->regs.P &= ~kClemensCPUStatus_Zero;
    } else {
        cpu->regs.P |= kClemensCPUStatus_Zero;
    }
}

static inline void _cpu_p_flags_n_z_data_816(
    struct Clemens65C816* cpu,
    uint16_t data,
    bool is8
) {
    if (is8) {
        _cpu_p_flags_n_z_data(cpu, (uint8_t)data);
    } else {
        _cpu_p_flags_n_z_data_16(cpu, data);
    }
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

static inline void _cpu_adc(
    struct Clemens65C816* cpu,
    uint16_t value,
    bool is8
) {
    uint32_t adc;
    uint8_t p = cpu->regs.P;
    bool carry = (cpu->regs.P & kClemensCPUStatus_Carry) != 0;
    if (is8) {
        adc = (cpu->regs.A & 0xff) + value + carry;
        if (!(adc & 0xff)) p |= kClemensCPUStatus_Zero;
        if (adc & 0x80) p |= kClemensCPUStatus_Negative;
        if (((cpu->regs.A & 0xff) ^ adc) & (value ^ adc) & 0x80) {
            p |= kClemensCPUStatus_Overflow;
        }
        if (adc & 0x100) p |= kClemensCPUStatus_Carry;
        cpu->regs.A = CLEM_UTIL_set16_lo(cpu->regs.A, (uint8_t)adc);
    } else {
        adc = cpu->regs.A + value + carry;
        if (!(adc & 0xffff)) p |= kClemensCPUStatus_Zero;
        if (adc & 0x8000) p |= kClemensCPUStatus_Negative;
        if ((cpu->regs.A ^ adc) & (value ^ adc) & 0x8000) {
            p |= kClemensCPUStatus_Overflow;
        }
        if (adc & 0x10000) p |= kClemensCPUStatus_Carry;
        cpu->regs.A = (uint16_t)adc;
    }
}

static inline void _cpu_lda(
    struct Clemens65C816* cpu,
    uint16_t value,
    bool is8
) {
    if (is8) {
        _cpu_p_flags_n_z_data(cpu, (uint8_t)value);
        cpu->regs.A = CLEM_UTIL_set16_lo(cpu->regs.A, value);
    } else {
        _cpu_p_flags_n_z_data_16(cpu, value);
        cpu->regs.A = value;
    }
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
static inline void _clem_read(
    ClemensMachine* clem,
    uint8_t* data,
    uint16_t adr,
    uint8_t bank,
    uint8_t flags
) {
    clem->cpu.pins.adr = adr;
    clem->cpu.pins.databank = bank;
    if (bank == 0x00) {
        if (adr >= 0xd000) {
            *data = clem->fpi_bank_map[0xff][adr];
        } else {
            *data = clem->fpi_bank_map[0x00][adr];
        }
    } else if (bank == 0xe0 || bank == 0xe1) {
        *data = clem->mega2_bank_map[bank & 0x1][adr];
    } else {
        *data = clem->fpi_bank_map[bank][adr];
    }
    // TODO: account for slow/fast memory access
    clem->clocks_spent += clem->clocks_step;
    ++clem->cpu.cycles_spent;
}

static inline void _clem_read_16(
    ClemensMachine* clem,
    uint16_t* data16,
    uint16_t adr,
    uint8_t bank,
    uint8_t flags
) {
    //  TODO: DATA read should wrap to next DBR
    uint8_t tmp_data;
    _clem_read(clem, &tmp_data, adr, bank, flags);
    *data16 = tmp_data;
    _clem_read(clem, &tmp_data, adr + 1, bank, flags);
    *data16 = ((uint16_t)tmp_data << 8) | (*data16);
}

static inline void _clem_read_pba(
    ClemensMachine* clem,
    uint8_t* data,
    uint16_t* pc
) {
    _clem_read(clem, data, (*pc)++, clem->cpu.regs.PBR,
                   CLEM_MEM_FLAG_PROGRAM);
}

static inline void _clem_read_pba_16(
    ClemensMachine* clem,
    uint16_t* data16,
    uint16_t* pc
) {
    uint8_t tmp_data;
    _clem_read_pba(clem, &tmp_data, pc);
    *data16 = tmp_data;
    _clem_read_pba(clem, &tmp_data, pc);
    *data16 = ((uint16_t)tmp_data << 8) | (*data16);
}

static inline void _clem_read_pba_816(
    ClemensMachine* clem,
    uint16_t* out,
    uint16_t* pc,
    bool is8
) {
    uint8_t tmp_data;
    _clem_read_pba(clem, &tmp_data, pc);
    *out = tmp_data;
    if (!is8) {
       _clem_read_pba(clem, &tmp_data, pc);
        *out = ((uint16_t)tmp_data << 8) | *out;
    }
}

static inline void _clem_read_pba_dp_addr(
    ClemensMachine* clem,
    uint16_t* eff_addr,
    uint16_t* pc,
    uint8_t* offset,
    uint16_t index
) {
    uint16_t D = clem->cpu.regs.D;
    uint16_t offset_index = index;
    _clem_read_pba(clem, offset, pc);
    offset_index += *offset;
    if (clem->cpu.emulation) {
        *eff_addr = (D & 0xff00) + ((D & 0xff) + offset_index) % 256;
    } else {
        *eff_addr = D + *offset + index;
    }
    if (D & 0x00ff) {
        _clem_cycle(clem, 1);
    }
}

static inline void _clem_next_dbr(
    ClemensMachine* clem,
    uint8_t* next_dbr,
    uint8_t dbr
) {
    if (!clem->cpu.emulation) {
        *next_dbr = dbr + 1;
    } else {
        *next_dbr = dbr;
    }
}

static inline void _clem_read_data_816(
    ClemensMachine* clem,
    uint16_t* out,
    uint16_t addr,
    uint8_t dbr,
    bool is8
) {
    uint8_t tmp_data;
    _clem_read(
        clem, &tmp_data, addr, dbr, CLEM_MEM_FLAG_DATA);
    *out = tmp_data;
    if (!is8) {
        uint8_t next_dbr;
        ++addr;
        if (!addr) {
            _clem_next_dbr(clem, &next_dbr, dbr);
        } else {
            next_dbr = dbr;
        }
        _clem_read(
            clem, &tmp_data, addr, next_dbr, CLEM_MEM_FLAG_DATA);
        *out = ((uint16_t)tmp_data << 8) | *out;
    }
}

static inline void _clem_read_data_indexed_816(
    ClemensMachine* clem,
    uint16_t* out,
    uint16_t addr,
    uint16_t index,
    uint8_t dbr,
    bool is_data_8,
    bool is_index_8
) {
    uint8_t dbr_actual;
    uint16_t eff_addr = addr + index;
    if (eff_addr < addr && !clem->cpu.emulation) {
        _clem_next_dbr(clem, &dbr_actual, dbr);
    } else {
        dbr_actual = dbr;
    }
    if (!is_index_8 || CLEM_UTIL_CROSSED_PAGE_BOUNDARY(addr, eff_addr)) {
        //  indexed address crossing a page boundary adds a cycle
        _clem_cycle(clem, 1);
    }
    _clem_read_data_816(clem, out, addr + index, dbr_actual, is_data_8);
}


static inline void _clem_read_addr_dp_indirect(
    ClemensMachine* clem,
    uint16_t* out_addr,
    uint16_t addr
) {
    _clem_read_16(clem, out_addr, addr, 0x00, CLEM_MEM_FLAG_DATA);
}

static inline void _clem_read_addr_dp_indirect_long(
    ClemensMachine* clem,
    uint16_t* out_addr,
    uint8_t* out_dbr,
    uint16_t addr
) {
    // TODO index?
    _clem_read_16(clem, out_addr, addr, 0x00, CLEM_MEM_FLAG_DATA);
    //  TODO: direct page wrap? (DH, DL=255 + 1 = DH, 0)?
    _clem_read(clem, out_dbr, addr + 1, 0x00, CLEM_MEM_FLAG_DATA);
}

static inline void _clem_write(
    ClemensMachine* clem,
    uint8_t data,
    uint16_t adr,
    uint8_t bank
) {
    clem->cpu.pins.adr = adr;
    clem->cpu.pins.databank = bank;
    if (bank == 0xe0 || bank == 0xe1) {
        clem->mega2_bank_map[bank & 0x1][adr] = data;
    } else {
        clem->fpi_bank_map[bank][adr] = data;
    }
    // TODO: account for slow/fast memory access
    clem->clocks_spent += clem->clocks_step;
    ++clem->cpu.cycles_spent;
}

static inline void _clem_write_16(
    ClemensMachine* clem,
    uint16_t data,
    uint16_t adr,
    uint8_t bank
) {
    _clem_write(clem, (uint8_t)data, adr, bank);
    _clem_write(clem, (uint8_t)(data >> 8), adr + 1, bank);
}

static inline void _clem_opc_push_reg_816(
    ClemensMachine* clem,
    uint16_t data,
    bool is8
) {
    struct Clemens65C816* cpu = &clem->cpu;
    _clem_cycle(clem, 1);
    if (!is8) {
        _clem_write(clem, (uint8_t)(data >> 8), cpu->regs.S, 0x00);
        _cpu_sp_dec(cpu);
    }
    _clem_write(clem, (uint8_t)(data), cpu->regs.S, 0x00);
    _cpu_sp_dec(cpu);
}

static inline void _clem_opc_pull_reg_816(
    ClemensMachine* clem,
    uint16_t* data,
    bool is8
) {
    struct Clemens65C816* cpu = &clem->cpu;
    uint8_t data8;
    _clem_cycle(clem, 2);
    _cpu_sp_inc(cpu);
    _clem_read(clem, &data8, cpu->regs.S, 0x00, CLEM_MEM_FLAG_DATA);
    *data = CLEM_UTIL_set16_lo(*data, data8);
    if (!is8) {
        _cpu_sp_inc(cpu);
        _clem_read(clem, &data8, cpu->regs.S, 0x00, CLEM_MEM_FLAG_DATA);
        *data = CLEM_UTIL_set16_lo((uint16_t)(data8) << 8, (uint8_t)(*data));
    }
}

static inline void _clem_opc_pull_reg_8(
    ClemensMachine* clem,
    uint8_t* data
) {
    struct Clemens65C816* cpu = &clem->cpu;
    _clem_cycle(clem, 2);
    _cpu_sp_inc(cpu);
    _clem_read(clem, data, cpu->regs.S, 0x00, CLEM_MEM_FLAG_DATA);
}


void cpu_execute(struct Clemens65C816* cpu, ClemensMachine* clem) {
    uint16_t tmp_addr;
    uint16_t tmp_eaddr;
    uint16_t tmp_value;
    uint16_t tmp_pc;
    uint8_t tmp_data;
    uint8_t tmp_dbr;
    uint8_t IR;

    struct ClemensInstruction opc_inst;
    uint16_t opc_addr;
    uint8_t opc_pbr;

    uint8_t carry;
    bool m_status;
    bool x_status;
    bool zero_flag;
    bool overflow_flag;

    assert(cpu->state_type == kClemensCPUStateType_Execute);
    /*
        Execute all cycles of an instruction here
    */
    cpu->pins.vpbOut = true;
    tmp_pc = cpu->regs.PC;
    opc_pbr = cpu->regs.PBR;
    opc_addr = tmp_pc;

    //  TODO: Okay, we enter native mode but PBR is still 0x00 though we are
    //        reading code from ROM.  research what to do during the switch to
    //        native mode!   do the I/O memory registers still tell us to read
    //        from ROM though we are at PBR bank 0x00?  Or should PBR change to
    //        0xff?
    _clem_read(clem, &cpu->regs.IR, tmp_pc++, cpu->regs.PBR,
                   CLEM_MEM_FLAG_OPCODE_FETCH);
    IR = cpu->regs.IR;
    //  This define may be overwritten by a non simple instruction
    _opcode_instruction_define_simple(&opc_inst, IR);

    m_status = (cpu->regs.P & kClemensCPUStatus_MemoryAccumulator) != 0;
    x_status = (cpu->regs.P & kClemensCPUStatus_Index) != 0;
    carry = (cpu->regs.P & kClemensCPUStatus_Carry) != 0;
    zero_flag = (cpu->regs.P & kClemensCPUStatus_Zero) != 0;
    overflow_flag = (cpu->regs.P & kClemensCPUStatus_Overflow) != 0;

    switch (IR) {
        //
        // Start ADC
        case CLEM_OPC_ADC_IMM:
            _clem_read_pba_816(clem, &tmp_value, &tmp_pc, m_status);
            _cpu_adc(cpu, tmp_value, m_status);
            _opcode_instruction_define(&opc_inst, IR, tmp_value, m_status);
            break;
        case CLEM_OPC_ADC_ABS:
            _clem_read_pba_16(clem, &tmp_addr, &tmp_pc);
            _clem_read_data_816(clem, &tmp_value, tmp_addr, cpu->regs.DBR, m_status);
            _cpu_adc(cpu, tmp_value, m_status);
            _opcode_instruction_define(&opc_inst, IR, tmp_addr, m_status);
            break;
        case CLEM_OPC_ADC_ABSL:
            //  TODO: emulation mode
            _clem_read_pba_16(clem, &tmp_addr, &tmp_pc);
            _clem_read_pba(clem, &tmp_dbr, &tmp_pc);
            _clem_read_data_816(clem, &tmp_value, tmp_addr, tmp_dbr, m_status);
            _cpu_adc(cpu, tmp_value, m_status);
            _opcode_instruction_define_long(&opc_inst, IR, tmp_dbr, tmp_addr);
            break;
        case CLEM_OPC_ADC_DP:
            _clem_read_pba_dp_addr(clem, &tmp_addr, &tmp_pc, &tmp_data, 0);
            _clem_read_data_816(clem, &tmp_value, tmp_addr, 0x00, m_status);
            _cpu_adc(cpu, tmp_value, m_status);
            _opcode_instruction_define_dp(&opc_inst, IR, tmp_data);
            break;
        case CLEM_OPC_ADC_DP_INDIRECT:
            _clem_read_pba_dp_addr(clem, &tmp_addr, &tmp_pc, &tmp_data, 0);
            _clem_read_addr_dp_indirect(clem, &tmp_eaddr, tmp_addr);
            _clem_read_data_816(clem, &tmp_value, tmp_eaddr, cpu->regs.DBR, m_status);
            _cpu_adc(cpu, tmp_value, m_status);
            _opcode_instruction_define_dp(&opc_inst, IR, tmp_data);
            break;
        case CLEM_OPC_ADC_DP_INDIRECTL:
            _clem_read_pba_dp_addr(clem, &tmp_addr, &tmp_pc, &tmp_data, 0);
            _clem_read_addr_dp_indirect_long(clem, &tmp_eaddr, &tmp_dbr, tmp_addr);
            _clem_read_data_816(clem, &tmp_value, tmp_eaddr, tmp_dbr, m_status);
            _cpu_adc(cpu, tmp_value, m_status);
            _opcode_instruction_define_dp(&opc_inst, IR, tmp_data);
            break;
        case CLEM_OPC_ADC_ABS_IDX:
            _clem_read_pba_16(clem, &tmp_addr, &tmp_pc);
            tmp_eaddr = x_status ? (cpu->regs.X & 0xff) : cpu->regs.X;
            _clem_read_data_indexed_816(
                clem, &tmp_value, tmp_addr, tmp_eaddr, cpu->regs.DBR, m_status, x_status);
            _cpu_adc(cpu, tmp_value, m_status);
            _opcode_instruction_define(&opc_inst, IR, tmp_addr, m_status);
            break;
        case CLEM_OPC_ADC_ABSL_IDX:
            _clem_read_pba_16(clem, &tmp_addr, &tmp_pc);
            _clem_read_pba(clem, &tmp_dbr, &tmp_pc);
            tmp_eaddr = x_status ? (cpu->regs.X & 0xff) : cpu->regs.X;
            _clem_read_data_indexed_816(
                clem, &tmp_value, tmp_addr, tmp_eaddr, tmp_dbr, m_status, x_status);
            _cpu_adc(cpu, tmp_value, m_status);
            _opcode_instruction_define_long(&opc_inst, IR, tmp_dbr, tmp_addr);
            break;
        case CLEM_OPC_ADC_ABS_IDY:      // $addr + Y
            _clem_read_pba_16(clem, &tmp_addr, &tmp_pc);
            tmp_eaddr = x_status ? (cpu->regs.Y & 0xff) : cpu->regs.Y;
            _clem_read_data_indexed_816(
                clem, &tmp_value, tmp_addr, tmp_eaddr, cpu->regs.DBR, m_status, x_status);
            _cpu_adc(cpu, tmp_value, m_status);
            _opcode_instruction_define(&opc_inst, IR, tmp_addr, m_status);
            break;
        case CLEM_OPC_ADC_DP_IDX:
            _clem_read_pba_dp_addr(clem, &tmp_addr, &tmp_pc, &tmp_data,
                 x_status ? (cpu->regs.X & 0xff) : cpu->regs.X);
            _clem_cycle(clem, 1);             // extra IO cycle for d,x
            _clem_read_data_816(clem, &tmp_value, tmp_addr, cpu->regs.DBR, m_status);
            _cpu_adc(cpu, tmp_value, m_status);
            _opcode_instruction_define_dp(&opc_inst, IR, tmp_data);
            break;
        case CLEM_OPC_ADC_DP_IDX_INDIRECT:
            _clem_read_pba_dp_addr(clem, &tmp_addr, &tmp_pc, &tmp_data,
                x_status ? (cpu->regs.X & 0xff) : cpu->regs.X);
            _clem_cycle(clem, 1);       // extra IO for (d, X)
            _clem_read_addr_dp_indirect(clem, &tmp_eaddr, tmp_addr);
            _clem_read_data_816(clem, &tmp_value, tmp_eaddr, cpu->regs.DBR, m_status);
            _cpu_adc(cpu, tmp_value, m_status);
            _opcode_instruction_define_dp(&opc_inst, IR, tmp_data);
            break;
        case CLEM_OPC_ADC_DP_INDIRECT_IDY:
            _clem_read_pba_dp_addr(clem, &tmp_addr, &tmp_pc, &tmp_data, 0);
            _clem_read_addr_dp_indirect(clem, &tmp_eaddr, tmp_addr);
            _clem_read_data_indexed_816(
                clem, &tmp_value, tmp_eaddr, cpu->regs.Y, cpu->regs.DBR, m_status, x_status);
            _cpu_adc(cpu, tmp_value, m_status);
            _opcode_instruction_define_dp(&opc_inst, IR, tmp_data);
            break;
        case CLEM_OPC_ADC_STACK_REL:
            _clem_read_pba(clem, &tmp_data, &tmp_pc);
            _clem_cycle(clem, 1);   //  extra IO
            _clem_read_data_816(
                clem, &tmp_value, cpu->regs.S + tmp_data, 0x00, m_status);
            _cpu_adc(cpu, tmp_value, m_status);
            _opcode_instruction_define(&opc_inst, IR, tmp_data, m_status);
            break;
        case CLEM_OPC_ADC_STACK_REL_INDIRECT_IDY:
            _clem_read_pba(clem, &tmp_data, &tmp_pc);
            _clem_cycle(clem, 1);   //  extra IO
            _clem_read_16(clem, &tmp_addr, cpu->regs.S + tmp_data, 0x00, CLEM_MEM_FLAG_DATA);
            _clem_cycle(clem, 1);   //  extra IO
            _clem_read_data_indexed_816(
                clem, &tmp_value, tmp_addr, cpu->regs.Y, cpu->regs.DBR, m_status, x_status);
            _cpu_adc(cpu, tmp_value, m_status);
            _opcode_instruction_define(&opc_inst, IR, tmp_data, m_status);
            break;
        //  End ADC
        //
        case CLEM_OPC_BRA:
            _clem_read_pba(clem, &tmp_data, &tmp_pc);
            tmp_addr = tmp_pc + (int8_t)tmp_data;
            if (cpu->emulation &&
                CLEM_UTIL_CROSSED_PAGE_BOUNDARY(tmp_pc, tmp_addr)) {
                _clem_cycle(clem, 1);
            }
            _clem_cycle(clem, 1);       // branch always taken cycle
            tmp_pc = tmp_addr;
            _opcode_instruction_define(&opc_inst, IR, tmp_data, false);
            break;
        case CLEM_OPC_BNE:
            _clem_read_pba(clem, &tmp_data, &tmp_pc);
            tmp_addr = tmp_pc + (int8_t)tmp_data;
            if (!zero_flag) {
                if (cpu->emulation &&
                    CLEM_UTIL_CROSSED_PAGE_BOUNDARY(tmp_pc, tmp_addr)) {
                    _clem_cycle(clem, 1);
                }
                _clem_cycle(clem, 1);
                tmp_pc = tmp_addr;
            }
            _opcode_instruction_define(&opc_inst, IR, tmp_data, false);
            break;
        case CLEM_OPC_CLC:
            cpu->regs.P &= ~kClemensCPUStatus_Carry;
            _clem_cycle(clem, 1);
            break;
        case CLEM_OPC_CLD:
            cpu->regs.P &= ~kClemensCPUStatus_Decimal;
            _clem_cycle(clem, 1);
            break;
        case CLEM_OPC_DEX:
            tmp_value = cpu->regs.X - 1;
            if (x_status) {
                cpu->regs.X = CLEM_UTIL_set16_lo(cpu->regs.X, (uint8_t)tmp_value);
                _cpu_p_flags_n_z_data(cpu, (uint8_t)tmp_value);
            } else {
                cpu->regs.X = tmp_value;
                _cpu_p_flags_n_z_data_16(cpu, tmp_value);
            }
            _opcode_instruction_define_simple(&opc_inst, IR);
            break;
        case CLEM_OPC_DEY:
            tmp_value = cpu->regs.Y - 1;
            if (x_status) {
                cpu->regs.Y = CLEM_UTIL_set16_lo(cpu->regs.Y, (uint8_t)tmp_value);
                _cpu_p_flags_n_z_data(cpu, (uint8_t)tmp_value);
            } else {
                cpu->regs.Y = tmp_value;
                _cpu_p_flags_n_z_data_16(cpu, tmp_value);
            }
            _opcode_instruction_define_simple(&opc_inst, IR);
            break;
        case CLEM_OPC_INX:
            tmp_value = cpu->regs.X + 1;
            if (x_status) {
                cpu->regs.X = CLEM_UTIL_set16_lo(cpu->regs.X, (uint8_t)tmp_value);
                _cpu_p_flags_n_z_data(cpu, (uint8_t)tmp_value);
            } else {
                cpu->regs.X = tmp_value;
                _cpu_p_flags_n_z_data_16(cpu, tmp_value);
            }
            _opcode_instruction_define_simple(&opc_inst, IR);
            break;
        case CLEM_OPC_INY:
            tmp_value = cpu->regs.Y + 1;
            if (x_status) {
                cpu->regs.Y = CLEM_UTIL_set16_lo(cpu->regs.Y, (uint8_t)tmp_value);
                _cpu_p_flags_n_z_data(cpu, (uint8_t)tmp_value);
            } else {
                cpu->regs.Y = tmp_value;
                _cpu_p_flags_n_z_data_16(cpu, tmp_value);
            }
            _opcode_instruction_define_simple(&opc_inst, IR);
            break;
        //
        //  Start LDA
        case CLEM_OPC_LDA_IMM:
            _clem_read_pba_816(clem, &tmp_value, &tmp_pc, m_status);
            _cpu_lda(cpu, tmp_value, m_status);
            _opcode_instruction_define(&opc_inst, IR, tmp_value, m_status);
            break;
        case CLEM_OPC_LDA_ABS:
            _clem_read_pba_16(clem, &tmp_addr, &tmp_pc);
            _clem_read_data_816(clem, &tmp_value, tmp_addr, cpu->regs.DBR, m_status);
            _cpu_lda(cpu, tmp_value, m_status);
            _opcode_instruction_define(&opc_inst, IR, tmp_addr, m_status);
            break;
        case CLEM_OPC_LDA_ABSL:
            //  TODO: emulation mode
            _clem_read_pba_16(clem, &tmp_addr, &tmp_pc);
            _clem_read_pba(clem, &tmp_dbr, &tmp_pc);
            _clem_read_data_816(clem, &tmp_value, tmp_addr, tmp_dbr, m_status);
            _cpu_lda(cpu, tmp_value, m_status);
            _opcode_instruction_define_long(&opc_inst, IR, tmp_dbr, tmp_addr);
            break;
        case CLEM_OPC_LDA_DP:
            _clem_read_pba_dp_addr(clem, &tmp_addr, &tmp_pc, &tmp_data, 0);
            _clem_read_data_816(clem, &tmp_value, tmp_addr, 0x00, m_status);
            _cpu_lda(cpu, tmp_value, m_status);
            _opcode_instruction_define_dp(&opc_inst, IR, tmp_data);
            break;
        case CLEM_OPC_LDA_DP_INDIRECT:
            _clem_read_pba_dp_addr(clem, &tmp_addr, &tmp_pc, &tmp_data, 0);
            _clem_read_addr_dp_indirect(clem, &tmp_eaddr, tmp_addr);
            _clem_read_data_816(clem, &tmp_value, tmp_eaddr, cpu->regs.DBR, m_status);
            _cpu_lda(cpu, tmp_value, m_status);
            _opcode_instruction_define_dp(&opc_inst, IR, tmp_data);
            break;
        case CLEM_OPC_LDA_DP_INDIRECTL:
            _clem_read_pba_dp_addr(clem, &tmp_addr, &tmp_pc, &tmp_data, 0);
            _clem_read_addr_dp_indirect_long(clem, &tmp_eaddr, &tmp_dbr, tmp_addr);
            _clem_read_data_816(clem, &tmp_value, tmp_eaddr, tmp_dbr, m_status);
            _cpu_lda(cpu, tmp_value, m_status);
            _opcode_instruction_define_dp(&opc_inst, IR, tmp_data);
            break;
        case CLEM_OPC_LDA_ABS_IDX:
            _clem_read_pba_16(clem, &tmp_addr, &tmp_pc);
            tmp_eaddr = x_status ? (cpu->regs.X & 0xff) : cpu->regs.X;
            _clem_read_data_indexed_816(
                clem, &tmp_value, tmp_addr, tmp_eaddr, cpu->regs.DBR, m_status, x_status);
            _cpu_lda(cpu, tmp_value, m_status);
            _opcode_instruction_define(&opc_inst, IR, tmp_addr, m_status);
            break;
        case CLEM_OPC_LDA_ABSL_IDX:
            _clem_read_pba_16(clem, &tmp_addr, &tmp_pc);
            _clem_read_pba(clem, &tmp_dbr, &tmp_pc);
            tmp_eaddr = x_status ? (cpu->regs.X & 0xff) : cpu->regs.X;
            _clem_read_data_indexed_816(
                clem, &tmp_value, tmp_addr, tmp_eaddr, tmp_dbr, m_status, x_status);
            _cpu_lda(cpu, tmp_value, m_status);
            _opcode_instruction_define_long(&opc_inst, IR, tmp_dbr, tmp_addr);
            break;
        case CLEM_OPC_LDA_ABS_IDY:      // $addr + Y
            _clem_read_pba_16(clem, &tmp_addr, &tmp_pc);
            tmp_eaddr = x_status ? (cpu->regs.Y & 0xff) : cpu->regs.Y;
            _clem_read_data_indexed_816(
                clem, &tmp_value, tmp_addr, tmp_eaddr, cpu->regs.DBR, m_status, x_status);
            _cpu_lda(cpu, tmp_value, m_status);
            _opcode_instruction_define(&opc_inst, IR, tmp_addr, m_status);
            break;
        case CLEM_OPC_LDA_DP_IDX:
            _clem_read_pba_dp_addr(clem, &tmp_addr, &tmp_pc, &tmp_data,
                 x_status ? (cpu->regs.X & 0xff) : cpu->regs.X);
            _clem_cycle(clem, 1);             // extra IO cycle for d,x
            _clem_read_data_816(clem, &tmp_value, tmp_addr, cpu->regs.DBR, m_status);
            _cpu_lda(cpu, tmp_value, m_status);
            _opcode_instruction_define_dp(&opc_inst, IR, tmp_data);
            break;
        case CLEM_OPC_LDA_DP_IDX_INDIRECT:
            _clem_read_pba_dp_addr(clem, &tmp_addr, &tmp_pc, &tmp_data,
                x_status ? (cpu->regs.X & 0xff) : cpu->regs.X);
            _clem_cycle(clem, 1);       // extra IO for (d, X)
            _clem_read_addr_dp_indirect(clem, &tmp_eaddr, tmp_addr);
            _clem_read_data_816(clem, &tmp_value, tmp_eaddr, cpu->regs.DBR, m_status);
            _cpu_lda(cpu, tmp_value, m_status);
            _opcode_instruction_define_dp(&opc_inst, IR, tmp_data);
            break;
        case CLEM_OPC_LDA_DP_INDIRECT_IDY:
            _clem_read_pba_dp_addr(clem, &tmp_addr, &tmp_pc, &tmp_data, 0);
            _clem_read_addr_dp_indirect(clem, &tmp_eaddr, tmp_addr);
            _clem_read_data_indexed_816(
                clem, &tmp_value, tmp_eaddr, cpu->regs.Y, cpu->regs.DBR, m_status, x_status);
            _cpu_lda(cpu, tmp_value, m_status);
            _opcode_instruction_define_dp(&opc_inst, IR, tmp_data);
            break;
        case CLEM_OPC_LDA_STACK_REL:
            _clem_read_pba(clem, &tmp_data, &tmp_pc);
            _clem_cycle(clem, 1);   //  extra IO
            _clem_read_data_816(
                clem, &tmp_value, cpu->regs.S + tmp_data, 0x00, m_status);
            _cpu_lda(cpu, tmp_value, m_status);
            _opcode_instruction_define(&opc_inst, IR, tmp_data, m_status);
            break;
        case CLEM_OPC_LDA_STACK_REL_INDIRECT_IDY:
            _clem_read_pba(clem, &tmp_data, &tmp_pc);
            _clem_cycle(clem, 1);   //  extra IO
            _clem_read_16(clem, &tmp_addr, cpu->regs.S + tmp_data, 0x00, CLEM_MEM_FLAG_DATA);
            _clem_cycle(clem, 1);   //  extra IO
            _clem_read_data_indexed_816(
                clem, &tmp_value, tmp_addr, cpu->regs.Y, cpu->regs.DBR, m_status, x_status);
            _cpu_lda(cpu, tmp_value, m_status);
            _opcode_instruction_define(&opc_inst, IR, tmp_data, m_status);
            break;
        //  End LDA
        //
        case CLEM_OPC_LDY_IMM:
            _clem_read_pba_816(clem, &tmp_value, &tmp_pc, x_status);
            if (x_status) {
                cpu->regs.Y = CLEM_UTIL_set16_lo(cpu->regs.Y, tmp_value);
            } else {
                cpu->regs.Y = tmp_value;
            }
            _opcode_instruction_define(&opc_inst, IR, tmp_value, x_status);
            break;
        case CLEM_OPC_LDX_IMM:
            _clem_read_pba_816(clem, &tmp_value, &tmp_pc, x_status);
            if (x_status) {
                cpu->regs.X = CLEM_UTIL_set16_lo(cpu->regs.X, tmp_value);
            } else {
                cpu->regs.X = tmp_value;
            }
            _opcode_instruction_define(&opc_inst, IR, tmp_value, x_status);
            break;
        case CLEM_OPC_PHA:
            _clem_opc_push_reg_816(clem, cpu->regs.A, m_status);
            _opcode_instruction_define_simple(&opc_inst, IR);
            break;
        case CLEM_OPC_PHB:
            _clem_cycle(clem, 1);
            _clem_write(clem, (uint8_t)cpu->regs.DBR, cpu->regs.S, 0x00);
            _cpu_sp_dec(cpu);
            _opcode_instruction_define_simple(&opc_inst, IR);
            break;
        case CLEM_OPC_PHD:
            _clem_cycle(clem, 1);
            //  65816 quirk - PHD can overrun the valid stack range
            _clem_write(
                clem, (uint8_t)(cpu->regs.A >> 8), cpu->regs.S, 0x00);
            _clem_write(
                clem, (uint8_t)(cpu->regs.A), cpu->regs.S - 1, 0x00);
            _cpu_sp_dec2(cpu);
            _opcode_instruction_define_simple(&opc_inst, IR);
            break;
        case CLEM_OPC_PHK:
            _clem_cycle(clem, 1);
            _clem_write(clem, (uint8_t)cpu->regs.PBR, cpu->regs.S, 0x00);
            _cpu_sp_dec(cpu);
            _opcode_instruction_define_simple(&opc_inst, IR);
            break;
        case CLEM_OPC_PHP:
            _clem_cycle(clem, 1);
            _clem_write(clem, cpu->regs.P, cpu->regs.S, 0x00);
            _cpu_sp_dec(cpu);
            break;
        case CLEM_OPC_PHX:
            _clem_opc_push_reg_816(clem, cpu->regs.X, x_status);
            _opcode_instruction_define_simple(&opc_inst, IR);
            break;
        case CLEM_OPC_PHY:
            _clem_opc_push_reg_816(clem, cpu->regs.Y, x_status);
            _opcode_instruction_define_simple(&opc_inst, IR);
            break;
        case CLEM_OPC_PLA:
            _clem_opc_pull_reg_816(clem, &cpu->regs.A, m_status);
            _cpu_p_flags_n_z_data_816(cpu, cpu->regs.A, m_status);
            _opcode_instruction_define_simple(&opc_inst, IR);
            break;
        case CLEM_OPC_PLB:
            _clem_opc_pull_reg_8(clem, &cpu->regs.DBR);
            _cpu_p_flags_n_z_data(cpu, cpu->regs.DBR);
            _opcode_instruction_define_simple(&opc_inst, IR);
            break;
        case CLEM_OPC_PLD:
            _clem_cycle(clem, 2);
            //  65816 quirk - PHD can overrun the valid stack range
            _clem_read_16(clem, &cpu->regs.D, cpu->regs.S, 0x00,
                              CLEM_MEM_FLAG_DATA);
            _cpu_p_flags_n_z_data_16(cpu, cpu->regs.D);
            _cpu_sp_inc2(cpu);
            _opcode_instruction_define_simple(&opc_inst, IR);
            break;
        case CLEM_OPC_PLP:
            _clem_opc_pull_reg_8(clem, &cpu->regs.P);
            _opcode_instruction_define_simple(&opc_inst, IR);
            break;
        case CLEM_OPC_PLX:
            _clem_opc_push_reg_816(clem, cpu->regs.X, x_status);
            _cpu_p_flags_n_z_data_816(cpu, cpu->regs.X, x_status);
            _opcode_instruction_define_simple(&opc_inst, IR);
            break;
        case CLEM_OPC_PLY:
            _clem_opc_push_reg_816(clem, cpu->regs.Y, x_status);
            _cpu_p_flags_n_z_data_816(cpu, cpu->regs.Y, x_status);
            _opcode_instruction_define_simple(&opc_inst, IR);
            break;
        case CLEM_OPC_REP:
            // Reset Status Bits
            _clem_read_pba(clem, &tmp_data, &tmp_pc);
            if (cpu->emulation) {
                tmp_data &= ~kClemensCPUStatus_MemoryAccumulator;
                tmp_data &= ~kClemensCPUStatus_Index;
            }
            cpu->regs.P &= (~tmp_data); // all 1 bits are turned OFF in P
            _clem_cycle(clem, 1);
            _opcode_instruction_define(&opc_inst, IR, tmp_data, false);
            break;
        case CLEM_OPC_SEC:
            cpu->regs.P |= kClemensCPUStatus_Carry;
            _clem_cycle(clem, 1);
            break;
        case CLEM_OPC_SEI:
            cpu->regs.P |= kClemensCPUStatus_IRQDisable;
            _clem_cycle(clem, 1);
            break;
        case CLEM_OPC_SEP:
            // Reset Status Bits
            _clem_read_pba(clem, &tmp_data, &tmp_pc);
            if (cpu->emulation) {
                tmp_data &= ~kClemensCPUStatus_MemoryAccumulator;
                tmp_data &= ~kClemensCPUStatus_Index;
            }
            cpu->regs.P |= tmp_data;    // all 1 bits are turned ON in P
            _clem_cycle(clem, 1);
            _opcode_instruction_define(&opc_inst, IR, tmp_data, false);
            break;
        //
        //  Start STA
        case CLEM_OPC_STA_ABS:
            _clem_read_pba_16(clem, &tmp_addr, &tmp_pc);
            tmp_data = cpu->regs.DBR;
            if (m_status) {
                _clem_write(clem, (uint8_t)cpu->regs.A, tmp_addr,
                                tmp_data);
            } else {
                _clem_write_16(clem, cpu->regs.A, tmp_addr, tmp_data);
            }
            _opcode_instruction_define(&opc_inst, IR, tmp_addr, m_status);
            break;
        case CLEM_OPC_STA_ABSL:
            //  absolute long read
            //  TODO: what about emulation mode?
            _clem_read_pba_16(clem, &tmp_addr, &tmp_pc);
            _clem_read_pba(clem, &tmp_data, &tmp_pc);
            if (m_status) {
                _clem_write(clem, (uint8_t)cpu->regs.A, tmp_addr,
                                tmp_data);
            } else {
                _clem_write_16(clem, cpu->regs.A, tmp_addr, tmp_data);
            }
            _opcode_instruction_define_long(&opc_inst, IR, tmp_data, tmp_addr);
            break;
        case CLEM_OPC_STA_DP:
            _clem_read_pba_dp_addr(clem, &tmp_addr, &tmp_pc, &tmp_data, 0);
            _cpu_sta(cpu, cpu->regs.DBR, tmp_addr, m_status);
            _opcode_instruction_define_dp(&opc_inst, IR, tmp_data);
            break;
        case CLEM_OPC_STA_DP_INDIRECT:
            _clem_read_pba_dp_addr(clem, &tmp_addr, &tmp_pc, &tmp_data, 0);
            _clem_read_addr_dp_indirect(clem, &tmp_eaddr, tmp_addr);
            _cpu_sta(cpu, cpu->regs.DBR, tmp_eaddr, m_status);
            _opcode_instruction_define_dp(&opc_inst, IR, tmp_data);
            break;
        case CLEM_OPC_LDA_DP_INDIRECTL:
            _clem_read_pba_dp_addr(clem, &tmp_addr, &tmp_pc, &tmp_data, 0);
            _clem_read_addr_dp_indirect_long(clem, &tmp_eaddr, &tmp_dbr, tmp_addr);
            _clem_read_data_816(clem, &tmp_value, tmp_eaddr, tmp_dbr, m_status);
            _cpu_lda(cpu, tmp_value, m_status);
            _opcode_instruction_define_dp(&opc_inst, IR, tmp_data);
            break;
        case CLEM_OPC_LDA_ABS_IDX:
            _clem_read_pba_16(clem, &tmp_addr, &tmp_pc);
            tmp_eaddr = x_status ? (cpu->regs.X & 0xff) : cpu->regs.X;
            _clem_read_data_indexed_816(
                clem, &tmp_value, tmp_addr, tmp_eaddr, cpu->regs.DBR, m_status, x_status);
            _cpu_lda(cpu, tmp_value, m_status);
            _opcode_instruction_define(&opc_inst, IR, tmp_addr, m_status);
            break;
        case CLEM_OPC_LDA_ABSL_IDX:
            _clem_read_pba_16(clem, &tmp_addr, &tmp_pc);
            _clem_read_pba(clem, &tmp_dbr, &tmp_pc);
            tmp_eaddr = x_status ? (cpu->regs.X & 0xff) : cpu->regs.X;
            _clem_read_data_indexed_816(
                clem, &tmp_value, tmp_addr, tmp_eaddr, tmp_dbr, m_status, x_status);
            _cpu_lda(cpu, tmp_value, m_status);
            _opcode_instruction_define_long(&opc_inst, IR, tmp_dbr, tmp_addr);
            break;
        case CLEM_OPC_LDA_ABS_IDY:      // $addr + Y
            _clem_read_pba_16(clem, &tmp_addr, &tmp_pc);
            tmp_eaddr = x_status ? (cpu->regs.Y & 0xff) : cpu->regs.Y;
            _clem_read_data_indexed_816(
                clem, &tmp_value, tmp_addr, tmp_eaddr, cpu->regs.DBR, m_status, x_status);
            _cpu_lda(cpu, tmp_value, m_status);
            _opcode_instruction_define(&opc_inst, IR, tmp_addr, m_status);
            break;
        case CLEM_OPC_LDA_DP_IDX:
            _clem_read_pba_dp_addr(clem, &tmp_addr, &tmp_pc, &tmp_data,
                 x_status ? (cpu->regs.X & 0xff) : cpu->regs.X);
            _clem_cycle(clem, 1);             // extra IO cycle for d,x
            _clem_read_data_816(clem, &tmp_value, tmp_addr, cpu->regs.DBR, m_status);
            _cpu_lda(cpu, tmp_value, m_status);
            _opcode_instruction_define_dp(&opc_inst, IR, tmp_data);
            break;
        case CLEM_OPC_LDA_DP_IDX_INDIRECT:
            _clem_read_pba_dp_addr(clem, &tmp_addr, &tmp_pc, &tmp_data,
                x_status ? (cpu->regs.X & 0xff) : cpu->regs.X);
            _clem_cycle(clem, 1);       // extra IO for (d, X)
            _clem_read_addr_dp_indirect(clem, &tmp_eaddr, tmp_addr);
            _clem_read_data_816(clem, &tmp_value, tmp_eaddr, cpu->regs.DBR, m_status);
            _cpu_lda(cpu, tmp_value, m_status);
            _opcode_instruction_define_dp(&opc_inst, IR, tmp_data);
            break;
        case CLEM_OPC_LDA_DP_INDIRECT_IDY:
            _clem_read_pba_dp_addr(clem, &tmp_addr, &tmp_pc, &tmp_data, 0);
            _clem_read_addr_dp_indirect(clem, &tmp_eaddr, tmp_addr);
            _clem_read_data_indexed_816(
                clem, &tmp_value, tmp_eaddr, cpu->regs.Y, cpu->regs.DBR, m_status, x_status);
            _cpu_lda(cpu, tmp_value, m_status);
            _opcode_instruction_define_dp(&opc_inst, IR, tmp_data);
            break;
        case CLEM_OPC_LDA_STACK_REL:
            _clem_read_pba(clem, &tmp_data, &tmp_pc);
            _clem_cycle(clem, 1);   //  extra IO
            _clem_read_data_816(
                clem, &tmp_value, cpu->regs.S + tmp_data, 0x00, m_status);
            _cpu_lda(cpu, tmp_value, m_status);
            _opcode_instruction_define(&opc_inst, IR, tmp_data, m_status);
            break;
        case CLEM_OPC_LDA_STACK_REL_INDIRECT_IDY:
            _clem_read_pba(clem, &tmp_data, &tmp_pc);
            _clem_cycle(clem, 1);   //  extra IO
            _clem_read_16(clem, &tmp_addr, cpu->regs.S + tmp_data, 0x00, CLEM_MEM_FLAG_DATA);
            _clem_cycle(clem, 1);   //  extra IO
            _clem_read_data_indexed_816(
                clem, &tmp_value, tmp_addr, cpu->regs.Y, cpu->regs.DBR, m_status, x_status);
            _cpu_lda(cpu, tmp_value, m_status);
            _opcode_instruction_define(&opc_inst, IR, tmp_data, m_status);
            break;
        //  End STA
        //
        case CLEM_OPC_TCS:
            if (cpu->emulation) {
                cpu->regs.S = CLEM_UTIL_set16_lo(cpu->regs.S, cpu->regs.A);
            } else {
                cpu->regs.S = cpu->regs.A;
            }
            _clem_cycle(clem, 1);
            break;
        case CLEM_OPC_TSB_ABS:
            //  Test and Set value in memory against accumulator
            _clem_read_pba_16(clem, &tmp_addr, &tmp_pc);
            _clem_read(clem, &tmp_data, tmp_addr, cpu->regs.DBR,
                            CLEM_MEM_FLAG_DATA);
            tmp_value = tmp_data;
            if (!m_status) {
                _clem_read(clem, &tmp_data, tmp_addr + 1,
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
                _clem_write(clem, tmp_value >> 8, tmp_addr + 1,
                                cpu->regs.DBR);
            }
            _clem_write(clem, (uint8_t)tmp_value, tmp_addr,
                            cpu->regs.DBR);
            _opcode_instruction_define(&opc_inst, IR, tmp_addr, m_status);
            break;
        case CLEM_OPC_XCE:
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
            _clem_read_pba_16(clem, &tmp_addr, &tmp_pc);
            --tmp_pc;       // point to last byte in operand
            _clem_cycle(clem, 1);
            //  stack receives last address of operand
            _clem_write(clem, (uint8_t)(tmp_pc >> 8), cpu->regs.S,
                            0x00);
            tmp_value = cpu->regs.S - 1;
            if (cpu->emulation) {
                tmp_value = CLEM_UTIL_set16_lo(cpu->regs.S, tmp_value);
            }
            _clem_write(clem, (uint8_t)tmp_pc, tmp_value, 0x00);
            _opcode_instruction_define(&opc_inst, IR, tmp_addr, false);
            _cpu_sp_dec2(cpu);
            tmp_pc = tmp_addr;      // set next PC to the JSR routine
            break;
        case CLEM_OPC_RTS:
            //  Stack [PCH, PCL]
            _clem_cycle(clem, 2);
            tmp_value = cpu->regs.S + 1;
            if (cpu->emulation) {
                tmp_value = CLEM_UTIL_set16_lo(cpu->regs.S, tmp_value);
            }
            _clem_read(clem, &tmp_data, tmp_value, 0x00,
                            CLEM_MEM_FLAG_DATA);
            tmp_addr = tmp_data;
            ++tmp_value;
            if (cpu->emulation) {
                tmp_value = CLEM_UTIL_set16_lo(cpu->regs.S, tmp_value);
            }
            _clem_read(clem, &tmp_data, tmp_value, 0x00,
                            CLEM_MEM_FLAG_DATA);
            tmp_addr = ((uint16_t)tmp_data << 8) | tmp_addr;
            _clem_cycle(clem, 1);
            _cpu_sp_inc2(cpu);
            tmp_pc = tmp_addr + 1;  //  point to next instruction
            break;
        case CLEM_OPC_JSL:
            // Stack [PBR, PCH, PCL]
            _clem_read_pba_16(clem, &tmp_addr, &tmp_pc);
            //  push old PBR
            _clem_write(clem, cpu->regs.PBR, cpu->regs.S, 0x00);
            _clem_cycle(clem, 1);
            //  new PBR
            _clem_read_pba(clem, &tmp_data, &tmp_pc);
            --tmp_pc;        // point to last byte in operand
            cpu->regs.PBR = tmp_data;
            //  JSL stack overrun will not wrap to 0x1ff (65816 quirk)
            //  SP will still wrap
            //  tmp_pc will be tha address of the last operand
            _clem_write(clem, (uint8_t)(tmp_pc >> 8), cpu->regs.S - 1,
                            0x00);
            tmp_value = cpu->regs.S - 1;
            _clem_write(clem, (uint8_t)tmp_pc, cpu->regs.S - 2, 0x00);
            _cpu_sp_dec3(cpu);
            _opcode_instruction_define_long(&opc_inst, IR, cpu->regs.PBR,
                                            tmp_addr);
            tmp_pc = tmp_addr;      // set next PC to the JSL routine
            break;
        case CLEM_OPC_RTL:
            _clem_cycle(clem, 2);
            //  again, 65816 quirk where RTL will read from over the top
            //  in emulation mode even
            _clem_read(clem, &tmp_data, cpu->regs.S + 1, 0x00,
                            CLEM_MEM_FLAG_DATA);
            tmp_addr = tmp_data;
            _clem_read(clem, &tmp_data, cpu->regs.S + 2, 0x00,
                            CLEM_MEM_FLAG_DATA);
            tmp_addr = ((uint16_t)tmp_data << 8) | tmp_addr;
            _clem_read(clem, &tmp_data, cpu->regs.S + 3, 0x00,
                            CLEM_MEM_FLAG_DATA);
            cpu->regs.PBR = tmp_data;
            _cpu_sp_inc3(cpu);
            tmp_pc = tmp_addr + 1;
            break;
        default:
            printf("Unknown IR = %x\n", IR);
            assert(false);
            break;
    }

    _opcode_print(&opc_inst, opc_pbr, opc_addr);

    cpu->regs.PC = tmp_pc;
}

void emulate(ClemensMachine* clem) {
    struct Clemens65C816* cpu = &clem->cpu;

    CLEM_I_PRINT_STATS(clem);

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
                kClemensCPUStatus_IRQDisable);
            cpu->intr_brk = false;
            cpu->emulation = true;
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

        _clem_read(clem, &tmp_data, cpu->regs.S, 0x00, CLEM_MEM_FLAG_NULL);
        tmp_addr = cpu->regs.S - 1;
        if (cpu->emulation) {
            tmp_addr = CLEM_UTIL_set16_lo(cpu->regs.S, tmp_addr);
        }
        _clem_read(clem, &tmp_datahi, tmp_addr, 0x00, CLEM_MEM_FLAG_NULL);
        _cpu_sp_dec2(cpu);
        _clem_read(clem, &tmp_data, cpu->regs.S, 0x00, CLEM_MEM_FLAG_NULL);
        _cpu_sp_dec(cpu);

        // vector pull low signal while the PC is being loaded
        cpu->pins.vpbOut = false;
        _clem_read(clem, &tmp_data, CLEM_65816_RESET_VECTOR_LO_ADDR, 0x00,
                       CLEM_MEM_FLAG_PROGRAM);
        _clem_read(clem, &tmp_datahi, CLEM_65816_RESET_VECTOR_HI_ADDR,
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
    //FILE* fp = fopen("gs_rom_3.rom", "rb");
    FILE* fp = fopen("testrom.rom", "rb");
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
    clemens_init(&machine, 1000, 1000, rom, 256 * 1024);

    machine.cpu.pins.resbIn = false;
    emulate(&machine);
    machine.cpu.pins.resbIn = true;

    while (machine.cpu.cycles_spent < 256) {
        emulate(&machine);
    }

    return 0;
}
