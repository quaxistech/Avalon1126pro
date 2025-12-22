/**
 * =============================================================================
 * @file    avalon10.h
 * @brief   Заголовочный файл драйвера Avalon10 для A1126pro
 * =============================================================================
 * 
 * Этот файл содержит все определения констант, структур и прототипов функций
 * для драйвера ASIC-чипов Avalon10, используемых в майнере A1126pro.
 * 
 * Основано на анализе оригинальной прошивки CGMiner 4.11.1
 * Процессор: Kendryte K210 (RISC-V 64-bit)
 * 
 * @author  Reconstructed from firmware
 * @version 1.0
 * =============================================================================
 */

#ifndef _AVALON10_H_
#define _AVALON10_H_

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* =============================================================================
 * КОНФИГУРАЦИЯ ЧАСТОТЫ
 * Частота работы ASIC-чипов в МГц
 * ============================================================================= */

#define AVA10_FREQUENCY_MIN         25      /**< Минимальная частота (МГц) */
#define AVA10_FREQUENCY_MAX         800     /**< Максимальная частота (МГц) */
#define AVA10_FREQUENCY_STEP        25      /**< Шаг изменения частоты */
#define AVA10_DEFAULT_FREQUENCY     550     /**< Частота по умолчанию */
#define AVA10_FREQUENCY_LEVELS      4       /**< Количество уровней PLL */

/* =============================================================================
 * КОНФИГУРАЦИЯ НАПРЯЖЕНИЯ
 * Напряжение питания ядер ASIC
 * ============================================================================= */

#define AVA10_VOLTAGE_LEVEL_MIN     0       /**< Минимальный уровень */
#define AVA10_VOLTAGE_LEVEL_MAX     75      /**< Максимальный уровень */
#define AVA10_DEFAULT_VOLTAGE       40      /**< Напряжение по умолчанию */
#define AVA10_VOLTAGE_OFFSET_MIN    (-2)    /**< Мин. смещение напряжения */
#define AVA10_VOLTAGE_OFFSET_MAX    1       /**< Макс. смещение напряжения */

/* =============================================================================
 * КОНФИГУРАЦИЯ ТЕМПЕРАТУРЫ
 * Управление тепловым режимом
 * ============================================================================= */

#define AVA10_TEMP_TARGET           90      /**< Целевая температура (°C) */
#define AVA10_TEMP_OVERHEAT         105     /**< Перегрев - аварийное отключение */
#define AVA10_TEMP_WARNING          95      /**< Предупреждение о перегреве */
#define AVA10_TEMP_HYSTERESIS       5       /**< Гистерезис температуры */

/* =============================================================================
 * КОНФИГУРАЦИЯ ВЕНТИЛЯТОРОВ
 * PWM управление вентиляторами охлаждения
 * ============================================================================= */

#define AVA10_FAN_MIN               5       /**< Минимальная скорость (%) */
#define AVA10_FAN_MAX               100     /**< Максимальная скорость (%) */
#define AVA10_FAN_DEFAULT           50      /**< Скорость по умолчанию */
#define AVA10_FAN_PWM_FREQ          25000   /**< Частота ШИМ (Гц) */

/* =============================================================================
 * КОНФИГУРАЦИЯ ХЕШИРОВАНИЯ
 * Параметры хеш-чейна
 * ============================================================================= */

#define AVA10_MINER_COUNT           4       /**< Количество хеш-плат */
#define AVA10_ASIC_PER_MINER        26      /**< ASIC чипов на плату */
#define AVA10_CORES_PER_ASIC        114     /**< Ядер в одном ASIC */
#define AVA10_TOTAL_ASICS           (AVA10_MINER_COUNT * AVA10_ASIC_PER_MINER)
#define AVA10_TOTAL_CORES           (AVA10_TOTAL_ASICS * AVA10_CORES_PER_ASIC)

/* =============================================================================
 * КОНФИГУРАЦИЯ PLL (Phase-Locked Loop)
 * Настройки генератора тактовой частоты
 * ============================================================================= */

#define AVA10_PLL_COUNT             4       /**< Количество PLL */
#define AVA10_PLL_SEL_DEFAULT       3       /**< PLL по умолчанию */

