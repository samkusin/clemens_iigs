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
    Compromise between accuracy and efficiency when emulating the clock
    Rely on clock mode and the multiplier, which determines the effective 
    clock frequency for the transmitter and receiver.  
    
    This means encoding  beyond NRZ/NRZI will likely not work since higher 
    effective clock frequencies are required to sample the I/O data stream.
    As a side effect, it's unlikely synchronous mode will work correctly.

    NRZ encoding will be supported by default.  Will evaluate other modes as
    needed.

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


static inline bool _clem_scc_is_tx_enabled(struct ClemensDeviceSCCChannel *channel) {
    return (channel->regs[5] & CLEM_SCC_TX_ENABLE) != 0;
}

static inline bool _clem_scc_is_rx_enabled(struct ClemensDeviceSCCChannel *channel) {
    return (channel->regs[3] & CLEM_SCC_RX_ENABLE) != 0;
}

static void _clem_scc_channel_set_tx_rx_clock_step(struct ClemensDeviceSCCChannel *channel, 
                            struct ClemensDeviceSCCClockRef* clock_ref) {
    bool brg_on = (channel->regs[14] & CLEM_SCC_CLK_BRG_ON) != 0;
    bool brg_rx = (brg_on && channel->regs[11] & CLEM_SCC_CLK_RX_SOURCE_BRG) != 0;
    bool brg_tx = (brg_on && channel->regs[11] & CLEM_SCC_CLK_TX_SOURCE_BRG) != 0;
    clem_clocks_duration_t step_rx = (brg_rx && channel->regs[14] & CLEM_SCC_CLK_BRG_PCLK) 
        ? clock_ref->pclk_step : clock_ref->xtal_step;
    clem_clocks_duration_t step_tx = (brg_tx && channel->regs[14] & CLEM_SCC_CLK_BRG_PCLK) 
        ? clock_ref->pclk_step : clock_ref->xtal_step;
    switch (channel->regs[4] & 0xc0) {
        case CLEM_SCC_CLOCK_X1:
            channel->tx_clock_step = step_tx;
            channel->rx_clock_step = step_rx;
            break;
        case CLEM_SCC_CLOCK_X16:
            channel->tx_clock_step = step_tx * 16;
            channel->rx_clock_step = step_rx * 16;
            break;
        case CLEM_SCC_CLOCK_X32:
            channel->tx_clock_step = step_tx * 32;
            channel->rx_clock_step = step_rx * 32;
            break;
        case CLEM_SCC_CLOCK_X64:
            channel->tx_clock_step = step_tx * 64;
            channel->rx_clock_step = step_rx * 64;
            break;
    }  
    //  modify steps to account for desired baud rates
    //  tc = ( xtal / (2 * baud_rate * clock_div) ) - 2
    if (brg_rx) {

    }   
    if (brg_tx) {

    }                           
}


static clem_clocks_time_t _clem_scc_channel_tx_next_ts(struct ClemensDeviceSCCChannel *channel) {
    clem_clocks_time_t next_ts = 0;
    switch (channel->regs[11] & 0x18) {
        case CLEM_SCC_CLK_TX_SOURCE_BRG:
            break;
        case CLEM_SCC_CLK_TX_SOURCE_TRxC:
            //  always use TRxC pin-in
            channel->tx_next_ts += channel->
             (channel->serial_port & CLEM_SCC_PORT_HSKI)
            break;
        case CLEM_SCC_CLK_TX_SOURCE_RTxC:
            break;        
        case CLEM_SCC_CLK_TX_SOURCE_DPLL:
            break;
    }
    return next_ts;
}

static clem_clocks_time_t _clem_scc_channel_rx_next_ts(struct ClemensDeviceSCCChannel *channel) {
    clem_clocks_time_t next_ts = 0;
    switch (channel->regs[11] & 0x60) {
        case CLEM_SCC_CLK_RX_SOURCE_BRG:
            break;
        case CLEM_SCC_CLK_RX_SOURCE_TRxC:
            //  always use TRxC pin-in
            break;
        case CLEM_SCC_CLK_RX_SOURCE_RTxC:
            break;        
        case CLEM_SCC_CLK_RX_SOURCE_DPLL:
            break;
    }
    return next_ts;
}

////////////////////////////////////////////////////////////////////////////////

void clem_scc_reset_channel(struct ClemensDeviceSCCChannel *channel, 
                            struct ClemensDeviceSCCClockRef* clock_ref, bool hw) {
    if (hw) {
        memset(channel->regs, 0, sizeof(channel->regs));
        channel->regs[11] |= CLEM_SCC_CLK_TX_SOURCE_TRxC;
    }
    channel->regs[4] |= CLEM_SCC_STOP_BIT_1;

    _clem_scc_channel_set_tx_rx_clock_step(channel, clock_ref);
}

void clem_scc_sync_channel_uart(struct ClemensDeviceSCCChannel *channel, 
                                struct ClemensDeviceSCCClockRef* clock_ref,
                                clem_clocks_time_t cur_ts, clem_clocks_time_t next_ts) {
    bool rx_enabled = _clem_scc_is_rx_enabled(channel);
    bool tx_enabled = _clem_scc_is_tx_enabled(channel);
    while (cur_ts < next_ts && (rx_enabled || tx_enabled)) {
        if (tx_enabled && channel->tx_next_ts < next_ts) {
            channel->tx_next_ts = _clem_scc_channel_tx_next_ts(channel);
        }
        if (rx_enabled && channel->tx_next_ts < next_ts) {
            channel->rx_next_ts = _clem_scc_channel_rx_next_ts(channel);
        }
    }

}

void clem_scc_reset(struct ClemensDeviceSCC *scc) {
    //  equivalent to a hardware reset
    memset(scc, 0, sizeof(*scc));
    scc->clock_ref.xtal_step = (clem_clocks_duration_t)floor(
        (14.31818 / CLEM_CLOCKS_SCC_XTAL_MHZ) * CLEM_CLOCKS_14MHZ_CYCLE + 0.5);
    scc->clock_ref.pclk_step = CLEM_CLOCKS_CREF_CYCLE;

    clem_scc_reset_channel(&scc->channel[0], &scc->clock_ref, true);

}

void clem_scc_glu_sync(struct ClemensDeviceSCC *scc, struct ClemensClock *clock) {
    //  channel A and B xmit/recv functionality is driven by either the PCLK
    //  or the XTAL (via configuration)
    //  synchronization between channel and GLU occurs on PCLK intervals

    clem_scc_sync_channel_uart(&scc->channel[0], &scc->clock_ref, scc->ts_last_frame, clock->ts);
    clem_scc_sync_channel_uart(&scc->channel[1],&scc->clock_ref, scc->ts_last_frame, clock->ts);

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
