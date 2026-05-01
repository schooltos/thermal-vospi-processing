#ifndef PROCESSING_NORMALIZE_H
#define PROCESSING_NORMALIZE_H

#include <stdint.h>
#include "core/types.h"
#include "core/config.h"

/*
 * Проста min-max нормалізація:
 * мінімум кадру -> 0
 * максимум кадру -> 255
 */
void normalize_minmax_to_display(const ThermalFrame *src,
                                 DisplayFrame *dst);

/*
 * Нормалізація з відсіканням крайніх значень.
 * low_percentile і high_percentile задаються в межах [0..100].
 *
 * Наприклад:
 * low_percentile = 2
 * high_percentile = 98
 */
void normalize_percentile_to_display(const ThermalFrame *src,
                                     DisplayFrame *dst,
                                     uint16_t low_percentile,
                                     uint16_t high_percentile);

#endif /* PROCESSING_NORMALIZE_H */