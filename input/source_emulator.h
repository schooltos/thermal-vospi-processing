#ifndef INPUT_SOURCE_EMULATOR_H
#define INPUT_SOURCE_EMULATOR_H

#include "core/types.h"

/*
 * Ініціалізація емулятора.
 * path - шлях до бінарного файлу з пакетами.
 *
 * Повертає:
 *   0  - успіх
 *  -1  - помилка
 */
int source_emulator_init(const char *path);

/*
 * Читання одного пакета з файлу.
 *
 * Повертає:
 *   0  - пакет успішно прочитано
 *   1  - досягнуто кінець файлу
 *  -1  - помилка
 */
int source_emulator_read_packet(VoSPIPacket *packet);

/*
 * Скидання читання на початок файлу.
 *
 * Повертає:
 *   0  - успіх
 *  -1  - помилка
 */
int source_emulator_reset(void);

/*
 * Закриття емулятора і файлу.
 */
void source_emulator_deinit(void);

#endif /* INPUT_SOURCE_EMULATOR_H */