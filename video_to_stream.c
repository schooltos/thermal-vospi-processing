#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>

#include "core/config.h"

/*
 * video_to_stream.c
 *
 * Призначення:
 *   Перетворює існуюче відео у .bin-потік для source_emulator.
 *
 * Логіка:
 *   1. ffmpeg декодує відео у raw grayscale frames 160x120
 *   2. програма читає кожен кадр із stdout ffmpeg
 *   3. кожен кадр пакується у 240 пакетів:
 *        - 4 сегменти
 *        - 60 пакетів у сегменті
 *        - 160 байтів payload у пакеті
 *   4. результати записуються в output stream.bin
 *
 * Формат пакета у файлі:
 *   [0..1]   packet_id   (uint16_t, little-endian)
 *   [2..3]   crc         (uint16_t, little-endian) -> зараз 0
 *   [4..163] payload     (160 bytes)
 *
 * Спрощене кодування packet_id:
 *   bits 15..12 : flags
 *   bits 11..8  : segment (0..3)
 *   bits 7..0   : packet number (0..59)
 *
 * Приклад збірки:
 *   gcc -O2 -o video_to_stream video_to_stream.c
 *
 * Приклад запуску:
 *   ./video_to_stream input.mp4 testdata/packets/stream.bin 8
 *
 * Параметри:
 *   argv[1] = input_video
 *   argv[2] = output_stream
 *   argv[3] = fps (необов'язково, за замовчуванням 8)
 */

#if defined(_WIN32)
    #define POPEN  _popen
    #define PCLOSE _pclose
#else
    #define POPEN  popen
    #define PCLOSE pclose
#endif

#define TOOL_DEFAULT_FPS 8

static int write_u16_le(FILE *fp, uint16_t value)
{
    unsigned char bytes[2];

    if (fp == NULL) {
        return -1;
    }

    bytes[0] = (unsigned char)(value & 0xFFU);
    bytes[1] = (unsigned char)((value >> 8U) & 0xFFU);

    if (fwrite(bytes, 1, 2, fp) != 2) {
        return -1;
    }

    return 0;
}

static uint16_t make_packet_id(uint8_t segment, uint8_t packet_number)
{
    return (uint16_t)(((uint16_t)(segment & 0x0FU) << 8U) |
                      (uint16_t)(packet_number & 0xFFU));
}

static int write_frame_as_packets(FILE *out_fp,
                                  const uint8_t *gray_frame,
                                  unsigned long frame_index)
{
    uint16_t packet_id;
    uint16_t crc = 0;
    uint8_t payload[VOSPI_PAYLOAD_SIZE];

    uint16_t packet_global;
    uint16_t row;
    uint16_t half;
    uint16_t base_x;
    uint16_t i;
    uint16_t segment;
    uint16_t packet_number;

    uint8_t v8;
    uint16_t v16;

    (void)frame_index; /* поки не використовується, але можна залишити для розширення */

    if ((out_fp == NULL) || (gray_frame == NULL)) {
        return -1;
    }

    /*
     * Один пакет = 160 байтів payload = 80 пікселів по 16 біт.
     * Два пакети на рядок.
     * Усього:
     *   160x120 = 19200 пікселів
     *   19200 / 80 = 240 пакетів
     *   240 = 4 сегменти * 60 пакетів
     */
    for (packet_global = 0; packet_global < VOSPI_TOTAL_PACKETS; packet_global++) {
        segment = (uint16_t)(packet_global / VOSPI_PACKETS_PER_SEGMENT);
        packet_number = (uint16_t)(packet_global % VOSPI_PACKETS_PER_SEGMENT);

        row = (uint16_t)(packet_global / 2U);
        half = (uint16_t)(packet_global % 2U);
        base_x = (uint16_t)(half * (FRAME_WIDTH / 2U));

        memset(payload, 0, sizeof(payload));

        for (i = 0; i < (FRAME_WIDTH / 2U); i++) {
            v8 = gray_frame[(size_t)row * FRAME_WIDTH + (size_t)base_x + i];

            /*
             * Переводимо 8-бітний grayscale у псевдо-16-бітне значення.
             * Наприклад:
             *   0x7A -> 0x7A7A
             */
            v16 = (uint16_t)(((uint16_t)v8 << 8U) | (uint16_t)v8);

            /*
             * У payload пишемо big-endian,
             * бо реконструктор читає:
             *   value = (payload[2*i] << 8) | payload[2*i+1]
             */
            payload[2U * i]     = (uint8_t)((v16 >> 8U) & 0xFFU);
            payload[2U * i + 1] = (uint8_t)(v16 & 0xFFU);
        }

        packet_id = make_packet_id((uint8_t)segment, (uint8_t)packet_number);

        if (write_u16_le(out_fp, packet_id) != 0) {
            return -1;
        }

        if (write_u16_le(out_fp, crc) != 0) {
            return -1;
        }

        if (fwrite(payload, 1, VOSPI_PAYLOAD_SIZE, out_fp) != VOSPI_PAYLOAD_SIZE) {
            return -1;
        }
    }

    return 0;
}

