/**
 * =============================================================================
 * @file    stratum.c
 * @brief   Реализация Stratum протокола для связи с майнинг-пулом
 * =============================================================================
 * 
 * Stratum - это протокол связи между майнером и пулом поверх TCP.
 * Использует JSON-RPC для обмена сообщениями.
 * 
 * Основные методы:
 * - mining.subscribe    - Подписка на задания
 * - mining.authorize    - Авторизация воркера
 * - mining.notify       - Получение нового задания
 * - mining.submit       - Отправка найденной шары
 * - mining.set_difficulty - Установка сложности
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
#include "miner.h"
#include "stratum.h"
#include "network.h"
#include "avalon10.h"

/* lwIP заголовки */
#include "lwip/sockets.h"
#include "lwip/netdb.h"

/* FreeRTOS */
#ifdef USE_FREERTOS
#include "FreeRTOS.h"
#include "task.h"
#include "semphr.h"
#include "queue.h"
#endif

/* =============================================================================
 * КОНСТАНТЫ
 * ============================================================================= */

/** Максимальный размер JSON сообщения */
#define STRATUM_MAX_MSG_SIZE    4096

/** Таймаут операций сокета (мс) */
#define STRATUM_SOCKET_TIMEOUT  30000

/** Интервал между ping запросами (мс) */
#define STRATUM_PING_INTERVAL   60000

/** Максимальное количество попыток переподключения */
#define STRATUM_MAX_RETRIES     10

/* =============================================================================
 * ЛОКАЛЬНЫЕ ПЕРЕМЕННЫЕ
 * ============================================================================= */

/** Сокет для связи с пулом */
static int stratum_socket = -1;

/** Флаг подписки */
static bool subscribed = false;

/** Флаг авторизации */
static bool authorized = false;

/** ID следующего JSON-RPC запроса */
static uint32_t rpc_id = 1;

/** Буфер для приёма данных */
static char recv_buffer[STRATUM_MAX_MSG_SIZE];

/** Позиция в буфере приёма */
static int recv_pos = 0;

/** Session ID от пула */
static char session_id[64] = {0};

/** ExtraNonce1 от пула */
static char extranonce1[32] = {0};

/** Размер ExtraNonce2 */
static int extranonce2_size = 4;

/** Текущая сложность */
static double current_difficulty = 1.0;

/** Текущее задание */
static stratum_job_t current_job = {0};

/** Мьютекс для защиты сокета */
static SemaphoreHandle_t socket_mutex = NULL;

/* =============================================================================
 * ВСПОМОГАТЕЛЬНЫЕ ФУНКЦИИ JSON
 * ============================================================================= */

/**
 * @brief Поиск строки в JSON
 * 
 * Простой парсер JSON для извлечения строковых значений.
 * 
 * @param json JSON строка
 * @param key Имя ключа
 * @param value [out] Буфер для значения
 * @param max_len Максимальная длина значения
 * @return true если ключ найден
 */
static bool json_get_string(const char *json, const char *key, char *value, int max_len)
{
    char pattern[64];
    snprintf(pattern, sizeof(pattern), "\"%s\":", key);
    
    const char *p = strstr(json, pattern);
    if (p == NULL) {
        return false;
    }
    
    p += strlen(pattern);
    
    /* Пропускаем пробелы */
    while (*p == ' ' || *p == '\t') p++;
    
    if (*p != '"') {
        return false;  /* Не строка */
    }
    p++;  /* Пропускаем открывающую кавычку */
    
    int i = 0;
    while (*p != '"' && *p != '\0' && i < max_len - 1) {
        if (*p == '\\' && *(p+1) != '\0') {
            p++;  /* Пропускаем escape символ */
        }
        value[i++] = *p++;
    }
    value[i] = '\0';
    
    return true;
}

/**
 * @brief Поиск числа в JSON
 * 
 * @param json JSON строка
 * @param key Имя ключа
 * @param value [out] Значение числа
 * @return true если ключ найден
 */
