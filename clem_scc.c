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

#define CLEM_SCC_CLOCK_MODE_PCLK 0
#define CLEM_SCC_CLOCK_MODE_XTAL 1

#define CLEM_SCC_BRG_STATUS_FF_FLAG      0x80000000
#define CLEM_SCC_BRG_STATUS_PULSE_FLAG   0x40000000
#define CLEM_SCC_BRG_STATUS_COUNTER_MASK 0x0000ffff

static unsigned s_scc_clk_x_speeds[] = {1, 16, 32, 64};

static inline bool _clem_scc_is_xtal_on(struct ClemensDeviceSCCChannel *channel) {
    return (channel->regs[CLEM_SCC_WR11_CLOCK_MODE] & CLEM_SCC_CLK_XTAL_ON) != 0;
}

static inline bool _clem_scc_is_tx_enabled(struct ClemensDeviceSCCChannel *channel) {
    return (channel->regs[CLEM_SCC_WR5_XMIT_CONTROL] & CLEM_SCC_TX_ENABLE) != 0;
}

static inline bool _clem_scc_is_rx_enabled(struct ClemensDeviceSCCChannel *channel) {
    return (channel->regs[CLEM_SCC_WR3_RECV_CONTROL] & CLEM_SCC_RX_ENABLE) != 0;
}

static inline bool _clem_scc_is_synchronous_mode(struct ClemensDeviceSCCChannel *channel) {
    return (channel->regs[CLEM_SCC_WR4_CLOCK_DATA_RATE] & 0x0c) == CLEM_SCC_STOP_SYNC_MODE;
}

static inline clem_clocks_duration_t
_clem_scc_clock_data_step_from_mode(clem_clocks_duration_t clock_step, uint8_t mode) {
    return clock_step * s_scc_clk_x_speeds[mode >> 6];
}

static inline clem_clocks_duration_t
_clem_scc_channel_calc_clock_step(struct ClemensDeviceSCCChannel *channel,
                                  clem_clocks_duration_t clock_step) {
    return _clem_scc_clock_data_step_from_mode(clock_step,
                                               channel->regs[CLEM_SCC_WR4_CLOCK_DATA_RATE] & 0xc0);
}

static void _clem_scc_channel_set_master_clock_step(struct ClemensDeviceSCCChannel *channel,
                                                    clem_clocks_duration_t clock_step) {

    channel->master_clock_step = _clem_scc_channel_calc_clock_step(channel, clock_step);
}

static inline void _clem_scc_brg_counter_reset(struct ClemensDeviceSCCChannel *channel, bool ff) {
    channel->brg_counter = (ff ? CLEM_SCC_BRG_STATUS_FF_FLAG : 0x0) |
                           ((unsigned)channel->regs[CLEM_SCC_WR13_TIME_CONST_HI] << 16) |
                           channel->regs[CLEM_SCC_WR12_TIME_CONST_LO];
}

static inline bool _clem_scc_is_xtal_enabled(struct ClemensDeviceSCCChannel *channel) {
    return (channel->regs[CLEM_SCC_WR14_MISC_CONTROL] & CLEM_SCC_CLK_XTAL_ON) != 0;
}

static inline bool _clem_scc_is_brg_on(struct ClemensDeviceSCCChannel *channel) {
    return (channel->regs[CLEM_SCC_WR14_MISC_CONTROL] & CLEM_SCC_CLK_BRG_ON) != 0;
}

static inline bool _clem_scc_is_brg_clock_mode(struct ClemensDeviceSCCChannel *channel,
                                               uint8_t mode) {
    bool brg_pclk = (channel->regs[CLEM_SCC_WR14_MISC_CONTROL] & CLEM_SCC_CLK_BRG_PCLK) != 0;
    if (_clem_scc_is_xtal_enabled(channel)) {
        return mode == CLEM_SCC_CLOCK_MODE_XTAL && !brg_pclk;
    } else {
        return mode == CLEM_SCC_CLOCK_MODE_PCLK && brg_pclk;
    }
}

