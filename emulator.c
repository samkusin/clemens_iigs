#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <assert.h>

#include "clem_mmio_defs.h"

#include "emulator.h"

#include "clem_code.h"
#include "clem_util.h"
#include "clem_device.h"


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

#define CLEM_I_PRINT_STATS(_clem_) do {\
    struct Clemens65C816* _cpu_ = &(_clem_)->cpu; \
    uint8_t _P_ = _cpu_->regs.P; \
    if (_cpu_->pins.emulation) { \
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
            '-', \
            '-', \
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

#if CLEM_COMPILE_DEFINE_JMP_LOGGING
#define CLEM_CPU_I_JSR_LOG(_clem_cpu_, _adr_) do { \
    fprintf(stderr, "%02X:%04X: JSR $%04X\n", \
        (_clem_cpu_)->regs.PBR, (_clem_cpu_)->regs.PC, _adr_); \
    fflush(stderr); \
} while(0)

#define CLEM_CPU_I_JSL_LOG(_clem_cpu_, _adr_, _bank_) do { \
    fprintf(stderr, "%02X:%04X: JSL $%02X%04X\n", \
        (_clem_cpu_)->regs.PBR, (_clem_cpu_)->regs.PC, _bank_, _adr_); \
    fflush(stderr); \
} while(0)

#define CLEM_CPU_I_RTS_LOG(_clem_cpu_, _adr_) do { \
    fprintf(stderr, "%02X:%04X: RTS (%04X)\n", \
        (_clem_cpu_)->regs.PBR, (_clem_cpu_)->regs.PC, _adr_); \
    fflush(stderr); \
} while(0)

#define CLEM_CPU_I_RTL_LOG(_clem_cpu_, _adr_, _bank_) do { \
    fprintf(stderr, "%02X:%04X: RTL (%02X%04X)\n", \
        (_clem_cpu_)->regs.PBR, (_clem_cpu_)->regs.PC, _bank_, _adr_); \
    fflush(stderr); \
} while(0)

#define CLEM_CPU_I_INTR_LOG(_clem_cpu_, _name_) do { \
    fprintf(stderr, "%02X:%04X: INTR %s\n", \
        (_clem_cpu_)->regs.PBR, (_clem_cpu_)->regs.PC, _name_); \
    fflush(stderr); \
} while(0)

#define CLEM_CPU_I_RTI_LOG(_clem_cpu_, _adr_, _bank_) do { \
    if ((_clem_cpu_)->pins.emulation) { \
        fprintf(stderr, "%02X:%04X: RTI (%04X)\n", \
            (_clem_cpu_)->regs.PBR, (_clem_cpu_)->regs.PC, _adr_); \
    } else { \
        fprintf(stderr, "%02X:%04X: RTI (%02X%04X)\n", \
            (_clem_cpu_)->regs.PBR, (_clem_cpu_)->regs.PC, _bank_, _adr_); \
    } \
    fflush(stderr); \
} while(0)
#else
#define CLEM_CPU_I_JSR_LOG(_clem_cpu_, _adr_)
#define CLEM_CPU_I_JSL_LOG(_clem_cpu_, _adr_, _bank_)
#define CLEM_CPU_I_RTS_LOG(_clem_cpu_, _adr_)
#define CLEM_CPU_I_RTL_LOG(_clem_cpu_, _adr_, _bank_)
#define CLEM_CPU_I_INTR_LOG(_clem_cpu_, _name_)
#define CLEM_CPU_I_RTI_LOG(_clem_cpu_, _adr_, _bank_)
#endif


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
    ClemensMachine* clem,
    struct ClemensInstruction* inst
) {
    char operand[24];
    operand[0] = '\0';

    switch (inst->desc->addr_mode) {
        case kClemensCPUAddrMode_Immediate:
            if (inst->opc_8) {
                snprintf(operand, sizeof(operand), "#$%02X", (uint8_t)inst->value);
            } else {
                snprintf(operand, sizeof(operand), "#$%04X", inst->value);
            }
            break;
        case kClemensCPUAddrMode_Absolute:
            snprintf(operand, sizeof(operand), "$%04X", inst->value);
            break;
        case kClemensCPUAddrMode_AbsoluteLong:
            snprintf(operand, sizeof(operand), "$%02X%04X", inst->bank, inst->value);
            break;
        case kClemensCPUAddrMode_Absolute_X:
            snprintf(operand, sizeof(operand), "$%04X, X", inst->value);
            break;
        case kClemensCPUAddrMode_Absolute_Y:
            snprintf(operand, sizeof(operand), "$%04X, Y", inst->value);
            break;
        case kClemensCPUAddrMode_AbsoluteLong_X:
            snprintf(operand, sizeof(operand), "$%02X%04X, X", inst->bank, inst->value);
            break;
        case kClemensCPUAddrMode_DirectPage:
            snprintf(operand, sizeof(operand), "$%02X", inst->value);
            break;
        case kClemensCPUAddrMode_DirectPage_X:
            snprintf(operand, sizeof(operand), "$%02X, X", inst->value);
            break;
        case kClemensCPUAddrMode_DirectPage_Y:
            snprintf(operand, sizeof(operand), "$%02X, Y", inst->value);
            break;
        case kClemensCPUAddrMode_DirectPageIndirect:
            snprintf(operand, sizeof(operand), "($%02X)", inst->value);
            break;
        case kClemensCPUAddrMode_DirectPageIndirectLong:
            snprintf(operand, sizeof(operand), "[$%02X]", inst->value);
            break;
        case kClemensCPUAddrMode_DirectPage_X_Indirect:
            snprintf(operand, sizeof(operand), "($%02X, X)", inst->value);
            break;
        case kClemensCPUAddrMode_DirectPage_Indirect_Y:
            snprintf(operand, sizeof(operand), "($%02X), Y", inst->value);
            break;
        case kClemensCPUAddrMode_DirectPage_IndirectLong_Y:
            snprintf(operand, sizeof(operand), "[$%02X], Y", inst->value);
            break;
        case kClemensCPUAddrMode_PCRelative:
            snprintf(operand, sizeof(operand), "$%02X (%d)", inst->value, (int8_t)inst->value);
            break;
        case kClemensCPUAddrMode_PCRelativeLong:
            snprintf(operand, sizeof(operand), "$%04X (%d)", inst->value, (int16_t)inst->value);
            break;
        case kClemensCPUAddrMode_PC:
            snprintf(operand, sizeof(operand), "$%04X", inst->value);
            break;
        case kClemensCPUAddrMode_PCIndirect:
            snprintf(operand, sizeof(operand), "($%04X)", inst->value);
            break;
        case kClemensCPUAddrMode_PCIndirect_X:
            snprintf(operand, sizeof(operand), "($%04X, X)", inst->value);
            break;
        case kClemensCPUAddrMode_PCLong:
            snprintf(operand, sizeof(operand), "$%02X%04X", inst->bank, inst->value);
            break;
        case kClemensCPUAddrMode_PCLongIndirect:
            snprintf(operand, sizeof(operand), "[$%04X]", inst->bank, inst->value);
            break;
        case kClemensCPUAddrMode_Operand:
            snprintf(operand, sizeof(operand), "%02X", inst->value);
            break;
        case kClemensCPUAddrMode_Stack_Relative:
            snprintf(operand, sizeof(operand), "%02X, S", inst->value);
            break;
        case kClemensCPUAddrMode_Stack_Relative_Indirect_Y:
            snprintf(operand, sizeof(operand), "(%02X, S), Y", inst->value);
            break;
    }

    if (clem->debug_flags & kClemensDebugFlag_StdoutOpcode) {
        printf(ANSI_COLOR_BLUE "%02X:%04X "
            ANSI_COLOR_CYAN "%s" ANSI_COLOR_YELLOW " %s" ANSI_COLOR_RESET "\n",
            inst->pbr, inst->addr, inst->desc->name, operand);
    }
    if ((clem->debug_flags & kClemensDebugFlag_OpcodeCallback) && clem->opcode_post) {
        (*clem->opcode_post)(inst,  operand, clem->debug_user_ptr);
    }
}

void _clem_debug_memory_dump(
    ClemensMachine* clem,
    uint8_t mem_page,
    uint8_t mem_bank,
    uint8_t page_count
) {
    char filename[64];
    snprintf(filename, sizeof(filename), "mem_%02x_%04x_%u.bin",
             (uint16_t)mem_page << 8, mem_bank, page_count);
    filename[63] = '\0';
    FILE* fp = fopen(filename, "wb");
    if (fp) {
        uint16_t mem_addr = (uint16_t)mem_page << 8;
        while (page_count > 0) {
            clem_clocks_duration_t clocks_spent;
            uint8_t mem_next_bank = mem_bank;
            fwrite(_clem_get_memory_bank(clem, mem_bank, &clocks_spent) + mem_addr, 256, 1, fp);
            if (mem_addr + 0x0100 < mem_addr) {
                _clem_next_dbr(clem, &mem_next_bank, mem_bank);
            }
            mem_addr += 0x0100;
            mem_bank = mem_next_bank;
            --page_count;
        }
        fclose(fp);
    } else {
        printf("Failed to dump memory %s", filename);
    }
}

