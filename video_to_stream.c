#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>

#include "core/config.h"

/*
 * video_to_stream.c
 *
 * Утиліта для перетворення звичайного відеофайлу
 * у VoSPI-подібний .bin потік для source_emulator.
 *
 * Загальна логіка:
 * - ffmpeg декодує відео у grayscale кадри 160x120;
 * - кожен кадр перетворюється у 16-бітні thermal-like значення;
 * - кадр пакується у VoSPI-подібні пакети;
 * - пакети записуються у .bin файл.
 *
 * Формат одного пакета:
 *   [0..3]     header      4 bytes
 *   [4..163]   payload     160 bytes
 *
 * Один пакет містить 80 пікселів по 16 біт.
 * Два пакети формують один рядок 160 пікселів.
 */

#if defined(_WIN32)
    #define POPEN  _popen
    #define PCLOSE _pclose
#else
    #define POPEN  popen
    #define PCLOSE pclose
#endif

#define TOOL_DEFAULT_FPS 8

/*
 * Формування VoSPI-подібного header.
 *
 * Поточний емуляційний формат:
 *   header[0] bits 7..4  - flags
 *   header[0] bits 3..0  - segment
 *   header[1]            - packet number
 *   header[2..3]         - службові байти / резерв
 *
 * Для звичайного коректного пакета flags = 0.
 */
static void make_vospi_header(uint8_t header[VOSPI_HEADER_SIZE],
                              uint8_t segment,
                              uint8_t packet_number)
{
    header[0] = (uint8_t)(segment & 0x0FU);
    header[1] = packet_number;
    header[2] = 0U;
    header[3] = 0U;
}

/*
 * Запис одного кадру у вигляді послідовності пакетів.
 *
 * Вхідний кадр:
 *   gray_frame - 8-бітний grayscale кадр 160x120.
 *
 * Вихід:
 *   у файл записуються 240 пакетів:
 *   - 4 сегменти;
 *   - 60 пакетів у кожному сегменті;
 *   - 160 байтів payload у кожному пакеті.
 */
static int write_frame_as_packets(FILE *out_fp,
                                  const uint8_t *gray_frame)
{
    uint8_t header[VOSPI_HEADER_SIZE];
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

    if ((out_fp == NULL) || (gray_frame == NULL)) {
        return -1;
    }

    /*
     * Для кадру 160x120:
     *
     * 160 * 120 = 19200 пікселів.
     * 1 пакет = 80 пікселів.
     * 19200 / 80 = 240 пакетів.
     *
     * 240 пакетів = 4 сегменти * 60 пакетів.
     */
    for (packet_global = 0;
         packet_global < VOSPI_TOTAL_PACKETS;
         packet_global++) {

        segment =
            (uint16_t)(packet_global /
                       VOSPI_PACKETS_PER_SEGMENT);

        packet_number =
            (uint16_t)(packet_global %
                       VOSPI_PACKETS_PER_SEGMENT);

        /*
         * Кожен рядок складається з двох пакетів:
         * - packet_global % 2 == 0: ліва половина рядка;
         * - packet_global % 2 == 1: права половина рядка.
         */
        row = (uint16_t)(packet_global / 2U);
        half = (uint16_t)(packet_global % 2U);

        base_x =
            (uint16_t)(half * (FRAME_WIDTH / 2U));

        make_vospi_header(header,
                          (uint8_t)segment,
                          (uint8_t)packet_number);

        memset(payload,
               0,
               sizeof(payload));

        /*
         * Перетворення 8-бітного grayscale у псевдо-16-бітне
         * thermal-like значення.
         *
         * Наприклад:
         *   0x7A -> 0x7A7A
         *
         * Payload записується у big-endian форматі,
         * оскільки frame_reconstructor читає значення як:
         *   value = payload[2*i] << 8 | payload[2*i + 1]
         */
        for (i = 0; i < (FRAME_WIDTH / 2U); i++) {

            v8 =
                gray_frame[(size_t)row * FRAME_WIDTH +
                           (size_t)base_x +
                           i];

            v16 =
                (uint16_t)(((uint16_t)v8 << 8U) |
                           (uint16_t)v8);

            payload[2U * i] =
                (uint8_t)((v16 >> 8U) & 0xFFU);

            payload[2U * i + 1U] =
                (uint8_t)(v16 & 0xFFU);
        }

        /*
         * Запис пакета у вихідний .bin файл.
         */
        if (fwrite(header,
                   1,
                   VOSPI_HEADER_SIZE,
                   out_fp) != VOSPI_HEADER_SIZE) {
            return -1;
        }

        if (fwrite(payload,
                   1,
                   VOSPI_PAYLOAD_SIZE,
                   out_fp) != VOSPI_PAYLOAD_SIZE) {
            return -1;
        }
    }

    return 0;
}

