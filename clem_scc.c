#include "clem_debug.h"
#include "clem_device.h"
#include "clem_mmio_defs.h"
#include "clem_mmio_types.h"

/*  SCC Implementation
    ==================
    This module implements communication between the machine and an emulated
    Zilog 8530.  Though not called out in the docs - the data/command per port
    interface is very similar to how the system communicates with the Ensoniq.

    Commands and data are funneled between the machine and emulated Zilog unit.
    Command sent from machine with data:    SCC_x_CMD,  SCC_x_DATA
    Zilog responds with data:               SCC_x_DATA

    The "GLU" will expect a command byte and a data byte

    The emulated SCC has the following components:

    - Command Data registers
    - Interrupt control logic
    - Channel controllers (I/O)


    These ports are used to communicate with a printer and modem (A, B)
    These "peripherals" will expect tx/rx from this module.
*/

struct ClemensDeviceSCC1 {};

#define CLEM_SCC_CH_A 0
#define CLEM_SCC_CH_B 1

#define CLEM_SCC_STATE_READY    0
#define CLEM_SCC_STATE_REGISTER 1

void clem_scc_reset(struct ClemensDeviceSCC *scc) {}

void clem_scc_glu_sync(struct ClemensDeviceSCC *scc, struct ClemensClock *clock) {
    scc->ts_last_frame = clock->ts;
}

void clem_scc_write_switch(struct ClemensDeviceSCC *scc, uint8_t ioreg, uint8_t value) {
    unsigned ch_idx;

    switch (ioreg) {
    case CLEM_MMIO_REG_SCC_B_CMD:
    case CLEM_MMIO_REG_SCC_A_CMD:
        ch_idx = CLEM_MMIO_REG_SCC_A_CMD - ioreg;
        if (scc->state[ch_idx] == CLEM_SCC_STATE_READY) {
            scc->selected_reg[ch_idx] = value;
            scc->state[ch_idx] = CLEM_SCC_STATE_REGISTER;
        } else if (scc->state[ch_idx] == CLEM_SCC_STATE_REGISTER) {
            //  command write register
            scc->selected_reg[ch_idx] = 0x00;
            scc->state[ch_idx] = CLEM_SCC_STATE_READY;
        }
        break;
    case CLEM_MMIO_REG_SCC_B_DATA:
    case CLEM_MMIO_REG_SCC_A_DATA:
        break;
    }
    // CLEM_LOG("clem_scc: %02X <- %02X", ioreg, value);
}

uint8_t clem_scc_read_switch(struct ClemensDeviceSCC *scc, uint8_t ioreg, uint8_t flags) {
    uint8_t value = 0;
    unsigned ch_idx;
    bool is_noop = (flags & CLEM_OP_IO_NO_OP) != 0;
    switch (ioreg) {
    case CLEM_MMIO_REG_SCC_B_CMD:
    case CLEM_MMIO_REG_SCC_A_CMD:
        //  always read from current register - this will reset selected register
        //  to 0x00 which is one way for the app to sync with the SCC.
        ch_idx = CLEM_MMIO_REG_SCC_A_CMD - ioreg;
        scc->selected_reg[ch_idx] = 0x00;
        scc->state[ch_idx] = CLEM_SCC_STATE_READY;
        break;
    case CLEM_MMIO_REG_SCC_B_DATA:
    case CLEM_MMIO_REG_SCC_A_DATA:
        break;
    }
    return value;
}
