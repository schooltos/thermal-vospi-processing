#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "app_pipeline.h"
#include "input/source_emulator.h"
#include "input/vospi_parser.h"
#include "output/output_image.h"

#define ENABLE_DEBUG_LOGS 0
#define DEFAULT_OUTPUT_VIDEO_FPS 8

typedef enum {
    PROCESSING_PROFILE_DEFAULT = 0,
    PROCESSING_PROFILE_PEOPLE,
    PROCESSING_PROFILE_TRANSPORT,
    PROCESSING_PROFILE_NATURE,
    PROCESSING_PROFILE_DRONE
} ProcessingProfile;

typedef struct {
    clock_t total_start;
    clock_t total_end;

    double read_time_sec;
    double pipeline_time_sec;
    double output_time_sec;
    double video_build_time_sec;

    unsigned long packets_read;
    unsigned long frames_written;
} RuntimeMetrics;

static double clock_diff_sec(clock_t start, clock_t end)
{
    return (double)(end - start) / (double)CLOCKS_PER_SEC;
}

static void metrics_init(RuntimeMetrics *metrics)
{
    if (metrics == NULL) {
        return;
    }

    memset(metrics, 0, sizeof(*metrics));
}

static ProcessingProfile parse_profile(const char *profile_name)
{
    if (profile_name == NULL) {
        return PROCESSING_PROFILE_DEFAULT;
    }

    if (strcmp(profile_name, "people") == 0) {
        return PROCESSING_PROFILE_PEOPLE;
    }

    if (strcmp(profile_name, "transport") == 0) {
        return PROCESSING_PROFILE_TRANSPORT;
    }

    if (strcmp(profile_name, "nature") == 0) {
        return PROCESSING_PROFILE_NATURE;
    }

    if (strcmp(profile_name, "drone") == 0) {
        return PROCESSING_PROFILE_DRONE;
    }

    return PROCESSING_PROFILE_DEFAULT;
}

static const char *profile_to_string(ProcessingProfile profile)
{
    switch (profile) {
        case PROCESSING_PROFILE_PEOPLE:
            return "people";

        case PROCESSING_PROFILE_TRANSPORT:
            return "transport";

        case PROCESSING_PROFILE_NATURE:
            return "nature";

        case PROCESSING_PROFILE_DRONE:
            return "drone";

        case PROCESSING_PROFILE_DEFAULT:
        default:
            return "default";
    }
}

/*
 * Налаштування пайплайна під різні типи thermal-сцен.
 *
 * Ідея:
 * - people: зменшити ризик пересвітлення гарячих силуетів;
 * - transport: збалансована нормалізація для технічних об'єктів;
 * - nature: підсилити слабкий контраст природних сцен;
 * - drone: підсилити дрібні об'єкти на складному фоні.
 */
static void apply_processing_profile(ProcessingConfig *cfg,
                                     ProcessingProfile profile)
{
    if (cfg == NULL) {
        return;
    }

    app_pipeline_load_default_config(cfg);

    cfg->enable_bad_pixel_correction = 1;
    cfg->enable_denoise = 1;
    cfg->enable_normalize = 1;

    cfg->use_percentile_normalization = 1;

    switch (profile) {
        case PROCESSING_PROFILE_PEOPLE:
            cfg->enable_contrast = 0;
            cfg->low_percentile = 1;
            cfg->high_percentile = 99;
            break;

        case PROCESSING_PROFILE_TRANSPORT:
            cfg->enable_contrast = 0;
            cfg->low_percentile = 2;
            cfg->high_percentile = 98;
            break;

        case PROCESSING_PROFILE_NATURE:
            cfg->enable_contrast = 1;
            cfg->low_percentile = 1;
            cfg->high_percentile = 99;
            break;

        case PROCESSING_PROFILE_DRONE:
            cfg->enable_contrast = 1;
            cfg->low_percentile = 2;
            cfg->high_percentile = 98;
            break;

        case PROCESSING_PROFILE_DEFAULT:
        default:
            cfg->enable_contrast = 0;
            cfg->low_percentile = 1;
            cfg->high_percentile = 99;
            break;
    }
}

