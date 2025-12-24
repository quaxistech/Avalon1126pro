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

#include <FreeRTOS.h>
#include <task.h>
#include <semphr.h>

#include "network.h"
#include "cgminer.h"
#include "mock_hardware.h"
#include <osdefs.h>

#ifndef DHCP_ADDRESS_ASSIGNED
#define DHCP_ADDRESS_ASSIGNED 5
#endif

/* lwIP headers */
#if !MOCK_NETWORK
#include "lwip/init.h"
#include "lwip/netif.h"
#include "lwip/tcpip.h"
#include "lwip/dhcp.h"
#include "lwip/dns.h"
#include "lwip/sockets.h"
#include "lwip/ip_addr.h"
#include "netif/ethernet.h"

/* DM9051 driver */
#include "network/dm9051.h"
#include <devices.h>
#include <hal.h>
#endif

static const char *TAG = "Network";

/* ===========================================================================
 * КОНФИГУРАЦИЯ
 * =========================================================================== */

/* Пины для DM9051 */
#define DM9051_SPI_NUM          1           /* SPI1 */
#define DM9051_SPI_CS           3           /* CS3 */
#define DM9051_INT_GPIO         6           /* GPIO6 для прерываний */
#define DM9051_RST_GPIO         7           /* GPIO7 для сброса */

/* Таймауты */
#define NETWORK_INIT_TIMEOUT_MS     10000
#define DHCP_TIMEOUT_MS             30000

/* ===========================================================================
 * ЛОКАЛЬНЫЕ ПЕРЕМЕННЫЕ
 * =========================================================================== */

static uint8_t s_ip_addr[4] = {0};
static uint8_t s_netmask[4] = {255, 255, 255, 0};
static uint8_t s_gateway[4] = {0};
static uint8_t s_mac_addr[6] = {0x02, 0x00, 0xAA, 0x11, 0x26, 0x00};
static int s_connected = 0;
static int s_dhcp_enabled = 1;
static int s_initialized = 0;

#if !MOCK_NETWORK
static struct netif dm9051_netif;
static handle_t dm9051_handle = 0;
static SemaphoreHandle_t network_event = NULL;
static handle_t netif_handle = 0;

/* Задача обработки сетевых пакетов */
static TaskHandle_t network_task_handle = NULL;

/* API freertos network (C интерфейс из SDK) */
extern handle_t network_interface_add(handle_t adapter_handle, const ip_address_t *ip_address,
                                      const ip_address_t *net_mask, const ip_address_t *gateway);
extern int network_interface_set_enable(handle_t netif_handle, bool enable);
extern int network_interface_set_as_default(handle_t netif_handle);
extern int network_interface_dhcp_pooling(handle_t netif_handle);
extern int network_get_addr(handle_t netif_handle, ip_address_t *ip_address,
                            ip_address_t *net_mask, ip_address_t *gateway);
#endif

/* ===========================================================================
 * ВНУТРЕННИЕ ФУНКЦИИ
 * =========================================================================== */

#if MOCK_NETWORK

/**
 * @brief Инициализация mock сети
 */
static int network_init_mock(void)
{
    mock_network_init();
    
    /* Симулируем успешное подключение с DHCP */
    s_ip_addr[0] = 192;
    s_ip_addr[1] = 168;
    s_ip_addr[2] = 1;
    s_ip_addr[3] = 100;
    
    s_gateway[0] = 192;
    s_gateway[1] = 168;
    s_gateway[2] = 1;
    s_gateway[3] = 1;
    
    s_connected = 1;
    s_initialized = 1;
    
    log_message(LOG_INFO, "%s: Mock IP: %d.%d.%d.%d", TAG,
               s_ip_addr[0], s_ip_addr[1], s_ip_addr[2], s_ip_addr[3]);
    
    return 0;
}

#else

/**
 * @brief Callback при изменении статуса netif
 */
