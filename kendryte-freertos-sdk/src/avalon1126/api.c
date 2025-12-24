/**
 * =============================================================================
 * @file    api.c
 * @brief   Avalon A1126pro - CGMiner API сервер (реализация)
 * @version 1.0
 * @date    2024
 * =============================================================================
 * 
 * ОПИСАНИЕ:
 * Реализация API сервера для удалённого управления майнером.
 * Формат ответов: JSON
 * 
 * ПРИМЕРЫ ИСПОЛЬЗОВАНИЯ:
 * echo "summary" | nc 192.168.1.100 4028
 * echo "pools" | nc 192.168.1.100 4028
 * 
 * =============================================================================
 */

#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include <FreeRTOS.h>
#include <task.h>

#include "api.h"
#include "cgminer.h"
#include "pool.h"
#include "avalon10.h"

/* ===========================================================================
 * ЛОКАЛЬНЫЕ КОНСТАНТЫ
 * =========================================================================== */

static const char *TAG = "API";

/* ===========================================================================
 * ВНЕШНИЕ ПЕРЕМЕННЫЕ
 * =========================================================================== */

extern avalon10_info_t *g_avalon10_info;

/* ===========================================================================
 * ОБРАБОТЧИКИ КОМАНД
 * =========================================================================== */

/**
 * @brief Команда version - версия ПО
 */
static int cmd_version(char *response, int len)
{
    return snprintf(response, len,
        "{\"STATUS\":[{\"STATUS\":\"S\",\"Code\":22}],"
        "\"VERSION\":[{\"CGMiner\":\"%s\",\"API\":\"%s\"}]}\n",
        CGMINER_VERSION, "3.2");
}

/**
 * @brief Команда summary - общая статистика
 */
static int cmd_summary(char *response, int len)
{
    uint64_t accepted = 0, rejected = 0;
    get_pool_stats(&accepted, &rejected);
    
    time_t elapsed = time(NULL) - g_start_time;
    
    return snprintf(response, len,
        "{\"STATUS\":[{\"STATUS\":\"S\",\"Code\":11}],"
        "\"SUMMARY\":[{"
        "\"Elapsed\":%ld,"
        "\"GHS 5s\":%.2f,"
        "\"GHS av\":%.2f,"
        "\"Accepted\":%llu,"
        "\"Rejected\":%llu,"
        "\"Hardware Errors\":%llu,"
        "\"Pool Rejected%%\":%.2f"
        "}]}\n",
        elapsed,
        g_avalon10_info ? g_avalon10_info->total_hashrate / 1e9 : 0,
        g_avalon10_info ? g_avalon10_info->total_hashrate / 1e9 : 0,
        (unsigned long long)accepted,
        (unsigned long long)rejected,
        g_avalon10_info ? (unsigned long long)g_avalon10_info->total_hw_errors : 0,
        accepted > 0 ? (100.0 * rejected / (accepted + rejected)) : 0);
}

/**
 * @brief Команда pools - информация о пулах
 */
static int cmd_pools(char *response, int len)
{
    int offset = snprintf(response, len,
        "{\"STATUS\":[{\"STATUS\":\"S\",\"Code\":7}],\"POOLS\":[");
    
    for (int i = 0; i < g_pool_count; i++) {
        pool_t *pool = &g_pools[i];
        if (i > 0) offset += snprintf(response + offset, len - offset, ",");
        
        offset += snprintf(response + offset, len - offset,
            "{"
            "\"POOL\":%d,"
            "\"URL\":\"%s\","
            "\"Status\":\"%s\","
            "\"Priority\":%d,"
            "\"Accepted\":%llu,"
            "\"Rejected\":%llu,"
            "\"Difficulty\":%g"
            "}",
            pool->pool_no,
            pool->url,
            pool->stratum_active ? "Alive" : "Dead",
            pool->priority,
            (unsigned long long)pool->accepted,
            (unsigned long long)pool->rejected,
            pool->diff);
    }
    
    offset += snprintf(response + offset, len - offset, "]}\n");
    return offset;
}

