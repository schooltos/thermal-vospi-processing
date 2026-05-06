#include "source_emulator.h"

#include <stdio.h>
#include <string.h>

#include "core/config.h"

/*
 * Глобальний файловий дескриптор емулятора.
 *
 * Емулятор відтворює поведінку тепловізора,
 * послідовно читаючи VoSPI-подібні пакети
 * із попередньо згенерованого .bin потоку.
 */
static FILE *g_emulator_file = NULL;

/*
 * Формат одного пакета у файлі:
 *
 * [0..3]     header      (4 bytes)
 * [4..163]   payload     (160 bytes)
 *
 * Загальний розмір пакета:
 * 164 байти
 *
 * Header використовується для:
 * - номера сегмента
 * - номера пакета
 * - discard-пакетів
 * - службових прапорців
 */

/*
 * Ініціалізація емулятора.
 *
 * Відкриває .bin файл із пакетом теплового потоку.
 *
 * Повертає:
 *   0  - успішно
 *  -1  - помилка
 */
int source_emulator_init(const char *path)
{
    if (path == NULL) {
        return -1;
    }

    /*
     * Якщо файл уже був відкритий —
     * закриваємо його перед повторною ініціалізацією.
     */
    if (g_emulator_file != NULL) {
        fclose(g_emulator_file);
        g_emulator_file = NULL;
    }

    g_emulator_file = fopen(path, "rb");
    if (g_emulator_file == NULL) {
        return -1;
    }

    return 0;
}

/*
 * Зчитування одного VoSPI-подібного пакета.
 *
 * Формат:
 *   4 bytes   - header
 *   160 bytes - payload
 *
 * Повертає:
 *   0  - пакет успішно прочитаний
 *   1  - досягнуто кінець файлу
 *  -1  - помилка читання
 */
int source_emulator_read_packet(VoSPIPacket *packet)
{
    if ((g_emulator_file == NULL) || (packet == NULL)) {
        return -1;
    }

    /*
     * Очищення структури перед заповненням.
     */
    memset(packet, 0, sizeof(*packet));

    /*
     * Зчитування VoSPI header.
     */
    if (fread(packet->header,
              1,
              VOSPI_HEADER_SIZE,
              g_emulator_file) != VOSPI_HEADER_SIZE) {

        if (feof(g_emulator_file)) {
            return 1;
        }

        return -1;
    }

    /*
     * Зчитування payload теплових даних.
     */
    if (fread(packet->payload,
              1,
              VOSPI_PAYLOAD_SIZE,
              g_emulator_file) != VOSPI_PAYLOAD_SIZE) {

        if (feof(g_emulator_file)) {
            return 1;
        }

        return -1;
    }

    return 0;
}

/*
 * Скидання позиції читання на початок потоку.
 *
 * Використовується для повторного прогону
 * того самого thermal stream.
 */
int source_emulator_reset(void)
{
    if (g_emulator_file == NULL) {
        return -1;
    }

    if (fseek(g_emulator_file, 0, SEEK_SET) != 0) {
        return -1;
    }

    return 0;
}

/*
 * Деініціалізація емулятора.
 *
 * Закриває файл і звільняє ресурси.
 */
void source_emulator_deinit(void)
{
    if (g_emulator_file != NULL) {
        fclose(g_emulator_file);
        g_emulator_file = NULL;
    }
}