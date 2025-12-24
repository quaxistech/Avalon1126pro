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
#include <limits.h>

#include <FreeRTOS.h>
#include <task.h>

#include "stratum.h"
#include "pool.h"
#include "cgminer.h"
#include "network.h"
#include "work.h"
#include "mock_hardware.h"

/* ===========================================================================
 * ЛОКАЛЬНЫЕ КОНСТАНТЫ
 * =========================================================================== */

static const char *TAG = "Stratum";

#define STRATUM_RECV_BUFSIZE    4096
#define STRATUM_LINE_BUFSIZE    2048

/* ===========================================================================
 * ЛОКАЛЬНЫЕ ПЕРЕМЕННЫЕ
 * =========================================================================== */

/* Буфер приёма для накопления неполных строк */
static char recv_buffer[STRATUM_RECV_BUFSIZE];
static int recv_buffer_len = 0;

/* Текущая работа, полученная от пула */
static work_t *current_work = NULL;

/* ===========================================================================
 * ВСПОМОГАТЕЛЬНЫЕ ФУНКЦИИ ПАРСИНГА JSON
 * =========================================================================== */

/**
 * @brief Простой парсер JSON - получение строкового значения по ключу
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
    
    /* Пропускаем пробелы */
    start++;
    while (*start == ' ' || *start == '\t') start++;
    
    if (*start != '"') return -1;
    start++;
    
    end = strchr(start, '"');
    if (!end) return -1;
    
    int vlen = end - start;
    if (vlen >= len) vlen = len - 1;
    
    strncpy(buf, start, vlen);
    buf[vlen] = '\0';
    
    return 0;
}

/**
 * @brief Парсинг числового значения
 */
static int json_get_int(const char *json, const char *key, int *value)
{
    char search[64];
    const char *start;
    
    snprintf(search, sizeof(search), "\"%s\":", key);
    start = strstr(json, search);
    if (!start) return -1;
    
    start = strchr(start, ':');
    if (!start) return -1;
    start++;
    
    while (*start == ' ' || *start == '\t') start++;
    
    *value = atoi(start);
    return 0;
}

/**
 * @brief Парсинг значения с плавающей точкой
 */
static int json_get_double(const char *json, const char *key, double *value)
{
    char search[64];
    const char *start;
    
    snprintf(search, sizeof(search), "\"%s\":", key);
    start = strstr(json, search);
    if (!start) return -1;
    
    start = strchr(start, ':');
    if (!start) return -1;
    start++;
    
    while (*start == ' ' || *start == '\t') start++;
    
    *value = atof(start);
    return 0;
}

/**
 * @brief Парсинг boolean значения
 */
static int json_get_bool(const char *json, const char *key, int *value)
{
    char search[64];
    const char *start;
    
    snprintf(search, sizeof(search), "\"%s\":", key);
    start = strstr(json, search);
    if (!start) return -1;
    
    start = strchr(start, ':');
    if (!start) return -1;
    start++;
    
    while (*start == ' ' || *start == '\t') start++;
    
    if (strncmp(start, "true", 4) == 0) {
        *value = 1;
    } else if (strncmp(start, "false", 5) == 0) {
        *value = 0;
    } else {
        return -1;
    }
    
    return 0;
}

/**
 * @brief Преобразование hex-строки в байты
 */
static int hex_decode(const char *hex, uint8_t *out, size_t max_len)
{
    size_t hex_len = strlen(hex);
    size_t byte_len = hex_len / 2;
    
    if (byte_len > max_len) byte_len = max_len;
    
    for (size_t i = 0; i < byte_len; i++) {
        unsigned int b;
        if (sscanf(hex + i * 2, "%2x", &b) != 1) {
            return -1;
        }
        out[i] = (uint8_t)b;
    }
    
    return (int)byte_len;
}

