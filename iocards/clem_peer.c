#include "clem_peer.h"

void clem_peripheral_peer_init(ClemensPeripheralPeer* peer) {
    peer->recv_queue_head = peer->recv_queue_tail = 0;
    peer->send_queue_head = peer->send_queue_tail = 0;
}

const uint8_t* clem_peripheral_peer_send_bytes(ClemensPeripheralPeer* peer, 
                                               const uint8_t* bytes, unsigned byte_cnt) {
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
    for (i = 0; i < byte_cnt && remaining > 0; i++, bytes++,r remaining--) {
        peer->send_queue[i + peer->send_queue_tail] = *bytes;
    }
    return bytes;
}

void clem_peripheral_peer_transact(ClemensPeripheralPeer* peer, unsigned* serial_port) {

}