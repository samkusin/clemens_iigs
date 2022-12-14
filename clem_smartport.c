#include "clem_smartport.h"
#include "clem_debug.h"
#include "clem_defs.h"

#include <stdbool.h>
#include <stdio.h>
#include <string.h>

/* PH0 + PH2 ONLY */
#define CLEM_SMARTPORT_BUS_RESET_PHASE (1 + 4)
/* PH1 + PH3 */
#define CLEM_SMARTPORT_BUS_ENABLE_PHASE (2 + 8)

#define CLEM_SMARTPORT_UNIT_STATE_NULL  0x00000000
#define CLEM_SMARTPORT_UNIT_STATE_READY 0x00010000

/* sync bytes up to the packet begin byte*/
#define CLEM_SMARTPORT_UNIT_STATE_PACKET          0x00020000
#define CLEM_SMARTPORT_UNIT_STATE_PACKET_HEADER   0x00020001
#define CLEM_SMARTPORT_UNIT_STATE_PACKET_CONTENTS 0x00020002
#define CLEM_SMARTPORT_UNIT_STATE_PACKET_CHECKSUM 0x00020003
#define CLEM_SMARTPORT_UNIT_STATE_PACKET_END      0x00020004
#define CLEM_SMARTPORT_UNIT_STATE_PACKET_BAD      0x0002ffff
#define CLEM_SMARTPORT_UNIT_STATE_EXECUTING       0x00030000
#define CLEM_SMARTPORT_UNIT_STATE_ACK             0x00040000

#define CLEM_SMARTPORT_BUS_WRITE 1
#define CLEM_SMARTPORT_BUS_DATA  2
#define CLEM_SMARTPORT_BUS_REQ   4

static inline void _clem_smartport_packet_state(struct ClemensSmartPortUnit *unit,
                                                unsigned packet_state) {
    unit->packet_state = packet_state;
    unit->packet_state_byte_cnt = 0;
}

static void _clem_smartport_packet_decode_data(uint8_t *dest, unsigned dest_size,
                                               const uint8_t *src, unsigned src_groups,
                                               unsigned src_odd) {
    // the input buffer contains encoded chunks of data:
    //      every chunk contains the MSB of each 7-bit encoded byte in the group
    const uint8_t *src_groups_data = src + 1;
    uint8_t msbs = src[0] << 1;
    uint8_t *dest_data = dest;
    while (src_groups_data < (src + src_odd + 1) && dest_data <= (dest + dest_size)) {
        dest_data[0] = (*src_groups_data & 0x7f) | (msbs & 0x80);
        msbs <<= 1;
        ++src_groups_data;
        ++dest_data;
    }
    while (dest_data <= (dest + dest_size - 7)) {
        msbs = src_groups_data[0] << 1;
        dest_data[0] = (src_groups_data[1] & 0x7f) | (msbs & 0x80);
        msbs <<= 1;
        dest_data[1] = (src_groups_data[2] & 0x7f) | (msbs & 0x80);
        msbs <<= 1;
        dest_data[2] = (src_groups_data[3] & 0x7f) | (msbs & 0x80);
        msbs <<= 1;
        dest_data[3] = (src_groups_data[4] & 0x7f) | (msbs & 0x80);
        msbs <<= 1;
        dest_data[4] = (src_groups_data[5] & 0x7f) | (msbs & 0x80);
        msbs <<= 1;
        dest_data[5] = (src_groups_data[6] & 0x7f) | (msbs & 0x80);
        msbs <<= 1;
        dest_data[6] = (src_groups_data[7] & 0x7f) | (msbs & 0x80);
        src_groups_data += 8;
        dest_data += 7;
    }
}

static bool _clem_smartport_handle_packet(struct ClemensSmartPortUnit *unit, unsigned delta_ns) {
    //  Parse the decoded packet based on the packet type (For commands, obtain the command type
    //  and parameter count, for example)
    if (unit->packet.type == kClemensSmartPortPacketType_Command) {
        uint8_t command_type = unit->packet.contents[0];
        switch (command_type) {
        case CLEM_SMARTPORT_COMMAND_INIT:
            unit->unit_id = unit->packet.dest_unit_id;
            unit->ph3_latch_lo = false;
            return true;
        default:
            CLEM_ASSERT(false);
            break;
        }
    }
    return false;
}

