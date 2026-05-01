#ifndef CORE_FRAME_H
#define CORE_FRAME_H

#include <stdint.h>
#include "types.h"

/*
 * Повертає лінійний індекс для координат (x, y).
 * Якщо координати виходять за межі кадру, повертає -1.
 */
int frame_index(uint16_t x, uint16_t y);

/* Очищення теплового кадру */
void thermal_frame_clear(ThermalFrame *frame);

/* Заповнення теплового кадру одним значенням */
void thermal_frame_fill(ThermalFrame *frame, uint16_t value);

/* Копіювання теплового кадру */
void thermal_frame_copy(ThermalFrame *dst, const ThermalFrame *src);

/* Отримання значення пікселя */
uint16_t thermal_frame_get_pixel(const ThermalFrame *frame, uint16_t x, uint16_t y);

/* Встановлення значення пікселя */
void thermal_frame_set_pixel(ThermalFrame *frame, uint16_t x, uint16_t y, uint16_t value);

/* Очищення display-кадру */
void display_frame_clear(DisplayFrame *frame);

/* Заповнення display-кадру одним значенням */
void display_frame_fill(DisplayFrame *frame, uint8_t value);

/* Копіювання display-кадру */
void display_frame_copy(DisplayFrame *dst, const DisplayFrame *src);

/* Отримання 8-бітного пікселя */
uint8_t display_frame_get_pixel(const DisplayFrame *frame, uint16_t x, uint16_t y);

/* Встановлення 8-бітного пікселя */
void display_frame_set_pixel(DisplayFrame *frame, uint16_t x, uint16_t y, uint8_t value);

#endif /* CORE_FRAME_H */