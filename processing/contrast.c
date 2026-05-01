#include "contrast.h"

#include <string.h>

void contrast_hist_eq(DisplayFrame *frame)
{
    uint32_t hist[256];
    uint32_t cdf[256];
    uint32_t i;
    uint32_t cumulative;
    uint8_t lut[256];
    uint32_t cdf_min;
    uint8_t pixel;

    if (frame == NULL) {
        return;
    }

    memset(hist, 0, sizeof(hist));
    memset(cdf, 0, sizeof(cdf));
    memset(lut, 0, sizeof(lut));

    /* 1. Будуємо гістограму */
    for (i = 0; i < FRAME_PIXEL_COUNT; i++) {
        hist[frame->pixels[i]]++;
    }

    /* 2. Обчислюємо кумулятивну гістограму */
    cumulative = 0;
    for (i = 0; i < 256U; i++) {
        cumulative += hist[i];
        cdf[i] = cumulative;
    }

    /*
     * 3. Шукаємо перше ненульове значення CDF,
     * щоб уникнути штучного зсуву в дуже темних кадрах.
     */
    cdf_min = 0;
    for (i = 0; i < 256U; i++) {
        if (cdf[i] != 0U) {
            cdf_min = cdf[i];
            break;
        }
    }

    /*
     * Якщо кадр порожній або всі значення однакові,
     * нічого не змінюємо.
     */
    if ((cdf_min == 0U) || (cdf_min == FRAME_PIXEL_COUNT)) {
        return;
    }

    /* 4. Формуємо LUT */
    for (i = 0; i < 256U; i++) {
        if (cdf[i] < cdf_min) {
            lut[i] = 0U;
        } else {
            lut[i] = (uint8_t)(((cdf[i] - cdf_min) * 255U) /
                               (FRAME_PIXEL_COUNT - cdf_min));
        }
    }

    /* 5. Застосовуємо LUT до кадру */
    for (i = 0; i < FRAME_PIXEL_COUNT; i++) {
        pixel = frame->pixels[i];
        frame->pixels[i] = lut[pixel];
    }
}