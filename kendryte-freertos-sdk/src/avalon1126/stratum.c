/**
 * =============================================================================
 * @file    stratum.c
 * @brief   Avalon A1126pro - Stratum протокол (реализация)
 * @version 1.0
 * @date    2024
 * =============================================================================
 * 
 * ОПИСАНИЕ:
 * Реализация Stratum протокола для связи с пулами майнинга.
 * 
 * ФОРМАТ СООБЩЕНИЙ:
 * Stratum использует JSON-RPC поверх TCP.
 * Каждое сообщение - одна строка JSON, завершённая \n
 * 
 * ПРИМЕР:
 * -> {"id":1,"method":"mining.subscribe","params":[]}
 * <- {"id":1,"result":[[["mining.notify","..."],"extranonce1",4]],"error":null}
 * 
 * =============================================================================
 */

#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include "stratum.h"
#include "pool.h"
#include "cgminer.h"

/* ===========================================================================
 * ЛОКАЛЬНЫЕ КОНСТАНТЫ
 * =========================================================================== */

static const char *TAG = "Stratum";

/* ===========================================================================
 * ВСПОМОГАТЕЛЬНЫЕ ФУНКЦИИ
 * =========================================================================== */

/**
 * @brief Простой парсер JSON - получение значения по ключу
 * 
 * @param json  JSON строка
 * @param key   Ключ для поиска
 * @param buf   Буфер для результата
 * @param len   Размер буфера
 * @return      0 при успехе
 */
static int json_get_string(const char *json, const char *key, char *buf, int len)
{
    char search[64];
    const char *start, *end;
    
    snprintf(search, sizeof(search), "\"%s\":\"", key);
    start = strstr(json, search);
    if (!start) {
        snprintf(search, sizeof(search), "\"%s\": \"", key);
        start = strstr(json, search);
    }
    
    if (!start) return -1;
    
    start = strchr(start, ':');
    if (!start) return -1;
    
    start = strchr(start, '"');
    if (!start) return -1;
    start++;
    
    end = strchr(start, '"');
    if (!end) return -1;
    
    int vlen = end - start;
    if (vlen >= len) vlen = len - 1;
    
    strncpy(buf, start, vlen);
    buf[vlen] = '\0';
    
    return 0;
}

/* ===========================================================================
 * РЕАЛИЗАЦИЯ ФУНКЦИЙ
 * =========================================================================== */

/**
 * @brief Подключение к пулу по Stratum
 */
int stratum_connect(pool_t *pool)
{
    if (!pool) return -1;
    
    log_message(LOG_INFO, "%s: Stratum подключение к %s:%d", 
               TAG, pool->host, pool->port);
    
    /* Подключение к TCP сокету уже выполнено в connect_pool() */
    
    /* Отправляем mining.subscribe */
    if (stratum_subscribe(pool) < 0) {
        log_message(LOG_ERR, "%s: Ошибка subscribe", TAG);
        return -1;
    }
    
    /* Отправляем mining.authorize */
    if (stratum_authorize(pool) < 0) {
        log_message(LOG_ERR, "%s: Ошибка authorize", TAG);
        return -1;
    }
    
    pool->stratum_active = 1;
    pool->state = POOL_STATE_ACTIVE;
    
    log_message(LOG_INFO, "%s: Stratum активен, пул #%d", TAG, pool->pool_no);
    
    return 0;
}

/**
 * @brief Отключение от пула
 */
void stratum_disconnect(pool_t *pool)
{
    if (pool) {
        pool->stratum_active = 0;
        pool->stratum_auth = 0;
    }
}

/**
 * @brief Отправка mining.subscribe
 */
int stratum_subscribe(pool_t *pool)
{
    char msg[512];
    
    pool->seq_getwork++;
    
    /* Формируем JSON сообщение */
    snprintf(msg, sizeof(msg),
            "{\"id\":%d,\"method\":\"mining.subscribe\",\"params\":[\"%s\"]}\n",
            pool->seq_getwork, "cgminer/" CGMINER_VERSION);
    
    log_message(LOG_DEBUG, "%s: -> %s", TAG, msg);
    
    return stratum_send_line(pool, msg);
}

/**
 * @brief Отправка mining.authorize
 */