/*
 * Зчитування FPS із аргументів командного рядка.
 *
 * Якщо значення некоректне, використовується
 * TOOL_DEFAULT_FPS.
 */
static int parse_fps(const char *s)
{
    long value;
    char *endptr = NULL;

    if (s == NULL) {
        return TOOL_DEFAULT_FPS;
    }

    value = strtol(s,
                   &endptr,
                   10);

    if ((endptr == s) ||
        (value <= 0) ||
        (value > 120)) {
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

    /*
     * Очікувані аргументи:
     *   argv[1] - шлях до вхідного відео;
     *   argv[2] - шлях до вихідного stream.bin;
     *   argv[3] - FPS для вибірки кадрів, необов'язково.
     */
    if (argc < 3) {
        fprintf(stderr,
                "Usage: %s <input_video> <output_stream> [fps]\n",
                argv[0]);

        fprintf(stderr,
                "Example: %s input.mp4 testdata/packets/stream.bin 8\n",
                argv[0]);

        return 1;
    }

    input_video = argv[1];
    output_stream = argv[2];

    fps =
        (argc >= 4) ?
        parse_fps(argv[3]) :
        TOOL_DEFAULT_FPS;

    /*
     * Команда ffmpeg:
     * - бере вхідне відео;
     * - вибирає кадри з потрібною частотою;
     * - масштабує до 160x120;
     * - переводить у grayscale;
     * - виводить raw video у stdout.
     *
     * На macOS для popen використовується режим "r",
     * а не "rb".
     */
    snprintf(ffmpeg_cmd,
             sizeof(ffmpeg_cmd),
             "ffmpeg -loglevel error -i \"%s\" "
             "-vf fps=%d,scale=%d:%d,format=gray "
             "-f rawvideo -pix_fmt gray -",
             input_video,
             fps,
             FRAME_WIDTH,
             FRAME_HEIGHT);

    ffmpeg_pipe = POPEN(ffmpeg_cmd,
                        "r");

    if (ffmpeg_pipe == NULL) {
        perror("popen failed");
        fprintf(stderr,
                "Failed to start ffmpeg\n");
        return 2;
    }

    /*
     * Відкриття вихідного .bin файлу.
     */
    out_fp = fopen(output_stream,
                   "wb");

    if (out_fp == NULL) {
        fprintf(stderr,
                "Failed to open output file: %s\n",
                output_stream);

        PCLOSE(ffmpeg_pipe);

        return 3;
    }

    /*
     * Зчитування кадрів із ffmpeg.
     *
     * Кожен кадр має розмір:
     *   FRAME_WIDTH * FRAME_HEIGHT байтів.
     */
    while (1) {

        bytes_read =
            fread(gray_frame,
                  1,
                  FRAME_PIXEL_COUNT,
                  ffmpeg_pipe);

        if (bytes_read == 0) {
            break;
        }

        if (bytes_read != FRAME_PIXEL_COUNT) {
            fprintf(stderr,
                    "Warning: partial frame detected (%zu bytes). Stopping.\n",
                    bytes_read);
            break;
        }

        /*
         * Пакування кадру у VoSPI-подібний потік.
         */
        if (write_frame_as_packets(out_fp,
                                   gray_frame) != 0) {

            fprintf(stderr,
                    "Failed while writing frame %lu\n",
                    frame_count);

            fclose(out_fp);
            PCLOSE(ffmpeg_pipe);

            return 4;
        }

        frame_count++;
    }

    fclose(out_fp);
    PCLOSE(ffmpeg_pipe);

    printf("Done.\n");
    printf("Frames written: %lu\n",
           frame_count);
    printf("Output stream : %s\n",
           output_stream);
    printf("Frame size    : %dx%d\n",
           FRAME_WIDTH,
           FRAME_HEIGHT);
    printf("FPS sampled   : %d\n",
           fps);

    return 0;
}