static bool _clem_scc_brg_tick(struct ClemensDeviceSCCChannel *channel) {
    //  every tick decreases the counter - if 0, then load in a new counter
    //  when 0, toggle the ff bit and reload the counter
    if (!_clem_scc_is_brg_on(channel))
        return false;

    //  the caller should've handled the pulse after the last call to tick()
    channel->brg_counter &= ~CLEM_SCC_BRG_STATUS_PULSE_FLAG;

    if (!(channel->brg_counter & CLEM_SCC_BRG_STATUS_COUNTER_MASK)) {
        //  a transition from high to low indicates a BRG pulse on the transmitter or receiver
        bool old_ff_status = (channel->brg_counter & CLEM_SCC_BRG_STATUS_FF_FLAG) != 0;
        _clem_scc_brg_counter_reset(channel, !old_ff_status);
        if (old_ff_status && !(channel->brg_counter & CLEM_SCC_BRG_STATUS_FF_FLAG)) {
            channel->brg_counter |= CLEM_SCC_BRG_STATUS_PULSE_FLAG;
        }
    } else {
        channel->brg_counter--;
    }
    return (channel->brg_counter & CLEM_SCC_BRG_STATUS_PULSE_FLAG) != 0;
}

static void _clem_scc_do_rx_tx(struct ClemensDeviceSCCChannel *channel, bool trxc_pulse,
                               bool rtxc_pulse, bool brg_pulse) {
    uint8_t mode = channel->regs[CLEM_SCC_WR11_CLOCK_MODE] & 0x60;
    switch (mode) {
    case CLEM_SCC_CLK_RX_SOURCE_BRG:
        break;
    case CLEM_SCC_CLK_RX_SOURCE_TRxC:
        break;
    case CLEM_SCC_CLK_RX_SOURCE_RTxC:
        break;
    case CLEM_SCC_CLK_RX_SOURCE_DPLL:
        //  TODO?
        break;
    }
    mode = channel->regs[CLEM_SCC_WR11_CLOCK_MODE] & 0x18;
    switch (mode) {
    case CLEM_SCC_CLK_TX_SOURCE_BRG:
        break;
    case CLEM_SCC_CLK_TX_SOURCE_TRxC:
        break;
    case CLEM_SCC_CLK_TX_SOURCE_RTxC:
        break;
    case CLEM_SCC_CLK_TX_SOURCE_DPLL:
        //  TODO?
        break;
    }
}

