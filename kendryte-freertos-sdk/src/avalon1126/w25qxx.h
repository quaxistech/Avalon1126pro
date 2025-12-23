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

int w25qxx_init(void);
int w25qxx_read(uint32_t addr, uint8_t *buf, uint32_t len);
int w25qxx_write(uint32_t addr, const uint8_t *buf, uint32_t len);
int w25qxx_erase_sector(uint32_t addr);
uint32_t w25qxx_read_id(void);

#endif /* __W25QXX_H__ */
