#ifndef CLEM_SMARTPORT_H
#define CLEM_SMARTPORT_H

#include "clem_shared.h"

/** Number of drives supported at one time on the system */
#define CLEM_SMARTPORT_DRIVE_LIMIT 1

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

/* SmartPort Block Device Commands */
#define CLEM_SMARTPORT_COMMAND_STATUS     0x00
#define CLEM_SMARTPORT_COMMAND_READBLOCK  0x01
#define CLEM_SMARTPORT_COMMAND_WRITEBLOCK 0x02
#define CLEM_SMARTPORT_COMMAND_FORMAT     0x03
#define CLEM_SMARTPORT_COMMAND_CONTROL    0x04
#define CLEM_SMARTPORT_COMMAND_INIT       0x05

/** Status codes returned from SmartPort calls - 0x7f is reserved for async wait
 *  by the SmartPort emulator for device implementations to return that the operation
 *  is imcomplete and to keep polling until its finished with one of the other
 *  result codes.
 */
#define CLEM_SMARTPORT_STATUS_CODE_OK            0x00
#define CLEM_SMARTPORT_STATUS_CODE_BAD_CMD       0x01
#define CLEM_SMARTPORT_STATUS_CODE_BUS_ERR       0x06
#define CLEM_SMARTPORT_STATUS_CODE_BAD_CTL       0x21
#define CLEM_SMARTPORT_STATUS_CODE_IO_ERR        0x27
#define CLEM_SMARTPORT_STATUS_CODE_INVALID_BLOCK 0x2D
#define CLEM_SMARTPORT_STATUS_CODE_OFFLINE       0x2f
#define CLEM_SMARTPORT_STATUS_CODE_WAIT          0x7f

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
    /** Indicates how the packet should be handled by the recipient */
    enum ClemensSmartPortPacketType type;
    /** Where the packet originates from (0 = host, > 0 are bus residents) */
    uint8_t source_unit_id;
    /** Where the packet is going (0 = host, > 0 are bus residents)*/
    uint8_t dest_unit_id;
    /** An extended SmartPort call (certain fields are extended) */
    uint8_t is_extended;
    /** Value is used by status and data packet types, and error code for commands */
    uint8_t status;
    /** Contents length */
    uint16_t contents_length;
    /*  Decoded contents (8-bit values) */
    uint8_t contents[CLEM_SMARTPORT_CONTENTS_LIMIT];
};

struct ClemensSmartPortDevice {
    unsigned device_id;
    void *device_data;

    /** Smartport bus resident on a bus being reset */
    uint8_t (*do_reset)(struct ClemensSmartPortDevice *context, unsigned delta_ns);
    /** Read a block from the device and store into packet when response state is returned*/
    uint8_t (*do_read_block)(struct ClemensSmartPortDevice *context,
                             struct ClemensSmartPortPacket *packet, unsigned block_index,
                             unsigned delta_ns);
    /** Write a block to the device and store into packet when response state is returned */
    uint8_t (*do_write_block)(struct ClemensSmartPortDevice *context,
                              struct ClemensSmartPortPacket *packet, unsigned block_index,
                              unsigned delta_ns);
    /** Retrieve a status block from the device */
    uint8_t (*do_status)(struct ClemensSmartPortDevice *context,
                         struct ClemensSmartPortPacket *packet, unsigned delta_ns);
    /** Retrieve a status block from the device */
    uint8_t (*do_format)(struct ClemensSmartPortDevice *context,
                         struct ClemensSmartPortPacket *packet, unsigned delta_ns);
    /** Retrieve a status block from the device */
    uint8_t (*do_control)(struct ClemensSmartPortDevice *context,
                          struct ClemensSmartPortPacket *packet, unsigned delta_ns);
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
    struct ClemensSmartPortDevice device;

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
    /** Active command for multi packet commands (i.e. WriteBlock)*/
    uint8_t command_id;

    /** Used to accumulate bytes to be serialized/unserliazed to bus */
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

    /** Debug only */
    int enable_debug;
    int debug_level;
    clem_clocks_time_t debug_ts;
};

#ifdef __cplusplus
}
#endif

#endif