/* =============================================================================
 * SMART SPEED - Умное управление скоростью
 * Автоматическая подстройка частоты
 * ============================================================================= */

#define AVA10_SS_OFF                0       /**< Выключено */
#define AVA10_SS_MODE1              1       /**< Режим 1 - базовый */
#define AVA10_SS_MODE2              2       /**< Режим 2 - агрессивный */
#define AVA10_SS_DEFAULT            AVA10_SS_MODE1

/* =============================================================================
 * ПАРАМЕТРЫ ПОРОГОВЫХ ЗНАЧЕНИЙ
 * Для валидации нонсов и управления качеством
 * ============================================================================= */

#define AVA10_TH_PASS_DEFAULT       160     /**< Порог прохождения */
#define AVA10_TH_FAIL_DEFAULT       8000    /**< Порог ошибки */
#define AVA10_TH_INIT_DEFAULT       32767   /**< Начальное значение */
#define AVA10_TH_TIMEOUT_DEFAULT    20000   /**< Таймаут (мс) */
#define AVA10_TH_MS_DEFAULT         5       /**< Интервал проверки (мс) */
#define AVA10_TH_ADD_DEFAULT        1       /**< Инкремент */

/* =============================================================================
 * МАСКА НОНСА
 * Определяет сложность для отправки на пул
 * ============================================================================= */

#define AVA10_NONCE_MASK_DEFAULT    24      /**< Биты маски нонса */
#define AVA10_NONCE_CHECK_DEFAULT   1       /**< Проверка нонса включена */

/* =============================================================================
 * ПАРАМЕТРЫ МУЛЬТИПЛЕКСОРА
 * Управление переключением чипов
 * ============================================================================= */

#define AVA10_MUX_L2H_DEFAULT       0       /**< Low to High */
#define AVA10_MUX_H2L_DEFAULT       1       /**< High to Low */
#define AVA10_H2LTIME0_SPD_DEFAULT  3       /**< Скорость переключения */
#define AVA10_SPDLOW_DEFAULT        0       /**< Нижний предел скорости */
#define AVA10_SPDHIGH_DEFAULT       3       /**< Верхний предел скорости */

/* =============================================================================
 * ТАЙМАУТЫ И ЗАДЕРЖКИ
 * ============================================================================= */

#define AVA10_POLLING_DELAY         20      /**< Задержка опроса (мс) */
#define AVA10_DETECT_TIMEOUT        1000    /**< Таймаут обнаружения (мс) */
#define AVA10_RESET_DELAY           100     /**< Задержка после сброса (мс) */

/* =============================================================================
 * ПРОТОКОЛ СВЯЗИ С ASIC
 * Пакеты для общения с хеш-платами
 * ============================================================================= */

/* Заголовок пакета (Canaan Protocol) */
#define AVA10_H1                    'C'     /**< Первый байт заголовка */
#define AVA10_H2                    'N'     /**< Второй байт заголовка */

/* Размеры пакетов */
#define AVA10_P_COUNT               40      /**< Макс. количество пакетов */
#define AVA10_P_DATA_LEN            32      /**< Длина данных пакета */
#define AVA10_MM_VER_LEN            15      /**< Длина строки версии */
#define AVA10_MM_DNA_LEN            8       /**< Длина DNA чипа */
#define AVA10_OTP_LEN               32      /**< Длина OTP данных */

/* Типы пакетов: Отправка на ASIC */
#define AVA10_P_DETECT              0x10    /**< Обнаружение устройства */
#define AVA10_P_STATIC              0x11    /**< Статическая конфигурация */
#define AVA10_P_JOB_ID              0x12    /**< ID задания */
#define AVA10_P_COINBASE            0x13    /**< Coinbase транзакция */
#define AVA10_P_MERKLES             0x14    /**< Merkle ветви */
#define AVA10_P_HEADER              0x15    /**< Заголовок блока */
#define AVA10_P_TARGET              0x16    /**< Целевая сложность */
#define AVA10_P_JOB_FIN             0x17    /**< Завершение задания */