static bool json_get_int(const char *json, const char *key, int *value)
{
    char pattern[64];
    snprintf(pattern, sizeof(pattern), "\"%s\":", key);
    
    const char *p = strstr(json, pattern);
    if (p == NULL) {
        return false;
    }
    
    p += strlen(pattern);
    
    /* Пропускаем пробелы */
    while (*p == ' ' || *p == '\t') p++;
    
    *value = atoi(p);
    return true;
}

/**
 * @brief Поиск double в JSON
 * 
 * @param json JSON строка
 * @param key Имя ключа
 * @param value [out] Значение числа
 * @return true если ключ найден
 */
static bool json_get_double(const char *json, const char *key, double *value)
{
    char pattern[64];
    snprintf(pattern, sizeof(pattern), "\"%s\":", key);
    
    const char *p = strstr(json, pattern);
    if (p == NULL) {
        return false;
    }
    
    p += strlen(pattern);
    
    /* Пропускаем пробелы */
    while (*p == ' ' || *p == '\t') p++;
    
    *value = atof(p);
    return true;
}

/**
 * @brief Поиск массива в JSON
 * 
 * @param json JSON строка
 * @param key Имя ключа
 * @param array [out] Указатель на начало массива
 * @return Длина массива или -1 при ошибке
 */
static int json_get_array(const char *json, const char *key, const char **array)
{
    char pattern[64];
    snprintf(pattern, sizeof(pattern), "\"%s\":", key);
    
    const char *p = strstr(json, pattern);
    if (p == NULL) {
        return -1;
    }
    
    p += strlen(pattern);
    
    /* Пропускаем пробелы */
    while (*p == ' ' || *p == '\t') p++;
    
    if (*p != '[') {
        return -1;  /* Не массив */
    }
    
    *array = p;
    
    /* Ищем закрывающую скобку */
    int depth = 1;
    int len = 1;
    p++;
    
    while (depth > 0 && *p != '\0') {
        if (*p == '[') depth++;
        else if (*p == ']') depth--;
        else if (*p == '"') {
            /* Пропускаем строку */
            p++;
            while (*p != '"' && *p != '\0') {
                if (*p == '\\') p++;
                p++;
            }
        }
        p++;
        len++;
    }
    
    return len;
}

/**
 * @brief Преобразование HEX строки в байты
 * 
 * @param hex HEX строка
 * @param bytes [out] Массив байт
 * @param len Ожидаемое количество байт
 * @return true при успехе
 */
static bool hex_to_bytes(const char *hex, uint8_t *bytes, int len)
{
    for (int i = 0; i < len; i++) {
        char h = hex[i * 2];
        char l = hex[i * 2 + 1];
        
        if (h >= '0' && h <= '9') bytes[i] = (h - '0') << 4;
        else if (h >= 'a' && h <= 'f') bytes[i] = (h - 'a' + 10) << 4;
        else if (h >= 'A' && h <= 'F') bytes[i] = (h - 'A' + 10) << 4;
        else return false;
        
        if (l >= '0' && l <= '9') bytes[i] |= (l - '0');
        else if (l >= 'a' && l <= 'f') bytes[i] |= (l - 'a' + 10);
        else if (l >= 'A' && l <= 'F') bytes[i] |= (l - 'A' + 10);
        else return false;
    }
    
    return true;
}

/**
 * @brief Преобразование байт в HEX строку
 * 
 * @param bytes Массив байт
 * @param len Длина массива
 * @param hex [out] HEX строка (должен вместить len*2+1 символов)
 */
static void bytes_to_hex(const uint8_t *bytes, int len, char *hex)
{
    static const char hexchars[] = "0123456789abcdef";
    
    for (int i = 0; i < len; i++) {
        hex[i * 2] = hexchars[(bytes[i] >> 4) & 0x0F];
        hex[i * 2 + 1] = hexchars[bytes[i] & 0x0F];
    }
    hex[len * 2] = '\0';
}

/* =============================================================================
 * СЕТЕВЫЕ ФУНКЦИИ
 * ============================================================================= */

