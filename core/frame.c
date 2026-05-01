#include "frame.h"

#include <string.h>

int frame_index(uint16_t x, uint16_t y)
{
    if ((x >= FRAME_WIDTH) || (y >= FRAME_HEIGHT)) {
        return -1;
    }

    return (int)(y * FRAME_WIDTH + x);
}

void thermal_frame_clear(ThermalFrame *frame)
{
    if (frame == NULL) {
        return;
    }

    memset(frame->pixels, 0, sizeof(frame->pixels));
    frame->valid = 0;
    frame->frame_number = 0;
}

void thermal_frame_fill(ThermalFrame *frame, uint16_t value)
{
    uint32_t i;

    if (frame == NULL) {
        return;
    }

    for (i = 0; i < FRAME_PIXEL_COUNT; i++) {
        frame->pixels[i] = value;
    }

    frame->valid = 1;
}

void thermal_frame_copy(ThermalFrame *dst, const ThermalFrame *src)
{
    if ((dst == NULL) || (src == NULL)) {
        return;
    }

    memcpy(dst, src, sizeof(ThermalFrame));
}

uint16_t thermal_frame_get_pixel(const ThermalFrame *frame, uint16_t x, uint16_t y)
{
    int idx;

    if (frame == NULL) {
        return 0;
    }

    idx = frame_index(x, y);
    if (idx < 0) {
        return 0;
    }

    return frame->pixels[idx];
}

void thermal_frame_set_pixel(ThermalFrame *frame, uint16_t x, uint16_t y, uint16_t value)
{
    int idx;

    if (frame == NULL) {
        return;
    }

    idx = frame_index(x, y);
    if (idx < 0) {
        return;
    }

    frame->pixels[idx] = value;
}

void display_frame_clear(DisplayFrame *frame)
{
    if (frame == NULL) {
        return;
    }

    memset(frame->pixels, 0, sizeof(frame->pixels));
    frame->valid = 0;
    frame->frame_number = 0;
}

void display_frame_fill(DisplayFrame *frame, uint8_t value)
{
    uint32_t i;

    if (frame == NULL) {
        return;
    }

    for (i = 0; i < FRAME_PIXEL_COUNT; i++) {
        frame->pixels[i] = value;
    }

    frame->valid = 1;
}

void display_frame_copy(DisplayFrame *dst, const DisplayFrame *src)
{
    if ((dst == NULL) || (src == NULL)) {
        return;
    }

    memcpy(dst, src, sizeof(DisplayFrame));
}

uint8_t display_frame_get_pixel(const DisplayFrame *frame, uint16_t x, uint16_t y)
{
    int idx;

    if (frame == NULL) {
        return 0;
    }

    idx = frame_index(x, y);
    if (idx < 0) {
        return 0;
    }

    return frame->pixels[idx];
}

void display_frame_set_pixel(DisplayFrame *frame, uint16_t x, uint16_t y, uint8_t value)
{
    int idx;

    if (frame == NULL) {
        return;
    }

    idx = frame_index(x, y);
    if (idx < 0) {
        return;
    }

    frame->pixels[idx] = value;
}