static void print_config(const ProcessingConfig *cfg,
                         ProcessingProfile profile)
{
    if (cfg == NULL) {
        return;
    }

    printf("Processing profile: %s\n", profile_to_string(profile));
    printf("Bad pixels:         %u\n", cfg->enable_bad_pixel_correction);
    printf("Denoise:            %u\n", cfg->enable_denoise);
    printf("Normalize:          %u\n", cfg->enable_normalize);
    printf("Contrast:           %u\n", cfg->enable_contrast);
    printf("Percentile mode:    %u\n", cfg->use_percentile_normalization);
    printf("Low percentile:     %u\n", cfg->low_percentile);
    printf("High percentile:    %u\n", cfg->high_percentile);
    printf("\n");
}

static int ensure_directory_exists(const char *directory)
{
    char command[512];

    if (directory == NULL) {
        return -1;
    }

    snprintf(command,
             sizeof(command),
             "mkdir -p \"%s\"",
             directory);

    return system(command);
}

static int build_output_video(const char *frames_dir,
                              const char *output_video_path,
                              int fps)
{
    char command[1024];

    if ((frames_dir == NULL) || (output_video_path == NULL)) {
        return -1;
    }

    snprintf(command,
             sizeof(command),
             "ffmpeg -y -loglevel error "
             "-framerate %d "
             "-i \"%s/frame_%%06d.pgm\" "
             "-c:v libx264 -pix_fmt yuv420p "
             "\"%s\"",
             fps,
             frames_dir,
             output_video_path);

    return system(command);
}

static void print_metrics(const AppContext *app,
                          const RuntimeMetrics *metrics)
{
    double total_time;
    double pipeline_time;
    double fps_total = 0.0;
    double fps_pipeline = 0.0;
    double avg_ms_per_frame = 0.0;
    double avg_us_per_packet = 0.0;

    if ((app == NULL) || (metrics == NULL)) {
        return;
    }

    total_time = clock_diff_sec(metrics->total_start,
                                metrics->total_end);

    pipeline_time = metrics->pipeline_time_sec;

    if ((app->frames_ready > 0) && (total_time > 0.0)) {
        fps_total = (double)app->frames_ready / total_time;
    }

    if ((app->frames_ready > 0) && (pipeline_time > 0.0)) {
        fps_pipeline = (double)app->frames_ready / pipeline_time;
        avg_ms_per_frame =
            (pipeline_time * 1000.0) / (double)app->frames_ready;
    }

    if ((metrics->packets_read > 0) && (metrics->read_time_sec > 0.0)) {
        avg_us_per_packet =
            (metrics->read_time_sec * 1000000.0) /
            (double)metrics->packets_read;
    }

    printf("\n========== Runtime metrics ==========\n");

    printf("Packets read:        %lu\n", metrics->packets_read);
    printf("Packets processed:   %lu\n", (unsigned long)app->packets_processed);
    printf("Frames ready:        %lu\n", (unsigned long)app->frames_ready);
    printf("Frames written:      %lu\n", metrics->frames_written);
    printf("Frames dropped:      %lu\n", (unsigned long)app->frames_dropped);

    printf("\n---------- Time ----------\n");
    printf("Total time:          %.6f s\n", total_time);
    printf("Packet read time:    %.6f s\n", metrics->read_time_sec);
    printf("Pipeline time:       %.6f s\n", metrics->pipeline_time_sec);
    printf("Frame output time:   %.6f s\n", metrics->output_time_sec);
    printf("Video build time:    %.6f s\n", metrics->video_build_time_sec);

    printf("\n---------- Performance ----------\n");
    printf("Total FPS:           %.2f frames/s\n", fps_total);
    printf("Pipeline FPS:        %.2f frames/s\n", fps_pipeline);
    printf("Avg pipeline/frame:  %.3f ms\n", avg_ms_per_frame);
    printf("Avg read/packet:     %.3f us\n", avg_us_per_packet);

    printf("=====================================\n");
}