/**
 * @brief Отправка данных через сокет
 * 
 * @param data Данные для отправки
 * @param len Длина данных
 * @return Количество отправленных байт или -1 при ошибке
 */
static int stratum_send(const char *data, int len)
{
    if (stratum_socket < 0) {
        return -1;
    }
    
    xSemaphoreTake(socket_mutex, portMAX_DELAY);
    
    int sent = 0;
    while (sent < len) {
        int ret = send(stratum_socket, data + sent, len - sent, 0);
        if (ret <= 0) {
            xSemaphoreGive(socket_mutex);
            applog(LOG_ERR, "Stratum: ошибка отправки данных");
            return -1;
        }
        sent += ret;
    }
    
    xSemaphoreGive(socket_mutex);
    
    applog(LOG_DEBUG, "Stratum TX: %s", data);
    
    return sent;
}

/**
 * @brief Отправка JSON-RPC запроса
 * 
 * @param method Имя метода
 * @param params Параметры (JSON массив)
 * @return ID запроса или -1 при ошибке
 */
static int stratum_send_request(const char *method, const char *params)
{
    char msg[STRATUM_MAX_MSG_SIZE];
    int msg_id = rpc_id++;
    
    /* Формируем JSON-RPC запрос */
    int len = snprintf(msg, sizeof(msg),
        "{\"id\":%d,\"method\":\"%s\",\"params\":%s}\n",
        msg_id, method, params);
    
    if (stratum_send(msg, len) < 0) {
        return -1;
    }
    
    return msg_id;
}

/**
 * @brief Чтение строки из сокета
 * 
 * Читает данные до символа новой строки (\n).
 * 
 * @param line [out] Буфер для строки
 * @param max_len Максимальная длина строки
 * @param timeout_ms Таймаут в миллисекундах
 * @return Длина строки или -1 при ошибке
 */
static int stratum_recv_line(char *line, int max_len, int timeout_ms)
{
    if (stratum_socket < 0) {
        return -1;
    }
    
    /* Настраиваем таймаут */
    struct timeval tv;
    tv.tv_sec = timeout_ms / 1000;
    tv.tv_usec = (timeout_ms % 1000) * 1000;
    setsockopt(stratum_socket, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));
    
    /* Читаем данные */
    while (recv_pos < sizeof(recv_buffer) - 1) {
        /* Проверяем, есть ли уже полная строка в буфере */
        char *newline = memchr(recv_buffer, '\n', recv_pos);
        if (newline != NULL) {
            int len = newline - recv_buffer;
            if (len > max_len - 1) {
                len = max_len - 1;
            }
            
            memcpy(line, recv_buffer, len);
            line[len] = '\0';
            
            /* Удаляем прочитанную строку из буфера */
            int remaining = recv_pos - (len + 1);
            if (remaining > 0) {
                memmove(recv_buffer, newline + 1, remaining);
            }
            recv_pos = remaining;
            
            applog(LOG_DEBUG, "Stratum RX: %s", line);
            
            return len;
        }
        
        /* Читаем ещё данные */
        int ret = recv(stratum_socket, recv_buffer + recv_pos, 
                       sizeof(recv_buffer) - recv_pos - 1, 0);
        
        if (ret <= 0) {
            if (ret == 0) {
                applog(LOG_ERR, "Stratum: соединение закрыто");
            }
            return -1;  /* Ошибка или таймаут */
        }
        
        recv_pos += ret;
    }
    
    /* Буфер переполнен */
    applog(LOG_ERR, "Stratum: переполнение буфера приёма");
    recv_pos = 0;
    return -1;
}

/* =============================================================================
 * ОБРАБОТКА СООБЩЕНИЙ ПУЛА
 * ============================================================================= */

/**
 * @brief Обработка mining.notify
 * 
 * Получение нового задания от пула.
 * 
 * Формат параметров:
 * [job_id, prevhash, coinb1, coinb2, merkle_branch, version, nbits, ntime, clean_jobs]
 * 
 * @param params JSON массив параметров
 * @return 0 при успехе
 */
