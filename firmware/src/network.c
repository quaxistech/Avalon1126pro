/**
 * =============================================================================
 * @file    network.c
 * @brief   Сетевой стек (lwIP) для Avalon A1126pro
 * =============================================================================
 * 
 * Реализация сетевых функций на базе lwIP:
 * - Инициализация Ethernet
 * - DHCP клиент
 * - Статическая IP конфигурация
 * - Сетевые утилиты
 * 
 * @author  Reconstructed from Avalon A1126pro firmware
 * @version 1.0
 * =============================================================================
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>

#include "config.h"
#include "network.h"
#include "k210_hal.h"

/* lwIP заголовки */
#include "lwip/init.h"
#include "lwip/netif.h"
#include "lwip/dhcp.h"
#include "lwip/dns.h"
#include "lwip/tcp.h"
#include "lwip/timeouts.h"
#include "netif/ethernet.h"

/* FreeRTOS */
#ifdef USE_FREERTOS
#include "FreeRTOS.h"
#include "task.h"
#include "semphr.h"
#endif

/* =============================================================================
 * ЛОКАЛЬНЫЕ ПЕРЕМЕННЫЕ
 * ============================================================================= */

/** Сетевой интерфейс lwIP */
static struct netif main_netif;

/** Флаг инициализации сети */
static bool network_initialized = false;

/** Флаг использования DHCP */
static bool use_dhcp = true;

/** Сохранённая конфигурация сети */
static network_config_t net_config = {
    .dhcp = true,
    .ip = {192, 168, 1, 100},
    .netmask = {255, 255, 255, 0},
    .gateway = {192, 168, 1, 1},
    .dns = {8, 8, 8, 8},
    .hostname = "avalon"
};

/** MAC адрес */
static uint8_t mac_address[6] = {0x00, 0x00, 0x00, 0x00, 0x00, 0x00};

/* =============================================================================
 * ФУНКЦИИ СЕТЕВОГО ИНТЕРФЕЙСА
 * ============================================================================= */

/**
 * @brief Callback при изменении статуса сетевого интерфейса
 * 
 * @param netif Сетевой интерфейс
 */
static void netif_status_callback(struct netif *netif)
{
    if (netif_is_up(netif)) {
        char ip_str[16];
        ipaddr_ntoa_r(&netif->ip_addr, ip_str, sizeof(ip_str));
        applog(LOG_INFO, "Сеть: интерфейс активен, IP: %s", ip_str);
    } else {
        applog(LOG_INFO, "Сеть: интерфейс неактивен");
    }
}

/**
 * @brief Инициализация MAC адреса
 * 
 * Генерирует уникальный MAC адрес на основе серийного номера устройства.
 */
static void init_mac_address(void)
{
    /* Читаем уникальный ID из OTP или генерируем на основе чего-то */
    /* Первые 3 байта - OUI (Organizationally Unique Identifier) */
    /* Используем локально администрируемый адрес */
    
    mac_address[0] = 0x02;  /* Локально администрируемый, unicast */
    mac_address[1] = 0x00;
    mac_address[2] = 0x00;
    
    /* Последние 3 байта - уникальные для устройства */
    /* В реальности нужно брать из OTP или серийника */
    uint32_t unique_id = 0x12345678;  /* TODO: Получить реальный ID */
    mac_address[3] = (unique_id >> 16) & 0xFF;
    mac_address[4] = (unique_id >> 8) & 0xFF;
    mac_address[5] = unique_id & 0xFF;
}

/**
 * @brief Callback инициализации сетевого интерфейса
 * 
 * @param netif Сетевой интерфейс
 * @return ERR_OK при успехе
 */
static err_t netif_init_callback(struct netif *netif)
{
    /* Устанавливаем имя интерфейса */
    netif->name[0] = 'e';
    netif->name[1] = 'n';
    
    /* Устанавливаем MAC адрес */
    netif->hwaddr_len = 6;
    memcpy(netif->hwaddr, mac_address, 6);
    
    /* Устанавливаем MTU */
    netif->mtu = 1500;
    
    /* Устанавливаем флаги */
    netif->flags = NETIF_FLAG_BROADCAST | NETIF_FLAG_ETHARP | NETIF_FLAG_LINK_UP;
    
    /* Устанавливаем функцию вывода */
    netif->output = etharp_output;
    netif->linkoutput = low_level_output;
    
    applog(LOG_INFO, "Сетевой интерфейс инициализирован");
    
    return ERR_OK;
}

