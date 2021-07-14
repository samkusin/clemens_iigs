#include "clem_device.h"
#include "clem_debug.h"

/* References:
    https://llx.com/Neil/a2/bram
    https://vintageapple.org/macbooks/pdf/Inside_Macintosh_Volumes_I-II-III_1985.pdf
*/

#define CLEM_RTC_C034_FLAG_START_XFER   0x80
#define CLEM_RTC_C034_FLAG_READ_OP      0x40
#define CLEM_RTC_C034_FLAG_LAST_BYTE    0x20
#define CLEM_RTC_C034_FLAG_MASK         0xE0

#define CLEM_RTC_EXECUTE_RECV_CMD           0x00
#define CLEM_RTC_EXECUTE_RECV_CMD_BRAM_R    0x01
#define CLEM_RTC_EXECUTE_READ_BRAM          0x02
#define CLEM_RTC_EXECUTE_RECV_CMD_BRAM_W    0x04
#define CLEM_RTC_EXECUTE_WRITE_BRAM         0x05
#define CLEM_RTC_EXECUTE_REG_TEST           0x06
#define CLEM_RTC_EXECUTE_REG_WRITE_PROTECT  0x07
#define CLEM_RTC_EXECUTE_REG_UNKNOWN        0x08
#define CLEM_RTC_EXECUTE_READ_CLOCK         0x09
#define CLEM_RTC_EXECUTE_WRITE_CLOCK        0x0A
#define CLEM_RTC_EXECUTE_ERROR              0xff

#define CLEM_RTC_CMD_SECONDS_LO             0x00
#define CLEM_RTC_CMD_SECONDS_HI             0x01
#define CLEM_RTC_CMD_REGISTER               0x06
#define CLEM_RTC_CMD_BRAM                   0x07

#define CLEM_RTC_FLAG_WRITE_PROTECT         1




void clem_rtc_reset(
    struct ClemensDeviceRTC* rtc,
    clem_clocks_duration_t latency
) {
    rtc->data_c033 = 0x00;
    rtc->ctl_c034 = 0x00;
    rtc->flags = 0;
    rtc->xfer_latency_duration = latency;
    rtc->xfer_started_time = CLEM_TIME_UNINITIALIZED;
    rtc->state = CLEM_RTC_EXECUTE_RECV_CMD;
}

void clem_rtc_set_clock_time(
    struct ClemensDeviceRTC* rtc,
    uint32_t seconds_since_1904
) {
    rtc->seconds_since_1904 = seconds_since_1904;
}

static void _clem_rtc_dispatch_cmd(
    struct ClemensDeviceRTC* rtc,
    unsigned data
) {
    unsigned cmd = (data >> 3) & 0xf;
    unsigned read = (data & 0x80);
    unsigned opt = (data & 0x07);
    /* CLEM_LOG("clem_rtc_dispatch: ctl=%02X, data=%02X", rtc->ctl_c034, data); */
    switch (cmd) {
        case CLEM_RTC_CMD_SECONDS_LO:
            rtc->index = opt;
            if (read) {
                rtc->state = CLEM_RTC_EXECUTE_READ_CLOCK;
            } else {
                rtc->state = CLEM_RTC_EXECUTE_WRITE_CLOCK;
            }
            break;
        case CLEM_RTC_CMD_SECONDS_HI:
            rtc->index = (0x80000000 | opt);
            if (read) {
                rtc->state = CLEM_RTC_EXECUTE_READ_CLOCK;
            } else {
                rtc->state = CLEM_RTC_EXECUTE_WRITE_CLOCK;
            }
            break;
        case CLEM_RTC_CMD_REGISTER:
            /* special case registers - write only supported */
            if (!read) {
                rtc->index = opt;
                if (rtc->index == 0x1) {
                    rtc->state = CLEM_RTC_EXECUTE_REG_TEST;
                } else if (rtc->index == 0x5) {
                    rtc->state = CLEM_RTC_EXECUTE_REG_WRITE_PROTECT;
                } else if (rtc->index == 0x7) {
                    rtc->state = CLEM_RTC_EXECUTE_REG_UNKNOWN;
                } else {
                    CLEM_UNIMPLEMENTED("RTC: register op is unsupported", rtc->index);
                }
            } else {
                CLEM_WARN("RTC: reg read is unsupported", cmd);
            }
            break;
        case CLEM_RTC_CMD_BRAM:
            /* bram read or write */
            if (read) {
                rtc->state = CLEM_RTC_EXECUTE_RECV_CMD_BRAM_R;
            } else {
                rtc->state = CLEM_RTC_EXECUTE_RECV_CMD_BRAM_W;
            }
            rtc->index = opt << 5;
            break;
        default:
            CLEM_UNIMPLEMENTED("RTC %02X", cmd);
            break;
    }
}

