#include "clem_smartport.h"
#include "clem_defs.h"

bool clem_smartport_do_reset(struct ClemensSmartPortUnit *unit, unsigned unit_count,
                             unsigned *io_flags, unsigned *out_phase, unsigned delta_ns) {
    unsigned select_bits = *out_phase;
    if (select_bits != (1 + 4)) {
        return false;
    }
    return true;
}

bool clem_smartport_do_enable(struct ClemensSmartPortUnit *unit, unsigned unit_count,
                              unsigned *io_flags, unsigned *out_phase, unsigned delta_ns) {
    unsigned select_bits = *out_phase;
    if (!(select_bits & 2) || !(select_bits & 8)) {
        return false;
    }

    //  ACK is HIGH for now
    *io_flags |= CLEM_IWM_FLAG_WRPROTECT_SENSE;

    return true;
}
