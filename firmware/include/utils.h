/**
 * =============================================================================
 * @file    utils.h
 * @brief   Заголовочный файл вспомогательных функций
 * =============================================================================
 * 
 * Определения для утилит:
 * - CRC функции
 * - Преобразования данных
 * - Работа со строками
 * - Отладочный вывод
 * 
 * @author  Reconstructed from Avalon A1126pro firmware
 * @version 1.0
 * =============================================================================
 */

#ifndef UTILS_H
#define UTILS_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* =============================================================================
 * УРОВНИ ОТЛАДКИ
 * ============================================================================= */

/**
 * @brief   Уровни отладочных сообщений
 */
typedef enum {
    DEBUG_NONE = 0,     /**< Отключить вывод */
    DEBUG_ERROR = 1,    /**< Только ошибки */
    DEBUG_WARNING = 2,  /**< Предупреждения и ошибки */
    DEBUG_INFO = 3,     /**< Информационные сообщения */
    DEBUG_DEBUG = 4,    /**< Отладочные сообщения */
    DEBUG_TRACE = 5     /**< Трассировка (максимум) */
} debug_level_t;

/* =============================================================================
 * ФУНКЦИИ CRC
 * ============================================================================= */

/**
 * @brief   Вычисление CRC16-CCITT
 * @param   data: указатель на данные
 * @param   length: длина данных
 * @return  CRC16
 */
uint16_t crc16_ccitt(const void *data, size_t length);

/**
 * @brief   Вычисление CRC32
 * @param   data: указатель на данные
 * @param   length: длина данных
 * @return  CRC32
 */
uint32_t crc32(const void *data, size_t length);

/**
 * @brief   Инкрементальное обновление CRC32
 * @param   crc: текущее значение CRC
 * @param   data: указатель на новые данные
 * @param   length: длина новых данных
 * @return  Обновлённое CRC32
 */
uint32_t crc32_update(uint32_t crc, const void *data, size_t length);

/* =============================================================================
 * ПРЕОБРАЗОВАНИЯ ДАННЫХ
 * ============================================================================= */

/**
 * @brief   Преобразование hex строки в байты
 * @param   hex: hex строка
 * @param   output: выходной буфер
 * @param   max_len: максимальная длина
 * @return  Количество байт или -1 при ошибке
 */
int hex_to_bytes(const char *hex, uint8_t *output, size_t max_len);

/**
 * @brief   Преобразование байтов в hex строку
 * @param   data: входные данные
 * @param   length: длина данных
 * @param   output: выходной буфер
 * @param   uppercase: заглавные буквы
 */
void bytes_to_hex(const uint8_t *data, size_t length, char *output, bool uppercase);

/**
 * @brief   Реверс байт в 32-битном слове
 */
uint32_t swap32(uint32_t value);

/**
 * @brief   Реверс байт в 64-битном слове
 */
uint64_t swap64(uint64_t value);

/* =============================================================================
 * ФУНКЦИИ ДЛЯ СТРОК
 * ============================================================================= */

/**
 * @brief   Безопасное копирование строки
 */
size_t safe_strcpy(char *dst, const char *src, size_t size);

/**
 * @brief   Безопасная конкатенация строк
 */
size_t safe_strcat(char *dst, const char *src, size_t size);

/**
 * @brief   Удаление пробелов в начале и конце
 */
char *str_trim(char *str);

/**
 * @brief   Парсинг целого числа
 */
bool parse_int(const char *str, int *value);

/**
 * @brief   Парсинг IP адреса
 */
bool parse_ip(const char *str, uint8_t ip[4]);

/* =============================================================================
 * ОТЛАДОЧНЫЙ ВЫВОД
 * ============================================================================= */

/**
 * @brief   Установка уровня отладки
 */
void debug_set_level(debug_level_t level);

/**
 * @brief   Отладочный вывод
 */
void debug_printf(debug_level_t level, const char *fmt, ...);

/**
 * @brief   Дамп памяти в hex
 */
void hex_dump(const void *data, size_t length, const char *prefix);

/* =============================================================================
 * ФОРМАТИРОВАНИЕ
 * ============================================================================= */

/**
 * @brief   Форматирование времени работы
 */
char *format_uptime(uint32_t seconds, char *buffer, size_t size);

/**
 * @brief   Форматирование хэшрейта
 */
char *format_hashrate(double hashrate, char *buffer, size_t size);

/**
 * @brief   Форматирование размера в байтах
 */
char *format_bytes(uint64_t bytes, char *buffer, size_t size);

/* =============================================================================
 * МАКРОСЫ ОТЛАДКИ
 * ============================================================================= */

/** Макросы для удобного вывода */
#define LOG_ERROR(...)   debug_printf(DEBUG_ERROR, __VA_ARGS__)
#define LOG_WARN(...)    debug_printf(DEBUG_WARNING, __VA_ARGS__)
#define LOG_INFO(...)    debug_printf(DEBUG_INFO, __VA_ARGS__)
#define LOG_DEBUG(...)   debug_printf(DEBUG_DEBUG, __VA_ARGS__)
#define LOG_TRACE(...)   debug_printf(DEBUG_TRACE, __VA_ARGS__)

/** Макрос для вывода с именем функции */
#define TRACE_ENTER()    LOG_TRACE(">>> %s", __func__)
#define TRACE_EXIT()     LOG_TRACE("<<< %s", __func__)

#ifdef __cplusplus
}
#endif

#endif /* UTILS_H */