/**
 * @brief Низкоуровневая функция отправки пакета
 * 
 * @param netif Сетевой интерфейс
 * @param p Буфер пакета
 * @return ERR_OK при успехе
 */
static err_t low_level_output(struct netif *netif, struct pbuf *p)
{
    /* TODO: Реализовать отправку через Ethernet контроллер */
    
    /* Копируем данные из pbuf в буфер передачи */
    struct pbuf *q;
    uint8_t *buf = NULL;  /* Буфер передачи Ethernet контроллера */
    int offset = 0;
    
    for (q = p; q != NULL; q = q->next) {
        /* Копируем данные */
        memcpy(buf + offset, q->payload, q->len);
        offset += q->len;
    }
    
    /* Запускаем передачу */
    /* eth_transmit(buf, offset); */
    
    return ERR_OK;
}

/**
 * @brief Низкоуровневая функция приёма пакета
 * 
 * @param netif Сетевой интерфейс
 * @return Буфер с принятым пакетом или NULL
 */
static struct pbuf* low_level_input(struct netif *netif)
{
    struct pbuf *p = NULL;
    
    /* TODO: Реализовать приём через Ethernet контроллер */
    
    /* Проверяем есть ли принятые пакеты */
    int len = 0;  /* eth_get_received_length(); */
    
    if (len > 0) {
        /* Выделяем pbuf */
        p = pbuf_alloc(PBUF_RAW, len, PBUF_POOL);
        
        if (p != NULL) {
            /* Копируем данные в pbuf */
            uint8_t *buf = NULL;  /* eth_get_received_buffer(); */
            pbuf_take(p, buf, len);
        }
    }
    
    return p;
}

/* =============================================================================
 * ОСНОВНЫЕ ФУНКЦИИ
 * ============================================================================= */

/**
 * @brief Инициализация сетевого стека
 * 
 * @return 0 при успехе, <0 при ошибке
 */
int network_init(void)
{
    applog(LOG_INFO, "Инициализация сетевого стека...");
    
    /* Генерируем MAC адрес */
    init_mac_address();
    
    /* Инициализируем lwIP */
    lwip_init();
    
    /* Настраиваем IP адреса */
    ip4_addr_t ipaddr, netmask, gw;
    
    if (net_config.dhcp) {
        /* При DHCP начинаем с нулевых адресов */
        IP4_ADDR(&ipaddr, 0, 0, 0, 0);
        IP4_ADDR(&netmask, 0, 0, 0, 0);
        IP4_ADDR(&gw, 0, 0, 0, 0);
    } else {
        /* Статическая конфигурация */
        IP4_ADDR(&ipaddr, 
                 net_config.ip[0], net_config.ip[1],
                 net_config.ip[2], net_config.ip[3]);
        IP4_ADDR(&netmask,
                 net_config.netmask[0], net_config.netmask[1],
                 net_config.netmask[2], net_config.netmask[3]);
        IP4_ADDR(&gw,
                 net_config.gateway[0], net_config.gateway[1],
                 net_config.gateway[2], net_config.gateway[3]);
    }
    
    /* Добавляем сетевой интерфейс */
    netif_add(&main_netif, &ipaddr, &netmask, &gw, NULL,
              netif_init_callback, ethernet_input);
    
    /* Устанавливаем как интерфейс по умолчанию */
    netif_set_default(&main_netif);
    
    /* Устанавливаем callback статуса */
    netif_set_status_callback(&main_netif, netif_status_callback);
    
    /* Активируем интерфейс */
    netif_set_up(&main_netif);
    
    /* Запускаем DHCP если нужно */
    if (net_config.dhcp) {
        applog(LOG_INFO, "Запуск DHCP клиента...");
        dhcp_start(&main_netif);
        use_dhcp = true;
    } else {
        use_dhcp = false;
    }
    
    /* Настраиваем DNS */
    ip4_addr_t dns_addr;
    IP4_ADDR(&dns_addr,
             net_config.dns[0], net_config.dns[1],
             net_config.dns[2], net_config.dns[3]);
    dns_setserver(0, &dns_addr);
    
    network_initialized = true;
    
    applog(LOG_INFO, "Сетевой стек инициализирован");
    applog(LOG_INFO, "MAC: %02X:%02X:%02X:%02X:%02X:%02X",
           mac_address[0], mac_address[1], mac_address[2],
           mac_address[3], mac_address[4], mac_address[5]);
    
    return 0;
}