static int parse_subscribe_result(pool_t *pool, const char *line)
{
    const char *p;
    const char *ex1_start = NULL;
    const char *ex1_end = NULL;
    char ex1_buf[64] = {0};
    int depth = 0;
    
    if (!pool || !line) {
        return -1;
    }
    
    p = strstr(line, "\"result\"");
    if (!p) {
        return -1;
    }
    
    p = strchr(p, '[');
    if (!p) {
        return -1;
    }
    
    /* Ищем конец первого элемента (подписки) в result,
     * первый символ ',' на глубине 1 после закрытия первой скобки */
    for (; *p; p++) {
        if (*p == '[') depth++;
        else if (*p == ']') depth--;
        
        if (depth == 1 && *p == ',' && *(p + 1) == '"') {
            ex1_start = p + 2; /* Указатель на начало extranonce1 */
            break;
        }
    }
    
    if (!ex1_start) {
        return -1;
    }
    
    ex1_end = strchr(ex1_start, '"');
    if (!ex1_end || ex1_end <= ex1_start) {
        return -1;
    }
    
    size_t ex1_len = (size_t)(ex1_end - ex1_start);
    if (ex1_len == 0) {
        return -1;
    }
    if (ex1_len >= sizeof(ex1_buf)) {
        ex1_len = sizeof(ex1_buf) - 1;
    }
    memcpy(ex1_buf, ex1_start, ex1_len);
    ex1_buf[ex1_len] = '\0';
    
    int decoded = hex_decode(ex1_buf, (uint8_t *)pool->extranonce1, sizeof(pool->extranonce1));
    if (decoded > 0) {
        pool->extranonce1_len = decoded;
    } else {
        return -1;
    }
    
    /* extranonce2_size после следующей запятой */
    const char *size_ptr = strchr(ex1_end, ',');
    if (size_ptr && *(size_ptr + 1) != '\0') {
        char *endptr = NULL;
        long val = strtol(size_ptr + 1, &endptr, 10);
        if (endptr && endptr > size_ptr + 1 && val >= 1 && val <= 8) {
            pool->extranonce2_len = (int)val;
        }
        if (pool->extranonce2_len < 1) pool->extranonce2_len = 4;
        if (pool->extranonce2_len > 8) pool->extranonce2_len = 8;
    }
    
    log_message(LOG_INFO, "%s: Подписка: extranonce1_len=%d, extranonce2_len=%d",
                TAG, pool->extranonce1_len, pool->extranonce2_len);
    
    return 0;
}

/**
 * @brief Преобразование байт в hex-строку
 */
static void hex_encode(const uint8_t *data, size_t len, char *out)
{
    for (size_t i = 0; i < len; i++) {
        sprintf(out + i * 2, "%02x", data[i]);
    }
    out[len * 2] = '\0';
}

/* ===========================================================================
 * ПАРСИНГ MINING.NOTIFY
 * =========================================================================== */

/**
 * @brief Парсинг mining.notify сообщения
 * 
 * Формат params:
 * [0] job_id - строка идентификатора задания
 * [1] prevhash - 32 байта в hex (64 символа)
 * [2] coinbase1 - первая часть coinbase транзакции
 * [3] coinbase2 - вторая часть coinbase транзакции
 * [4] merkle_branch - массив хэшей для построения merkle root
 * [5] version - версия блока (hex)
 * [6] nbits - сложность в компактной форме (hex)
 * [7] ntime - временная метка (hex)
 * [8] clean_jobs - если true, отменить предыдущие работы
 */
