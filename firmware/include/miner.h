/**
 * =============================================================================
 * @file    miner.h
 * @brief   Основные структуры и определения для майнера
 * =============================================================================
 * 
 * Содержит базовые структуры для работы CGMiner:
 * - Управление пулами
 * - Обработка заданий (work)
 * - Статистика майнинга
 * - Потоковая обработка
 * 
 * @author  Based on CGMiner 4.11.1
 * @version 1.0
 * =============================================================================
 */

#ifndef _MINER_H_
#define _MINER_H_

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include <string.h>

#include "config.h"

#ifdef __cplusplus
extern "C" {
#endif

/* =============================================================================
 * ТИПЫ ЛОГИРОВАНИЯ
 * ============================================================================= */

typedef enum {
    LOG_ERR,        /**< Ошибка */
    LOG_WARNING,    /**< Предупреждение */
    LOG_NOTICE,     /**< Уведомление */
    LOG_INFO,       /**< Информация */
    LOG_DEBUG,      /**< Отладка */
} log_level_t;

/* Макросы для логирования */
#define applog(level, fmt, ...) do { \
    if ((level) <= DEBUG_LOG_LEVEL) { \
        miner_log(level, __FILE__, __LINE__, fmt, ##__VA_ARGS__); \
    } \
} while(0)

/* =============================================================================
 * КОНСТАНТЫ ХЕШИРОВАНИЯ
 * ============================================================================= */

#define SHA256_DIGEST_LENGTH    32      /**< Длина SHA256 хеша */
#define BLOCK_HEADER_SIZE       80      /**< Размер заголовка блока */
#define MERKLE_ROOT_SIZE        32      /**< Размер Merkle root */
#define MAX_MERKLE_BRANCHES     30      /**< Макс. Merkle веток */

/* =============================================================================
 * СТРУКТУРА ПУЛА
 * ============================================================================= */

typedef enum {
    POOL_DISABLED,      /**< Пул отключён */
    POOL_ENABLED,       /**< Пул включён */
    POOL_CONNECTED,     /**< Подключён к пулу */
    POOL_ACTIVE,        /**< Активно майнит */
    POOL_REJECTING,     /**< Пул отклоняет шары */
} pool_state_t;

typedef struct pool {
    uint8_t         id;                         /**< ID пула */
    char            url[256];                   /**< URL пула (stratum+tcp://...) */
    char            host[128];                  /**< Хост */
    uint16_t        port;                       /**< Порт */
    char            user[128];                  /**< Имя воркера */
    char            pass[64];                   /**< Пароль */
    
    pool_state_t    state;                      /**< Текущее состояние */
    bool            stratum_active;             /**< Stratum соединение активно */
    bool            has_stratum;                /**< Поддерживает Stratum */
    
    /* Подписка Stratum */
    char            session_id[64];             /**< ID сессии */
    char            extranonce1[32];            /**< Extranonce1 */
    uint8_t         extranonce1_len;            /**< Длина extranonce1 */
    uint8_t         extranonce2_len;            /**< Длина extranonce2 */
    
    /* Текущая сложность */
    double          sdiff;                      /**< Share difficulty */
    double          diff1;                      /**< Сложность для diff=1 */
    
    /* Статистика */
    uint32_t        accepted;                   /**< Принятых шар */
    uint32_t        rejected;                   /**< Отклонённых шар */
    uint32_t        stale;                      /**< Устаревших шар */
    uint64_t        diff_accepted;              /**< Сумма сложности принятых */
    uint64_t        diff_rejected;              /**< Сумма сложности отклонённых */
    
    /* Время */
    uint32_t        last_share_time;            /**< Время последней шары */
    uint32_t        connect_time;               /**< Время подключения */
    uint32_t        disconnect_time;            /**< Время отключения */
    
    /* Сокет */
    int             sock;                       /**< Дескриптор сокета */
    
} pool_t;

/* =============================================================================
 * СТРУКТУРА ЗАДАНИЯ (WORK)
 * ============================================================================= */

typedef struct work {
    /* Идентификация */
    uint32_t        id;                         /**< Уникальный ID */
    uint8_t         job_id[8];                  /**< ID задания от пула */
    pool_t          *pool;                      /**< Пул-источник */
    
    /* Данные для хеширования */
    uint8_t         data[BLOCK_HEADER_SIZE];    /**< Заголовок блока */
    uint8_t         midstate[32];               /**< Предвычисленный midstate */
    uint8_t         target[32];                 /**< Целевой хеш */
    uint8_t         hash[32];                   /**< Результирующий хеш */
    
    /* Merkle данные */
    uint8_t         merkle_branches[MAX_MERKLE_BRANCHES][32];
    uint8_t         merkle_count;               /**< Количество веток */
    
    /* Coinbase */
    uint8_t         *coinbase;                  /**< Coinbase транзакция */
    uint16_t        coinbase_len;               /**< Длина coinbase */
    
    /* Параметры */
    uint32_t        version;                    /**< Версия блока */
    uint32_t        ntime;                      /**< Временная метка */
    uint32_t        nbits;                      /**< Compact difficulty */
    uint32_t        nonce;                      /**< Нонс (результат) */
    uint32_t        nonce2;                     /**< Extranonce2 */
    
    /* Статус */
    bool            clone;                      /**< Это клон задания */
    bool            rolltime;                   /**< Можно изменять ntime */
    bool            stratum;                    /**< Получено через Stratum */
    
    /* Время */
    uint32_t        tv_staged;                  /**< Когда получено */
    uint32_t        tv_work_start;              /**< Когда начали */
    uint32_t        tv_work_found;              /**< Когда найдено */
    
    /* Связный список */
    struct work     *next;                      /**< Следующее задание */
    struct work     *prev;                      /**< Предыдущее задание */
    
} work_t;

/* =============================================================================
 * СТРУКТУРА ПОТОКА МАЙНЕРА
 * ============================================================================= */

typedef struct thr_info {
    uint32_t        id;                         /**< ID потока */
    void            *cgpu;                      /**< Указатель на устройство */
    
    /* Текущее задание */
    work_t          *work;                      /**< Текущее задание */
    bool            work_restart;               /**< Нужно перезапустить */
    bool            pause;                      /**< Приостановлено */
    
    /* Статистика */
    uint64_t        hashes_done;                /**< Хешей вычислено */
    uint32_t        accepted;                   /**< Принято шар */
    uint32_t        rejected;                   /**< Отклонено шар */
    uint32_t        hw_errors;                  /**< HW ошибок */
    
    /* Хешрейт */
    double          hashrate;                   /**< Текущий хешрейт */
    double          hashrate_5s;                /**< Хешрейт за 5 сек */
    double          hashrate_1m;                /**< Хешрейт за 1 мин */
    double          hashrate_5m;                /**< Хешрейт за 5 мин */
    double          hashrate_15m;               /**< Хешрейт за 15 мин */
    
} thr_info_t;

/* =============================================================================
 * СТРУКТУРА УСТРОЙСТВА (CGPU)
 * ============================================================================= */

typedef struct cgpu_info {
    uint8_t         device_id;                  /**< ID устройства */
    char            device_name[32];            /**< Имя устройства */
    
    /* Драйвер */
    void            *driver;                    /**< Указатель на драйвер */
    void            *device_data;               /**< Приватные данные */
    
    /* Статус */
    bool            enabled;                    /**< Включено */
    bool            initialized;                /**< Инициализировано */
    
    /* Потоки */
    thr_info_t      *threads;                   /**< Массив потоков */
    uint8_t         thread_count;               /**< Количество потоков */
    
    /* Статистика */
    uint64_t        total_hashes;               /**< Всего хешей */
    double          total_hashrate;             /**< Общий хешрейт */
    uint32_t        accepted;                   /**< Принято */
    uint32_t        rejected;                   /**< Отклонено */
    uint32_t        hw_errors;                  /**< HW ошибки */
    double          utility;                    /**< Utility (shares/min) */
    
    /* Температура */
    float           temp;                       /**< Температура */
    float           temp_max;                   /**< Макс. температура */
    
    /* Время */
    uint32_t        device_start_time;          /**< Время запуска */
    uint32_t        last_work_time;             /**< Время последней работы */
    
} cgpu_info_t;

/* =============================================================================
 * СТРУКТУРА ДРАЙВЕРА
 * ============================================================================= */

typedef struct device_drv {
    /* Идентификация */
    uint8_t         drv_id;                     /**< ID драйвера */
    char            *dname;                     /**< Длинное имя */
    char            *name;                      /**< Короткое имя */
    
    /* Функции драйвера */
    
    /** Обнаружение устройств */
    int (*drv_detect)(void);
    
    /** Инициализация потока */
    bool (*thread_prepare)(thr_info_t *thr);
    
    /** Главный цикл хеширования */
    int64_t (*scanwork)(thr_info_t *thr);
    
    /** Обновление работы */
    void (*hash_work)(thr_info_t *thr);
    
    /** Формирование строки статуса */
    void (*get_statline_before)(char *buf, size_t size, cgpu_info_t *cgpu);
    
    /** Получение API статистики */
    void *(*get_api_stats)(cgpu_info_t *cgpu);
    
    /** Завершение потока */
    void (*thread_shutdown)(thr_info_t *thr);
    
    /** Завершение работы */
    void (*shutdown)(thr_info_t *thr);
    
} device_drv_t;

/* =============================================================================
 * ГЛОБАЛЬНЫЕ ПЕРЕМЕННЫЕ
 * ============================================================================= */

extern pool_t       pools[MAX_POOLS];           /**< Массив пулов */
extern int          pool_count;                 /**< Количество пулов */
extern pool_t       *current_pool;              /**< Текущий активный пул */

extern cgpu_info_t  *devices[];                 /**< Массив устройств */
extern int          device_count;               /**< Количество устройств */

extern bool         mining_active;              /**< Майнинг активен */
extern uint32_t     total_accepted;             /**< Всего принято шар */
extern uint32_t     total_rejected;             /**< Всего отклонено */
extern uint64_t     total_hashes;               /**< Всего хешей */

/* =============================================================================
 * ПРОТОТИПЫ ФУНКЦИЙ
 * ============================================================================= */

/* ----- Логирование ----- */

/**
 * @brief Вывод лог-сообщения
 * @param level Уровень логирования
 * @param file Файл (для отладки)
 * @param line Строка (для отладки)
 * @param fmt Формат строки
 */
void miner_log(log_level_t level, const char *file, int line, const char *fmt, ...);

/* ----- Управление пулами ----- */

/**
 * @brief Добавление пула
 * @param url URL пула
 * @param user Пользователь
 * @param pass Пароль
 * @return ID пула или -1 при ошибке
 */
int add_pool(const char *url, const char *user, const char *pass);

/**
 * @brief Удаление пула
 * @param pool_id ID пула
 * @return 0 при успехе
 */
int remove_pool(int pool_id);

/**
 * @brief Переключение на пул
 * @param pool_id ID целевого пула
 * @return 0 при успехе
 */
int switch_pool(int pool_id);

/**
 * @brief Подключение к пулу
 * @param pool Структура пула
 * @return 0 при успехе
 */
int pool_connect(pool_t *pool);

/**
 * @brief Отключение от пула
 * @param pool Структура пула
 */
void pool_disconnect(pool_t *pool);

/* ----- Управление заданиями ----- */

/**
 * @brief Получение нового задания
 * @return Указатель на задание или NULL
 */
work_t *get_work(void);

/**
 * @brief Освобождение задания
 * @param work Задание для освобождения
 */
void free_work(work_t *work);

/**
 * @brief Клонирование задания
 * @param work Исходное задание
 * @return Копия задания
 */
work_t *clone_work(work_t *work);

/**
 * @brief Отправка найденной шары
 * @param work Задание с найденным нонсом
 * @return true если отправлено успешно
 */
bool submit_work(work_t *work);

/* ----- Статистика ----- */

/**
 * @brief Получение текущего хешрейта
 * @return Хешрейт в H/s
 */
uint64_t get_total_hashrate(void);

/**
 * @brief Обновление статистики
 */
void update_statistics(void);

/* ----- Утилиты ----- */

/**
 * @brief Преобразование hex строки в байты
 * @param hex Hex строка
 * @param bin Буфер для байтов
 * @param len Длина буфера
 * @return true при успехе
 */
bool hex2bin(const char *hex, uint8_t *bin, size_t len);

/**
 * @brief Преобразование байтов в hex строку
 * @param bin Байты
 * @param len Длина байтов
 * @param hex Буфер для hex строки
 */
void bin2hex(const uint8_t *bin, size_t len, char *hex);

/**
 * @brief Разворот байтов (для little/big endian)
 * @param data Данные
 * @param len Длина
 */
void flip_bytes(uint8_t *data, size_t len);

/**
 * @brief Получение текущего времени в миллисекундах
 * @return Время в мс
 */
uint32_t get_ms_time(void);

#ifdef __cplusplus
}
#endif

#endif /* _MINER_H_ */