/**
 * @brief Обработка сетевого стека
 * 
 * Должна вызываться периодически из сетевой задачи.
 */
void network_poll(void)
{
    if (!network_initialized) {
        return;
    }
    
    /* Проверяем входящие пакеты */
    struct pbuf *p = low_level_input(&main_netif);
    if (p != NULL) {
        /* Передаём пакет в стек lwIP */
        if (main_netif.input(p, &main_netif) != ERR_OK) {
            pbuf_free(p);
        }
    }
    
    /* Обрабатываем таймауты lwIP */
    sys_check_timeouts();
}

/**
 * @brief Проверка подключения к сети
 * 
 * @return true если есть IP адрес
 */
bool network_is_connected(void)
{
    if (!network_initialized) {
        return false;
    }
    
    /* Проверяем есть ли IP адрес */
    return !ip4_addr_isany_val(*netif_ip4_addr(&main_netif));
}

/**
 * @brief Проверка использования DHCP
 * 
 * @return true если используется DHCP
 */
bool network_is_dhcp(void)
{
    return use_dhcp;
}

/* =============================================================================
 * ФУНКЦИИ ПОЛУЧЕНИЯ ИНФОРМАЦИИ
 * ============================================================================= */

/**
 * @brief Получение IP адреса
 * 
 * @param ip_str [out] Строка для IP адреса (минимум 16 символов)
 */
void network_get_ip(char *ip_str)
{
    if (!network_initialized) {
        strcpy(ip_str, "0.0.0.0");
        return;
    }
    
    ipaddr_ntoa_r(&main_netif.ip_addr, ip_str, 16);
}

/**
 * @brief Получение маски подсети
 * 
 * @param netmask_str [out] Строка для маски (минимум 16 символов)
 */
void network_get_netmask(char *netmask_str)
{
    if (!network_initialized) {
        strcpy(netmask_str, "0.0.0.0");
        return;
    }
    
    ipaddr_ntoa_r(&main_netif.netmask, netmask_str, 16);
}

/**
 * @brief Получение шлюза
 * 
 * @param gateway_str [out] Строка для шлюза (минимум 16 символов)
 */
void network_get_gateway(char *gateway_str)
{
    if (!network_initialized) {
        strcpy(gateway_str, "0.0.0.0");
        return;
    }
    
    ipaddr_ntoa_r(&main_netif.gw, gateway_str, 16);
}

/**
 * @brief Получение DNS сервера
 * 
 * @param dns_str [out] Строка для DNS (минимум 16 символов)
 */
void network_get_dns(char *dns_str)
{
    const ip_addr_t *dns = dns_getserver(0);
    ipaddr_ntoa_r(dns, dns_str, 16);
}

/**
 * @brief Получение MAC адреса
 * 
 * @param mac_str [out] Строка для MAC (минимум 18 символов)
 */
void network_get_mac(char *mac_str)
{
    snprintf(mac_str, 18, "%02X:%02X:%02X:%02X:%02X:%02X",
             mac_address[0], mac_address[1], mac_address[2],
             mac_address[3], mac_address[4], mac_address[5]);
}

/**
 * @brief Получение имени хоста
 * 
 * @param hostname [out] Строка для имени хоста
 */
void network_get_hostname(char *hostname)
{
    strcpy(hostname, net_config.hostname);
}

/* =============================================================================
 * ФУНКЦИИ НАСТРОЙКИ
 * ============================================================================= */

/**
 * @brief Установка статического IP адреса
 * 
 * @param ip IP адрес (4 байта)
 * @param netmask Маска подсети (4 байта)
 * @param gateway Шлюз (4 байта)
 * @return 0 при успехе
 */
