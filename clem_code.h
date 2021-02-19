#include "clem_types.h"

static inline void _clem_cycle(
    ClemensMachine* clem,
    uint32_t cycle_count
) {
    clem->clocks_spent += clem->clocks_step * cycle_count;
    ++clem->cpu.cycles_spent;
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
    if (!clem->cpu.emulation) {
        *next_dbr = dbr + 1;
    } else {
        *next_dbr = dbr;
    }
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
    uint16_t eff_index = is_index_8 ? (index & 0xff) : index;
    uint16_t eff_addr = addr + eff_index;
    if (eff_addr < addr && !clem->cpu.emulation) {
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
    if (clem->cpu.emulation) {
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
    _clem_read(clem, eff_bank, tmp_addr + 1, 0x00, CLEM_MEM_FLAG_DATA);
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
    uint8_t p = cpu->regs.P;
    bool carry = (cpu->regs.P & kClemensCPUStatus_Carry) != 0;
    if (is8) {
        value = value & 0xff;
        adc = (cpu->regs.A & 0xff) + value  + carry;
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
    cpu->regs.P = p;
}

static inline void _cpu_sbc(
    struct Clemens65C816* cpu,
    uint16_t value,
    bool is8
) {
    /* inverse adc implementation a + (-b) */
    uint32_t adc;
    uint8_t p = cpu->regs.P;
    bool carry = (cpu->regs.P & kClemensCPUStatus_Carry) != 0;
    if (is8) {
        uint16_t a = cpu->regs.A & 0xff;
        value = value & 0xff;
        value = value ^ 0xff;   // convert to negative
        adc = a + value + carry;
        if (!(adc & 0xff)) p |= kClemensCPUStatus_Zero;
        if (adc & 0x80) p |= kClemensCPUStatus_Negative;
        if ((a ^ adc) & (value ^ adc) & 0x80) {
            p |= kClemensCPUStatus_Overflow;
        }
        if (adc & 0x100) p |= kClemensCPUStatus_Carry;
        cpu->regs.A = CLEM_UTIL_set16_lo(cpu->regs.A, (uint8_t)adc);
    } else {
        value = value ^ 0xffff; // negative
        adc = cpu->regs.A + value + carry;
        if (!(adc & 0xffff)) p |= kClemensCPUStatus_Zero;
        if (adc & 0x8000) p |= kClemensCPUStatus_Negative;
        if ((cpu->regs.A ^ adc) & (value ^ adc) & 0x8000) {
            p |= kClemensCPUStatus_Overflow;
        }
        if (adc & 0x10000) p |= kClemensCPUStatus_Carry;
        cpu->regs.A = (uint16_t)adc;
    }
    cpu->regs.P = p;
}

static inline void _cpu_and(
    struct Clemens65C816* cpu,
    uint16_t value,
    bool is8
) {
    uint16_t and;
    uint8_t p = cpu->regs.P;
    if (is8) {
        value = value & 0xff;
        and = (cpu->regs.A & 0xff) & value;
        if (!(and & 0xff)) p |= kClemensCPUStatus_Zero;
        if (and & 0x80) p |= kClemensCPUStatus_Negative;
        cpu->regs.A = CLEM_UTIL_set16_lo(cpu->regs.A, (uint8_t)and);
    } else {
        and = cpu->regs.A & value;
        if (!(and & 0xffff)) p |= kClemensCPUStatus_Zero;
        if (and & 0x8000) p |= kClemensCPUStatus_Negative;
        cpu->regs.A = and;
    }
    cpu->regs.P = p;
}

static inline void _cpu_cmp(
    struct Clemens65C816* cpu,
    uint16_t value,
    bool is8
) {
    uint32_t cmp;
    uint8_t p = cpu->regs.P;
    if (is8) {
        value = value & 0xff;
        cmp = (cpu->regs.A & 0xff);
        if (cmp >= value) p |= kClemensCPUStatus_Carry;
        cmp -= value;
        if (!(cmp & 0xff)) p |= kClemensCPUStatus_Zero;
        if (cmp & 0x80) p |= kClemensCPUStatus_Negative;
    } else {
        if (cpu->regs.A >= value) p |= kClemensCPUStatus_Carry;
        cmp = cpu->regs.A - value;
        if (!(cmp & 0xffff)) p |= kClemensCPUStatus_Zero;
        if (cmp & 0x8000) p |= kClemensCPUStatus_Negative;
    }
    cpu->regs.P = p;
}

static inline void _cpu_eor(
    struct Clemens65C816* cpu,
    uint16_t value,
    bool is8
) {
    uint16_t eor;
    uint8_t p = cpu->regs.P;
    if (is8) {
        value = value & 0xff;
        eor = (cpu->regs.A & 0xff) ^ value;
        if (!(eor & 0xff)) p |= kClemensCPUStatus_Zero;
        if (eor & 0x80) p |= kClemensCPUStatus_Negative;
        cpu->regs.A = CLEM_UTIL_set16_lo(cpu->regs.A, (uint8_t)eor);
    } else {
        eor = cpu->regs.A ^ value;
        if (!(eor & 0xffff)) p |= kClemensCPUStatus_Zero;
        if (eor & 0x8000) p |= kClemensCPUStatus_Negative;
        cpu->regs.A = eor;
    }
    cpu->regs.P = p;
}

static inline void _cpu_ora(
    struct Clemens65C816* cpu,
    uint16_t value,
    bool is8
) {
    uint16_t ora;
    uint8_t p = cpu->regs.P;
    if (is8) {
        value = value & 0xff;
        ora = (cpu->regs.A & 0xff) | value;
        if (!(ora & 0xff)) p |= kClemensCPUStatus_Zero;
        if (ora & 0x80) p |= kClemensCPUStatus_Negative;
        cpu->regs.A = CLEM_UTIL_set16_lo(cpu->regs.A, (uint8_t)ora);
    } else {
        ora = cpu->regs.A | value;
        if (!(ora & 0xffff)) p |= kClemensCPUStatus_Zero;
        if (ora & 0x8000) p |= kClemensCPUStatus_Negative;
        cpu->regs.A = ora;
    }
    cpu->regs.P = p;
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

static inline void _clem_write_816(
    ClemensMachine* clem,
    uint16_t value,
    uint8_t dbr,
    uint16_t addr,
    bool is8
) {
    if (is8) {
        _clem_write(clem, (uint8_t)value, addr, dbr);
    } else {
        _clem_write_16(clem, value, addr, dbr);
    }
}

static inline void _clem_write_indexed_816(
    ClemensMachine* clem,
    uint16_t value,
    uint8_t dbr,
    uint16_t addr,
    uint16_t index,
    bool is_data_8,
    bool is_index_8
) {
    uint16_t eff_index = is_index_8 ? (index & 0xff) : index;
    uint16_t eff_addr = addr + eff_index;
    uint8_t dbr_actual;
    if (eff_addr < addr && !clem->cpu.emulation) {
        _clem_next_dbr(clem, &dbr_actual, dbr);
    } else {
        dbr_actual = dbr;
    }
    if (is_data_8) {
        _clem_write(clem, (uint8_t)value, eff_addr, dbr_actual);
    } else {
        _clem_write_16(clem, value, eff_addr, dbr_actual);
    }
}