/* Типы пакетов: Настройка */
#define AVA10_P_SET                 0x20    /**< Установка параметров */
#define AVA10_P_SET_FIN             0x21    /**< Завершение настройки */
#define AVA10_P_SET_VOLT            0x22    /**< Установка напряжения */
#define AVA10_P_SET_PMU             0x24    /**< Настройка PMU */
#define AVA10_P_SET_PLL             0x25    /**< Настройка PLL */
#define AVA10_P_SET_SS              0x26    /**< Настройка Smart Speed */
#define AVA10_P_SET_FAC             0x28    /**< Заводские настройки */
#define AVA10_P_SET_OC              0x29    /**< Разгон */

/* Типы пакетов: Запросы */
#define AVA10_P_POLLING             0x30    /**< Опрос статуса */
#define AVA10_P_SYNC                0x31    /**< Синхронизация */
#define AVA10_P_TEST                0x32    /**< Тестовый режим */
#define AVA10_P_RSTMMTX             0x33    /**< Сброс MM TX */
#define AVA10_P_GET_VOLT            0x34    /**< Запрос напряжения */

/* Типы пакетов: Ответы от ASIC */
#define AVA10_P_ACKDETECT           0x40    /**< Подтверждение обнаружения */
#define AVA10_P_STATUS              0x41    /**< Статус устройства */
#define AVA10_P_NONCE               0x42    /**< Найденный нонс */
#define AVA10_P_TEST_RET            0x43    /**< Результат теста */
#define AVA10_P_STATUS_VOLT         0x46    /**< Статус напряжения */
#define AVA10_P_STATUS_PMU          0x48    /**< Статус PMU */
#define AVA10_P_STATUS_PLL          0x49    /**< Статус PLL */
#define AVA10_P_STATUS_LOG          0x4A    /**< Лог статуса */
#define AVA10_P_STATUS_ASIC         0x4B    /**< Статус ASIC */
#define AVA10_P_STATUS_PVT          0x4C    /**< PVT данные */
#define AVA10_P_STATUS_FAC          0x4D    /**< Заводской статус */
#define AVA10_P_STATUS_OC           0x4E    /**< Статус разгона */
#define AVA10_P_STATUS_OTP          0x4F    /**< OTP статус */
#define AVA10_P_SET_ASIC_OTP        0x50    /**< Установка ASIC OTP */

/* Broadcast адрес */
#define AVA10_MODULE_BROADCAST      0       /**< Широковещательный адрес */

/* =============================================================================
 * PID РЕГУЛЯТОР
 * Для автоматического управления температурой/частотой
 * ============================================================================= */

#define AVA10_PID_P_DEFAULT         2       /**< Пропорциональный коэфф. */
#define AVA10_PID_I_DEFAULT         5       /**< Интегральный коэфф. */
#define AVA10_PID_D_DEFAULT         0       /**< Дифференциальный коэфф. */
#define AVA10_PID_TEMP_MIN          50      /**< Мин. температура для PID */
#define AVA10_PID_TEMP_MAX          100     /**< Макс. температура для PID */

/* =============================================================================
 * РАЗМЕРЫ БУФЕРОВ
 * ============================================================================= */

#define AVA10_P_COINBASE_SIZE       (6 * 1024 + 64)  /**< Размер coinbase */
#define AVA10_P_MERKLES_COUNT       30               /**< Макс. merkle веток */

/* =============================================================================
 * СТРУКТУРЫ ДАННЫХ
 * ============================================================================= */

/**
 * @brief Структура задания для хеширования
 * 
 * Содержит всю информацию для майнинга одного блока:
 * - Заголовок блока (80 байт)
 * - Merkle root
 * - Target (сложность)
 * - Coinbase для формирования транзакции
 */
typedef struct {
    uint8_t     job_id[4];              /**< Уникальный ID задания */
    uint8_t     prev_hash[32];          /**< Хеш предыдущего блока */
    uint8_t     coinbase1[256];         /**< Первая часть coinbase */
    uint16_t    coinbase1_len;          /**< Длина coinbase1 */
    uint8_t     coinbase2[256];         /**< Вторая часть coinbase */
    uint16_t    coinbase2_len;          /**< Длина coinbase2 */
    uint8_t     merkle_branches[AVA10_P_MERKLES_COUNT][32]; /**< Merkle ветви */
    uint8_t     merkle_count;           /**< Количество Merkle веток */
    uint32_t    version;                /**< Версия блока */
    uint32_t    nbits;                  /**< Сложность (compact) */
    uint32_t    ntime;                  /**< Временная метка */
    uint8_t     target[32];             /**< Целевой хеш */
    bool        clean_jobs;             /**< Очистить старые задания */
} avalon10_job_t;