////////////////////////////////////////////////////////////////////////////////
/*
void clem_scc_reset_channel_clocks(struct ClemensDeviceSCC *scc, unsigned ch_idx,
                                   unsigned options) {
    // The transmitter and receiver each operate on a clock defined by registers
    // WR4 (clock mode), WR11 (TRxC, RTxC) and W14 (baud rate gen.)  These
    // clocks align with the data rate - so for synchronous communication,
    // x1 clock steps are enforced (very slow - likely not used in real world
    // IIGS applications? fingers crossed.)
    struct ClemensDeviceSCCChannel *channel = &scc->channel[ch_idx];
    bool brg_on = (channel->regs[CLEM_SCC_WR14_MISC_CONTROL] & CLEM_SCC_CLK_BRG_ON) != 0;
    bool brg_rx =
        (brg_on && channel->regs[CLEM_SCC_WR11_CLOCK_MODE] & CLEM_SCC_CLK_RX_SOURCE_BRG) != 0;
    bool brg_tx =
        (brg_on && channel->regs[CLEM_SCC_WR11_CLOCK_MODE] & CLEM_SCC_CLK_TX_SOURCE_BRG) != 0;
    uint8_t clk_mode = channel->regs[CLEM_SCC_WR4_CLOCK_DATA_RATE] & 0xc0;
    clem_clocks_duration_t step_rx =
        (brg_rx && channel->regs[CLEM_SCC_WR14_MISC_CONTROL] & CLEM_SCC_CLK_BRG_PCLK)
            ? scc->pclk_step
            : scc->xtal_step;
    //  TX source
    if (options & CLEM_SCC_RESET_TX_CLOCK) {
        switch (channel->regs[CLEM_SCC_WR11_CLOCK_MODE] & 0x18) {
        case CLEM_SCC_CLK_TX_SOURCE_RTxC:
            channel->tx_clock_step = _clem_scc_is_xtal_on(channel) ? scc->xtal_step : 0;
            break;
        case CLEM_SCC_CLK_TX_SOURCE_TRxC:
            //  poll at the data rate to wait for the TRxC signal
            channel->tx_clock_step = scc->xtal_step;
            break;
        case CLEM_SCC_CLK_TX_SOURCE_BRG:
            channel->tx_clock_step =
                (brg_tx && channel->regs[CLEM_SCC_WR14_MISC_CONTROL] & CLEM_SCC_CLK_BRG_PCLK)
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
        switch (channel->regs[CLEM_SCC_WR11_CLOCK_MODE] & 0x60) {
        case CLEM_SCC_CLK_RX_SOURCE_RTxC:
            channel->rx_clock_step = _clem_scc_is_xtal_on(channel) ? scc->xtal_step : 0;
            break;
        case CLEM_SCC_CLK_RX_SOURCE_TRxC:
            channel->tx_clock_step = scc->xtal_step;
            break;
        case CLEM_SCC_CLK_RX_SOURCE_BRG:
            channel->rx_clock_step =
                (brg_rx && channel->regs[CLEM_SCC_WR14_MISC_CONTROL] & CLEM_SCC_CLK_BRG_PCLK)
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
*/

void clem_scc_reset_channel(struct ClemensDeviceSCC *scc, clem_clocks_time_t ts, unsigned ch_idx,
                            bool hw) {
    struct ClemensDeviceSCCChannel *channel = &scc->channel[ch_idx];
    if (hw) {
        memset(channel->regs, 0, sizeof(channel->regs));
        channel->regs[CLEM_SCC_WR11_CLOCK_MODE] |= CLEM_SCC_CLK_TX_SOURCE_TRxC;
        channel->master_clock_ts = ts;
        channel->xtal_edge_ts =
            channel->master_clock_ts + _clem_scc_channel_calc_clock_step(channel, scc->xtal_step);
        channel->pclk_edge_ts =
            channel->master_clock_ts + _clem_scc_channel_calc_clock_step(channel, scc->pclk_step);
    }
    _clem_scc_channel_set_master_clock_step(channel, scc->xtal_step);
    channel->regs[CLEM_SCC_WR4_CLOCK_DATA_RATE] |= CLEM_SCC_STOP_BIT_1;
    channel->rr0 |= CLEM_SCC_RR0_TX_EMPTY;
    _clem_scc_brg_counter_reset(channel, true);
}

