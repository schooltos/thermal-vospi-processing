#include "normalize.h"

#include <string.h>

static uint16_t find_min_value(const ThermalFrame *src)
{
    uint32_t i;
    uint16_t min_val;

    min_val = src->pixels[0];

    for (i = 1; i < FRAME_PIXEL_COUNT; i++) {
        if (src->pixels[i] < min_val) {
            min_val = src->pixels[i];
        }
    }

    return min_val;
}

static uint16_t find_max_value(const ThermalFrame *src)
{
    uint32_t i;
    uint16_t max_val;

    max_val = src->pixels[0];

    for (i = 1; i < FRAME_PIXEL_COUNT; i++) {
        if (src->pixels[i] > max_val) {
            max_val = src->pixels[i];
        }
    }

    return max_val;
}

static uint16_t clamp_u16(uint16_t value, uint16_t low, uint16_t high)
{
    if (value < low) {
        return low;
    }

    if (value > high) {
        return high;
    }

    return value;
}

static uint8_t scale_to_u8(uint16_t value, uint16_t low, uint16_t high)
{
    uint32_t scaled;

    if (high <= low) {
        return 0U;
    }

    scaled = ((uint32_t)(value - low) * 255U) / (uint32_t)(high - low);

    if (scaled > 255U) {
        scaled = 255U;
    }

    return (uint8_t)scaled;
}

/*
 * Обчислює гістограму для 16-бітних значень.
 * Для простоти беремо повний діапазон 0..65535.
 * Так, це 65536 бінів, але для ПК-версії і тестового етапу це нормально.
 * Для ESP32 потім можна перейти на грубіше бінування.
 */
static void build_histogram(const ThermalFrame *src, uint32_t *hist)
{
    uint32_t i;

    memset(hist, 0, 65536U * sizeof(uint32_t));

    for (i = 0; i < FRAME_PIXEL_COUNT; i++) {
        hist[src->pixels[i]]++;
    }
}

static uint16_t percentile_value_from_histogram(const uint32_t *hist,
                                                uint16_t percentile)
{
    uint32_t target_count;
    uint32_t cumulative = 0;
    uint32_t i;

    if (percentile > 100U) {
        percentile = 100U;
    }

    /*
     * Наприклад, для 2% шукаємо таку точку,
     * де накопичено 2% від кількості пікселів.
     */
    target_count = ((uint32_t)percentile * FRAME_PIXEL_COUNT) / 100U;

    for (i = 0; i < 65536U; i++) {
        cumulative += hist[i];
        if (cumulative >= target_count) {
            return (uint16_t)i;
        }
    }

    return 65535U;
}

void normalize_minmax_to_display(const ThermalFrame *src,
                                 DisplayFrame *dst)
{
    uint32_t i;
    uint16_t min_val;
    uint16_t max_val;

    if ((src == NULL) || (dst == NULL)) {
        return;
    }

    min_val = find_min_value(src);
    max_val = find_max_value(src);

    /*
     * Якщо кадр рівномірний, заповнюємо нулями.
     */
    if (max_val <= min_val) {
        memset(dst->pixels, 0, sizeof(dst->pixels));
        dst->valid = src->valid;
        dst->frame_number = src->frame_number;
        return;
    }

    for (i = 0; i < FRAME_PIXEL_COUNT; i++) {
        dst->pixels[i] = scale_to_u8(src->pixels[i], min_val, max_val);
    }

    dst->valid = src->valid;
    dst->frame_number = src->frame_number;
}

void normalize_percentile_to_display(const ThermalFrame *src,
                                     DisplayFrame *dst,
                                     uint16_t low_percentile,
                                     uint16_t high_percentile)
{
    uint32_t i;
    uint16_t low_val;
    uint16_t high_val;
    uint16_t clamped;
    uint32_t hist[65536];

    if ((src == NULL) || (dst == NULL)) {
        return;
    }

    if (low_percentile > 100U) {
        low_percentile = 100U;
    }

    if (high_percentile > 100U) {
        high_percentile = 100U;
    }

    if (low_percentile >= high_percentile) {
        normalize_minmax_to_display(src, dst);
        return;
    }

    build_histogram(src, hist);

    low_val = percentile_value_from_histogram(hist, low_percentile);
    high_val = percentile_value_from_histogram(hist, high_percentile);

    if (high_val <= low_val) {
        normalize_minmax_to_display(src, dst);
        return;
    }

    for (i = 0; i < FRAME_PIXEL_COUNT; i++) {
        clamped = clamp_u16(src->pixels[i], low_val, high_val);
        dst->pixels[i] = scale_to_u8(clamped, low_val, high_val);
    }

    dst->valid = src->valid;
    dst->frame_number = src->frame_number;
}