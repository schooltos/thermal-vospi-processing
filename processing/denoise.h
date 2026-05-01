#ifndef PROCESSING_DENOISE_H
#define PROCESSING_DENOISE_H

#include <stdint.h>
#include "core/types.h"
#include "core/config.h"

/*
 * Контекст часового шумопригнічення.
 * prev_frame зберігає попередній згладжений результат.
 */
typedef struct {
    uint16_t prev_frame[FRAME_PIXEL_COUNT];
    uint8_t initialized;
} DenoiseContext;

/*
 * Ініціалізація контексту.
 */
void denoise_init(DenoiseContext *ctx);

/*
 * Скидання контексту.
 */
void denoise_reset(DenoiseContext *ctx);

/*
 * Temporal IIR filter:
 * out = alpha * current + (1 - alpha) * previous
 *
 * alpha має бути в межах (0; 1].
 * Менше alpha -> сильніше згладжування.
 * Більше alpha -> швидша реакція на зміни.
 */
void denoise_iir_apply(ThermalFrame *frame,
                       DenoiseContext *ctx,
                       float alpha);

void denoise_median3x3_apply(ThermalFrame *frame);

#endif /* PROCESSING_DENOISE_H */