void clem_scc_sync_channel_uart(struct ClemensDeviceSCC *scc, unsigned ch_idx,
                                clem_clocks_time_t next_ts) {
    struct ClemensDeviceSCCChannel *channel = &scc->channel[ch_idx];
    bool rx_enabled = _clem_scc_is_rx_enabled(channel);
    bool tx_enabled = _clem_scc_is_tx_enabled(channel);

    //  device and scc combined poll
    //  multiple serial ops from a potential peer can occur multiple times per
    //  call to this sync() function by design (since instructions can be several
    //  1 mhz cycles, though realistically that implies something like 200-300k baud rate)
    //  so likely not...?
    //
    bool trxc_pulse = false;
    bool brg_pulse = false;

    if (!rx_enabled && !tx_enabled) {
        channel->master_clock_ts = next_ts;
        channel->xtal_edge_ts =
            channel->master_clock_ts + _clem_scc_channel_calc_clock_step(channel, scc->xtal_step);
        channel->pclk_edge_ts =
            channel->master_clock_ts + _clem_scc_channel_calc_clock_step(channel, scc->pclk_step);
        return;
    }

    while (channel->master_clock_ts < next_ts) {
        while (channel->master_clock_ts >= channel->xtal_edge_ts ||
               channel->master_clock_step >= channel->pclk_edge_ts) {
            if (channel->master_clock_ts >= channel->xtal_edge_ts) {
                if (_clem_scc_is_xtal_enabled(channel)) {
                    trxc_pulse = (channel->serial_port & CLEM_SCC_PORT_HSKI) != 0;
                    if (_clem_scc_is_brg_clock_mode(channel, CLEM_SCC_CLOCK_MODE_XTAL)) {
                        brg_pulse = _clem_scc_brg_tick(channel);
                    }
                    _clem_scc_do_rx_tx(channel, trxc_pulse, true, brg_pulse);
                }

                channel->xtal_edge_ts += _clem_scc_channel_calc_clock_step(channel, scc->xtal_step);
            }
            if (channel->master_clock_step >= channel->pclk_edge_ts) {
                if (_clem_scc_is_brg_clock_mode(channel, CLEM_SCC_CLOCK_MODE_PCLK)) {
                    brg_pulse = _clem_scc_brg_tick(channel);
                }
                if (_clem_scc_is_xtal_enabled(channel)) {
                    trxc_pulse = false;
                } else {
                    trxc_pulse = (channel->serial_port & CLEM_SCC_PORT_HSKI) != 0;
                }
                _clem_scc_do_rx_tx(channel, trxc_pulse, false, brg_pulse);

                channel->pclk_edge_ts += _clem_scc_channel_calc_clock_step(channel, scc->xtal_step);
            }
        }

        channel->master_clock_ts += channel->master_clock_step;
    }
}

void clem_scc_reset(struct ClemensDeviceSCC *scc, struct ClemensTimeSpec *tspec) {
    //  equivalent to a hardware reset
    memset(scc, 0, sizeof(*scc));
    scc->xtal_step = (clem_clocks_duration_t)floor(
        (14.31818 / CLEM_CLOCKS_SCC_XTAL_MHZ) * CLEM_CLOCKS_14MHZ_CYCLE + 0.5);
    scc->pclk_step = CLEM_CLOCKS_CREF_CYCLE;

    clem_scc_reset_channel(scc, tspec->clocks_spent, 0, true);
    clem_scc_reset_channel(scc, tspec->clocks_spent, 1, true);
}

void clem_scc_glu_sync(struct ClemensDeviceSCC *scc, struct ClemensClock *clock) {
    //  channel A and B xmit/recv functionality is driven by either the PCLK
    //  or the XTAL (via configuration)
    //  synchronization between channel and GLU occurs on PCLK intervals

    clem_scc_sync_channel_uart(scc, 0, clock->ts);
    clem_scc_sync_channel_uart(scc, 1, clock->ts);
}

void clem_scc_reg_interrupt_vector_wr2(struct ClemensDeviceSCC *scc, uint8_t value) {
    //  a single value replicated on both channels
    CLEM_SCC_LOG("SCC: WR2 Interrupt Vector %02x", value);
    scc->channel[0].regs[CLEM_SCC_WR2_INT_VECTOR] = value;
    scc->channel[1].regs[CLEM_SCC_WR2_INT_VECTOR] = value;
}

void clem_scc_reg_recv_control_wr3(struct ClemensDeviceSCC *scc, struct ClemensTimeSpec *tspec,
                                   unsigned ch_idx, uint8_t value) {
    uint8_t xvalue = scc->channel[ch_idx].regs[CLEM_SCC_WR3_RECV_CONTROL] ^ value;
    if (!xvalue)
        return;

    CLEM_SCC_LOG("SCC: WR3 %c RX:%s, %02X", 'A' + ch_idx, value & CLEM_SCC_RX_ENABLE ? "ON" : "OFF",
                 value);
    scc->channel[ch_idx].regs[CLEM_SCC_WR3_RECV_CONTROL] = value;
}

