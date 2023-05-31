#include "clem_scc.h"

#include "clem_debug.h"
#include "clem_device.h"
#include "clem_mmio_types.h"
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

#define CLEM_SCC_RESET_TX_CLOCK 1
#define CLEM_SCC_RESET_RX_CLOCK 2

static inline bool _clem_scc_is_xtal_on(struct ClemensDeviceSCCChannel *channel) {
    return (channel->regs[11] & CLEM_SCC_CLK_XTAL_ON) != 0;
}

static inline bool _clem_scc_is_tx_enabled(struct ClemensDeviceSCCChannel *channel) {
    return (channel->regs[5] & CLEM_SCC_TX_ENABLE) != 0;
}

static inline bool _clem_scc_is_rx_enabled(struct ClemensDeviceSCCChannel *channel) {
    return (channel->regs[3] & CLEM_SCC_RX_ENABLE) != 0;
}

static inline bool _clem_scc_is_synchronous_mode(struct ClemensDeviceSCCChannel *channel) {
    return (channel->regs[4] & 0x0c) == CLEM_SCC_STOP_SYNC_MODE;
}

static inline clem_clocks_duration_t
_clem_scc_clock_data_step_from_mode(clem_clocks_duration_t clock_step, uint8_t mode) {
    switch (mode) {
    case CLEM_SCC_CLOCK_X1:
        return clock_step;
    case CLEM_SCC_CLOCK_X16:
        return clock_step * 16;
    case CLEM_SCC_CLOCK_X32:
        return clock_step * 32;
    case CLEM_SCC_CLOCK_X64:
        return clock_step * 64;
    default:
        CLEM_ASSERT(false);
    }
    return clock_step;
}

