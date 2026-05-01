#include "frame_reconstructor.h"

#include <string.h>
#include "core/frame.h"

/* -------------------- Внутрішні допоміжні функції -------------------- */

static int packet_is_discard(const VoSPIPacket *packet)
{
    if (packet == NULL) {
        return 0;
    }

    return ((packet->packet_id & VOSPI_ID_FLAG_MASK) == VOSPI_ID_DISCARD_FLAG);
}

static uint8_t packet_get_segment(const VoSPIPacket *packet)
{
    return (uint8_t)((packet->packet_id & VOSPI_ID_SEGMENT_MASK) >> VOSPI_ID_SEGMENT_SHIFT);
}

static uint8_t packet_get_number(const VoSPIPacket *packet)
{
    return (uint8_t)(packet->packet_id & VOSPI_ID_PACKET_MASK);
}

static int packet_header_valid(const VoSPIPacket *packet)
{
    uint8_t segment;
    uint8_t packet_number;

    if (packet == NULL) {
        return 0;
    }

    if (packet_is_discard(packet)) {
        return 1;
    }

    segment = packet_get_segment(packet);
    packet_number = packet_get_number(packet);

    if (segment >= VOSPI_SEGMENT_COUNT) {
        return 0;
    }

    if (packet_number >= VOSPI_PACKETS_PER_SEGMENT) {
        return 0;
    }

    return 1;
}

static void reconstructor_prepare_new_frame(ReconstructorContext *ctx)
{
    thermal_frame_clear(&ctx->working_frame);

    memset(ctx->segment_received, 0, sizeof(ctx->segment_received));

    ctx->synced = 1;
    ctx->expected_segment = 0;
    ctx->expected_packet = 0;
    ctx->packets_in_current_frame = 0;
}

static void reconstructor_drop_partial_frame(ReconstructorContext *ctx)
{
    thermal_frame_clear(&ctx->working_frame);

    memset(ctx->segment_received, 0, sizeof(ctx->segment_received));

    ctx->synced = 0;
    ctx->expected_segment = 0;
    ctx->expected_packet = 0;
    ctx->packets_in_current_frame = 0;
}

static void reconstructor_advance_expected(ReconstructorContext *ctx)
{
    if (ctx->expected_packet + 1U < VOSPI_PACKETS_PER_SEGMENT) {
        ctx->expected_packet++;
    } else {
        ctx->expected_packet = 0;
        ctx->expected_segment++;
    }
}

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
     * У payload 160 байтів = 80 пікселів по 16 біт.
     * На один рядок 160 пікселів -> 2 пакети на рядок.
     */
    global_packet_index = (uint16_t)(segment * VOSPI_PACKETS_PER_SEGMENT + packet_number);
    row = (uint16_t)(global_packet_index / 2U);
    half = (uint16_t)(global_packet_index % 2U);

    if (row >= FRAME_HEIGHT) {
        return -1;
    }

    base_x = (uint16_t)(half * (FRAME_WIDTH / 2U));

    for (i = 0; i < (FRAME_WIDTH / 2U); i++) {
        /*
         * Приймаємо big-endian представлення 16-бітного пікселя:
         * payload[2*i]   - старший байт
         * payload[2*i+1] - молодший байт
         */
        pixel_value = (uint16_t)(((uint16_t)packet->payload[2U * i] << 8U) |
                                  (uint16_t)packet->payload[2U * i + 1U]);

        pixel_index = (uint16_t)(row * FRAME_WIDTH + base_x + i);
        frame->pixels[pixel_index] = pixel_value;
    }

    return 0;
}

static int packet_is_expected(const ReconstructorContext *ctx,
                              uint8_t segment,
                              uint8_t packet_number)
{
    if (ctx == NULL) {
        return 0;
    }

    return ((segment == ctx->expected_segment) &&
            (packet_number == ctx->expected_packet));
}

static int packet_is_start_of_frame(uint8_t segment, uint8_t packet_number)
{
    return ((segment == 0U) && (packet_number == 0U));
}

static int packet_is_last_in_frame(uint8_t segment, uint8_t packet_number)
{
    return ((segment == (VOSPI_SEGMENT_COUNT - 1U)) &&
            (packet_number == (VOSPI_PACKETS_PER_SEGMENT - 1U)));
}

