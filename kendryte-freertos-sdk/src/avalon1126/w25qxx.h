/**
 * =============================================================================
 * @file    w25qxx.h
 * @brief   Avalon A1126pro - Драйвер W25Q64 SPI Flash (заголовочный файл)
 * @version 1.0
 * @date    2024
 * =============================================================================
 * 
 * ОПИСАНИЕ:
 * Драйвер для SPI флеш-памяти Winbond W25Q64 (8 МБ).
 * Используется для хранения конфигурации и логов.
 * 
 * =============================================================================
 */

#ifndef __W25QXX_H__
#define __W25QXX_H__

#include <stdint.h>

#define W25QXX_SECTOR_SIZE      4096    /* Размер сектора (4 КБ) */
#define W25QXX_PAGE_SIZE        256     /* Размер страницы */
#define W25QXX_TOTAL_SIZE       (8*1024*1024)  /* 8 МБ */

/* Адреса разделов во flash */
#define W25QXX_CONFIG_ADDR      0x000000    /* Конфигурация (64 KB) */
#define W25QXX_CONFIG_BACKUP    0x010000    /* Резервная конфигурация */
#define W25QXX_LOG_ADDR         0x020000    /* Логи (256 KB) */
#define W25QXX_FIRMWARE_ADDR    0x100000    /* Область прошивки */

/**
 * @brief Инициализация драйвера W25QXX
 * @return 0 при успехе, -1 при ошибке
 */
int w25qxx_init(void);

/**
 * @brief Чтение данных из Flash
 * @param addr  Адрес начала чтения
 * @param buf   Буфер для данных
 * @param len   Количество байт для чтения
 * @return 0 при успехе, -1 при ошибке
 */
int w25qxx_read(uint32_t addr, uint8_t *buf, uint32_t len);

/**
 * @brief Запись данных в Flash
 * @param addr  Адрес начала записи
 * @param buf   Данные для записи
 * @param len   Количество байт для записи
 * @return 0 при успехе, -1 при ошибке
 */
int w25qxx_write(uint32_t addr, const uint8_t *buf, uint32_t len);

/**
 * @brief Стирание сектора (4KB)
 * @param addr  Адрес внутри сектора
 * @return 0 при успехе, -1 при ошибке
 */
int w25qxx_erase_sector(uint32_t addr);

/**
 * @brief Стирание блока (64KB)
 * @param addr  Адрес внутри блока
 * @return 0 при успехе, -1 при ошибке
 */
int w25qxx_erase_block(uint32_t addr);

/**
 * @brief Полное стирание чипа
 * @return 0 при успехе, -1 при ошибке
 */
int w25qxx_erase_chip(void);

/**
 * @brief Чтение JEDEC ID чипа
 * @return JEDEC ID (24 бита) или 0 при ошибке
 */
uint32_t w25qxx_read_id(void);

/**
 * @brief Проверка инициализации драйвера
 * @return 1 если инициализирован, 0 иначе
 */
int w25qxx_is_initialized(void);

/**
 * @brief Получение ID чипа (после инициализации)
 * @return JEDEC ID чипа
 */
uint32_t w25qxx_get_chip_id(void);

#endif /* __W25QXX_H__ */