static void _clem_smartport_bus_handshake(struct ClemensSmartPortUnit *unit, unsigned bus_in,
                                          unsigned delta_ns) {
    uint8_t *data_tail;
    uint8_t *data_start;

    if (unit->packet_state == CLEM_SMARTPORT_UNIT_STATE_EXECUTING) {
        //  must wait until the host has marked the request as 'done' before acknowledging
        //  packet's exectuion has completed.
        if (_clem_smartport_handle_packet(unit, delta_ns)) {
            unit->packet_state = CLEM_SMARTPORT_UNIT_STATE_ACK;
        }
    } else if (unit->packet_state == CLEM_SMARTPORT_UNIT_STATE_ACK) {
        if (!(bus_in & CLEM_SMARTPORT_BUS_REQ)) {
            unit->ack_hi = true;
            unit->packet_state = CLEM_SMARTPORT_UNIT_STATE_READY;
        }
    }

    if (!(bus_in & CLEM_SMARTPORT_BUS_REQ))
        return;

    if (bus_in & CLEM_SMARTPORT_BUS_WRITE) {
        //  write into data buffer from the disk port
        bool data_signal = (bus_in & CLEM_SMARTPORT_BUS_DATA) != 0;
        if (data_signal != unit->write_signal) {
            unit->data_reg |= 1;
        } else {
            unit->data_reg &= ~1;
        }
        unit->data_pulse_count++;
        if ((unit->data_pulse_count % 8) == 0) {
            unit->write_signal = data_signal;
            if (unit->data_bit_count > 0 || (unit->data_reg & 0x01)) {
                unit->data_bit_count++;
                if (unit->data_bit_count >= 8) {
                    if (unit->data_size < CLEM_SMARTPORT_DATA_BUFFER_LIMIT) {
                        printf("SmartPort => %02X\n", unit->data_reg);
                        unit->data[unit->data_size++] = unit->data_reg;
                        unit->packet_state_byte_cnt++;
                    } else {
                        CLEM_LOG("SmartPort: Data overrun at unit %u, device %u", unit->unit_id,
                                 unit->device_id);
                    }
                    unit->data_reg = 0;
                    unit->data_bit_count = 0;
                } else {
                    unit->data_reg <<= 1;
                }
            }
        }
    }

    if (unit->packet_state_byte_cnt < 1)
        return;

    data_start = unit->data + unit->data_size - unit->packet_state_byte_cnt;
    data_tail = unit->data + unit->data_size;

    switch (unit->packet_state) {
    case CLEM_SMARTPORT_UNIT_STATE_READY:
        _clem_smartport_packet_state(unit, CLEM_SMARTPORT_UNIT_STATE_PACKET);
        break;
    case CLEM_SMARTPORT_UNIT_STATE_PACKET:
        if (data_tail[-1] != 0xff) {
            if (data_tail[-1] == 0xC3) {
                //  save off the start of the actual packet data
                _clem_smartport_packet_state(unit, CLEM_SMARTPORT_UNIT_STATE_PACKET_HEADER);
                memset(&unit->packet, 0, sizeof(unit->packet));
            } else {
                _clem_smartport_packet_state(unit, CLEM_SMARTPORT_UNIT_STATE_PACKET_BAD);
                CLEM_WARN("SmartPort: sync byte expected but got %02X", data_tail[-1]);
            }
        }
        break;
    case CLEM_SMARTPORT_UNIT_STATE_PACKET_HEADER:
        if (unit->packet_state_byte_cnt == 7) {
            //  the last two bytes of the header contain the packet length in unencoded bytes
            //  we must calculate the encoded length of 7-bit bytes to pull in from the packet.
            _clem_smartport_packet_state(unit, CLEM_SMARTPORT_UNIT_STATE_PACKET_CONTENTS);
            unit->packet.source_unit_id = data_start[1] & 0x7f;
            unit->packet.dest_unit_id = data_start[0] & 0x7f;
            if (data_start[2] == 0x80) {
                unit->packet.type = kClemensSmartPortPacketType_Command;
            } else if (data_start[2] = 0x81) {
                unit->packet.type = kClemensSmartPortPacketType_Status;
            } else if (data_start[2] == 0x82) {
                unit->packet.type = kClemensSmartPortPacketType_Data;
            }
            if (data_start[3] == 0xC0) {
                unit->packet.is_extended = true;
            }
            unit->packet.status = data_start[4] & 0x7f;
            //  get the decoded contents counts necessary to decode the odd and
            //  7 byte groups.
            unit->packet.contents_length = (data_tail[-1] & 0x7f) << 8;
            unit->packet.contents_length |= (data_tail[-2] & 0x7f);
            //  calculate encoded content length
            unit->packet_cntr = (((unsigned)(data_tail[-1]) & 0x7f) * 7) + (data_tail[-2] & 0x7f);
            unit->packet_cntr = (unit->packet_cntr * 8 + 6) / 7;
        }
        break;
    case CLEM_SMARTPORT_UNIT_STATE_PACKET_CONTENTS:
        if (unit->packet_state_byte_cnt == unit->packet_cntr) {
            _clem_smartport_packet_decode_data(unit->packet.contents, unit->packet_cntr, data_start,
                                               unit->packet.contents_length >> 8,
                                               unit->packet.contents_length & 0xff);
            _clem_smartport_packet_state(unit, CLEM_SMARTPORT_UNIT_STATE_PACKET_CHECKSUM);
        }
        break;
    case CLEM_SMARTPORT_UNIT_STATE_PACKET_CHECKSUM:
        if (unit->packet_state_byte_cnt == 2) {
            //  16-bit encoded checksum (8-bit actual)
            //  calc checksum up to (data_size - 2)
            _clem_smartport_packet_state(unit, CLEM_SMARTPORT_UNIT_STATE_PACKET_END);
        }
        break;
    case CLEM_SMARTPORT_UNIT_STATE_PACKET_END:
        if (data_tail[-1] == 0xC8) {
            _clem_smartport_packet_state(unit, CLEM_SMARTPORT_UNIT_STATE_EXECUTING);
            unit->ack_hi = false;
        } else {
            _clem_smartport_packet_state(unit, CLEM_SMARTPORT_UNIT_STATE_PACKET_BAD);
        }
        break;
    }
}