void _clem_rtc_bram_state(
    struct ClemensDeviceRTC* rtc,
    unsigned data
) {
    /* read second part of the index embedded in the data packet */
    /* CLEM_LOG("clem_rtc_bram: ctl=%02X, data=%02X", rtc->ctl_c034, data); */
    rtc->index = rtc->index | ((data >> 2) & 0x1f);
    if (rtc->state == CLEM_RTC_EXECUTE_RECV_CMD_BRAM_R) {
        rtc->state = CLEM_RTC_EXECUTE_READ_BRAM;
    } else if (rtc->state == CLEM_RTC_EXECUTE_RECV_CMD_BRAM_W) {
        rtc->state = CLEM_RTC_EXECUTE_WRITE_BRAM;
    } else {
        CLEM_ASSERT(false);
    }
}

uint8_t _clem_rtc_bram_read(
    struct ClemensDeviceRTC* rtc
) {
    uint8_t data = rtc->bram[rtc->index & 0xff];
    /*
    CLEM_LOG("clem_rtc_bram_read: @%02X ctl=%02X, data=%02X",
             rtc->index, rtc->ctl_c034, data);
    */
    CLEM_ASSERT(rtc->state == CLEM_RTC_EXECUTE_READ_BRAM);
    return data;
}

void _clem_rtc_bram_write(
    struct ClemensDeviceRTC* rtc,
    unsigned data
) {
    /*
    CLEM_LOG("clem_rtc_bram_write: @%02X ctl=%02X, data=%02X",
             rtc->index, rtc->ctl_c034, data);
    */
    CLEM_ASSERT(rtc->state == CLEM_RTC_EXECUTE_WRITE_BRAM);
    rtc->bram[rtc->index & 0xff] = data;
}

uint8_t _clem_rtc_clock_read(struct ClemensDeviceRTC* rtc) {
    unsigned opt = rtc->index & 0xff;
    if (opt & 1) {
        if (rtc->index & 0x80000000) {
            /* hi portion of clock data */
            if (opt & 0x4) return (uint8_t)((rtc->seconds_since_1904 >> 24) & 0xff);
            else return (uint8_t)((rtc->seconds_since_1904 >> 16) & 0xff);
        } else {
            /* lo portion of clock data */
            if (opt & 0x4) return (uint8_t)((rtc->seconds_since_1904 >> 8) & 0xff);
            else return (uint8_t)(rtc->seconds_since_1904 & 0xff);
        }
    }
    CLEM_WARN("clem_rtc: clock read bad opt (%02X)", opt);
    return 0;
}