/**
 * @brief Команда devs - информация об устройствах
 */
static int cmd_devs(char *response, int len)
{
    int offset = snprintf(response, len,
        "{\"STATUS\":[{\"STATUS\":\"S\",\"Code\":9}],\"DEVS\":[");
    
    if (g_avalon10_info) {
        for (int i = 0; i < AVALON10_DEFAULT_MODULARS; i++) {
            avalon10_module_t *module = &g_avalon10_info->modules[i];
            if (module->state == AVALON10_MODULE_STATE_NONE) continue;
            
            if (i > 0) offset += snprintf(response + offset, len - offset, ",");
            
            offset += snprintf(response + offset, len - offset,
                "{"
                "\"ASC\":%d,"
                "\"Enabled\":\"%s\","
                "\"Status\":\"%s\","
                "\"Temperature\":%.1f,"
                "\"MHS 5s\":%.2f,"
                "\"Accepted\":%lu,"
                "\"Rejected\":%lu,"
                "\"Hardware Errors\":%lu"
                "}",
                i,
                module->enabled ? "Y" : "N",
                avalon10_state_str(module->state),
                module->temp_avg / 10.0,
                module->hashrate / 1e6,
                (unsigned long)module->accepted,
                (unsigned long)module->rejected,
                (unsigned long)module->hw_errors);
        }
    }
    
    offset += snprintf(response + offset, len - offset, "]}\n");
    return offset;
}

/**
 * @brief Команда config - конфигурация
 */
static int cmd_config(char *response, int len)
{
    return snprintf(response, len,
        "{\"STATUS\":[{\"STATUS\":\"S\",\"Code\":33}],"
        "\"CONFIG\":[{"
        "\"ASC Count\":%d,"
        "\"PGA Count\":%d,"
        "\"Pool Count\":%d,"
        "\"Strategy\":\"%s\","
        "\"Log Interval\":%d"
        "}]}\n",
        g_asc_count,
        g_pga_count,
        g_pool_count,
        pool_strategy_names[g_pool_strategy],
        g_log_interval);
}

/**
 * @brief Команда stats - детальная статистика
 */
static int cmd_stats(char *response, int len)
{
    int offset = snprintf(response, len,
        "{\"STATUS\":[{\"STATUS\":\"S\",\"Code\":70}],\"STATS\":[");
    
    if (g_avalon10_info) {
        offset += snprintf(response + offset, len - offset,
            "{"
            "\"ID\":\"AVA10\","
            "\"Elapsed\":%lu,"
            "\"Modules\":%d,"
            "\"Chips\":%d,"
            "\"Cores\":%d,"
            "\"GHs\":%.2f,"
            "\"Temp\":%.1f,"
            "\"Fan1\":%d,"
            "\"Fan2\":%d,"
            "\"Freq\":%d,"
            "\"Volt\":%d"
            "}",
            (unsigned long)g_avalon10_info->uptime,
            g_avalon10_info->module_count,
            g_avalon10_info->module_count * AVALON10_DEFAULT_MINER_CNT,
            g_avalon10_info->module_count * AVALON10_CORES_PER_MODULE,
            g_avalon10_info->total_hashrate / 1e9,
            g_avalon10_info->modules[0].temp_avg / 10.0,
            g_avalon10_info->fan_pwm[0],
            g_avalon10_info->fan_pwm[1],
            g_avalon10_info->default_freq[0],
            g_avalon10_info->default_voltage);
    }
    
    offset += snprintf(response + offset, len - offset, "]}\n");
    return offset;
}

/* ===========================================================================
 * ОСНОВНЫЕ ФУНКЦИИ API
 * =========================================================================== */

/**
 * @brief Обработка API запроса
 */