void _clem_init_instruction_map() {
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

    _opcode_description(CLEM_OPC_AND_IMM, "AND", kClemensCPUAddrMode_Immediate);
    _opcode_description(CLEM_OPC_AND_ABS, "AND", kClemensCPUAddrMode_Absolute);
    _opcode_description(CLEM_OPC_AND_ABSL, "AND",
                        kClemensCPUAddrMode_AbsoluteLong);
    _opcode_description(CLEM_OPC_AND_DP,  "AND",
                        kClemensCPUAddrMode_DirectPage);
    _opcode_description(CLEM_OPC_AND_DP_INDIRECT,  "AND",
                        kClemensCPUAddrMode_DirectPageIndirect);
    _opcode_description(CLEM_OPC_AND_DP_INDIRECTL,  "AND",
                        kClemensCPUAddrMode_DirectPageIndirectLong);
    _opcode_description(CLEM_OPC_AND_ABS_IDX,  "AND",
                        kClemensCPUAddrMode_Absolute_X);
    _opcode_description(CLEM_OPC_AND_ABSL_IDX,  "AND",
                        kClemensCPUAddrMode_AbsoluteLong_X);
    _opcode_description(CLEM_OPC_AND_ABS_IDY,  "AND",
                        kClemensCPUAddrMode_Absolute_Y);
    _opcode_description(CLEM_OPC_AND_DP_IDX,  "AND",
                        kClemensCPUAddrMode_DirectPage_X);
    _opcode_description(CLEM_OPC_AND_DP_IDX_INDIRECT,  "AND",
                        kClemensCPUAddrMode_DirectPage_X_Indirect);
    _opcode_description(CLEM_OPC_AND_DP_INDIRECT_IDY,  "AND",
                        kClemensCPUAddrMode_DirectPage_Indirect_Y);
    _opcode_description(CLEM_OPC_AND_DP_INDIRECTL_IDY,  "AND",
                        kClemensCPUAddrMode_DirectPage_IndirectLong_Y);
    _opcode_description(CLEM_OPC_AND_STACK_REL,  "AND",
                        kClemensCPUAddrMode_Stack_Relative);
    _opcode_description(CLEM_OPC_AND_STACK_REL_INDIRECT_IDY,  "AND",
                        kClemensCPUAddrMode_Stack_Relative_Indirect_Y);

    _opcode_description(CLEM_OPC_ASL_A,   "ASL", kClemensCPUAddrMode_None);
    _opcode_description(CLEM_OPC_ASL_ABS, "ASL", kClemensCPUAddrMode_Absolute);
    _opcode_description(CLEM_OPC_ASL_DP,  "ASL",
                        kClemensCPUAddrMode_DirectPage);
    _opcode_description(CLEM_OPC_ASL_ABS_IDX,  "ASL",
                        kClemensCPUAddrMode_Absolute_X);
    _opcode_description(CLEM_OPC_ASL_ABS_DP_IDX,  "ASL",
                        kClemensCPUAddrMode_DirectPage_X);

    _opcode_description(CLEM_OPC_BCC,     "BCC", kClemensCPUAddrMode_PCRelative);
    _opcode_description(CLEM_OPC_BCS,     "BCS", kClemensCPUAddrMode_PCRelative);
    _opcode_description(CLEM_OPC_BEQ,     "BEQ", kClemensCPUAddrMode_PCRelative);

    _opcode_description(CLEM_OPC_BIT_IMM, "BIT", kClemensCPUAddrMode_Immediate);
    _opcode_description(CLEM_OPC_BIT_ABS, "BIT", kClemensCPUAddrMode_Absolute);
    _opcode_description(CLEM_OPC_BIT_DP,  "BIT", kClemensCPUAddrMode_DirectPage);
    _opcode_description(CLEM_OPC_BIT_ABS_IDX, "BIT",
                        kClemensCPUAddrMode_Absolute_X);
    _opcode_description(CLEM_OPC_BIT_DP_IDX,  "BIT",
                        kClemensCPUAddrMode_DirectPage_X);

    _opcode_description(CLEM_OPC_BMI,     "BMI", kClemensCPUAddrMode_PCRelative);
    _opcode_description(CLEM_OPC_BNE,     "BNE", kClemensCPUAddrMode_PCRelative);
    _opcode_description(CLEM_OPC_BPL,     "BPL", kClemensCPUAddrMode_PCRelative);
    _opcode_description(CLEM_OPC_BRA,     "BRA", kClemensCPUAddrMode_PCRelative);
    _opcode_description(CLEM_OPC_BRL,     "BRL", kClemensCPUAddrMode_PCRelativeLong);
    _opcode_description(CLEM_OPC_BVC,     "BVC", kClemensCPUAddrMode_PCRelative);
    _opcode_description(CLEM_OPC_BVS,     "BVS", kClemensCPUAddrMode_PCRelative);

    _opcode_description(CLEM_OPC_BRK,     "BRK", kClemensCPUAddrMode_Operand);

    _opcode_description(CLEM_OPC_CLC,     "CLC", kClemensCPUAddrMode_None);
    _opcode_description(CLEM_OPC_CLD,     "CLD", kClemensCPUAddrMode_None);
    _opcode_description(CLEM_OPC_CLI,     "CLI", kClemensCPUAddrMode_None);
    _opcode_description(CLEM_OPC_CLV,     "CLV", kClemensCPUAddrMode_None);

    _opcode_description(CLEM_OPC_CMP_IMM, "CMP", kClemensCPUAddrMode_Immediate);
    _opcode_description(CLEM_OPC_CMP_ABS, "CMP", kClemensCPUAddrMode_Absolute);
    _opcode_description(CLEM_OPC_CMP_ABSL, "CMP",
                        kClemensCPUAddrMode_AbsoluteLong);
    _opcode_description(CLEM_OPC_CMP_DP,  "CMP",
                        kClemensCPUAddrMode_DirectPage);
    _opcode_description(CLEM_OPC_CMP_DP_INDIRECT,  "CMP",
                        kClemensCPUAddrMode_DirectPageIndirect);
    _opcode_description(CLEM_OPC_CMP_DP_INDIRECTL,  "CMP",
                        kClemensCPUAddrMode_DirectPageIndirectLong);
    _opcode_description(CLEM_OPC_CMP_ABS_IDX,  "CMP",
                        kClemensCPUAddrMode_Absolute_X);
    _opcode_description(CLEM_OPC_CMP_ABSL_IDX,  "CMP",
                        kClemensCPUAddrMode_AbsoluteLong_X);
    _opcode_description(CLEM_OPC_CMP_ABS_IDY,  "CMP",
                        kClemensCPUAddrMode_Absolute_Y);
    _opcode_description(CLEM_OPC_CMP_DP_IDX,  "CMP",
                        kClemensCPUAddrMode_DirectPage_X);
    _opcode_description(CLEM_OPC_CMP_DP_IDX_INDIRECT,  "CMP",
                        kClemensCPUAddrMode_DirectPage_X_Indirect);
    _opcode_description(CLEM_OPC_CMP_DP_INDIRECT_IDY,  "CMP",
                        kClemensCPUAddrMode_DirectPage_Indirect_Y);
    _opcode_description(CLEM_OPC_CMP_DP_INDIRECTL_IDY,  "CMP",
                        kClemensCPUAddrMode_DirectPage_IndirectLong_Y);
    _opcode_description(CLEM_OPC_CMP_STACK_REL,  "CMP",
                        kClemensCPUAddrMode_Stack_Relative);
    _opcode_description(CLEM_OPC_CMP_STACK_REL_INDIRECT_IDY,  "CMP",
                        kClemensCPUAddrMode_Stack_Relative_Indirect_Y);

    _opcode_description(CLEM_OPC_COP,     "COP", kClemensCPUAddrMode_Operand);

    _opcode_description(CLEM_OPC_CPX_IMM, "CPX", kClemensCPUAddrMode_Immediate);
    _opcode_description(CLEM_OPC_CPX_ABS, "CPX", kClemensCPUAddrMode_Absolute);
    _opcode_description(CLEM_OPC_CPX_DP,  "CPX", kClemensCPUAddrMode_DirectPage);

    _opcode_description(CLEM_OPC_CPY_IMM, "CPY", kClemensCPUAddrMode_Immediate);
    _opcode_description(CLEM_OPC_CPY_ABS, "CPY", kClemensCPUAddrMode_Absolute);
    _opcode_description(CLEM_OPC_CPY_DP,  "CPY", kClemensCPUAddrMode_DirectPage);

    _opcode_description(CLEM_OPC_DEC_A,   "DEC", kClemensCPUAddrMode_None);
    _opcode_description(CLEM_OPC_DEC_ABS, "DEC", kClemensCPUAddrMode_Absolute);
    _opcode_description(CLEM_OPC_DEC_DP,  "DEC",
                        kClemensCPUAddrMode_DirectPage);
    _opcode_description(CLEM_OPC_DEC_ABS_IDX,  "DEC",
                        kClemensCPUAddrMode_Absolute_X);
    _opcode_description(CLEM_OPC_DEC_ABS_DP_IDX,  "DEC",
                        kClemensCPUAddrMode_DirectPage_X);

    _opcode_description(CLEM_OPC_DEX,     "DEX", kClemensCPUAddrMode_None);
    _opcode_description(CLEM_OPC_DEY,     "DEY", kClemensCPUAddrMode_None);

    _opcode_description(CLEM_OPC_EOR_IMM, "EOR", kClemensCPUAddrMode_Immediate);
    _opcode_description(CLEM_OPC_EOR_ABS, "EOR", kClemensCPUAddrMode_Absolute);
    _opcode_description(CLEM_OPC_EOR_ABSL, "EOR",
                        kClemensCPUAddrMode_AbsoluteLong);
    _opcode_description(CLEM_OPC_EOR_DP,  "EOR",
                        kClemensCPUAddrMode_DirectPage);
    _opcode_description(CLEM_OPC_EOR_DP_INDIRECT,  "EOR",
                        kClemensCPUAddrMode_DirectPageIndirect);
    _opcode_description(CLEM_OPC_EOR_DP_INDIRECTL,  "EOR",
                        kClemensCPUAddrMode_DirectPageIndirectLong);
    _opcode_description(CLEM_OPC_EOR_ABS_IDX,  "EOR",
                        kClemensCPUAddrMode_Absolute_X);
    _opcode_description(CLEM_OPC_EOR_ABSL_IDX,  "EOR",
                        kClemensCPUAddrMode_AbsoluteLong_X);
    _opcode_description(CLEM_OPC_EOR_ABS_IDY,  "EOR",
                        kClemensCPUAddrMode_Absolute_Y);
    _opcode_description(CLEM_OPC_EOR_DP_IDX,  "EOR",
                        kClemensCPUAddrMode_DirectPage_X);
    _opcode_description(CLEM_OPC_EOR_DP_IDX_INDIRECT,  "EOR",
                        kClemensCPUAddrMode_DirectPage_X_Indirect);
    _opcode_description(CLEM_OPC_EOR_DP_INDIRECT_IDY,  "EOR",
                        kClemensCPUAddrMode_DirectPage_Indirect_Y);
    _opcode_description(CLEM_OPC_EOR_DP_INDIRECTL_IDY,  "EOR",
                        kClemensCPUAddrMode_DirectPage_IndirectLong_Y);
    _opcode_description(CLEM_OPC_EOR_STACK_REL,  "EOR",
                        kClemensCPUAddrMode_Stack_Relative);
    _opcode_description(CLEM_OPC_EOR_STACK_REL_INDIRECT_IDY,  "EOR",
                        kClemensCPUAddrMode_Stack_Relative_Indirect_Y);

    _opcode_description(CLEM_OPC_INC_A,   "INC", kClemensCPUAddrMode_None);
    _opcode_description(CLEM_OPC_INC_ABS, "INC", kClemensCPUAddrMode_Absolute);
    _opcode_description(CLEM_OPC_INC_DP,  "INC",
                        kClemensCPUAddrMode_DirectPage);
    _opcode_description(CLEM_OPC_INC_ABS_IDX,  "INC",
                        kClemensCPUAddrMode_Absolute_X);
    _opcode_description(CLEM_OPC_INC_ABS_DP_IDX,  "INC",
                        kClemensCPUAddrMode_DirectPage_X);

    _opcode_description(CLEM_OPC_INX,     "INX", kClemensCPUAddrMode_None);
    _opcode_description(CLEM_OPC_INY,     "INY", kClemensCPUAddrMode_None);


    _opcode_description(CLEM_OPC_JMP_ABS, "JMP", kClemensCPUAddrMode_PC);
    _opcode_description(CLEM_OPC_JMP_INDIRECT, "JMP",
                        kClemensCPUAddrMode_PCIndirect);
    _opcode_description(CLEM_OPC_JMP_INDIRECT_IDX, "JMP",
                        kClemensCPUAddrMode_PCIndirect_X);
    _opcode_description(CLEM_OPC_JMP_ABSL, "JML",
                        kClemensCPUAddrMode_PCLong);
    _opcode_description(CLEM_OPC_JMP_ABSL_INDIRECT, "JML",
                        kClemensCPUAddrMode_PCLongIndirect);

    _opcode_description(CLEM_OPC_JSL,     "JSL", kClemensCPUAddrMode_AbsoluteLong);
    _opcode_description(CLEM_OPC_JSR,     "JSR", kClemensCPUAddrMode_Absolute);

    _opcode_description(CLEM_OPC_LDA_IMM, "LDA", kClemensCPUAddrMode_Immediate);
    _opcode_description(CLEM_OPC_LDA_ABS, "LDA", kClemensCPUAddrMode_Absolute);
    _opcode_description(CLEM_OPC_LDA_ABSL, "LDA",
                        kClemensCPUAddrMode_AbsoluteLong);
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


    _opcode_description(CLEM_OPC_LDX_IMM, "LDX", kClemensCPUAddrMode_Immediate);
    _opcode_description(CLEM_OPC_LDX_ABS, "LDX", kClemensCPUAddrMode_Absolute);
    _opcode_description(CLEM_OPC_LDX_DP,  "LDX", kClemensCPUAddrMode_DirectPage);
    _opcode_description(CLEM_OPC_LDX_ABS_IDY, "LDX",
                        kClemensCPUAddrMode_Absolute_Y);
    _opcode_description(CLEM_OPC_LDX_DP_IDY,  "LDX",
                        kClemensCPUAddrMode_DirectPage_Y);

    _opcode_description(CLEM_OPC_LDY_IMM, "LDY", kClemensCPUAddrMode_Immediate);
    _opcode_description(CLEM_OPC_LDY_ABS, "LDY", kClemensCPUAddrMode_Absolute);
    _opcode_description(CLEM_OPC_LDY_DP,  "LDY", kClemensCPUAddrMode_DirectPage);
    _opcode_description(CLEM_OPC_LDY_ABS_IDX, "LDY",
                        kClemensCPUAddrMode_Absolute_X);
    _opcode_description(CLEM_OPC_LDY_DP_IDX,  "LDY",
                        kClemensCPUAddrMode_DirectPage_X);

    _opcode_description(CLEM_OPC_LSR_A,   "LSR", kClemensCPUAddrMode_None);
    _opcode_description(CLEM_OPC_LSR_ABS, "LSR", kClemensCPUAddrMode_Absolute);
    _opcode_description(CLEM_OPC_LSR_DP,  "LSR",
                        kClemensCPUAddrMode_DirectPage);
    _opcode_description(CLEM_OPC_LSR_ABS_IDX,  "LSR",
                        kClemensCPUAddrMode_Absolute_X);
    _opcode_description(CLEM_OPC_LSR_ABS_DP_IDX,  "LSR",
                        kClemensCPUAddrMode_DirectPage_X);
    _opcode_description(CLEM_OPC_MVN,     "MVN", kClemensCPUAddrMode_MoveBlock);
    _opcode_description(CLEM_OPC_MVP,     "MVP", kClemensCPUAddrMode_MoveBlock);

    _opcode_description(CLEM_OPC_NOP,     "NOP", kClemensCPUAddrMode_None);

    _opcode_description(CLEM_OPC_ORA_IMM, "ORA", kClemensCPUAddrMode_Immediate);
    _opcode_description(CLEM_OPC_ORA_ABS, "ORA", kClemensCPUAddrMode_Absolute);
    _opcode_description(CLEM_OPC_ORA_ABSL, "ORA",
                        kClemensCPUAddrMode_AbsoluteLong);
    _opcode_description(CLEM_OPC_ORA_DP,  "ORA",
                        kClemensCPUAddrMode_DirectPage);
    _opcode_description(CLEM_OPC_ORA_DP_INDIRECT,  "ORA",
                        kClemensCPUAddrMode_DirectPageIndirect);
    _opcode_description(CLEM_OPC_ORA_DP_INDIRECTL,  "ORA",
                        kClemensCPUAddrMode_DirectPageIndirectLong);
    _opcode_description(CLEM_OPC_ORA_ABS_IDX,  "ORA",
                        kClemensCPUAddrMode_Absolute_X);
    _opcode_description(CLEM_OPC_ORA_ABSL_IDX,  "ORA",
                        kClemensCPUAddrMode_AbsoluteLong_X);
    _opcode_description(CLEM_OPC_ORA_ABS_IDY,  "ORA",
                        kClemensCPUAddrMode_Absolute_Y);
    _opcode_description(CLEM_OPC_ORA_DP_IDX,  "ORA",
                        kClemensCPUAddrMode_DirectPage_X);
    _opcode_description(CLEM_OPC_ORA_DP_IDX_INDIRECT,  "ORA",
                        kClemensCPUAddrMode_DirectPage_X_Indirect);
    _opcode_description(CLEM_OPC_ORA_DP_INDIRECT_IDY,  "ORA",
                        kClemensCPUAddrMode_DirectPage_Indirect_Y);
    _opcode_description(CLEM_OPC_ORA_DP_INDIRECTL_IDY,  "ORA",
                        kClemensCPUAddrMode_DirectPage_IndirectLong_Y);
    _opcode_description(CLEM_OPC_ORA_STACK_REL,  "ORA",
                        kClemensCPUAddrMode_Stack_Relative);
    _opcode_description(CLEM_OPC_ORA_STACK_REL_INDIRECT_IDY,  "ORA",
                        kClemensCPUAddrMode_Stack_Relative_Indirect_Y);

    _opcode_description(CLEM_OPC_PEA_ABS, "PEA", kClemensCPUAddrMode_Absolute);
    _opcode_description(CLEM_OPC_PEI_DP_INDIRECT, "PEI",
                        kClemensCPUAddrMode_DirectPageIndirect);
    _opcode_description(CLEM_OPC_PER,     "PER",
                        kClemensCPUAddrMode_PCRelativeLong);
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

    _opcode_description(CLEM_OPC_ROL_A,   "ROL", kClemensCPUAddrMode_None);
    _opcode_description(CLEM_OPC_ROL_ABS, "ROL", kClemensCPUAddrMode_Absolute);
    _opcode_description(CLEM_OPC_ROL_DP,  "ROL",
                        kClemensCPUAddrMode_DirectPage);
    _opcode_description(CLEM_OPC_ROL_ABS_IDX,  "ROL",
                        kClemensCPUAddrMode_Absolute_X);
    _opcode_description(CLEM_OPC_ROL_ABS_DP_IDX,  "ROL",
                        kClemensCPUAddrMode_DirectPage_X);

    _opcode_description(CLEM_OPC_ROR_A,   "ROR", kClemensCPUAddrMode_None);
    _opcode_description(CLEM_OPC_ROR_ABS, "ROR", kClemensCPUAddrMode_Absolute);
    _opcode_description(CLEM_OPC_ROR_DP,  "ROR",
                        kClemensCPUAddrMode_DirectPage);
    _opcode_description(CLEM_OPC_ROR_ABS_IDX,  "ROR",
                        kClemensCPUAddrMode_Absolute_X);
    _opcode_description(CLEM_OPC_ROR_ABS_DP_IDX,  "ROR",
                        kClemensCPUAddrMode_DirectPage_X);

    _opcode_description(CLEM_OPC_RTI,     "RTI", kClemensCPUAddrMode_None);
    _opcode_description(CLEM_OPC_RTL,     "RTL", kClemensCPUAddrMode_None);
    _opcode_description(CLEM_OPC_RTS,     "RTS", kClemensCPUAddrMode_None);

    _opcode_description(CLEM_OPC_SBC_IMM, "SBC", kClemensCPUAddrMode_Immediate);
    _opcode_description(CLEM_OPC_SBC_ABS, "SBC", kClemensCPUAddrMode_Absolute);
    _opcode_description(CLEM_OPC_SBC_ABSL, "SBC",
                        kClemensCPUAddrMode_AbsoluteLong);
    _opcode_description(CLEM_OPC_SBC_DP,  "SBC",
                        kClemensCPUAddrMode_DirectPage);
    _opcode_description(CLEM_OPC_SBC_DP_INDIRECT,  "SBC",
                        kClemensCPUAddrMode_DirectPageIndirect);
    _opcode_description(CLEM_OPC_SBC_DP_INDIRECTL,  "SBC",
                        kClemensCPUAddrMode_DirectPageIndirectLong);
    _opcode_description(CLEM_OPC_SBC_ABS_IDX,  "SBC",
                        kClemensCPUAddrMode_Absolute_X);
    _opcode_description(CLEM_OPC_SBC_ABSL_IDX,  "SBC",
                        kClemensCPUAddrMode_AbsoluteLong_X);
    _opcode_description(CLEM_OPC_SBC_ABS_IDY,  "SBC",
                        kClemensCPUAddrMode_Absolute_Y);
    _opcode_description(CLEM_OPC_SBC_DP_IDX,  "SBC",
                        kClemensCPUAddrMode_DirectPage_X);
    _opcode_description(CLEM_OPC_SBC_DP_IDX_INDIRECT,  "SBC",
                        kClemensCPUAddrMode_DirectPage_X_Indirect);
    _opcode_description(CLEM_OPC_SBC_DP_INDIRECT_IDY,  "SBC",
                        kClemensCPUAddrMode_DirectPage_Indirect_Y);
    _opcode_description(CLEM_OPC_SBC_DP_INDIRECTL_IDY,  "SBC",
                        kClemensCPUAddrMode_DirectPage_IndirectLong_Y);
    _opcode_description(CLEM_OPC_SBC_STACK_REL,  "SBC",
                        kClemensCPUAddrMode_Stack_Relative);
    _opcode_description(CLEM_OPC_SBC_STACK_REL_INDIRECT_IDY,  "SBC",
                        kClemensCPUAddrMode_Stack_Relative_Indirect_Y);

    _opcode_description(CLEM_OPC_SEC,     "SEC", kClemensCPUAddrMode_None);
    _opcode_description(CLEM_OPC_SED,     "SED", kClemensCPUAddrMode_None);
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

    _opcode_description(CLEM_OPC_STP,     "STP", kClemensCPUAddrMode_None);

    _opcode_description(CLEM_OPC_STX_ABS, "STX", kClemensCPUAddrMode_Absolute);
    _opcode_description(CLEM_OPC_STX_DP,  "STX", kClemensCPUAddrMode_DirectPage);
    _opcode_description(CLEM_OPC_STX_DP_IDY, "STX",
                        kClemensCPUAddrMode_DirectPage_Y);
    _opcode_description(CLEM_OPC_STY_ABS, "STY", kClemensCPUAddrMode_Absolute);
    _opcode_description(CLEM_OPC_STY_DP,  "STY", kClemensCPUAddrMode_DirectPage);
    _opcode_description(CLEM_OPC_STY_DP_IDX, "STY",
                        kClemensCPUAddrMode_DirectPage_X);
    _opcode_description(CLEM_OPC_STZ_ABS, "STZ", kClemensCPUAddrMode_Absolute);
    _opcode_description(CLEM_OPC_STZ_DP,  "STZ", kClemensCPUAddrMode_DirectPage);
    _opcode_description(CLEM_OPC_STZ_ABS_IDX, "STZ",
                        kClemensCPUAddrMode_Absolute_X);
    _opcode_description(CLEM_OPC_STZ_DP_IDX, "STZ",
                        kClemensCPUAddrMode_DirectPage_X);

    _opcode_description(CLEM_OPC_TRB_ABS, "TRB", kClemensCPUAddrMode_Absolute);
    _opcode_description(CLEM_OPC_TRB_DP,  "TRB", kClemensCPUAddrMode_DirectPage);
    _opcode_description(CLEM_OPC_TSB_ABS, "TSB", kClemensCPUAddrMode_Absolute);
    _opcode_description(CLEM_OPC_TSB_DP,  "TSB", kClemensCPUAddrMode_DirectPage);

    _opcode_description(CLEM_OPC_TAX,     "TAX", kClemensCPUAddrMode_None);
    _opcode_description(CLEM_OPC_TAY,     "TAX", kClemensCPUAddrMode_None);
    _opcode_description(CLEM_OPC_TCD,     "TCD", kClemensCPUAddrMode_None);
    _opcode_description(CLEM_OPC_TDC,     "TDC", kClemensCPUAddrMode_None);
    _opcode_description(CLEM_OPC_TCS,     "TCS", kClemensCPUAddrMode_None);
    _opcode_description(CLEM_OPC_TSC,     "TSC", kClemensCPUAddrMode_None);
    _opcode_description(CLEM_OPC_TSX,     "TSX", kClemensCPUAddrMode_None);
    _opcode_description(CLEM_OPC_TXA,     "TXA", kClemensCPUAddrMode_None);
    _opcode_description(CLEM_OPC_TXS,     "TXS", kClemensCPUAddrMode_None);
    _opcode_description(CLEM_OPC_TXY,     "TXY", kClemensCPUAddrMode_None);
    _opcode_description(CLEM_OPC_TYA,     "TYA", kClemensCPUAddrMode_None);
    _opcode_description(CLEM_OPC_TYX,     "TYX", kClemensCPUAddrMode_None);


    _opcode_description(CLEM_OPC_WAI,     "WAI", kClemensCPUAddrMode_None);
    _opcode_description(CLEM_OPC_WDM,     "WDM", kClemensCPUAddrMode_Operand);

    _opcode_description(CLEM_OPC_XBA,     "XBA", kClemensCPUAddrMode_None);
    _opcode_description(CLEM_OPC_XCE,     "XCE", kClemensCPUAddrMode_None);
}

bool clemens_is_initialized_simple(const ClemensMachine* machine) {
    return (machine->fpi_bank_map[0xff] != NULL);
}

bool clemens_is_mmio_initialized(const ClemensMachine* machine) {
    return machine->mmio.bank_page_map[0] && machine->mmio.bank_page_map[1];
}

bool clemens_is_initialized(const ClemensMachine* machine) {
    if (!clemens_is_initialized_simple(machine)) {
        return false;
    }
    if (!machine->fpi_bank_map[0] || !machine->fpi_bank_map[1]) {
        return false;
    }
    if (!machine->mega2_bank_map[0] || !machine->mega2_bank_map[1]) {
        return false;
    }
    if (!machine->clocks_step ||
        machine->clocks_step > machine->clocks_step_mega2) {
        return false;
    }
    return true;
}

void clemens_opcode_callback(
    ClemensMachine* clem,
    ClemensOpcodeCallback callback,
    void* callback_ptr
) {
    if (callback) {
        clem->debug_flags |= kClemensDebugFlag_OpcodeCallback;
    } else {
        clem->debug_flags &= ~kClemensDebugFlag_OpcodeCallback;
    }
    clem->opcode_post = callback;
    clem->debug_user_ptr = callback_ptr;
}

int clemens_init(
    ClemensMachine* machine,
    uint32_t speed_factor,
    uint32_t clocks_step,
    void* rom,
    size_t romSize,
    void* e0bank,
    void* e1bank,
    void* fpiRAM,
    void* slotROM,
    void* slotExpansionROM,
    unsigned int fpiRAMBankCount
) {
    machine->cpu.pins.resbIn = true;
    machine->clocks_step = clocks_step;
    machine->clocks_step_mega2 = speed_factor;
    machine->clocks_spent = 0;
    if (romSize != CLEM_IIGS_ROM3_SIZE || rom == NULL) {
        return -1;
    }
    if (fpiRAMBankCount < 4 || fpiRAM == NULL ||
        e0bank == NULL || e1bank == NULL
    ) {
        return -2;
    }
    /* memory organization for the FPI */
    machine->fpi_bank_map[0xfc] = (uint8_t*)rom;
    machine->fpi_bank_map[0xfd] = (uint8_t*)rom + CLEM_IIGS_BANK_SIZE;
    machine->fpi_bank_map[0xfe] = (uint8_t*)rom + CLEM_IIGS_BANK_SIZE * 2;
    machine->fpi_bank_map[0xff] = (uint8_t*)rom + CLEM_IIGS_BANK_SIZE * 3;

    /* TODO: clear memory according to spec 0x00, 0xff, etc (look it up) */
    if (fpiRAMBankCount > 128) fpiRAMBankCount = 128;

    for (uint8_t i  = 0; i < (uint8_t)fpiRAMBankCount; ++i) {
        machine->fpi_bank_map[i] = ((uint8_t*)fpiRAM) + (i * CLEM_IIGS_BANK_SIZE);
        memset(machine->fpi_bank_map[i], 0, CLEM_IIGS_BANK_SIZE);
    }
    /* TODO: remap non used banks to used banks per the wrapping mechanism on
       the IIgs
    */
    machine->mega2_bank_map[0x00] = (uint8_t*)e0bank;
    memset(machine->mega2_bank_map[0x00], 0, CLEM_IIGS_BANK_SIZE);
    machine->mega2_bank_map[0x01] = (uint8_t*)e1bank;
    memset(machine->mega2_bank_map[0x01], 0, CLEM_IIGS_BANK_SIZE);

    for (uint8_t i = 0; i < 7; ++i) {
        machine->card_slot_memory[i] = ((uint8_t*)slotROM) + (i * 256);
        machine->card_slot_expansion_memory[i] = ((uint8_t*)slotExpansionROM) + (i * 256);
    }

    for (unsigned i = 0; i < 256; ++i) {
        _opcode_description((uint8_t)i, "...", kClemensCPUAddrMode_None);
    }

    _clem_init_instruction_map();

    return 0;
}

uint64_t clemens_clocks_per_second(ClemensMachine* clem, bool* is_slow_speed) {
    if (clem->mmio.speed_c036 & CLEM_MMIO_SPEED_FAST_ENABLED) {
        *is_slow_speed = false;
    } else {
        *is_slow_speed = true;
    }
    return clem->clocks_step_mega2 * 1000000;
}


