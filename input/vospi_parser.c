#include "vospi_parser.h"
#include <stddef.h>
#include "core/config.h"

/*
 * Simplified VoSPI-like header used by emulator:
 *
 * header[0] bits 7..4 : flags
 * header[0] bits 3..0 : segment 0..3
 * header[1]           : packet number 0..59
 * header[2..3]        : reserved / optional CRC field, currently unused
 *
 * flags == 0xF -> discard packet
 */

#define VOSPI_FLAG_DISCARD 0x0FU

VoSPIPacketInfo vospi_parse_packet(const VoSPIPacket *packet)
{
    VoSPIPacketInfo info;
    uint8_t flags;

    info.valid = 0U;
    info.discard = 0U;
    info.segment = 0U;
    info.packet_number = 0U;

    if (packet == NULL) {
        return info;
    }

    flags = (uint8_t)((packet->header[0] >> 4U) & 0x0FU);

    if (flags == VOSPI_FLAG_DISCARD) {
        info.valid = 1U;
        info.discard = 1U;
        return info;
    }

    info.segment = (uint8_t)(packet->header[0] & 0x0FU);
    info.packet_number = packet->header[1];

    if (info.segment >= VOSPI_SEGMENT_COUNT) {
        return info;
    }

    if (info.packet_number >= VOSPI_PACKETS_PER_SEGMENT) {
        return info;
    }

    info.valid = 1U;
    return info;
}