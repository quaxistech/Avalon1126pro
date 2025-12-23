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
    
    /* TODO: Чтение из flash через W25QXX */
    config_set_defaults(cfg);
    
    log_message(LOG_INFO, "%s: Конфигурация загружена", TAG);
    return 0;
}

int config_save(const cgminer_config_t *cfg)
{
    if (!cfg) return -1;
    
    /* TODO: Запись в flash через W25QXX */
    
    log_message(LOG_INFO, "%s: Конфигурация сохранена", TAG);
    return 0;
}
