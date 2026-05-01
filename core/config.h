#ifndef CORE_CONFIG_H
#define CORE_CONFIG_H

#include <stdint.h>

/* Розмір кадру Lepton 3.5 */
#define FRAME_WIDTH              160
#define FRAME_HEIGHT             120
#define FRAME_PIXEL_COUNT        (FRAME_WIDTH * FRAME_HEIGHT)

/*
 * Параметри VoSPI.
 * Для базової моделі беремо найуживаніший варіант:
 * 4 байти заголовка + 160 байт корисних даних.
 */
#define VOSPI_HEADER_SIZE        4
#define VOSPI_PAYLOAD_SIZE       160
#define VOSPI_PACKET_SIZE        (VOSPI_HEADER_SIZE + VOSPI_PAYLOAD_SIZE)

/* Lepton 3.x: 4 сегменти по 60 пакетів */
#define VOSPI_SEGMENT_COUNT      4
#define VOSPI_PACKETS_PER_SEGMENT 60
#define VOSPI_TOTAL_PACKETS      (VOSPI_SEGMENT_COUNT * VOSPI_PACKETS_PER_SEGMENT)

/* Діапазон для 8-бітного відображення */
#define DISPLAY_MIN_VALUE        0
#define DISPLAY_MAX_VALUE        255

#endif /* CORE_CONFIG_H */