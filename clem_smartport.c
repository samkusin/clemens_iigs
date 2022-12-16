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
#define CLEM_SMARTPORT_UNIT_STATE_RESPONSE        0x00040000
#define CLEM_SMARTPORT_UNIT_STATE_COMPLETE        0x00050000

#define CLEM_SMARTPORT_BUS_WRITE 1
#define CLEM_SMARTPORT_BUS_READ  2
#define CLEM_SMARTPORT_BUS_DATA  4
#define CLEM_SMARTPORT_BUS_REQ   8

static inline void _clem_smartport_packet_state(struct ClemensSmartPortUnit *unit,
                                                unsigned packet_state) {
    unit->packet_state = packet_state;
    unit->packet_state_byte_cnt = 0;
}

static inline uint8_t _clem_smartport_encode_byte(uint8_t data, uint8_t *chksum) {
    data |= 0x80;
    *chksum ^= data;
    return data;
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

static unsigned _clem_smartport_packet_encode_data(uint8_t *out, unsigned out_limit,
                                                   struct ClemensSmartPortPacket *packet,
                                                   uint8_t unit_id) {
    unsigned out_size = 0;
    unsigned index;
    uint8_t chksum = 0x00;
    uint8_t odd_cnt = (uint8_t)(packet->contents_length % 7);
    uint8_t g7_cnt = (uint8_t)(packet->contents_length / 7);
    uint8_t tmp;
    uint16_t raw_contents_size;

    uint8_t *packet_src;

    //  sync bytes
    while (out_size < out_limit && out_size < 6) {
        out[out_size++] = 0xFF;
    }
    if (out_size + 8 <= out_limit) {
        //  NOTE: checksum is performed on the encoded bytes (high bit on)
        //        which differs from how the checksum is calculated for the
        //        contents (see below.)
        out[out_size++] = 0xC3;
        out[out_size++] = _clem_smartport_encode_byte(0x00, &chksum);
        out[out_size++] = _clem_smartport_encode_byte(unit_id, &chksum);
        if (packet->type == kClemensSmartPortPacketType_Command) {
            tmp = 0x00;
        } else if (packet->type == kClemensSmartPortPacketType_Status) {
            tmp = 0x01;
        } else if (packet->type == kClemensSmartPortPacketType_Data) {
            tmp = 0x02;
        } else {
            tmp = 0x7F;
        }
        out[out_size++] = _clem_smartport_encode_byte(tmp, &chksum);
        if (packet->is_extended) {
            out[out_size++] = _clem_smartport_encode_byte(0x40, &chksum);
        } else {
            out[out_size++] = _clem_smartport_encode_byte(0x00, &chksum);
        }
        out[out_size++] = _clem_smartport_encode_byte(packet->status, &chksum);
        out[out_size++] = _clem_smartport_encode_byte(odd_cnt, &chksum);
        out[out_size++] = _clem_smartport_encode_byte(g7_cnt, &chksum);
    }
    raw_contents_size = odd_cnt > 0 ? odd_cnt + 1 : 0;
    raw_contents_size += (g7_cnt * 8);
    packet_src = packet->contents;
    if (out_size + raw_contents_size < out_limit) {
        //  NOTE: checksums only on the decoded contents, so perform the checksum
        //        calculation on the source bytes
        tmp = 0;
        for (index = 0; index < (unsigned)odd_cnt; ++index) {
            if (packet_src[index] & 0x80) {
                tmp |= 1;
            }
            tmp <<= 1;
        }
        //  if odd_cnt == 6 (the maximum), bit 0 will be 0 at this point, so
        //  the shift calculation below should work.
        tmp <<= (6 - odd_cnt);
        if (odd_cnt > 0) {
            out[out_size++] = 0x80 | tmp;
            while (odd_cnt > 0) {
                out[out_size++] = 0x80 | packet_src[0];
                chksum ^= packet_src[0];
                ++packet_src;
                --odd_cnt;
            }
        }
        while (g7_cnt > 0) {
            tmp = (packet_src[0] & 0x80) >> 1;
            tmp |= ((packet_src[1] & 0x80) >> 2);
            tmp |= ((packet_src[2] & 0x80) >> 3);
            tmp |= ((packet_src[3] & 0x80) >> 4);
            tmp |= ((packet_src[4] & 0x80) >> 5);
            tmp |= ((packet_src[5] & 0x80) >> 6);
            tmp |= ((packet_src[6] & 0x80) >> 7);
            out[out_size++] = 0x80 | tmp;
            out[out_size++] = 0x80 | packet_src[0];
            chksum ^= packet_src[0];
            out[out_size++] = 0x80 | packet_src[1];
            chksum ^= packet_src[1];
            out[out_size++] = 0x80 | packet_src[2];
            chksum ^= packet_src[2];
            out[out_size++] = 0x80 | packet_src[3];
            chksum ^= packet_src[3];
            out[out_size++] = 0x80 | packet_src[4];
            chksum ^= packet_src[4];
            out[out_size++] = 0x80 | packet_src[5];
            chksum ^= packet_src[5];
            out[out_size++] = 0x80 | packet_src[6];
            chksum ^= packet_src[6];
            --g7_cnt;
        }
    }
    if (out_size + 3 <= out_limit) {
        tmp = 0x80 | (chksum & 0x40) | 0x20 | (chksum & 0x10) | 0x08 | (chksum & 0x04) | 0x02 |
              (chksum & 0x1);
        out[out_size++] = tmp;
        tmp = (chksum & 0x80) | 0x40 | (chksum & 0x20) | 0x10 | (chksum & 0x08) | 0x04 |
              (chksum & 0x02);
        out[out_size++] = 0x80 | (tmp >> 1);
        out[out_size++] = 0xC8;
    }

    return out_size;
}

static bool _clem_smartport_handle_packet(struct ClemensSmartPortUnit *unit, unsigned delta_ns) {
    //  Parse the decoded packet based on the packet type (For commands, obtain the command type
    //  and parameter count, for example)
    if (unit->packet.type == kClemensSmartPortPacketType_Command) {
        uint8_t command_type = unit->packet.contents[0];
        switch (command_type) {
        case CLEM_SMARTPORT_COMMAND_INIT:
            CLEM_DEBUG("SmartPort: [%02X] INIT", unit->unit_id);
            unit->unit_id = unit->packet.dest_unit_id;
            unit->ph3_latch_lo = false;
            //  generate response
            unit->packet.status = 0x00;
            unit->packet.dest_unit_id = unit->packet.source_unit_id;
            unit->packet.source_unit_id = unit->unit_id;
            return true;
        default:
            CLEM_ASSERT(false);
            break;
        }
    }
    return false;
}

static unsigned _clem_smartport_bus_handshake(struct ClemensSmartPortUnit *unit, unsigned bus_state,
                                              unsigned delta_ns) {
    uint8_t *data_tail;
    uint8_t *data_start;

    if (!(bus_state & CLEM_SMARTPORT_BUS_REQ)) {
        if (unit->packet_state == CLEM_SMARTPORT_UNIT_STATE_EXECUTING) {
            //  must wait until the host has marked the request as 'done' before acknowledging
            //  packet's exectuion has completed.
            if (_clem_smartport_handle_packet(unit, delta_ns)) {
                _clem_smartport_packet_state(unit, CLEM_SMARTPORT_UNIT_STATE_RESPONSE);
                unit->data_size = _clem_smartport_packet_encode_data(
                    unit->data, CLEM_SMARTPORT_DATA_BUFFER_LIMIT, &unit->packet, unit->unit_id);
                unit->ack_hi = true;
                //  setup IWM 'read from drive'
                unit->data_pulse_count = 0;
                unit->data_bit_count = 0;
            }
        } else if (unit->packet_state == CLEM_SMARTPORT_UNIT_STATE_COMPLETE) {
            _clem_smartport_packet_state(unit, CLEM_SMARTPORT_UNIT_STATE_READY);
            unit->ack_hi = true;
        }
        return bus_state;
    }

    if (bus_state & CLEM_SMARTPORT_BUS_WRITE) {
        //  write into data buffer from the disk port
        bool data_signal = (bus_state & CLEM_SMARTPORT_BUS_DATA) != 0;
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
                                 unit->device.device_id);
                    }
                    unit->data_reg = 0;
                    unit->data_bit_count = 0;
                } else {
                    unit->data_reg <<= 1;
                }
            }
        }
    } else if (unit->packet_state == CLEM_SMARTPORT_UNIT_STATE_RESPONSE) {
        bus_state &= ~CLEM_SMARTPORT_BUS_DATA;
        unit->data_pulse_count++;
        if ((unit->data_pulse_count % 8) == 0) {
            bus_state |= CLEM_SMARTPORT_BUS_READ;
            if (unit->data_bit_count == 0) {
                if (unit->packet_state_byte_cnt >= unit->data_size) {
                    _clem_smartport_packet_state(unit, CLEM_SMARTPORT_UNIT_STATE_COMPLETE);
                    return bus_state;
                } else {
                    unit->data_reg = unit->data[unit->packet_state_byte_cnt++];
                    printf("SmartPort <= %02X\n", unit->data_reg);
                    unit->data_bit_count = 8;
                }
            }
            --unit->data_bit_count;
            if (unit->data_reg & 0x80) {
                bus_state |= CLEM_SMARTPORT_BUS_DATA;
            }
            unit->data_reg <<= 1;
        }
        return bus_state;
    } else if (unit->packet_state == CLEM_SMARTPORT_UNIT_STATE_COMPLETE) {
        unit->ack_hi = false;
    }

    if (unit->packet_state_byte_cnt < 1)
        return bus_state;

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
            // store off the actual length of the packet in bytes vs its group/odd count
            unit->packet.contents_length =
                (unit->packet.contents_length >> 8) * 7 + (unit->packet.contents_length & 0xff);
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
            //  INIT commands are ignored if our device was already set by
            //  an earlier INIT and the unit wasn't reset
            if (unit->unit_id && unit->packet.dest_unit_id != unit->unit_id) {
                _clem_smartport_packet_state(unit, CLEM_SMARTPORT_UNIT_STATE_READY);
            } else {
                _clem_smartport_packet_state(unit, CLEM_SMARTPORT_UNIT_STATE_EXECUTING);
                unit->ack_hi = false;
            }
        } else {
            _clem_smartport_packet_state(unit, CLEM_SMARTPORT_UNIT_STATE_PACKET_BAD);
        }
        break;
    }

    return bus_state;
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
        if (unit->device.device_id == 0)
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
            bus_state = _clem_smartport_bus_handshake(unit, bus_state, delta_ns);
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

        *io_flags &= ~CLEM_IWM_FLAG_READ_DATA;
        if (bus_state & CLEM_SMARTPORT_BUS_READ) {
            *io_flags |= CLEM_IWM_FLAG_PULSE_HIGH;
            if (bus_state & CLEM_SMARTPORT_BUS_DATA) {
                *io_flags |= CLEM_IWM_FLAG_READ_DATA;
            }
        } else {
            *io_flags &= ~CLEM_IWM_FLAG_PULSE_HIGH;
        }

        //    printf("SmartPort: REQ: %u,  ACK: %u\n", (select_bits & 1) != 0, is_ack_hi);
    }

    return is_bus_enabled;
}
