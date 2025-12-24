/**
 * =============================================================================
 * @file    config.c
 * @brief   Avalon A1126pro - Управление конфигурацией (реализация)
 * @version 1.0
 * @date    2024
 * =============================================================================
 * 
 * ОПИСАНИЕ:
 * Работа с конфигурацией, хранящейся в flash памяти W25Q64.
 * 
 * =============================================================================
 */

#include <stdio.h>
#include <string.h>

#include "config.h"
#include "cgminer.h"
#include "w25qxx.h"

static const char *TAG = "Config";

void config_set_defaults(cgminer_config_t *cfg)
{
    memset(cfg, 0, sizeof(cgminer_config_t));
    
    cfg->dhcp_enabled = 1;
    cfg->api_port = 4028;
    cfg->api_listen = 1;
    cfg->http_port = 80;
    cfg->http_enabled = 1;
    cfg->log_level = LOG_NOTICE;
    cfg->pool_strategy = POOL_STRATEGY_FAILOVER;
    cfg->freq[0] = 400;
    cfg->freq[1] = 500;
    cfg->voltage = 780;
    cfg->fan_min = 10;
    cfg->fan_max = 100;
    cfg->temp_target = 75;
    cfg->temp_overheat = 95;
    cfg->temp_cutoff = 105;
    cfg->config_version = 1;
}

void config_init(void)
{
    log_message(LOG_INFO, "%s: Инициализация конфигурации", TAG);
    
    if (config_load(&g_config) < 0) {
        log_message(LOG_WARNING, "%s: Загрузка по умолчанию", TAG);
        config_set_defaults(&g_config);
    }
}

int config_load(cgminer_config_t *cfg)
{
    if (!cfg) return -1;
    
    /* Адрес конфигурации в flash (последний сектор 4KB от 8MB - 4KB) */
    #define CONFIG_FLASH_ADDR   (8 * 1024 * 1024 - 4096)
    #define CONFIG_MAGIC        0x41564C4E  /* "AVLN" */
    
    uint32_t magic = 0;
    uint32_t crc_stored = 0;
    uint32_t crc_calc = 0;
    
    /* Читаем magic */
    if (w25qxx_read(CONFIG_FLASH_ADDR, (uint8_t *)&magic, sizeof(magic)) != 0) {
        log_message(LOG_WARNING, "%s: Ошибка чтения flash", TAG);
        config_set_defaults(cfg);
        return -1;
    }
    
    /* Проверяем magic */
    if (magic != CONFIG_MAGIC) {
        log_message(LOG_WARNING, "%s: Конфигурация не найдена (magic=0x%08X)", TAG, magic);
        config_set_defaults(cfg);
        return -1;
    }
    
    /* Читаем конфигурацию */
    if (w25qxx_read(CONFIG_FLASH_ADDR + 4, (uint8_t *)cfg, sizeof(*cfg)) != 0) {
        log_message(LOG_WARNING, "%s: Ошибка чтения конфигурации", TAG);
        config_set_defaults(cfg);
        return -1;
    }
    
    /* Читаем и проверяем CRC */
    w25qxx_read(CONFIG_FLASH_ADDR + 4 + sizeof(*cfg), (uint8_t *)&crc_stored, sizeof(crc_stored));
    
    /* Простая контрольная сумма */
    uint8_t *p = (uint8_t *)cfg;
    for (size_t i = 0; i < sizeof(*cfg); i++) {
        crc_calc += p[i];
    }
    
    if (crc_calc != crc_stored) {
        log_message(LOG_WARNING, "%s: CRC ошибка (0x%08X != 0x%08X)", TAG, crc_calc, crc_stored);
        config_set_defaults(cfg);
        return -1;
    }
    
    log_message(LOG_INFO, "%s: Конфигурация загружена из flash", TAG);
    return 0;
}

int config_save(const cgminer_config_t *cfg)
{
    if (!cfg) return -1;
    
    #define CONFIG_FLASH_ADDR   (8 * 1024 * 1024 - 4096)
    #define CONFIG_MAGIC        0x41564C4E  /* "AVLN" */
    
    uint32_t magic = CONFIG_MAGIC;
    uint32_t crc = 0;
    
    /* Вычисляем контрольную сумму */
    const uint8_t *p = (const uint8_t *)cfg;
    for (size_t i = 0; i < sizeof(*cfg); i++) {
        crc += p[i];
    }
    
    /* Стираем сектор */
    if (w25qxx_erase_sector(CONFIG_FLASH_ADDR) != 0) {
        log_message(LOG_ERR, "%s: Ошибка стирания flash", TAG);
        return -1;
    }
    
    /* Записываем magic */
    if (w25qxx_write(CONFIG_FLASH_ADDR, (const uint8_t *)&magic, sizeof(magic)) != 0) {
        log_message(LOG_ERR, "%s: Ошибка записи magic", TAG);
        return -1;
    }
    
    /* Записываем конфигурацию */
    if (w25qxx_write(CONFIG_FLASH_ADDR + 4, (const uint8_t *)cfg, sizeof(*cfg)) != 0) {
        log_message(LOG_ERR, "%s: Ошибка записи конфигурации", TAG);
        return -1;
    }
    
    /* Записываем CRC */
    if (w25qxx_write(CONFIG_FLASH_ADDR + 4 + sizeof(*cfg), (const uint8_t *)&crc, sizeof(crc)) != 0) {
        log_message(LOG_ERR, "%s: Ошибка записи CRC", TAG);
        return -1;
    }
    
    log_message(LOG_INFO, "%s: Конфигурация сохранена в flash", TAG);
    return 0;
}
