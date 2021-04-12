#include "clem_types.h"
#include "clem_mem.h"


static inline void _clem_cycle(
    ClemensMachine* clem,
    uint32_t cycle_count
) {
    clem->clocks_spent += clem->clocks_step * cycle_count;
    ++clem->cpu.cycles_spent;
}

static inline void _cpu_p_flags_n_data(
    struct Clemens65C816* cpu,
    uint8_t data
) {
    if (data & 0x80) {
        cpu->regs.P |= kClemensCPUStatus_Negative;
    } else {
        cpu->regs.P &= ~kClemensCPUStatus_Negative;
    }
}

static inline void _cpu_p_flags_n_data_16(
    struct Clemens65C816* cpu,
    uint16_t data
) {
    if (data & 0x8000) {
        cpu->regs.P |= kClemensCPUStatus_Negative;
    } else {
        cpu->regs.P &= ~kClemensCPUStatus_Negative;
    }
}

static inline void _cpu_p_flags_z_data(
    struct Clemens65C816* cpu,
    uint8_t data
) {
    if (data) {
        cpu->regs.P &= ~kClemensCPUStatus_Zero;
    } else {
        cpu->regs.P |= kClemensCPUStatus_Zero;
    }
}

static inline void _cpu_p_flags_z_data_16(
    struct Clemens65C816* cpu,
    uint16_t data
) {
    if (data) {
        cpu->regs.P &= ~kClemensCPUStatus_Zero;
    } else {
        cpu->regs.P |= kClemensCPUStatus_Zero;
    }
}

static inline void _cpu_p_flags_n_z_data(
    struct Clemens65C816* cpu,
    uint8_t data
) {
    _cpu_p_flags_n_data(cpu, data);
    _cpu_p_flags_z_data(cpu, data);
}

