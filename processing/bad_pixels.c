#include "bad_pixels.h"

#include <string.h>
#include "core/frame.h"

static uint16_t interpolate_bad_pixel(const ThermalFrame *frame,
                                      const BadPixelMap *map,
                                      uint16_t x,
                                      uint16_t y)
{
    int dx, dy;
    int nx, ny;
    int idx;
    uint32_t sum = 0;
    uint32_t count = 0;

    /*
     * Використовуємо 8-сусідство навколо дефектного пікселя.
     * У середнє включаємо тільки валідні сусідні пікселі,
     * які не позначені як дефектні.
     */
    for (dy = -1; dy <= 1; dy++) {
        for (dx = -1; dx <= 1; dx++) {
            if ((dx == 0) && (dy == 0)) {
                continue;
            }

            nx = (int)x + dx;
            ny = (int)y + dy;

            if ((nx < 0) || (ny < 0) ||
                (nx >= FRAME_WIDTH) || (ny >= FRAME_HEIGHT)) {
                continue;
            }

            if (bad_pixel_map_get(map, (uint16_t)nx, (uint16_t)ny)) {
                continue;
            }

            idx = frame_index((uint16_t)nx, (uint16_t)ny);
            if (idx < 0) {
                continue;
            }

            sum += frame->pixels[idx];
            count++;
        }
    }

    /*
     * Якщо не знайшли жодного коректного сусіда,
     * повертаємо початкове значення пікселя.
     */
    if (count == 0) {
        idx = frame_index(x, y);
        if (idx < 0) {
            return 0;
        }

        return frame->pixels[idx];
    }

    return (uint16_t)(sum / count);
}

void bad_pixel_map_clear(BadPixelMap *map)
{
    if (map == NULL) {
        return;
    }

    memset(map->mask, 0, sizeof(map->mask));
}

void bad_pixel_map_set(BadPixelMap *map,
                       uint16_t x,
                       uint16_t y,
                       uint8_t is_bad)
{
    int idx;

    if (map == NULL) {
        return;
    }

    idx = frame_index(x, y);
    if (idx < 0) {
        return;
    }

    map->mask[idx] = (is_bad != 0U) ? 1U : 0U;
}

uint8_t bad_pixel_map_get(const BadPixelMap *map,
                          uint16_t x,
                          uint16_t y)
{
    int idx;

    if (map == NULL) {
        return 0U;
    }

    idx = frame_index(x, y);
    if (idx < 0) {
        return 0U;
    }

    return map->mask[idx];
}

void bad_pixels_apply(ThermalFrame *frame,
                      const BadPixelMap *map)
{
    uint16_t corrected[FRAME_PIXEL_COUNT];
    uint32_t i;
    uint16_t x, y;
    int idx;

    if ((frame == NULL) || (map == NULL)) {
        return;
    }

    /*
     * Щоб уже виправлений піксель не впливав на сусідні
     * в межах того ж проходу, спочатку рахуємо всі значення
     * в окремий буфер.
     */
    memcpy(corrected, frame->pixels, sizeof(corrected));

    for (y = 0; y < FRAME_HEIGHT; y++) {
        for (x = 0; x < FRAME_WIDTH; x++) {
            if (!bad_pixel_map_get(map, x, y)) {
                continue;
            }

            idx = frame_index(x, y);
            if (idx < 0) {
                continue;
            }

            corrected[idx] = interpolate_bad_pixel(frame, map, x, y);
        }
    }

    for (i = 0; i < FRAME_PIXEL_COUNT; i++) {
        frame->pixels[i] = corrected[i];
    }
}