static int handle_mining_notify(const char *params)
{
    applog(LOG_INFO, "Получено новое задание от пула");
    
    /* Парсим параметры из JSON массива */
    const char *p = params;
    if (*p != '[') {
        return -1;
    }
    p++;  /* Пропускаем '[' */
    
    /* job_id */
    while (*p == ' ' || *p == '"') p++;
    const char *job_id_start = p;
    while (*p != '"' && *p != '\0') p++;
    int job_id_len = p - job_id_start;
    if (job_id_len > 31) job_id_len = 31;
    memcpy(current_job.job_id, job_id_start, job_id_len);
    current_job.job_id[job_id_len] = '\0';
    
    /* Пропускаем до следующего параметра */
    while (*p != ',' && *p != '\0') p++;
    if (*p == ',') p++;
    
    /* prevhash (64 hex символа = 32 байта) */
    while (*p == ' ' || *p == '"') p++;
    if (!hex_to_bytes(p, current_job.prev_hash, 32)) {
        applog(LOG_ERR, "Ошибка парсинга prevhash");
        return -2;
    }
    p += 64;
    
    while (*p != ',' && *p != '\0') p++;
    if (*p == ',') p++;
    
    /* coinb1 */
    while (*p == ' ' || *p == '"') p++;
    const char *coinb1_start = p;
    while (*p != '"' && *p != '\0') p++;
    int coinb1_len = (p - coinb1_start) / 2;
    if (coinb1_len > sizeof(current_job.coinbase1)) {
        coinb1_len = sizeof(current_job.coinbase1);
    }
    hex_to_bytes(coinb1_start, current_job.coinbase1, coinb1_len);
    current_job.coinbase1_len = coinb1_len;
    
    while (*p != ',' && *p != '\0') p++;
    if (*p == ',') p++;
    
    /* coinb2 */
    while (*p == ' ' || *p == '"') p++;
    const char *coinb2_start = p;
    while (*p != '"' && *p != '\0') p++;
    int coinb2_len = (p - coinb2_start) / 2;
    if (coinb2_len > sizeof(current_job.coinbase2)) {
        coinb2_len = sizeof(current_job.coinbase2);
    }
    hex_to_bytes(coinb2_start, current_job.coinbase2, coinb2_len);
    current_job.coinbase2_len = coinb2_len;
    
    while (*p != ',' && *p != '\0') p++;
    if (*p == ',') p++;
    
    /* merkle_branch (массив) */
    while (*p == ' ') p++;
    if (*p == '[') {
        p++;
        current_job.merkle_count = 0;
        
        while (*p != ']' && *p != '\0' && current_job.merkle_count < 16) {
            while (*p == ' ' || *p == '"' || *p == ',') p++;
            if (*p == ']') break;
            
            if (!hex_to_bytes(p, current_job.merkle_branch[current_job.merkle_count], 32)) {
                break;
            }
            current_job.merkle_count++;
            p += 64;
        }
        
        while (*p != ']' && *p != '\0') p++;
        if (*p == ']') p++;
    }
    
    while (*p != ',' && *p != '\0') p++;
    if (*p == ',') p++;
    
    /* version (8 hex символов = 4 байта) */
    while (*p == ' ' || *p == '"') p++;
    uint8_t version_bytes[4];
    if (hex_to_bytes(p, version_bytes, 4)) {
        current_job.version = ((uint32_t)version_bytes[0] << 24) |
                              ((uint32_t)version_bytes[1] << 16) |
                              ((uint32_t)version_bytes[2] << 8) |
                              ((uint32_t)version_bytes[3]);
    }
    p += 8;
    
    while (*p != ',' && *p != '\0') p++;
    if (*p == ',') p++;
    
    /* nbits (8 hex символов = 4 байта) */
    while (*p == ' ' || *p == '"') p++;
    uint8_t nbits_bytes[4];
    if (hex_to_bytes(p, nbits_bytes, 4)) {
        current_job.nbits = ((uint32_t)nbits_bytes[0] << 24) |
                            ((uint32_t)nbits_bytes[1] << 16) |
                            ((uint32_t)nbits_bytes[2] << 8) |
                            ((uint32_t)nbits_bytes[3]);
    }
    p += 8;
    
    while (*p != ',' && *p != '\0') p++;
    if (*p == ',') p++;
    
    /* ntime (8 hex символов = 4 байта) */
    while (*p == ' ' || *p == '"') p++;
    uint8_t ntime_bytes[4];
    if (hex_to_bytes(p, ntime_bytes, 4)) {
        current_job.ntime = ((uint32_t)ntime_bytes[0] << 24) |
                            ((uint32_t)ntime_bytes[1] << 16) |
                            ((uint32_t)ntime_bytes[2] << 8) |
                            ((uint32_t)ntime_bytes[3]);
    }
    p += 8;
    
    while (*p != ',' && *p != '\0') p++;
    if (*p == ',') p++;
    
    /* clean_jobs */
    while (*p == ' ') p++;
    current_job.clean_jobs = (*p == 't' || *p == 'T' || *p == '1');
    
    /* Устанавливаем сложность */
    current_job.difficulty = current_difficulty;
    
    /* Копируем extranonce1 */
    memcpy(current_job.extranonce1, extranonce1, sizeof(current_job.extranonce1));
    current_job.extranonce2_size = extranonce2_size;
    
    applog(LOG_INFO, "Job ID: %s, Clean: %s", 
           current_job.job_id,
           current_job.clean_jobs ? "да" : "нет");
    
    /* Отправляем задание в очередь для майнера */
    if (g_work_queue != NULL) {
        /* Преобразуем stratum_job_t в work_t и отправляем */
        work_t *work = malloc(sizeof(work_t));
        if (work != NULL) {
            memset(work, 0, sizeof(*work));
            strncpy(work->job_id, current_job.job_id, sizeof(work->job_id) - 1);
            memcpy(work->data + 4, current_job.prev_hash, 32);
            work->version = current_job.version;
            work->nbits = current_job.nbits;
            work->ntime = current_job.ntime;
            work->difficulty = current_job.difficulty;
            
            /* Отправляем в очередь */
            if (xQueueSend(g_work_queue, &work, pdMS_TO_TICKS(100)) != pdTRUE) {
                free(work);
                applog(LOG_WARNING, "Не удалось поставить задание в очередь");
            }
        }
    }
    
    return 0;
}

