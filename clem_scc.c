#include "clem_scc.h"

#include "clem_debug.h"
#include "clem_device.h"
#include "clem_shared.h"

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

    ~SYNCA, ~RTXCA, RTXCB = 3.6864 mhz, or the XTAL oscillator clock
    PCLK = Mega 2 CREF

    Peripheral (Plug to SCC Pin)
    ==========
    1:CLEM_SCC_PORT_DTR     - DTR
    2:CLEM_SCC_PORT_HSKI    - CTS, TRxC
    3:CLEM_SCC_PORT_TX_D_LO - TxD
    5:CLEM_SCC_PORT_RX_D_LO - RxD
    6:CLEM_SCC_PORT_TX_D_HI - RTS
    7:CLEM_SCC_PORT_GPI     - DCD


    Timing
    ======
    BRG baud constant = (Clock Freq) / (2 * BAUD * ClockMode) - 2
    CTS, TRxC, XTAL (RTxC, SYNC), or Baud-rate-generator (BRG via PCLK/RTxC)

    Interrupts
    ==========
    See 4.2 Z8530 1986 documentation (page 4-1), figure 4-2 (page 4-4)
    Note, since /INTACK is always high as referenced in the IIGS schemeatic,
    this means 'Interrupt Without Acknowledge' is the only approach that needs
    to be implemented.  This method relies on register reads to extract details.

    Each channel will trigger an interrupt based on recv/xmit status.

    Prioritization may be required (page 4-2, Interrupt Sources) so that high
    priority interrupts don't erase lower-priority ones of interest.


*/

#define CLEM_CLOCKS_SCC_XTAL_MHZ 3.6864

#define CLEM_SCC_STATE_READY    0
#define CLEM_SCC_STATE_REGISTER 1

void clem_scc_reset_channel(struct ClemensDeviceSCCChannel *channel, bool hw) {
    if (hw) {
        memset(channel->regs, 0, sizeof(channel->regs));
        channel->regs[11] |= CLEM_SCC_CLK_SOURCE_TRxC;
    }
    channel->regs[4] |= CLEM_SCC_STOP_BIT_1;
}

void clem_scc_sync_channel_uart(struct ClemensDeviceSCCChannel *channel, clem_clocks_time_t prev_ts,
                                clem_clocks_time_t ts) {
    //  may need to run at a high enough rate
    //  may need to run like a master/slave where the serial connection is
    //  guaranteed to run in sync with us
    //  if tx enabled (auto-enable or explicitly via reg, may just need to check reg)
    //      while (channel->tx_next_ts - prev_ts <= ts - prev_ts) {}
}

void clem_scc_reset(struct ClemensDeviceSCC *scc) {
    //  equivalent to a hardware reset
    memset(scc, 0, sizeof(*scc));
    scc->xtal_clocks = (clem_clocks_duration_t)floor(
        (14.31818 / CLEM_CLOCKS_SCC_XTAL_MHZ) * CLEM_CLOCKS_14MHZ_CYCLE + 0.5);

    clem_scc_reset_channel(&scc->channel[0], true);
}

void clem_scc_glu_sync(struct ClemensDeviceSCC *scc, struct ClemensClock *clock) {
    //  channel A and B xmit/recv functionality is driven by either the PCLK
    //  or the XTAL (via configuration)
    //  synchronization between channel and GLU occurs on PCLK intervals

    clem_scc_sync_channel_uart(&scc->channel[0], scc->ts_last_frame, clock->ts);
    clem_scc_sync_channel_uart(&scc->channel[1], scc->ts_last_frame, clock->ts);

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
