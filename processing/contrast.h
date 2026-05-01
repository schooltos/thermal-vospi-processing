#ifndef PROCESSING_CONTRAST_H
#define PROCESSING_CONTRAST_H

#include <stdint.h>
#include "core/types.h"
#include "core/config.h"

/*
 * Глобальне гістограмне вирівнювання для 8-бітного кадру.
 */
void contrast_hist_eq(DisplayFrame *frame);

#endif /* PROCESSING_CONTRAST_H */