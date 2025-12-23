/**
 * =============================================================================
 * @file    config.h
 * @brief   Avalon A1126pro - Управление конфигурацией (заголовочный файл)
 * @version 1.0
 * @date    2024
 * =============================================================================
 */

#ifndef __CONFIG_H__
#define __CONFIG_H__

#include "cgminer.h"

void config_init(void);
int config_load(cgminer_config_t *cfg);
int config_save(const cgminer_config_t *cfg);
void config_set_defaults(cgminer_config_t *cfg);

#endif /* __CONFIG_H__ */
