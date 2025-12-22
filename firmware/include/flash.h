/**
 * =============================================================================
 * @file    flash.h
 * @brief   Заголовочный файл драйвера Flash памяти
 * =============================================================================
 * 
 * Определения для работы с SPI Flash:
 * - Структуры данных
 * - Адреса разметки памяти
 * - Прототипы функций
 * 
 * @author  Reconstructed from Avalon A1126pro firmware
 * @version 1.0
 * =============================================================================
 */

#ifndef FLASH_H
#define FLASH_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* =============================================================================
 * РАЗМЕТКА FLASH ПАМЯТИ
 * ============================================================================= */

/**
 * @brief   Карта памяти Flash (8MB)
 * 
 * +----------------+ 0x000000
 * | Bootloader     | 64KB (kflash bootloader)
 * +----------------+ 0x010000
 * | Slot A         | 3.5MB (основная прошивка)
 * | (Firmware)     |
 * +----------------+ 0x380000
 * | Slot B         | 3.5MB (резерв для OTA)
 * | (OTA Update)   |
 * +----------------+ 0x6F0000
 * | Config         | 64KB (конфигурация)
 * +----------------+ 0x700000
 * | Web Resources  | 512KB (HTML/CSS/JS)
 * +----------------+ 0x780000
 * | Reserved       | 512KB (зарезервировано)
 * +----------------+ 0x800000
 */

/** Размер Bootloader области */
#define FLASH_BOOTLOADER_OFFSET     0x000000
#define FLASH_BOOTLOADER_SIZE       (64 * 1024)

/** Слот A - основная прошивка */
#define FLASH_SLOT_A_OFFSET         0x010000
#define FLASH_SLOT_SIZE             (3584 * 1024)   /* 3.5 MB */

/** Слот B - OTA обновление */
#define FLASH_SLOT_B_OFFSET         0x380000

/** Область конфигурации */
#define FLASH_CONFIG_OFFSET         0x6F0000
#define FLASH_CONFIG_SIZE           (64 * 1024)

/** Web ресурсы */
#define FLASH_WEB_OFFSET            0x700000
#define FLASH_WEB_SIZE              (512 * 1024)

/** Зарезервированная область */
#define FLASH_RESERVED_OFFSET       0x780000
#define FLASH_RESERVED_SIZE         (512 * 1024)

/** Общий размер Flash */
#define FLASH_TOTAL_SIZE            (8 * 1024 * 1024)

/* =============================================================================
 * МАГИЧЕСКИЕ ЧИСЛА И КОНСТАНТЫ
 * ============================================================================= */

/** Магическое число конфигурации */
#define CONFIG_MAGIC                0x41564C4E  /* "AVLN" */

/** Версия формата конфигурации */
#define CONFIG_VERSION              0x0100

/** Магическое число OTA образа */
#define OTA_MAGIC                   0x4F544131  /* "OTA1" */

/** Флаги OTA */
#define OTA_FLAG_NONE               0x00000000
#define OTA_FLAG_PENDING            0x50454E44  /* "PEND" */
#define OTA_FLAG_DONE               0x444F4E45  /* "DONE" */

/** Смещение флага OTA внутри конфигурации */
#define FLASH_OTA_FLAG_OFFSET       0x0F00

/* =============================================================================
 * СТРУКТУРЫ ДАННЫХ
 * ============================================================================= */

/**
 * @brief   Информация о Flash чипе
 */
typedef struct {
    uint8_t manufacturer_id;        /**< ID производителя */
    uint8_t memory_type;            /**< Тип памяти */
    uint8_t capacity_id;            /**< ID ёмкости */
    uint32_t total_size;            /**< Общий размер (байт) */
    uint32_t sector_size;           /**< Размер сектора (байт) */
    uint32_t page_size;             /**< Размер страницы (байт) */
    uint32_t block_size;            /**< Размер блока (байт) */
    uint32_t sector_count;          /**< Количество секторов */
} flash_info_t;

/**
 * @brief   Заголовок OTA образа
 */
typedef struct {
    uint32_t magic;                 /**< Магическое число OTA_MAGIC */
    uint32_t version;               /**< Версия прошивки */
    uint32_t image_size;            /**< Размер образа */
    uint32_t image_crc;             /**< CRC32 образа */
    uint32_t header_crc;            /**< CRC32 заголовка */
    char description[64];           /**< Описание версии */
    uint8_t reserved[184];          /**< Зарезервировано (до 256 байт) */
} ota_header_t;

/**
 * @brief   Конфигурация пула
 */
