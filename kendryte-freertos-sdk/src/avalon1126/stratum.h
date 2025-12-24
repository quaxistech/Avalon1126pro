/**
 * =============================================================================
 * @file    stratum.h
 * @brief   Avalon A1126pro - Stratum протокол (заголовочный файл)
 * @version 1.0
 * @date    2024
 * =============================================================================
 * 
 * ОПИСАНИЕ:
 * Реализация Stratum протокола для связи с пулами майнинга Bitcoin.
 * 
 * STRATUM КОМАНДЫ:
 * - mining.subscribe    - Подписка на уведомления
 * - mining.authorize    - Авторизация воркера
 * - mining.notify       - Новое задание от пула
 * - mining.submit       - Отправка найденной шары
 * - mining.set_difficulty - Установка сложности
 * 
 * =============================================================================
 */

#ifndef __STRATUM_H__
#define __STRATUM_H__

#include <stdint.h>
#include "pool.h"

/* ===========================================================================
 * КОНСТАНТЫ
 * =========================================================================== */

/**
 * @brief Версия Stratum протокола
 */
#define STRATUM_VERSION     "1.0"

/**
 * @brief Максимальный размер JSON сообщения
 */
#define STRATUM_MAX_MSG     4096

/* ===========================================================================
 * ПРОТОТИПЫ ФУНКЦИЙ
 * =========================================================================== */

/**
 * @brief Подключение к пулу по Stratum
 * 
 * Выполняет mining.subscribe и mining.authorize
 * 
 * @param pool  Указатель на пул
 * @return      0 при успехе
 */
int stratum_connect(pool_t *pool);

/**
 * @brief Отключение от пула
 * 
 * @param pool  Указатель на пул
 */
void stratum_disconnect(pool_t *pool);

/**
 * @brief Чтение строки от пула
 * 
 * @param pool  Указатель на пул
 * @return      Указатель на строку (нужно освободить) или NULL
 */
char *stratum_recv_line(pool_t *pool);

/**
 * @brief Отправка строки на пул
 * 
 * @param pool  Указатель на пул
 * @param str   Строка для отправки
 * @return      0 при успехе
 */
int stratum_send_line(pool_t *pool, const char *str);

/**
 * @brief Парсинг JSON ответа от пула
 * 
 * Обрабатывает mining.notify, mining.set_difficulty и другие команды
 * 
 * @param pool  Указатель на пул
 * @param line  JSON строка
 * @return      0 при успехе
 */
int stratum_parse_response(pool_t *pool, const char *line);

/**
 * @brief Отправка работы на пул
 * 
 * Проверяет наличие готовых шар и отправляет их
 * 
 * @param pool  Указатель на пул
 * @return      Количество отправленных шар
 */
int stratum_send_work(pool_t *pool);

/**
 * @brief Отправка найденной шары (mining.submit)
 * 
 * @param pool      Указатель на пул
 * @param job_id    ID задания
 * @param nonce2    ExtraNonce2
 * @param ntime     Время
 * @param nonce     Найденный nonce
 * @return          0 при успехе
 */
int stratum_submit(pool_t *pool, const char *job_id, 
                   const char *nonce2, const char *ntime, const char *nonce);

/**
 * @brief Отправка mining.subscribe
 * 
 * @param pool  Указатель на пул
 * @return      0 при успехе
 */
int stratum_subscribe(pool_t *pool);

/**
 * @brief Отправка mining.authorize
 * 
 * @param pool  Указатель на пул
 * @return      0 при успехе
 */
int stratum_authorize(pool_t *pool);

/**
 * @brief Обработка всех доступных ответов от пула
 * 
 * @param pool  Указатель на пул
 * @return      Количество обработанных сообщений
 */
int stratum_process_responses(pool_t *pool);

/**
 * @brief Получение текущей работы
 * 
 * @return Указатель на текущую работу или NULL
 */
struct work *stratum_get_current_work(void);

/**
 * @brief Отправка найденного nonce на пул (mining.submit)
 * 
 * Формирует JSON сообщение и отправляет на пул.
 * Формат: {"id":N,"method":"mining.submit","params":["user","job_id","nonce2","ntime","nonce"]}
 * 
 * @param pool  Указатель на пул
 * @param work  Указатель на работу с найденным nonce
 * @return      0 при успехе, -1 при ошибке
 */
int stratum_submit_nonce(pool_t *pool, work_t *work);

#endif /* __STRATUM_H__ */
