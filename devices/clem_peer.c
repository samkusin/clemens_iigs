#include "clem_peer.h"
#include "clem_shared.h"

#include <stdbool.h>

/* This is generally a prototype of the eventual SCC implementation minus the
   interface and more esoteric functionalty.
*/

static unsigned s_baud_rates[] = {0, 300, 1200, 2400, 4800, 9600, 19200, 38400, 57600, 0};

static double s_xtal_frequency = 28.636e6;

void clem_serial_peer_init(ClemensSerialPeer *peer, struct ClemensClock *clock) {
    peer->recv_queue_head = peer->recv_queue_tail = 0;
    peer->send_queue_head = peer->send_queue_tail = 0;
    peer->last_transact_time = clock->ts;
    peer->leftover_baud_gen_clocks_dt = 0;
    peer->xmit_shift_reg = 0;
    peer->recv_shift_reg = 0;
    clem_serial_peer_set_baud_rate(peer, kClemensSerialBaudRate_Clock);
}

void clem_serial_peer_set_baud_rate(ClemensSerialPeer *peer, enum ClemensSerialBaudRate baud_rate) {
    //  tc = ( xtal / (2 * baud_rate * clock_div) ) - 2
    if (baud_rate == kClemensSerialBaudRate_Clock) {
        peer->baud_gen_clocks_dt = CLEM_CLOCKS_28MHZ_CYCLE;
    } else if (baud_rate == kClemensSerialBaudRate_None) {
        peer->baud_gen_clocks_dt = 0;
    } else {
        peer->baud_gen_clocks_dt =
            (s_xtal_frequency / s_baud_rates[baud_rate]) * CLEM_CLOCKS_28MHZ_CYCLE;
    }
}

unsigned clem_serial_peer_send_bytes(ClemensSerialPeer *peer, const uint8_t *bytes,
                                     unsigned byte_cnt) {
    unsigned remaining = CLEM_PERI_PEER_QUEUE_SIZE - peer->send_queue_tail;
    unsigned i;
    if (remaining < byte_cnt) {
        for (i = peer->send_queue_head; i < peer->send_queue_tail; i++) {
            peer->send_queue[i - peer->send_queue_head] = peer->send_queue[i];
        }
        peer->send_queue_tail = i - peer->send_queue_head;
        peer->send_queue_head = 0;
        remaining = CLEM_PERI_PEER_QUEUE_SIZE;
    }
    for (i = 0; i < byte_cnt && remaining > 0; i++, bytes++, remaining--) {
        peer->send_queue[peer->send_queue_tail++] = *bytes;
    }
    return i;
}

unsigned clem_serial_peer_recv_byte(ClemensSerialPeer *peer, uint8_t *bytes, unsigned bytes_limit) {
    uint8_t *bytes_end = bytes + bytes_limit;
    unsigned cnt = 0;
    while (bytes < bytes_end && peer->recv_queue_head < peer->recv_queue_tail) {
        *(bytes++) = peer->recv_queue[peer->recv_queue_head++];
        cnt++;
    }
    return cnt;
}

/*
 *  Starting from a no connection, we have xmit,recv queues that are controlled
 *  by the signals from `serial_port`.
 *
 *  The send queue will be emptied when CTS is signaled
 *  The recv queue will be emptied if there is content
 *
 *  The UART here will fill in the shift registers with start/stop/parity bits
 *  one bit at a time when enough time (measured by peer->baud_gen_clocks_dt)
 *  has passed.
 *
 */
void clem_serial_peer_transact(ClemensSerialPeer *peer, struct ClemensClock *clock,
                               unsigned *serial_port) {
    clem_clocks_duration_t dt = clock->ts - peer->last_transact_time;

    while (peer->baud_gen_clocks_dt > 0 && dt >= peer->baud_gen_clocks_dt) {
        bool cts = (*serial_port & CLEM_SCC_PORT_HSKI) != 0;

        dt -= peer->baud_gen_clocks_dt;
    }

    peer->leftover_baud_gen_clocks_dt = dt;
    peer->last_transact_time = clock->ts;
}