/**
 * @brief Обработка mining.set_difficulty
 * 
 * Установка новой сложности от пула.
 * 
 * @param params JSON массив параметров [difficulty]
 * @return 0 при успехе
 */
static int handle_set_difficulty(const char *params)
{
    /* Парсим сложность из [difficulty] */
    const char *p = params;
    if (*p == '[') p++;
    while (*p == ' ') p++;
    
    double new_diff = atof(p);
    
    if (new_diff > 0) {
        current_difficulty = new_diff;
        applog(LOG_INFO, "Новая сложность: %f", current_difficulty);
    }
    
    return 0;
}

/**
 * @brief Обработка ответа на subscribe
 * 
 * @param result JSON объект результата
 * @return 0 при успехе
 */
static int handle_subscribe_result(const char *result)
{
    /* Формат: [[["mining.set_difficulty", "session_id"], ["mining.notify", "session_id"]], "extranonce1", extranonce2_size] */
    
    /* Ищем extranonce1 */
    const char *p = result;
    
    /* Пропускаем вложенный массив */
    int depth = 0;
    while (*p != '\0') {
        if (*p == '[') depth++;
        else if (*p == ']') {
            depth--;
            if (depth == 0) {
                p++;
                break;
            }
        }
        p++;
    }
    
    /* Ищем extranonce1 */
    while (*p == ' ' || *p == ',') p++;
    if (*p == '"') {
        p++;
        const char *en1_start = p;
        while (*p != '"' && *p != '\0') p++;
        int en1_len = p - en1_start;
        if (en1_len > sizeof(extranonce1) - 1) {
            en1_len = sizeof(extranonce1) - 1;
        }
        memcpy(extranonce1, en1_start, en1_len);
        extranonce1[en1_len] = '\0';
        
        applog(LOG_INFO, "ExtraNonce1: %s", extranonce1);
    }
    
    /* Ищем extranonce2_size */
    while (*p != ',' && *p != '\0') p++;
    if (*p == ',') p++;
    while (*p == ' ') p++;
    
    extranonce2_size = atoi(p);
    applog(LOG_INFO, "ExtraNonce2 size: %d", extranonce2_size);
    
    subscribed = true;
    
    return 0;
}