void clem_scc_reg_clock_data_rate_wr4(struct ClemensDeviceSCC *scc, unsigned ch_idx,
                                      uint8_t value) {
    uint8_t xvalue = scc->channel[ch_idx].regs[CLEM_SCC_WR4_CLOCK_DATA_RATE] ^ value;
    if (!xvalue)
        return;

    if (xvalue & 0x0c) {
        if ((value & 0xc) == CLEM_SCC_STOP_SYNC_MODE) {
            CLEM_UNIMPLEMENTED("SCC: WR4 %c Synchronous Mode Set", 'A' + ch_idx);
        }
    }
    CLEM_SCC_LOG("SCC: WR4 %c X%u CLK, %02X", ch_idx ? 'B' : 'A', s_scc_clk_x_speeds[value >> 6],
                 value);

    scc->channel[ch_idx].regs[CLEM_SCC_WR4_CLOCK_DATA_RATE] = value;
}

void clem_scc_reg_xmit_control_wr5(struct ClemensDeviceSCC *scc, struct ClemensTimeSpec *tspec,
                                   unsigned ch_idx, uint8_t value) {
    uint8_t xvalue = scc->channel[ch_idx].regs[CLEM_SCC_WR5_XMIT_CONTROL] ^ value;
    if (!xvalue)
        return;

    CLEM_SCC_LOG("SCC: WR5 %c TX:%s, %02X", 'A' + ch_idx, value & CLEM_SCC_TX_ENABLE ? "ON" : "OFF",
                 value);

    scc->channel[ch_idx].regs[CLEM_SCC_WR5_XMIT_CONTROL] = value;
}

void clem_scc_reg_xmit_byte(struct ClemensDeviceSCC *scc, unsigned ch_idx, uint8_t value) {
    //  this can always be written to, but it's emptied by clem_scc_sync_channel_uart()
    //  this byte is shifted into the tx_register when the tx_shift_ctr is 0
    //  additional bits may be prepended/appended based on various settings detailed
    //  in clem_scc_sync_channel_uart.
    if (!(scc->channel[ch_idx].rr0 & CLEM_SCC_RR0_TX_EMPTY)) {
        CLEM_WARN("SCC: WR8 %c tx buffer full", ch_idx + 'A');
    }
    scc->channel[ch_idx].tx_byte = value;
    scc->channel[ch_idx].rr0 &= ~CLEM_SCC_RR0_TX_EMPTY;
    CLEM_WARN("SCC: WR8 %c tx %02X", ch_idx + 'A', value);
}

void clem_scc_reg_master_interrupt_wr9(struct ClemensDeviceSCC *scc, clem_clocks_time_t ts,
                                       uint8_t value) {
    uint8_t reset = (value & 0xc0);

    switch (reset) {
    case 0x40:
        CLEM_SCC_LOG("SCC: WR9 B reset");
        clem_scc_reset_channel(scc, ts, 1, false);
        break;
    case 0x80:
        CLEM_SCC_LOG("SCC: WR9 A reset");
        clem_scc_reset_channel(scc, ts, 0, false);
        break;
    case 0xc0:
        CLEM_SCC_LOG("SCC: WR9 Hardware Reset");
        clem_scc_reset_channel(scc, ts, 0, true);
        clem_scc_reset_channel(scc, ts, 1, true);
        break;
    }
}