static int parse_mining_notify(pool_t *pool, const char *params)
{
    work_t *work;
    const char *ptr = params;
    char buf[256];
    int i;
    
    /* Создаём новую работу */
    work = create_work();
    if (!work) {
        log_message(LOG_ERR, "%s: Не удалось создать work", TAG);
        return -1;
    }
    
    work->pool_no = pool->pool_no;
    
    /* Ищем начало массива params */
    ptr = strchr(ptr, '[');
    if (!ptr) goto parse_error;
    ptr++;
    
    /* [0] job_id */
    ptr = strchr(ptr, '"');
    if (!ptr) goto parse_error;
    ptr++;
    
    i = 0;
    while (*ptr && *ptr != '"' && i < MAX_JOB_ID_LEN - 1) {
        work->job_id[i++] = *ptr++;
    }
    work->job_id[i] = '\0';
    
    if (*ptr != '"') goto parse_error;
    ptr++;
    
    /* [1] prevhash */
    ptr = strchr(ptr, '"');
    if (!ptr) goto parse_error;
    ptr++;
    
    memset(buf, 0, sizeof(buf));
    i = 0;
    while (*ptr && *ptr != '"' && i < 64) {
        buf[i++] = *ptr++;
    }
    buf[i] = '\0';
    
    if (i != 64) {
        log_message(LOG_WARNING, "%s: Неверная длина prevhash: %d", TAG, i);
    }
    
    hex_decode(buf, work->prevhash, 32);
    
    if (*ptr != '"') goto parse_error;
    ptr++;
    
    /* [2] coinbase1 */
    ptr = strchr(ptr, '"');
    if (!ptr) goto parse_error;
    ptr++;
    
    i = 0;
    while (*ptr && *ptr != '"' && i < sizeof(buf) - 1) {
        buf[i++] = *ptr++;
    }
    buf[i] = '\0';
    
    work->coinbase1_len = hex_decode(buf, work->coinbase1, sizeof(work->coinbase1));
    
    if (*ptr != '"') goto parse_error;
    ptr++;
    
    /* [3] coinbase2 */
    ptr = strchr(ptr, '"');
    if (!ptr) goto parse_error;
    ptr++;
    
    i = 0;
    while (*ptr && *ptr != '"' && i < sizeof(buf) - 1) {
        buf[i++] = *ptr++;
    }
    buf[i] = '\0';
    
    work->coinbase2_len = hex_decode(buf, work->coinbase2, sizeof(work->coinbase2));
    
    if (*ptr != '"') goto parse_error;
    ptr++;
    
    /* [4] merkle_branch - массив */
    ptr = strchr(ptr, '[');
    if (!ptr) goto parse_error;
    ptr++;
    
    work->merkle_count = 0;
    while (*ptr && *ptr != ']' && work->merkle_count < 16) {
        if (*ptr == '"') {
            ptr++;
            i = 0;
            while (*ptr && *ptr != '"' && i < 64) {
                buf[i++] = *ptr++;
            }
            buf[i] = '\0';
            
            if (i == 64) {
                hex_decode(buf, work->merkle_branch[work->merkle_count], 32);
                work->merkle_count++;
            }
            
            if (*ptr == '"') ptr++;
        } else {
            ptr++;
        }
    }
    
    if (*ptr == ']') ptr++;
    
    /* [5] version */
    ptr = strchr(ptr, '"');
    if (!ptr) goto parse_error;
    ptr++;
    
    i = 0;
    while (*ptr && *ptr != '"' && i < 8) {
        buf[i++] = *ptr++;
    }
    buf[i] = '\0';
    
    work->version = (uint32_t)strtoul(buf, NULL, 16);
    
    if (*ptr != '"') goto parse_error;
    ptr++;
    
    /* [6] nbits */
    ptr = strchr(ptr, '"');
    if (!ptr) goto parse_error;
    ptr++;
    
    i = 0;
    while (*ptr && *ptr != '"' && i < 8) {
        buf[i++] = *ptr++;
    }
    buf[i] = '\0';
    
    work->nbits = (uint32_t)strtoul(buf, NULL, 16);
    
    if (*ptr != '"') goto parse_error;
    ptr++;
    
    /* [7] ntime */
    ptr = strchr(ptr, '"');
    if (!ptr) goto parse_error;
    ptr++;
    
    i = 0;
    while (*ptr && *ptr != '"' && i < 8) {
        buf[i++] = *ptr++;
    }
    buf[i] = '\0';
    
    work->ntime = (uint32_t)strtoul(buf, NULL, 16);
    
    /* [8] clean_jobs (опционально) */
    int clean = 0;
    if (strstr(ptr, "true")) {
        clean = 1;
    }
    
    work->timestamp = time(NULL);
    
    /* ExtraNonce из пула */
    if (pool->extranonce1_len > 0) {
        work->extranonce1_len = pool->extranonce1_len;
        memcpy(work->extranonce1, pool->extranonce1, pool->extranonce1_len);
    }
    work->nonce2_len = pool->extranonce2_len > 0 ? pool->extranonce2_len : work->nonce2_len;
    memset(work->nonce2, 0, sizeof(work->nonce2));
    
    /* Формируем заголовок блока */
    work_to_header(work);
    
    /* Сохраняем как текущую работу */
    if (current_work) {
        current_work->stale = 1;
        free_work(current_work);
    }
    current_work = work;
    
    log_message(LOG_INFO, "%s: Новое задание: job=%s, merkle=%d, clean=%d", 
               TAG, work->job_id, work->merkle_count, clean);
    
    pool->getworks++;
    pool->last_work_time = time(NULL);
    
    return 0;
    
parse_error:
    log_message(LOG_ERR, "%s: Ошибка парсинга mining.notify", TAG);
    free_work(work);
    return -1;
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
    
    /* Сбрасываем буфер приёма */
    recv_buffer_len = 0;
    
    /* Подключение к TCP сокету уже выполнено в connect_pool() */
    
    /* Отправляем mining.subscribe */
    if (stratum_subscribe(pool) < 0) {
        log_message(LOG_ERR, "%s: Ошибка subscribe", TAG);
        return -1;
    }
    
    /* Ждём ответ на subscribe */
    vTaskDelay(pdMS_TO_TICKS(500));
    stratum_process_responses(pool);
    
    /* Отправляем mining.authorize */
    if (stratum_authorize(pool) < 0) {
        log_message(LOG_ERR, "%s: Ошибка authorize", TAG);
        return -1;
    }
    
    /* Ждём ответ на authorize */
    vTaskDelay(pdMS_TO_TICKS(500));
    stratum_process_responses(pool);
    
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
    
    recv_buffer_len = 0;
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
 * 
 * Читает данные из сокета и возвращает полную строку (до \n).
 * Неполные строки накапливаются в буфере.
 */
char *stratum_recv_line(pool_t *pool)
{
    if (!pool || pool->sock < 0) {
        return NULL;
    }
    
    /* Проверяем, есть ли уже полная строка в буфере */
    char *newline = strchr(recv_buffer, '\n');
    if (newline) {
        /* Извлекаем строку */
        int line_len = newline - recv_buffer + 1;
        char *line = (char *)malloc(line_len + 1);
        if (!line) return NULL;
        
        memcpy(line, recv_buffer, line_len);
        line[line_len] = '\0';
        
        /* Сдвигаем оставшиеся данные */
        recv_buffer_len -= line_len;
        memmove(recv_buffer, newline + 1, recv_buffer_len);
        recv_buffer[recv_buffer_len] = '\0';
        
        return line;
    }
    
    /* Читаем новые данные */
    int space = sizeof(recv_buffer) - recv_buffer_len - 1;
    if (space <= 0) {
        /* Буфер переполнен, сбрасываем */
        recv_buffer_len = 0;
        recv_buffer[0] = '\0';
        return NULL;
    }
    
    int len = network_socket_recv(pool->sock, recv_buffer + recv_buffer_len, space, 100);
    
    if (len > 0) {
        recv_buffer_len += len;
        recv_buffer[recv_buffer_len] = '\0';
        
        /* Проверяем на полную строку */
        newline = strchr(recv_buffer, '\n');
        if (newline) {
            int line_len = newline - recv_buffer + 1;
            char *line = (char *)malloc(line_len + 1);
            if (!line) return NULL;
            
            memcpy(line, recv_buffer, line_len);
            line[line_len] = '\0';
            
            recv_buffer_len -= line_len;
            memmove(recv_buffer, newline + 1, recv_buffer_len);
            recv_buffer[recv_buffer_len] = '\0';
            
            return line;
        }
    }
    else if (len == 0) {
        /* Соединение закрыто пулом */
        log_message(LOG_WARNING, "%s: Соединение закрыто пулом", TAG);
        if (pool->sock >= 0) {
            network_socket_close(pool->sock);
            pool->sock = -1;
        }
        stratum_disconnect(pool);
        pool->state = POOL_STATE_DEAD;
    }
    
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
    
    size_t len = strlen(str);
    int sent = network_socket_send(pool->sock, str, len);
    
    if (sent != (int)len) {
        log_message(LOG_ERR, "%s: Ошибка отправки: отправлено %d из %zu", TAG, sent, len);
        return -1;
    }
    
    return 0;
}

/**
 * @brief Парсинг JSON ответа от пула
 */
int stratum_parse_response(pool_t *pool, const char *line)
{
    if (!pool || !line) return -1;
    
    /* Убираем лишний перевод строки для вывода в лог */
    char clean_line[512];
    strncpy(clean_line, line, sizeof(clean_line) - 1);
    clean_line[sizeof(clean_line) - 1] = '\0';
    char *nl = strchr(clean_line, '\n');
    if (nl) *nl = '\0';
    
    log_message(LOG_DEBUG, "%s: <- %s", TAG, clean_line);
    
    /* Проверяем тип сообщения */
    if (strstr(line, "mining.notify")) {
        /* Новое задание от пула */
        const char *params = strstr(line, "\"params\"");
        if (params) {
            params = strchr(params, ':');
            if (params) {
                parse_mining_notify(pool, params + 1);
            }
        }
        
    } else if (strstr(line, "mining.subscribe") || strstr(line, "\"result\":[[[\"mining.notify\"")) {
        /* Ответ на subscribe содержит extranonce1 и extranonce2_size */
        parse_subscribe_result(pool, line);
        
    } else if (strstr(line, "mining.set_difficulty")) {
        /* Установка сложности */
        double diff = 0;
        const char *params = strstr(line, "\"params\"");
        if (params) {
            params = strchr(params, '[');
            if (params) {
                params++;
                diff = atof(params);
            }
        }
        
        if (diff > 0) {
            pool->sdiff = diff;
            log_message(LOG_INFO, "%s: Сложность: %.2f", TAG, pool->sdiff);
        }
        
    } else if (strstr(line, "mining.set_extranonce")) {
        /* Установка нового ExtraNonce от пула (BIP 310)
         * Пул может изменить extranonce во время сессии */
        char extranonce1_hex[64] = {0};
        
        /* Парсим параметры: ["extranonce1", extranonce2_size] */
        const char *params = strstr(line, "\"params\"");
        if (params) {
            params = strchr(params, '[');
            if (params) {
                params++;  /* Пропускаем [ */
                /* Ищем первую строку в кавычках */
                const char *start = strchr(params, '"');
                if (start) {
                    start++;
                    const char *end = strchr(start, '"');
                    if (end && (end - start) < (int)sizeof(extranonce1_hex)) {
                        strncpy(extranonce1_hex, start, end - start);
                        
                        /* Декодируем hex в бинарный формат */
                        int len = hex_decode(extranonce1_hex, (uint8_t *)pool->extranonce1, 
                                            sizeof(pool->extranonce1));
                        if (len > 0) {
                            pool->extranonce1_len = len;
                            
                            /* Ищем extranonce2_size после запятой */
                            const char *comma = strchr(end, ',');
                            if (comma) {
                                pool->extranonce2_len = atoi(comma + 1);
                                if (pool->extranonce2_len < 1) pool->extranonce2_len = 4;
                                if (pool->extranonce2_len > 8) pool->extranonce2_len = 8;
                            }
                            
                            log_message(LOG_INFO, "%s: ExtraNonce обновлён: len1=%d, len2=%d", 
                                       TAG, pool->extranonce1_len, pool->extranonce2_len);
                        }
                    }
                }
            }
        }
        
    } else if (strstr(line, "\"result\":true")) {
        /* Успешный результат */
        int id = 0;
        json_get_int(line, "id", &id);
        
        if (id > 0) {
            /* Это ответ на наш запрос */
            if (!pool->stratum_auth) {
                pool->stratum_auth = 1;
                log_message(LOG_INFO, "%s: Авторизация успешна", TAG);
            } else {
                /* Шара принята */
                pool->accepted++;
                pool->total_diff += pool->sdiff;
                log_message(LOG_INFO, "%s: Шара принята! Всего: %llu", 
                           TAG, (unsigned long long)pool->accepted);
            }
        }
        
    } else if (strstr(line, "\"result\":false") || strstr(line, "\"error\":")) {
        /* Ошибка или отклонение */
        int id = 0;
        json_get_int(line, "id", &id);
        
        if (strstr(line, "\"error\":null") && !strstr(line, "\"result\":false")) {
            /* Это не ошибка, просто null error при успехе */
        } else {
            pool->rejected++;
            log_message(LOG_WARNING, "%s: Шара отклонена или ошибка", TAG);
        }
    }
    
    return 0;
}

/**
 * @brief Обработка всех доступных ответов
 */
int stratum_process_responses(pool_t *pool)
{
    char *line;
    int count = 0;
    
    while ((line = stratum_recv_line(pool)) != NULL) {
        stratum_parse_response(pool, line);
        free(line);
        count++;
    }
    
    return count;
}

/**
 * @brief Отправка работы на пул (проверка очереди шар)
 */
int stratum_send_work(pool_t *pool)
{
    /* Эта функция вызывается из задачи stratum_send */
    /* Обрабатываем входящие сообщения */
    stratum_process_responses(pool);
    
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

/**
 * @brief Получение текущей работы
 */
work_t *stratum_get_current_work(void)
{
    return current_work;
}

/**
 * @brief Отправка найденного nonce на пул
 * 
 * Конвертирует бинарные данные в hex и вызывает stratum_submit.
 * 
 * @param pool  Указатель на пул
 * @param work  Указатель на работу с найденным nonce
 * @return      0 при успехе
 */
int stratum_submit_nonce(pool_t *pool, work_t *work)
{
    if (!pool || !work) return -1;
    
    char nonce2_hex[32] = {0};
    char ntime_hex[16] = {0};
    char nonce_hex[16] = {0};
    
    /* Конвертируем nonce2 в hex */
    for (int i = 0; i < work->nonce2_len && i < 8; i++) {
        sprintf(nonce2_hex + i * 2, "%02x", work->nonce2[i]);
    }
    
    /* Конвертируем ntime в hex (big-endian для Stratum) */
    sprintf(ntime_hex, "%08x", work->ntime);
    
    /* Конвертируем nonce в hex (big-endian для Stratum) */
    sprintf(nonce_hex, "%08x", work->nonce);
    
    log_message(LOG_INFO, "%s: Submit: job=%s, nonce2=%s, ntime=%s, nonce=%s",
               TAG, work->job_id, nonce2_hex, ntime_hex, nonce_hex);
    
    return stratum_submit(pool, work->job_id, nonce2_hex, ntime_hex, nonce_hex);
}

/* ===========================================================================
 * КОНЕЦ ФАЙЛА stratum.c
 * =========================================================================== */
