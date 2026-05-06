/*
 * Модуль реконструкції теплового кадру.
 *
 * Даний модуль приймає послідовність VoSPI-подібних пакетів,
 * перевіряє їх порядок та формує повний тепловий кадр.
 *
 * Основна логіка:
 * - пакети групуються по сегментах;
 * - кожен пакет містить половину рядка зображення;
 * - два пакети формують один повний рядок;
 * - після отримання всіх пакетів формується готовий кадр;
 * - при втраті синхронізації частковий кадр скидається.
 */

#include "frame_reconstructor.h"

#include <string.h>

#include "core/frame.h"
#include "input/vospi_parser.h"

/*
 * Підготовка нового кадру.
 *
 * Очищуються:
 * - робочий буфер кадру;
 * - інформація про отримані сегменти;
 * - службові лічильники реконструктора.
 */
static void reconstructor_prepare_new_frame(ReconstructorContext *ctx)
{
    thermal_frame_clear(&ctx->working_frame);

    memset(ctx->segment_received,
           0,
           sizeof(ctx->segment_received));

    ctx->synced = 1U;
    ctx->expected_segment = 0U;
    ctx->expected_packet = 0U;
    ctx->packets_in_current_frame = 0U;
}

/*
 * Скидання частково реконструйованого кадру.
 *
 * Використовується при:
 * - втраті синхронізації;
 * - неправильному порядку пакетів;
 * - помилках реконструкції.
 */
static void reconstructor_drop_partial_frame(ReconstructorContext *ctx)
{
    thermal_frame_clear(&ctx->working_frame);

    memset(ctx->segment_received,
           0,
           sizeof(ctx->segment_received));

    ctx->synced = 0U;
    ctx->expected_segment = 0U;
    ctx->expected_packet = 0U;
    ctx->packets_in_current_frame = 0U;
}

/*
 * Перехід до очікуваного наступного пакета.
 *
 * Логіка:
 * - спочатку збільшується номер пакета;
 * - після завершення сегмента — номер сегмента.
 */
static void reconstructor_advance_expected(ReconstructorContext *ctx)
{
    if (ctx->expected_packet + 1U < VOSPI_PACKETS_PER_SEGMENT) {
        ctx->expected_packet++;
    } else {
        ctx->expected_packet = 0U;
        ctx->expected_segment++;
    }
}

/*
 * Перевірка відповідності поточного пакета
 * очікуваному сегменту та номеру пакета.
 */
static int packet_is_expected(const ReconstructorContext *ctx,
                              uint8_t segment,
                              uint8_t packet_number)
{
    return ((ctx != NULL) &&
            (segment == ctx->expected_segment) &&
            (packet_number == ctx->expected_packet));
}

/*
 * Перевірка, чи є пакет першим у кадрі.
 */
static int packet_is_start_of_frame(uint8_t segment,
                                    uint8_t packet_number)
{
    return ((segment == 0U) &&
            (packet_number == 0U));
}

/*
 * Перевірка, чи є пакет останнім у кадрі.
 */
static int packet_is_last_in_frame(uint8_t segment,
                                   uint8_t packet_number)
{
    return ((segment == (VOSPI_SEGMENT_COUNT - 1U)) &&
            (packet_number == (VOSPI_PACKETS_PER_SEGMENT - 1U)));
}

/*
 * Запис payload пакета у буфер теплового кадру.
 *
 * Один пакет містить половину рядка:
 * - ліва половина;
 * - права половина.
 *
 * Два пакети формують один повний рядок кадру.
 */
static int write_packet_to_frame(ThermalFrame *frame,
                                 uint8_t segment,
                                 uint8_t packet_number,
                                 const VoSPIPacket *packet)
{
    uint16_t global_packet_index;
    uint16_t row;
    uint16_t half;
    uint16_t base_x;

    uint16_t i;
    uint16_t pixel_value;
    uint16_t pixel_index;

    if ((frame == NULL) || (packet == NULL)) {
        return -1;
    }

    /*
     * Перетворення позиції пакета у:
     * - номер рядка;
     * - половину рядка.
     */
    global_packet_index =
        (uint16_t)(segment * VOSPI_PACKETS_PER_SEGMENT +
                   packet_number);

    row = (uint16_t)(global_packet_index / 2U);
    half = (uint16_t)(global_packet_index % 2U);

    if (row >= FRAME_HEIGHT) {
        return -1;
    }

    base_x = (uint16_t)(half * (FRAME_WIDTH / 2U));

    /*
     * Payload містить 16-бітні thermal values
     * у big-endian форматі.
     */
    for (i = 0; i < (FRAME_WIDTH / 2U); i++) {

        pixel_value =
            (uint16_t)(((uint16_t)packet->payload[2U * i] << 8U) |
                       (uint16_t)packet->payload[2U * i + 1U]);

        pixel_index =
            (uint16_t)(row * FRAME_WIDTH +
                       base_x +
                       i);

        frame->pixels[pixel_index] = pixel_value;
    }

    return 0;
}

