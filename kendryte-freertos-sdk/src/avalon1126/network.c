/**
 * =============================================================================
 * @file    network.c
 * @brief   Avalon A1126pro - Сетевой модуль (реализация)
 * @version 1.0
 * @date    2024
 * =============================================================================
 * 
 * ОПИСАНИЕ:
 * Реализация сетевого модуля для DM9051 SPI Ethernet контроллера.
 * Использует lwIP TCP/IP стек.
 * 
 * =============================================================================
 */

#include <stdio.h>
#include <string.h>

#include "network.h"
#include "cgminer.h"

static const char *TAG = "Network";

static uint8_t s_ip_addr[4] = {0};
static int s_connected = 0;

/**
 * @brief Инициализация сети
 */
int network_init(void)
{
    log_message(LOG_INFO, "%s: Инициализация сети...", TAG);
    
    /* TODO: Инициализация lwIP и DM9051 */
    
    s_connected = 1;
    
    log_message(LOG_INFO, "%s: Сеть инициализирована", TAG);
    return 0;
}

/**
 * @brief Получение IP адреса
 */
int network_get_ip(uint8_t *ip)
{
    if (ip) {
        memcpy(ip, s_ip_addr, 4);
        return 0;
    }
    return -1;
}

/**
 * @brief Проверка состояния сети
 */
int network_is_connected(void)
{
    return s_connected;
}