int api_handle_request(const char *request, char *response, int resp_len)
{
    if (!request || !response) return 0;
    
    log_message(LOG_DEBUG, "%s: Запрос: %s", TAG, request);
    
    /* Удаляем пробелы и переводы строк */
    char cmd[64];
    strncpy(cmd, request, sizeof(cmd) - 1);
    char *p = strchr(cmd, '|');
    if (p) *p = '\0';
    p = strchr(cmd, '\n');
    if (p) *p = '\0';
    
    /* Обработка команд */
    if (strcmp(cmd, "version") == 0) {
        return cmd_version(response, resp_len);
    }
    else if (strcmp(cmd, "summary") == 0) {
        return cmd_summary(response, resp_len);
    }
    else if (strcmp(cmd, "pools") == 0) {
        return cmd_pools(response, resp_len);
    }
    else if (strcmp(cmd, "devs") == 0) {
        return cmd_devs(response, resp_len);
    }
    else if (strcmp(cmd, "config") == 0) {
        return cmd_config(response, resp_len);
    }
    else if (strcmp(cmd, "stats") == 0) {
        return cmd_stats(response, resp_len);
    }
    else {
        return snprintf(response, resp_len,
            "{\"STATUS\":[{\"STATUS\":\"E\",\"Code\":14,"
            "\"Msg\":\"Invalid command\"}]}\n");
    }
}

/* ===========================================================================
 * TCP SERVER IMPLEMENTATION
 * =========================================================================== */

#include "network.h"

static int api_server_socket = -1;
static int api_server_running = 0;

/**
 * @brief Обработка одного клиентского подключения
 */
static void api_handle_client(int client_sock)
{
    char request[256];
    char response[4096];
    int recv_len;
    
    /* Получаем команду */
    recv_len = network_socket_recv(client_sock, request, sizeof(request) - 1, 5000);
    
    if (recv_len > 0) {
        request[recv_len] = '\0';
        
        /* Обрабатываем запрос */
        int resp_len = api_handle_request(request, response, sizeof(response));
        
        /* Отправляем ответ */
        if (resp_len > 0) {
            network_socket_send(client_sock, response, resp_len);
        }
    }
    
    /* Закрываем соединение */
    network_socket_close(client_sock);
}

/**
 * @brief Задача API сервера
 */
static void api_server_task(void *param)
{
    int port = (int)(intptr_t)param;
    int client_sock;
    
    log_message(LOG_INFO, "%s: API сервер запущен на порту %d", TAG, port);
    
    while (api_server_running) {
        /* Ждём нового подключения */
        client_sock = network_socket_accept(api_server_socket, 1000);
        
        if (client_sock >= 0) {
            log_message(LOG_DEBUG, "%s: Новое API подключение", TAG);
            api_handle_client(client_sock);
        }
        
        /* Даём другим задачам время */
        vTaskDelay(pdMS_TO_TICKS(10));
    }
    
    log_message(LOG_INFO, "%s: API сервер остановлен", TAG);
    vTaskDelete(NULL);
}

/**
 * @brief Запуск API сервера
 */
int api_server_start(int port)
{
    log_message(LOG_INFO, "%s: Запуск API сервера на порту %d", TAG, port);
    
    /* Создаём серверный сокет */
    api_server_socket = network_socket_create();
    if (api_server_socket < 0) {
        log_message(LOG_ERR, "%s: Не удалось создать сокет", TAG);
        return -1;
    }
    
    /* Начинаем слушать */
    if (network_socket_listen(api_server_socket, port, 5) < 0) {
        log_message(LOG_ERR, "%s: Не удалось начать прослушивание порта %d", TAG, port);
        network_socket_close(api_server_socket);
        api_server_socket = -1;
        return -1;
    }
    
    api_server_running = 1;
    
    /* Создаём задачу API сервера */
    xTaskCreate(
        api_server_task,
        "api_server",
        4096,
        (void *)(intptr_t)port,
        API_TASK_PRIORITY,
        NULL
    );
    
    return 0;
}

/**
 * @brief Остановка API сервера
 */
void api_server_stop(void)
{
    log_message(LOG_INFO, "%s: Остановка API сервера", TAG);
    
    api_server_running = 0;
    
    if (api_server_socket >= 0) {
        network_socket_close(api_server_socket);
        api_server_socket = -1;
    }
}

/* ===========================================================================
 * КОНЕЦ ФАЙЛА api.c
 * =========================================================================== */
