/**
 * =============================================================================
 * @file    work.h
 * @brief   Avalon A1126pro - Управление заданиями (заголовочный файл)
 * @version 1.0
 * @date    2024
 * =============================================================================
 * 
 * ОПИСАНИЕ:
 * Функции для работы с заданиями на майнинг.
 * 
 * =============================================================================
 */

#ifndef __WORK_H__
#define __WORK_H__

#include "cgminer.h"

/* ===========================================================================
 * ОСНОВНЫЕ ФУНКЦИИ
 * =========================================================================== */

/**
 * @brief Создание новой работы
 * @return Указатель на созданную работу или NULL при ошибке
 */
work_t *create_work(void);

/**
 * @brief Освобождение работы
 * @param work Указатель на работу
 */
void free_work(work_t *work);

/**
 * @brief Формирование 80-байтного заголовка блока
 * @param work Указатель на работу
 * @return 0 при успехе, -1 при ошибке
 */
int work_to_header(work_t *work);

/**
 * @brief Вычисление целевого хэша (target) из nbits
 * @param work Указатель на работу
 */
void work_calculate_target(work_t *work);

/**
 * @brief Проверка nonce на соответствие сложности
 * @param work  Указатель на работу
 * @param nonce Nonce для проверки
 * @return 1 если валиден, 0 иначе
 */
int work_check_nonce(work_t *work, uint32_t nonce);

/**
 * @brief Получение данных для submit
 * @param work          Указатель на работу
 * @param nonce         Найденный nonce
 * @param nonce2_hex    Буфер для extranonce2 (hex строка)
 * @param ntime_hex     Буфер для ntime (hex строка)
 * @param nonce_hex     Буфер для nonce (hex строка)
 */
void work_get_submit_data(work_t *work, uint32_t nonce,
                          char *nonce2_hex, char *ntime_hex, char *nonce_hex);

/**
 * @brief Увеличение ExtraNonce2
 * @param work Указатель на работу
 */
void work_increment_nonce2(work_t *work);

/**
 * @brief Клонирование работы
 * @param work Указатель на исходную работу
 * @return Указатель на копию или NULL при ошибке
 */
work_t *clone_work(const work_t *work);

/**
 * @brief Проверка, устарела ли работа
 * @param work Указатель на работу
 * @return 1 если устарела, 0 иначе
 */
int work_is_stale(const work_t *work);

#endif /* __WORK_H__ */
