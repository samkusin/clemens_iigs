#include "clem_debug.h"
#include "clem_device.h"
#include "clem_mmio_defs.h"
#include "clem_mmio_types.h"

#include <math.h>
#include <string.h>

/*  SCC Implementation
    ==================
    This module implements communication between the machine and an emulated
    Zilog 8530 (NMOS).  Though not called out in the docs - the data/command per
    port interface is very similar to how the system communicates with the Ensoniq.

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

    Timing (from IIGS schematic docs) and
    https://www.kansasfest.org/wp-content/uploads/2011-krue-fpi.pdf

    ~SYNCA, ~RTXCA, RTXCB = 3.6864 mhz, or the XTAL oscillator clock
    PCLK = Mega 2 CREF

    Peripheral (Plug to SCC Pin)
    ==========
    1:CLEM_SCC_PORT_DTR     - /DTR
    2:CLEM_SCC_PORT_HSKI    - /CTS, /TRxC
    3:CLEM_SCC_PORT_TX_D_LO - TxD
    5:CLEM_SCC_PORT_RX_D_LO - RxD
    6:CLEM_SCC_PORT_TX_D_HI - /RTS
    7:CLEM_SCC_PORT_GPI     - DCD


    Timing
    ======
    BRG baud constant = (Clock Freq) / (2 * BAUD * ClockMode) - 2

*/

#define CLEM_CLOCKS_SCC_XTAL_MHZ 3.6864

#define CLEM_SCC_STATE_READY    0
#define CLEM_SCC_STATE_REGISTER 1

//  Clock Mode (WR11)
#define CLEM_SCC_CLK_TRxC_OUT_XTAL 0x00
#define CLEM_SCC_CLK_TRxC_OUT_XMIT 0x01
#define CLEM_SCC_CLK_TRxC_OUT_BRG  0x02
#define CLEM_SCC_CLK_TRxC_OUT_DPLL 0x03
#define CLEM_SCC_CLK_SOURCE_RTxC   0x00
#define CLEM_SCC_CLK_SOURCE_TRxC   0x01
#define CLEM_SCC_CLK_SOURCE_BRG    0x02
#define CLEM_SCC_CLK_SOURCE_DPLL   0x03

void clem_scc_reset_channel(struct ClemensDeviceSCCChannel *channel, bool hw) {
    // WR11 reset
    channel->tx_clk_mode = channel->tx_clk_mode = CLEM_SCC_CLK_SOURCE_TRxC;
    // WR4 reset
    channel->stop_half_bits = channel->stop_half_bits = 2;
}

void clem_scc_reset(struct ClemensDeviceSCC *scc) {
    //  equivalent to a hardware reset
    memset(scc, 0, sizeof(*scc));
    scc->xtal_clocks = (clem_clocks_duration_t)floor(
        (14.31818 / CLEM_CLOCKS_SCC_XTAL_MHZ) * CLEM_CLOCKS_14MHZ_CYCLE + 0.5);

    clem_scc_reset_channel(&scc->channel[0], true);
}

void clem_scc_glu_sync(struct ClemensDeviceSCC *scc, struct ClemensClock *clock) {
    scc->ts_last_frame = clock->ts;
}

void clem_scc_write_switch(struct ClemensDeviceSCC *scc, uint8_t ioreg, uint8_t value) {
    unsigned ch_idx;

    switch (ioreg) {
    case CLEM_MMIO_REG_SCC_B_CMD:
    case CLEM_MMIO_REG_SCC_A_CMD:
        ch_idx = CLEM_MMIO_REG_SCC_A_CMD - ioreg;
        if (scc->channel[ch_idx].state == CLEM_SCC_STATE_READY) {
            scc->channel[ch_idx].selected_reg = value;
            scc->channel[ch_idx].state = CLEM_SCC_STATE_REGISTER;
        } else if (scc->channel[ch_idx].state == CLEM_SCC_STATE_REGISTER) {
            //  command write register
            scc->channel[ch_idx].selected_reg = 0x00;
            scc->channel[ch_idx].state = CLEM_SCC_STATE_READY;
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
        scc->channel[ch_idx].selected_reg = 0x00;
        scc->channel[ch_idx].state = CLEM_SCC_STATE_READY;
        break;
    case CLEM_MMIO_REG_SCC_B_DATA:
    case CLEM_MMIO_REG_SCC_A_DATA:
        break;
    }
    return value;
}