/**
 * @brief Обработка ответа на authorize
 * 
 * @param result JSON значение результата (true/false)
 * @return 0 при успехе
 */
static int handle_authorize_result(const char *result)
{
    /* Пропускаем пробелы */
    while (*result == ' ') result++;
    
    if (*result == 't' || *result == 'T' || *result == '1') {
        authorized = true;
        applog(LOG_INFO, "Авторизация успешна");
    } else {
        authorized = false;
        applog(LOG_ERR, "Авторизация отклонена");
        return -1;
    }
    
    return 0;
}

/**
 * @brief Обработка ответа на submit
 * 
 * @param result JSON значение результата (true/false)
 * @param error Сообщение об ошибке (может быть NULL)
 * @return 0 если шара принята
 */
static int handle_submit_result(const char *result, const char *error)
{
    if (result != NULL) {
        while (*result == ' ') result++;
        
        if (*result == 't' || *result == 'T' || *result == '1') {
            applog(LOG_INFO, "Шара принята");
            total_accepted++;
            return 0;
        }
    }
    
    if (error != NULL) {
        applog(LOG_WARNING, "Шара отклонена: %s", error);
    } else {
        applog(LOG_WARNING, "Шара отклонена");
    }
    
    total_rejected++;
    return -1;
}

/* =============================================================================
 * ОСНОВНЫЕ ФУНКЦИИ STRATUM
 * ============================================================================= */

/**
 * @brief Подключение к майнинг-пулу
 * 
 * @param pool Указатель на структуру пула
 * @return 0 при успехе, <0 при ошибке
 */