static void netif_status_callback(struct netif *netif)
{
    if (netif_is_up(netif)) {
        if (!ip4_addr_isany_val(*netif_ip4_addr(netif))) {
            s_ip_addr[0] = ip4_addr1(netif_ip4_addr(netif));
            s_ip_addr[1] = ip4_addr2(netif_ip4_addr(netif));
            s_ip_addr[2] = ip4_addr3(netif_ip4_addr(netif));
            s_ip_addr[3] = ip4_addr4(netif_ip4_addr(netif));
            
            s_netmask[0] = ip4_addr1(netif_ip4_netmask(netif));
            s_netmask[1] = ip4_addr2(netif_ip4_netmask(netif));
            s_netmask[2] = ip4_addr3(netif_ip4_netmask(netif));
            s_netmask[3] = ip4_addr4(netif_ip4_netmask(netif));
            
            s_gateway[0] = ip4_addr1(netif_ip4_gw(netif));
            s_gateway[1] = ip4_addr2(netif_ip4_gw(netif));
            s_gateway[2] = ip4_addr3(netif_ip4_gw(netif));
            s_gateway[3] = ip4_addr4(netif_ip4_gw(netif));
            
            s_connected = 1;
            
            log_message(LOG_INFO, "%s: IP: %d.%d.%d.%d", TAG,
                       s_ip_addr[0], s_ip_addr[1], s_ip_addr[2], s_ip_addr[3]);
            log_message(LOG_INFO, "%s: Gateway: %d.%d.%d.%d", TAG,
                       s_gateway[0], s_gateway[1], s_gateway[2], s_gateway[3]);
        }
    } else {
        s_connected = 0;
        log_message(LOG_WARNING, "%s: Сеть отключена", TAG);
    }
}

/**
 * @brief Задача обработки сетевых пакетов DM9051
 */
static void network_rx_task(void *pvParameters)
{
    log_message(LOG_DEBUG, "%s: Задача RX запущена", TAG);
    
    while (1) {
        if (xSemaphoreTake(network_event, pdMS_TO_TICKS(100)) == pdTRUE) {
            /* Обрабатываем входящие пакеты */
            /* dm9051 driver обрабатывает это автоматически */
        }
        
        /* Периодическая проверка состояния link */
        static int link_check_counter = 0;
        if (++link_check_counter >= 10) {  /* Каждую секунду */
            link_check_counter = 0;
            
            /* Проверяем link status */
            /* Это делается через драйвер DM9051 */
        }
    }
}

/**
 * @brief Инициализация реального сетевого интерфейса
 */
static int network_init_hardware(void)
{
    log_message(LOG_INFO, "%s: Инициализация DM9051...", TAG);
    
    /* Создаём семафор для событий */
    network_event = xSemaphoreCreateBinary();
    if (!network_event) {
        log_message(LOG_ERR, "%s: Не удалось создать семафор", TAG);
        return -1;
    }
    
    /* Инициализируем lwIP */
    tcpip_init(NULL, NULL);
    
    /* Установка MAC адреса из конфигурации */
    if (g_config.mac_addr[0] || g_config.mac_addr[1]) {
        memcpy(s_mac_addr, g_config.mac_addr, 6);
    }
    
    /* Инициализация DM9051 через SDK */
    handle_t spi_handle = io_open("/dev/spi1");
    handle_t gpio_handle = io_open("/dev/gpio0");
    
    if (!spi_handle || !gpio_handle) {
        log_message(LOG_ERR, "%s: Не удалось открыть SPI/GPIO", TAG);
        return -1;
    }
    
    mac_address_t mac = {
        .address = {s_mac_addr[0], s_mac_addr[1], s_mac_addr[2],
                   s_mac_addr[3], s_mac_addr[4], s_mac_addr[5]}
    };
    
    dm9051_handle = dm9051_driver_install(spi_handle, DM9051_SPI_CS,
                                          gpio_handle, DM9051_INT_GPIO, &mac);
    
    if (!dm9051_handle) {
        log_message(LOG_ERR, "%s: Ошибка инициализации DM9051", TAG);
        return -1;
    }

    ip_address_t ip_cfg = {0};
    ip_address_t mask_cfg = {0};
    ip_address_t gw_cfg = {0};
    if (!s_dhcp_enabled && !g_config.dhcp_enabled) {
        ip_cfg.data[0] = g_config.ip_addr[0];
        ip_cfg.data[1] = g_config.ip_addr[1];
        ip_cfg.data[2] = g_config.ip_addr[2];
        ip_cfg.data[3] = g_config.ip_addr[3];
        
        mask_cfg.data[0] = g_config.netmask[0];
        mask_cfg.data[1] = g_config.netmask[1];
        mask_cfg.data[2] = g_config.netmask[2];
        mask_cfg.data[3] = g_config.netmask[3];
        
        gw_cfg.data[0] = g_config.gateway[0];
        gw_cfg.data[1] = g_config.gateway[1];
        gw_cfg.data[2] = g_config.gateway[2];
        gw_cfg.data[3] = g_config.gateway[3];
    }
    
    netif_handle = network_interface_add(dm9051_handle, &ip_cfg, &mask_cfg, &gw_cfg);
    if (!netif_handle) {
        log_message(LOG_ERR, "%s: Не удалось создать сетевой интерфейс", TAG);
        return -1;
    }
    
    network_interface_set_as_default(netif_handle);
    network_interface_set_enable(netif_handle, true);
    
    if (s_dhcp_enabled || g_config.dhcp_enabled) {
        log_message(LOG_INFO, "%s: Запуск DHCP...", TAG);
        int state = network_interface_dhcp_pooling(netif_handle);
        int retries = 3;
        while (state != DHCP_ADDRESS_ASSIGNED && retries-- > 0) {
            vTaskDelay(pdMS_TO_TICKS(500));
            state = network_interface_dhcp_pooling(netif_handle);
        }
        if (state == DHCP_ADDRESS_ASSIGNED) {
            ip_address_t ip_get = {0}, mask_get = {0}, gw_get = {0};
            if (network_get_addr(netif_handle, &ip_get, &mask_get, &gw_get) == 0) {
                memcpy(s_ip_addr, ip_get.data, 4);
                memcpy(s_netmask, mask_get.data, 4);
                memcpy(s_gateway, gw_get.data, 4);
                s_connected = 1;
            }
        } else {
            log_message(LOG_WARNING, "%s: DHCP не получил адрес (state=%d)", TAG, state);
        }
    } else {
        s_connected = 1;
        memcpy(s_ip_addr, ip_cfg.data, 4);
        memcpy(s_netmask, mask_cfg.data, 4);
        memcpy(s_gateway, gw_cfg.data, 4);
    }
    
    /* Создаём задачу обработки пакетов */
    xTaskCreate(network_rx_task, "net_rx", 4096, NULL, 3, &network_task_handle);
    
    s_initialized = 1;
    
    log_message(LOG_INFO, "%s: MAC: %02X:%02X:%02X:%02X:%02X:%02X", TAG,
               s_mac_addr[0], s_mac_addr[1], s_mac_addr[2],
               s_mac_addr[3], s_mac_addr[4], s_mac_addr[5]);
    
    return 0;
}