int stratum_authorize(pool_t *pool)
{
    char msg[512];
    
    pool->seq_getwork++;
    
    snprintf(msg, sizeof(msg),
            "{\"id\":%d,\"method\":\"mining.authorize\",\"params\":[\"%s\",\"%s\"]}\n",
            pool->seq_getwork, pool->user, pool->pass);
    
    log_message(LOG_DEBUG, "%s: -> %s", TAG, msg);
    
    if (stratum_send_line(pool, msg) == 0) {
        pool->stratum_auth = 1;
        return 0;
    }
    
    return -1;
}

/**
 * @brief Чтение строки от пула
 */
char *stratum_recv_line(pool_t *pool)
{
    if (!pool || pool->sock < 0) {
        return NULL;
    }
    
    /* TODO: Реальное чтение из сокета
     * char *line = malloc(STRATUM_MAX_MSG);
     * int len = recv(pool->sock, line, STRATUM_MAX_MSG - 1, 0);
     */
    
    return NULL;
}

/**
 * @brief Отправка строки на пул
 */
int stratum_send_line(pool_t *pool, const char *str)
{
    if (!pool || !str || pool->sock < 0) {
        return -1;
    }
    
    /* TODO: Реальная отправка в сокет
     * return send(pool->sock, str, strlen(str), 0);
     */
    
    return 0;
}

/**
 * @brief Парсинг JSON ответа от пула
 * 
 * Обрабатывает:
 * - mining.notify - новое задание
 * - mining.set_difficulty - установка сложности
 * - result - ответ на запрос
 */
int stratum_parse_response(pool_t *pool, const char *line)
{
    if (!pool || !line) return -1;
    
    log_message(LOG_DEBUG, "%s: <- %s", TAG, line);
    
    /* Проверяем тип сообщения */
    if (strstr(line, "mining.notify")) {
        /* Новое задание от пула */
        log_message(LOG_INFO, "%s: Новое задание", TAG);
        
        /* TODO: Парсинг job_id, prevhash, coinbase1, coinbase2, merkle_branch, version, nbits, ntime */
        
        pool->getworks++;
        pool->last_work_time = time(NULL);
        
    } else if (strstr(line, "mining.set_difficulty")) {
        /* Установка сложности */
        char diff_str[32];
        if (json_get_string(line, "params", diff_str, sizeof(diff_str)) == 0) {
            pool->sdiff = atof(diff_str);
            log_message(LOG_INFO, "%s: Сложность: %.2f", TAG, pool->sdiff);
        }
        
    } else if (strstr(line, "\"result\":true")) {
        /* Шара принята */
        pool->accepted++;
        log_message(LOG_INFO, "%s: Шара принята! Всего: %llu", 
                   TAG, (unsigned long long)pool->accepted);
        
    } else if (strstr(line, "\"result\":false") || strstr(line, "\"error\":")) {
        /* Шара отклонена */
        pool->rejected++;
        log_message(LOG_WARNING, "%s: Шара отклонена", TAG);
    }
    
    return 0;
}

/**
 * @brief Отправка работы на пул
 */
int stratum_send_work(pool_t *pool)
{
    /* TODO: Проверка очереди готовых шар и отправка */
    return 0;
}

/**
 * @brief Отправка найденной шары (mining.submit)
 */
int stratum_submit(pool_t *pool, const char *job_id, 
                   const char *nonce2, const char *ntime, const char *nonce)
{
    char msg[512];
    
    if (!pool || !job_id || !nonce2 || !ntime || !nonce) {
        return -1;
    }
    
    pool->seq_submit++;
    
    snprintf(msg, sizeof(msg),
            "{\"id\":%d,\"method\":\"mining.submit\","
            "\"params\":[\"%s\",\"%s\",\"%s\",\"%s\",\"%s\"]}\n",
            pool->seq_submit, pool->user, job_id, nonce2, ntime, nonce);
    
    log_message(LOG_DEBUG, "%s: -> %s", TAG, msg);
    pool->last_submit_time = time(NULL);
    
    return stratum_send_line(pool, msg);
}

/* ===========================================================================
 * КОНЕЦ ФАЙЛА stratum.c
 * =========================================================================== */