typedef struct {
    char url[128];                  /**< URL пула */
    char worker[64];                /**< Имя воркера */
    char password[64];              /**< Пароль */
} pool_config_t;

/**
 * @brief   Сетевая конфигурация
 */
typedef struct {
    uint8_t dhcp;                   /**< Использовать DHCP */
    uint8_t ip[4];                  /**< IP адрес */
    uint8_t netmask[4];             /**< Маска подсети */
    uint8_t gateway[4];             /**< Шлюз по умолчанию */
    uint8_t dns[4];                 /**< DNS сервер */
} network_config_t;

/**
 * @brief   Конфигурация майнера
 */
typedef struct {
    uint16_t frequency;             /**< Частота чипов (МГц) */
    uint16_t voltage;               /**< Напряжение (мВ) */
    uint8_t fan_speed;              /**< Скорость вентилятора (%) */
    uint8_t auto_fan;               /**< Авто-регулировка вентилятора */
    uint8_t target_temp;            /**< Целевая температура (°C) */
    uint8_t reserved[9];            /**< Зарезервировано */
} mining_config_t;

/**
 * @brief   Полная конфигурация системы
 */
typedef struct {
    uint16_t version;               /**< Версия конфигурации */
    pool_config_t pool;             /**< Настройки пула */
    pool_config_t pool_backup[2];   /**< Резервные пулы */
    network_config_t network;       /**< Сетевые настройки */
    mining_config_t miner;          /**< Настройки майнинга */
    uint8_t reserved[256];          /**< Зарезервировано */
} miner_config_t;

/* =============================================================================
 * ПРОТОТИПЫ ФУНКЦИЙ
 * ============================================================================= */

/**
 * @name    Инициализация
 * @{
 */

/**
 * @brief   Инициализация Flash памяти
 * @return  0 при успехе, <0 при ошибке
 */
int flash_init(void);

/**
 * @brief   Получение информации о Flash
 * @return  Указатель на структуру flash_info_t
 */
const flash_info_t *flash_get_info(void);

/** @} */

/**
 * @name    Базовые операции
 * @{
 */

/**
 * @brief   Чтение данных из Flash
 * @param   address: адрес во Flash
 * @param   data: буфер для данных
 * @param   length: количество байт
 * @return  Количество прочитанных байт, <0 при ошибке
 */
int flash_read(uint32_t address, void *data, size_t length);

/**
 * @brief   Запись данных во Flash
 * @param   address: адрес во Flash
 * @param   data: данные для записи
 * @param   length: количество байт
 * @return  Количество записанных байт, <0 при ошибке
 * @note    Сектор должен быть предварительно очищен!
 */
int flash_write(uint32_t address, const void *data, size_t length);

/**
 * @brief   Стирание сектора (4KB)
 * @param   address: адрес внутри сектора
 * @return  0 при успехе, <0 при ошибке
 */
int flash_erase_sector(uint32_t address);

/**
 * @brief   Стирание блока (64KB)
 * @param   address: адрес внутри блока
 * @return  0 при успехе, <0 при ошибке
 */
int flash_erase_block(uint32_t address);

/**
 * @brief   Стирание всего чипа
 * @return  0 при успехе, <0 при ошибке
 * @warning Занимает много времени!
 */
int flash_erase_chip(void);

/** @} */

/**
 * @name    OTA обновление
 * @{
 */

/**
 * @brief   Запись OTA образа в резервный слот
 * @param   image: данные образа
 * @param   size: размер образа
 * @return  0 при успехе, <0 при ошибке
 */
int flash_ota_write(const void *image, size_t size);

/**
 * @brief   Переключение на новую прошивку
 * @return  Не возвращается при успехе (перезагрузка)
 */
int flash_ota_switch(void);

/**
 * @brief   Подтверждение успешного обновления
 * @return  0 при успехе
 */
int flash_ota_commit(void);

/** @} */

/**
 * @name    Конфигурация
 * @{
 */

/**
 * @brief   Чтение конфигурации из Flash
 * @param   config: указатель на структуру для заполнения
 * @return  0 при успехе, <0 при ошибке
 */
int flash_config_read(miner_config_t *config);

/**
 * @brief   Запись конфигурации во Flash
 * @param   config: указатель на конфигурацию
 * @return  0 при успехе, <0 при ошибке
 */
int flash_config_write(const miner_config_t *config);

/**
 * @brief   Сброс конфигурации на значения по умолчанию
 * @return  0 при успехе
 */
int flash_config_reset(void);

/** @} */

#ifdef __cplusplus
}
#endif

#endif /* FLASH_H */