void cpu_execute(struct Clemens65C816* cpu, ClemensMachine* clem) {
    uint16_t tmp_addr;
    uint16_t tmp_eaddr;
    uint16_t tmp_value;
    uint16_t tmp_pc;
    uint8_t tmp_data;
    uint8_t tmp_bnk0;
    uint8_t tmp_bnk1;
    uint8_t IR;

    struct ClemensInstruction opc_inst;
    uint16_t opc_addr;
    uint8_t opc_pbr;

    uint8_t carry;
    bool m_status;
    bool x_status;
    bool zero_flag;
    bool overflow_flag;
    bool neg_flag;

    assert(cpu->state_type == kClemensCPUStateType_Execute);
    /*
        Execute all cycles of an instruction here
    */
    tmp_pc = cpu->regs.PC;
    opc_pbr = cpu->regs.PBR;
    opc_addr = tmp_pc;

    //  TODO: Okay, we enter native mode but PBR is still 0x00 though we are
    //        reading code from ROM.  research what to do during the switch to
    //        native mode!   do the I/O memory registers still tell us to read
    //        from ROM though we are at PBR bank 0x00?  Or should PBR change to
    //        0xff?
    clem_read(clem, &cpu->regs.IR, tmp_pc++, cpu->regs.PBR,
                   CLEM_MEM_FLAG_OPCODE_FETCH);
    IR = cpu->regs.IR;
    //  This define may be overwritten by a non simple instruction
    _opcode_instruction_define_simple(&opc_inst, IR);

    m_status = (cpu->regs.P & kClemensCPUStatus_MemoryAccumulator) != 0;
    x_status = (cpu->regs.P & kClemensCPUStatus_Index) != 0;
    carry = (cpu->regs.P & kClemensCPUStatus_Carry) != 0;
    zero_flag = (cpu->regs.P & kClemensCPUStatus_Zero) != 0;

    switch (IR) {
        //
        // Start ADC
        case CLEM_OPC_ADC_IMM:
            _clem_read_pba_mode_imm_816(clem, &tmp_value, &tmp_pc, m_status);
            _cpu_adc(cpu, tmp_value, m_status);
            _opcode_instruction_define(&opc_inst, IR, tmp_value, m_status);
            break;
        case CLEM_OPC_ADC_ABS:
            _clem_read_pba_mode_abs(clem, &tmp_addr, &tmp_pc);
            _clem_read_data_816(clem, &tmp_value, tmp_addr, cpu->regs.DBR, m_status);
            _cpu_adc(cpu, tmp_value, m_status);
            _opcode_instruction_define(&opc_inst, IR, tmp_addr, m_status);
            break;
        case CLEM_OPC_ADC_ABSL:
            //  TODO: emulation mode
            _clem_read_pba_mode_absl(clem, &tmp_addr, &tmp_bnk0, &tmp_pc);
            _clem_read_data_816(clem, &tmp_value, tmp_addr, tmp_bnk0, m_status);
            _cpu_adc(cpu, tmp_value, m_status);
            _opcode_instruction_define_long(&opc_inst, IR, tmp_bnk0, tmp_addr);
            break;
        case CLEM_OPC_ADC_DP:
            _clem_read_pba_mode_dp(clem, &tmp_addr, &tmp_pc, &tmp_data, 0, false);
            _clem_read_data_816(clem, &tmp_value, tmp_addr, 0x00, m_status);
            _cpu_adc(cpu, tmp_value, m_status);
            _opcode_instruction_define_dp(&opc_inst, IR, tmp_data);
            break;
        case CLEM_OPC_ADC_DP_INDIRECT:
            _clem_read_pba_mode_dp_indirect(clem, &tmp_addr, &tmp_pc, &tmp_data, 0, false);
            _clem_read_data_816(clem, &tmp_value, tmp_addr, cpu->regs.DBR, m_status);
            _cpu_adc(cpu, tmp_value, m_status);
            _opcode_instruction_define_dp(&opc_inst, IR, tmp_data);
            break;
        case CLEM_OPC_ADC_DP_INDIRECTL:
            _clem_read_pba_mode_dp_indirectl(
                clem, &tmp_addr, &tmp_bnk0, &tmp_pc, &tmp_data, 0, false);
            _clem_read_data_816(clem, &tmp_value, tmp_addr, tmp_bnk0, m_status);
            _cpu_adc(cpu, tmp_value, m_status);
            _opcode_instruction_define_dp(&opc_inst, IR, tmp_data);
            break;
        case CLEM_OPC_ADC_ABS_IDX:
            _clem_read_pba_mode_abs(clem, &tmp_addr, &tmp_pc);
            _clem_read_data_indexed_816(
                clem, &tmp_value, tmp_addr, cpu->regs.X, cpu->regs.DBR,
                m_status, x_status);
            _cpu_adc(cpu, tmp_value, m_status);
            _opcode_instruction_define(&opc_inst, IR, tmp_addr, m_status);
            break;
        case CLEM_OPC_ADC_ABSL_IDX:
            _clem_read_pba_mode_absl(clem, &tmp_addr, &tmp_bnk0, &tmp_pc);
            _clem_read_data_indexed_816(
                clem, &tmp_value, tmp_addr, cpu->regs.X, tmp_bnk0, m_status, x_status);
            _cpu_adc(cpu, tmp_value, m_status);
            _opcode_instruction_define_long(&opc_inst, IR, tmp_bnk0, tmp_addr);
            break;
        case CLEM_OPC_ADC_ABS_IDY:      // $addr + Y
            _clem_read_pba_mode_abs(clem, &tmp_addr, &tmp_pc);
            _clem_read_data_indexed_816(clem, &tmp_value, tmp_addr, cpu->regs.Y,
                cpu->regs.DBR, m_status, x_status);
            _cpu_adc(cpu, tmp_value, m_status);
            _opcode_instruction_define(&opc_inst, IR, tmp_addr, m_status);
            break;
        case CLEM_OPC_ADC_DP_IDX:
            _clem_read_pba_mode_dp(clem, &tmp_addr, &tmp_pc, &tmp_data,
                cpu->regs.X, x_status);
            _clem_cycle(clem, 1);             // extra IO cycle for d,x
            _clem_read_data_816(clem, &tmp_value, tmp_addr, 0x00, m_status);
            _cpu_adc(cpu, tmp_value, m_status);
            _opcode_instruction_define_dp(&opc_inst, IR, tmp_data);
            break;
        case CLEM_OPC_ADC_DP_IDX_INDIRECT:
            _clem_read_pba_mode_dp_indirect(clem, &tmp_addr, &tmp_pc, &tmp_data,
                cpu->regs.X, x_status);
            _clem_cycle(clem, 1);       // extra IO for (d, X)
            _clem_read_data_816(clem, &tmp_value, tmp_addr, cpu->regs.DBR, m_status);
            _cpu_adc(cpu, tmp_value, m_status);
            _opcode_instruction_define_dp(&opc_inst, IR, tmp_data);
            break;
        case CLEM_OPC_ADC_DP_INDIRECT_IDY:
            _clem_read_pba_mode_dp_indirect(clem, &tmp_addr, &tmp_pc, &tmp_data, 0, false);
            _clem_read_data_indexed_816(
                clem, &tmp_value, tmp_addr, cpu->regs.Y, cpu->regs.DBR, m_status, x_status);
            _cpu_adc(cpu, tmp_value, m_status);
            _opcode_instruction_define_dp(&opc_inst, IR, tmp_data);
            break;
        case CLEM_OPC_ADC_DP_INDIRECTL_IDY:
            _clem_read_pba_mode_dp_indirectl(clem, &tmp_addr, &tmp_bnk0, &tmp_pc,
                &tmp_data, 0, false);
            _clem_read_data_indexed_816(
                clem, &tmp_value, tmp_addr, cpu->regs.Y, tmp_bnk0, m_status, x_status);
            _cpu_adc(cpu, tmp_value, m_status);
            _opcode_instruction_define_dp(&opc_inst, IR, tmp_data);
            break;
        case CLEM_OPC_ADC_STACK_REL:
            _clem_read_pba_mode_stack_rel(clem, &tmp_addr, &tmp_pc, &tmp_data);
            _clem_read_data_816(clem, &tmp_value, tmp_addr, 0x00, m_status);
            _cpu_adc(cpu, tmp_value, m_status);
            _opcode_instruction_define(&opc_inst, IR, tmp_data, m_status);
            break;
        case CLEM_OPC_ADC_STACK_REL_INDIRECT_IDY:
            _clem_read_pba_mode_stack_rel_indirect(clem, &tmp_addr, &tmp_pc, &tmp_data);
            _clem_read_data_indexed_816(
                clem, &tmp_value, tmp_addr, cpu->regs.Y, cpu->regs.DBR, m_status, x_status);
            _cpu_adc(cpu, tmp_value, m_status);
            _opcode_instruction_define(&opc_inst, IR, tmp_data, m_status);
            break;
        //  End ADC
        //
        //  Start AND
        case CLEM_OPC_AND_IMM:
            _clem_read_pba_mode_imm_816(clem, &tmp_value, &tmp_pc, m_status);
            _cpu_and(cpu, tmp_value, m_status);
            _opcode_instruction_define(&opc_inst, IR, tmp_value, m_status);
            break;
        case CLEM_OPC_AND_ABS:
            _clem_read_pba_mode_abs(clem, &tmp_addr, &tmp_pc);
            _clem_read_data_816(clem, &tmp_value, tmp_addr, cpu->regs.DBR, m_status);
            _cpu_and(cpu, tmp_value, m_status);
            _opcode_instruction_define(&opc_inst, IR, tmp_addr, m_status);
            break;
        case CLEM_OPC_AND_ABSL:
            //  TODO: emulation mode
            _clem_read_pba_mode_absl(clem, &tmp_addr, &tmp_bnk0, &tmp_pc);
            _clem_read_data_816(clem, &tmp_value, tmp_addr, tmp_bnk0, m_status);
            _cpu_and(cpu, tmp_value, m_status);
            _opcode_instruction_define_long(&opc_inst, IR, tmp_bnk0, tmp_addr);
            break;
        case CLEM_OPC_AND_DP:
            _clem_read_pba_mode_dp(clem, &tmp_addr, &tmp_pc, &tmp_data, 0, false);
            _clem_read_data_816(clem, &tmp_value, tmp_addr, 0x00, m_status);
            _cpu_and(cpu, tmp_value, m_status);
            _opcode_instruction_define_dp(&opc_inst, IR, tmp_data);
            break;
        case CLEM_OPC_AND_DP_INDIRECT:
            _clem_read_pba_mode_dp_indirect(clem, &tmp_addr, &tmp_pc, &tmp_data, 0, false);
            _clem_read_data_816(clem, &tmp_value, tmp_addr, cpu->regs.DBR, m_status);
            _cpu_and(cpu, tmp_value, m_status);
            _opcode_instruction_define_dp(&opc_inst, IR, tmp_data);
            break;
        case CLEM_OPC_AND_DP_INDIRECTL:
            _clem_read_pba_mode_dp_indirectl(
                clem, &tmp_addr, &tmp_bnk0, &tmp_pc, &tmp_data, 0, false);
            _clem_read_data_816(clem, &tmp_value, tmp_addr, tmp_bnk0, m_status);
            _cpu_and(cpu, tmp_value, m_status);
            _opcode_instruction_define_dp(&opc_inst, IR, tmp_data);
            break;
        case CLEM_OPC_AND_ABS_IDX:
            _clem_read_pba_mode_abs(clem, &tmp_addr, &tmp_pc);
            _clem_read_data_indexed_816(
                clem, &tmp_value, tmp_addr, cpu->regs.X, cpu->regs.DBR,
                m_status, x_status);
            _cpu_and(cpu, tmp_value, m_status);
            _opcode_instruction_define(&opc_inst, IR, tmp_addr, m_status);
            break;
        case CLEM_OPC_AND_ABSL_IDX:
            _clem_read_pba_mode_absl(clem, &tmp_addr, &tmp_bnk0, &tmp_pc);
            _clem_read_data_indexed_816(
                clem, &tmp_value, tmp_addr, cpu->regs.X, tmp_bnk0, m_status, x_status);
            _cpu_and(cpu, tmp_value, m_status);
            _opcode_instruction_define_long(&opc_inst, IR, tmp_bnk0, tmp_addr);
            break;
        case CLEM_OPC_AND_ABS_IDY:      // $addr + Y
            _clem_read_pba_mode_abs(clem, &tmp_addr, &tmp_pc);
            _clem_read_data_indexed_816(clem, &tmp_value, tmp_addr, cpu->regs.Y,
                cpu->regs.DBR, m_status, x_status);
            _cpu_and(cpu, tmp_value, m_status);
            _opcode_instruction_define(&opc_inst, IR, tmp_addr, m_status);
            break;
        case CLEM_OPC_AND_DP_IDX:
            _clem_read_pba_mode_dp(clem, &tmp_addr, &tmp_pc, &tmp_data,
                cpu->regs.X, x_status);
            _clem_cycle(clem, 1);             // extra IO cycle for d,x
            _clem_read_data_816(clem, &tmp_value, tmp_addr, 0x00, m_status);
            _cpu_and(cpu, tmp_value, m_status);
            _opcode_instruction_define_dp(&opc_inst, IR, tmp_data);
            break;
        case CLEM_OPC_AND_DP_IDX_INDIRECT:
            _clem_read_pba_mode_dp_indirect(clem, &tmp_addr, &tmp_pc, &tmp_data,
                cpu->regs.X, x_status);
            _clem_cycle(clem, 1);       // extra IO for (d, X)
            _clem_read_data_816(clem, &tmp_value, tmp_addr, cpu->regs.DBR, m_status);
            _cpu_and(cpu, tmp_value, m_status);
            _opcode_instruction_define_dp(&opc_inst, IR, tmp_data);
            break;
        case CLEM_OPC_AND_DP_INDIRECT_IDY:
            _clem_read_pba_mode_dp_indirect(clem, &tmp_addr, &tmp_pc, &tmp_data, 0, false);
            _clem_read_data_indexed_816(
                clem, &tmp_value, tmp_addr, cpu->regs.Y, cpu->regs.DBR, m_status, x_status);
            _cpu_and(cpu, tmp_value, m_status);
            _opcode_instruction_define_dp(&opc_inst, IR, tmp_data);
            break;
        case CLEM_OPC_AND_DP_INDIRECTL_IDY:
            _clem_read_pba_mode_dp_indirectl(clem, &tmp_addr, &tmp_bnk0, &tmp_pc,
                &tmp_data, 0, false);
            _clem_read_data_indexed_816(
                clem, &tmp_value, tmp_addr, cpu->regs.Y, tmp_bnk0, m_status, x_status);
            _cpu_and(cpu, tmp_value, m_status);
            _opcode_instruction_define_dp(&opc_inst, IR, tmp_data);
            break;
        case CLEM_OPC_AND_STACK_REL:
            _clem_read_pba_mode_stack_rel(clem, &tmp_addr, &tmp_pc, &tmp_data);
            _clem_read_data_816(clem, &tmp_value, tmp_addr, 0x00, m_status);
            _cpu_and(cpu, tmp_value, m_status);
            _opcode_instruction_define(&opc_inst, IR, tmp_data, m_status);
            break;
        case CLEM_OPC_AND_STACK_REL_INDIRECT_IDY:
            _clem_read_pba_mode_stack_rel_indirect(clem, &tmp_addr, &tmp_pc, &tmp_data);
            _clem_read_data_indexed_816(
                clem, &tmp_value, tmp_addr, cpu->regs.Y, cpu->regs.DBR, m_status, x_status);
            _cpu_and(cpu, tmp_value, m_status);
            _opcode_instruction_define(&opc_inst, IR, tmp_data, m_status);
            break;
        //  End ADC
        //
        //  Start ASL
        case CLEM_OPC_ASL_A:
            _cpu_asl(cpu, &cpu->regs.A, m_status);
            _clem_cycle(clem, 1);
            break;
        case CLEM_OPC_ASL_ABS:
            _clem_read_pba_mode_abs(clem, &tmp_addr, &tmp_pc);
            _clem_read_data_816(clem, &tmp_value, tmp_addr, cpu->regs.DBR, m_status);
            _cpu_asl(cpu, &tmp_value, m_status);
            _clem_cycle(clem, 1);
            _clem_write_816(clem, tmp_value, tmp_addr, cpu->regs.DBR, m_status);
            _opcode_instruction_define(&opc_inst, IR, tmp_addr, m_status);
            break;
        case CLEM_OPC_ASL_DP:
            _clem_read_pba_mode_dp(clem, &tmp_addr, &tmp_pc, &tmp_data, 0, false);
            _clem_read_data_816(clem, &tmp_value, tmp_addr, 0x00, m_status);
            _cpu_asl(cpu, &tmp_value, m_status);
            _clem_cycle(clem, 1);
            _clem_write_816(clem, tmp_value, tmp_addr, 0x00, m_status);
            _opcode_instruction_define_dp(&opc_inst, IR, tmp_data);
            break;
        case CLEM_OPC_ASL_ABS_IDX:
            _clem_read_pba_mode_abs(clem, &tmp_addr, &tmp_pc);
            _clem_cycle(clem, 1);
            _clem_read_data_indexed_816(
                clem, &tmp_value, tmp_addr, cpu->regs.X, cpu->regs.DBR,
                m_status, x_status);
            _cpu_asl(cpu, &tmp_value, m_status);
            _clem_cycle(clem, 1);
            _clem_write_indexed_816(clem, tmp_value, cpu->regs.DBR, tmp_addr,
                cpu->regs.X, m_status, x_status);
            _opcode_instruction_define(&opc_inst, IR, tmp_addr, m_status);
            break;
        case CLEM_OPC_ASL_ABS_DP_IDX:
            _clem_read_pba_mode_dp(clem, &tmp_addr, &tmp_pc, &tmp_data,
                cpu->regs.X, x_status);
            _clem_cycle(clem, 1);             // extra IO cycle for d,x
            _clem_read_data_816(clem, &tmp_value, tmp_addr, 0x00, m_status);
            _cpu_asl(cpu, &tmp_value, m_status);
            _clem_cycle(clem, 1);
            _clem_write_816(clem, tmp_value, tmp_addr, 0x00, m_status);
            _opcode_instruction_define_dp(&opc_inst, IR, tmp_data);
            break;
        //  End ASL
        //
        //  Start BIT
        case CLEM_OPC_BIT_IMM:
            _clem_read_pba_mode_imm_816(clem, &tmp_value, &tmp_pc, m_status);
            _cpu_bit(cpu, tmp_value, m_status);
            _opcode_instruction_define(&opc_inst, IR, tmp_value, m_status);
            break;
        case CLEM_OPC_BIT_ABS:
            _clem_read_pba_mode_abs(clem, &tmp_addr, &tmp_pc);
            _clem_read_data_816(clem, &tmp_value, tmp_addr, cpu->regs.DBR, m_status);
            _cpu_bit(cpu, tmp_value, m_status);
            _opcode_instruction_define(&opc_inst, IR, tmp_addr, m_status);
            break;
        case CLEM_OPC_BIT_DP:
            _clem_read_pba_mode_dp(clem, &tmp_addr, &tmp_pc, &tmp_data, 0, false);
            _clem_read_data_816(clem, &tmp_value, tmp_addr, 0x00, m_status);
            _cpu_bit(cpu, tmp_value, m_status);
            _opcode_instruction_define_dp(&opc_inst, IR, tmp_data);
            break;
        case CLEM_OPC_BIT_ABS_IDX:
            _clem_read_pba_mode_abs(clem, &tmp_addr, &tmp_pc);
            _clem_read_data_indexed_816(
                clem, &tmp_value, tmp_addr, cpu->regs.X, cpu->regs.DBR,
                m_status, x_status);
            _cpu_bit(cpu, tmp_value, m_status);
            _opcode_instruction_define(&opc_inst, IR, tmp_addr, m_status);
            break;
        case CLEM_OPC_BIT_DP_IDX:
            _clem_read_pba_mode_dp(clem, &tmp_addr, &tmp_pc, &tmp_data,
                cpu->regs.X, x_status);
            _clem_read_data_816(clem, &tmp_value, tmp_addr, 0x00, m_status);
            _cpu_bit(cpu, tmp_value, m_status);
            _opcode_instruction_define_dp(&opc_inst, IR, tmp_data);
            break;
        //  End BIT
        //
        //  Start Branch
        case CLEM_OPC_BCC:
            _clem_read_pba(clem, &tmp_data, &tmp_pc);
            _clem_branch(clem, &tmp_pc, tmp_data, !carry);
            _opcode_instruction_define(&opc_inst, IR, tmp_data, false);
            break;
        case CLEM_OPC_BCS:
            _clem_read_pba(clem, &tmp_data, &tmp_pc);
            _clem_branch(clem, &tmp_pc, tmp_data, carry);
            _opcode_instruction_define(&opc_inst, IR, tmp_data, false);
            break;
        case CLEM_OPC_BEQ:
            _clem_read_pba(clem, &tmp_data, &tmp_pc);
            _clem_branch(clem, &tmp_pc, tmp_data, zero_flag);
            _opcode_instruction_define(&opc_inst, IR, tmp_data, false);
            break;
        case CLEM_OPC_BMI:
            _clem_read_pba(clem, &tmp_data, &tmp_pc);
            _clem_branch(
                clem, &tmp_pc, tmp_data, cpu->regs.P & kClemensCPUStatus_Negative);
            _opcode_instruction_define(&opc_inst, IR, tmp_data, false);
            break;
        case CLEM_OPC_BNE:
            _clem_read_pba(clem, &tmp_data, &tmp_pc);
            _clem_branch(clem, &tmp_pc, tmp_data, !zero_flag);
            _opcode_instruction_define(&opc_inst, IR, tmp_data, false);
            break;
        case CLEM_OPC_BPL:
            _clem_read_pba(clem, &tmp_data, &tmp_pc);
            _clem_branch(
                clem, &tmp_pc, tmp_data, !(cpu->regs.P & kClemensCPUStatus_Negative));
            _opcode_instruction_define(&opc_inst, IR, tmp_data, false);
            break;
        case CLEM_OPC_BRA:
            _clem_read_pba(clem, &tmp_data, &tmp_pc);
            _clem_branch(clem, &tmp_pc, tmp_data, true);
            _opcode_instruction_define(&opc_inst, IR, tmp_data, false);
            break;
        case CLEM_OPC_BRL:
            _clem_read_pba_16(clem, &tmp_value, &tmp_pc);
            tmp_addr = tmp_pc + (int16_t)tmp_value;
            _clem_cycle(clem, 1);
            tmp_pc = tmp_addr;
            _opcode_instruction_define(&opc_inst, IR, tmp_addr, false);
            break;
        case CLEM_OPC_BVC:
            _clem_read_pba(clem, &tmp_data, &tmp_pc);
            _clem_branch(
                clem, &tmp_pc, tmp_data, !(cpu->regs.P & kClemensCPUStatus_Overflow));
            _opcode_instruction_define(&opc_inst, IR, tmp_data, false);
            break;
        case CLEM_OPC_BVS:
            _clem_read_pba(clem, &tmp_data, &tmp_pc);
            _clem_branch(
                clem, &tmp_pc, tmp_data, cpu->regs.P & kClemensCPUStatus_Overflow);
            _opcode_instruction_define(&opc_inst, IR, tmp_data, false);
            break;
        //  End Branch
        //
        case CLEM_OPC_CLC:
            cpu->regs.P &= ~kClemensCPUStatus_Carry;
            _clem_cycle(clem, 1);
            break;
        case CLEM_OPC_CLD:
            cpu->regs.P &= ~kClemensCPUStatus_Decimal;
            _clem_cycle(clem, 1);
            break;
        case CLEM_OPC_CLI:
            cpu->regs.P &= ~kClemensCPUStatus_IRQDisable;
            _clem_cycle(clem, 1);
            break;
        case CLEM_OPC_CLV:
            cpu->regs.P &= ~kClemensCPUStatus_Overflow;
            _clem_cycle(clem, 1);
            break;
        //
        //  Start CMP
        case CLEM_OPC_CMP_IMM:
            _clem_read_pba_mode_imm_816(clem, &tmp_value, &tmp_pc, m_status);
            _cpu_cmp(cpu, cpu->regs.A, tmp_value, m_status);
            _opcode_instruction_define(&opc_inst, IR, tmp_value, m_status);
            break;
        case CLEM_OPC_CMP_ABS:
            _clem_read_pba_mode_abs(clem, &tmp_addr, &tmp_pc);
            _clem_read_data_816(clem, &tmp_value, tmp_addr, cpu->regs.DBR, m_status);
            _cpu_cmp(cpu, cpu->regs.A, tmp_value, m_status);
            _opcode_instruction_define(&opc_inst, IR, tmp_addr, m_status);
            break;
        case CLEM_OPC_CMP_ABSL:
            //  TODO: emulation mode
            _clem_read_pba_mode_absl(clem, &tmp_addr, &tmp_bnk0, &tmp_pc);
            _clem_read_data_816(clem, &tmp_value, tmp_addr, tmp_bnk0, m_status);
            _cpu_cmp(cpu, cpu->regs.A, tmp_value, m_status);
            _opcode_instruction_define_long(&opc_inst, IR, tmp_bnk0, tmp_addr);
            break;
        case CLEM_OPC_CMP_DP:
            _clem_read_pba_mode_dp(clem, &tmp_addr, &tmp_pc, &tmp_data, 0, false);
            _clem_read_data_816(clem, &tmp_value, tmp_addr, 0x00, m_status);
            _cpu_cmp(cpu, cpu->regs.A, tmp_value, m_status);
            _opcode_instruction_define_dp(&opc_inst, IR, tmp_data);
            break;
        case CLEM_OPC_CMP_DP_INDIRECT:
            _clem_read_pba_mode_dp_indirect(clem, &tmp_addr, &tmp_pc, &tmp_data, 0, false);
            _clem_read_data_816(clem, &tmp_value, tmp_addr, cpu->regs.DBR, m_status);
            _cpu_cmp(cpu, cpu->regs.A, tmp_value, m_status);
            _opcode_instruction_define_dp(&opc_inst, IR, tmp_data);
            break;
        case CLEM_OPC_CMP_DP_INDIRECTL:
            _clem_read_pba_mode_dp_indirectl(
                clem, &tmp_addr, &tmp_bnk0, &tmp_pc, &tmp_data, 0, false);
            _clem_read_data_816(clem, &tmp_value, tmp_addr, tmp_bnk0, m_status);
            _cpu_cmp(cpu, cpu->regs.A, tmp_value, m_status);
            _opcode_instruction_define_dp(&opc_inst, IR, tmp_data);
            break;
        case CLEM_OPC_CMP_ABS_IDX:
            _clem_read_pba_mode_abs(clem, &tmp_addr, &tmp_pc);
            _clem_read_data_indexed_816(
                clem, &tmp_value, tmp_addr, cpu->regs.X, cpu->regs.DBR,
                m_status, x_status);
            _cpu_cmp(cpu, cpu->regs.A, tmp_value, m_status);
            _opcode_instruction_define(&opc_inst, IR, tmp_addr, m_status);
            break;
        case CLEM_OPC_CMP_ABSL_IDX:
            _clem_read_pba_mode_absl(clem, &tmp_addr, &tmp_bnk0, &tmp_pc);
            _clem_read_data_indexed_816(
                clem, &tmp_value, tmp_addr, cpu->regs.X, tmp_bnk0, m_status, x_status);
            _cpu_cmp(cpu, cpu->regs.A, tmp_value, m_status);
            _opcode_instruction_define_long(&opc_inst, IR, tmp_bnk0, tmp_addr);
            break;
        case CLEM_OPC_CMP_ABS_IDY:      // $addr + Y
            _clem_read_pba_mode_abs(clem, &tmp_addr, &tmp_pc);
            _clem_read_data_indexed_816(clem, &tmp_value, tmp_addr, cpu->regs.Y,
                cpu->regs.DBR, m_status, x_status);
            _cpu_cmp(cpu, cpu->regs.A, tmp_value, m_status);
            _opcode_instruction_define(&opc_inst, IR, tmp_addr, m_status);
            break;
        case CLEM_OPC_CMP_DP_IDX:
            _clem_read_pba_mode_dp(clem, &tmp_addr, &tmp_pc, &tmp_data,
                cpu->regs.X, x_status);
            _clem_cycle(clem, 1);             // extra IO cycle for d,x
            _clem_read_data_816(clem, &tmp_value, tmp_addr, 0x00, m_status);
            _cpu_cmp(cpu, cpu->regs.A, tmp_value, m_status);
            _opcode_instruction_define_dp(&opc_inst, IR, tmp_data);
            break;
        case CLEM_OPC_CMP_DP_IDX_INDIRECT:
            _clem_read_pba_mode_dp_indirect(clem, &tmp_addr, &tmp_pc, &tmp_data,
                cpu->regs.X, x_status);
            _clem_cycle(clem, 1);       // extra IO for (d, X)
            _clem_read_data_816(clem, &tmp_value, tmp_addr, cpu->regs.DBR, m_status);
            _cpu_cmp(cpu, cpu->regs.A, tmp_value, m_status);
            _opcode_instruction_define_dp(&opc_inst, IR, tmp_data);
            break;
        case CLEM_OPC_CMP_DP_INDIRECT_IDY:
            _clem_read_pba_mode_dp_indirect(clem, &tmp_addr, &tmp_pc, &tmp_data, 0, false);
            _clem_read_data_indexed_816(
                clem, &tmp_value, tmp_addr, cpu->regs.Y, cpu->regs.DBR, m_status, x_status);
            _cpu_cmp(cpu, cpu->regs.A, tmp_value, m_status);
            _opcode_instruction_define_dp(&opc_inst, IR, tmp_data);
            break;
        case CLEM_OPC_CMP_DP_INDIRECTL_IDY:
            _clem_read_pba_mode_dp_indirectl(clem, &tmp_addr, &tmp_bnk0, &tmp_pc,
                &tmp_data, 0, false);
            _clem_read_data_indexed_816(
                clem, &tmp_value, tmp_addr, cpu->regs.Y, tmp_bnk0, m_status, x_status);
            _cpu_cmp(cpu, cpu->regs.A, tmp_value, m_status);
            _opcode_instruction_define_dp(&opc_inst, IR, tmp_data);
            break;
        case CLEM_OPC_CMP_STACK_REL:
            _clem_read_pba_mode_stack_rel(clem, &tmp_addr, &tmp_pc, &tmp_data);
            _clem_read_data_816(clem, &tmp_value, tmp_addr, 0x00, m_status);
            _cpu_cmp(cpu, cpu->regs.A, tmp_value, m_status);
            _opcode_instruction_define(&opc_inst, IR, tmp_data, m_status);
            break;
        case CLEM_OPC_CMP_STACK_REL_INDIRECT_IDY:
            _clem_read_pba_mode_stack_rel_indirect(clem, &tmp_addr, &tmp_pc, &tmp_data);
            _clem_read_data_indexed_816(
                clem, &tmp_value, tmp_addr, cpu->regs.Y, cpu->regs.DBR, m_status, x_status);
            _cpu_cmp(cpu, cpu->regs.A, tmp_value, m_status);
            _opcode_instruction_define(&opc_inst, IR, tmp_data, m_status);
            break;
        //  End CMP
        //
        case CLEM_OPC_CPX_IMM:
            _clem_read_pba_mode_imm_816(clem, &tmp_value, &tmp_pc, x_status);
            _cpu_cmp(cpu, cpu->regs.X, tmp_value, x_status);
            _opcode_instruction_define(&opc_inst, IR, tmp_value, x_status);
            break;
        case CLEM_OPC_CPX_ABS:
            _clem_read_pba_mode_abs(clem, &tmp_addr, &tmp_pc);
            _clem_read_data_816(clem, &tmp_value, tmp_addr, cpu->regs.DBR, x_status);
            _cpu_cmp(cpu, cpu->regs.X, tmp_value, x_status);
            _opcode_instruction_define(&opc_inst, IR, tmp_addr, x_status);
            break;
        case CLEM_OPC_CPX_DP:
            _clem_read_pba_mode_dp(clem, &tmp_addr, &tmp_pc, &tmp_data, 0, x_status);
            _clem_read_data_816(clem, &tmp_value, tmp_addr, 0x00, x_status);
            _cpu_cmp(cpu, cpu->regs.X, tmp_value, x_status);
            _opcode_instruction_define_dp(&opc_inst, IR, tmp_data);
            break;
        case CLEM_OPC_CPY_IMM:
            _clem_read_pba_mode_imm_816(clem, &tmp_value, &tmp_pc, x_status);
            _cpu_cmp(cpu, cpu->regs.Y, tmp_value, x_status);
            _opcode_instruction_define(&opc_inst, IR, tmp_value, x_status);
            break;
        case CLEM_OPC_CPY_ABS:
            _clem_read_pba_mode_abs(clem, &tmp_addr, &tmp_pc);
            _clem_read_data_816(clem, &tmp_value, tmp_addr, cpu->regs.DBR, x_status);
            _cpu_cmp(cpu, cpu->regs.Y, tmp_value, x_status);
            _opcode_instruction_define(&opc_inst, IR, tmp_addr, x_status);
            break;
        case CLEM_OPC_CPY_DP:
            _clem_read_pba_mode_dp(clem, &tmp_addr, &tmp_pc, &tmp_data, 0, x_status);
            _clem_read_data_816(clem, &tmp_value, tmp_addr, 0x00, x_status);
            _cpu_cmp(cpu, cpu->regs.Y, tmp_value, x_status);
            _opcode_instruction_define_dp(&opc_inst, IR, tmp_data);
            break;
        //
        //  Start DEC
        case CLEM_OPC_DEC_A:
            _cpu_dec(cpu, &cpu->regs.A, m_status);
            _clem_cycle(clem, 1);
            break;
        case CLEM_OPC_DEC_ABS:
            _clem_read_pba_mode_abs(clem, &tmp_addr, &tmp_pc);
            _clem_read_data_816(clem, &tmp_value, tmp_addr, cpu->regs.DBR, m_status);
            _cpu_dec(cpu, &tmp_value, m_status);
            _clem_cycle(clem, 1);
            _clem_write_816(clem, tmp_value, tmp_addr, cpu->regs.DBR, m_status);
            _opcode_instruction_define(&opc_inst, IR, tmp_addr, m_status);
            break;
        case CLEM_OPC_DEC_DP:
            _clem_read_pba_mode_dp(clem, &tmp_addr, &tmp_pc, &tmp_data, 0, false);
            _clem_read_data_816(clem, &tmp_value, tmp_addr, 0x00, m_status);
            _cpu_dec(cpu, &tmp_value, m_status);
            _clem_cycle(clem, 1);
            _clem_write_816(clem, tmp_value, tmp_addr, 0x00, m_status);
            _opcode_instruction_define_dp(&opc_inst, IR, tmp_data);
            break;
        case CLEM_OPC_DEC_ABS_IDX:
            _clem_read_pba_mode_abs(clem, &tmp_addr, &tmp_pc);
            _clem_cycle(clem, 1);
            _clem_read_data_indexed_816(
                clem, &tmp_value, tmp_addr, cpu->regs.X, cpu->regs.DBR,
                m_status, x_status);
            _cpu_dec(cpu, &tmp_value, m_status);
            _clem_cycle(clem, 1);
            _clem_write_indexed_816(clem, tmp_value, tmp_addr, cpu->regs.X,
                cpu->regs.DBR, m_status, x_status);
            _opcode_instruction_define(&opc_inst, IR, tmp_addr, m_status);
            break;
        case CLEM_OPC_DEC_ABS_DP_IDX:
            _clem_read_pba_mode_dp(clem, &tmp_addr, &tmp_pc, &tmp_data,
                cpu->regs.X, x_status);
            _clem_cycle(clem, 1);             // extra IO cycle for d,x
            _clem_read_data_816(clem, &tmp_value, tmp_addr, 0x00, m_status);
            _cpu_dec(cpu, &tmp_value, m_status);
            _clem_cycle(clem, 1);
            _clem_write_816(clem, tmp_value, tmp_addr, 0x00, m_status);
            _opcode_instruction_define_dp(&opc_inst, IR, tmp_data);
            break;
        //  End DEC
        //
        case CLEM_OPC_DEX:
            tmp_value = cpu->regs.X - 1;
            if (x_status) {
                cpu->regs.X = CLEM_UTIL_set16_lo(cpu->regs.X, tmp_value);
                _cpu_p_flags_n_z_data(cpu, (uint8_t)tmp_value);
            } else {
                cpu->regs.X = tmp_value;
                _cpu_p_flags_n_z_data_16(cpu, tmp_value);
            }
            break;
        case CLEM_OPC_DEY:
            tmp_value = cpu->regs.Y - 1;
            if (x_status) {
                cpu->regs.Y = CLEM_UTIL_set16_lo(cpu->regs.Y, tmp_value);
                _cpu_p_flags_n_z_data(cpu, (uint8_t)tmp_value);
            } else {
                cpu->regs.Y = tmp_value;
                _cpu_p_flags_n_z_data_16(cpu, tmp_value);
            }
            break;
        //
        //  Start EOR
        case CLEM_OPC_EOR_IMM:
            _clem_read_pba_mode_imm_816(clem, &tmp_value, &tmp_pc, m_status);
            _cpu_eor(cpu, tmp_value, m_status);
            _opcode_instruction_define(&opc_inst, IR, tmp_value, m_status);
            break;
        case CLEM_OPC_EOR_ABS:
            _clem_read_pba_mode_abs(clem, &tmp_addr, &tmp_pc);
            _clem_read_data_816(clem, &tmp_value, tmp_addr, cpu->regs.DBR, m_status);
            _cpu_eor(cpu, tmp_value, m_status);
            _opcode_instruction_define(&opc_inst, IR, tmp_addr, m_status);
            break;
        case CLEM_OPC_EOR_ABSL:
            //  TODO: emulation mode
            _clem_read_pba_mode_absl(clem, &tmp_addr, &tmp_bnk0, &tmp_pc);
            _clem_read_data_816(clem, &tmp_value, tmp_addr, tmp_bnk0, m_status);
            _cpu_eor(cpu, tmp_value, m_status);
            _opcode_instruction_define_long(&opc_inst, IR, tmp_bnk0, tmp_addr);
            break;
        case CLEM_OPC_EOR_DP:
            _clem_read_pba_mode_dp(clem, &tmp_addr, &tmp_pc, &tmp_data, 0, false);
            _clem_read_data_816(clem, &tmp_value, tmp_addr, 0x00, m_status);
            _cpu_eor(cpu, tmp_value, m_status);
            _opcode_instruction_define_dp(&opc_inst, IR, tmp_data);
            break;
        case CLEM_OPC_EOR_DP_INDIRECT:
            _clem_read_pba_mode_dp_indirect(clem, &tmp_addr, &tmp_pc, &tmp_data, 0, false);
            _clem_read_data_816(clem, &tmp_value, tmp_addr, cpu->regs.DBR, m_status);
            _cpu_eor(cpu, tmp_value, m_status);
            _opcode_instruction_define_dp(&opc_inst, IR, tmp_data);
            break;
        case CLEM_OPC_EOR_DP_INDIRECTL:
            _clem_read_pba_mode_dp_indirectl(
                clem, &tmp_addr, &tmp_bnk0, &tmp_pc, &tmp_data, 0, false);
            _clem_read_data_816(clem, &tmp_value, tmp_addr, tmp_bnk0, m_status);
            _cpu_eor(cpu, tmp_value, m_status);
            _opcode_instruction_define_dp(&opc_inst, IR, tmp_data);
            break;
        case CLEM_OPC_EOR_ABS_IDX:
            _clem_read_pba_mode_abs(clem, &tmp_addr, &tmp_pc);
            _clem_read_data_indexed_816(
                clem, &tmp_value, tmp_addr, cpu->regs.X, cpu->regs.DBR,
                m_status, x_status);
            _cpu_eor(cpu, tmp_value, m_status);
            _opcode_instruction_define(&opc_inst, IR, tmp_addr, m_status);
            break;
        case CLEM_OPC_EOR_ABSL_IDX:
            _clem_read_pba_mode_absl(clem, &tmp_addr, &tmp_bnk0, &tmp_pc);
            _clem_read_data_indexed_816(
                clem, &tmp_value, tmp_addr, cpu->regs.X, tmp_bnk0, m_status, x_status);
            _cpu_eor(cpu, tmp_value, m_status);
            _opcode_instruction_define_long(&opc_inst, IR, tmp_bnk0, tmp_addr);
            break;
        case CLEM_OPC_EOR_ABS_IDY:      // $addr + Y
            _clem_read_pba_mode_abs(clem, &tmp_addr, &tmp_pc);
            _clem_read_data_indexed_816(clem, &tmp_value, tmp_addr, cpu->regs.Y,
                cpu->regs.DBR, m_status, x_status);
            _cpu_eor(cpu, tmp_value, m_status);
            _opcode_instruction_define(&opc_inst, IR, tmp_addr, m_status);
            break;
        case CLEM_OPC_EOR_DP_IDX:
            _clem_read_pba_mode_dp(clem, &tmp_addr, &tmp_pc, &tmp_data,
                cpu->regs.X, x_status);
            _clem_cycle(clem, 1);             // extra IO cycle for d,x
            _clem_read_data_816(clem, &tmp_value, tmp_addr, 0x00, m_status);
            _cpu_eor(cpu, tmp_value, m_status);
            _opcode_instruction_define_dp(&opc_inst, IR, tmp_data);
            break;
        case CLEM_OPC_EOR_DP_IDX_INDIRECT:
            _clem_read_pba_mode_dp_indirect(clem, &tmp_addr, &tmp_pc, &tmp_data,
                cpu->regs.X, x_status);
            _clem_cycle(clem, 1);       // extra IO for (d, X)
            _clem_read_data_816(clem, &tmp_value, tmp_addr, cpu->regs.DBR, m_status);
            _cpu_eor(cpu, tmp_value, m_status);
            _opcode_instruction_define_dp(&opc_inst, IR, tmp_data);
            break;
        case CLEM_OPC_EOR_DP_INDIRECT_IDY:
            _clem_read_pba_mode_dp_indirect(clem, &tmp_addr, &tmp_pc, &tmp_data, 0, false);
            _clem_read_data_indexed_816(
                clem, &tmp_value, tmp_addr, cpu->regs.Y, cpu->regs.DBR, m_status, x_status);
            _cpu_eor(cpu, tmp_value, m_status);
            _opcode_instruction_define_dp(&opc_inst, IR, tmp_data);
            break;
        case CLEM_OPC_EOR_DP_INDIRECTL_IDY:
            _clem_read_pba_mode_dp_indirectl(clem, &tmp_addr, &tmp_bnk0, &tmp_pc,
                &tmp_data, 0, false);
            _clem_read_data_indexed_816(
                clem, &tmp_value, tmp_addr, cpu->regs.Y, tmp_bnk0, m_status, x_status);
            _cpu_eor(cpu, tmp_value, m_status);
            _opcode_instruction_define_dp(&opc_inst, IR, tmp_data);
            break;
        case CLEM_OPC_EOR_STACK_REL:
            _clem_read_pba_mode_stack_rel(clem, &tmp_addr, &tmp_pc, &tmp_data);
            _clem_read_data_816(clem, &tmp_value, tmp_addr, 0x00, m_status);
            _cpu_eor(cpu, tmp_value, m_status);
            _opcode_instruction_define(&opc_inst, IR, tmp_data, m_status);
            break;
        case CLEM_OPC_EOR_STACK_REL_INDIRECT_IDY:
            _clem_read_pba_mode_stack_rel_indirect(clem, &tmp_addr, &tmp_pc, &tmp_data);
            _clem_read_data_indexed_816(
                clem, &tmp_value, tmp_addr, cpu->regs.Y, cpu->regs.DBR, m_status, x_status);
            _cpu_eor(cpu, tmp_value, m_status);
            _opcode_instruction_define(&opc_inst, IR, tmp_data, m_status);
            break;
        //  End EOR
        //
        //  Start INC
        case CLEM_OPC_INC_A:
            _cpu_inc(cpu, &cpu->regs.A, m_status);
            _clem_cycle(clem, 1);
            break;
        case CLEM_OPC_INC_ABS:
            _clem_read_pba_mode_abs(clem, &tmp_addr, &tmp_pc);
            _clem_read_data_816(clem, &tmp_value, tmp_addr, cpu->regs.DBR, m_status);
            _cpu_inc(cpu, &tmp_value, m_status);
            _clem_cycle(clem, 1);
            _clem_write_816(clem, tmp_value, tmp_addr, cpu->regs.DBR, m_status);
            _opcode_instruction_define(&opc_inst, IR, tmp_addr, m_status);
            break;
        case CLEM_OPC_INC_DP:
            _clem_read_pba_mode_dp(clem, &tmp_addr, &tmp_pc, &tmp_data, 0, false);
            _clem_read_data_816(clem, &tmp_value, tmp_addr, 0x00, m_status);
            _cpu_inc(cpu, &tmp_value, m_status);
            _clem_cycle(clem, 1);
            _clem_write_816(clem, tmp_value, tmp_addr, 0x00, m_status);
            _opcode_instruction_define_dp(&opc_inst, IR, tmp_data);
            break;
        case CLEM_OPC_INC_ABS_IDX:
            _clem_read_pba_mode_abs(clem, &tmp_addr, &tmp_pc);
            _clem_cycle(clem, 1);
            _clem_read_data_indexed_816(
                clem, &tmp_value, tmp_addr, cpu->regs.X, cpu->regs.DBR,
                m_status, x_status);
            _cpu_inc(cpu, &tmp_value, m_status);
            _clem_cycle(clem, 1);
            _clem_write_indexed_816(clem, tmp_value, tmp_addr, cpu->regs.X,
                cpu->regs.DBR, m_status, x_status);
            _opcode_instruction_define(&opc_inst, IR, tmp_addr, m_status);
            break;
        case CLEM_OPC_INC_ABS_DP_IDX:
            _clem_read_pba_mode_dp(clem, &tmp_addr, &tmp_pc, &tmp_data,
                cpu->regs.X, x_status);
            _clem_cycle(clem, 1);             // extra IO cycle for d,x
            _clem_read_data_816(clem, &tmp_value, tmp_addr, 0x00, m_status);
            _cpu_inc(cpu, &tmp_value, m_status);
            _clem_cycle(clem, 1);
            _clem_write_816(clem, tmp_value, tmp_addr, 0x00, m_status);
            _opcode_instruction_define_dp(&opc_inst, IR, tmp_data);
            break;
        //  End INC
        //
        case CLEM_OPC_INX:
            tmp_value = cpu->regs.X + 1;
            if (x_status) {
                cpu->regs.X = CLEM_UTIL_set16_lo(cpu->regs.X, tmp_value);
                _cpu_p_flags_n_z_data(cpu, (uint8_t)tmp_value);
            } else {
                cpu->regs.X = tmp_value;
                _cpu_p_flags_n_z_data_16(cpu, tmp_value);
            }
            break;
        case CLEM_OPC_INY:
            tmp_value = cpu->regs.Y + 1;
            if (x_status) {
                cpu->regs.Y = CLEM_UTIL_set16_lo(cpu->regs.Y, tmp_value);
                _cpu_p_flags_n_z_data(cpu, (uint8_t)tmp_value);
            } else {
                cpu->regs.Y = tmp_value;
                _cpu_p_flags_n_z_data_16(cpu, tmp_value);
            }
            break;
        //
        //  Start JMP
        case CLEM_OPC_JMP_ABS:
            _clem_read_pba_16(clem, &tmp_addr, &tmp_pc);
            tmp_pc = tmp_addr;
            _opcode_instruction_define(&opc_inst, IR, tmp_addr, m_status);
            break;
        case CLEM_OPC_JMP_INDIRECT:
            _clem_read_pba_16(clem, &tmp_addr, &tmp_pc);
            _clem_read_16(clem, &tmp_eaddr, tmp_addr, 0x00, CLEM_MEM_FLAG_DATA);
            tmp_pc = tmp_eaddr;
            _opcode_instruction_define(&opc_inst, IR, tmp_addr, m_status);
            break;
        case CLEM_OPC_JMP_INDIRECT_IDX:
            _clem_read_pba_16(clem, &tmp_addr, &tmp_pc);
            if (x_status) {
                tmp_eaddr = tmp_addr + (cpu->regs.X & 0x00ff);
            } else {
                tmp_eaddr = tmp_addr + cpu->regs.X;
            }
            _clem_cycle(clem, 1);
            _clem_read_16(clem, &tmp_eaddr, tmp_addr, cpu->regs.PBR, CLEM_MEM_FLAG_DATA);
            tmp_pc = tmp_eaddr;
            _opcode_instruction_define(&opc_inst, IR, tmp_addr, x_status);
            break;
        case CLEM_OPC_JMP_ABSL:
            _clem_read_pba_16(clem, &tmp_addr, &tmp_pc);
            _clem_read_pba(clem, &tmp_bnk0, &tmp_pc);
            tmp_pc = tmp_addr;
            cpu->regs.PBR = tmp_bnk0;
            _opcode_instruction_define_long(&opc_inst, IR, tmp_bnk0, tmp_addr);
            break;
        case CLEM_OPC_JMP_ABSL_INDIRECT:
            _clem_read_pba_16(clem, &tmp_addr, &tmp_pc);
            _clem_read_16(clem, &tmp_eaddr, tmp_addr, 0x00, CLEM_MEM_FLAG_DATA);
            clem_read(clem, &tmp_bnk0, tmp_addr + 2, 0x00, CLEM_MEM_FLAG_DATA);
            tmp_pc = tmp_eaddr;
            cpu->regs.PBR = tmp_bnk0;
            _opcode_instruction_define(&opc_inst, IR, tmp_addr, m_status);
            break;
        //  End JMP
        //
        //  Start LDA
        case CLEM_OPC_LDA_IMM:
            _clem_read_pba_mode_imm_816(clem, &tmp_value, &tmp_pc, m_status);
            _cpu_lda(cpu, tmp_value, m_status);
            _opcode_instruction_define(&opc_inst, IR, tmp_value, m_status);
            break;
        case CLEM_OPC_LDA_ABS:
            _clem_read_pba_mode_abs(clem, &tmp_addr, &tmp_pc);
            _clem_read_data_816(clem, &tmp_value, tmp_addr, cpu->regs.DBR, m_status);
            _cpu_lda(cpu, tmp_value, m_status);
            _opcode_instruction_define(&opc_inst, IR, tmp_addr, m_status);
            break;
        case CLEM_OPC_LDA_ABSL:
            //  TODO: emulation mode
            _clem_read_pba_mode_absl(clem, &tmp_addr, &tmp_bnk0, &tmp_pc);
            _clem_read_data_816(clem, &tmp_value, tmp_addr, tmp_bnk0, m_status);
            _cpu_lda(cpu, tmp_value, m_status);
            _opcode_instruction_define_long(&opc_inst, IR, tmp_bnk0, tmp_addr);
            break;
        case CLEM_OPC_LDA_DP:
            _clem_read_pba_mode_dp(clem, &tmp_addr, &tmp_pc, &tmp_data, 0, false);
            _clem_read_data_816(clem, &tmp_value, tmp_addr, 0x00, m_status);
            _cpu_lda(cpu, tmp_value, m_status);
            _opcode_instruction_define_dp(&opc_inst, IR, tmp_data);
            break;
        case CLEM_OPC_LDA_DP_INDIRECT:
            _clem_read_pba_mode_dp_indirect(clem, &tmp_addr, &tmp_pc, &tmp_data, 0, false);
            _clem_read_data_816(clem, &tmp_value, tmp_addr, cpu->regs.DBR, m_status);
            _cpu_lda(cpu, tmp_value, m_status);
            _opcode_instruction_define_dp(&opc_inst, IR, tmp_data);
            break;
        case CLEM_OPC_LDA_DP_INDIRECTL:
            _clem_read_pba_mode_dp_indirectl(clem, &tmp_addr, &tmp_bnk0, &tmp_pc,
                &tmp_data, 0, false);
            _clem_read_data_816(clem, &tmp_value, tmp_addr, tmp_bnk0, m_status);
            _cpu_lda(cpu, tmp_value, m_status);
            _opcode_instruction_define_dp(&opc_inst, IR, tmp_data);
            break;
        case CLEM_OPC_LDA_ABS_IDX:
            _clem_read_pba_mode_abs(clem, &tmp_addr, &tmp_pc);
            _clem_read_data_indexed_816(
                clem, &tmp_value, tmp_addr, cpu->regs.X, cpu->regs.DBR, m_status, x_status);
            _cpu_lda(cpu, tmp_value, m_status);
            _opcode_instruction_define(&opc_inst, IR, tmp_addr, m_status);
            break;
        case CLEM_OPC_LDA_ABSL_IDX:
            _clem_read_pba_mode_absl(clem, &tmp_addr, &tmp_bnk0, &tmp_pc);
            _clem_read_data_indexed_816(
                clem, &tmp_value, tmp_addr, cpu->regs.X, tmp_bnk0, m_status, x_status);
            _cpu_lda(cpu, tmp_value, m_status);
            _opcode_instruction_define_long(&opc_inst, IR, tmp_bnk0, tmp_addr);
            break;
        case CLEM_OPC_LDA_ABS_IDY:      // $addr + Y
            _clem_read_pba_mode_abs(clem, &tmp_addr, &tmp_pc);
            _clem_read_data_indexed_816(
                clem, &tmp_value, tmp_addr, cpu->regs.Y, cpu->regs.DBR, m_status, x_status);
            _cpu_lda(cpu, tmp_value, m_status);
            _opcode_instruction_define(&opc_inst, IR, tmp_addr, m_status);
            break;
        case CLEM_OPC_LDA_DP_IDX:
            _clem_read_pba_mode_dp(clem, &tmp_addr, &tmp_pc, &tmp_data, cpu->regs.X, x_status);
            _clem_cycle(clem, 1);             // extra IO cycle for d,x
            _clem_read_data_816(clem, &tmp_value, tmp_addr, 0x00, m_status);
            _cpu_lda(cpu, tmp_value, m_status);
            _opcode_instruction_define_dp(&opc_inst, IR, tmp_data);
            break;
        case CLEM_OPC_LDA_DP_IDX_INDIRECT:
            _clem_read_pba_mode_dp_indirect(clem, &tmp_addr, &tmp_pc, &tmp_data,
                cpu->regs.X, x_status);
            _clem_cycle(clem, 1);       // extra IO for (d, X)
            _clem_read_data_816(clem, &tmp_value, tmp_addr, cpu->regs.DBR, m_status);
            _cpu_lda(cpu, tmp_value, m_status);
            _opcode_instruction_define_dp(&opc_inst, IR, tmp_data);
            break;
        case CLEM_OPC_LDA_DP_INDIRECT_IDY:
            _clem_read_pba_mode_dp_indirect(clem, &tmp_addr, &tmp_pc, &tmp_data, 0, false);
            _clem_read_data_indexed_816(
                clem, &tmp_value, tmp_addr, cpu->regs.Y, cpu->regs.DBR, m_status, x_status);
            _cpu_lda(cpu, tmp_value, m_status);
            _opcode_instruction_define_dp(&opc_inst, IR, tmp_data);
            break;
        case CLEM_OPC_LDA_DP_INDIRECTL_IDY:
            _clem_read_pba_mode_dp_indirectl(clem, &tmp_addr, &tmp_bnk0, &tmp_pc,
                &tmp_data, 0, false);
            _clem_read_data_indexed_816(
                clem, &tmp_value, tmp_addr, cpu->regs.Y, tmp_bnk0, m_status, x_status);
            _cpu_lda(cpu, tmp_value, m_status);
            _opcode_instruction_define_dp(&opc_inst, IR, tmp_data);
            break;
        case CLEM_OPC_LDA_STACK_REL:
            _clem_read_pba_mode_stack_rel(clem, &tmp_addr, &tmp_pc, &tmp_data);
            _clem_read_data_816(
                clem, &tmp_value, tmp_addr, 0x00, m_status);
            _cpu_lda(cpu, tmp_value, m_status);
            _opcode_instruction_define(&opc_inst, IR, tmp_data, m_status);
            break;
        case CLEM_OPC_LDA_STACK_REL_INDIRECT_IDY:
            _clem_read_pba_mode_stack_rel_indirect(clem, &tmp_addr, &tmp_pc, &tmp_data);
            _clem_read_data_indexed_816(
                clem, &tmp_value, tmp_addr, cpu->regs.Y, cpu->regs.DBR, m_status, x_status);
            _cpu_lda(cpu, tmp_value, m_status);
            _opcode_instruction_define(&opc_inst, IR, tmp_data, m_status);
            break;
        //  End LDA
        //
        case CLEM_OPC_LDX_IMM:
            _clem_read_pba_816(clem, &tmp_value, &tmp_pc, x_status);
            _cpu_ldxy(cpu, &cpu->regs.X, tmp_value, x_status);
            _opcode_instruction_define(&opc_inst, IR, tmp_value, x_status);
            break;
        case CLEM_OPC_LDX_ABS:
            _clem_read_pba_mode_abs(clem, &tmp_addr, &tmp_pc);
            _clem_read_data_816(clem, &tmp_value, tmp_addr, cpu->regs.DBR, x_status);
            _cpu_ldxy(cpu, &cpu->regs.X, tmp_value, x_status);
            _opcode_instruction_define(&opc_inst, IR, tmp_addr, x_status);
            break;
        case CLEM_OPC_LDX_DP:
            _clem_read_pba_mode_dp(clem, &tmp_addr, &tmp_pc, &tmp_data, 0, x_status);
            _clem_read_data_816(clem, &tmp_value, tmp_addr, 0x00, x_status);
            _cpu_ldxy(cpu, &cpu->regs.X, tmp_value, x_status);
            _opcode_instruction_define_dp(&opc_inst, IR, tmp_data);
            break;
        case CLEM_OPC_LDX_ABS_IDY:
            _clem_read_pba_mode_abs(clem, &tmp_addr, &tmp_pc);
            _clem_read_data_indexed_816(clem, &tmp_value, tmp_addr, cpu->regs.Y,
                cpu->regs.DBR, x_status, x_status);
            _cpu_ldxy(cpu, &cpu->regs.X, tmp_value, x_status);
            _opcode_instruction_define(&opc_inst, IR, tmp_addr, x_status);
            break;
        case CLEM_OPC_LDX_DP_IDY:
            _clem_read_pba_mode_dp(clem, &tmp_addr, &tmp_pc, &tmp_data,
                cpu->regs.Y, x_status);
            _clem_read_data_816(clem, &tmp_value, tmp_addr, 0x00, x_status);
            _cpu_ldxy(cpu, &cpu->regs.X, tmp_value, x_status);
            _opcode_instruction_define_dp(&opc_inst, IR, tmp_data);
            break;
        case CLEM_OPC_LDY_IMM:
            _clem_read_pba_mode_imm_816(clem, &tmp_value, &tmp_pc, x_status);
            _cpu_ldxy(cpu, &cpu->regs.Y, tmp_value, x_status);
            _opcode_instruction_define(&opc_inst, IR, tmp_value, x_status);
            break;
        case CLEM_OPC_LDY_ABS:
            _clem_read_pba_mode_abs(clem, &tmp_addr, &tmp_pc);
            _clem_read_data_816(clem, &tmp_value, tmp_addr, cpu->regs.DBR, x_status);
            _cpu_ldxy(cpu, &cpu->regs.Y, tmp_value, x_status);
            _opcode_instruction_define(&opc_inst, IR, tmp_addr, x_status);
            break;
        case CLEM_OPC_LDY_DP:
            _clem_read_pba_mode_dp(clem, &tmp_addr, &tmp_pc, &tmp_data, 0, x_status);
            _clem_read_data_816(clem, &tmp_value, tmp_addr, 0x00, x_status);
            _cpu_ldxy(cpu, &cpu->regs.Y, tmp_value, x_status);
            _opcode_instruction_define_dp(&opc_inst, IR, tmp_data);
            break;
        case CLEM_OPC_LDY_ABS_IDX:
            _clem_read_pba_mode_abs(clem, &tmp_addr, &tmp_pc);
            _clem_read_data_indexed_816(clem, &tmp_value, tmp_addr, cpu->regs.X,
                cpu->regs.DBR, x_status, x_status);
            _cpu_ldxy(cpu, &cpu->regs.Y, tmp_value, x_status);
            _opcode_instruction_define(&opc_inst, IR, tmp_addr, x_status);
            break;
        case CLEM_OPC_LDY_DP_IDX:
            _clem_read_pba_mode_dp(clem, &tmp_addr, &tmp_pc, &tmp_data,
                cpu->regs.X, x_status);
            _clem_read_data_816(clem, &tmp_value, tmp_addr, 0x00, x_status);
            _cpu_ldxy(cpu, &cpu->regs.Y, tmp_value, x_status);
            _opcode_instruction_define_dp(&opc_inst, IR, tmp_data);
            break;
        //
        //  Start ASL
        case CLEM_OPC_LSR_A:
            _cpu_lsr(cpu, &cpu->regs.A, m_status);
            _clem_cycle(clem, 1);
            break;
        case CLEM_OPC_LSR_ABS:
            _clem_read_pba_mode_abs(clem, &tmp_addr, &tmp_pc);
            _clem_read_data_816(clem, &tmp_value, tmp_addr, cpu->regs.DBR, m_status);
            _cpu_lsr(cpu, &tmp_value, m_status);
            _clem_cycle(clem, 1);
            _clem_write_816(clem, tmp_value, tmp_addr, cpu->regs.DBR, m_status);
            _opcode_instruction_define(&opc_inst, IR, tmp_addr, m_status);
            break;
        case CLEM_OPC_LSR_DP:
            _clem_read_pba_mode_dp(clem, &tmp_addr, &tmp_pc, &tmp_data, 0, false);
            _clem_read_data_816(clem, &tmp_value, tmp_addr, 0x00, m_status);
            _cpu_lsr(cpu, &tmp_value, m_status);
            _clem_cycle(clem, 1);
            _clem_write_816(clem, tmp_value, tmp_addr, 0x00, m_status);
            _opcode_instruction_define_dp(&opc_inst, IR, tmp_data);
            break;
        case CLEM_OPC_LSR_ABS_IDX:
            _clem_read_pba_mode_abs(clem, &tmp_addr, &tmp_pc);
            _clem_cycle(clem, 1);
            _clem_read_data_indexed_816(
                clem, &tmp_value, tmp_addr, cpu->regs.X, cpu->regs.DBR,
                m_status, x_status);
            _cpu_lsr(cpu, &tmp_value, m_status);
            _clem_cycle(clem, 1);
            _clem_write_indexed_816(clem, tmp_value, tmp_addr, cpu->regs.X,
                cpu->regs.DBR, m_status, x_status);
            _opcode_instruction_define(&opc_inst, IR, tmp_addr, m_status);
            break;
        case CLEM_OPC_LSR_ABS_DP_IDX:
            _clem_read_pba_mode_dp(clem, &tmp_addr, &tmp_pc, &tmp_data,
                cpu->regs.X, x_status);
            _clem_cycle(clem, 1);             // extra IO cycle for d,x
            _clem_read_data_816(clem, &tmp_value, tmp_addr, 0x00, m_status);
            _cpu_lsr(cpu, &tmp_value, m_status);
            _clem_cycle(clem, 1);
            _clem_write_816(clem, tmp_value, tmp_addr, 0x00, m_status);
            _opcode_instruction_define_dp(&opc_inst, IR, tmp_data);
            break;
        //  End LSR
        //
        case CLEM_OPC_MVN:
            //  copy X -> Y, incrementing X, Y, decrement C
            _clem_read_pba(clem, &tmp_bnk1, &tmp_pc);   // dest
            _clem_read_pba(clem, &tmp_bnk0, &tmp_pc);    // src
            clem_read(clem, &tmp_data, cpu->regs.X, tmp_bnk0, CLEM_MEM_FLAG_DATA);
            clem_write(clem, tmp_data, cpu->regs.Y, tmp_bnk1, CLEM_MEM_FLAG_DATA);
            if (x_status) {
                cpu->regs.X = CLEM_UTIL_set16_lo(cpu->regs.X, cpu->regs.X + 1);
                cpu->regs.Y = CLEM_UTIL_set16_lo(cpu->regs.Y, cpu->regs.Y + 1);
            } else {
                ++cpu->regs.X;
                ++cpu->regs.Y;
            }
            _clem_cycle(clem, 2);
            --cpu->regs.A;
            if (cpu->regs.A != 0xffff) {
                tmp_pc = cpu->regs.PC;  // repeat
            }
            break;
        case CLEM_OPC_MVP:
            //  copy X -> Y, decrementing X, Y, decrement C
            _clem_read_pba(clem, &tmp_bnk1, &tmp_pc);   // dest
            _clem_read_pba(clem, &tmp_bnk0, &tmp_pc);    // src
            clem_read(clem, &tmp_data, cpu->regs.X, tmp_bnk0, CLEM_MEM_FLAG_DATA);
            clem_write(clem, tmp_data, cpu->regs.Y, tmp_bnk1, CLEM_MEM_FLAG_DATA);
            if (x_status) {
                cpu->regs.X = CLEM_UTIL_set16_lo(cpu->regs.X, cpu->regs.X - 1);
                cpu->regs.Y = CLEM_UTIL_set16_lo(cpu->regs.Y, cpu->regs.Y - 1);
            } else {
                --cpu->regs.X;
                --cpu->regs.Y;
            }
            _clem_cycle(clem, 2);
            --cpu->regs.A;
            if (cpu->regs.A != 0xffff) {
                tmp_pc = cpu->regs.PC;  // repeat
            }
            break;
        case CLEM_OPC_NOP:
            _clem_cycle(clem, 1);
            break;
        //
        //  Start ORA
        case CLEM_OPC_ORA_IMM:
            _clem_read_pba_mode_imm_816(clem, &tmp_value, &tmp_pc, m_status);
            _cpu_ora(cpu, tmp_value, m_status);
            _opcode_instruction_define(&opc_inst, IR, tmp_value, m_status);
            break;
        case CLEM_OPC_ORA_ABS:
            _clem_read_pba_mode_abs(clem, &tmp_addr, &tmp_pc);
            _clem_read_data_816(clem, &tmp_value, tmp_addr, cpu->regs.DBR, m_status);
            _cpu_ora(cpu, tmp_value, m_status);
            _opcode_instruction_define(&opc_inst, IR, tmp_addr, m_status);
            break;
        case CLEM_OPC_ORA_ABSL:
            //  TODO: emulation mode
            _clem_read_pba_mode_absl(clem, &tmp_addr, &tmp_bnk0, &tmp_pc);
            _clem_read_data_816(clem, &tmp_value, tmp_addr, tmp_bnk0, m_status);
            _cpu_ora(cpu, tmp_value, m_status);
            _opcode_instruction_define_long(&opc_inst, IR, tmp_bnk0, tmp_addr);
            break;
        case CLEM_OPC_ORA_DP:
            _clem_read_pba_mode_dp(clem, &tmp_addr, &tmp_pc, &tmp_data, 0, false);
            _clem_read_data_816(clem, &tmp_value, tmp_addr, 0x00, m_status);
            _cpu_ora(cpu, tmp_value, m_status);
            _opcode_instruction_define_dp(&opc_inst, IR, tmp_data);
            break;
        case CLEM_OPC_ORA_DP_INDIRECT:
            _clem_read_pba_mode_dp_indirect(clem, &tmp_addr, &tmp_pc, &tmp_data, 0, false);
            _clem_read_data_816(clem, &tmp_value, tmp_addr, cpu->regs.DBR, m_status);
            _cpu_ora(cpu, tmp_value, m_status);
            _opcode_instruction_define_dp(&opc_inst, IR, tmp_data);
            break;
        case CLEM_OPC_ORA_DP_INDIRECTL:
            _clem_read_pba_mode_dp_indirectl(
                clem, &tmp_addr, &tmp_bnk0, &tmp_pc, &tmp_data, 0, false);
            _clem_read_data_816(clem, &tmp_value, tmp_addr, tmp_bnk0, m_status);
            _cpu_ora(cpu, tmp_value, m_status);
            _opcode_instruction_define_dp(&opc_inst, IR, tmp_data);
            break;
        case CLEM_OPC_ORA_ABS_IDX:
            _clem_read_pba_mode_abs(clem, &tmp_addr, &tmp_pc);
            _clem_read_data_indexed_816(
                clem, &tmp_value, tmp_addr, cpu->regs.X, cpu->regs.DBR,
                m_status, x_status);
            _cpu_ora(cpu, tmp_value, m_status);
            _opcode_instruction_define(&opc_inst, IR, tmp_addr, m_status);
            break;
        case CLEM_OPC_ORA_ABSL_IDX:
            _clem_read_pba_mode_absl(clem, &tmp_addr, &tmp_bnk0, &tmp_pc);
            _clem_read_data_indexed_816(
                clem, &tmp_value, tmp_addr, cpu->regs.X, tmp_bnk0, m_status, x_status);
            _cpu_ora(cpu, tmp_value, m_status);
            _opcode_instruction_define_long(&opc_inst, IR, tmp_bnk0, tmp_addr);
            break;
        case CLEM_OPC_ORA_ABS_IDY:      // $addr + Y
            _clem_read_pba_mode_abs(clem, &tmp_addr, &tmp_pc);
            _clem_read_data_indexed_816(clem, &tmp_value, tmp_addr, cpu->regs.Y,
                cpu->regs.DBR, m_status, x_status);
            _cpu_ora(cpu, tmp_value, m_status);
            _opcode_instruction_define(&opc_inst, IR, tmp_addr, m_status);
            break;
        case CLEM_OPC_ORA_DP_IDX:
            _clem_read_pba_mode_dp(clem, &tmp_addr, &tmp_pc, &tmp_data,
                cpu->regs.X, x_status);
            _clem_cycle(clem, 1);             // extra IO cycle for d,x
            _clem_read_data_816(clem, &tmp_value, tmp_addr, 0x00, m_status);
            _cpu_ora(cpu, tmp_value, m_status);
            _opcode_instruction_define_dp(&opc_inst, IR, tmp_data);
            break;
        case CLEM_OPC_ORA_DP_IDX_INDIRECT:
            _clem_read_pba_mode_dp_indirect(clem, &tmp_addr, &tmp_pc, &tmp_data,
                cpu->regs.X, x_status);
            _clem_cycle(clem, 1);       // extra IO for (d, X)
            _clem_read_data_816(clem, &tmp_value, tmp_addr, cpu->regs.DBR, m_status);
            _cpu_ora(cpu, tmp_value, m_status);
            _opcode_instruction_define_dp(&opc_inst, IR, tmp_data);
            break;
        case CLEM_OPC_ORA_DP_INDIRECT_IDY:
            _clem_read_pba_mode_dp_indirect(clem, &tmp_addr, &tmp_pc, &tmp_data, 0, false);
            _clem_read_data_indexed_816(
                clem, &tmp_value, tmp_addr, cpu->regs.Y, cpu->regs.DBR, m_status, x_status);
            _cpu_ora(cpu, tmp_value, m_status);
            _opcode_instruction_define_dp(&opc_inst, IR, tmp_data);
            break;
        case CLEM_OPC_ORA_DP_INDIRECTL_IDY:
            _clem_read_pba_mode_dp_indirectl(clem, &tmp_addr, &tmp_bnk0, &tmp_pc,
                &tmp_data, 0, false);
            _clem_read_data_indexed_816(
                clem, &tmp_value, tmp_addr, cpu->regs.Y, tmp_bnk0, m_status, x_status);
            _cpu_ora(cpu, tmp_value, m_status);
            _opcode_instruction_define_dp(&opc_inst, IR, tmp_data);
            break;
        case CLEM_OPC_ORA_STACK_REL:
            _clem_read_pba_mode_stack_rel(clem, &tmp_addr, &tmp_pc, &tmp_data);
            _clem_read_data_816(clem, &tmp_value, tmp_addr, 0x00, m_status);
            _cpu_ora(cpu, tmp_value, m_status);
            _opcode_instruction_define(&opc_inst, IR, tmp_data, m_status);
            break;
        case CLEM_OPC_ORA_STACK_REL_INDIRECT_IDY:
            _clem_read_pba_mode_stack_rel_indirect(clem, &tmp_addr, &tmp_pc, &tmp_data);
            _clem_read_data_indexed_816(
                clem, &tmp_value, tmp_addr, cpu->regs.Y, cpu->regs.DBR, m_status, x_status);
            _cpu_ora(cpu, tmp_value, m_status);
            _opcode_instruction_define(&opc_inst, IR, tmp_data, m_status);
            break;
        //  End ORA
        //
        case CLEM_OPC_PEA_ABS:
            _clem_read_pba_mode_abs(clem, &tmp_addr, &tmp_pc);
            _cpu_sp_dec2(cpu);
            _clem_write_16(clem, tmp_addr, cpu->regs.S + 1, 0x00);
            _opcode_instruction_define(&opc_inst, IR, tmp_addr, m_status);
            break;
        case CLEM_OPC_PEI_DP_INDIRECT:
            _clem_read_pba_mode_dp_indirect(clem, &tmp_addr, &tmp_pc, &tmp_data,
                0, false);

            _cpu_sp_dec2(cpu);
            _clem_write_16(clem, tmp_addr, cpu->regs.S + 1, 0x00);
            _opcode_instruction_define_dp(&opc_inst, IR, tmp_data);
            break;
        case CLEM_OPC_PER:
            _clem_read_pba_16(clem, &tmp_value, &tmp_pc);
            tmp_addr = tmp_pc + (int16_t)tmp_value;
            _clem_cycle(clem, 1);
            _cpu_sp_dec2(cpu);
            _clem_write_16(clem, tmp_addr, cpu->regs.S + 1, 0x00);
            break;
        case CLEM_OPC_PHA:
            _clem_opc_push_reg_816(clem, cpu->regs.A, m_status);
            break;
        case CLEM_OPC_PHB:
            _clem_cycle(clem, 1);
            clem_write(clem, (uint8_t)cpu->regs.DBR, cpu->regs.S, 0x00,
                       CLEM_MEM_FLAG_DATA);
            _cpu_sp_dec(cpu);
            break;
        case CLEM_OPC_PHD:
            _clem_cycle(clem, 1);
            //  65816 quirk - PHD can overrun the valid stack range
            clem_write(
                clem, (uint8_t)(cpu->regs.A >> 8), cpu->regs.S, 0x00,
                CLEM_MEM_FLAG_DATA);
            clem_write(
                clem, (uint8_t)(cpu->regs.A), cpu->regs.S - 1, 0x00,
                CLEM_MEM_FLAG_DATA);
            _cpu_sp_dec2(cpu);
            break;
        case CLEM_OPC_PHK:
            _clem_cycle(clem, 1);
            clem_write(clem, (uint8_t)cpu->regs.PBR, cpu->regs.S, 0x00,
                       CLEM_MEM_FLAG_DATA);
            _cpu_sp_dec(cpu);
            break;
        case CLEM_OPC_PHP:
            _clem_cycle(clem, 1);
            _clem_opc_push_status(clem, false);
            break;
        case CLEM_OPC_PHX:
            _clem_opc_push_reg_816(clem, cpu->regs.X, x_status);
            break;
        case CLEM_OPC_PHY:
            _clem_opc_push_reg_816(clem, cpu->regs.Y, x_status);
            break;
        case CLEM_OPC_PLA:
            _clem_opc_pull_reg_816(clem, &cpu->regs.A, m_status);
            _cpu_p_flags_n_z_data_816(cpu, cpu->regs.A, m_status);
            break;
        case CLEM_OPC_PLB:
            _clem_opc_pull_reg_8(clem, &cpu->regs.DBR);
            _cpu_p_flags_n_z_data(cpu, cpu->regs.DBR);
            break;
        case CLEM_OPC_PLD:
            _clem_cycle(clem, 2);
            //  65816 quirk - PHD can overrun the valid stack range
            _clem_read_16(clem, &cpu->regs.D, cpu->regs.S, 0x00,
                              CLEM_MEM_FLAG_DATA);
            _cpu_p_flags_n_z_data_16(cpu, cpu->regs.D);
            _cpu_sp_inc2(cpu);
            break;
        case CLEM_OPC_PLP:
            // In emulation, the B flag is not restored - it should
            // instead set x_status to 1? (can we set x_status to 0 in
            // emulation?)
            _clem_cycle(clem, 2);
            _clem_opc_pull_status(clem);
            break;
        case CLEM_OPC_PLX:
            _clem_opc_push_reg_816(clem, cpu->regs.X, x_status);
            _cpu_p_flags_n_z_data_816(cpu, cpu->regs.X, x_status);
            break;
        case CLEM_OPC_PLY:
            _clem_opc_push_reg_816(clem, cpu->regs.Y, x_status);
            _cpu_p_flags_n_z_data_816(cpu, cpu->regs.Y, x_status);
            break;
        case CLEM_OPC_REP:
            // Reset Status Bits
            _clem_read_pba(clem, &tmp_data, &tmp_pc);
            cpu->regs.P &= (~tmp_data); // all 1 bits are turned OFF in P
            if (cpu->pins.emulation) {
                cpu->regs.P |= kClemensCPUStatus_MemoryAccumulator;
                cpu->regs.P |= kClemensCPUStatus_Index;
            }
            _cpu_p_flags_apply_m_x(cpu);
            _clem_cycle(clem, 1);
            _opcode_instruction_define(&opc_inst, IR, tmp_data, false);
            break;
        //
        //  Start ROL
        case CLEM_OPC_ROL_A:
            _cpu_rol(cpu, &cpu->regs.A, m_status);
            _clem_cycle(clem, 1);
            break;
        case CLEM_OPC_ROL_ABS:
            _clem_read_pba_mode_abs(clem, &tmp_addr, &tmp_pc);
            _clem_read_data_816(clem, &tmp_value, tmp_addr, cpu->regs.DBR, m_status);
            _cpu_rol(cpu, &tmp_value, m_status);
            _clem_cycle(clem, 1);
            _clem_write_816(clem, tmp_value, tmp_addr, cpu->regs.DBR, m_status);
            _opcode_instruction_define(&opc_inst, IR, tmp_addr, m_status);
            break;
        case CLEM_OPC_ROL_DP:
            _clem_read_pba_mode_dp(clem, &tmp_addr, &tmp_pc, &tmp_data, 0, false);
            _clem_read_data_816(clem, &tmp_value, tmp_addr, 0x00, m_status);
            _cpu_rol(cpu, &tmp_value, m_status);
            _clem_cycle(clem, 1);
            _clem_write_816(clem, tmp_value, tmp_addr, 0x00, m_status);
            _opcode_instruction_define_dp(&opc_inst, IR, tmp_data);
            break;
        case CLEM_OPC_ROL_ABS_IDX:
            _clem_read_pba_mode_abs(clem, &tmp_addr, &tmp_pc);
            _clem_cycle(clem, 1);
            _clem_read_data_indexed_816(
                clem, &tmp_value, tmp_addr, cpu->regs.X, cpu->regs.DBR,
                m_status, x_status);
            _cpu_rol(cpu, &tmp_value, m_status);
            _clem_cycle(clem, 1);
            _clem_write_indexed_816(clem, tmp_value, tmp_addr, cpu->regs.X,
                cpu->regs.DBR, m_status, x_status);
            _opcode_instruction_define(&opc_inst, IR, tmp_addr, m_status);
            break;
        case CLEM_OPC_ROL_ABS_DP_IDX:
            _clem_read_pba_mode_dp(clem, &tmp_addr, &tmp_pc, &tmp_data,
                cpu->regs.X, x_status);
            _clem_cycle(clem, 1);             // extra IO cycle for d,x
            _clem_read_data_816(clem, &tmp_value, tmp_addr, 0x00, m_status);
            _cpu_rol(cpu, &tmp_value, m_status);
            _clem_cycle(clem, 1);
            _clem_write_816(clem, tmp_value, tmp_addr, 0x00, m_status);
            _opcode_instruction_define_dp(&opc_inst, IR, tmp_data);
            break;
        //  End ROL
        //
        //  Start ROR
        case CLEM_OPC_ROR_A:
            _cpu_ror(cpu, &cpu->regs.A, m_status);
            _clem_cycle(clem, 1);
            break;
        case CLEM_OPC_ROR_ABS:
            _clem_read_pba_mode_abs(clem, &tmp_addr, &tmp_pc);
            _clem_read_data_816(clem, &tmp_value, tmp_addr, cpu->regs.DBR, m_status);
            _cpu_rol(cpu, &tmp_value, m_status);
            _clem_cycle(clem, 1);
            _clem_write_816(clem, tmp_value, tmp_addr, cpu->regs.DBR, m_status);
            _opcode_instruction_define(&opc_inst, IR, tmp_addr, m_status);
            break;
        case CLEM_OPC_ROR_DP:
            _clem_read_pba_mode_dp(clem, &tmp_addr, &tmp_pc, &tmp_data, 0, false);
            _clem_read_data_816(clem, &tmp_value, tmp_addr, 0x00, m_status);
            _cpu_ror(cpu, &tmp_value, m_status);
            _clem_cycle(clem, 1);
            _clem_write_816(clem, tmp_value, tmp_addr, 0x00, m_status);
            _opcode_instruction_define_dp(&opc_inst, IR, tmp_data);
            break;
        case CLEM_OPC_ROR_ABS_IDX:
            _clem_read_pba_mode_abs(clem, &tmp_addr, &tmp_pc);
            _clem_cycle(clem, 1);
            _clem_read_data_indexed_816(
                clem, &tmp_value, tmp_addr, cpu->regs.X, cpu->regs.DBR,
                m_status, x_status);
            _cpu_ror(cpu, &tmp_value, m_status);
            _clem_cycle(clem, 1);
            _clem_write_indexed_816(clem, tmp_value, tmp_addr, cpu->regs.X,
                cpu->regs.DBR, m_status, x_status);
            _opcode_instruction_define(&opc_inst, IR, tmp_addr, m_status);
            break;
        case CLEM_OPC_ROR_ABS_DP_IDX:
            _clem_read_pba_mode_dp(clem, &tmp_addr, &tmp_pc, &tmp_data,
                cpu->regs.X, x_status);
            _clem_cycle(clem, 1);             // extra IO cycle for d,x
            _clem_read_data_816(clem, &tmp_value, tmp_addr, 0x00, m_status);
            _cpu_ror(cpu, &tmp_value, m_status);
            _clem_cycle(clem, 1);
            _clem_write_816(clem, tmp_value, tmp_addr, 0x00, m_status);
            _opcode_instruction_define_dp(&opc_inst, IR, tmp_data);
            break;
        //  End ROR
        //
        // Start SBC
        case CLEM_OPC_SBC_IMM:
            _clem_read_pba_mode_imm_816(clem, &tmp_value, &tmp_pc, m_status);
            _cpu_sbc(cpu, tmp_value, m_status);
            _opcode_instruction_define(&opc_inst, IR, tmp_value, m_status);
            break;
        case CLEM_OPC_SBC_ABS:
            _clem_read_pba_mode_abs(clem, &tmp_addr, &tmp_pc);
            _clem_read_data_816(clem, &tmp_value, tmp_addr, cpu->regs.DBR, m_status);
            _cpu_sbc(cpu, tmp_value, m_status);
            _opcode_instruction_define(&opc_inst, IR, tmp_addr, m_status);
            break;
        case CLEM_OPC_SBC_ABSL:
            //  TODO: emulation mode
            _clem_read_pba_mode_absl(clem, &tmp_addr, &tmp_bnk0, &tmp_pc);
            _clem_read_data_816(clem, &tmp_value, tmp_addr, tmp_bnk0, m_status);
            _cpu_sbc(cpu, tmp_value, m_status);
            _opcode_instruction_define_long(&opc_inst, IR, tmp_bnk0, tmp_addr);
            break;
        case CLEM_OPC_SBC_DP:
            _clem_read_pba_mode_dp(clem, &tmp_addr, &tmp_pc, &tmp_data, 0, false);
            _clem_read_data_816(clem, &tmp_value, tmp_addr, 0x00, m_status);
            _cpu_sbc(cpu, tmp_value, m_status);
            _opcode_instruction_define_dp(&opc_inst, IR, tmp_data);
            break;
        case CLEM_OPC_SBC_DP_INDIRECT:
            _clem_read_pba_mode_dp_indirect(clem, &tmp_addr, &tmp_pc, &tmp_data, 0, false);
            _clem_read_data_816(clem, &tmp_value, tmp_addr, cpu->regs.DBR, m_status);
            _cpu_sbc(cpu, tmp_value, m_status);
            _opcode_instruction_define_dp(&opc_inst, IR, tmp_data);
            break;
        case CLEM_OPC_SBC_DP_INDIRECTL:
            _clem_read_pba_mode_dp_indirectl(
                clem, &tmp_addr, &tmp_bnk0, &tmp_pc, &tmp_data, 0, false);
            _clem_read_data_816(clem, &tmp_value, tmp_addr, tmp_bnk0, m_status);
            _cpu_sbc(cpu, tmp_value, m_status);
            _opcode_instruction_define_dp(&opc_inst, IR, tmp_data);
            break;
        case CLEM_OPC_SBC_ABS_IDX:
            _clem_read_pba_mode_abs(clem, &tmp_addr, &tmp_pc);
            _clem_read_data_indexed_816(
                clem, &tmp_value, tmp_addr, cpu->regs.X, cpu->regs.DBR,
                m_status, x_status);
            _cpu_sbc(cpu, tmp_value, m_status);
            _opcode_instruction_define(&opc_inst, IR, tmp_addr, m_status);
            break;
        case CLEM_OPC_SBC_ABSL_IDX:
            _clem_read_pba_mode_absl(clem, &tmp_addr, &tmp_bnk0, &tmp_pc);
            _clem_read_data_indexed_816(
                clem, &tmp_value, tmp_addr, cpu->regs.X, tmp_bnk0, m_status, x_status);
            _cpu_sbc(cpu, tmp_value, m_status);
            _opcode_instruction_define_long(&opc_inst, IR, tmp_bnk0, tmp_addr);
            break;
        case CLEM_OPC_SBC_ABS_IDY:      // $addr + Y
            _clem_read_pba_mode_abs(clem, &tmp_addr, &tmp_pc);
            _clem_read_data_indexed_816(clem, &tmp_value, tmp_addr, cpu->regs.Y,
                cpu->regs.DBR, m_status, x_status);
            _cpu_sbc(cpu, tmp_value, m_status);
            _opcode_instruction_define(&opc_inst, IR, tmp_addr, m_status);
            break;
        case CLEM_OPC_SBC_DP_IDX:
            _clem_read_pba_mode_dp(clem, &tmp_addr, &tmp_pc, &tmp_data,
                cpu->regs.X, x_status);
            _clem_cycle(clem, 1);             // extra IO cycle for d,x
            _clem_read_data_816(clem, &tmp_value, tmp_addr, 0x00, m_status);
            _cpu_sbc(cpu, tmp_value, m_status);
            _opcode_instruction_define_dp(&opc_inst, IR, tmp_data);
            break;
        case CLEM_OPC_SBC_DP_IDX_INDIRECT:
            _clem_read_pba_mode_dp_indirect(clem, &tmp_addr, &tmp_pc, &tmp_data,
                cpu->regs.X, x_status);
            _clem_cycle(clem, 1);       // extra IO for (d, X)
            _clem_read_data_816(clem, &tmp_value, tmp_addr, cpu->regs.DBR, m_status);
            _cpu_sbc(cpu, tmp_value, m_status);
            _opcode_instruction_define_dp(&opc_inst, IR, tmp_data);
            break;
        case CLEM_OPC_SBC_DP_INDIRECT_IDY:
            _clem_read_pba_mode_dp_indirect(clem, &tmp_addr, &tmp_pc, &tmp_data, 0, false);
            _clem_read_data_indexed_816(
                clem, &tmp_value, tmp_addr, cpu->regs.Y, cpu->regs.DBR, m_status, x_status);
            _cpu_sbc(cpu, tmp_value, m_status);
            _opcode_instruction_define_dp(&opc_inst, IR, tmp_data);
            break;
        case CLEM_OPC_SBC_DP_INDIRECTL_IDY:
            _clem_read_pba_mode_dp_indirectl(clem, &tmp_addr, &tmp_bnk0, &tmp_pc,
                &tmp_data, 0, false);
            _clem_read_data_indexed_816(
                clem, &tmp_value, tmp_addr, cpu->regs.Y, tmp_bnk0, m_status, x_status);
            _cpu_sbc(cpu, tmp_value, m_status);
            _opcode_instruction_define_dp(&opc_inst, IR, tmp_data);
            break;
        case CLEM_OPC_SBC_STACK_REL:
            _clem_read_pba_mode_stack_rel(clem, &tmp_addr, &tmp_pc, &tmp_data);
            _clem_read_data_816(clem, &tmp_value, tmp_addr, 0x00, m_status);
            _cpu_sbc(cpu, tmp_value, m_status);
            _opcode_instruction_define(&opc_inst, IR, tmp_data, m_status);
            break;
        case CLEM_OPC_SBC_STACK_REL_INDIRECT_IDY:
            _clem_read_pba_mode_stack_rel_indirect(clem, &tmp_addr, &tmp_pc, &tmp_data);
            _clem_read_data_indexed_816(
                clem, &tmp_value, tmp_addr, cpu->regs.Y, cpu->regs.DBR, m_status, x_status);
            _cpu_sbc(cpu, tmp_value, m_status);
            _opcode_instruction_define(&opc_inst, IR, tmp_data, m_status);
            break;
        //  End ADC
        //
        case CLEM_OPC_SEC:
            cpu->regs.P |= kClemensCPUStatus_Carry;
            _clem_cycle(clem, 1);
            break;
        case CLEM_OPC_SED:
            cpu->regs.P |= kClemensCPUStatus_Decimal;
            _clem_cycle(clem, 1);

            break;
        case CLEM_OPC_SEI:
            cpu->regs.P |= kClemensCPUStatus_IRQDisable;
            _clem_cycle(clem, 1);
            break;
        case CLEM_OPC_SEP:
            // Reset Status Bits
            _clem_read_pba(clem, &tmp_data, &tmp_pc);
            if (cpu->pins.emulation) {
                tmp_data |= kClemensCPUStatus_MemoryAccumulator;
                tmp_data |= kClemensCPUStatus_Index;
            }
            cpu->regs.P |= tmp_data;    // all 1 bits are turned ON in P
            _cpu_p_flags_apply_m_x(cpu);
            _clem_cycle(clem, 1);
            _opcode_instruction_define(&opc_inst, IR, tmp_data, false);
            break;
        //
        //  Start STA
        case CLEM_OPC_STA_ABS:
            _clem_read_pba_mode_abs(clem, &tmp_addr, &tmp_pc);
            _clem_write_816(clem, cpu->regs.A, tmp_addr, cpu->regs.DBR, m_status);
            _opcode_instruction_define(&opc_inst, IR, tmp_addr, m_status);
            break;
        case CLEM_OPC_STA_ABSL:
            //  absolute long read
            //  TODO: what about emulation mode?
            _clem_read_pba_mode_absl(clem, &tmp_addr, &tmp_bnk0, &tmp_pc);
            _clem_write_816(clem, cpu->regs.A, tmp_addr, tmp_bnk0, m_status);
            _opcode_instruction_define_long(&opc_inst, IR, tmp_bnk0, tmp_addr);
            break;
        case CLEM_OPC_STA_DP:
            _clem_read_pba_mode_dp(clem, &tmp_addr, &tmp_pc, &tmp_data, 0, false);
            _clem_write_816(clem, cpu->regs.A, tmp_addr, 0x00, m_status);
            _opcode_instruction_define_dp(&opc_inst, IR, tmp_data);
            break;
        case CLEM_OPC_STA_DP_INDIRECT:
            _clem_read_pba_mode_dp_indirect(clem, &tmp_addr, &tmp_pc, &tmp_data, 0, false);
            _clem_write_816(clem, cpu->regs.A, tmp_addr, cpu->regs.DBR, m_status);
            _opcode_instruction_define_dp(&opc_inst, IR, tmp_data);
            break;
        case CLEM_OPC_STA_DP_INDIRECTL:
            _clem_read_pba_mode_dp_indirectl(clem, &tmp_addr, &tmp_bnk0, &tmp_pc,
                &tmp_data, 0, false);
            _clem_write_816(clem, cpu->regs.A, tmp_addr, tmp_bnk0, m_status);
            _opcode_instruction_define_dp(&opc_inst, IR, tmp_data);
            break;
        case CLEM_OPC_STA_ABS_IDX:
            _clem_read_pba_mode_abs(clem, &tmp_addr, &tmp_pc);
            _clem_write_indexed_816(clem, cpu->regs.A, tmp_addr, cpu->regs.X,
                cpu->regs.DBR, m_status, x_status);
            _opcode_instruction_define(&opc_inst, IR, tmp_addr, m_status);
            break;
        case CLEM_OPC_STA_ABSL_IDX:
            _clem_read_pba_mode_absl(clem, &tmp_addr, &tmp_bnk0, &tmp_pc);
            _clem_write_indexed_816(clem, cpu->regs.A, tmp_addr, cpu->regs.X,
                 tmp_bnk0, m_status, x_status);
            _opcode_instruction_define_long(&opc_inst, IR, tmp_bnk0, tmp_addr);
            break;
        case CLEM_OPC_STA_ABS_IDY:      // $addr + Y
            _clem_read_pba_mode_abs(clem, &tmp_addr, &tmp_pc);
            _clem_write_indexed_816(clem, cpu->regs.A, tmp_addr, cpu->regs.Y,
                cpu->regs.DBR, m_status, x_status);
            _opcode_instruction_define(&opc_inst, IR, tmp_addr, m_status);
            break;
        case CLEM_OPC_STA_DP_IDX:
            _clem_read_pba_mode_dp(clem, &tmp_addr, &tmp_pc, &tmp_data,
                cpu->regs.X, x_status);
            _clem_cycle(clem, 1);             // extra IO cycle for d,x
            _clem_write_816(clem, cpu->regs.A, tmp_addr, 0x00, m_status);
            _opcode_instruction_define_dp(&opc_inst, IR, tmp_data);
            break;
        case CLEM_OPC_STA_DP_IDX_INDIRECT:
            _clem_read_pba_mode_dp_indirect(clem, &tmp_addr, &tmp_pc, &tmp_data,
                cpu->regs.X, x_status);
            _clem_cycle(clem, 1);       // extra IO for (d, X)
            _clem_write_816(clem, cpu->regs.A, tmp_addr, cpu->regs.DBR, m_status);
            _opcode_instruction_define_dp(&opc_inst, IR, tmp_data);
            break;
        case CLEM_OPC_STA_DP_INDIRECT_IDY:
            _clem_read_pba_mode_dp_indirect(clem, &tmp_addr, &tmp_pc, &tmp_data,
                0, false);
            _clem_write_indexed_816(clem, cpu->regs.A, tmp_addr, cpu->regs.Y,
                cpu->regs.DBR, m_status, x_status);
            _opcode_instruction_define_dp(&opc_inst, IR, tmp_data);
            break;
        case CLEM_OPC_STA_DP_INDIRECTL_IDY:
            _clem_read_pba_mode_dp_indirectl(clem, &tmp_addr, &tmp_bnk0, &tmp_pc,
                &tmp_data, 0, false);
            _clem_write_indexed_816(clem, cpu->regs.A, tmp_addr, cpu->regs.Y,
                tmp_bnk0, m_status, x_status);
            _opcode_instruction_define_dp(&opc_inst, IR, tmp_data);
            break;
        case CLEM_OPC_STA_STACK_REL:
            _clem_read_pba_mode_stack_rel(clem, &tmp_addr, &tmp_pc, &tmp_data);
            _clem_write_816(clem, cpu->regs.A, tmp_addr, 0x00, m_status);
            _opcode_instruction_define(&opc_inst, IR, tmp_data, m_status);
            break;
        case CLEM_OPC_STA_STACK_REL_INDIRECT_IDY:
            _clem_read_pba_mode_stack_rel_indirect(clem, &tmp_addr, &tmp_pc, &tmp_data);
            _clem_write_indexed_816(clem, cpu->regs.A, tmp_addr, cpu->regs.Y,
                cpu->regs.DBR, m_status, x_status);
            _opcode_instruction_define(&opc_inst, IR, tmp_data, m_status);
            break;
        //  End STA
        //
        //  Start STX,STY,STZ
        case CLEM_OPC_STX_ABS:
            _clem_read_pba_mode_abs(clem, &tmp_addr, &tmp_pc);
            _clem_write_816(clem, cpu->regs.X, tmp_addr, cpu->regs.DBR, x_status);
            _opcode_instruction_define(&opc_inst, IR, tmp_addr, x_status);
            break;
        case CLEM_OPC_STX_DP:
            _clem_read_pba_mode_dp(clem, &tmp_addr, &tmp_pc, &tmp_data, 0, x_status);
            _clem_write_816(clem, cpu->regs.X, tmp_addr, 0x00,  x_status);
            _opcode_instruction_define_dp(&opc_inst, IR, tmp_data);
            break;
        case CLEM_OPC_STX_DP_IDY:
            _clem_read_pba_mode_dp(clem, &tmp_addr, &tmp_pc, &tmp_data,
                cpu->regs.Y, x_status);
            _clem_cycle(clem, 1);             // extra IO cycle for d,x
            _clem_write_816(clem, cpu->regs.X, tmp_addr, 0x00, x_status);
            _opcode_instruction_define_dp(&opc_inst, IR, tmp_data);
            break;
        case CLEM_OPC_STY_ABS:
            _clem_read_pba_mode_abs(clem, &tmp_addr, &tmp_pc);
            _clem_write_816(clem, cpu->regs.Y, tmp_addr, cpu->regs.DBR, x_status);
            _opcode_instruction_define(&opc_inst, IR, tmp_addr, x_status);
            break;
        case CLEM_OPC_STY_DP:
            _clem_read_pba_mode_dp(clem, &tmp_addr, &tmp_pc, &tmp_data, 0, x_status);
            _clem_write_816(clem, cpu->regs.Y, tmp_addr, 0x00, x_status);
            _opcode_instruction_define_dp(&opc_inst, IR, tmp_data);
            break;
        case CLEM_OPC_STY_DP_IDX:
            _clem_read_pba_mode_dp(clem, &tmp_addr, &tmp_pc, &tmp_data,
                cpu->regs.X, x_status);
            _clem_cycle(clem, 1);             // extra IO cycle for d,x
            _clem_write_816(clem, cpu->regs.Y, tmp_addr, 0x00, x_status);
            _opcode_instruction_define_dp(&opc_inst, IR, tmp_data);
            break;
        case CLEM_OPC_STZ_ABS:
            _clem_read_pba_mode_abs(clem, &tmp_addr, &tmp_pc);
            _clem_write_816(clem, 0x0000, tmp_addr, cpu->regs.DBR, m_status);
            _opcode_instruction_define(&opc_inst, IR, tmp_addr, m_status);
            break;
        case CLEM_OPC_STZ_DP:
            _clem_read_pba_mode_dp(clem, &tmp_addr, &tmp_pc, &tmp_data, 0, false);
            _clem_write_816(clem, 0x0000, tmp_addr, 0x00, m_status);
            _opcode_instruction_define_dp(&opc_inst, IR, tmp_data);
            break;
        case CLEM_OPC_STZ_ABS_IDX:
            _clem_read_pba_mode_abs(clem, &tmp_addr, &tmp_pc);
            _clem_write_indexed_816(clem, 0x0000, tmp_addr, cpu->regs.X,
                cpu->regs.DBR, m_status, x_status);
            _opcode_instruction_define(&opc_inst, IR, tmp_addr, m_status);
            break;
        case CLEM_OPC_STZ_DP_IDX:
            _clem_read_pba_mode_dp(clem, &tmp_addr, &tmp_pc, &tmp_data,
                cpu->regs.X, x_status);
            _clem_cycle(clem, 1);             // extra IO cycle for d,x
            _clem_write_816(clem, 0x0000, tmp_addr, 0x00, m_status);
            _opcode_instruction_define_dp(&opc_inst, IR, tmp_data);
            break;
        //  End STX,STY,STZ
        //
        //  Start Transfer
        case CLEM_OPC_TAX:
            if (x_status) {
                cpu->regs.X = CLEM_UTIL_set16_lo(cpu->regs.X, cpu->regs.A);
                _cpu_p_flags_n_z_data(cpu, (uint8_t)cpu->regs.X);
            } else {
                cpu->regs.X = cpu->regs.A;
                _cpu_p_flags_n_z_data_16(cpu, cpu->regs.X);
            }
            _clem_cycle(clem, 1);
            break;
        case CLEM_OPC_TAY:
            if (x_status) {
                cpu->regs.Y = CLEM_UTIL_set16_lo(cpu->regs.Y, cpu->regs.A);
                _cpu_p_flags_n_z_data(cpu, (uint8_t)cpu->regs.Y);
            } else {
                cpu->regs.Y = cpu->regs.A;
                _cpu_p_flags_n_z_data_16(cpu, cpu->regs.Y);
            }
            _clem_cycle(clem, 1);
            break;
        case CLEM_OPC_TCD:
            cpu->regs.D = cpu->regs.A;
            _cpu_p_flags_n_z_data_16(cpu, cpu->regs.D);
             _clem_cycle(clem, 1);
            break;
        case CLEM_OPC_TDC:
            cpu->regs.A = cpu->regs.D;
            _cpu_p_flags_n_z_data_16(cpu, cpu->regs.A);
             _clem_cycle(clem, 1);
            break;
        case CLEM_OPC_TCS:
            if (cpu->pins.emulation) {
                cpu->regs.S = CLEM_UTIL_set16_lo(cpu->regs.S, cpu->regs.A);
            } else {
                cpu->regs.S = cpu->regs.A;
            }
            _clem_cycle(clem, 1);
            break;
        case CLEM_OPC_TSC:
            cpu->regs.A = cpu->regs.S;
            _cpu_p_flags_n_z_data_16(cpu, cpu->regs.A);
            _clem_cycle(clem, 1);
            break;
        case CLEM_OPC_TSX:
            if (!cpu->pins.emulation && !x_status) {
                cpu->regs.X = cpu->regs.S;
                _cpu_p_flags_n_z_data_16(cpu, cpu->regs.X);
            } else if (x_status) {
                cpu->regs.X = CLEM_UTIL_set16_lo(cpu->regs.X, cpu->regs.S);
                _cpu_p_flags_n_z_data(cpu, (uint8_t)cpu->regs.X);
            }
            break;
        case CLEM_OPC_TXA:
            if (m_status) {
                cpu->regs.A = CLEM_UTIL_set16_lo(cpu->regs.A, cpu->regs.X);
                _cpu_p_flags_n_z_data(cpu, (uint8_t)cpu->regs.A);
            } else {
                cpu->regs.A = x_status ? (uint8_t)cpu->regs.X : cpu->regs.X;
                _cpu_p_flags_n_z_data_16(cpu, cpu->regs.A);
            }
            break;
        case CLEM_OPC_TXS:
            //  no n,z flags set
            if (cpu->pins.emulation) {
                cpu->regs.S = CLEM_UTIL_set16_lo(cpu->regs.S, cpu->regs.X);
            } else if (x_status) {
                cpu->regs.S = cpu->regs.X & 0x00ff;
            } else {
                cpu->regs.S = cpu->regs.X;
            }
            break;
        case CLEM_OPC_TXY:
            if (x_status) {
                cpu->regs.Y = CLEM_UTIL_set16_lo(cpu->regs.Y, cpu->regs.X);
                _cpu_p_flags_n_z_data(cpu, (uint8_t)cpu->regs.Y);
            } else {
                cpu->regs.Y = cpu->regs.X;
                _cpu_p_flags_n_z_data_16(cpu, cpu->regs.Y);
            }
            break;
        case CLEM_OPC_TYA:
            if (m_status) {
                cpu->regs.A = CLEM_UTIL_set16_lo(cpu->regs.A, cpu->regs.Y);
                _cpu_p_flags_n_z_data(cpu, (uint8_t)cpu->regs.A);
            } else {
                cpu->regs.A = x_status ? (uint8_t)cpu->regs.A : cpu->regs.Y;
                _cpu_p_flags_n_z_data_16(cpu, cpu->regs.A);
            }
            break;
        case CLEM_OPC_TYX:
            if (x_status) {
                cpu->regs.X = CLEM_UTIL_set16_lo(cpu->regs.X, cpu->regs.Y);
                _cpu_p_flags_n_z_data(cpu, (uint8_t)cpu->regs.X);
            } else {
                cpu->regs.X = cpu->regs.Y;
                _cpu_p_flags_n_z_data_16(cpu, cpu->regs.X);
            }
            break;
        //  End Transfer
        //
        case CLEM_OPC_TRB_ABS:
            _clem_read_pba_mode_abs(clem, &tmp_addr, &tmp_pc);
            _clem_read_data_816(clem, &tmp_value, tmp_addr, cpu->regs.DBR, m_status);
            _cpu_trb(cpu, &tmp_value, m_status);
            _clem_cycle(clem, 1);
            _clem_write_816(clem, tmp_value, tmp_addr, cpu->regs.DBR, m_status);
            _opcode_instruction_define(&opc_inst, IR, tmp_addr, m_status);
            break;
        case CLEM_OPC_TRB_DP:
            _clem_read_pba_mode_dp(clem, &tmp_addr, &tmp_pc, &tmp_data, 0, false);
            _clem_read_data_816(clem, &tmp_value, tmp_addr, 0x00, m_status);
            _cpu_trb(cpu, &tmp_value, m_status);
            _clem_cycle(clem, 1);
            _clem_write_816(clem, tmp_value, tmp_addr, 0x00, m_status);
            _opcode_instruction_define_dp(&opc_inst, IR, tmp_data);
            break;
        case CLEM_OPC_TSB_ABS:
            _clem_read_pba_mode_abs(clem, &tmp_addr, &tmp_pc);
            _clem_read_data_816(clem, &tmp_value, tmp_addr, cpu->regs.DBR, m_status);
            _cpu_tsb(cpu, &tmp_value, m_status);
            _clem_cycle(clem, 1);
            _clem_write_816(clem, tmp_value, tmp_addr, cpu->regs.DBR, m_status);
            _opcode_instruction_define(&opc_inst, IR, tmp_addr, m_status);
            break;
        case CLEM_OPC_TSB_DP:
            _clem_read_pba_mode_dp(clem, &tmp_addr, &tmp_pc, &tmp_data, 0, false);
            _clem_read_data_816(clem, &tmp_value, tmp_addr, 0x00, m_status);
            _cpu_tsb(cpu, &tmp_value, m_status);
            _clem_cycle(clem, 1);
            _clem_write_816(clem, tmp_value, tmp_addr, 0x00, m_status);
            _opcode_instruction_define_dp(&opc_inst, IR, tmp_data);
            break;
        case CLEM_OPC_XBA:
            tmp_value = cpu->regs.A;
            cpu->regs.A = (tmp_value & 0xff00) >> 8;
            cpu->regs.A = (tmp_value & 0x00ff) << 8 | cpu->regs.A;
            _cpu_p_flags_n_z_data(cpu, (uint8_t)(cpu->regs.A & 0x00ff));
            _clem_cycle(clem, 2);
            break;
        case CLEM_OPC_XCE:
            tmp_value = cpu->pins.emulation;
            cpu->pins.emulation = (cpu->regs.P & kClemensCPUStatus_Carry) != 0;
            if (tmp_value != cpu->pins.emulation) {
                cpu->regs.P |= kClemensCPUStatus_Index;
                cpu->regs.P |= kClemensCPUStatus_MemoryAccumulator;
                if (tmp_value) {
                    // TODO: log internally
                } else {
                    // switch to emulation, and emulation stack
                    cpu->regs.S = CLEM_UTIL_set16_lo(0x0100, cpu->regs.S);
                }
                _cpu_p_flags_apply_m_x(cpu);
            }
            if (tmp_value) {
                cpu->regs.P |= kClemensCPUStatus_Carry;
            } else {
                cpu->regs.P &= ~kClemensCPUStatus_Carry;
            }
            _clem_cycle(clem, 1);
            break;
        case CLEM_OPC_WDM:
            _clem_read_pba(clem, &tmp_data, &tmp_pc);
            //  TODO: add option for wdm custom ops vs NOP
            //        right now, always a custom op
            if (tmp_data == 0x01) {
                // memory dump
                // byte 0 = pages to print (0-255)
                // byte 1 = bank
                // byte 2,3 = adrlo, hi
                _clem_read_pba(clem, &tmp_data, &tmp_pc);
                _clem_read_pba(clem, &tmp_bnk0, &tmp_pc);
                _clem_read_pba(clem, &tmp_bnk1, &tmp_pc);
                _clem_debug_memory_dump(clem, tmp_bnk1, tmp_bnk0, tmp_data);
            }
            break;
        //  Jump, JSR,
        case CLEM_OPC_JSR:
            // Stack [PCH, PCL]
            _clem_read_pba_16(clem, &tmp_addr, &tmp_pc);
            --tmp_pc;       // point to last byte in operand
            _clem_cycle(clem, 1);
            _clem_opc_push_pc16(clem, tmp_pc);
            _opcode_instruction_define(&opc_inst, IR, tmp_addr, false);
            CLEM_CPU_I_JSR_LOG(cpu, tmp_addr);
            tmp_pc = tmp_addr;      // set next PC to the JSR routine
            break;
        case CLEM_OPC_RTS:
            //  Stack [PCH, PCL]
            _clem_cycle(clem, 2);
            tmp_value = cpu->regs.S + 1;
            if (cpu->pins.emulation) {
                tmp_value = CLEM_UTIL_set16_lo(cpu->regs.S, tmp_value);
            }
            clem_read(clem, &tmp_data, tmp_value, 0x00,
                            CLEM_MEM_FLAG_DATA);
            tmp_addr = tmp_data;
            ++tmp_value;
            if (cpu->pins.emulation) {
                tmp_value = CLEM_UTIL_set16_lo(cpu->regs.S, tmp_value);
            }
            clem_read(clem, &tmp_data, tmp_value, 0x00,
                            CLEM_MEM_FLAG_DATA);
            tmp_addr = ((uint16_t)tmp_data << 8) | tmp_addr;
            _clem_cycle(clem, 1);
            _cpu_sp_inc2(cpu);
            tmp_pc = tmp_addr + 1;  //  point to next instruction
            CLEM_CPU_I_RTS_LOG(cpu, tmp_pc);
            break;
        case CLEM_OPC_JSL:
            // Stack [PBR, PCH, PCL]
            _clem_read_pba_16(clem, &tmp_addr, &tmp_pc);
            //  push old PBR
            clem_write(clem, cpu->regs.PBR, cpu->regs.S, 0x0,
                       CLEM_MEM_FLAG_DATA);
            _clem_cycle(clem, 1);
            //  new PBR
            _clem_read_pba(clem, &tmp_data, &tmp_pc);
            --tmp_pc;        // point to last byte in operand
            tmp_bnk0 = tmp_data;
            //  JSL stack overrun will not wrap to 0x1ff (65816 quirk)
            //  SP will still wrap
            //  tmp_pc will be tha address of the last operand
            clem_write(clem, (uint8_t)(tmp_pc >> 8), cpu->regs.S - 1, 0x00,
                       CLEM_MEM_FLAG_DATA);
            tmp_value = cpu->regs.S - 1;
            clem_write(clem, (uint8_t)tmp_pc, cpu->regs.S - 2, 0x00,
                       CLEM_MEM_FLAG_DATA);
            _cpu_sp_dec3(cpu);
            _opcode_instruction_define_long(&opc_inst, IR, tmp_bnk0, tmp_addr);
            CLEM_CPU_I_JSL_LOG(cpu, tmp_addr, tmp_bnk0);
            tmp_pc = tmp_addr;      // set next PC to the JSL routine
            cpu->regs.PBR = tmp_bnk0;
            break;
        case CLEM_OPC_RTL:
            _clem_cycle(clem, 2);
            //  again, 65816 quirk where RTL will read from over the top
            //  in emulation mode even
            clem_read(clem, &tmp_data, cpu->regs.S + 1, 0x00,
                            CLEM_MEM_FLAG_DATA);
            tmp_addr = tmp_data;
            clem_read(clem, &tmp_data, cpu->regs.S + 2, 0x00,
                            CLEM_MEM_FLAG_DATA);
            tmp_addr = ((uint16_t)tmp_data << 8) | tmp_addr;
            clem_read(clem, &tmp_data, cpu->regs.S + 3, 0x00,
                            CLEM_MEM_FLAG_DATA);
            _cpu_sp_inc3(cpu);
            tmp_pc = tmp_addr + 1;
            CLEM_CPU_I_RTL_LOG(cpu, tmp_pc, tmp_data);
            cpu->regs.PBR = tmp_data;
            break;

        //  interrupt opcodes (RESET is handled separately)
        case CLEM_OPC_BRK:
            //  ignore irq disable
            _clem_read_pba(clem, &tmp_data, &tmp_pc);
            CLEM_CPU_I_INTR_LOG(cpu, "BRK");
            tmp_value = tmp_data;
            //  push PBR (native)
            //  push PC
            //  push P - move PHP code into common utility for this
            //  irq_disable is set
            //  reset P:decimal flag to 0 (assuming before running BRK program)
            //  PC <- use native or emulation mode vector
            //  TODO: what to do if we are already in a brk?
            //  native mode push PBR
            if (!cpu->pins.emulation) {
                _cpu_sp_dec(cpu);
                clem_write(clem, (uint8_t)cpu->regs.PBR, cpu->regs.S + 1, 0x00,
                           CLEM_MEM_FLAG_DATA);
            }
            //  push PC and status
            _cpu_sp_dec2(cpu);
            _clem_write_16(clem, tmp_pc, cpu->regs.S + 1, 0x00);
            _clem_opc_push_status(clem, true);
            cpu->regs.P |= kClemensCPUStatus_IRQDisable;
            //  docs conflict on whether 65816 emulation mode clears the decimal
            //  status or not.
            //  TODO: address this conflict if the need arises
            cpu->regs.P &= ~kClemensCPUStatus_Decimal;
            if (cpu->pins.emulation) {
                clem_read(clem, &tmp_data, CLEM_6502_IRQBRK_VECTOR_LO_ADDR, 0x00,
                       CLEM_MEM_FLAG_PROGRAM);
                tmp_addr = tmp_data;
                clem_read(clem, &tmp_data, CLEM_6502_IRQBRK_VECTOR_HI_ADDR,
                       0x00, CLEM_MEM_FLAG_PROGRAM);
                tmp_pc = ((uint16_t)tmp_data << 8) | tmp_addr;
            } else {
                cpu->regs.PBR = 0x00;
                clem_read(clem, &tmp_data, CLEM_65816_BRK_VECTOR_LO_ADDR, 0x00,
                       CLEM_MEM_FLAG_PROGRAM);
                tmp_addr = tmp_data;
                clem_read(clem, &tmp_data, CLEM_65816_BRK_VECTOR_HI_ADDR,
                       0x00, CLEM_MEM_FLAG_PROGRAM);
                tmp_pc = ((uint16_t)tmp_data << 8) | tmp_addr;
            }
            _opcode_instruction_define(&opc_inst, IR, tmp_value, true);
            break;
        case CLEM_OPC_COP:
            //  ignore irq disable
            _clem_read_pba(clem, &tmp_data, &tmp_pc);
            tmp_value = tmp_data;
            //  push PBR (native)
            //  push PC
            //  push P - move PHP code into common utility for this
            //  irq_disable is set
            //  reset P:decimal flag to 0 (assuming before running BRK program)
            //  PC <- use native or emulation mode vector
            //  TODO: what to do if we are already in a brk?
            //  native mode push PBR
            if (!cpu->pins.emulation) {
                _cpu_sp_dec(cpu);
                clem_write(clem, (uint8_t)cpu->regs.PBR, cpu->regs.S + 1, 0x00,
                           CLEM_MEM_FLAG_DATA);
            }
            //  push PC and status
            _cpu_sp_dec2(cpu);
            _clem_write_16(clem, tmp_pc, cpu->regs.S + 1, 0x00);
            _clem_opc_push_status(clem, false);
            cpu->regs.P |= kClemensCPUStatus_IRQDisable;
            //  docs conflict on whether 65816 emulation mode clears the decimal
            //  status or not.
            //  TODO: address this conflict if the need arises
            cpu->regs.P &= ~kClemensCPUStatus_Decimal;
            if (cpu->pins.emulation) {
                clem_read(clem, &tmp_data, CLEM_6502_COP_VECTOR_LO_ADDR, 0x00,
                       CLEM_MEM_FLAG_PROGRAM);
                tmp_addr = tmp_data;
                clem_read(clem, &tmp_data, CLEM_6502_COP_VECTOR_HI_ADDR,
                       0x00, CLEM_MEM_FLAG_PROGRAM);
                tmp_pc = ((uint16_t)tmp_data << 8) | tmp_addr;
            } else {
                cpu->regs.PBR = 0x00;
                clem_read(clem, &tmp_data, CLEM_65816_COP_VECTOR_LO_ADDR, 0x00,
                       CLEM_MEM_FLAG_PROGRAM);
                tmp_addr = tmp_data;
                clem_read(clem, &tmp_data, CLEM_65816_COP_VECTOR_HI_ADDR,
                       0x00, CLEM_MEM_FLAG_PROGRAM);
                tmp_pc = ((uint16_t)tmp_data << 8) | tmp_addr;
            }
            _opcode_instruction_define(&opc_inst, IR, tmp_value, true);
            break;
        case CLEM_OPC_RTI:
            _clem_cycle(clem, 2);
            _clem_opc_pull_status(clem);
            _clem_read_16(clem, &tmp_addr, cpu->regs.S + 1, 0x00, CLEM_MEM_FLAG_DATA);
            _cpu_sp_inc2(cpu);
            if (!cpu->pins.emulation) {
                clem_read(clem, &tmp_bnk0, cpu->regs.S + 1, 0x00, CLEM_MEM_FLAG_DATA);
                _cpu_sp_inc(cpu);
                cpu->regs.PBR = tmp_bnk0;
            }
            tmp_pc = tmp_addr;
            break;
        case CLEM_OPC_WAI:
            //  the calling application should interpret ReadyOut
            //  TODO: should we guard against emulate() running cpu_execute()
            //        if readyOut is false?
            _clem_cycle(clem, 2);
            cpu->pins.readyOut = false;
            break;
        case CLEM_OPC_STP:
            _clem_cycle(clem, 2);
            cpu->enabled = false;
            break;
        default:
            printf("Unknown IR = %x\n", IR);
            assert(false);
            break;
    }

    cpu->regs.PC = tmp_pc;
    if (clem->debug_flags) {
        opc_inst.pbr = opc_pbr;
        opc_inst.addr = opc_addr;
        _opcode_print(clem, &opc_inst);
    }
}

