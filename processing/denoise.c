#include "denoise.h"

#include <string.h>

static float clamp_alpha(float alpha)
{
    if (alpha <= 0.0f) {
        return 0.2f;
    }

    if (alpha > 1.0f) {
        return 1.0f;
    }

    return alpha;
}

void denoise_init(DenoiseContext *ctx)
{
    if (ctx == NULL) {
        return;
    }

    memset(ctx->prev_frame, 0, sizeof(ctx->prev_frame));
    ctx->initialized = 0U;
}

void denoise_reset(DenoiseContext *ctx)
{
    denoise_init(ctx);
}

void denoise_iir_apply(ThermalFrame *frame,
                       DenoiseContext *ctx,
                       float alpha)
{
    uint32_t i;
    float a;
    float inv_a;
    float filtered_value;

    if ((frame == NULL) || (ctx == NULL)) {
        return;
    }

    a = clamp_alpha(alpha);
    inv_a = 1.0f - a;

    /*
     * На першому кадрі немає попередньої історії,
     * тому просто ініціалізуємо буфер поточним кадром.
     */
    if (!ctx->initialized) {
        memcpy(ctx->prev_frame, frame->pixels, sizeof(ctx->prev_frame));
        ctx->initialized = 1U;
        return;
    }

    for (i = 0; i < FRAME_PIXEL_COUNT; i++) {
        filtered_value =
            a * (float)frame->pixels[i] +
            inv_a * (float)ctx->prev_frame[i];

        /*
         * Округлення до найближчого цілого.
         */
        frame->pixels[i] = (uint16_t)(filtered_value + 0.5f);
        ctx->prev_frame[i] = frame->pixels[i];
    }
}

static void sort9(uint16_t *v, uint8_t n)
{
    uint8_t i, j;
    uint16_t tmp;

    for (i = 0; i < n; i++) {
        for (j = (uint8_t)(i + 1); j < n; j++) {
            if (v[j] < v[i]) {
                tmp = v[i];
                v[i] = v[j];
                v[j] = tmp;
            }
        }
    }
}

void denoise_median3x3_apply(ThermalFrame *frame)
{
    uint16_t copy[FRAME_PIXEL_COUNT];
    uint16_t values[9];
    uint32_t x, y;
    int dx, dy;
    int nx, ny;
    uint8_t count;
    uint32_t idx;

    if (frame == NULL) {
        return;
    }

    memcpy(copy, frame->pixels, sizeof(copy));

    for (y = 0; y < FRAME_HEIGHT; y++) {
        for (x = 0; x < FRAME_WIDTH; x++) {
            count = 0;

            for (dy = -1; dy <= 1; dy++) {
                for (dx = -1; dx <= 1; dx++) {
                    nx = (int)x + dx;
                    ny = (int)y + dy;

                    if (nx < 0 || ny < 0 ||
                        nx >= FRAME_WIDTH || ny >= FRAME_HEIGHT) {
                        continue;
                    }

                    values[count++] = copy[(uint32_t)ny * FRAME_WIDTH + (uint32_t)nx];
                }
            }

            sort9(values, count);

            idx = y * FRAME_WIDTH + x;
            frame->pixels[idx] = values[count / 2];
        }
    }
}