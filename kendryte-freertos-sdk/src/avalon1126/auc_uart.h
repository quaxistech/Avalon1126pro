/**
 * =============================================================================
 * @file    auc_uart.h
 * @brief   Avalon A1126pro - AUC (Avalon UART Controller) драйвер
 * @version 1.0
 * @date    2024
 * =============================================================================
 * 
 * ОПИСАНИЕ:
 * Драйвер для коммуникации с ASIC модулями через UART интерфейс.
 * Реализует протокол Avalon AUC для передачи пакетов.
 * 
 * ПРОТОКОЛ AUC:
 * - Baudrate: 115200 (можно изменить до 3M)
 * - Формат: 8N1
 * - Пакеты: 40 байт (head[2] + type + opt + idx + cnt + data[32] + crc[2])
 * 
 * АППАРАТНОЕ ПОДКЛЮЧЕНИЕ (K210):
 * - UART1: основной канал к ASIC (GPIO 4/5)
 * - UART2: резервный/debug (GPIO 6/7)
 * - GPIO для выбора модуля (module select)
 * 
 * =============================================================================
 */

#ifndef __AUC_UART_H__
#define __AUC_UART_H__

#include <stdint.h>
#include <stdbool.h>
#include "avalon10.h"

/* ===========================================================================
 * КОНСТАНТЫ КОНФИГУРАЦИИ
 * =========================================================================== */

/**
 * @brief Номер UART для связи с ASIC
 */
#define AUC_UART_NUM            1

/**
 * @brief Скорость UART по умолчанию (115200 baud)
 */
#define AUC_UART_BAUDRATE       115200

/**
 * @brief Высокоскоростной режим (3M baud)
 */
#define AUC_UART_BAUDRATE_HIGH  3000000

/**
 * @brief Размер буфера приёма
 */
#define AUC_RX_BUFFER_SIZE      256

/**
 * @brief Размер буфера передачи
 */
#define AUC_TX_BUFFER_SIZE      256

/**
 * @brief Таймаут приёма по умолчанию (мс)
 */
#define AUC_DEFAULT_TIMEOUT     100

/**
 * @brief Максимальное количество попыток передачи
 */
#define AUC_MAX_RETRIES         3

/* ---------------------------------------------------------------------------
 * GPIO пины для управления модулями
 * --------------------------------------------------------------------------- */

/**
 * @brief Базовый GPIO для выбора модуля
 * Модули выбираются GPIO 8-11
 */
#define AUC_MODULE_SEL_BASE     8

/**
 * @brief GPIO для сброса ASIC
 */
#define AUC_RESET_GPIO          12

/**
 * @brief GPIO для управления питанием
 */
#define AUC_POWER_GPIO          13

/* ---------------------------------------------------------------------------
 * Коды ошибок
 * --------------------------------------------------------------------------- */

#define AUC_OK                  0
#define AUC_ERR_TIMEOUT         -1
#define AUC_ERR_CRC             -2
#define AUC_ERR_INVALID_PKG     -3
#define AUC_ERR_NO_RESPONSE     -4
#define AUC_ERR_INIT            -5

/* ===========================================================================
 * СТРУКТУРЫ ДАННЫХ
 * =========================================================================== */

/**
 * @brief Состояние AUC драйвера
 */
typedef struct {
    bool initialized;           /**< Флаг инициализации */
    uint32_t baudrate;          /**< Текущая скорость */
    uint8_t current_module;     /**< Текущий выбранный модуль */
    
    /* Статистика */
    uint32_t tx_packets;        /**< Отправлено пакетов */
    uint32_t rx_packets;        /**< Принято пакетов */
    uint32_t crc_errors;        /**< Ошибки CRC */
    uint32_t timeouts;          /**< Таймауты */
    uint32_t retries;           /**< Повторные передачи */
    
    /* Буферы */
    uint8_t rx_buffer[AUC_RX_BUFFER_SIZE];
    uint8_t tx_buffer[AUC_TX_BUFFER_SIZE];
    volatile int rx_head;
    volatile int rx_tail;
} auc_state_t;

/* ===========================================================================
 * ПРОТОТИПЫ ФУНКЦИЙ
 * =========================================================================== */

/**
 * @brief Инициализация AUC драйвера
 * 
 * Настраивает UART и GPIO для работы с ASIC модулями.
 * 
 * @return AUC_OK при успехе, код ошибки при неудаче
 */
int auc_init(void);

/**
 * @brief Деинициализация AUC драйвера
 */
void auc_deinit(void);

/**
 * @brief Установка скорости UART
 * 
 * @param baudrate  Новая скорость (115200 или 3000000)
 * @return AUC_OK при успехе
 */
int auc_set_baudrate(uint32_t baudrate);

/**
 * @brief Выбор модуля для коммуникации
 * 
 * @param module_id ID модуля (0-3)
 * @return AUC_OK при успехе
 */
int auc_select_module(int module_id);

/**
 * @brief Отправка пакета через AUC
 * 
 * @param module_id ID модуля (0-3)
 * @param pkg       Указатель на пакет для отправки
 * @return AUC_OK при успехе, код ошибки при неудаче
 */
int auc_send_pkg(int module_id, const avalon10_pkg_t *pkg);

/**
 * @brief Приём пакета через AUC
 * 
 * @param module_id ID модуля (0-3)
 * @param pkg       Указатель для записи принятого пакета
 * @param timeout   Таймаут в мс
 * @return AUC_OK при успехе, код ошибки при неудаче
 */
int auc_recv_pkg(int module_id, avalon10_pkg_t *pkg, int timeout);

/**
 * @brief Сброс выбранного модуля
 * 
 * @param module_id ID модуля (0-3), -1 для сброса всех
 * @return AUC_OK при успехе
 */
int auc_reset_module(int module_id);

/**
 * @brief Сброс всех модулей
 * 
 * @return AUC_OK при успехе
 */
int auc_reset_all(void);

/**
 * @brief Получение статистики AUC
 * 
 * @return Указатель на структуру состояния (только чтение)
 */
const auc_state_t *auc_get_stats(void);

/**
 * @brief Сброс статистики AUC
 */
void auc_reset_stats(void);

/**
 * @brief Проверка готовности модуля
 * 
 * @param module_id ID модуля (0-3)
 * @return true если модуль готов
 */
bool auc_module_ready(int module_id);

/**
 * @brief Отправка и приём пакета (транзакция)
 * 
 * Отправляет пакет и ждёт ответа с автоматическими повторами.
 * 
 * @param module_id ID модуля
 * @param tx_pkg    Пакет для отправки
 * @param rx_pkg    Буфер для ответа (может быть NULL)
 * @param timeout   Таймаут в мс
 * @return AUC_OK при успехе, код ошибки при неудаче
 */
int auc_transaction(int module_id, const avalon10_pkg_t *tx_pkg, 
                    avalon10_pkg_t *rx_pkg, int timeout);

#endif /* __AUC_UART_H__ */
