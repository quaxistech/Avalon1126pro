/**
 * =============================================================================
 * @file    fpga_loader.h
 * @brief   Avalon A1126pro - FPGA Bitstream Loader
 * @version 1.0
 * @date    2024
 * =============================================================================
 * 
 * ОПИСАНИЕ:
 * Драйвер для загрузки bitstream в FPGA на модулях Avalon A1126pro.
 * FPGA используется для интерфейса между K210 и ASIC чипами.
 * 
 * ПРОЦЕСС ЗАГРУЗКИ:
 * 1. Сброс FPGA (PROG_B low)
 * 2. Ожидание INIT_B high
 * 3. Передача bitstream по SPI/UART
 * 4. Ожидание DONE high
 * 5. Верификация
 * 
 * ХРАНЕНИЕ BITSTREAM:
 * - Flash: offset 0x300000, размер до 2MB
 * - Формат: сырой .bin или .bit с заголовком
 * 
 * =============================================================================
 */

#ifndef __FPGA_LOADER_H__
#define __FPGA_LOADER_H__

#include <stdint.h>
#include <stdbool.h>

/* ===========================================================================
 * КОНСТАНТЫ КОНФИГУРАЦИИ
 * =========================================================================== */

/**
 * @brief Адрес bitstream во flash
 */
#define FPGA_BITSTREAM_ADDR         0x300000

/**
 * @brief Максимальный размер bitstream (2MB)
 */
#define FPGA_BITSTREAM_MAX_SIZE     (2 * 1024 * 1024)

/**
 * @brief Размер буфера для загрузки
 */
#define FPGA_LOAD_BUFFER_SIZE       4096

/**
 * @brief Магическое число в заголовке bitstream
 */
#define FPGA_BITSTREAM_MAGIC        0x46504741  /* "FPGA" */

/**
 * @brief Версия формата заголовка
 */
#define FPGA_HEADER_VERSION         1

/* ---------------------------------------------------------------------------
 * GPIO пины для управления FPGA
 * --------------------------------------------------------------------------- */

/**
 * @brief GPIO для PROG_B (программирование, активный LOW)
 */
#define FPGA_PROG_B_GPIO            14

/**
 * @brief GPIO для INIT_B (готовность к программированию)
 */
#define FPGA_INIT_B_GPIO            15

/**
 * @brief GPIO для DONE (завершение программирования)
 */
#define FPGA_DONE_GPIO              16

/**
 * @brief GPIO для CCLK (clock для bitstream)
 */
#define FPGA_CCLK_GPIO              17

/**
 * @brief GPIO для DIN (data для bitstream)
 */
#define FPGA_DIN_GPIO               18

/* ---------------------------------------------------------------------------
 * Таймауты (мс)
 * --------------------------------------------------------------------------- */

#define FPGA_RESET_DELAY            10      /**< Задержка сброса */
#define FPGA_INIT_TIMEOUT           1000    /**< Ожидание INIT_B */
#define FPGA_DONE_TIMEOUT           5000    /**< Ожидание DONE */

/* ---------------------------------------------------------------------------
 * Коды ошибок
 * --------------------------------------------------------------------------- */

#define FPGA_OK                     0
#define FPGA_ERR_INIT               -1      /**< Ошибка инициализации */
#define FPGA_ERR_TIMEOUT            -2      /**< Таймаут */
#define FPGA_ERR_FLASH_READ         -3      /**< Ошибка чтения flash */
#define FPGA_ERR_INVALID_BITSTREAM  -4      /**< Неверный bitstream */
#define FPGA_ERR_VERIFY             -5      /**< Ошибка верификации */
#define FPGA_ERR_NOT_DONE           -6      /**< DONE не поднялся */
#define FPGA_ERR_NO_BITSTREAM       -7      /**< Bitstream не найден */

/* ===========================================================================
 * СТРУКТУРЫ ДАННЫХ
 * =========================================================================== */

/**
 * @brief Заголовок bitstream во flash
 * 
 * Структура хранится в начале области bitstream во flash.
 */
