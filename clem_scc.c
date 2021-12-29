#include "clem_device.h"
#include "clem_types.h"
#include "clem_mmio_defs.h"
#include "clem_debug.h"

#define CLEM_SCC_STATE_REGISTER_WAIT        0
#define CLEM_SCC_STATE_REGISTER_SELECTED    1



void clem_scc_reset(struct ClemensDeviceSCC* scc) {

}

void clem_scc_glu_sync(
    struct ClemensDeviceSCC* scc,
    struct ClemensClock* clock
) {
    scc->ts_last_frame = clock->ts;
}

void clem_scc_write_switch(
    struct ClemensDeviceSCC* scc,
    uint8_t ioreg, uint8_t value
) {

    switch (ioreg) {
        case CLEM_MMIO_REG_SCC_B_CMD:
        case CLEM_MMIO_REG_SCC_A_CMD:
            break;
        case CLEM_MMIO_REG_SCC_B_DATA:
        case CLEM_MMIO_REG_SCC_A_DATA:
            if (scc->state == CLEM_SCC_STATE_REGISTER_WAIT) {
                scc->selected_reg[CLEM_MMIO_REG_SCC_A_DATA - ioreg] = value;
                scc->state = CLEM_SCC_STATE_REGISTER_SELECTED;
            } else {
                scc->state = CLEM_SCC_STATE_REGISTER_WAIT;
            }

            break;
    }
    //CLEM_LOG("clem_scc: %02X <- %02X", ioreg, value);
}

uint8_t clem_scc_read_switch(
    struct ClemensDeviceSCC* scc,
    uint8_t ioreg,
    uint8_t flags
) {
    uint8_t value = 0;
    bool is_noop = (flags & CLEM_OP_IO_READ_NO_OP) != 0;
    switch (ioreg) {
        case CLEM_MMIO_REG_SCC_B_CMD:
        case CLEM_MMIO_REG_SCC_A_CMD:
            break;
        case CLEM_MMIO_REG_SCC_B_DATA:
        case CLEM_MMIO_REG_SCC_A_DATA:
            break;
    }
    //if (!is_noop) {
    //    CLEM_LOG("clem_scc: %02X -> %02X", ioreg, value);
    //}
    return value;
}
