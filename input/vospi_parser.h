#ifndef INPUT_VOSPI_PARSER_H
#define INPUT_VOSPI_PARSER_H

#include <stdint.h>
#include "core/types.h"

typedef struct {
    uint8_t valid;
    uint8_t discard;
    uint8_t segment;
    uint8_t packet_number;
} VoSPIPacketInfo;

VoSPIPacketInfo vospi_parse_packet(const VoSPIPacket *packet);

#endif