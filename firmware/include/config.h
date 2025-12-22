/**
 * =============================================================================
 * @file    config.h
 * @brief   Главный конфигурационный файл прошивки Avalon A1126pro
 * =============================================================================
 * 
 * Содержит все глобальные настройки системы, версии, и feature flags.
 * 
 * @author  Reconstructed from firmware
 * @version 1.0
 * =============================================================================
 */

#ifndef _CONFIG_H_
#define _CONFIG_H_

/* =============================================================================
 * ИНФОРМАЦИЯ О ПРОШИВКЕ
 * ============================================================================= */

#define FIRMWARE_NAME           "Avalon A1126pro"
#define FIRMWARE_VERSION        "1.0.0"
#define FIRMWARE_BUILD_DATE     __DATE__
#define FIRMWARE_BUILD_TIME     __TIME__

/* Версия CGMiner, на которой основан код */
#define CGMINER_VERSION         "4.11.1"

/* Информация о производителе */
#define MANUFACTURER_NAME       "Canaan Creative"
#define DEVICE_MODEL            "A1126pro"

/* =============================================================================
 * КОНФИГУРАЦИЯ ПРОЦЕССОРА K210
 * ============================================================================= */

/* Частоты системы */
#define K210_CPU_FREQ           400000000   /**< Частота CPU: 400 МГц */
#define K210_PLL0_FREQ          800000000   /**< PLL0: 800 МГц */
#define K210_PLL1_FREQ          400000000   /**< PLL1: 400 МГц (периферия) */
#define K210_PLL2_FREQ          45158400    /**< PLL2: для аудио */

/* Размеры памяти */
#define K210_SRAM_SIZE          (6 * 1024 * 1024)   /**< 6 МБ SRAM */
#define K210_AI_SRAM_SIZE       (2 * 1024 * 1024)   /**< 2 МБ AI SRAM */

/* Базовые адреса памяти */
#define K210_SRAM_BASE          0x80000000
#define K210_AI_SRAM_BASE       0x80600000
#define K210_FLASH_BASE         0x00000000

/* =============================================================================
 * ВКЛЮЧЕНИЕ МОДУЛЕЙ (FEATURE FLAGS)
 * ============================================================================= */

#define USE_AVALON10            1   /**< Включить драйвер Avalon10 */
#define USE_FREERTOS            1   /**< Использовать FreeRTOS */
#define USE_LWIP                1   /**< Сетевой стек lwIP */
#define USE_HTTP_SERVER         1   /**< Веб-интерфейс */
#define USE_STRATUM             1   /**< Протокол Stratum */
#define USE_JSON_API            1   /**< JSON API */
#define USE_WATCHDOG            1   /**< Watchdog таймер */
#define USE_TEMPERATURE_CTRL    1   /**< Контроль температуры */
#define USE_FAN_CONTROL         1   /**< Управление вентиляторами */
#define USE_SMART_SPEED         1   /**< Умное управление частотой */
#define USE_OTA_UPDATE          1   /**< OTA обновление прошивки */

/* Отладочные опции */
#define DEBUG_ENABLED           0   /**< Отладочный вывод */
#define DEBUG_VERBOSE           0   /**< Подробный отладочный вывод */
#define DEBUG_UART              1   /**< UART для отладки */
#define DEBUG_LOG_LEVEL         3   /**< 0=off, 1=error, 2=warn, 3=info, 4=debug */

/* =============================================================================
 * КОНФИГУРАЦИЯ СЕТИ
 * ============================================================================= */

/* DHCP настройки */
#define NET_USE_DHCP            1           /**< Использовать DHCP */
#define NET_HOSTNAME            "avalon1126pro"

/* Статический IP (если DHCP выключен) */
#define NET_STATIC_IP           "192.168.1.100"
#define NET_STATIC_MASK         "255.255.255.0"
#define NET_STATIC_GW           "192.168.1.1"
#define NET_STATIC_DNS          "8.8.8.8"

/* HTTP сервер */
#define HTTP_SERVER_PORT        80
#define HTTP_MAX_CONNECTIONS    4

/* API сервер */
#define API_SERVER_PORT         4028        /**< Стандартный порт CGMiner API */

/* =============================================================================
 * КОНФИГУРАЦИЯ ПУЛОВ
 * ============================================================================= */

#define MAX_POOLS               3           /**< Максимум пулов */
#define POOL_CONNECT_TIMEOUT    30000       /**< Таймаут подключения (мс) */
#define POOL_RETRY_INTERVAL     5000        /**< Интервал переподключения (мс) */
#define POOL_MAX_RETRIES        5           /**< Макс. попыток переподключения */

/* Пул по умолчанию */
#define DEFAULT_POOL_URL        "stratum+tcp://stratum.slushpool.com:3333"
#define DEFAULT_POOL_USER       "worker"
#define DEFAULT_POOL_PASS       "x"

/* =============================================================================
 * КОНФИГУРАЦИЯ GPIO
 * Назначение пинов процессора K210
 * ============================================================================= */

/* SPI для связи с ASIC */
#define PIN_SPI_MOSI            0
#define PIN_SPI_MISO            1
#define PIN_SPI_SCLK            2
#define PIN_SPI_CS0             3
#define PIN_SPI_CS1             4
#define PIN_SPI_CS2             5
#define PIN_SPI_CS3             6

/* Управление хеш-платами */
#define PIN_HASH_RESET          7           /**< Сброс ASIC */
#define PIN_HASH_CHAIN_EN       8           /**< Включение хеш-чейна */
#define PIN_HASH_POWER_EN       9           /**< Питание хеш-плат */