void clem_rtc_command(
    struct ClemensDeviceRTC* rtc,
    clem_clocks_time_t ts,
    unsigned op
) {
    /* A command involves 1 or more data bytes being sent to or received
        by the RTC controller.  This is done serially in hardware, but here we
        implement a state machine based on the incoming data and control bits
        from the application via MMIO
    */
    bool is_write_cmd = !(rtc->ctl_c034 & CLEM_RTC_C034_FLAG_READ_OP);
    bool has_recv_started = (rtc->ctl_c034 & CLEM_RTC_C034_FLAG_START_XFER);
    bool is_new_txn = !(rtc->ctl_c034 & CLEM_RTC_C034_FLAG_LAST_BYTE);

    if (op == CLEM_IO_WRITE) {
        if (is_new_txn) {
            rtc->xfer_started_time = CLEM_TIME_UNINITIALIZED;
            rtc->state = CLEM_RTC_EXECUTE_RECV_CMD;
            return;
        }

        switch (rtc->state) {
        case CLEM_RTC_EXECUTE_RECV_CMD:
            if (has_recv_started && is_write_cmd) {
                _clem_rtc_dispatch_cmd(rtc, rtc->data_c033);
            }
            else {
                CLEM_WARN("RTC: unexpected ctrl %02X, state: %02X",
                    rtc->ctl_c034, rtc->state);
            }
            break;
        case CLEM_RTC_EXECUTE_RECV_CMD_BRAM_R:
        case CLEM_RTC_EXECUTE_RECV_CMD_BRAM_W:
            if (has_recv_started && is_write_cmd) {
                _clem_rtc_bram_state(rtc, rtc->data_c033);
            }
            else {
                CLEM_WARN("RTC: unexpected ctrl %02X, state: %02X",
                    rtc->ctl_c034, rtc->state);
            }
            break;
        case CLEM_RTC_EXECUTE_READ_BRAM:
            if (has_recv_started) {
                if (!is_write_cmd) {
                    rtc->data_c033 = _clem_rtc_bram_read(rtc);
                }
                else {
                    CLEM_WARN("RTC: unexpected ctrl $02X, state: %02X",
                        rtc->ctl_c034, rtc->state);
                }
            }
            break;
        case CLEM_RTC_EXECUTE_WRITE_BRAM:
            if (has_recv_started) {
                if (is_write_cmd) {
                    _clem_rtc_bram_write(rtc, rtc->data_c033);
                }
                else {
                    CLEM_WARN("RTC: unexpected ctrl $02X, state: %02X",
                        rtc->ctl_c034, rtc->state);
                }
            }
            break;
        case CLEM_RTC_EXECUTE_REG_TEST:
            if (rtc->data_c033 & 0xc0) {
                /* bits 6-7 must be zero */
                CLEM_WARN("RTC: non zero bits 6,7 set: %02X", rtc->data_c033);
            } else {
                CLEM_LOG("RTC: test register set to %02X", rtc->data_c033);
            }
            break;
        case CLEM_RTC_EXECUTE_REG_WRITE_PROTECT:
            if (rtc->data_c033 & 0x80) {
                rtc->flags |= CLEM_RTC_FLAG_WRITE_PROTECT;
            } else {
                rtc->flags |= CLEM_RTC_FLAG_WRITE_PROTECT;
            }
            CLEM_LOG("RTC: write-protect register set to %02X", rtc->data_c033);
            break;
        case CLEM_RTC_EXECUTE_REG_UNKNOWN:
            CLEM_LOG("RTC: register_unknown data %02X", rtc->data_c033);
            break;
        case CLEM_RTC_EXECUTE_READ_CLOCK:
            if (has_recv_started) {
                if (!is_write_cmd) {
                    rtc->data_c033 = _clem_rtc_clock_read(rtc);
                } else {

                }
            }
            break;
        case CLEM_RTC_EXECUTE_WRITE_CLOCK:
            break;
        default:
            break;
        }
    } else if (op == CLEM_IO_READ) {
        /* simulate delayed IO when polling for a response from the RTC chip */
        if (rtc->xfer_started_time == CLEM_TIME_UNINITIALIZED) {
            rtc->xfer_started_time = ts;
        }
        CLEM_ASSERT(rtc->xfer_started_time != CLEM_TIME_UNINITIALIZED);
        if (rtc->xfer_started_time + rtc->xfer_latency_duration > ts) return;

        rtc->xfer_started_time = CLEM_TIME_UNINITIALIZED;
        rtc->ctl_c034 &= ~CLEM_RTC_C034_FLAG_START_XFER;
        /* CLEM_LOG("clem_rtc_xfer: ctl=%02X, data=%02X", rtc->ctl_c034, rtc->data_c033); */
    }
}