int pool_connect(pool_t *pool)
{
    if (pool == NULL) {
        return -1;
    }
    
    applog(LOG_INFO, "Подключение к пулу %s:%d...", pool->hostname, pool->port);
    
    /* Создаём мьютекс если нужно */
    if (socket_mutex == NULL) {
        socket_mutex = xSemaphoreCreateMutex();
    }
    
    /* Разрешаем имя хоста */
    struct addrinfo hints = {0};
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;
    
    struct addrinfo *res;
    char port_str[16];
    snprintf(port_str, sizeof(port_str), "%d", pool->port);
    
    int ret = getaddrinfo(pool->hostname, port_str, &hints, &res);
    if (ret != 0) {
        applog(LOG_ERR, "Не удалось разрешить имя хоста: %s", pool->hostname);
        return -2;
    }
    
    /* Создаём сокет */
    stratum_socket = socket(res->ai_family, res->ai_socktype, res->ai_protocol);
    if (stratum_socket < 0) {
        applog(LOG_ERR, "Не удалось создать сокет");
        freeaddrinfo(res);
        return -3;
    }
    
    /* Настраиваем таймаут подключения */
    struct timeval tv;
    tv.tv_sec = STRATUM_SOCKET_TIMEOUT / 1000;
    tv.tv_usec = 0;
    setsockopt(stratum_socket, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));
    setsockopt(stratum_socket, SOL_SOCKET, SO_SNDTIMEO, &tv, sizeof(tv));
    
    /* Подключаемся */
    ret = connect(stratum_socket, res->ai_addr, res->ai_addrlen);
    freeaddrinfo(res);
    
    if (ret < 0) {
        applog(LOG_ERR, "Не удалось подключиться к пулу");
        close(stratum_socket);
        stratum_socket = -1;
        return -4;
    }
    
    applog(LOG_INFO, "TCP соединение установлено");
    
    /* Сбрасываем состояние */
    recv_pos = 0;
    subscribed = false;
    authorized = false;
    
    /* Отправляем mining.subscribe */
    char params[256];
    snprintf(params, sizeof(params), 
             "[\"%s/%s\", null]", 
             FIRMWARE_NAME, CGMINER_VERSION);
    
    if (stratum_send_request("mining.subscribe", params) < 0) {
        applog(LOG_ERR, "Ошибка отправки mining.subscribe");
        close(stratum_socket);
        stratum_socket = -1;
        return -5;
    }
    
    /* Ждём ответ на subscribe */
    char line[STRATUM_MAX_MSG_SIZE];
    if (stratum_recv_line(line, sizeof(line), STRATUM_SOCKET_TIMEOUT) < 0) {
        applog(LOG_ERR, "Таймаут ожидания ответа на subscribe");
        close(stratum_socket);
        stratum_socket = -1;
        return -6;
    }
    
    /* Парсим ответ */
    const char *result_start;
    int result_len = json_get_array(line, "result", &result_start);
    if (result_len > 0) {
        handle_subscribe_result(result_start);
    } else {
        applog(LOG_ERR, "Неверный ответ на subscribe");
        close(stratum_socket);
        stratum_socket = -1;
        return -7;
    }
    
    /* Отправляем mining.authorize */
    snprintf(params, sizeof(params),
             "[\"%s\", \"%s\"]",
             pool->worker, pool->password);
    
    if (stratum_send_request("mining.authorize", params) < 0) {
        applog(LOG_ERR, "Ошибка отправки mining.authorize");
        close(stratum_socket);
        stratum_socket = -1;
        return -8;
    }
    
    /* Ждём ответ на authorize */
    if (stratum_recv_line(line, sizeof(line), STRATUM_SOCKET_TIMEOUT) < 0) {
        applog(LOG_ERR, "Таймаут ожидания ответа на authorize");
        close(stratum_socket);
        stratum_socket = -1;
        return -9;
    }
    
    /* Проверяем результат */
    char result_str[32] = {0};
    if (json_get_string(line, "result", result_str, sizeof(result_str)) ||
        strstr(line, "\"result\":true") != NULL) {
        handle_authorize_result("true");
    } else {
        handle_authorize_result("false");
        close(stratum_socket);
        stratum_socket = -1;
        return -10;
    }
    
    applog(LOG_INFO, "Подключение к пулу успешно завершено");
    
    return 0;
}

/**
 * @brief Отключение от пула
 */
void pool_disconnect(void)
{
    if (stratum_socket >= 0) {
        close(stratum_socket);
        stratum_socket = -1;
    }
    
    subscribed = false;
    authorized = false;
    recv_pos = 0;
    
    applog(LOG_INFO, "Отключено от пула");
}

/**
 * @brief Обработка Stratum протокола
 * 
 * Вызывается периодически из задачи Stratum.
 * Читает и обрабатывает сообщения от пула.
 * 
 * @param pool Указатель на структуру пула
 * @return 0 при успехе, <0 при ошибке
 */
