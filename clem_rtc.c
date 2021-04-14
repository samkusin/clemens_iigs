#include "clem_device.h"
#include "clem_debug.h"

#define CLEM_RTC_C034_FLAG_START_XFER   0x80
#define CLEM_RTC_C034_FLAG_READ_OP      0x40
#define CLEM_RTC_C034_FLAG_LAST_BYTE    0x20
#define CLEM_RTC_C034_FLAG_MASK         0xE0

#define CLEM_RTC_EXECUTE_RECV_CMD           0x00
#define CLEM_RTC_EXECUTE_RECV_CMD_BRAM_R    0x01
#define CLEM_RTC_EXECUTE_READ_BRAM          0x02
#define CLEM_RTC_EXECUTE_RECV_CMD_BRAM_W    0x04
#define CLEM_RTC_EXECUTE_WRITE_BRAM         0x05
#define CLEM_RTC_EXECUTE_ERROR              0xff

void clem_rtc_reset(
    struct ClemensDeviceRTC* rtc,
    clem_clocks_duration_t latency
) {
    rtc->data_c033 = 0x00;
    rtc->ctl_c034 = 0x00;
    rtc->xfer_latency_duration = latency;
    rtc->xfer_started_time = CLEM_TIME_UNINITIALIZED;
    rtc->state = CLEM_RTC_EXECUTE_RECV_CMD;
}

static void _clem_rtc_dispatch_cmd(
    struct ClemensDeviceRTC* rtc,
    unsigned data
) {
    /* r, c, c, c, c, d, d, d  where r = read, c = cmd, d = data */
    unsigned cmd = (data >> 3) & 0xf;
    /* CLEM_LOG("clem_rtc_dispatch: ctl=%02X, data=%02X", rtc->ctl_c034, data); */
    switch (cmd) {
        case 0x7:
            /* bram read or write */
            if (data & 0x80) {
                rtc->state = CLEM_RTC_EXECUTE_RECV_CMD_BRAM_R;
            } else {
                rtc->state = CLEM_RTC_EXECUTE_RECV_CMD_BRAM_W;
            }
            rtc->index = (data & 0x7) << 5;
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
    unsigned data = rtc->data_c033;

    if (op == CLEM_IO_WRITE) {
        if (is_new_txn) {
            rtc->xfer_started_time = CLEM_TIME_UNINITIALIZED;
            rtc->state = CLEM_RTC_EXECUTE_RECV_CMD;
            return;
        }

        switch (rtc->state) {
        case CLEM_RTC_EXECUTE_RECV_CMD:
            if (has_recv_started && is_write_cmd) {
                _clem_rtc_dispatch_cmd(rtc, data);
            }
            else {
                CLEM_WARN("clem_rtc: unexpected ctrl %02X, state: %02X",
                    rtc->ctl_c034, rtc->state);
            }
            break;
        case CLEM_RTC_EXECUTE_RECV_CMD_BRAM_R:
        case CLEM_RTC_EXECUTE_RECV_CMD_BRAM_W:
            if (has_recv_started && is_write_cmd) {
                _clem_rtc_bram_state(rtc, data);
            }
            else {
                CLEM_WARN("clem_rtc: unexpected ctrl %02X, state: %02X",
                    rtc->ctl_c034, rtc->state);
            }
            break;
        case CLEM_RTC_EXECUTE_READ_BRAM:
            if (has_recv_started) {
                if (!is_write_cmd) {
                    rtc->data_c033 = _clem_rtc_bram_read(rtc);
                }
                else {
                    CLEM_WARN("clem_rtc: unexpected ctrl $02X, state: %02X",
                        rtc->ctl_c034, rtc->state);
                }
            }
            break;
        case CLEM_RTC_EXECUTE_WRITE_BRAM:
            if (has_recv_started) {
                if (is_write_cmd) {
                    _clem_rtc_bram_write(rtc, data);
                }
                else {
                    CLEM_WARN("clem_rtc: unexpected ctrl $02X, state: %02X",
                        rtc->ctl_c034, rtc->state);
                }
            }
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