void clem_scc_reg_clock_mode_wr11(struct ClemensDeviceSCC *scc, unsigned ch_idx, uint8_t value) {
    static const char *s_clock_sources[] = {"RTxC", "TRxC", "BRG", "DPLL"};
    uint8_t xvalue = scc->channel[ch_idx].regs[CLEM_SCC_WR11_CLOCK_MODE] ^ value;
    if (!xvalue)
        return;

    CLEM_SCC_LOG("SCC: WR11 %c XTAL:%s TRxC:%02X", ch_idx ? 'B' : 'A',
                 value & CLEM_SCC_CLK_XTAL_ON ? "ON" : "OFF", value & 0x3);
    if (xvalue & 0x60) { // recv clk changed
        CLEM_SCC_LOG("SCC: WR11 %c rx_clock %s", ch_idx ? 'B' : 'A',
                     s_clock_sources[(value & 0x60) >> 5]);
    }
    if (xvalue & 0x18) { // send clk changed
        CLEM_SCC_LOG("SCC: WR11 %c tx_clock %s", ch_idx ? 'B' : 'A',
                     s_clock_sources[(value & 0x18) >> 3]);
    }
    // TODO: TRxC O/I and special case overrides

    scc->channel[ch_idx].regs[CLEM_SCC_WR11_CLOCK_MODE] = value;
}

void clem_scc_reg_dpll_command_wr14(struct ClemensDeviceSCC *scc, unsigned ch_idx, uint8_t value) {
    static const char *mode_names[] = {"NUL",     "SEARCH",   "RSTCM",  "NODPLL",
                                       "BRGDPLL", "DPLLRTxC", "DPLLFM", "DPLLNRZI"};
    uint8_t xvalue = (scc->channel[ch_idx].regs[CLEM_SCC_WR14_MISC_CONTROL] & 0xe0) ^ value;
    if (!xvalue)
        return;
    CLEM_SCC_LOG("SCCL WR14 %c %s", 'A' + ch_idx, mode_names[value >> 5]);
    scc->channel[ch_idx].regs[CLEM_SCC_WR14_MISC_CONTROL] &= 0x1f;
    scc->channel[ch_idx].regs[CLEM_SCC_WR14_MISC_CONTROL] |= value;
}

void clem_scc_reg_baud_gen_enable_wr14(struct ClemensDeviceSCC *scc, unsigned ch_idx,
                                       uint8_t value) {
    struct ClemensDeviceSCCChannel *channel = &scc->channel[ch_idx];
    uint8_t xvalue = (channel->regs[CLEM_SCC_WR14_MISC_CONTROL] & 0x03) ^ value;
    if (!xvalue)
        return;

    CLEM_SCC_LOG("SCC: WR14 %c BRG %s %s", 'A' + ch_idx,
                 value & CLEM_SCC_CLK_BRG_PCLK ? "PCLK" : "XTAL",
                 value & CLEM_SCC_CLK_BRG_ON ? "ON" : "OFF");

    if ((xvalue & CLEM_SCC_CLK_BRG_ON) && (value & CLEM_SCC_CLK_BRG_ON)) {
        //  BRG enabled, start pulse on high
        _clem_scc_brg_counter_reset(channel, true);
    }

    channel->regs[CLEM_SCC_WR14_MISC_CONTROL] &= 0xfc;
    channel->regs[CLEM_SCC_WR14_MISC_CONTROL] |= value;
}

void clem_scc_reg_misc_control_wr14(struct ClemensDeviceSCC *scc, unsigned ch_idx, uint8_t value) {
    uint8_t xvalue = (scc->channel[ch_idx].regs[CLEM_SCC_WR14_MISC_CONTROL] & 0x1c) ^ value;
    if (!xvalue)
        return;

    CLEM_SCC_LOG("SCC: WR14 %c LOOPBACK", 'A' + ch_idx,
                 value & CLEM_SCC_LOCAL_LOOPBACK ? "ON" : "OFF");
    CLEM_SCC_LOG("SCC: WR14 %c AUTO ECHO", 'A' + ch_idx, value & CLEM_SCC_AUTO_ECHO ? "ON" : "OFF");
    CLEM_SCC_LOG("SCC: WR14 %c DTR_FN", 'A' + ch_idx, value & CLEM_SCC_DTR_FUNCTION ? "ON" : "OFF");

    scc->channel[ch_idx].regs[CLEM_SCC_WR14_MISC_CONTROL] &= 0xe3;
    scc->channel[ch_idx].regs[CLEM_SCC_WR14_MISC_CONTROL] |= value;
}

