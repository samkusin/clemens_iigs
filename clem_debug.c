#include "clem_debug.h"
#include "clem_device.h"

static void _clem_print_io_reg_counters(struct ClemensDeviceDebugger* dbg) {
    for (unsigned i = 0; i < 256; ++i) {
        if (dbg->ioreg_read_ctr[i] || dbg->ioreg_write_ctr[i]) {
            CLEM_LOG("IO %02X RW (%u, %u)", i & 0xff,
                                            dbg->ioreg_read_ctr[i],
                                            dbg->ioreg_write_ctr[i]);
        }
    }
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
    switch (debug_reason) {
        case CLEM_DEBUG_UNIMPL_IOREAD:
            _clem_print_io_reg_counters(dbg);
            CLEM_UNIMPLEMENTED("IO Read: %04X, %02X", param0, param1);
            break;
        case CLEM_DEBUG_UNIMPL_IOWRITE:
            _clem_print_io_reg_counters(dbg);
            CLEM_UNIMPLEMENTED("IO Write: %04X, %02X", param0, param1);
            break;
        default:
            break;
    }

}