/**
 * @brief Структура найденного нонса
 * 
 * Возвращается ASIC-чипом при нахождении подходящего хеша
 */
typedef struct {
    uint8_t     job_id[4];              /**< ID задания */
    uint32_t    nonce;                  /**< Найденный нонс */
    uint32_t    nonce2;                 /**< Extranonce2 */
    uint32_t    ntime;                  /**< Временная метка */
    uint8_t     chip_id;                /**< ID чипа, нашедшего нонс */
    uint8_t     core_id;                /**< ID ядра */
    uint8_t     miner_id;               /**< ID хеш-платы */
} avalon10_nonce_t;

/**
 * @brief Статус одного ASIC чипа
 */
typedef struct {
    uint8_t     chip_id;                /**< ID чипа */
    bool        enabled;                /**< Чип включён */
    uint32_t    frequency;              /**< Текущая частота (МГц) */
    int8_t      temperature;            /**< Температура (°C) */
    uint32_t    hw_errors;              /**< Аппаратные ошибки */
    uint32_t    nonces_found;           /**< Найдено нонсов */
    uint32_t    last_nonce_time;        /**< Время последнего нонса */
} avalon10_asic_status_t;

/**
 * @brief Статус хеш-платы (miner)
 */
typedef struct {
    uint8_t     miner_id;               /**< ID платы */
    bool        present;                /**< Плата обнаружена */
    bool        enabled;                /**< Плата включена */
    char        version[AVA10_MM_VER_LEN + 1];  /**< Версия прошивки */
    uint8_t     dna[AVA10_MM_DNA_LEN];  /**< Уникальный DNA */
    
    /* Температуры */
    int8_t      temp_chip;              /**< Температура чипов */
    int8_t      temp_board;             /**< Температура платы */
    int8_t      temp_inlet;             /**< Входная температура */
    int8_t      temp_outlet;            /**< Выходная температура */
    
    /* Напряжения */
    uint16_t    voltage[8];             /**< Напряжения по каналам */
    
    /* Частоты */
    uint32_t    frequency[AVA10_PLL_COUNT]; /**< Частоты PLL */
    
    /* Статистика */
    uint64_t    hashrate;               /**< Хешрейт (H/s) */
    uint32_t    accepted;               /**< Принятых шар */
    uint32_t    rejected;               /**< Отклонённых шар */
    uint32_t    hw_errors;              /**< Аппаратных ошибок */
    
    /* Статусы ASIC */
    avalon10_asic_status_t asics[AVA10_ASIC_PER_MINER];
} avalon10_miner_status_t;

/**
 * @brief Глобальный статус устройства Avalon10
 */
typedef struct {
    bool        initialized;            /**< Устройство инициализировано */
    bool        mining_active;          /**< Майнинг запущен */
    
    /* Информация об устройстве */
    char        device_name[32];        /**< Имя устройства */
    char        firmware_ver[16];       /**< Версия прошивки */
    uint8_t     device_id;              /**< ID устройства */
    
    /* Общие параметры */
    uint32_t    frequency;              /**< Текущая частота */
    uint8_t     voltage_level;          /**< Уровень напряжения */
    int8_t      voltage_offset;         /**< Смещение напряжения */
    uint8_t     fan_speed;              /**< Скорость вентиляторов (%) */
    uint8_t     fan_pwm;                /**< Текущий PWM (0-255) */
    
    /* Тепловой режим */
    int8_t      temp_target;            /**< Целевая температура */
    int8_t      temp_current;           /**< Текущая максимальная */
    bool        overheat_protection;    /**< Защита от перегрева активна */
    
    /* Smart Speed */
    uint8_t     smart_speed_mode;       /**< Режим Smart Speed */
    
    /* Статистика */
    uint64_t    total_hashrate;         /**< Общий хешрейт */
    uint64_t    total_hashes;           /**< Всего вычислено хешей */
    uint32_t    total_accepted;         /**< Всего принято */
    uint32_t    total_rejected;         /**< Всего отклонено */
    uint32_t    total_hw_errors;        /**< Всего HW ошибок */
    uint32_t    uptime;                 /**< Время работы (сек) */
    
    /* Хеш-платы */
    uint8_t     miner_count;            /**< Количество обнаруженных плат */
    avalon10_miner_status_t miners[AVA10_MINER_COUNT];
    
    /* Текущее задание */
    avalon10_job_t current_job;         /**< Текущее задание */
    bool        has_job;                /**< Есть активное задание */
    
} avalon10_device_t;

