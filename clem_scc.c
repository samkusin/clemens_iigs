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

#define CLEM_SCC_DEBUG

#ifdef CLEM_SCC_DEBUG
#define CLEM_SCC_LOG(...) CLEM_LOG(__VA_ARGS__)
#else
#define CLEM_SCC_LOG(...) CLEM_DEBUG(__VA_ARGS__)
#endif

#define CLEM_CLOCKS_SCC_XTAL_MHZ 3.6864

#define CLEM_SCC_STATE_READY    0
#define CLEM_SCC_STATE_REGISTER 1

#define CLEM_SCC_RESET_TX_CLOCK 1
#define CLEM_SCC_RESET_RX_CLOCK 2

static unsigned s_scc_clk_x_speeds[] = {1, 16, 32, 64};

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
    return clock_step * s_scc_clk_x_speeds[mode >> 6];
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
                                   unsigned options) {
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
    }
}

void clem_scc_reset_channel(struct ClemensDeviceSCC *scc, unsigned ch_idx, bool hw) {
    struct ClemensDeviceSCCChannel *channel = &scc->channel[ch_idx];
    if (hw) {
        memset(channel->regs, 0, sizeof(channel->regs));
        channel->regs[11] |= CLEM_SCC_CLK_TX_SOURCE_TRxC;
    }
    channel->regs[4] |= CLEM_SCC_STOP_BIT_1;
    clem_scc_reset_channel_clocks(scc, ch_idx, CLEM_SCC_RESET_TX_CLOCK | CLEM_SCC_RESET_RX_CLOCK);
    channel->poll_device_clock_ts = 0;
}

