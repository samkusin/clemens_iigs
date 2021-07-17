#include "clem_debug.h"
#include "clem_device.h"

#include <stdio.h>

static void _clem_print_io_reg_counters(struct ClemensDeviceDebugger* dbg) {
    for (unsigned i = 0; i < 256; ++i) {
        if (dbg->ioreg_read_ctr[i] || dbg->ioreg_write_ctr[i]) {
            CLEM_LOG("IO %02X RW (%u, %u)", i & 0xff,
                                            dbg->ioreg_read_ctr[i],
                                            dbg->ioreg_write_ctr[i]);
        }
    }
}

void clem_debug_reset(struct ClemensDeviceDebugger* dbg) {
    memset(dbg, 0, sizeof(*dbg));
}

void clem_debug_call_stack(struct ClemensDeviceDebugger* dbg) {
    for (unsigned i = 0; i < dbg->jsr_context_count; ++i) {
        struct ClemensDebugJSRContext* ctx = &dbg->jsr_contexts[i];
        CLEM_LOG("[%02X:%04X] %02X%04X, S=%04X", (ctx->adr >> 16) & 0xff,
                                                 ctx->adr & 0xffff,
                                                 (ctx->jmp >> 16) & 0xff,
                                                 ctx->jmp & 0xffff,
                                                 ctx->sp);
    }
}


void clem_debug_counters(struct ClemensDeviceDebugger* dbg) {
    _clem_print_io_reg_counters(dbg);
}

void clem_debug_break(
    struct ClemensDeviceDebugger* dbg,
    struct Clemens65C816* cpu,
    unsigned debug_reason,
    unsigned param0,
    unsigned param1
) {
    CLEM_WARN("PC=%02X:%04X: DBR=%02X P=%02X",
              cpu->regs.PBR, cpu->regs.PC, cpu->regs.DBR, cpu->regs.P);
    clem_debug_call_stack(dbg);
    switch (debug_reason) {
        case CLEM_DEBUG_BREAK_UNIMPL_IOREAD:
            _clem_print_io_reg_counters(dbg);
            CLEM_UNIMPLEMENTED("IO Read: %04X, %02X", param0, param1);
            break;
        case CLEM_DEBUG_BREAK_UNIMPL_IOWRITE:
            _clem_print_io_reg_counters(dbg);
            CLEM_UNIMPLEMENTED("IO Write: %04X, %02X", param0, param1);
            break;
        default:
            break;
    }
}

void clem_debug_jsr(
    struct ClemensDeviceDebugger* dbg,
    struct Clemens65C816* cpu,
    uint16_t next_pc,
    uint8_t pbr
) {
    struct ClemensDebugJSRContext* ctx;
    CLEM_ASSERT(dbg->jsr_context_count < CLEM_DEBUG_JSR_CONTEXT_LIMIT);
    if (dbg->jsr_context_count >= CLEM_DEBUG_JSR_CONTEXT_LIMIT) return;
    ctx = &dbg->jsr_contexts[dbg->jsr_context_count++];
    ctx->sp = cpu->regs.S;
    ctx->adr = ((unsigned)(cpu->regs.PBR) << 16) | cpu->regs.PC;
    ctx->jmp = ((unsigned)(pbr) << 16) | next_pc;
}

void clem_debug_rts(
    struct ClemensDeviceDebugger* dbg,
    struct Clemens65C816* cpu,
    uint16_t next_pc,
    uint8_t pbr
) {
    struct ClemensDebugJSRContext* ctx;

    if (dbg->jsr_context_count >= CLEM_DEBUG_JSR_CONTEXT_LIMIT) return;
    if (dbg->jsr_context_count == 0) return;

    ctx = &dbg->jsr_contexts[--dbg->jsr_context_count];
}