static inline void _cpu_p_flags_n_z_data_16(
    struct Clemens65C816* cpu,
    uint16_t data
) {
    _cpu_p_flags_n_data_16(cpu, data);
    _cpu_p_flags_z_data_16(cpu, data);
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

static inline void _cpu_p_flags_apply_m_x(
    struct Clemens65C816* cpu
) {
    bool idx8 = (cpu->regs.P & kClemensCPUStatus_Index) != 0;
    if (idx8) {
        cpu->regs.X &= 0xff;
        cpu->regs.Y &= 0xff;
    } else {
        cpu->regs.P &= ~kClemensCPUStatus_Index;
    }
}

static inline void _cpu_sp_dec3(
    struct Clemens65C816* cpu
) {
    uint16_t tmp = cpu->regs.S - 3;
    if (cpu->pins.emulation) {
        tmp = (cpu->regs.S & 0xff00) | (tmp & 0x00ff);
    }
    cpu->regs.S = tmp;
}

static inline void _cpu_sp_dec2(
    struct Clemens65C816* cpu
) {
    uint16_t tmp = cpu->regs.S - 2;
    if (cpu->pins.emulation) {
        tmp = (cpu->regs.S & 0xff00) | (tmp & 0x00ff);
    }
    cpu->regs.S = tmp;
}

static inline void _cpu_sp_dec(
    struct Clemens65C816* cpu
) {
    uint16_t tmp = cpu->regs.S - 1;
    if (cpu->pins.emulation) {
        tmp = (cpu->regs.S & 0xff00) | (tmp & 0x00ff);
    }
    cpu->regs.S = tmp;
}

static inline void _cpu_sp_inc3(
    struct Clemens65C816* cpu
) {
    uint16_t tmp = cpu->regs.S + 3;
    if (cpu->pins.emulation) {
        tmp = (cpu->regs.S & 0xff00) | (tmp & 0x00ff);
    }
    cpu->regs.S = tmp;
}

static inline void _cpu_sp_inc2(
    struct Clemens65C816* cpu
) {
    uint16_t tmp = cpu->regs.S + 2;
    if (cpu->pins.emulation) {
        tmp = (cpu->regs.S & 0xff00) | (tmp & 0x00ff);
    }
    cpu->regs.S = tmp;
}

static inline void _cpu_sp_inc(
    struct Clemens65C816* cpu
) {
    uint16_t tmp = cpu->regs.S + 1;
    if (cpu->pins.emulation) {
        tmp = (cpu->regs.S & 0xff00) | (tmp & 0x00ff);
    }
    cpu->regs.S = tmp;
}


/*  Memory Reads and Writes:
    Requirements:
        Handle FPI access to ROM
        Handle FPI and MEGA2 fast and slow accesses to RAM
        Handle Access based on the Shadow Register


*/
static inline void _clem_next_dbr(
    ClemensMachine* clem,
    uint8_t* next_dbr,
    uint8_t dbr
) {
    if (!clem->cpu.pins.emulation) {
        *next_dbr = dbr + 1;
    } else {
        *next_dbr = dbr;
    }
}

static inline void _clem_write_16(
    ClemensMachine* clem,
    uint16_t data,
    uint16_t adr,
    uint8_t bank
) {
    clem_write(clem, (uint8_t)data, adr, bank, CLEM_MEM_FLAG_DATA);
    clem_write(clem, (uint8_t)(data >> 8), adr + 1, bank, CLEM_MEM_FLAG_DATA);
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
    clem_read(clem, &tmp_data, adr, bank, flags);
    *data16 = tmp_data;
    clem_read(clem, &tmp_data, adr + 1, bank, flags);
    *data16 = ((uint16_t)tmp_data << 8) | (*data16);
}

static inline void _clem_read_pba(
    ClemensMachine* clem,
    uint8_t* data,
    uint16_t* pc
) {
    clem_read(clem, data, (*pc)++, clem->cpu.regs.PBR,
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

static inline void _clem_read_data_816(
    ClemensMachine* clem,
    uint16_t* out,
    uint16_t addr,
    uint8_t dbr,
    bool is8
) {
    uint8_t tmp_data;
    clem_read(
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
        clem_read(
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
    uint16_t eff_index = is_index_8 ? (index & 0xff) : index;
    uint16_t eff_addr = addr + eff_index;
    if (eff_addr < addr && !clem->cpu.pins.emulation) {
        _clem_next_dbr(clem, &dbr_actual, dbr);
    } else {
        dbr_actual = dbr;
    }
    if (!is_index_8 || CLEM_UTIL_CROSSED_PAGE_BOUNDARY(addr, eff_addr)) {
        //  indexed address crossing a page boundary adds a cycle
        _clem_cycle(clem, 1);
    }
    _clem_read_data_816(clem, out, eff_addr, dbr_actual, is_data_8);
}

static inline void _clem_opc_push_reg_816(
    ClemensMachine* clem,
    uint16_t data,
    bool is8
) {
    struct Clemens65C816* cpu = &clem->cpu;
    _clem_cycle(clem, 1);
    if (!is8) {
        clem_write(clem, (uint8_t)(data >> 8), cpu->regs.S, 0x00, CLEM_MEM_FLAG_DATA);
        _cpu_sp_dec(cpu);
    }
    clem_write(clem, (uint8_t)(data), cpu->regs.S, 0x00, CLEM_MEM_FLAG_DATA);
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
    clem_read(clem, &data8, cpu->regs.S, 0x00, CLEM_MEM_FLAG_DATA);
    *data = CLEM_UTIL_set16_lo(*data, data8);
    if (!is8) {
        _cpu_sp_inc(cpu);
        clem_read(clem, &data8, cpu->regs.S, 0x00, CLEM_MEM_FLAG_DATA);
        *data = CLEM_UTIL_set16_lo((uint16_t)(data8) << 8, *data);
    }
}

static inline void _clem_opc_pull_reg_8(
    ClemensMachine* clem,
    uint8_t* data
) {
    struct Clemens65C816* cpu = &clem->cpu;
    _clem_cycle(clem, 2);
    _cpu_sp_inc(cpu);
    clem_read(clem, data, cpu->regs.S, 0x00, CLEM_MEM_FLAG_DATA);
}

static inline void _clem_opc_push_status(
    ClemensMachine* clem,
    bool is_brk
) {
    uint8_t tmp_data = clem->cpu.regs.P;
    if (clem->cpu.pins.emulation) {
        if (is_brk) {
            tmp_data |= kClemensCPUStatus_EmulatedBrk;
        } else {
            tmp_data &= ~kClemensCPUStatus_EmulatedBrk;
        }
    }
    clem_write(clem, tmp_data, clem->cpu.regs.S, 0x00, CLEM_MEM_FLAG_DATA);
    _cpu_sp_dec(&clem->cpu);
}

static inline void _clem_opc_pull_status(
    ClemensMachine* clem
) {
    uint8_t tmp_p;
    _cpu_sp_inc(&clem->cpu);
    clem_read(clem, &tmp_p, clem->cpu.regs.S, 0x00, CLEM_MEM_FLAG_DATA);

    if (clem->cpu.pins.emulation) {
        tmp_p |= kClemensCPUStatus_MemoryAccumulator;
        tmp_p |= kClemensCPUStatus_Index;
    }
    clem->cpu.regs.P = tmp_p;
    _cpu_p_flags_apply_m_x(&clem->cpu);
}

static inline void _clem_opc_push_pc16(
    ClemensMachine* clem,
    uint16_t pc
) {
    uint16_t tmp_s = clem->cpu.regs.S;
    //  stack receives last address of operand
    clem_write(clem, (uint8_t)(pc >> 8), tmp_s, 0x00, CLEM_MEM_FLAG_DATA);
    --tmp_s;
    if (clem->cpu.pins.emulation) {
        tmp_s = CLEM_UTIL_set16_lo(clem->cpu.regs.S, tmp_s);
    }
    clem_write(clem, (uint8_t)pc, tmp_s, 0x00, CLEM_MEM_FLAG_DATA);
    _cpu_sp_dec2(&clem->cpu);
}


/*  Handle opcode addressing modes
*/
static inline void _clem_read_pba_mode_imm_816(
  ClemensMachine* clem,
  uint16_t* imm,
  uint16_t* pc,
  bool is8
) {
    _clem_read_pba_816(clem, imm, pc, is8);
}

static inline void _clem_read_pba_mode_abs(
  ClemensMachine* clem,
  uint16_t* addr,
  uint16_t* pc
) {
  _clem_read_pba_16(clem, addr, pc);
}

static inline void _clem_read_pba_mode_absl(
  ClemensMachine* clem,
  uint16_t* addr,
  uint8_t* dbr,
  uint16_t* pc
) {
    _clem_read_pba_16(clem, addr, pc);
    _clem_read_pba(clem, dbr, pc);
}


static inline void _clem_read_pba_mode_dp(
    ClemensMachine* clem,
    uint16_t* eff_addr,
    uint16_t* pc,
    uint8_t* offset,
    uint16_t index,
    bool is_index_8
) {
    uint16_t D = clem->cpu.regs.D;
    uint16_t offset_index = is_index_8 ? (index & 0xff) : index;

    _clem_read_pba(clem, offset, pc);
    offset_index += *offset;
    if (clem->cpu.pins.emulation) {
        *eff_addr = (D & 0xff00) + ((D & 0xff) + offset_index) % 256;
    } else {
        *eff_addr = D + *offset + index;
    }
    if (D & 0x00ff) {
        _clem_cycle(clem, 1);
    }
}


static inline void _clem_read_pba_mode_dp_indirect(
    ClemensMachine* clem,
    uint16_t* eff_addr,
    uint16_t* pc,
    uint8_t* offset,
    uint16_t index,
    bool is_index_8
) {
    uint16_t tmp_addr;
    _clem_read_pba_mode_dp(clem, &tmp_addr, pc, offset, index, is_index_8);
    _clem_read_16(clem, eff_addr, tmp_addr, 0x00, CLEM_MEM_FLAG_DATA);
}

static inline void _clem_read_pba_mode_dp_indirectl(
    ClemensMachine* clem,
    uint16_t* eff_addr,
    uint8_t* eff_bank,
    uint16_t* pc,
    uint8_t* offset,
    uint16_t index,
    bool is_index_8
) {
    uint16_t tmp_addr;
    _clem_read_pba_mode_dp(clem, &tmp_addr, pc, offset, index, is_index_8);
    _clem_read_16(clem, eff_addr, tmp_addr, 0x00, CLEM_MEM_FLAG_DATA);
    //  TODO: direct page wrap? (DH, DL=255 + 1 = DH, 0)?
    clem_read(clem, eff_bank, tmp_addr + 1, 0x00, CLEM_MEM_FLAG_DATA);
}

static inline void _clem_read_pba_mode_stack_rel(
    ClemensMachine* clem,
    uint16_t* addr,
    uint16_t* pc,
    uint8_t* offset
) {
    _clem_read_pba(clem, offset, pc);
    _clem_cycle(clem, 1);   //  extra IO
    *addr = clem->cpu.regs.S + *offset;
}

static inline void _clem_read_pba_mode_stack_rel_indirect(
    ClemensMachine* clem,
    uint16_t* addr,
    uint16_t* pc,
    uint8_t* offset
) {
    uint16_t tmp_addr;
    _clem_read_pba_mode_stack_rel(clem, &tmp_addr, pc, offset);
    _clem_read_16(clem, addr, tmp_addr, 0x00, CLEM_MEM_FLAG_DATA);
    _clem_cycle(clem, 1);   //  extra IO
}

static inline void _cpu_adc(
    struct Clemens65C816* cpu,
    uint16_t value,
    bool is8
) {
    uint32_t adc;
    uint8_t p;
    bool carry = (cpu->regs.P & kClemensCPUStatus_Carry) != 0;
    if (is8) {
        value = value & 0xff;
        adc = (cpu->regs.A & 0xff) + value  + carry;
        _cpu_p_flags_n_z_data(cpu, (uint8_t)adc);
        p = cpu->regs.P;
        if (((cpu->regs.A & 0xff) ^ adc) & (value ^ adc) & 0x80) p |= kClemensCPUStatus_Overflow;
        else p &= ~kClemensCPUStatus_Overflow;
        if (adc & 0x100) p |= kClemensCPUStatus_Carry;
        else p &= ~kClemensCPUStatus_Carry;
        cpu->regs.A = CLEM_UTIL_set16_lo(cpu->regs.A, adc);
    } else {
        adc = cpu->regs.A + value + carry;
        _cpu_p_flags_n_z_data_16(cpu, (uint16_t)adc);
        p = cpu->regs.P;
        if ((cpu->regs.A ^ adc) & (value ^ adc) & 0x8000) p |= kClemensCPUStatus_Overflow;
        else p &= ~kClemensCPUStatus_Overflow;
        if (adc & 0x10000) p |= kClemensCPUStatus_Carry;
        else p &= ~kClemensCPUStatus_Carry;
        cpu->regs.A = (uint16_t)adc;
    }
    cpu->regs.P = p;
}


static inline void _cpu_sbc(
    struct Clemens65C816* cpu,
    uint16_t value,
    bool is8
) {
    /* inverse adc implementation a + (-b) */
    uint32_t adc;
    uint8_t p;
    bool carry = (cpu->regs.P & kClemensCPUStatus_Carry) != 0;
    if (is8) {
        uint16_t a = cpu->regs.A & 0xff;
        value = value & 0xff;
        value = value ^ 0xff;   // convert to negative
        adc = a + value + carry;
        _cpu_p_flags_n_z_data(cpu, (uint8_t)adc);
        p = cpu->regs.P;
        if ((a ^ adc) & (value ^ adc) & 0x80) p |= kClemensCPUStatus_Overflow;
        else p &= ~kClemensCPUStatus_Overflow;
        if (adc & 0x100) p |= kClemensCPUStatus_Carry;
        else p &= ~kClemensCPUStatus_Carry;
        cpu->regs.A = CLEM_UTIL_set16_lo(cpu->regs.A, adc);
    } else {
        value = value ^ 0xffff; // negative
        adc = cpu->regs.A + value + carry;
        _cpu_p_flags_n_z_data_16(cpu, (uint16_t)adc);
        p = cpu->regs.P;
        if ((cpu->regs.A ^ adc) & (value ^ adc) & 0x8000) p |= kClemensCPUStatus_Overflow;
        else p &= ~kClemensCPUStatus_Overflow;
        if (adc & 0x10000) p |= kClemensCPUStatus_Carry;
        else p &= ~kClemensCPUStatus_Carry;
        cpu->regs.A = (uint16_t)adc;
    }
    cpu->regs.P = p;
}

static inline void _cpu_asl(
    struct Clemens65C816* cpu,
    uint16_t *value,
    bool is8
) {
    uint8_t p = cpu->regs.P;
    if (is8) {
        uint8_t v = (uint8_t)(*value);
        if (v & 0x80) p |= kClemensCPUStatus_Carry;
        else p &= ~kClemensCPUStatus_Carry;
        v <<= 1;
        cpu->regs.P = p;
        _cpu_p_flags_n_z_data(cpu, v);
        *value = CLEM_UTIL_set16_lo(*value, v);
    } else {
        if (*value & 0x8000) p |= kClemensCPUStatus_Carry;
        else p &= ~kClemensCPUStatus_Carry;
        *value <<= 1;
        cpu->regs.P = p;
        _cpu_p_flags_n_z_data_16(cpu, *value);
    }
}

static inline void _cpu_rol(
    struct Clemens65C816* cpu,
    uint16_t *value,
    bool is8
) {
    uint8_t p = cpu->regs.P;
    if (is8) {
        uint8_t v = (uint8_t)(*value);
        if (v & 0x80) p |= kClemensCPUStatus_Carry;
        else p &= ~kClemensCPUStatus_Carry;
        v <<= 1;
        if (p & kClemensCPUStatus_Carry) v |= 0x01;
        else v &= 0xfe;
        cpu->regs.P = p;
        _cpu_p_flags_n_z_data(cpu, v);
        *value = CLEM_UTIL_set16_lo(*value, v);
    } else {
        if (*value & 0x8000) p |= kClemensCPUStatus_Carry;
        else p &= ~kClemensCPUStatus_Carry;
        *value <<= 1;
        if (p & kClemensCPUStatus_Carry) *value |= 0x0001;
        else *value &= 0xfffe;
        cpu->regs.P = p;
        _cpu_p_flags_n_z_data_16(cpu, *value);
    }
}

static inline void _cpu_lsr(
    struct Clemens65C816* cpu,
    uint16_t *value,
    bool is8
) {
    uint8_t p = cpu->regs.P;
    if (is8) {
        uint8_t v = (uint8_t)(*value);
        if (v & 0x01) p |= kClemensCPUStatus_Carry;
        else p &= ~kClemensCPUStatus_Carry;
        v >>= 1;
        cpu->regs.P = p;
        _cpu_p_flags_n_z_data(cpu, v);
        *value = CLEM_UTIL_set16_lo(*value, v);
    } else {
        if (*value & 0x0001) p |= kClemensCPUStatus_Carry;
        else p &= ~kClemensCPUStatus_Carry;
        *value >>= 1;
        cpu->regs.P = p;
        _cpu_p_flags_n_z_data_16(cpu, *value);
    }
}

static inline void _cpu_ror(
    struct Clemens65C816* cpu,
    uint16_t *value,
    bool is8
) {
    uint8_t p = cpu->regs.P;
    if (is8) {
        uint8_t v = (uint8_t)(*value);
        if (v & 0x01) p |= kClemensCPUStatus_Carry;
        else p &= ~kClemensCPUStatus_Carry;
        v >>= 1;
        if (p & kClemensCPUStatus_Carry) v |= 0x80;
        else v &= 0x7f;
        cpu->regs.P = p;
        _cpu_p_flags_n_z_data(cpu, v);
        *value = CLEM_UTIL_set16_lo(*value, v);
    } else {
        if (*value & 0x0001) p |= kClemensCPUStatus_Carry;
        else p &= ~kClemensCPUStatus_Carry;
        *value >>= 1;
        if (p & kClemensCPUStatus_Carry) *value |= 0x8000;
        else *value &= 0x7fff;
        cpu->regs.P = p;
        _cpu_p_flags_n_z_data_16(cpu, *value);
    }
}

static inline void _cpu_cmp(
    struct Clemens65C816* cpu,
    uint16_t reg,
    uint16_t value,
    bool is8
) {
    uint32_t cmp;
    uint8_t p = cpu->regs.P;
    if (is8) {
        value = value & 0xff;
        cmp = (reg & 0xff);
        if (cmp >= value) p |= kClemensCPUStatus_Carry;
        else p &= ~kClemensCPUStatus_Carry;
        cmp -= value;
        cpu->regs.P = p;
        _cpu_p_flags_n_z_data(cpu, (uint8_t)cmp);
    } else {
        if (reg >= value) p |= kClemensCPUStatus_Carry;
        else p &= ~kClemensCPUStatus_Carry;
        cmp = reg;
        cmp -= value;
        cpu->regs.P = p;
        _cpu_p_flags_n_z_data_16(cpu, (uint16_t)cmp);
    }
}

static inline void _cpu_and(
    struct Clemens65C816* cpu,
    uint16_t value,
    bool is8
) {
    uint16_t and;
    if (is8) {
        value = value & 0xff;
        and = (cpu->regs.A & 0xff) & value;
        _cpu_p_flags_n_z_data(cpu, (uint8_t)and);
        cpu->regs.A = CLEM_UTIL_set16_lo(cpu->regs.A, and);
    } else {
        and = cpu->regs.A & value;
        _cpu_p_flags_n_z_data_16(cpu, and);
        cpu->regs.A = and;
    }
}

static inline void _cpu_eor(
    struct Clemens65C816* cpu,
    uint16_t value,
    bool is8
) {
    uint16_t eor;
    if (is8) {
        value = value & 0xff;
        eor = (cpu->regs.A & 0xff) ^ value;
        _cpu_p_flags_n_z_data(cpu, (uint8_t)eor);
        cpu->regs.A = CLEM_UTIL_set16_lo(cpu->regs.A, eor);
    } else {
        eor = cpu->regs.A ^ value;
        _cpu_p_flags_n_z_data_16(cpu, eor);
        cpu->regs.A = eor;
    }
}

static inline void _cpu_ora(
    struct Clemens65C816* cpu,
    uint16_t value,
    bool is8
) {
    uint16_t ora;
    if (is8) {
        value = value & 0xff;
        ora = (cpu->regs.A & 0xff) | value;
        _cpu_p_flags_n_z_data(cpu, (uint8_t)ora);
        cpu->regs.A = CLEM_UTIL_set16_lo(cpu->regs.A, ora);
    } else {
        ora = cpu->regs.A | value;
        _cpu_p_flags_n_z_data_16(cpu, ora);
        cpu->regs.A = ora;
    }
}

static inline void _cpu_bit(
    struct Clemens65C816* cpu,
    uint16_t value,
    bool is8
) {
    if (is8) {
        uint8_t v = (uint8_t)(value);
        uint8_t a = (uint8_t)(cpu->regs.A);
        if (v & 0x40) cpu->regs.P |= kClemensCPUStatus_Overflow;
        else cpu->regs.P &= ~kClemensCPUStatus_Overflow;
        _cpu_p_flags_n_data(cpu, v);
        _cpu_p_flags_z_data(cpu, v & a);

    } else {
        if (value & 0x4000) cpu->regs.P |= kClemensCPUStatus_Overflow;
        else cpu->regs.P &= ~kClemensCPUStatus_Overflow;
        _cpu_p_flags_n_data_16(cpu, value);
        _cpu_p_flags_z_data_16(cpu, value & cpu->regs.A);
    }
}

static inline void _cpu_inc(
    struct Clemens65C816* cpu,
    uint16_t* value,
    bool is8
) {
    if (is8) {
        uint8_t v = (uint8_t)(*value);
        ++v;
        _cpu_p_flags_n_z_data(cpu, v);
        *value = CLEM_UTIL_set16_lo(*value, v);
    } else {
        *value += 1;
        _cpu_p_flags_n_z_data_16(cpu, *value);
    }
}

static inline void _cpu_dec(
    struct Clemens65C816* cpu,
    uint16_t* value,
    bool is8
) {
    if (is8) {
        uint8_t v = (uint8_t)(*value);
        --v;
        _cpu_p_flags_n_z_data(cpu, v);
        *value = CLEM_UTIL_set16_lo(*value, v);
    } else {
        *value -= 1;
        _cpu_p_flags_n_z_data_16(cpu, *value);
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

static inline void _cpu_ldxy(
    struct Clemens65C816* cpu,
    uint16_t* reg,
    uint16_t value,
    bool is8
) {
    if (is8) {
        _cpu_p_flags_n_z_data(cpu, (uint8_t)value);
        *reg = CLEM_UTIL_set16_lo(*reg, value);
    } else {
        _cpu_p_flags_n_z_data_16(cpu, value);
        *reg = value;
    }
}

static inline void _cpu_trb(
    struct Clemens65C816* cpu,
    uint16_t* value,
    bool is8
) {
    if (is8) {
        uint8_t v = (uint8_t)(*value);
        uint8_t a = (uint8_t)(cpu->regs.A);
        v &= ~a;
        _cpu_p_flags_z_data(cpu, v & a);
        cpu->regs.A = CLEM_UTIL_set16_lo(cpu->regs.A, v);
    } else {
         *value &= ~cpu->regs.A;
        _cpu_p_flags_z_data_16(cpu, *value & cpu->regs.A);
    }
}

static inline void _cpu_tsb(
    struct Clemens65C816* cpu,
    uint16_t* value,
    bool is8
) {
    if (is8) {
        uint8_t v = (uint8_t)(*value);
        uint8_t a = (uint8_t)(cpu->regs.A);
        v |= a;
        _cpu_p_flags_z_data(cpu, v & a);
        cpu->regs.A = CLEM_UTIL_set16_lo(cpu->regs.A, v);
    } else {
         *value |= cpu->regs.A;
        _cpu_p_flags_z_data_16(cpu, *value & cpu->regs.A);
    }
}

static inline void _clem_write_816(
    ClemensMachine* clem,
    uint16_t value,
    uint16_t addr,
    uint8_t dbr,
    bool is8
) {
    if (is8) {
        clem_write(clem, (uint8_t)value, addr, dbr, CLEM_MEM_FLAG_DATA);
    } else {
        _clem_write_16(clem, value, addr, dbr);
    }
}

static inline void _clem_write_indexed_816(
    ClemensMachine* clem,
    uint16_t value,
    uint16_t addr,
    uint16_t index,
    uint8_t dbr,
    bool is_data_8,
    bool is_index_8
) {
    uint16_t eff_index = is_index_8 ? (index & 0xff) : index;
    uint16_t eff_addr = addr + eff_index;
    uint8_t dbr_actual;
    if (eff_addr < addr && !clem->cpu.pins.emulation) {
        _clem_next_dbr(clem, &dbr_actual, dbr);
    } else {
        dbr_actual = dbr;
    }
    if (is_data_8) {
        clem_write(clem, (uint8_t)value, eff_addr, dbr_actual, CLEM_MEM_FLAG_DATA);
    } else {
        _clem_write_16(clem, value, eff_addr, dbr_actual);
    }
}

static inline void _clem_branch(
    ClemensMachine* clem,
    uint16_t* pc,
    int8_t offset,
    bool do_branch
) {
    if (!do_branch) return;
    uint16_t tmp_addr = *pc + offset;
    if (clem->cpu.pins.emulation &&
        CLEM_UTIL_CROSSED_PAGE_BOUNDARY(*pc, tmp_addr)) {
        _clem_cycle(clem, 1);
    }
    _clem_cycle(clem, 1);
    *pc = tmp_addr;
}