void clemens_emulate(ClemensMachine* clem) {
    struct Clemens65C816* cpu = &clem->cpu;
    struct ClemensMMIO* mmio = &clem->mmio;
    uint32_t delta_mega2_cycles;

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
            cpu->pins.emulation = true;
            cpu->pins.readyOut = true;
            cpu->enabled = true;

            _clem_mmio_init(mmio, clem->clocks_step_mega2);
            _clem_cycle(clem, 1);
        }
        _clem_cycle(clem, 1);
        return;
    }
    //  RESB high during reset invokes our interrupt microcode
    if (!cpu->enabled) return;

    // CLEM_I_PRINT_STATS(clem);

    if (cpu->state_type == kClemensCPUStateType_Reset) {
        uint16_t tmp_addr;
        uint16_t tmp_value;
        uint8_t tmp_data;
        uint8_t tmp_datahi;

        clem_read(clem, &tmp_data, cpu->regs.S, 0x00, CLEM_MEM_FLAG_DATA);
        tmp_addr = cpu->regs.S - 1;
        if (cpu->pins.emulation) {
            tmp_addr = CLEM_UTIL_set16_lo(cpu->regs.S, tmp_addr);
        }
        clem_read(clem, &tmp_datahi, tmp_addr, 0x00, CLEM_MEM_FLAG_DATA);
        _cpu_sp_dec2(cpu);
        clem_read(clem, &tmp_data, cpu->regs.S, 0x00, CLEM_MEM_FLAG_DATA);
        _cpu_sp_dec(cpu);

        // vector pull low signal while the PC is being loaded
        clem_read(clem, &tmp_data, CLEM_6502_RESET_VECTOR_LO_ADDR, 0x00,
                       CLEM_MEM_FLAG_PROGRAM);
        clem_read(clem, &tmp_datahi, CLEM_6502_RESET_VECTOR_HI_ADDR,
                       0x00, CLEM_MEM_FLAG_PROGRAM);
        cpu->regs.PC = (uint16_t)(tmp_datahi << 8) | tmp_data;

        cpu->state_type = kClemensCPUStateType_Execute;
        return;
    } else if (cpu->state_type == kClemensCPUStateType_IRQ) {
        uint8_t tmp_data;
        uint8_t tmp_datahi;
        // 2 cycles of 'internal ops'
        // 2 cycles of pushing PC (next instruction to run)
        // 1 cycle push status reg
        // 2 cycles vector pull to PC
        // disable interrupts, clear decimal mode
        _clem_cycle(clem, 2);
        _clem_opc_push_pc16(clem, cpu->regs.PC);
        _clem_opc_push_status(clem, false);
        // vector pull low signal while the PC is being loaded
        clem_read(clem, &tmp_data, CLEM_6502_IRQBRK_VECTOR_LO_ADDR, 0x00,
                       CLEM_MEM_FLAG_PROGRAM);
        clem_read(clem, &tmp_datahi, CLEM_6502_IRQBRK_VECTOR_HI_ADDR,
                       0x00, CLEM_MEM_FLAG_PROGRAM);
        cpu->regs.PC = (uint16_t)(tmp_datahi << 8) | tmp_data;
        cpu->regs.P |= kClemensCPUStatus_IRQDisable;
        cpu->regs.P &= ~kClemensCPUStatus_Decimal;
        cpu->state_type = kClemensCPUStateType_Execute;
        return;
    }

    cpu_execute(cpu, clem);

    delta_mega2_cycles = (uint32_t)(
        (clem->clocks_spent / clem->clocks_step_mega2) - mmio->mega2_cycles);
    mmio->mega2_cycles += delta_mega2_cycles;
    mmio->timer_60hz_us += delta_mega2_cycles;

    /* background execution of some async devices on the 60 hz timer */
    while (mmio->timer_60hz_us >= CLEM_MEGA2_CYCLES_PER_60TH) {
        mmio->irq_line = clem_timer_sync(
            &mmio->dev_timer, CLEM_MEGA2_CYCLES_PER_60TH, mmio->irq_line);
        mmio->irq_line = clem_adb_glu_sync(
            &mmio->dev_adb, CLEM_MEGA2_CYCLES_PER_60TH, mmio->irq_line);
        mmio->timer_60hz_us -= CLEM_MEGA2_CYCLES_PER_60TH;
    }

    /* IRQB low triggers an interrupt next frame */
    cpu->pins.irqbIn = mmio->irq_line == 0;
    if (!cpu->pins.irqbIn && cpu->state_type == kClemensCPUStateType_Execute) {
        if (!(cpu->regs.P & kClemensCPUStatus_IRQDisable)) {
            cpu->state_type = kClemensCPUStateType_IRQ;
        }
    }
}

void clemens_input(
    ClemensMachine* machine,
    const struct ClemensInputEvent* input
) {
    clem_adb_device_input(&machine->mmio.dev_adb, input);
}
