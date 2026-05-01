#ifndef OUTPUT_OUTPUT_IMAGE_H
#define OUTPUT_OUTPUT_IMAGE_H

#include "core/types.h"

int output_image_write_pgm(const char *path, const DisplayFrame *frame);

/*
 * Зберігає кадр у форматі:
 *   <directory>/frame_000001.pgm
 *
 * Повертає:
 *   0  - успіх
 *  -1  - помилка
 */
int output_image_write_pgm_indexed(const char *directory,
                                   unsigned long frame_number,
                                   const DisplayFrame *frame);

#endif /* OUTPUT_OUTPUT_IMAGE_H */