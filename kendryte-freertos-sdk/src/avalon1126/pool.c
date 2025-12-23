/**
 * =============================================================================
 * @file    pool.c
 * @brief   Avalon A1126pro - Управление пулами майнинга (реализация)
 * @version 1.0
 * @date    2024
 * =============================================================================
 * 
 * ОПИСАНИЕ:
 * Реализация модуля управления пулами майнинга.
 * Обеспечивает подключение, переключение и статистику пулов.
 * 
 * ОРИГИНАЛЬНЫЕ ФУНКЦИИ:
 * - FUN_ram_8001cfec @ 0x8001cfec - add_pool
 * - FUN_ram_8001ce1e @ 0x8001ce1e - pool_init
 * - FUN_ram_8001d0b4 @ 0x8001d0b4 - connect_pool
 * 
 * =============================================================================
 */

/* ===========================================================================
 * ПОДКЛЮЧАЕМЫЕ ФАЙЛЫ
 * =========================================================================== */

#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include <FreeRTOS.h>
#include <semphr.h>

#include "pool.h"
#include "cgminer.h"

/* ===========================================================================
 * ЛОКАЛЬНЫЕ КОНСТАНТЫ
 * =========================================================================== */

static const char *TAG = "Pool";

/* ===========================================================================
 * РЕАЛИЗАЦИЯ ФУНКЦИЙ
 * =========================================================================== */

/**
 * @brief Инициализация модуля пулов
 * 
 * Очищает массив пулов и устанавливает начальные значения.
 */
void pool_init(void)
{
    log_message(LOG_INFO, "%s: Инициализация модуля пулов", TAG);
    
    /* Очистка всех пулов */
    memset(g_pools, 0, sizeof(g_pools));
    
    for (int i = 0; i < MAX_POOLS; i++) {
        g_pools[i].pool_no = i;
        g_pools[i].priority = i;
        g_pools[i].sock = -1;
        g_pools[i].state = POOL_STATE_DISABLED;
    }
    
    g_pool_count = 0;
    g_current_pool = NULL;
}

/**
 * @brief Парсинг URL пула
 * 
 * Извлекает хост и порт из URL вида stratum+tcp://host:port
 * 
 * @param pool  Указатель на пул
 * @return      0 при успехе
 */
static int parse_pool_url(pool_t *pool)
{
    const char *url = pool->url;
    char *host_start;
    char *port_start;
    
    /* Пропускаем протокол stratum+tcp:// */
    host_start = strstr(url, "://");
    if (host_start) {
        host_start += 3;
    } else {
        host_start = (char*)url;
    }
    
    /* Ищем порт */
    port_start = strchr(host_start, ':');
    if (port_start) {
        int host_len = port_start - host_start;
        if (host_len >= MAX_URL_LEN) host_len = MAX_URL_LEN - 1;
        strncpy(pool->host, host_start, host_len);
        pool->host[host_len] = '\0';
        pool->port = atoi(port_start + 1);
    } else {
        strncpy(pool->host, host_start, MAX_URL_LEN - 1);
        pool->port = 3333;  /* Порт по умолчанию */
    }
    
    return 0;
}

/**
 * @brief Добавление нового пула
 * 
 * Создаёт новый пул с указанными параметрами.
 * 
 * @param url   URL пула
 * @param user  Имя пользователя
 * @param pass  Пароль
 * @return      Указатель на пул или NULL
 */
pool_t *add_pool(const char *url, const char *user, const char *pass)
{
    if (g_pool_count >= MAX_POOLS) {
        log_message(LOG_ERR, "%s: Максимум %d пулов", TAG, MAX_POOLS);
        return NULL;
    }
    
    pool_t *pool = &g_pools[g_pool_count];
    
    /* Копируем параметры */
    strncpy(pool->url, url ? url : "", MAX_URL_LEN - 1);
    strncpy(pool->user, user ? user : "", MAX_USER_LEN - 1);
    strncpy(pool->pass, pass ? pass : "x", MAX_PASS_LEN - 1);
    
    /* Парсим URL */
    parse_pool_url(pool);
    
    pool->pool_no = g_pool_count;
    pool->priority = g_pool_count;
    pool->enabled = 1;
    pool->state = POOL_STATE_DISABLED;
    pool->sock = -1;
    pool->diff = 1.0;
    
    g_pool_count++;
    
    log_message(LOG_INFO, "%s: Добавлен пул #%d: %s:%d, пользователь: %s",
               TAG, pool->pool_no, pool->host, pool->port, pool->user);
    
    /* Если это первый пул - делаем его текущим */
    if (g_current_pool == NULL) {
        g_current_pool = pool;
    }
    
    return pool;
}

/**
 * @brief Удаление пула
 */
int remove_pool(int pool_no)
{
    if (pool_no < 0 || pool_no >= g_pool_count) {
        return -1;
    }
    
    pool_t *pool = &g_pools[pool_no];
    
    /* Отключаемся от пула */
    disconnect_pool(pool);
    
    /* Сдвигаем остальные пулы */
    for (int i = pool_no; i < g_pool_count - 1; i++) {
        memcpy(&g_pools[i], &g_pools[i + 1], sizeof(pool_t));
        g_pools[i].pool_no = i;
    }
    
    g_pool_count--;
    
    /* Обновляем текущий пул */
    if (g_current_pool == pool) {
        g_current_pool = g_pool_count > 0 ? &g_pools[0] : NULL;
    }
    
    log_message(LOG_INFO, "%s: Удалён пул #%d", TAG, pool_no);
    
    return 0;
}

