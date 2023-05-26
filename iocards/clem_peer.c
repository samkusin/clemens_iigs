#include "clem_peer.h"
#include "clem_shared.h"

static unsigned s_baud_rates[] = {300, 1200, 2400, 4800, 9600, 19200, 38400, 57600, 0};

void clem_serial_peer_init(ClemensSerialPeer *peer, struct ClemensClock *clock) {
    peer->recv_queue_head = peer->recv_queue_tail = 0;
    peer->send_queue_head = peer->send_queue_tail = 0;
    peer->last_transact_time = clock->ts;
    clem_serial_peer_set_baud_rate(peer, kClemensSerialBaudRate_Clock);
}

void clem_serial_peer_set_baud_rate(ClemensSerialPeer *peer, enum ClemensSerialBaudRate baud_rate) {
    //  tc = ( xtal / (2 * baud_rate * clock_div) ) - 2
}

const uint8_t *clem_serial_peer_send_bytes(ClemensSerialPeer *peer, const uint8_t *bytes,
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
        peer->send_queue[i + peer->send_queue_tail] = *bytes;
    }
    return bytes;
}

void clem_serial_peer_transact(ClemensSerialPeer *peer, struct ClemensClock *clock,
                               unsigned *serial_port) {
    //
}