/**
 * @brief Конфигурация устройства (сохраняется во flash)
 */
typedef struct {
    uint32_t    magic;                  /**< Магическое число 0xAVA10CFG */
    uint16_t    version;                /**< Версия конфигурации */
    
    /* Частота и напряжение */
    uint32_t    frequency;              /**< Частота работы */
    uint8_t     voltage_level;          /**< Уровень напряжения */
    int8_t      voltage_offset;         /**< Смещение напряжения */
    uint8_t     pll_select;             /**< Выбор PLL */
    
    /* Охлаждение */
    uint8_t     fan_mode;               /**< Режим вентиляторов (авто/ручной) */
    uint8_t     fan_speed;              /**< Скорость вентиляторов */
    int8_t      temp_target;            /**< Целевая температура */
    
    /* Smart Speed */
    uint8_t     smart_speed;            /**< Режим Smart Speed */
    
    /* Пороговые значения */
    uint32_t    th_pass;                /**< Порог прохождения */
    uint32_t    th_fail;                /**< Порог ошибки */
    uint32_t    th_timeout;             /**< Таймаут */
    uint32_t    nonce_mask;             /**< Маска нонса */
    
    /* MUX настройки */
    uint8_t     mux_l2h;
    uint8_t     mux_h2l;
    uint8_t     h2ltime0_spd;
    uint8_t     spdlow;
    uint8_t     spdhigh;
    
    /* PID регулятор */
    uint8_t     pid_p;
    uint8_t     pid_i;
    uint8_t     pid_d;
    
    /* Зарезервировано */
    uint8_t     reserved[32];
    
    /* CRC для проверки целостности */
    uint32_t    crc32;
} avalon10_config_t;

/* =============================================================================
 * ГЛОБАЛЬНЫЕ ПЕРЕМЕННЫЕ (extern)
 * ============================================================================= */

extern avalon10_device_t    g_avalon10_device;  /**< Глобальное состояние */
extern avalon10_config_t    g_avalon10_config;  /**< Конфигурация */

/* =============================================================================
 * ПРОТОТИПЫ ФУНКЦИЙ
 * ============================================================================= */

/* ----- Инициализация и управление устройством ----- */

/**
 * @brief Инициализация драйвера Avalon10
 * @return 0 при успехе, отрицательный код ошибки при неудаче
 */
int avalon10_init(void);

/**
 * @brief Деинициализация драйвера
 */
void avalon10_deinit(void);

/**
 * @brief Обнаружение подключённых хеш-плат
 * @return Количество обнаруженных плат
 */
int avalon10_detect(void);

/**
 * @brief Сброс устройства
 * @param miner_id ID платы (0xFF для всех)
 * @return 0 при успехе
 */
int avalon10_reset(uint8_t miner_id);

/* ----- Управление майнингом ----- */

/**
 * @brief Отправка нового задания на хеш-платы
 * @param job Структура задания
 * @return 0 при успехе
 */
int avalon10_send_job(const avalon10_job_t *job);

/**
 * @brief Опрос хеш-плат на наличие нонсов
 * @param nonces Буфер для найденных нонсов
 * @param max_nonces Максимальное количество нонсов
 * @return Количество найденных нонсов
 */
int avalon10_poll_nonces(avalon10_nonce_t *nonces, int max_nonces);

/**
 * @brief Проверка валидности найденного нонса
 * @param nonce Структура нонса
 * @param job Задание, к которому относится нонс
 * @return true если нонс валиден
 */