/*
   Bus Reset starts the unit assignment process from the host.

   Each device will force PH30 on its *output* phase, which is passed to the
   next device on the daisy-chained disk port.

   The host will reenable the bus and assign a unit ID to the next available
   device in the chain. See do_enable() for details on subsequent events.

   To achieve this, on do_reset(), each device maintains an 'available' flag
   which is TRUE once it receives its ID assignment.  When 'available', the
   device no longer forces PH3 to 0 on its output and the next device will
   pick up on the PH3 as set by the host.
*/

/*
  Bus Enable marks that all 'available' bus residents can handle SmartPort
  commands.  See do_reset() for details on how a device becomes available.

  Much of the implementation follows the SmartPort protocol diagrams in
  Chapter 7 of the Apple IIgs firmware reference.

  Each device may have its own command handler.  They share common bus
  negotation logic with the host, which is provided below.
*/

bool clem_smartport_bus(struct ClemensSmartPortUnit *unit, unsigned unit_count, unsigned *io_flags,
                        unsigned *out_phase, unsigned delta_ns) {
    struct ClemensSmartPortUnit *unit_end = unit + unit_count;
    unsigned select_bits = *out_phase;
    unsigned bus_state = 0;
    bool is_bus_enabled = false;
    bool is_ack_hi = false;

    for (; unit < unit_end; unit++) {
        if (unit->device_id == 0)
            continue;
        if (select_bits == CLEM_SMARTPORT_BUS_RESET_PHASE) {
            unit->unit_id = 0x00;
            unit->ph3_latch_lo = 1;
            unit->bus_enabled = false;
        } else if ((select_bits & CLEM_SMARTPORT_BUS_ENABLE_PHASE) ==
                   CLEM_SMARTPORT_BUS_ENABLE_PHASE) {
            if (!unit->bus_enabled) {
                unit->data_pulse_count = 0;
                unit->data_bit_count = 0;
                unit->data_reg = 0x00;
                unit->data_size = 0;
                unit->write_signal = 0;
                unit->ack_hi = true;
                unit->bus_enabled = true;
                _clem_smartport_packet_state(unit, CLEM_SMARTPORT_UNIT_STATE_READY);
            }
            if ((select_bits & 1) != 0) {
                bus_state |= CLEM_SMARTPORT_BUS_REQ;
            }
            if (*io_flags & CLEM_IWM_FLAG_WRITE_REQUEST) {
                bus_state |= CLEM_SMARTPORT_BUS_WRITE;
                if (*io_flags & CLEM_IWM_FLAG_WRITE_DATA) {
                    bus_state |= CLEM_SMARTPORT_BUS_DATA;
                }
            }
            _clem_smartport_bus_handshake(unit, bus_state, delta_ns);
        } else {
            unit->bus_enabled = false;
        }
        if (unit->ph3_latch_lo) {
            // PH3 OFF for downstream devices
            // This should only occur after a bus reset for a device that hasn't
            // received its ID from the hsot
            select_bits &= ~8;
        }
        is_bus_enabled = unit->bus_enabled;
        is_ack_hi = unit->ack_hi;
        unit++;
    }

    if (!is_bus_enabled) {
        is_bus_enabled =
            (select_bits & CLEM_SMARTPORT_BUS_ENABLE_PHASE) == CLEM_SMARTPORT_BUS_ENABLE_PHASE;
        is_ack_hi = is_bus_enabled;
    }

    if (is_bus_enabled) {
        if (is_ack_hi) {
            *io_flags |= CLEM_IWM_FLAG_WRPROTECT_SENSE;
        } else {
            *io_flags &= ~CLEM_IWM_FLAG_WRPROTECT_SENSE;
        }

        printf("SmartPort: REQ: %u,  ACK: %u\n", (select_bits & 1) != 0, is_ack_hi);
    }

    return unit->bus_enabled;
}