void clem_scc_sync_channel_uart(struct ClemensDeviceSCC *scc, unsigned ch_idx,
                                clem_clocks_time_t next_ts) {
    struct ClemensDeviceSCCChannel *channel = &scc->channel[ch_idx];
    bool rx_enabled = _clem_scc_is_rx_enabled(channel) && channel->rx_clock_step > 0;
    bool tx_enabled = _clem_scc_is_tx_enabled(channel) && channel->tx_clock_step > 0;
    if (!rx_enabled && !tx_enabled)
        return;

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

void clem_scc_reg_interrupt_vector(struct ClemensDeviceSCC *scc, uint8_t value) {
    //  a single value replicated on both channels
    CLEM_SCC_LOG("SCC: WR2 Interrupt Vector <= %02x", value);
    scc->channel[0].regs[CLEM_SCC_WR2_INT_VECTOR] = value;
    scc->channel[1].regs[CLEM_SCC_WR2_INT_VECTOR] = value;
}

void clem_scc_reg_master_interrupt(struct ClemensDeviceSCC *scc, uint8_t value) {
    uint8_t reset = (value & 0xc0);

    switch (reset) {
    case 0x40:
        CLEM_SCC_LOG("SCC: WR9 B reset");
        clem_scc_reset_channel(scc, 1, false);
        break;
    case 0x80:
        CLEM_SCC_LOG("SCC: WR9 A reset");
        clem_scc_reset_channel(scc, 0, false);
        break;
    case 0xc0:
        CLEM_SCC_LOG("SCC: WR9 Hardware Reset");
        clem_scc_reset_channel(scc, 0, true);
        clem_scc_reset_channel(scc, 1, true);
        break;
    }
}

void clem_scc_reg_recv_control(struct ClemensDeviceSCC *scc, struct ClemensTimeSpec *tspec,
                               unsigned ch_idx, uint8_t value) {
    uint8_t xvalue = scc->channel[ch_idx].regs[CLEM_SCC_WR3_RECV_CONTROL] ^ value;
    if (!xvalue)
        return;
    if (value & CLEM_SCC_RX_ENABLE) {
        scc->channel[ch_idx].rx_clock_ts = tspec->clocks_spent;
    }
}

void clem_scc_reg_clock_mode(struct ClemensDeviceSCC *scc, unsigned ch_idx, uint8_t value) {
    unsigned options = 0;
    uint8_t xvalue = scc->channel[ch_idx].regs[CLEM_SCC_WR11_CLOCK_MODE] ^ value;
    if (!xvalue)
        return;

    CLEM_SCC_LOG("SCC: WR11 %c mode <= %02x", ch_idx ? 'B' : 'A', value);
    if (xvalue & 0x60) { // recv clk changed
        options |= CLEM_SCC_RESET_RX_CLOCK;
    }
    if (xvalue & 0x18) { // send clk changed
        options |= CLEM_SCC_RESET_TX_CLOCK;
    }
    // TODO: TRxC O/I and special case overrides
    clem_scc_reset_channel_clocks(scc, ch_idx, options);

    scc->channel[ch_idx].regs[CLEM_SCC_WR11_CLOCK_MODE] = value;
}

void clem_scc_reg_clock_data_rate(struct ClemensDeviceSCC *scc, unsigned ch_idx, uint8_t value) {
    uint8_t xvalue = scc->channel[ch_idx].regs[CLEM_SCC_WR4_CLOCK_DATA_RATE] ^ value;

    scc->channel[ch_idx].regs[CLEM_SCC_WR4_CLOCK_DATA_RATE] = value;

    CLEM_SCC_LOG("SCC: WR4 %c data <= X%u CLK, %02X", ch_idx ? 'B' : 'A',
                 s_scc_clk_x_speeds[value >> 6], value);
    if (!(xvalue & 0xc0))
        return; // clock rate not modified

    clem_scc_reset_channel_clocks(scc, ch_idx, CLEM_SCC_RESET_RX_CLOCK | CLEM_SCC_RESET_TX_CLOCK);
}

void clem_scc_reg_set_interrupt_enable(struct ClemensDeviceSCC *scc, unsigned ch_idx,
                                       uint8_t value) {
    CLEM_SCC_LOG("SCC: WR15 %c mode <= %02x", ch_idx ? 'B' : 'A', value);
    scc->channel[ch_idx].regs[CLEM_SCC_WR15_INT_ENABLE] = value;
}

void clem_scc_reg_dpll_command(struct ClemensDeviceSCC *scc, unsigned ch_idx, uint8_t value) {
    static const char *mode_names[] = {"NUL",     "SEARCH",   "RSTCM",  "NODPLL",
                                       "BRGDPLL", "DPLLRTxC", "DPLLFM", "DPLLNRZI"};
    uint8_t xvalue = (scc->channel[ch_idx].regs[CLEM_SCC_WR14_MISC_CONTROL] & 0xe0) ^ value;
    if (!xvalue)
        return;
    CLEM_SCC_LOG("SCC WR14 %c %s", 'A' + ch_idx, mode_names[value >> 5]);
    scc->channel[ch_idx].regs[CLEM_SCC_WR14_MISC_CONTROL] &= 0x1f;
    scc->channel[ch_idx].regs[CLEM_SCC_WR14_MISC_CONTROL] |= value;
}

void clem_scc_reg_baud_gen_enable(struct ClemensDeviceSCC *scc, unsigned ch_idx, uint8_t value) {
    uint8_t xvalue = (scc->channel[ch_idx].regs[CLEM_SCC_WR14_MISC_CONTROL] & 0x03) ^ value;
    if (!xvalue)
        return;

    CLEM_SCC_LOG("SCC WR14 %c BRG %s %s", 'A' + ch_idx,
                 value & CLEM_SCC_CLK_BRG_PCLK ? "PCLK" : "XTAL",
                 value & CLEM_SCC_CLK_BRG_ON ? "ON" : "OFF");

    scc->channel[ch_idx].regs[CLEM_SCC_WR14_MISC_CONTROL] &= 0xfc;
    scc->channel[ch_idx].regs[CLEM_SCC_WR14_MISC_CONTROL] |= value;
}

void clem_scc_reg_misc_control_wr14(struct ClemensDeviceSCC *scc, unsigned ch_idx, uint8_t value) {
    uint8_t xvalue = (scc->channel[ch_idx].regs[CLEM_SCC_WR14_MISC_CONTROL] & 0x1c) ^ value;
    if (!xvalue)
        return;

    CLEM_SCC_LOG("SCC WR14 %c LOOPBACK", 'A' + ch_idx,
                 value & CLEM_SCC_LOCAL_LOOPBACK ? "ON" : "OFF");
    CLEM_SCC_LOG("SCC WR14 %c AUTO ENABLE", 'A' + ch_idx,
                 value & CLEM_SCC_AUTO_ENABLE ? "ON" : "OFF");
    CLEM_SCC_LOG("SCC WR14 %c DTR_FN", 'A' + ch_idx, value & CLEM_SCC_DTR_FUNCTION ? "ON" : "OFF");

    scc->channel[ch_idx].regs[CLEM_SCC_WR14_MISC_CONTROL] &= 0xe3;
    scc->channel[ch_idx].regs[CLEM_SCC_WR14_MISC_CONTROL] |= value;
}

void clem_scc_write_switch(struct ClemensDeviceSCC *scc, struct ClemensTimeSpec *tspec,
                           uint8_t ioreg, uint8_t value) {
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
            switch (scc->channel[ch_idx].selected_reg) {
            case CLEM_SCC_WR2_INT_VECTOR:
                clem_scc_reg_interrupt_vector(scc, value);
                break;
            case CLEM_SCC_WR3_RECV_CONTROL:
                clem_scc_reg_recv_control(scc, tspec, ch_idx, value);
                break;
            case CLEM_SCC_WR4_CLOCK_DATA_RATE:
                clem_scc_reg_clock_data_rate(scc, ch_idx, value);
                break;
            case CLEM_SCC_WR9_MASTER_INT:
                clem_scc_reg_master_interrupt(scc, value);
                break;
            case CLEM_SCC_WR11_CLOCK_MODE:
                clem_scc_reg_clock_mode(scc, ch_idx, value);
                break;
            case CLEM_SCC_WR12_TIME_CONST_LO:
                scc->channel[ch_idx].regs[CLEM_SCC_WR12_TIME_CONST_LO] = value;
                CLEM_SCC_LOG("SCC: WR12 %c TC LO <= %02x", 'A' + ch_idx, value);
                break;
            case CLEM_SCC_WR13_TIME_CONST_HI:
                scc->channel[ch_idx].regs[CLEM_SCC_WR13_TIME_CONST_HI] = value;
                CLEM_SCC_LOG("SCC: WR13 %c TC HI <= %02x", 'A' + ch_idx, value);
                break;
            case CLEM_SCC_WR14_MISC_CONTROL:
                clem_scc_reg_dpll_command(scc, ch_idx, value & 0xe0);
                clem_scc_reg_baud_gen_enable(scc, ch_idx, value & 0x3);
                clem_scc_reg_misc_control_wr14(scc, ch_idx, value & 0x1c);
                break;
            case CLEM_SCC_WR15_INT_ENABLE:
                clem_scc_reg_set_interrupt_enable(scc, ch_idx, value);
                break;
            default:
                CLEM_SCC_LOG("SCC: WR%u %c <= %02x", scc->channel[ch_idx].selected_reg,
                             'A' + ch_idx, value);
                break;
            }
            scc->channel[ch_idx].selected_reg = 0x00;
            scc->channel[ch_idx].state = CLEM_SCC_STATE_READY;
        }
        break;
    case CLEM_MMIO_REG_SCC_B_DATA:
    case CLEM_MMIO_REG_SCC_A_DATA:
        CLEM_SCC_LOG("SCC: WR DATA %c <= %02X", 'A' + ch_idx, value);
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
        if (!CLEM_IS_IO_NO_OP(flags)) {
            // CLEM_LOG("SCC: Read Reg %u => ??", scc->channel[ch_idx].selected_reg);
            //  Some later Zilog docs mention the NMOS version will return
            //  an 'image' of another register - and we'll try that here.
            switch (scc->channel[ch_idx].selected_reg) {
            case CLEM_SCC_RR2_INT_VECTOR:
                value = scc->channel[ch_idx].regs[CLEM_SCC_WR2_INT_VECTOR];
                break;
            case CLEM_SCC_RR12_TIME_CONST_LO:
                value = scc->channel[ch_idx].regs[CLEM_SCC_WR12_TIME_CONST_LO];
                break;
            case 0x09: //  returns an image of RR13/WR13
            case CLEM_SCC_RR13_TIME_CONST_HI:
                value = scc->channel[ch_idx].regs[CLEM_SCC_WR13_TIME_CONST_HI];
                break;
            case 0x0B: //  returns an image of RR15/WR15
            case CLEM_SCC_RR15_INT_ENABLE:
                // bits 0 and 2 are always 0
                value = scc->channel[ch_idx].regs[CLEM_SCC_WR15_INT_ENABLE] & 0xfa;
                break;
            default:
                //   CLEM_SCC_LOG("SCC: RR%u unhandled", scc->channel[ch_idx].selected_reg);
                break;
            }
            scc->channel[ch_idx].selected_reg = 0x00;
            scc->channel[ch_idx].state = CLEM_SCC_STATE_READY;
        }

        break;
    case CLEM_MMIO_REG_SCC_B_DATA:
    case CLEM_MMIO_REG_SCC_A_DATA:
        break;
    }
    return value;
}
