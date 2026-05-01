#include "source_emulator.h"

#include <stdio.h>
#include <string.h>

static FILE *g_emulator_file = NULL;

/*
 * Формат одного пакета у файлі:
 *
 * [0..1]   packet_id  (uint16_t, little-endian)
 * [2..3]   crc        (uint16_t, little-endian)
 * [4..163] payload    (160 bytes)
 *
 * Усього: 164 байти на пакет
 */

static int read_u16_le(FILE *fp, uint16_t *value)
{
    unsigned char bytes[2];

    if ((fp == NULL) || (value == NULL)) {
        return -1;
    }

    if (fread(bytes, 1, 2, fp) != 2) {
        return 1; /* EOF або помилка читання */
    }

    *value = (uint16_t)((uint16_t)bytes[0] |
                        ((uint16_t)bytes[1] << 8U));

    return 0;
}

int source_emulator_init(const char *path)
{
    if (path == NULL) {
        return -1;
    }

    if (g_emulator_file != NULL) {
        fclose(g_emulator_file);
        g_emulator_file = NULL;
    }

    g_emulator_file = fopen(path, "rb");
    if (g_emulator_file == NULL) {
        return -1;
    }

    return 0;
}

int source_emulator_read_packet(VoSPIPacket *packet)
{
    int status;

    if ((g_emulator_file == NULL) || (packet == NULL)) {
        return -1;
    }

    memset(packet, 0, sizeof(*packet));

    status = read_u16_le(g_emulator_file, &packet->packet_id);
    if (status != 0) {
        return status;
    }

    status = read_u16_le(g_emulator_file, &packet->crc);
    if (status != 0) {
        return status;
    }

    if (fread(packet->payload, 1, VOSPI_PAYLOAD_SIZE, g_emulator_file) != VOSPI_PAYLOAD_SIZE) {
        if (feof(g_emulator_file)) {
            return 1;
        }
        return -1;
    }

    return 0;
}

int source_emulator_reset(void)
{
    if (g_emulator_file == NULL) {
        return -1;
    }

    if (fseek(g_emulator_file, 0, SEEK_SET) != 0) {
        return -1;
    }

    return 0;
}

void source_emulator_deinit(void)
{
    if (g_emulator_file != NULL) {
        fclose(g_emulator_file);
        g_emulator_file = NULL;
    }
}