static clem_clocks_time_t _clem_scc_channel_tx_next_ts(struct ClemensDeviceSCCChannel *channel) {
    clem_clocks_time_t next_ts = 0;
    switch (channel->regs[11] & 0x18) {
    case CLEM_SCC_CLK_TX_SOURCE_BRG:
        break;
    case CLEM_SCC_CLK_TX_SOURCE_TRxC:
        //  always use TRxC pin-in
        // channel->tx_next_ts += channel->(channel->serial_port & CLEM_SCC_PORT_HSKI) break;
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

void clem_scc_reset_channel_clocks(struct ClemensDeviceSCC *scc, unsigned ch_idx,
                                   clem_clocks_time_t ts, unsigned options) {
    // The transmitter and receiver each operate on a clock defined by registers
    // WR4 (clock mode), WR11 (TRxC, RTxC) and W14 (baud rate gen.)  These
    // clocks align with the data rate - so for synchronous communication,
    // x1 clock steps are enforced (very slow - likely not used in real world
    // IIGS applications? fingers crossed.)
    struct ClemensDeviceSCCChannel *channel = &scc->channel[ch_idx];
    bool brg_on = (channel->regs[14] & CLEM_SCC_CLK_BRG_ON) != 0;
    bool brg_rx = (brg_on && channel->regs[11] & CLEM_SCC_CLK_RX_SOURCE_BRG) != 0;
    bool brg_tx = (brg_on && channel->regs[11] & CLEM_SCC_CLK_TX_SOURCE_BRG) != 0;
    uint8_t clk_mode = channel->regs[4] & 0xc0;
    clem_clocks_duration_t step_rx =
        (brg_rx && channel->regs[14] & CLEM_SCC_CLK_BRG_PCLK) ? scc->pclk_step : scc->xtal_step;
    //  TX source
    if (options & CLEM_SCC_RESET_TX_CLOCK) {
        switch (channel->regs[11] & 0x18) {
        case CLEM_SCC_CLK_TX_SOURCE_RTxC:
            channel->tx_clock_step = _clem_scc_is_xtal_on(channel) ? scc->xtal_step : 0;
            break;
        case CLEM_SCC_CLK_TX_SOURCE_TRxC:
            //  poll at the data rate to wait for the TRxC signal
            channel->tx_clock_step = scc->xtal_step;
            break;
        case CLEM_SCC_CLK_TX_SOURCE_BRG:
            channel->tx_clock_step = (brg_tx && channel->regs[14] & CLEM_SCC_CLK_BRG_PCLK)
                                         ? scc->pclk_step
                                         : scc->xtal_step;
            break;
        case CLEM_SCC_CLK_TX_SOURCE_DPLL:
            CLEM_UNIMPLEMENTED("SCC: DPLL TX clock");
            channel->tx_clock_step = 0;
            break;
        default:
            CLEM_ASSERT(false);
            break;
        }
        if (!_clem_scc_is_synchronous_mode(channel)) {
            channel->tx_clock_step =
                _clem_scc_clock_data_step_from_mode(channel->tx_clock_step, clk_mode);
        }
        channel->tx_clock_ts = ts;
    }
    if (options & CLEM_SCC_RESET_RX_CLOCK) {
        switch (channel->regs[11] & 0x60) {
        case CLEM_SCC_CLK_RX_SOURCE_RTxC:
            channel->rx_clock_step = _clem_scc_is_xtal_on(channel) ? scc->xtal_step : 0;
            break;
        case CLEM_SCC_CLK_RX_SOURCE_TRxC:
            channel->tx_clock_step = scc->xtal_step;
            break;
        case CLEM_SCC_CLK_RX_SOURCE_BRG:
            channel->rx_clock_step = (brg_rx && channel->regs[14] & CLEM_SCC_CLK_BRG_PCLK)
                                         ? scc->pclk_step
                                         : scc->xtal_step;
            break;
        case CLEM_SCC_CLK_RX_SOURCE_DPLL:
            CLEM_UNIMPLEMENTED("SCC: DPLL RX clock");
            channel->rx_clock_step = 0;
            break;
        default:
            CLEM_ASSERT(false);
            break;
        }
        if (!_clem_scc_is_synchronous_mode(channel)) {
            channel->rx_clock_step =
                _clem_scc_clock_data_step_from_mode(channel->rx_clock_step, clk_mode);
        }
        channel->rx_clock_ts = ts;
    }
}

void clem_scc_reset_channel(struct ClemensDeviceSCC *scc, unsigned ch_idx, bool hw) {
    struct ClemensDeviceSCCChannel *channel = &scc->channel[ch_idx];
    if (hw) {
        memset(channel->regs, 0, sizeof(channel->regs));
        channel->regs[11] |= CLEM_SCC_CLK_TX_SOURCE_TRxC;
    }
    channel->regs[4] |= CLEM_SCC_STOP_BIT_1;
    clem_scc_reset_channel_clocks(scc, ch_idx, 0,
                                  CLEM_SCC_RESET_TX_CLOCK | CLEM_SCC_RESET_RX_CLOCK);
    channel->poll_device_clock_ts = 0;
}

void clem_scc_sync_channel_uart(struct ClemensDeviceSCC *scc, unsigned ch_idx,
                                clem_clocks_time_t next_ts) {
    struct ClemensDeviceSCCChannel *channel = &scc->channel[ch_idx];
    bool rx_enabled = _clem_scc_is_rx_enabled(channel) && channel->rx_clock_step > 0;
    bool tx_enabled = _clem_scc_is_tx_enabled(channel) && channel->tx_clock_step > 0;

    while (rx_enabled || tx_enabled) {
        if (channel->rx_clock_ts >= next_ts)
            rx_enabled = false;
        if (rx_enabled) {
            if (channel->rx_clock_ts != channel->poll_device_clock_ts) {
                //  DO DEVICE POLL HERE which will set the new poll time
                //  clem_scc_sync_channel_device(scc, ch_idx, channel->rx_clock_ts);
            }
            channel->rx_clock_ts += channel->rx_clock_step;
        }
        if (channel->tx_clock_ts >= next_ts)
            tx_enabled = false;
        if (tx_enabled) {
            if (channel->tx_clock_ts != channel->poll_device_clock_ts) {
                //  DO DEVICE POLL HERE which will set the new poll time
                //  clem_scc_sync_channel_device(scc, ch_idx, channel->tx_clock_ts);
            }
            channel->tx_clock_ts += channel->tx_clock_step;
        }
    }
}

void clem_scc_reset(struct ClemensDeviceSCC *scc) {
    //  equivalent to a hardware reset
    memset(scc, 0, sizeof(*scc));
    scc->xtal_step = (clem_clocks_duration_t)floor(
        (14.31818 / CLEM_CLOCKS_SCC_XTAL_MHZ) * CLEM_CLOCKS_14MHZ_CYCLE + 0.5);
    scc->pclk_step = CLEM_CLOCKS_CREF_CYCLE;

    clem_scc_reset_channel(scc, 0, true);
    clem_scc_reset_channel(scc, 1, true);
}

void clem_scc_glu_sync(struct ClemensDeviceSCC *scc, struct ClemensClock *clock) {
    //  channel A and B xmit/recv functionality is driven by either the PCLK
    //  or the XTAL (via configuration)
    //  synchronization between channel and GLU occurs on PCLK intervals

    clem_scc_sync_channel_uart(scc, 0, clock->ts);
    clem_scc_sync_channel_uart(scc, 1, clock->ts);
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
            CLEM_LOG("SCC: Write Reg %u <= %02x", scc->channel[ch_idx].selected_reg, value);
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
        CLEM_LOG("SCC: Read Reg %u => ??", scc->channel[ch_idx].selected_reg);
        scc->channel[ch_idx].selected_reg = 0x00;
        scc->channel[ch_idx].state = CLEM_SCC_STATE_READY;
        break;
    case CLEM_MMIO_REG_SCC_B_DATA:
    case CLEM_MMIO_REG_SCC_A_DATA:
        break;
    }
    return value;
}
