#ifndef APP_PIPELINE_H
#define APP_PIPELINE_H

#include <stdint.h>
#include "core/types.h"
#include "input/frame_reconstructor.h"
#include "processing/bad_pixels.h"
#include "processing/denoise.h"
#include "processing/normalize.h"
#include "processing/contrast.h"

/*
 * Статуси роботи пайплайна.
 * APP_PIPELINE_NO_FRAME означає, що пакет прийнято,
 * але повний кадр ще не зібрано.
 */
typedef enum {
    APP_PIPELINE_OK = 0,
    APP_PIPELINE_NO_FRAME = 1,

    APP_PIPELINE_INVALID_ARG = -1,
    APP_PIPELINE_RECON_ERROR = -2,
    APP_PIPELINE_FRAME_INVALID = -3
} AppPipelineStatus;

/*
 * Конфігурація пайплайна.
 * Дає змогу вмикати або вимикати окремі етапи.
 */
typedef struct {
    uint8_t enable_bad_pixel_correction;
    uint8_t enable_denoise;
    uint8_t enable_normalize;
    uint8_t enable_contrast;

    uint8_t use_percentile_normalization;
    float iir_alpha;

    /* Наприклад, 2 і 98 */
    uint16_t low_percentile;
    uint16_t high_percentile;
} ProcessingConfig;

/*
 * Головний контекст застосунку.
 * Тут зберігається стан усіх модулів.
 */
typedef struct {
    ReconstructorContext recon_ctx;
    DenoiseContext denoise_ctx;
    BadPixelMap bad_pixel_map;
    ProcessingConfig cfg;

    uint32_t packets_processed;
    uint32_t frames_ready;
    uint32_t frames_dropped;
} AppContext;

// Заповнює структуру типовими параметрами пайплайна
void app_pipeline_load_default_config(ProcessingConfig *cfg);

/*
 * Ініціалізація пайплайна.
 * user_bad_map може бути NULL — тоді карта дефектів буде порожньою.
 */
int app_pipeline_init(AppContext *ctx,
                      const ProcessingConfig *cfg,
                      const BadPixelMap *user_bad_map);

/*
 * Скидання внутрішнього стану пайплайна.
 */
void app_pipeline_reset(AppContext *ctx);

/*
 * Обробка одного пакета.
 *
 * Якщо повний кадр ще не готовий:
 *   повертається APP_PIPELINE_NO_FRAME
 *
 * Якщо кадр зібрано і успішно оброблено:
 *   повертається APP_PIPELINE_OK,
 *   а out_frame містить готовий результат для відображення.
 */
AppPipelineStatus app_pipeline_process_packet(AppContext *ctx,
                                              const VoSPIPacket *packet,
                                              DisplayFrame *out_frame);

/*
 * Звільнення або очищення стану.
 * Для поточного каркаса це фактично reset.
 */
void app_pipeline_deinit(AppContext *ctx);

#endif /* APP_PIPELINE_H */