int main(int argc, char **argv)
{
    const char *input_stream_path;
    const char *output_frames_dir;
    const char *output_video_path;
    const char *profile_name;

    ProcessingProfile profile;

    AppContext app;
    ProcessingConfig cfg;
    DisplayFrame out_frame;
    VoSPIPacket packet;
    AppPipelineStatus status;
    RuntimeMetrics metrics;

    int read_status;
    int video_status;

    clock_t t0;
    clock_t t1;

    if (argc < 4) {
        printf("Usage: %s <stream.bin> <output_frames_dir> <output_video.mp4> [profile]\n",
               argv[0]);
        printf("Profiles: default, people, transport, nature, drone\n");
        return 1;
    }

    input_stream_path = argv[1];
    output_frames_dir = argv[2];
    output_video_path = argv[3];
    profile_name = (argc >= 5) ? argv[4] : "default";

    profile = parse_profile(profile_name);

    metrics_init(&metrics);

    apply_processing_profile(&cfg, profile);
    print_config(&cfg, profile);

    if (ensure_directory_exists(output_frames_dir) != 0) {
        printf("failed to create output directory: %s\n", output_frames_dir);
        return 1;
    }

    if (app_pipeline_init(&app, &cfg, NULL) != APP_PIPELINE_OK) {
        printf("app_pipeline_init failed\n");
        return 1;
    }

    if (source_emulator_init(input_stream_path) != 0) {
        printf("source_emulator_init failed: %s\n", input_stream_path);
        app_pipeline_deinit(&app);
        return 1;
    }

    metrics.total_start = clock();

    while (1) {
        t0 = clock();
        read_status = source_emulator_read_packet(&packet);
        t1 = clock();

        metrics.read_time_sec += clock_diff_sec(t0, t1);

        if (read_status == 0) {
            metrics.packets_read++;

#if ENABLE_DEBUG_LOGS
            VoSPIPacketInfo info = vospi_parse_packet(&packet);

            printf(
                "read_status=%d, header=[%02X %02X %02X %02X], "
                "valid=%u, discard=%u, segment=%u, packet=%u\n",
                read_status,
                packet.header[0],
                packet.header[1],
                packet.header[2],
                packet.header[3],
                info.valid,
                info.discard,
                info.segment,
                info.packet_number
            );
#endif

        } else {
#if ENABLE_DEBUG_LOGS
            printf("read_status=%d\n", read_status);
#endif
            break;
        }

        t0 = clock();
        status = app_pipeline_process_packet(&app, &packet, &out_frame);
        t1 = clock();

        metrics.pipeline_time_sec += clock_diff_sec(t0, t1);

        if (status == APP_PIPELINE_OK) {
            t0 = clock();

            if (output_image_write_pgm_indexed(output_frames_dir,
                                               out_frame.frame_number,
                                               &out_frame) == 0) {
                metrics.frames_written++;
            } else {
                printf("failed to write frame %lu\n",
                       (unsigned long)out_frame.frame_number);
            }

            t1 = clock();
            metrics.output_time_sec += clock_diff_sec(t0, t1);

        } else if (status < 0) {
            printf("pipeline error: %d\n", status);
        }
    }

    source_emulator_deinit();

    t0 = clock();
    video_status = build_output_video(output_frames_dir,
                                      output_video_path,
                                      DEFAULT_OUTPUT_VIDEO_FPS);
    t1 = clock();

    metrics.video_build_time_sec = clock_diff_sec(t0, t1);

    metrics.total_end = clock();

    print_metrics(&app, &metrics);

    if (video_status != 0) {
        printf("Video build failed: %s\n", output_video_path);
    } else {
        printf("Video created: %s\n", output_video_path);
    }

    app_pipeline_deinit(&app);

    return 0;
}