typedef struct __attribute__((packed)) {
    uint32_t magic;             /**< Магическое число FPGA_BITSTREAM_MAGIC */
    uint32_t version;           /**< Версия формата */
    uint32_t size;              /**< Размер bitstream в байтах */
    uint32_t crc32;             /**< CRC32 bitstream данных */
    uint32_t timestamp;         /**< Unix timestamp создания */
    char     device[16];        /**< Название устройства (например "XC7A35T") */
    char     build_info[32];    /**< Информация о сборке */
    uint8_t  reserved[56];      /**< Резерв до 128 байт */
} fpga_bitstream_header_t;

/**
 * @brief Состояние FPGA
 */
typedef enum {
    FPGA_STATE_UNKNOWN = 0,     /**< Неизвестно */
    FPGA_STATE_RESET,           /**< В состоянии сброса */
    FPGA_STATE_PROGRAMMING,     /**< Идёт программирование */
    FPGA_STATE_CONFIGURED,      /**< Сконфигурирован */
    FPGA_STATE_ERROR            /**< Ошибка */
} fpga_state_t;

/**
 * @brief Информация о FPGA
 */
typedef struct {
    fpga_state_t state;                 /**< Текущее состояние */
    bool bitstream_valid;               /**< Bitstream валиден во flash */
    uint32_t bitstream_size;            /**< Размер bitstream */
    uint32_t bitstream_crc;             /**< CRC32 bitstream */
    uint32_t load_time_ms;              /**< Время загрузки в мс */
    uint32_t load_count;                /**< Количество загрузок */
    char device_name[16];               /**< Название FPGA */
} fpga_info_t;

/* ===========================================================================
 * ПРОТОТИПЫ ФУНКЦИЙ
 * =========================================================================== */

/**
 * @brief Инициализация FPGA loader
 * 
 * Настраивает GPIO для управления FPGA.
 * 
 * @return FPGA_OK при успехе, код ошибки при неудаче
 */
int fpga_init(void);

/**
 * @brief Загрузка bitstream из flash
 * 
 * Читает bitstream из flash и загружает в FPGA.
 * 
 * @return FPGA_OK при успехе, код ошибки при неудаче
 */
int fpga_load_bitstream(void);

/**
 * @brief Загрузка bitstream из буфера памяти
 * 
 * @param data      Указатель на данные bitstream
 * @param size      Размер данных в байтах
 * @return FPGA_OK при успехе, код ошибки при неудаче
 */
int fpga_load_from_buffer(const uint8_t *data, uint32_t size);

/**
 * @brief Сброс FPGA
 * 
 * Переводит FPGA в состояние сброса.
 * 
 * @return FPGA_OK при успехе
 */
int fpga_reset(void);

/**
 * @brief Проверка готовности FPGA
 * 
 * @return true если FPGA сконфигурирован и готов
 */
bool fpga_is_ready(void);

/**
 * @brief Получение информации о FPGA
 * 
 * @return Указатель на структуру информации (только чтение)
 */
const fpga_info_t *fpga_get_info(void);

/**
 * @brief Проверка наличия bitstream во flash
 * 
 * Читает заголовок и проверяет CRC.
 * 
 * @return true если bitstream валиден
 */
bool fpga_check_bitstream(void);

/**
 * @brief Запись bitstream во flash
 * 
 * @param data      Указатель на данные bitstream
 * @param size      Размер данных в байтах
 * @return FPGA_OK при успехе, код ошибки при неудаче
 */
int fpga_write_bitstream(const uint8_t *data, uint32_t size);

/**
 * @brief Стирание bitstream из flash
 * 
 * @return FPGA_OK при успехе
 */
int fpga_erase_bitstream(void);

/**
 * @brief Получение строки ошибки
 * 
 * @param error_code    Код ошибки
 * @return Строка с описанием ошибки
 */
const char *fpga_error_string(int error_code);

#endif /* __FPGA_LOADER_H__ */