/**
 * @brief Включение пула
 */
void enable_pool(int pool_no)
{
    if (pool_no >= 0 && pool_no < g_pool_count) {
        g_pools[pool_no].enabled = 1;
        log_message(LOG_INFO, "%s: Пул #%d включён", TAG, pool_no);
    }
}

/**
 * @brief Отключение пула
 */
void disable_pool(int pool_no)
{
    if (pool_no >= 0 && pool_no < g_pool_count) {
        g_pools[pool_no].enabled = 0;
        disconnect_pool(&g_pools[pool_no]);
        log_message(LOG_INFO, "%s: Пул #%d отключён", TAG, pool_no);
    }
}

/**
 * @brief Подключение к пулу
 * 
 * Устанавливает TCP соединение с пулом.
 */
int connect_pool(pool_t *pool)
{
    if (!pool || !pool->enabled) {
        return -1;
    }
    
    log_message(LOG_INFO, "%s: Подключение к %s:%d...", TAG, pool->host, pool->port);
    
    pool->state = POOL_STATE_CONNECTING;
    
    /* TODO: Реальное TCP подключение через lwIP
     * pool->sock = socket(AF_INET, SOCK_STREAM, 0);
     * connect(pool->sock, ...);
     */
    
    /* Симуляция успешного подключения */
    pool->sock = 1;
    pool->state = POOL_STATE_CONNECTED;
    pool->connect_time = time(NULL);
    pool->fail_count = 0;
    
    log_message(LOG_INFO, "%s: Подключён к пулу #%d", TAG, pool->pool_no);
    
    return 0;
}

/**
 * @brief Отключение от пула
 */
void disconnect_pool(pool_t *pool)
{
    if (!pool) return;
    
    if (pool->sock >= 0) {
        /* close(pool->sock); */
        pool->sock = -1;
    }
    
    pool->state = POOL_STATE_DISABLED;
    pool->stratum_active = 0;
    pool->stratum_auth = 0;
    
    log_message(LOG_INFO, "%s: Отключён от пула #%d", TAG, pool->pool_no);
}

/**
 * @brief Переподключение к пулу
 */
int reconnect_pool(pool_t *pool)
{
    disconnect_pool(pool);
    vTaskDelay(pdMS_TO_TICKS(1000));
    return connect_pool(pool);
}

/**
 * @brief Получение текущего активного пула
 */
pool_t *get_current_pool(void)
{
    return g_current_pool;
}

/**
 * @brief Переключение на следующий пул
 * 
 * Используется при сбое текущего пула (failover).
 */
pool_t *switch_pool(void)
{
    if (g_pool_count == 0) {
        return NULL;
    }
    
    /* Помечаем текущий пул как сбойный */
    if (g_current_pool) {
        g_current_pool->fail_count++;
        g_current_pool->last_fail = time(NULL);
        disconnect_pool(g_current_pool);
    }
    
    /* Ищем следующий доступный пул */
    for (int i = 0; i < g_pool_count; i++) {
        pool_t *pool = &g_pools[i];
        if (pool->enabled && pool != g_current_pool) {
            g_current_pool = pool;
            log_message(LOG_INFO, "%s: Переключение на пул #%d", TAG, pool->pool_no);
            return pool;
        }
    }
    
    /* Если нет других - возвращаемся к первому */
    if (g_pool_count > 0) {
        g_current_pool = &g_pools[0];
        return g_current_pool;
    }
    
    return NULL;
}

/**
 * @brief Выбор лучшего пула по стратегии
 */
pool_t *select_best_pool(void)
{
    pool_t *best = NULL;
    
    switch (g_pool_strategy) {
        case POOL_STRATEGY_FAILOVER:
            /* Выбираем первый доступный пул с минимальным приоритетом */
            for (int i = 0; i < g_pool_count; i++) {
                pool_t *pool = &g_pools[i];
                if (pool->enabled && pool->state != POOL_STATE_DEAD) {
                    if (!best || pool->priority < best->priority) {
                        best = pool;
                    }
                }
            }
            break;
            
        case POOL_STRATEGY_ROUND_ROBIN:
            /* Циклический перебор */
            if (g_current_pool) {
                int next = (g_current_pool->pool_no + 1) % g_pool_count;
                best = &g_pools[next];
            } else {
                best = &g_pools[0];
            }
            break;
            
        default:
            best = &g_pools[0];
            break;
    }
    
    return best;
}

/**
 * @brief Получение общей статистики
 */
void get_pool_stats(uint64_t *accepted, uint64_t *rejected)
{
    uint64_t acc = 0, rej = 0;
    
    for (int i = 0; i < g_pool_count; i++) {
        acc += g_pools[i].accepted;
        rej += g_pools[i].rejected;
    }
    
    if (accepted) *accepted = acc;
    if (rejected) *rejected = rej;
}

/**
 * @brief Сброс статистики пула
 */
void reset_pool_stats(pool_t *pool)
{
    if (pool) {
        pool->accepted = 0;
        pool->rejected = 0;
        pool->stale = 0;
        pool->getworks = 0;
        pool->total_diff = 0;
    }
}

/* ===========================================================================
 * КОНЕЦ ФАЙЛА pool.c
 * =========================================================================== */
