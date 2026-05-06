#include <stdio.h>
#include <stdlib.h>
#include <time.h>

#include "app_pipeline.h"
#include "input/source_emulator.h"
#include "input/vospi_parser.h"
#include "output/output_image.h"

#define ENABLE_DEBUG_LOGS 0
#define INPUT_STREAM_PATH "testdata/packets/stream.bin"
#define OUTPUT_FRAMES_DIR "testdata/output_frames"
#define OUTPUT_VIDEO_PATH "testdata/result_video.mp4"
#define OUTPUT_VIDEO_FPS 8

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

    metrics->total_start = 0;
    metrics->total_end = 0;

    metrics->read_time_sec = 0.0;
    metrics->pipeline_time_sec = 0.0;
    metrics->output_time_sec = 0.0;
    metrics->video_build_time_sec = 0.0;

    metrics->packets_read = 0;
    metrics->frames_written = 0;
}

static void print_metrics(const AppContext *app, const RuntimeMetrics *metrics)
{
    double total_time;
    double processing_time;
    double fps_total;
    double fps_pipeline;
    double avg_ms_per_frame;
    double avg_us_per_packet;

    if ((app == NULL) || (metrics == NULL)) {
        return;
    }

    total_time = clock_diff_sec(metrics->total_start, metrics->total_end);
    processing_time = metrics->pipeline_time_sec;

    fps_total = 0.0;
    fps_pipeline = 0.0;
    avg_ms_per_frame = 0.0;
    avg_us_per_packet = 0.0;

    if (app->frames_ready > 0) {
        fps_total = (double)app->frames_ready / total_time;
        fps_pipeline = (double)app->frames_ready / processing_time;
        avg_ms_per_frame = (processing_time * 1000.0) / (double)app->frames_ready;
    }

    if (metrics->packets_read > 0) {
        avg_us_per_packet = (metrics->read_time_sec * 1000000.0) /
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

static int build_output_video(void)
{
    char command[512];

    snprintf(command,
             sizeof(command),
             "ffmpeg -y -loglevel error "
             "-framerate %d "
             "-i %s/frame_%%06d.pgm "
             "-c:v libx264 -pix_fmt yuv420p "
             "%s",
             OUTPUT_VIDEO_FPS,
             OUTPUT_FRAMES_DIR,
             OUTPUT_VIDEO_PATH);

    return system(command);
}

int main(void)
{
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

    metrics_init(&metrics);

    system("mkdir -p testdata/output_frames");

    app_pipeline_load_default_config(&cfg);

    if (app_pipeline_init(&app, &cfg, NULL) != APP_PIPELINE_OK) {
        printf("app_pipeline_init failed\n");
        return 1;
    }

    if (source_emulator_init(INPUT_STREAM_PATH) != 0) {
        printf("source_emulator_init failed: %s\n", INPUT_STREAM_PATH);
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

            if (output_image_write_pgm_indexed(
                    OUTPUT_FRAMES_DIR,
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
    video_status = build_output_video();
    t1 = clock();

    metrics.video_build_time_sec = clock_diff_sec(t0, t1);

    metrics.total_end = clock();

    print_metrics(&app, &metrics);

    if (video_status != 0) {
        printf("Video build failed\n");
    } else {
        printf("Video created: %s\n", OUTPUT_VIDEO_PATH);
    }

    app_pipeline_deinit(&app);

    return 0;
}