/*
 * Ініціалізація контексту реконструктора.
 */
void reconstructor_init(ReconstructorContext *ctx)
{
    if (ctx == NULL) {
        return;
    }

    memset(ctx, 0, sizeof(*ctx));

    thermal_frame_clear(&ctx->working_frame);
    thermal_frame_clear(&ctx->ready_frame);
}

/*
 * Повне скидання реконструктора.
 */
void reconstructor_reset(ReconstructorContext *ctx)
{
    reconstructor_init(ctx);
}

/*
 * Основна функція обробки пакета.
 *
 * Виконує:
 * - розбір VoSPI header;
 * - перевірку синхронізації;
 * - перевірку порядку пакетів;
 * - реконструкцію кадру;
 * - формування готового кадру.
 */
int reconstructor_push_packet(ReconstructorContext *ctx,
                              const VoSPIPacket *packet)
{
    VoSPIPacketInfo info;

    uint8_t segment;
    uint8_t packet_number;

    if ((ctx == NULL) || (packet == NULL)) {
        return -1;
    }

    /*
     * Розбір VoSPI-подібного header.
     */
    info = vospi_parse_packet(packet);

    /*
     * Невалідний пакет.
     */
    if (!info.valid) {

        ctx->sync_errors++;
        ctx->dropped_packets++;

        reconstructor_drop_partial_frame(ctx);

        return 0;
    }

    /*
     * Discard packets ігноруються.
     */
    if (info.discard) {

        ctx->dropped_packets++;

        return 0;
    }

    segment = info.segment;
    packet_number = info.packet_number;

    /*
     * Якщо синхронізація ще не отримана —
     * очікується початок нового кадру.
     */
    if (!ctx->synced) {

        if (!packet_is_start_of_frame(segment,
                                      packet_number)) {
            return 0;
        }

        reconstructor_prepare_new_frame(ctx);
    }

    /*
     * Перевірка правильності порядку пакетів.
     */
    if (!packet_is_expected(ctx,
                            segment,
                            packet_number)) {

        ctx->sync_errors++;
        ctx->dropped_packets++;

        /*
         * Якщо отримано новий старт кадру —
         * починаємо реконструкцію заново.
         */
        if (packet_is_start_of_frame(segment,
                                     packet_number)) {

            reconstructor_prepare_new_frame(ctx);

        } else {

            reconstructor_drop_partial_frame(ctx);
            return 0;
        }
    }

    /*
     * Запис thermal payload у frame buffer.
     */
    if (write_packet_to_frame(&ctx->working_frame,
                              segment,
                              packet_number,
                              packet) != 0) {

        ctx->sync_errors++;
        ctx->dropped_packets++;

        reconstructor_drop_partial_frame(ctx);

        return 0;
    }

    ctx->packets_in_current_frame++;

    ctx->segment_received[segment] = 1U;

    /*
     * Якщо отримано останній пакет —
     * формуємо готовий кадр.
     */
    if (packet_is_last_in_frame(segment,
                                packet_number)) {

        ctx->working_frame.valid = 1U;

        ctx->working_frame.frame_number =
            ++ctx->frame_counter;

        thermal_frame_copy(&ctx->ready_frame,
                           &ctx->working_frame);

        ctx->ready_frame.valid = 1U;

        ctx->frame_ready = 1U;

        thermal_frame_clear(&ctx->working_frame);

        memset(ctx->segment_received,
               0,
               sizeof(ctx->segment_received));

        ctx->expected_segment = 0U;
        ctx->expected_packet = 0U;
        ctx->packets_in_current_frame = 0U;

        ctx->synced = 1U;

        return 0;
    }

    /*
     * Очікування наступного пакета.
     */
    reconstructor_advance_expected(ctx);

    return 0;
}

/*
 * Перевірка готовності кадру.
 */
int reconstructor_is_frame_ready(const ReconstructorContext *ctx)
{
    if (ctx == NULL) {
        return 0;
    }

    return (ctx->frame_ready != 0U);
}

/*
 * Копіювання готового кадру у вихідний буфер.
 */
int reconstructor_get_frame(ReconstructorContext *ctx,
                            ThermalFrame *out_frame)
{
    if ((ctx == NULL) || (out_frame == NULL)) {
        return -1;
    }

    if (!ctx->frame_ready) {
        return -1;
    }

    thermal_frame_copy(out_frame,
                       &ctx->ready_frame);

    ctx->frame_ready = 0U;

    return 0;
}