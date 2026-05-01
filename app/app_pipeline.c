#include "app_pipeline.h"
#include <string.h>

#define APP_DEFAULT_IIR_ALPHA        0.2f
#define APP_DEFAULT_LOW_PERCENTILE   2U
#define APP_DEFAULT_HIGH_PERCENTILE  98U

/*
 * Внутрішня функція: застосування всіх етапів обробки
 * до вже реконструйованого кадру.
 */
static AppPipelineStatus process_full_frame(AppContext *ctx,
                                            ThermalFrame *thermal_frame,
                                            DisplayFrame *display_frame)
{
    if ((ctx == NULL) || (thermal_frame == NULL) || (display_frame == NULL)) {
        return APP_PIPELINE_INVALID_ARG;
    }

    /* 1. Компенсація дефектних пікселів */
    if (ctx->cfg.enable_bad_pixel_correction) {
        bad_pixels_apply(thermal_frame, &ctx->bad_pixel_map);
    }

    /* 2. Шумопригнічення */
    if (ctx->cfg.enable_denoise) {
        denoise_median3x3_apply(thermal_frame);
    }

    /* 3. Нормалізація до 8-бітного відображення */
    if (ctx->cfg.enable_normalize) {
        if (ctx->cfg.use_percentile_normalization) {
            normalize_percentile_to_display(
                thermal_frame,
                display_frame,
                ctx->cfg.low_percentile,
                ctx->cfg.high_percentile
            );
        } else {
            normalize_minmax_to_display(thermal_frame, display_frame);
        }
    } else {
        /*
         * Якщо нормалізація вимкнена, усе одно треба сформувати display frame.
         * Для простоти використовуємо min-max як fallback.
         */
        normalize_minmax_to_display(thermal_frame, display_frame);
    }

    /* 4. Підсилення контрасту */
    if (ctx->cfg.enable_contrast) {
        contrast_hist_eq(display_frame);
    }

    return APP_PIPELINE_OK;
}

void app_pipeline_load_default_config(ProcessingConfig *cfg)
{
    if (cfg == NULL) {
        return;
    }

    memset(cfg, 0, sizeof(*cfg));

    cfg->enable_bad_pixel_correction = 1;
    cfg->enable_denoise = 1;
    cfg->enable_normalize = 1;
    cfg->enable_contrast = 1;

    cfg->use_percentile_normalization = 1;
    cfg->iir_alpha = 0.8f;
    cfg->low_percentile = 2;
    cfg->high_percentile = 98;
}

int app_pipeline_init(AppContext *ctx,
                      const ProcessingConfig *cfg,
                      const BadPixelMap *user_bad_map)
{
    if ((ctx == NULL) || (cfg == NULL)) {
        return APP_PIPELINE_INVALID_ARG;
    }

    memset(ctx, 0, sizeof(*ctx));

    ctx->cfg = *cfg;

    /*
     * Перевірка альфи IIR.
     * Якщо значення некоректне — ставимо безпечне типове.
     */
    if ((ctx->cfg.iir_alpha <= 0.0f) || (ctx->cfg.iir_alpha > 1.0f)) {
        ctx->cfg.iir_alpha = 0.2f;
    }

    if (user_bad_map != NULL) {
        ctx->bad_pixel_map = *user_bad_map;
    } else {
        memset(&ctx->bad_pixel_map, 0, sizeof(ctx->bad_pixel_map));
    }

    reconstructor_init(&ctx->recon_ctx);
    denoise_init(&ctx->denoise_ctx);

    ctx->packets_processed = 0;
    ctx->frames_ready = 0;
    ctx->frames_dropped = 0;

    return APP_PIPELINE_OK;
}

void app_pipeline_reset(AppContext *ctx)
{
    if (ctx == NULL) {
        return;
    }

    reconstructor_reset(&ctx->recon_ctx);
    denoise_reset(&ctx->denoise_ctx);

    ctx->packets_processed = 0;
    ctx->frames_ready = 0;
    ctx->frames_dropped = 0;
}

AppPipelineStatus app_pipeline_process_packet(AppContext *ctx,
                                              const VoSPIPacket *packet,
                                              DisplayFrame *out_frame)
{
    ThermalFrame thermal_frame;
    int push_result;
    int get_result;

    if ((ctx == NULL) || (packet == NULL) || (out_frame == NULL)) {
        return APP_PIPELINE_INVALID_ARG;
    }

    ctx->packets_processed++;

    /*
     * Передаємо пакет у реконструктор.
     * Він сам вирішує, чи пакет корисний, чи треба re-sync.
     */
    push_result = reconstructor_push_packet(&ctx->recon_ctx, packet);
    if (push_result < 0) {
        return APP_PIPELINE_RECON_ERROR;
    }

    /*
     * Якщо повний кадр ще не готовий — просто чекаємо наступні пакети.
     */
    if (!reconstructor_is_frame_ready(&ctx->recon_ctx)) {
        return APP_PIPELINE_NO_FRAME;
    }

    /*
     * Отримуємо повний кадр.
     */
    memset(&thermal_frame, 0, sizeof(thermal_frame));
    get_result = reconstructor_get_frame(&ctx->recon_ctx, &thermal_frame);
    if (get_result < 0) {
        ctx->frames_dropped++;
        return APP_PIPELINE_FRAME_INVALID;
    }

    if (!thermal_frame.valid) {
        ctx->frames_dropped++;
        return APP_PIPELINE_FRAME_INVALID;
    }

    /*
     * Повний пайплайн обробки.
     */
    if (process_full_frame(ctx, &thermal_frame, out_frame) != APP_PIPELINE_OK) {
        ctx->frames_dropped++;
        return APP_PIPELINE_FRAME_INVALID;
    }

    ctx->frames_ready++;
    return APP_PIPELINE_OK;
}

void app_pipeline_deinit(AppContext *ctx)
{
    if (ctx == NULL) {
        return;
    }

    app_pipeline_reset(ctx);
}