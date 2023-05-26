#ifndef CLEM_PERIPHERAL_PEER_DEVICE_H
#define CLEM_PERIPHERAL_PEER_DEVICE_H

#include "clem_shared.h"

#ifdef __cplusplus
extern "C" {
#endif

#define CLEM_PERI_PEER_QUEUE_SIZE 16

/**
 * @brief Defines a peer for serial operations that can be built upon for devices
 *
 * The clocks timing is based on the relative clocks defined in clem_shared.h.
 * For serial operations to work, both the peer and the emulator needs to run
 * at the same clock rate.
 */
typedef struct {
    /* time of last call to transact */
    clem_clocks_time_t last_transact_time;
    /* number of clocks until the next bit is sent or read */
    clem_clocks_duration_t baud_gen_clocks_dt;
    uint8_t send_queue[CLEM_PERI_PEER_QUEUE_SIZE];
    int send_queue_head;
    int send_queue_tail;
    uint8_t recv_queue[CLEM_PERI_PEER_QUEUE_SIZE];
    int recv_queue_head;
    int recv_queue_tail;
} ClemensSerialPeer;

void clem_serial_peer_init(ClemensSerialPeer *peer, struct ClemensClock *clock);

void clem_serial_peer_set_baud_rate(ClemensSerialPeer *peer, enum ClemensSerialBaudRate baud_rate);

const uint8_t *clem_serial_peer_send_bytes(ClemensSerialPeer *peer, const uint8_t *bytes,
                                           unsigned byte_cnt);

void clem_serial_peer_transact(ClemensSerialPeer *peer, struct ClemensClock *clock,
                               unsigned *serial_port);

#ifdef __cplusplus
}
#endif

#endif
