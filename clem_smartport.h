#ifndef CLEM_SMARTPORT_H
#define CLEM_SMARTPORT_H

#include <stdint.h>

/* SmartPort devices by ID */
#define CLEM_SMARTPORT_DEVICE_ID_NONE         0
#define CLEM_SMARTPORT_DEVICE_ID_REFERENCE    1
#define CLEM_SMARTPORT_DEVICE_ID_PRODOS_HDD32 2

/* TODO: reevaluate - this should be some value around 768 */
#define CLEM_SMARTPORT_DATA_BUFFER_LIMIT 768
/* TODO: this should suffice for block devices, but the documentation hints at
         possibly larger values (but obviously limited by the packet size limit?)
*/
#define CLEM_SMARTPORT_CONTENTS_LIMIT 576

/* The INIT command that sends the assigned unit ID to the target device */
#define CLEM_SMARTPORT_COMMAND_INIT 0x05

#ifdef __cplusplus
extern "C" {
#endif

enum ClemensSmartPortPacketType {
    kClemensSmartPortPacketType_Unknown,
    kClemensSmartPortPacketType_Command,
    kClemensSmartPortPacketType_Status,
    kClemensSmartPortPacketType_Data
};

/**
 * @brief
 *
 */
struct ClemensSmartPortPacket {
    /** Where the packet originates from (0 = host, > 0 are bus residents) */
    uint8_t source_unit_id;
    /** Where the packet is going (0 = host, > 0 are bus residents)*/
    uint8_t dest_unit_id;
    /** Indicates how the packet should be handled by the recipient */
    enum ClemensSmartPortPacketType type;
    /** An extended SmartPort call (certain fields are extended) */
    uint8_t is_extended;
    /** Value is used by status and data packet types, and error code for commands */
    uint8_t status;
    /** Contents length */
    uint16_t contents_length;
    /*  Decoded contents (8-bit values) */
    uint8_t contents[CLEM_SMARTPORT_CONTENTS_LIMIT];
};

/**
 * @brief Defines the abstract interface to SmartPort based hardware
 *
 * To initialize, a SmartPort implementation should provide an intialization
 * function (for example):
 *      my_smartport_device_initialize(struct ClemensSmartPortUnit* unit)
 *
 * The implemenation should initialize whatever structures it needs for the
 * unit.  For convenience/dependency injection purposes, a 'device' pointer
 * and 'device_id' is provided inside this struct and should be filled so that
 * the host can access this information if needed (for debugging, internal
 * state management, etc.)
 *
 */
struct ClemensSmartPortUnit {
    /** A unique (to the emulator) ID Identifying the Smartport device
        implementation.   This in tandem with `device` is the custom state
        implemented for the device (i.e A specific HD brand, etc). */
    unsigned device_id;
    /** Device implemented in this unit.  This must be provided at emulator startup */
    void *device;
    /** Smartport bus resident on a bus being reset */
    unsigned (*bus_reset)(struct ClemensSmartPortUnit *context, unsigned phase_flags,
                          unsigned delta_ns);
    /** Smartport bus resident on an enabled bus */
    unsigned (*bus_enable)(struct ClemensSmartPortUnit *context, unsigned phase_flags,
                           unsigned delta_ns);

    /* Note these memebers are managed by the emulator and should not be modified
       unless you know what you're doing!
    */
    /** BUS ENABLE */
    uint8_t bus_enabled;
    /** PH3 is forced slow for downstream units */
    uint8_t ph3_latch_lo;
    /** Data register */
    uint8_t data_reg;
    /** Write head latch */
    uint8_t write_signal;
    /** The ID assigned by the host*/
    uint8_t unit_id;
    /** ACK signal */
    uint8_t ack_hi;

    /** Used to accumulate bytes to be serialized/unserliazed to bus */
    unsigned data_pulse_count;
    unsigned data_bit_count;

    /** Packet processor state */
    unsigned packet_state;
    unsigned packet_state_byte_cnt;
    unsigned packet_cntr;

    /** Raw encoded xfer buffer */
    unsigned data_size;
    uint8_t data[CLEM_SMARTPORT_DATA_BUFFER_LIMIT];

    /** Decoded packet */
    struct ClemensSmartPortPacket packet;
};

#ifdef __cplusplus
}
#endif

#endif
