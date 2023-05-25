#ifndef CLEM_PERIPHERAL_PEER_DEVICE_H
#define CLEM_PERIPHERAL_PEER_DEVICE_H

#include "clem_shared.h"

#ifdef __cplusplus
extern "C" {
#endif

#define CLEM_PERI_PEER_QUEUE_SIZE  16

typedef struct {
    uint8_t send_queue[CLEM_PERI_PEER_QUEUE_SIZE];
    int send_queue_head;
    int send_queue_tail;
    uint8_t recv_queue[CLEM_PERI_PEER_QUEUE_SIZE];
    int recv_queue_head;
    int recv_queue_tail;
} ClemensPeripheralPeer;

void clem_peripheral_peer_init(ClemensPeripheralPeer* peer);

const uint8_t* clem_peripheral_peer_send_bytes(ClemensPeripheralPeer* peer, 
                                               const uint8_t* bytes, unsigned byte_cnt);

void clem_peripheral_peer_transact(ClemensPeripheralPeer* peer, unsigned* serial_port);

#ifdef __cplusplus
}
#endif

#endif