int stratum_process(pool_t *pool)
{
    if (stratum_socket < 0 || !subscribed || !authorized) {
        return -1;
    }
    
    /* Пытаемся прочитать сообщение (с коротким таймаутом) */
    char line[STRATUM_MAX_MSG_SIZE];
    int len = stratum_recv_line(line, sizeof(line), 100);  /* 100мс таймаут */
    
    if (len <= 0) {
        /* Нет данных - это нормально */
        return 0;
    }
    
    /* Определяем тип сообщения */
    char method[64] = {0};
    if (json_get_string(line, "method", method, sizeof(method))) {
        /* Это уведомление от пула */
        
        /* Находим params */
        const char *params;
        json_get_array(line, "params", &params);
        
        if (strcmp(method, "mining.notify") == 0) {
            handle_mining_notify(params);
        }
        else if (strcmp(method, "mining.set_difficulty") == 0) {
            handle_set_difficulty(params);
        }
        else if (strcmp(method, "client.reconnect") == 0) {
            applog(LOG_INFO, "Получен запрос на переподключение");
            pool_disconnect();
            return -2;  /* Требуется переподключение */
        }
        else if (strcmp(method, "client.get_version") == 0) {
            /* Отвечаем версией */
            int id;
            if (json_get_int(line, "id", &id)) {
                char response[256];
                int resp_len = snprintf(response, sizeof(response),
                    "{\"id\":%d,\"result\":\"%s/%s\",\"error\":null}\n",
                    id, FIRMWARE_NAME, CGMINER_VERSION);
                stratum_send(response, resp_len);
            }
        }
        else {
            applog(LOG_DEBUG, "Неизвестный метод: %s", method);
        }
    }
    else {
        /* Это ответ на наш запрос */
        /* Определяем по id какой это был запрос */
        
        char result[32] = {0};
        char error[256] = {0};
        
        if (json_get_string(line, "result", result, sizeof(result)) ||
            strstr(line, "\"result\":true") != NULL ||
            strstr(line, "\"result\":false") != NULL) {
            
            json_get_string(line, "error", error, sizeof(error));
            
            /* Предполагаем что это ответ на submit */
            if (strstr(line, "\"result\":true") != NULL) {
                handle_submit_result("true", NULL);
            } else {
                handle_submit_result("false", strlen(error) > 0 ? error : NULL);
            }
        }
    }
    
    return 0;
}

/**
 * @brief Отправка найденной шары на пул
 * 
 * @param work Указатель на структуру работы с найденным нонсом
 * @return true если шара отправлена успешно
 */
bool submit_work(const work_t *work)
{
    if (work == NULL || stratum_socket < 0 || !authorized) {
        return false;
    }
    
    /* Формируем extranonce2 из nonce2 */
    char extranonce2_hex[32];
    uint8_t en2_bytes[4];
    en2_bytes[0] = (work->nonce2 >> 24) & 0xFF;
    en2_bytes[1] = (work->nonce2 >> 16) & 0xFF;
    en2_bytes[2] = (work->nonce2 >> 8) & 0xFF;
    en2_bytes[3] = work->nonce2 & 0xFF;
    bytes_to_hex(en2_bytes, extranonce2_size, extranonce2_hex);
    
    /* Формируем ntime */
    char ntime_hex[16];
    uint8_t ntime_bytes[4];
    ntime_bytes[0] = (work->ntime >> 24) & 0xFF;
    ntime_bytes[1] = (work->ntime >> 16) & 0xFF;
    ntime_bytes[2] = (work->ntime >> 8) & 0xFF;
    ntime_bytes[3] = work->ntime & 0xFF;
    bytes_to_hex(ntime_bytes, 4, ntime_hex);
    
    /* Формируем nonce */
    char nonce_hex[16];
    uint8_t nonce_bytes[4];
    nonce_bytes[0] = (work->nonce >> 24) & 0xFF;
    nonce_bytes[1] = (work->nonce >> 16) & 0xFF;
    nonce_bytes[2] = (work->nonce >> 8) & 0xFF;
    nonce_bytes[3] = work->nonce & 0xFF;
    bytes_to_hex(nonce_bytes, 4, nonce_hex);
    
    /* Формируем параметры mining.submit */
    /* [worker, job_id, extranonce2, ntime, nonce] */
    char params[512];
    snprintf(params, sizeof(params),
             "[\"%s\", \"%s\", \"%s\", \"%s\", \"%s\"]",
             current_pool->worker,
             work->job_id,
             extranonce2_hex,
             ntime_hex,
             nonce_hex);
    
    applog(LOG_INFO, "Отправка шары: job=%s, nonce=0x%08X",
           work->job_id, work->nonce);
    
    /* Отправляем */
    if (stratum_send_request("mining.submit", params) < 0) {
        applog(LOG_ERR, "Ошибка отправки шары");
        return false;
    }
    
    return true;
}

/**
 * @brief Проверка состояния подключения
 * 
 * @return true если подключено и авторизовано
 */
bool stratum_is_connected(void)
{
    return (stratum_socket >= 0 && subscribed && authorized);
}