static int parse_fps(const char *s)
{
    long value;
    char *endptr = NULL;

    if (s == NULL) {
        return TOOL_DEFAULT_FPS;
    }

    value = strtol(s, &endptr, 10);
    if ((endptr == s) || (value <= 0) || (value > 120)) {
        return TOOL_DEFAULT_FPS;
    }

    return (int)value;
}

int main(int argc, char **argv)
{
    const char *input_video;
    const char *output_stream;
    int fps;

    char ffmpeg_cmd[1024];
    FILE *ffmpeg_pipe = NULL;
    FILE *out_fp = NULL;

    uint8_t gray_frame[FRAME_PIXEL_COUNT];
    size_t bytes_read;
    unsigned long frame_count = 0;

    if (argc < 3) {
        fprintf(stderr, "Usage: %s <input_video> <output_stream> [fps]\n", argv[0]);
        fprintf(stderr, "Example: %s input.mp4 testdata/packets/stream.bin 8\n", argv[0]);
        return 1;
    }

    input_video = argv[1];
    output_stream = argv[2];
    fps = (argc >= 4) ? parse_fps(argv[3]) : TOOL_DEFAULT_FPS;

    /*
     * ffmpeg:
     *   -loglevel error  -> тільки помилки
     *   -i input         -> вхідне відео
     *   -vf fps=...,scale=160:120,format=gray
     *                     -> вибір кадрів, масштабування, grayscale
     *   -f rawvideo
     *   -pix_fmt gray
     *   -                -> вивід у stdout
     *
     * Важливо:
     *   шлях до input_video краще без лапок усередині імені.
     */
    snprintf(ffmpeg_cmd, sizeof(ffmpeg_cmd),
             "ffmpeg -loglevel error -i \"%s\" "
             "-vf fps=%d,scale=%d:%d,format=gray "
             "-f rawvideo -pix_fmt gray -",
             input_video,
             fps,
             FRAME_WIDTH,
             FRAME_HEIGHT);

    printf("FFmpeg command:\n%s\n", ffmpeg_cmd);

    ffmpeg_pipe = POPEN(ffmpeg_cmd, "r");
    if (ffmpeg_pipe == NULL) {
        perror("popen failed");
        fprintf(stderr, "Failed to start ffmpeg\n");
        return 2;
    }

    out_fp = fopen(output_stream, "wb");
    if (out_fp == NULL) {
        fprintf(stderr, "Failed to open output file: %s\n", output_stream);
        PCLOSE(ffmpeg_pipe);
        return 3;
    }

    while (1) {
        bytes_read = fread(gray_frame, 1, FRAME_PIXEL_COUNT, ffmpeg_pipe);

        if (bytes_read == 0) {
            break; /* normal EOF */
        }

        if (bytes_read != FRAME_PIXEL_COUNT) {
            fprintf(stderr, "Warning: partial frame detected (%zu bytes). Stopping.\n", bytes_read);
            break;
        }

        if (write_frame_as_packets(out_fp, gray_frame, frame_count) != 0) {
            fprintf(stderr, "Failed while writing frame %lu\n", frame_count);
            fclose(out_fp);
            PCLOSE(ffmpeg_pipe);
            return 4;
        }

        frame_count++;
    }

    fclose(out_fp);
    PCLOSE(ffmpeg_pipe);

    printf("Done.\n");
    printf("Frames written: %lu\n", frame_count);
    printf("Output stream : %s\n", output_stream);
    printf("Frame size    : %dx%d\n", FRAME_WIDTH, FRAME_HEIGHT);
    printf("FPS sampled   : %d\n", fps);

    return 0;
}