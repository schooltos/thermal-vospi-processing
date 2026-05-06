#ifndef CORE_TYPES_H
#define CORE_TYPES_H

#include <stdint.h>
#include "config.h"

/*
 * VoSPI-пакет.
 * Для спрощення зберігаємо заголовок у вигляді 2 полів по 16 біт:
 * packet_id і crc, а корисні дані — окремо.
 */
typedef struct {
    uint8_t header[VOSPI_HEADER_SIZE];
    uint8_t payload[VOSPI_PAYLOAD_SIZE];
} VoSPIPacket;

/*
 * Сирий тепловізійний кадр.
 * Зберігаємо у 16-бітному вигляді, щоб не втрачати динамічний діапазон
 * до етапу нормалізації.
 */
typedef struct {
    uint16_t pixels[FRAME_PIXEL_COUNT];
    uint8_t valid;
    uint32_t frame_number;
} ThermalFrame;

/*
 * Кадр для відображення після нормалізації.
 * Це вже 8-бітне представлення.
 */
typedef struct {
    uint8_t pixels[FRAME_PIXEL_COUNT];
    uint8_t valid;
    uint32_t frame_number;
} DisplayFrame;

#endif /* CORE_TYPES_H */