/* Вентиляторы (PWM) */
#define PIN_FAN_PWM0            10
#define PIN_FAN_PWM1            11
#define PIN_FAN_TACH0           12          /**< Тахометр 0 */
#define PIN_FAN_TACH1           13          /**< Тахометр 1 */

/* Светодиоды */
#define PIN_LED_STATUS          14          /**< Статус (зелёный) */
#define PIN_LED_ERROR           15          /**< Ошибка (красный) */
#define PIN_LED_NETWORK         16          /**< Сеть (жёлтый) */

/* I2C для датчиков */
#define PIN_I2C_SDA             17
#define PIN_I2C_SCL             18

/* UART отладки */
#define PIN_UART_TX             19
#define PIN_UART_RX             20

/* Ethernet PHY */
#define PIN_ETH_RST             21
#define PIN_ETH_INT             22

/* Кнопки */
#define PIN_BUTTON_RESET        23
#define PIN_BUTTON_IP           24          /**< Показать IP на дисплее */

/* =============================================================================
 * КОНФИГУРАЦИЯ SPI
 * ============================================================================= */

#define SPI_HASH_DEVICE         0           /**< SPI устройство для ASIC */
#define SPI_HASH_BAUDRATE       10000000    /**< 10 МГц */
#define SPI_HASH_MODE           0           /**< SPI Mode 0 */
#define SPI_HASH_BITS           8           /**< 8 бит данных */

/* =============================================================================
 * КОНФИГУРАЦИЯ UART
 * ============================================================================= */

#define UART_DEBUG_NUM          0           /**< UART для отладки */
#define UART_DEBUG_BAUDRATE     115200      /**< Скорость отладочного UART */

/* =============================================================================
 * КОНФИГУРАЦИЯ I2C
 * ============================================================================= */

#define I2C_SENSOR_NUM          0           /**< I2C для датчиков */
#define I2C_SENSOR_SPEED        400000      /**< 400 кГц (Fast Mode) */

/* Адреса I2C устройств */
#define I2C_ADDR_TEMP_SENSOR    0x48        /**< Датчик температуры */
#define I2C_ADDR_EEPROM         0x50        /**< EEPROM конфигурации */

/* =============================================================================
 * КОНФИГУРАЦИЯ FreeRTOS
 * ============================================================================= */

#define RTOS_TICK_RATE_HZ       1000        /**< 1 мс тик */
#define RTOS_HEAP_SIZE          (512 * 1024)/**< 512 КБ куча */

/* Приоритеты задач (выше число = выше приоритет) */
#define TASK_PRIORITY_IDLE      0
#define TASK_PRIORITY_LOW       1
#define TASK_PRIORITY_NORMAL    2
#define TASK_PRIORITY_HIGH      3
#define TASK_PRIORITY_REALTIME  4

/* Размеры стеков задач */
#define TASK_STACK_MAIN         4096
#define TASK_STACK_MINER        8192
#define TASK_STACK_NETWORK      4096
#define TASK_STACK_HTTP         4096
#define TASK_STACK_MONITOR      2048

/* =============================================================================
 * КОНФИГУРАЦИЯ FLASH
 * Адреса разделов во flash памяти
 * ============================================================================= */

#define FLASH_SIZE              (8 * 1024 * 1024)   /**< 8 МБ */

/* Разделы */
#define FLASH_BOOTLOADER_ADDR   0x000000    /**< Загрузчик */
#define FLASH_BOOTLOADER_SIZE   0x010000    /**< 64 КБ */

#define FLASH_FIRMWARE_A_ADDR   0x010000    /**< Прошивка слот A */
#define FLASH_FIRMWARE_A_SIZE   0x1F0000    /**< ~2 МБ */

#define FLASH_FIRMWARE_B_ADDR   0x200000    /**< Прошивка слот B (OTA) */
#define FLASH_FIRMWARE_B_SIZE   0x1F0000    /**< ~2 МБ */

#define FLASH_WEB_ADDR          0x400000    /**< Веб-ресурсы */
#define FLASH_WEB_SIZE          0x100000    /**< 1 МБ */

#define FLASH_CONFIG_ADDR       0x7F0000    /**< Конфигурация */
#define FLASH_CONFIG_SIZE       0x010000    /**< 64 КБ */

/* =============================================================================
 * КОНФИГУРАЦИЯ WATCHDOG
 * ============================================================================= */

#define WDT_TIMEOUT_MS          30000       /**< 30 секунд */
#define WDT_FEED_INTERVAL_MS    10000       /**< Кормить каждые 10 сек */

/* =============================================================================
 * ЛИМИТЫ И БУФЕРЫ
 * ============================================================================= */

#define MAX_STRATUM_MSG_SIZE    16384       /**< Макс. размер Stratum сообщения */
#define MAX_HTTP_REQUEST_SIZE   4096        /**< Макс. размер HTTP запроса */
#define MAX_HTTP_RESPONSE_SIZE  16384       /**< Макс. размер HTTP ответа */
#define MAX_LOG_MESSAGE_SIZE    256         /**< Макс. размер лог-сообщения */
#define MAX_JOB_QUEUE_SIZE      4           /**< Очередь заданий */
#define MAX_NONCE_QUEUE_SIZE    64          /**< Очередь нонсов */

/* =============================================================================
 * МАГИЧЕСКИЕ ЧИСЛА
 * ============================================================================= */

#define CONFIG_MAGIC            0x41564131  /**< "AVA1" - магия конфигурации */
#define FIRMWARE_MAGIC          0x4B323130  /**< "K210" - магия прошивки */

#endif /* _CONFIG_H_ */