bool avalon10_verify_nonce(const avalon10_nonce_t *nonce, const avalon10_job_t *job);

/**
 * @brief Главный цикл сканирования хешей
 * @return Количество вычисленных хешей
 */
int64_t avalon10_scanhash(void);

/* ----- Управление частотой и напряжением ----- */

/**
 * @brief Установка частоты работы ASIC
 * @param frequency Частота в МГц
 * @param miner_id ID платы (0xFF для всех)
 * @return 0 при успехе
 */
int avalon10_set_frequency(uint32_t frequency, uint8_t miner_id);

/**
 * @brief Установка уровня напряжения
 * @param level Уровень (0-75)
 * @param miner_id ID платы (0xFF для всех)
 * @return 0 при успехе
 */
int avalon10_set_voltage(uint8_t level, uint8_t miner_id);

/**
 * @brief Установка смещения напряжения
 * @param offset Смещение (-2..+1)
 * @return 0 при успехе
 */
int avalon10_set_voltage_offset(int8_t offset);

/* ----- Управление охлаждением ----- */

/**
 * @brief Установка скорости вентиляторов
 * @param speed Скорость в процентах (0-100)
 * @return 0 при успехе
 */
int avalon10_set_fan_speed(uint8_t speed);

/**
 * @brief Установка целевой температуры
 * @param temp Температура в градусах Цельсия
 * @return 0 при успехе
 */
int avalon10_set_temp_target(int8_t temp);

/**
 * @brief Обновление управления вентиляторами (вызывать периодически)
 */
void avalon10_update_fan_control(void);

/* ----- Smart Speed ----- */

/**
 * @brief Установка режима Smart Speed
 * @param mode Режим (0=выкл, 1=базовый, 2=агрессивный)
 * @return 0 при успехе
 */
int avalon10_set_smart_speed(uint8_t mode);

/**
 * @brief Обновление Smart Speed (вызывать периодически)
 */
void avalon10_update_smart_speed(void);

/* ----- Статистика и статус ----- */

/**
 * @brief Получение текущего хешрейта
 * @return Хешрейт в H/s
 */
uint64_t avalon10_get_hashrate(void);

/**
 * @brief Получение статуса хеш-платы
 * @param miner_id ID платы
 * @param status Буфер для статуса
 * @return 0 при успехе
 */
int avalon10_get_miner_status(uint8_t miner_id, avalon10_miner_status_t *status);

/**
 * @brief Получение статуса устройства
 * @param device Буфер для статуса
 * @return 0 при успехе
 */
int avalon10_get_device_status(avalon10_device_t *device);

/**
 * @brief Формирование строки статуса для отображения
 * @param buf Буфер для строки
 * @param size Размер буфера
 */
void avalon10_statline_before(char *buf, size_t size);

/* ----- Конфигурация ----- */

/**
 * @brief Загрузка конфигурации из flash
 * @return 0 при успехе
 */
int avalon10_load_config(void);

/**
 * @brief Сохранение конфигурации во flash
 * @return 0 при успехе
 */
int avalon10_save_config(void);

/**
 * @brief Сброс конфигурации к заводским настройкам
 */
void avalon10_reset_config(void);

/* ----- Низкоуровневые функции ----- */

/**
 * @brief Отправка пакета на ASIC
 * @param miner_id ID платы
 * @param type Тип пакета
 * @param data Данные
 * @param len Длина данных
 * @return 0 при успехе
 */
int avalon10_send_packet(uint8_t miner_id, uint8_t type, const uint8_t *data, size_t len);

/**
 * @brief Получение пакета от ASIC
 * @param miner_id ID платы
 * @param type Буфер для типа пакета
 * @param data Буфер для данных
 * @param max_len Максимальная длина
 * @param timeout_ms Таймаут в миллисекундах
 * @return Длина полученных данных, отрицательное при ошибке
 */
int avalon10_recv_packet(uint8_t miner_id, uint8_t *type, uint8_t *data, size_t max_len, uint32_t timeout_ms);

#ifdef __cplusplus
}
#endif

#endif /* _AVALON10_H_ */
