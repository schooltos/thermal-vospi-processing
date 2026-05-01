#ifndef PROCESSING_BAD_PIXELS_H
#define PROCESSING_BAD_PIXELS_H

#include <stdint.h>
#include "core/types.h"
#include "core/config.h"

/*
 * Маска дефектних пікселів.
 * 0 - піксель нормальний
 * 1 - піксель дефектний
 */
typedef struct {
    uint8_t mask[FRAME_PIXEL_COUNT];
} BadPixelMap;

/*
 * Ініціалізація карти дефектів нулями.
 */
void bad_pixel_map_clear(BadPixelMap *map);

/*
 * Позначення одного пікселя як дефектного / нормального.
 * is_bad: 0 або 1
 */
void bad_pixel_map_set(BadPixelMap *map,
                       uint16_t x,
                       uint16_t y,
                       uint8_t is_bad);

/*
 * Перевірка, чи є піксель дефектним.
 * Повертає 1, якщо дефектний, і 0 в іншому випадку.
 */
uint8_t bad_pixel_map_get(const BadPixelMap *map,
                          uint16_t x,
                          uint16_t y);

/*
 * Застосування компенсації дефектних пікселів до кадру.
 * Для кожного дефектного пікселя значення замінюється
 * на середнє по сусідніх коректних пікселях.
 */
void bad_pixels_apply(ThermalFrame *frame,
                      const BadPixelMap *map);

#endif /* PROCESSING_BAD_PIXELS_H */