#endif /* MOCK_NETWORK */

/* ===========================================================================
 * ПУБЛИЧНЫЕ ФУНКЦИИ
 * =========================================================================== */

/**
 * @brief Инициализация сети
 */
int network_init(void)
{
    log_message(LOG_INFO, "%s: Инициализация сети...", TAG);
    
#if MOCK_NETWORK
    return network_init_mock();
#else
    return network_init_hardware();
#endif
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
 * @brief Получение маски подсети
 */
int network_get_netmask(uint8_t *mask)
{
    if (mask) {
        memcpy(mask, s_netmask, 4);
        return 0;
    }
    return -1;
}

/**
 * @brief Получение шлюза
 */
int network_get_gateway(uint8_t *gw)
{
    if (gw) {
        memcpy(gw, s_gateway, 4);
        return 0;
    }
    return -1;
}

/**
 * @brief Получение MAC адреса
 */
int network_get_mac(uint8_t *mac)
{
    if (mac) {
        memcpy(mac, s_mac_addr, 6);
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

/**
 * @brief Проверка инициализации
 */
int network_is_initialized(void)
{
    return s_initialized;
}

/**
 * @brief Установка статического IP
 */
int network_set_static_ip(const uint8_t *ip, const uint8_t *mask, const uint8_t *gw)
{
    if (!ip || !mask || !gw) return -1;
    
    s_dhcp_enabled = 0;
    memcpy(s_ip_addr, ip, 4);
    memcpy(s_netmask, mask, 4);
    memcpy(s_gateway, gw, 4);
    
#if !MOCK_NETWORK
    if (s_initialized) {
        ip4_addr_t ipaddr, netmask, gateway;
        IP4_ADDR(&ipaddr, ip[0], ip[1], ip[2], ip[3]);
        IP4_ADDR(&netmask, mask[0], mask[1], mask[2], mask[3]);
        IP4_ADDR(&gateway, gw[0], gw[1], gw[2], gw[3]);
        
        dhcp_stop(&dm9051_netif);
        netif_set_addr(&dm9051_netif, &ipaddr, &netmask, &gateway);
    }
#endif
    
    s_connected = 1;
    
    log_message(LOG_INFO, "%s: Статический IP: %d.%d.%d.%d", TAG,
               ip[0], ip[1], ip[2], ip[3]);
    
    return 0;
}

/**
 * @brief Включение DHCP
 */
int network_enable_dhcp(void)
{
    s_dhcp_enabled = 1;
    
#if !MOCK_NETWORK
    if (s_initialized) {
        dhcp_start(&dm9051_netif);
    }
#endif
    
    log_message(LOG_INFO, "%s: DHCP включён", TAG);
    return 0;
}

/* ===========================================================================
 * СОКЕТНЫЕ ФУНКЦИИ
 * =========================================================================== */

/**
 * @brief Создание TCP сокета
 */
int network_socket_create(void)
{
#if MOCK_NETWORK
    return mock_socket_create();
#else
    return lwip_socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
#endif
}

/**
 * @brief Подключение TCP сокета
 */
int network_socket_connect(int sock, const char *host, int port)
{
#if MOCK_NETWORK
    return mock_socket_connect(sock, host, port);
#else
    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    
    /* Преобразуем hostname в IP */
    ip_addr_t ip;
    if (dns_gethostbyname(host, &ip, NULL, NULL) == ERR_OK) {
        addr.sin_addr.s_addr = ip.addr;
    } else {
        /* Пробуем как IP адрес напрямую */
        addr.sin_addr.s_addr = ipaddr_addr(host);
    }
    
    /* Устанавливаем таймаут */
    struct timeval tv;
    tv.tv_sec = 10;
    tv.tv_usec = 0;
    lwip_setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));
    lwip_setsockopt(sock, SOL_SOCKET, SO_SNDTIMEO, &tv, sizeof(tv));
    
    return lwip_connect(sock, (struct sockaddr *)&addr, sizeof(addr));
#endif
}

/**
 * @brief Отправка данных через сокет
 */
int network_socket_send(int sock, const void *data, size_t len)
{
#if MOCK_NETWORK
    return mock_socket_send(sock, data, len);
#else
    return lwip_send(sock, data, len, 0);
#endif
}

/**
 * @brief Приём данных из сокета
 */
int network_socket_recv(int sock, void *buf, size_t len, int timeout)
{
#if MOCK_NETWORK
    (void)timeout;
    return mock_socket_recv(sock, buf, len);
#else
    /* Устанавливаем таймаут */
    if (timeout > 0) {
        struct timeval tv;
        tv.tv_sec = timeout / 1000;
        tv.tv_usec = (timeout % 1000) * 1000;
        lwip_setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));
    }
    return lwip_recv(sock, buf, len, 0);
