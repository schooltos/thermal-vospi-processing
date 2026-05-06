#ifndef INPUT_FRAME_RECONSTRUCTOR_H
#define INPUT_FRAME_RECONSTRUCTOR_H

#include <stdint.h>
#include "core/types.h"
#include "core/config.h"
#include "input/vospi_parser.h"

/*
 * Спрощене кодування packet_id для програмної емуляції:
 *
 * bits 15..12 : flags
 * bits 11..8  : segment (0..3)
 * bits 7..0   : packet number inside segment (0..59)
 *
 * flags == 0xF -> discard packet
 */
#define VOSPI_ID_FLAG_MASK         0xF000U
#define VOSPI_ID_SEGMENT_MASK      0x0F00U
#define VOSPI_ID_PACKET_MASK       0x00FFU

#define VOSPI_ID_SEGMENT_SHIFT     8U
#define VOSPI_ID_DISCARD_FLAG      0xF000U

/*
 * Контекст реконструктора.
 * Зберігає робочий кадр, останній готовий кадр
 * та службову інформацію про поточний стан синхронізації.
 */
typedef struct {
    ThermalFrame working_frame;
    ThermalFrame ready_frame;

    uint8_t frame_ready;
    uint8_t synced;

    uint8_t expected_segment;
    uint8_t expected_packet;

    uint16_t packets_in_current_frame;
    uint8_t segment_received[VOSPI_SEGMENT_COUNT];

    uint32_t frame_counter;
    uint32_t sync_errors;
    uint32_t dropped_packets;
} ReconstructorContext;

/*
 * Ініціалізація реконструктора.
 */
void reconstructor_init(ReconstructorContext *ctx);

/*
 * Повний скидання стану реконструктора.
 */
void reconstructor_reset(ReconstructorContext *ctx);

/*
 * Передача одного пакета в реконструктор.
 *
 * Повертає:
 *   0  - пакет оброблено або проігноровано
 *  -1  - некоректний аргумент
 */
int reconstructor_push_packet(ReconstructorContext *ctx,
                              const VoSPIPacket *packet);

/*
 * Перевірка, чи готовий повний кадр.
 */
int reconstructor_is_frame_ready(const ReconstructorContext *ctx);

/*
 * Отримання готового кадру.
 *
 * Повертає:
 *   0  - кадр успішно скопійовано в out_frame
 *  -1  - помилка або кадр не готовий
 */
int reconstructor_get_frame(ReconstructorContext *ctx,
                            ThermalFrame *out_frame);

#endif /* INPUT_FRAME_RECONSTRUCTOR_H */