void clem_scc_reg_set_interrupt_enable_wr15(struct ClemensDeviceSCC *scc, unsigned ch_idx,
                                            uint8_t value) {
    CLEM_SCC_LOG("SCC: WR15 %c mode %02x", ch_idx ? 'B' : 'A', value);
    scc->channel[ch_idx].regs[CLEM_SCC_WR15_INT_ENABLE] = value;
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
                clem_scc_reg_interrupt_vector_wr2(scc, value);
                break;
            case CLEM_SCC_WR3_RECV_CONTROL:
                clem_scc_reg_recv_control_wr3(scc, tspec, ch_idx, value);
                break;
            case CLEM_SCC_WR4_CLOCK_DATA_RATE:
                clem_scc_reg_clock_data_rate_wr4(scc, ch_idx, value);
                break;
            case CLEM_SCC_WR5_XMIT_CONTROL:
                clem_scc_reg_xmit_control_wr5(scc, tspec, ch_idx, value);
                break;
            case CLEM_SCC_WR6_SYNC_CHAR_1:
                scc->channel[ch_idx].regs[CLEM_SCC_WR6_SYNC_CHAR_1] = value;
                CLEM_SCC_LOG("SCC: WR6 %c SYNC %02X", 'A' + ch_idx, value);
                break;
            case CLEM_SCC_WR7_SYNC_CHAR_2:
                scc->channel[ch_idx].regs[CLEM_SCC_WR7_SYNC_CHAR_2] = value;
                CLEM_SCC_LOG("SCC: WR7 %c SYNC %02X", 'A' + ch_idx, value);
                break;
            case CLEM_SCC_WR8_XMIT_BUFFER:
                clem_scc_reg_xmit_byte(scc, ch_idx, value);
                break;
            case CLEM_SCC_WR9_MASTER_INT:
                clem_scc_reg_master_interrupt_wr9(scc, tspec->clocks_spent, value);
                break;
            case CLEM_SCC_WR11_CLOCK_MODE:
                clem_scc_reg_clock_mode_wr11(scc, ch_idx, value);
                break;
            case CLEM_SCC_WR12_TIME_CONST_LO:
                scc->channel[ch_idx].regs[CLEM_SCC_WR12_TIME_CONST_LO] = value;
                CLEM_SCC_LOG("SCC: WR12 %c TC LO %02x", 'A' + ch_idx, value);
                break;
            case CLEM_SCC_WR13_TIME_CONST_HI:
                scc->channel[ch_idx].regs[CLEM_SCC_WR13_TIME_CONST_HI] = value;
                CLEM_SCC_LOG("SCC: WR13 %c TC HI %02x", 'A' + ch_idx, value);
                break;
            case CLEM_SCC_WR14_MISC_CONTROL:
                clem_scc_reg_dpll_command_wr14(scc, ch_idx, value & 0xe0);
                clem_scc_reg_baud_gen_enable_wr14(scc, ch_idx, value & 0x3);
                clem_scc_reg_misc_control_wr14(scc, ch_idx, value & 0x1c);
                break;
            case CLEM_SCC_WR15_INT_ENABLE:
                clem_scc_reg_set_interrupt_enable_wr15(scc, ch_idx, value);
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
        ch_idx = CLEM_MMIO_REG_SCC_A_CMD - ioreg;
        CLEM_SCC_LOG("SCC: WR DATA %c <= %02X", 'A' + ch_idx, value);
        break;
    }
    // CLEM_LOG("clem_scc: %02X <- %02X", ioreg, value);
}

uint8_t clem_scc_reg_get_tx_rx_status(struct ClemensDeviceSCC *scc, unsigned ch_idx) {
    struct ClemensDeviceSCCChannel *channel = &scc->channel[ch_idx];
    return channel->rr0;
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
            case CLEM_SCC_RR0_STATUS:
                value = clem_scc_reg_get_tx_rx_status(scc, ch_idx);
                break;
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