#endif
}

/**
 * @brief Закрытие сокета
 */
void network_socket_close(int sock)
{
#if MOCK_NETWORK
    mock_socket_close(sock);
#else
    lwip_close(sock);
#endif
}

/**
 * @brief Проверка данных в сокете (non-blocking)
 */
int network_socket_available(int sock)
{
#if MOCK_NETWORK
    return 0;  /* Mock не поддерживает */
#else
    int bytes_available = 0;
    lwip_ioctl(sock, FIONREAD, &bytes_available);
    return bytes_available;
#endif
}

/**
 * @brief Создание слушающего TCP сокета (для сервера)
 */
int network_socket_listen(int sock, int port, int backlog)
{
#if MOCK_NETWORK
    (void)sock;
    (void)port;
    (void)backlog;
    return 0;  /* Mock - OK */
#else
    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    addr.sin_addr.s_addr = INADDR_ANY;
    
    /* Разрешаем повторное использование адреса */
    int opt = 1;
    lwip_setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
    
    if (lwip_bind(sock, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
        return -1;
    }
    
    if (lwip_listen(sock, backlog) < 0) {
        return -1;
    }
    
    return 0;
#endif
}

/**
 * @brief Принятие входящего соединения
 */
int network_socket_accept(int listen_sock, int timeout)
{
#if MOCK_NETWORK
    (void)listen_sock;
    (void)timeout;
    return mock_socket_create();
#else
    /* Устанавливаем таймаут */
    if (timeout > 0) {
        struct timeval tv;
        tv.tv_sec = timeout / 1000;
        tv.tv_usec = (timeout % 1000) * 1000;
        lwip_setsockopt(listen_sock, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));
    }
    
    struct sockaddr_in client_addr;
    socklen_t addr_len = sizeof(client_addr);
    return lwip_accept(listen_sock, (struct sockaddr *)&client_addr, &addr_len);
#endif
}

/* ===========================================================================
 * КОНЕЦ ФАЙЛА network.c
 * =========================================================================== */