/* -------------------- Публічні функції -------------------- */

void reconstructor_init(ReconstructorContext *ctx)
{
    if (ctx == NULL) {
        return;
    }

    memset(ctx, 0, sizeof(*ctx));
    thermal_frame_clear(&ctx->working_frame);
    thermal_frame_clear(&ctx->ready_frame);
}

void reconstructor_reset(ReconstructorContext *ctx)
{
    if (ctx == NULL) {
        return;
    }

    memset(ctx, 0, sizeof(*ctx));
    thermal_frame_clear(&ctx->working_frame);
    thermal_frame_clear(&ctx->ready_frame);
}

int reconstructor_push_packet(ReconstructorContext *ctx,
                              const VoSPIPacket *packet)
{
    uint8_t segment;
    uint8_t packet_number;

    if ((ctx == NULL) || (packet == NULL)) {
        return -1;
    }

    if (!packet_header_valid(packet)) {
        ctx->sync_errors++;
        ctx->dropped_packets++;
        reconstructor_drop_partial_frame(ctx);
        return 0;
    }

    if (packet_is_discard(packet)) {
        ctx->dropped_packets++;
        return 0;
    }

    segment = packet_get_segment(packet);
    packet_number = packet_get_number(packet);

    /*
     * Якщо ще не синхронізовані,
     * чекаємо на початок нового кадру.
     */
    if (!ctx->synced) {
        if (!packet_is_start_of_frame(segment, packet_number)) {
            return 0;
        }

        reconstructor_prepare_new_frame(ctx);
    }

    /*
     * Якщо прийшов не той пакет, який очікувався,
     * вважаємо потік порушеним.
     */
    if (!packet_is_expected(ctx, segment, packet_number)) {
        ctx->sync_errors++;
        ctx->dropped_packets++;

        /*
         * Якщо в потоці раптом з’явився новий початок кадру,
         * одразу починаємо синхронізацію з нього.
         */
        if (packet_is_start_of_frame(segment, packet_number)) {
            reconstructor_prepare_new_frame(ctx);
        } else {
            reconstructor_drop_partial_frame(ctx);
            return 0;
        }
    }

    if (write_packet_to_frame(&ctx->working_frame, segment, packet_number, packet) != 0) {
        ctx->sync_errors++;
        ctx->dropped_packets++;
        reconstructor_drop_partial_frame(ctx);
        return 0;
    }

    ctx->packets_in_current_frame++;
    ctx->segment_received[segment] = 1U;

    /*
     * Якщо це останній пакет кадру — формуємо готовий кадр.
     */
    if (packet_is_last_in_frame(segment, packet_number)) {
        ctx->working_frame.valid = 1U;
        ctx->working_frame.frame_number = ++ctx->frame_counter;

        thermal_frame_copy(&ctx->ready_frame, &ctx->working_frame);
        ctx->ready_frame.valid = 1U;
        ctx->frame_ready = 1U;

        /*
         * Готуємося до наступного кадру.
         * Синхронізацію не скидаємо повністю —
         * просто чекаємо знову пакет 0/0 як наступний початок кадру.
         */
        thermal_frame_clear(&ctx->working_frame);
        memset(ctx->segment_received, 0, sizeof(ctx->segment_received));

        ctx->expected_segment = 0U;
        ctx->expected_packet = 0U;
        ctx->packets_in_current_frame = 0U;
        ctx->synced = 1U;

        return 0;
    }

    reconstructor_advance_expected(ctx);
    return 0;
}

int reconstructor_is_frame_ready(const ReconstructorContext *ctx)
{
    if (ctx == NULL) {
        return 0;
    }

    return (ctx->frame_ready != 0U);
}

int reconstructor_get_frame(ReconstructorContext *ctx,
                            ThermalFrame *out_frame)
{
    if ((ctx == NULL) || (out_frame == NULL)) {
        return -1;
    }

    if (!ctx->frame_ready) {
        return -1;
    }

    thermal_frame_copy(out_frame, &ctx->ready_frame);
    ctx->frame_ready = 0U;

    return 0;
}