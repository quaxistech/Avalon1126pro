/**
 * =============================================================================
 * @file    stratum.h
 * @brief   Заголовочный файл Stratum протокола
 * =============================================================================
 * 
 * Определения для связи с майнинг-пулом по протоколу Stratum.
 * 
 * @author  Reconstructed from Avalon A1126pro firmware
 * @version 1.0
 * =============================================================================
 */

#ifndef _STRATUM_H_
#define _STRATUM_H_

#include <stdint.h>
#include <stdbool.h>

#include "miner.h"

#ifdef __cplusplus
extern "C" {
#endif

/* =============================================================================
 * КОНСТАНТЫ
 * ============================================================================= */

/** Максимальная длина job_id */
#define STRATUM_JOB_ID_MAX      32

/** Максимальная длина extranonce */
#define STRATUM_EXTRANONCE_MAX  16

/** Максимальное количество merkle веток */
#define STRATUM_MERKLE_MAX      16

/** Максимальный размер coinbase */
#define STRATUM_COINBASE_MAX    256

/* =============================================================================
 * СТРУКТУРЫ ДАННЫХ
 * ============================================================================= */

/**
 * @brief Структура задания Stratum
 * 
 * Содержит все данные, полученные от пула через mining.notify
 */
typedef struct {
    /** Идентификатор задания */
    char job_id[STRATUM_JOB_ID_MAX];
    
    /** Хеш предыдущего блока (32 байта) */
    uint8_t prev_hash[32];
    
    /** Coinbase часть 1 */
    uint8_t coinbase1[STRATUM_COINBASE_MAX];
    int coinbase1_len;
    
    /** Coinbase часть 2 */
    uint8_t coinbase2[STRATUM_COINBASE_MAX];
    int coinbase2_len;
    
    /** Merkle ветви */
    uint8_t merkle_branch[STRATUM_MERKLE_MAX][32];
    int merkle_count;
    
    /** Версия блока */
    uint32_t version;
    
    /** Сложность (nBits) */
    uint32_t nbits;
    
    /** Временная метка (nTime) */
    uint32_t ntime;
    
    /** Флаг очистки старых заданий */
    bool clean_jobs;
    
    /** Текущая сложность */
    double difficulty;
    
    /** ExtraNonce1 от пула */
    char extranonce1[STRATUM_EXTRANONCE_MAX];
    
    /** Размер ExtraNonce2 */
    int extranonce2_size;
    
} stratum_job_t;

/**
 * @brief Структура найденной шары для отправки
 */
typedef struct {
    /** Идентификатор задания */
    char job_id[STRATUM_JOB_ID_MAX];
    
    /** ExtraNonce2 (hex строка) */
    char extranonce2[STRATUM_EXTRANONCE_MAX * 2 + 1];
    
    /** nTime (hex строка) */
    char ntime[9];
    
    /** Найденный nonce (hex строка) */
    char nonce[9];
    
} stratum_submit_t;

/* =============================================================================
 * ГЛОБАЛЬНЫЕ ПЕРЕМЕННЫЕ (объявлены в main.c или pool_manager.c)
 * ============================================================================= */

/** Текущий активный пул */
extern pool_t *current_pool;

/** Массив пулов */
extern pool_t pools[];

/** Количество сконфигурированных пулов */
extern int pool_count;

/** Очередь заданий для майнера */
extern QueueHandle_t g_work_queue;

/** Счётчики статистики */
extern uint32_t total_accepted;
extern uint32_t total_rejected;

/* =============================================================================
 * ФУНКЦИИ
 * ============================================================================= */

/**
 * @brief Подключение к майнинг-пулу
 * 
 * Устанавливает TCP соединение, выполняет subscribe и authorize.
 * 
 * @param pool Указатель на структуру пула
 * @return 0 при успехе, <0 при ошибке
 */
int pool_connect(pool_t *pool);

/**
 * @brief Отключение от пула
 * 
 * Закрывает соединение и сбрасывает состояние.
 */
void pool_disconnect(void);

/**
 * @brief Обработка Stratum протокола
 * 
 * Читает и обрабатывает входящие сообщения от пула.
 * Должна вызываться периодически из основного цикла.
 * 
 * @param pool Указатель на структуру пула
 * @return 0 при успехе, <0 при ошибке (требуется переподключение)
 */
int stratum_process(pool_t *pool);

/**
 * @brief Отправка найденной шары на пул
 * 
 * @param work Указатель на структуру работы с найденным нонсом
 * @return true если шара отправлена успешно
 */
bool submit_work(const work_t *work);

/**
 * @brief Проверка состояния подключения
 * 
 * @return true если подключено и авторизовано
 */
bool stratum_is_connected(void);

#ifdef __cplusplus
}
#endif

#endif /* _STRATUM_H_ */