int network_set_static_ip(const uint8_t *ip, const uint8_t *netmask, const uint8_t *gateway)
{
    if (!network_initialized) {
        return -1;
    }
    
    /* Останавливаем DHCP если был запущен */
    if (use_dhcp) {
        dhcp_stop(&main_netif);
        use_dhcp = false;
    }
    
    /* Устанавливаем новые адреса */
    ip4_addr_t ip_addr, nm_addr, gw_addr;
    
    IP4_ADDR(&ip_addr, ip[0], ip[1], ip[2], ip[3]);
    IP4_ADDR(&nm_addr, netmask[0], netmask[1], netmask[2], netmask[3]);
    IP4_ADDR(&gw_addr, gateway[0], gateway[1], gateway[2], gateway[3]);
    
    netif_set_addr(&main_netif, &ip_addr, &nm_addr, &gw_addr);
    
    /* Сохраняем в конфигурацию */
    net_config.dhcp = false;
    memcpy(net_config.ip, ip, 4);
    memcpy(net_config.netmask, netmask, 4);
    memcpy(net_config.gateway, gateway, 4);
    
    applog(LOG_INFO, "Установлен статический IP: %d.%d.%d.%d",
           ip[0], ip[1], ip[2], ip[3]);
    
    return 0;
}

/**
 * @brief Включение DHCP
 * 
 * @return 0 при успехе
 */
int network_enable_dhcp(void)
{
    if (!network_initialized) {
        return -1;
    }
    
    if (use_dhcp) {
        return 0;  /* Уже включен */
    }
    
    /* Сбрасываем IP адрес */
    ip4_addr_t zero;
    IP4_ADDR(&zero, 0, 0, 0, 0);
    netif_set_addr(&main_netif, &zero, &zero, &zero);
    
    /* Запускаем DHCP */
    dhcp_start(&main_netif);
    use_dhcp = true;
    
    net_config.dhcp = true;
    
    applog(LOG_INFO, "DHCP включен");
    
    return 0;
}

/**
 * @brief Установка DNS сервера
 * 
 * @param dns IP адрес DNS (4 байта)
 * @return 0 при успехе
 */
int network_set_dns(const uint8_t *dns)
{
    ip4_addr_t dns_addr;
    IP4_ADDR(&dns_addr, dns[0], dns[1], dns[2], dns[3]);
    dns_setserver(0, &dns_addr);
    
    memcpy(net_config.dns, dns, 4);
    
    return 0;
}

/**
 * @brief Установка имени хоста
 * 
 * @param hostname Имя хоста
 * @return 0 при успехе
 */
int network_set_hostname(const char *hostname)
{
    strncpy(net_config.hostname, hostname, sizeof(net_config.hostname) - 1);
    net_config.hostname[sizeof(net_config.hostname) - 1] = '\0';
    
    /* TODO: Обновить hostname в netif */
    
    return 0;
}

/* =============================================================================
 * СОХРАНЕНИЕ/ЗАГРУЗКА КОНФИГУРАЦИИ
 * ============================================================================= */

/**
 * @brief Сохранение сетевой конфигурации
 * 
 * @return 0 при успехе
 */
int network_save_config(void)
{
    /* Сохраняем в Flash */
    if (flash_erase_sector(FLASH_NETWORK_ADDR) != 0) {
        return -1;
    }
    
    if (flash_write(FLASH_NETWORK_ADDR, (uint8_t *)&net_config, 
                    sizeof(net_config)) != 0) {
        return -2;
    }
    
    applog(LOG_INFO, "Сетевая конфигурация сохранена");
    
    return 0;
}

/**
 * @brief Загрузка сетевой конфигурации
 * 
 * @return 0 при успехе
 */
int network_load_config(void)
{
    network_config_t loaded;
    
    if (flash_read(FLASH_NETWORK_ADDR, (uint8_t *)&loaded, 
                   sizeof(loaded)) != 0) {
        return -1;
    }
    
    /* Проверяем валидность (простая проверка) */
    if (loaded.ip[0] == 0xFF && loaded.ip[1] == 0xFF) {
        /* Flash не инициализирована */
        return -2;
    }
    
    memcpy(&net_config, &loaded, sizeof(net_config));
    
    applog(LOG_INFO, "Сетевая конфигурация загружена");
    
    return 0;
}
