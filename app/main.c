#include <stdio.h>
#include <stdlib.h>

#include "app_pipeline.h"
#include "input/source_emulator.h"
#include "output/output_image.h"

int main(void)
{
    AppContext app;
    ProcessingConfig cfg;
    DisplayFrame out_frame;
    VoSPIPacket packet;
    AppPipelineStatus status;
    int read_status;

    app_pipeline_load_default_config(&cfg);

    if (app_pipeline_init(&app, &cfg, NULL) != APP_PIPELINE_OK) {
        printf("app_pipeline_init failed\n");
        return 1;
    }

    if (source_emulator_init("testdata/packets/stream.bin") != 0) {
        printf("source_emulator_init failed\n");
        app_pipeline_deinit(&app);
        return 1;
    }

    while (1) {
        read_status = source_emulator_read_packet(&packet);
        printf("read_status = %d, packet_id = 0x%04X\n",
            read_status,
            packet.packet_id);

        if (read_status != 0) {
            break;
        }

        status = app_pipeline_process_packet(&app, &packet, &out_frame);

        if (status == APP_PIPELINE_OK) {
            output_image_write_pgm_indexed(
                "testdata/output_frames",
                out_frame.frame_number,
                &out_frame
            );
        } else if (status < 0) {
            printf("pipeline error: %d\n", status);
        }
    }

    source_emulator_deinit();

    printf("Packets processed: %lu\n", (unsigned long)app.packets_processed);
    printf("Frames ready:      %lu\n", (unsigned long)app.frames_ready);
    printf("Frames dropped:    %lu\n", (unsigned long)app.frames_dropped);

    /* --- Збірка відео через ffmpeg --- */
    int ret = system(
        "ffmpeg -y -framerate 8 "
        "-i testdata/output_frames/frame_%06d.pgm "
        "-c:v libx264 -pix_fmt yuv420p "
        "testdata/result_video.mp4"
    );

    if (ret != 0) {
        printf("ffmpeg failed with code %d\n", ret);
    } else {
        printf("Video created: testdata/result_video.mp4\n");
    }

    app_pipeline_deinit(&app);

    return 0;
}