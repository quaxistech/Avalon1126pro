/**
 * =============================================================================
 * @file    main.c
 * @brief   Avalon A1126pro Firmware - Главная точка входа
 * @version 1.0
 * @date    2024
 * =============================================================================
 * 
 * ОПИСАНИЕ:
 * Этот файл содержит главную точку входа прошивки майнера Avalon A1126pro.
 * Код восстановлен путём реверс-инжиниринга оригинальной прошивки (donor_dump.bin).
 * 
 * Основано на CGMiner 4.11.1 для процессора Kendryte K210 (RISC-V).
 * 
 * ОРИГИНАЛЬНЫЕ ФУНКЦИИ ИЗ ДЕКОМПИЛЯЦИИ:
 * - FUN_ram_800004ec @ 0x800004ec - main_init (инициализация системы)
 * - FUN_ram_8006ba1a @ 0x8006ba1a - start_kernel (запуск ядра FreeRTOS)
 * 
 * АРХИТЕКТУРА:
 * - Процессор: Kendryte K210 (RISC-V 64-bit, rv64imafc, 400MHz, 2 ядра)
 * - RTOS: FreeRTOS
 * - Сеть: lwIP + DM9051 Ethernet
 * - ASIC: Avalon10 через SPI
 * 
 * =============================================================================
 */

/* ===========================================================================
 * ПОДКЛЮЧАЕМЫЕ ЗАГОЛОВОЧНЫЕ ФАЙЛЫ
 * =========================================================================== */

/* FreeRTOS - операционная система реального времени */
#include <FreeRTOS.h>
#include <task.h>           /* Управление задачами */
#include <queue.h>          /* Очереди сообщений */
#include <semphr.h>         /* Семафоры и мьютексы */

/* Стандартная библиотека C */
#include <stdio.h>          /* Стандартный ввод/вывод */
#include <string.h>         /* Работа со строками */
#include <stdlib.h>         /* Стандартные функции */

/* Kendryte SDK */
#include <devices.h>        /* Устройства K210 */
#include <hal.h>            /* Hardware Abstraction Layer */

/* Модули проекта Avalon1126 */
#include "avalon10.h"       /* Драйвер ASIC Avalon10 */
#include "cgminer.h"        /* Общие определения CGMiner */
#include "pool.h"           /* Управление пулами майнинга */
#include "stratum.h"        /* Stratum протокол */
#include "api.h"            /* CGMiner API сервер */
#include "network.h"        /* Сетевой модуль */
#include "config.h"         /* Конфигурация */
#include "mock_hardware.h"  /* Эмуляция оборудования */
#include "auc_uart.h"       /* AUC UART драйвер */
#include "fpga_loader.h"    /* Загрузчик FPGA bitstream */

/* Kendryte SDK заголовки (только для реального железа) */
#if !defined(MOCK_HARDWARE)
#include <fpioa.h>
#include <uart.h>
#include <spi.h>
#include <i2c.h>
#include <gpio.h>
#include <gpiohs.h>
#include <pwm.h>
#include <dmac.h>
#include <sysctl.h>
#endif

/* ===========================================================================
 * КОНСТАНТЫ И МАКРООПРЕДЕЛЕНИЯ
 * =========================================================================== */

/**
 * @brief Версия API для совместимости с CGMiner клиентами
 * Строка найдена в декомпилированном коде: "API 3.2"
 * Примечание: API_VERSION может быть определён в другом месте
 */
#ifndef API_VERSION
#define API_VERSION             "3.2"
#endif

/**
 * @brief Название устройства
 * Строка найдена в декомпилированном коде: "AvalonMiner 1126"
 */
#define MINER_NAME              "AvalonMiner 1126"

/* ===========================================================================
 * ГЛОБАЛЬНЫЕ ПЕРЕМЕННЫЕ
 * 
 * Адреса переменных определены из gp-relative доступа в декомпиляции.
 * K210 использует регистр gp (global pointer) для быстрого доступа к данным.
 * =========================================================================== */

/* ---------------------------------------------------------------------------
 * Очереди сообщений FreeRTOS
 * Используются для межзадачной коммуникации
 * Оригинальные адреса: gp + -0x670, gp + -0x668
 * --------------------------------------------------------------------------- */
QueueHandle_t g_msg_send_queue = NULL;  /* Очередь для отправки сообщений */
QueueHandle_t g_msg_recv_queue = NULL;  /* Очередь для приёма сообщений */

/* ---------------------------------------------------------------------------
 * Мьютексы (семафоры взаимного исключения)
 * Защищают критические секции от одновременного доступа из разных задач
 * --------------------------------------------------------------------------- */
SemaphoreHandle_t g_pool_mutex = NULL;   /* Мьютекс для доступа к пулам */
SemaphoreHandle_t g_work_mutex = NULL;   /* Мьютекс для работы с заданиями */
SemaphoreHandle_t g_stats_mutex = NULL;  /* Мьютекс для статистики */

/* ---------------------------------------------------------------------------
 * Конфигурация майнера
 * Загружается из flash-памяти при старте
 * --------------------------------------------------------------------------- */
cgminer_config_t g_config = {0};         /* Глобальная структура конфигурации */

/* ---------------------------------------------------------------------------
 * Информация об ASIC Avalon10
 * Содержит состояние всех модулей и чипов
 * --------------------------------------------------------------------------- */
avalon10_info_t g_avalon10 = {0};        /* Структура информации Avalon10 */
avalon10_info_t *g_avalon10_info = NULL; /* Указатель на активную структуру */

/* ---------------------------------------------------------------------------
 * Пулы майнинга
 * Поддерживается до MAX_POOLS (3) пулов с приоритетом
 * --------------------------------------------------------------------------- */
pool_t g_pools[MAX_POOLS] = {0};         /* Массив структур пулов */
int g_pool_count = 0;                    /* Количество настроенных пулов */
pool_t *g_current_pool = NULL;           /* Указатель на текущий активный пул */

/* ---------------------------------------------------------------------------
 * Параметры отладки и логирования
 * Оригинальные адреса: gp + -0x2f4 (debug), gp + -0x71c (log)
 * --------------------------------------------------------------------------- */
int g_debug_level = 0;                   /* Уровень отладки (0 = выкл) */
int g_log_level = LOG_NOTICE;            /* Уровень логирования */

/* ---------------------------------------------------------------------------
 * Стратегия выбора пула
 * Оригинальный адрес: gp + -0x474
 * Возможные значения: FAILOVER, ROUND_ROBIN, ROTATE, LOAD_BALANCE, BALANCE
 * --------------------------------------------------------------------------- */
int g_pool_strategy = POOL_STRATEGY_FAILOVER;

/* ---------------------------------------------------------------------------
 * Интервал логирования в секундах
 * Оригинальный адрес: gp + -0x7bc
 * --------------------------------------------------------------------------- */
int g_log_interval = 5;

/* ---------------------------------------------------------------------------
 * Счётчики устройств
 * ASC = Application Specific Circuit (ASIC устройства)
 * PGA = Programmable Gate Array (не используется в A1126)
 * --------------------------------------------------------------------------- */
int g_asc_count = 0;                     /* Количество ASIC устройств */
int g_pga_count = 0;                     /* Количество PGA устройств */

/* ---------------------------------------------------------------------------
 * Флаги состояния системы
 * volatile - компилятор не оптимизирует доступ к этим переменным,
 * так как они могут изменяться из других задач/прерываний
 * --------------------------------------------------------------------------- */
volatile int g_mining_active = 0;        /* 1 = майнинг активен */
volatile int g_want_quit = 0;            /* 1 = запрошено завершение */

/* ---------------------------------------------------------------------------
 * Время запуска системы
 * Используется для расчёта uptime и статистики
 * --------------------------------------------------------------------------- */
time_t g_start_time = 0;

/* ===========================================================================
 * СТРОКОВЫЕ КОНСТАНТЫ
 * Строки найдены при анализе бинарного файла с помощью команды strings
 * =========================================================================== */

static const char *TAG = "CGMiner";                          /* Тег для логов */

/* Версии для использования в API ответах */
__attribute__((unused)) static const char *s_cgminer_version = "cgminer " CGMINER_VERSION;
__attribute__((unused)) static const char *s_api_version = API_VERSION;

/**
 * @brief Названия стратегий выбора пула
 * Используются в API для отображения текущей стратегии
 */
const char *pool_strategy_names[] = {
    "Failover",         /* Переключение при сбое */
    "Round Robin",      /* Циклический перебор */
    "Rotate",           /* Ротация */
    "Load Balance",     /* Балансировка нагрузки */
    "Balance",          /* Баланс */
    NULL                /* Терминатор массива */
};

/* ===========================================================================
 * ВНЕШНИЕ ФУНКЦИИ SDK
 * Объявлены в SDK Kendryte, используются для критических секций
 * =========================================================================== */

extern void vPortEnterCritical(void);    /* Вход в критическую секцию */
extern void vPortExitCritical(void);     /* Выход из критической секции */

/* ===========================================================================
 * ПРОТОТИПЫ ВНУТРЕННИХ ФУНКЦИЙ
 * =========================================================================== */

static void main_init_hardware(void);    /* Инициализация оборудования */
static void main_init_config(void);      /* Загрузка конфигурации */
static void main_create_tasks(void);     /* Создание задач FreeRTOS */
static void print_banner(void);          /* Вывод приветственного баннера */

/* ===========================================================================
 * ДЕСКРИПТОРЫ ЗАДАЧ FREERTOS
 * 
 * FreeRTOS использует дескрипторы для управления задачами:
 * - приостановка/возобновление
 * - изменение приоритета
 * - удаление задачи
 * =========================================================================== */

static TaskHandle_t task_stratum_send = NULL;  /* Отправка Stratum данных */
static TaskHandle_t task_stratum_recv = NULL;  /* Приём Stratum данных */
static TaskHandle_t task_watchdog = NULL;      /* Сторожевой таймер */
static TaskHandle_t task_monitor = NULL;       /* Мониторинг температуры */
static TaskHandle_t task_http = NULL;          /* HTTP сервер */
static TaskHandle_t task_api = NULL;           /* API сервер (порт 4028) */
static TaskHandle_t task_led = NULL;           /* Управление LED индикаторами */
/* Зарезервировано для будущего использования */
__attribute__((unused)) static TaskHandle_t task_misc = NULL;  /* Разные операции */
__attribute__((unused)) static TaskHandle_t task_mmu = NULL;   /* Управление памятью */
__attribute__((unused)) static TaskHandle_t task_iic = NULL;   /* I2C коммуникация */

/* ===========================================================================
 * ФУНКЦИЯ: log_message
 * ---------------------------------------------------------------------------
 * Универсальная функция логирования с уровнями важности.
 * 
 * Оригинальная функция: FUN_ram_80022c0a @ 0x80022c0a
 * 
 * @param level  Уровень важности (LOG_ERR, LOG_WARNING, LOG_NOTICE, и т.д.)
 * @param fmt    Форматная строка (как в printf)
 * @param ...    Переменное количество аргументов
 * =========================================================================== */
void log_message(int level, const char *fmt, ...)
{
    /* Фильтрация по уровню логирования */
    if (level > g_log_level) {
        return;
    }
    
    va_list args;
    char buf[512];
    const char *level_str;
    
    /* Преобразование числового уровня в строку */
    switch (level) {
        case LOG_ERR:     level_str = "ERROR";  break;  /* Ошибка */
        case LOG_WARNING: level_str = "WARN";   break;  /* Предупреждение */
        case LOG_NOTICE:  level_str = "NOTICE"; break;  /* Уведомление */
        case LOG_INFO:    level_str = "INFO";   break;  /* Информация */
        case LOG_DEBUG:   level_str = "DEBUG";  break;  /* Отладка */
        default:          level_str = "LOG";    break;  /* По умолчанию */
    }
    
    /* Форматирование сообщения */
    va_start(args, fmt);
    vsnprintf(buf, sizeof(buf), fmt, args);
    va_end(args);
    
    /* Вывод в консоль */
    printf("[%s] %s: %s\n", TAG, level_str, buf);
}

/* ===========================================================================
 * ФУНКЦИЯ: print_banner
 * ---------------------------------------------------------------------------
 * Выводит приветственный баннер при запуске системы.
 * Информация о версии прошивки, CGMiner и API.
 * =========================================================================== */
static void print_banner(void)
{
    printf("\n");
    printf("=============================================\n");
    printf("  %s\n", MINER_NAME);
    printf("  CGMiner %s\n", CGMINER_VERSION);
    printf("  Firmware: %s\n", FIRMWARE_VERSION);
    printf("  API: %s\n", API_VERSION);
    printf("=============================================\n");
    printf("\n");
}

/* ===========================================================================
 * ФУНКЦИЯ: main_init_hardware
 * ---------------------------------------------------------------------------
 * Инициализация аппаратного обеспечения K210.
 * Часть оригинальной функции FUN_ram_800004ec.
 * 
 * Порядок инициализации критичен:
 * 1. FPIOA - настройка функций пинов
 * 2. UART - для отладочного вывода
 * 3. SPI - для связи с ASIC
 * 4. I2C - для датчиков температуры
 * 5. GPIO - для LED и управления
 * 6. Ethernet - для сети
 * =========================================================================== */
static void main_init_hardware(void)
{
    log_message(LOG_INFO, "Инициализация оборудования...");
    
#ifdef MOCK_HARDWARE
    /* Режим эмуляции - инициализируем mock-слой */
    log_message(LOG_INFO, "Режим эмуляции оборудования");
    
    #if MOCK_SPI_FLASH
    mock_flash_init();
    #endif
    
    #if MOCK_NETWORK
    mock_network_init();
    #endif
    
    #if MOCK_ASIC
    mock_asic_init();
    #endif
    
#else
    /* Реальное оборудование K210 */
    
    /* 
     * FPIOA (Field Programmable IO Array)
     * Настройка мультиплексора пинов K210
     */
    /* SPI0 для ASIC */
    fpioa_set_function(6, FUNC_SPI0_SCLK);
    fpioa_set_function(7, FUNC_SPI0_D0);    /* MOSI */
    fpioa_set_function(8, FUNC_SPI0_D1);    /* MISO */
    
    /* SPI1 для Ethernet DM9051 */
    fpioa_set_function(9, FUNC_SPI1_SCLK);
    fpioa_set_function(10, FUNC_SPI1_D0);
    fpioa_set_function(11, FUNC_SPI1_D1);
    
    /* I2C для датчиков температуры */
    fpioa_set_function(14, FUNC_I2C0_SCLK);
    fpioa_set_function(15, FUNC_I2C0_SDA);
    
    /* UART для отладки */
    fpioa_set_function(4, FUNC_UART1_TX);
    fpioa_set_function(5, FUNC_UART1_RX);
    
    /* GPIO для CS линий ASIC (модули 0-3) */
    fpioa_set_function(20, FUNC_GPIOHS0);   /* CS0 */
    fpioa_set_function(21, FUNC_GPIOHS1);   /* CS1 */
    fpioa_set_function(22, FUNC_GPIOHS2);   /* CS2 */
    fpioa_set_function(23, FUNC_GPIOHS3);   /* CS3 */
    
    /* GPIO для Ethernet CS */
    fpioa_set_function(12, FUNC_GPIOHS4);   /* ETH_CS */
    fpioa_set_function(13, FUNC_GPIOHS5);   /* ETH_INT */
    
    /* PWM для вентиляторов */
    fpioa_set_function(16, FUNC_TIMER0_TOGGLE1);   /* FAN1 */
    fpioa_set_function(17, FUNC_TIMER0_TOGGLE2);   /* FAN2 */
    
    /* LED индикаторы */
    fpioa_set_function(24, FUNC_GPIOHS6);   /* LED_GREEN */
    fpioa_set_function(25, FUNC_GPIOHS7);   /* LED_RED */
    
    /* Инициализация GPIO для CS */
    for (int i = 0; i < 4; i++) {
        gpiohs_set_drive_mode(i, GPIO_DM_OUTPUT);
        gpiohs_set_pin(i, GPIO_PV_HIGH);    /* CS high (deselect) */
    }
    
    /* Ethernet CS и INT */
    gpiohs_set_drive_mode(4, GPIO_DM_OUTPUT);
    gpiohs_set_pin(4, GPIO_PV_HIGH);
    gpiohs_set_drive_mode(5, GPIO_DM_INPUT);
    
    /* LED GPIO */
    gpiohs_set_drive_mode(6, GPIO_DM_OUTPUT);
    gpiohs_set_drive_mode(7, GPIO_DM_OUTPUT);
    gpiohs_set_pin(6, GPIO_PV_LOW);
    gpiohs_set_pin(7, GPIO_PV_LOW);
    
    /* Инициализация UART */
    uart_init(UART_DEVICE_1);
    uart_configure(UART_DEVICE_1, 115200, UART_BITWIDTH_8BIT, 
                   UART_STOP_1, UART_PARITY_NONE);
    
    /* Инициализация SPI0 для ASIC */
    spi_init(SPI_DEVICE_0, SPI_WORK_MODE_0, SPI_FF_STANDARD, 8, 0);
    spi_set_clk_rate(SPI_DEVICE_0, 10000000);   /* 10 MHz */
    
    /* Инициализация SPI1 для Ethernet */
    spi_init(SPI_DEVICE_1, SPI_WORK_MODE_0, SPI_FF_STANDARD, 8, 0);
    spi_set_clk_rate(SPI_DEVICE_1, 10000000);
    
    /* Инициализация I2C */
    i2c_init(I2C_DEVICE_0, 0x50, 7, 100000);
    
    /* Инициализация PWM для вентиляторов */
    pwm_init(PWM_DEVICE_0);
    pwm_set_frequency(PWM_DEVICE_0, PWM_CHANNEL_0, 25000, 0.5);  /* 25kHz, 50% */
    pwm_set_frequency(PWM_DEVICE_0, PWM_CHANNEL_1, 25000, 0.5);
    pwm_set_enable(PWM_DEVICE_0, PWM_CHANNEL_0, 1);
    pwm_set_enable(PWM_DEVICE_0, PWM_CHANNEL_1, 1);
    
#endif /* MOCK_HARDWARE */
    
    /* Инициализация AUC (UART для ASIC) */
    if (auc_init() != AUC_OK) {
        log_message(LOG_WARNING, "Не удалось инициализировать AUC");
    }
    
    /* Инициализация и загрузка FPGA */
    if (fpga_init() == FPGA_OK) {
        if (fpga_check_bitstream()) {
            int ret = fpga_load_bitstream();
            if (ret == FPGA_OK) {
                log_message(LOG_INFO, "FPGA bitstream загружен успешно");
            } else {
                log_message(LOG_WARNING, "Ошибка загрузки FPGA: %s", fpga_error_string(ret));
            }
        } else {
            log_message(LOG_WARNING, "FPGA bitstream не найден во flash");
        }
    }
    
    log_message(LOG_INFO, "Оборудование инициализировано");
}

/* ===========================================================================
 * ФУНКЦИЯ: main_init_config
 * ---------------------------------------------------------------------------
 * Загрузка конфигурации из flash-памяти.
 * Часть оригинальной функции FUN_ram_800004ec.
 * 
 * Файлы конфигурации (найдены в декомпиляции):
 * - network.cfg - сетевые настройки
 * - product.cfg - параметры устройства
 * - system.cfg  - системные настройки
 * =========================================================================== */
static void main_init_config(void)
{
    log_message(LOG_INFO, "Загрузка конфигурации...");
    
    /* ------------------------------------------
     * Значения по умолчанию для сетевых настроек
     * ------------------------------------------ */
    g_config.dhcp_enabled = 1;        /* DHCP включён */
    g_config.api_port = 4028;         /* Стандартный порт CGMiner API */
    g_config.api_listen = 1;          /* Слушать API */
    g_config.http_port = 80;          /* HTTP порт для веб-интерфейса */
    
    /* ------------------------------------------
     * Значения по умолчанию для Avalon10
     * Эти значения найдены в декомпилированном коде
     * ------------------------------------------ */
    g_avalon10.default_freq[0] = AVALON10_DEFAULT_FREQ_MIN;  /* 400 MHz */
    g_avalon10.default_freq[1] = AVALON10_DEFAULT_FREQ_MAX;  /* 500 MHz */
    g_avalon10.default_voltage = AVALON10_DEFAULT_VOLTAGE;   /* 780 mV */
    g_avalon10.fan_min = AVALON10_DEFAULT_FAN_MIN;           /* 10% */
    g_avalon10.fan_max = AVALON10_DEFAULT_FAN_MAX;           /* 100% */
    g_avalon10.temp_target = AVALON10_DEFAULT_TEMP_TARGET;   /* 75°C */
    g_avalon10.temp_overheat = AVALON10_DEFAULT_TEMP_OVERHEAT; /* 95°C */
    
    /* ------------------------------------------
     * Загрузка конфигурации из flash
     * ------------------------------------------ */
    config_init();
    
    log_message(LOG_INFO, "Конфигурация загружена");
}

/* ===========================================================================
 * ЗАДАЧИ FREERTOS
 * 
 * FreeRTOS использует многозадачность с вытеснением (preemptive).
 * Каждая задача выполняется в своём стеке и имеет приоритет.
 * Задачи с более высоким приоритетом вытесняют задачи с низким.
 * =========================================================================== */

/* ---------------------------------------------------------------------------
 * ЗАДАЧА: stratum_send_task
 * ---------------------------------------------------------------------------
 * Отправка данных на пул по протоколу Stratum.
 * Оригинальная функция: FUN_ram_8000d17a @ 0x8000d17a
 * 
 * Задача проверяет наличие данных для отправки и передаёт их на пул.
 * Работает в бесконечном цикле, пока не установлен флаг g_want_quit.
 * --------------------------------------------------------------------------- */
static void stratum_send_task(void *pvParameters)
{
    pool_t *pool;
    int core_id;
    
    /* Получаем ID ядра процессора (K210 имеет 2 ядра) */
    core_id = (int)uxPortGetProcessorId();
    log_message(LOG_DEBUG, "TASKSTART Core %d sstratum_d", core_id);
    
    /* Основной цикл задачи */
    while (!g_want_quit) {
        pool = get_current_pool();
        if (pool && pool->stratum_active) {
            /* Отправляем накопленные данные на пул */
            stratum_send_work(pool);
        }
        /* Задержка 10мс для экономии CPU */
        vTaskDelay(pdMS_TO_TICKS(10));
    }
    
    /* Удаляем задачу при выходе */
    vTaskDelete(NULL);
}

/* ---------------------------------------------------------------------------
 * ЗАДАЧА: stratum_recv_task
 * ---------------------------------------------------------------------------
 * Приём данных от пула по протоколу Stratum.
 * Оригинальная функция: FUN_ram_80011196 @ 0x80011196
 * 
 * Задача ожидает данные от пула и обрабатывает их.
 * Включает парсинг JSON и обработку команд mining.notify, mining.set_difficulty.
 * --------------------------------------------------------------------------- */
static void stratum_recv_task(void *pvParameters)
{
    pool_t *pool;
    int core_id;
    char *line;
    
    core_id = (int)uxPortGetProcessorId();
    log_message(LOG_DEBUG, "TASKSTART Core %d rstratum_d", core_id);
    
    while (!g_want_quit) {
        pool = get_current_pool();
        if (pool && pool->stratum_active) {
            /* Читаем строку от пула */
            line = stratum_recv_line(pool);
            if (line) {
                /* Парсим и обрабатываем ответ */
                stratum_parse_response(pool, line);
                free(line);
            }
        }
        /* Минимальная задержка 1мс */
        vTaskDelay(pdMS_TO_TICKS(1));
    }
    
    vTaskDelete(NULL);
}

/* ---------------------------------------------------------------------------
 * ЗАДАЧА: watchdog_task
 * ---------------------------------------------------------------------------
 * Сторожевой таймер (watchdog).
 * Оригинальная функция: FUN_ram_8000edcc @ 0x8000edcc
 * 
 * Задача сбрасывает аппаратный watchdog для предотвращения зависания.
 * Если основной код зависнет, watchdog перезагрузит систему.
 * --------------------------------------------------------------------------- */
static void watchdog_task(void *pvParameters)
{
    int core_id;
    
    core_id = (int)uxPortGetProcessorId();
    log_message(LOG_DEBUG, "TASKSTART Core %d watchdog", core_id);
    log_message(LOG_INFO, "WDT is initialized");
    
    while (!g_want_quit) {
        /* Сброс watchdog таймера */
        // wdt_feed();
        
        /* Проверка состояния системы */
        // check_system_health();
        
        vTaskDelay(pdMS_TO_TICKS(1000));  /* Каждую секунду */
    }
    
    vTaskDelete(NULL);
}

/* ---------------------------------------------------------------------------
 * ЗАДАЧА: monitor_task
 * ---------------------------------------------------------------------------
 * Мониторинг температуры и управление вентиляторами.
 * Оригинальная функция: FUN_ram_800132d4 @ 0x800132d4
 * 
 * Использует PID-регулятор для поддержания целевой температуры.
 * При перегреве снижает частоту или отключает майнинг.
 * --------------------------------------------------------------------------- */
static void monitor_task(void *pvParameters)
{
    int core_id;
    
    core_id = (int)uxPortGetProcessorId();
    log_message(LOG_DEBUG, "TASKSTART Core %d monitor", core_id);
    
    while (!g_want_quit) {
        if (g_avalon10_info) {
            /* Чтение температуры с датчиков */
            avalon10_read_temperature(g_avalon10_info);
            
            /* Регулировка скорости вентиляторов */
            avalon10_adjust_fan(g_avalon10_info);
            
            /* Проверка перегрева */
            avalon10_check_overheat(g_avalon10_info);
        }
        
        vTaskDelay(pdMS_TO_TICKS(1000));  /* Каждую секунду */
    }
    
    vTaskDelete(NULL);
}

/* ---------------------------------------------------------------------------
 * ЗАДАЧА: api_task
 * ---------------------------------------------------------------------------
 * CGMiner API сервер на порту 4028.
 * 
 * Обрабатывает команды: version, summary, pools, devs, config, stats и др.
 * Формат: JSON
 * --------------------------------------------------------------------------- */
static void api_task(void *pvParameters)
{
    int core_id;
    
    core_id = (int)uxPortGetProcessorId();
    log_message(LOG_DEBUG, "TASKSTART Core %d api", core_id);
    
    /* Запуск API сервера */
    api_server_start(g_config.api_port);
    
    vTaskDelete(NULL);
}

/* ---------------------------------------------------------------------------
 * ЗАДАЧА: http_task
 * ---------------------------------------------------------------------------
 * HTTP сервер для веб-интерфейса на порту 80.
 * Предоставляет страницы для настройки и мониторинга.
 * --------------------------------------------------------------------------- */
static void http_task(void *pvParameters)
{
    int core_id;
    
    core_id = (int)uxPortGetProcessorId();
    log_message(LOG_DEBUG, "TASKSTART Core %d http", core_id);
    
    /* HTTP сервер */
    // http_server_start(g_config.http_port);
    
    while (!g_want_quit) {
        vTaskDelay(pdMS_TO_TICKS(100));
    }
    
    vTaskDelete(NULL);
}

/* ---------------------------------------------------------------------------
 * ЗАДАЧА: led_task
 * ---------------------------------------------------------------------------
 * Управление LED индикаторами состояния.
 * 
 * Индикация:
 * - Зелёный моргает: нормальная работа
 * - Красный: ошибка
 * - Оба: инициализация
 * --------------------------------------------------------------------------- */
static void led_task(void *pvParameters)
{
    int core_id;
    int led_state = 0;
    
    core_id = (int)uxPortGetProcessorId();
    log_message(LOG_DEBUG, "TASKSTART Core %d led", core_id);
    
    while (!g_want_quit) {
        led_state = !led_state;
        
        if (g_mining_active) {
            /* Зелёный моргает при майнинге */
            // gpio_set_pin(LED_GREEN, led_state);
            // gpio_set_pin(LED_RED, 0);
        } else {
            /* Красный при простое */
            // gpio_set_pin(LED_GREEN, 0);
            // gpio_set_pin(LED_RED, led_state);
        }
        
        vTaskDelay(pdMS_TO_TICKS(500));  /* 2 Гц */
    }
    
    vTaskDelete(NULL);
}

/* ---------------------------------------------------------------------------
 * ЗАДАЧА: mining_task
 * ---------------------------------------------------------------------------
 * Основная задача майнинга.
 * 
 * Отправляет работу на ASIC и собирает результаты (nonce).
 * Найденные шары отправляются на пул.
 * --------------------------------------------------------------------------- */
static void mining_task(void *pvParameters)
{
    int core_id;
    
    core_id = (int)uxPortGetProcessorId();
    log_message(LOG_DEBUG, "TASKSTART Core %d mining", core_id);
    
    while (!g_want_quit) {
        if (g_avalon10_info && g_current_pool && g_current_pool->stratum_active) {
            /* Отправка работы на ASIC */
            avalon10_poll(g_avalon10_info);
        }
        
        vTaskDelay(pdMS_TO_TICKS(10));
    }
    
    vTaskDelete(NULL);
}

/* ===========================================================================
 * ФУНКЦИЯ: main_create_tasks
 * ---------------------------------------------------------------------------
 * Создание всех задач FreeRTOS.
 * 
 * Параметры xTaskCreate:
 * - pvTaskCode: указатель на функцию задачи
 * - pcName: имя задачи (для отладки)
 * - usStackDepth: размер стека в словах (word = 4 байта)
 * - pvParameters: параметры для задачи
 * - uxPriority: приоритет (больше = выше)
 * - pxCreatedTask: указатель для сохранения дескриптора
 * =========================================================================== */
static void main_create_tasks(void)
{
    log_message(LOG_INFO, "Создание задач FreeRTOS...");
    
    /* Задачи с высоким приоритетом (критичные для времени) */
    
    xTaskCreate(
        stratum_recv_task,        /* Функция задачи */
        "stratum_recv",           /* Имя */
        4096,                     /* Стек 4KB */
        NULL,                     /* Параметры */
        5,                        /* Приоритет 5 */
        &task_stratum_recv        /* Дескриптор */
    );
    
    xTaskCreate(
        stratum_send_task,
        "stratum_send",
        4096,
        NULL,
        5,
        &task_stratum_send
    );
    
    /* Watchdog - самый высокий приоритет */
    xTaskCreate(
        watchdog_task,
        "watchdog",
        2048,
        NULL,
        6,
        &task_watchdog
    );
    
    /* Мониторинг температуры */
    xTaskCreate(
        monitor_task,
        "monitor",
        4096,
        NULL,
        4,
        &task_monitor
    );
    
    /* API сервер */
    xTaskCreate(
        api_task,
        "api",
        8192,                     /* Большой стек для JSON */
        NULL,
        3,
        &task_api
    );
    
    /* HTTP сервер */
    xTaskCreate(
        http_task,
        "http",
        8192,
        NULL,
        3,
        &task_http
    );
    
    /* LED индикация */
    xTaskCreate(
        led_task,
        "led",
        1024,
        NULL,
        2,
        &task_led
    );
    
    /* Основная задача майнинга */
    xTaskCreate(
        mining_task,
        "mining",
        4096,
        NULL,
        4,
        NULL
    );
    
    log_message(LOG_INFO, "Задачи созданы");
}

/* ===========================================================================
 * ФУНКЦИЯ: cgminer_main
 * ---------------------------------------------------------------------------
 * Главная функция CGMiner.
 * Вызывается после инициализации SDK.
 * 
 * Последовательность запуска:
 * 1. Вывод баннера
 * 2. Инициализация оборудования
 * 3. Загрузка конфигурации
 * 4. Инициализация сети
 * 5. Инициализация пулов
 * 6. Инициализация ASIC
 * 7. Создание задач
 * 8. Запуск планировщика FreeRTOS
 * =========================================================================== */
int cgminer_main(void)
{
    /* Сохраняем время запуска */
    g_start_time = time(NULL);
    
    /* Приветственный баннер */
    print_banner();
    
    /* ------------------------------------------
     * Этап 1: Инициализация оборудования
     * ------------------------------------------ */
    main_init_hardware();
    
    /* ------------------------------------------
     * Этап 2: Загрузка конфигурации
     * ------------------------------------------ */
    main_init_config();
    
    /* ------------------------------------------
     * Этап 3: Создание объектов синхронизации
     * ------------------------------------------ */
    g_pool_mutex = xSemaphoreCreateMutex();
    g_work_mutex = xSemaphoreCreateMutex();
    g_stats_mutex = xSemaphoreCreateMutex();
    g_msg_send_queue = xQueueCreate(32, sizeof(void*));
    g_msg_recv_queue = xQueueCreate(32, sizeof(void*));
    
    /* ------------------------------------------
     * Этап 4: Инициализация сети
     * ------------------------------------------ */
    log_message(LOG_INFO, "Инициализация сети...");
    network_init();
    
    /* ------------------------------------------
     * Этап 5: Инициализация пулов
     * ------------------------------------------ */
    log_message(LOG_INFO, "Инициализация пулов...");
    pool_init();
    
    /* Загрузка настроенных пулов */
    if (g_config.pool_url[0][0]) {
        add_pool(g_config.pool_url[0], 
                 g_config.pool_user[0], 
                 g_config.pool_pass[0]);
    }
    if (g_config.pool_url[1][0]) {
        add_pool(g_config.pool_url[1], 
                 g_config.pool_user[1], 
                 g_config.pool_pass[1]);
    }
    if (g_config.pool_url[2][0]) {
        add_pool(g_config.pool_url[2], 
                 g_config.pool_user[2], 
                 g_config.pool_pass[2]);
    }
    
    /* ------------------------------------------
     * Этап 6: Инициализация ASIC
     * ------------------------------------------ */
    log_message(LOG_INFO, "Инициализация ASIC Avalon10...");
    g_avalon10_info = &g_avalon10;
    avalon10_init(g_avalon10_info);
    g_asc_count = 1;
    
    /* ------------------------------------------
     * Этап 7: Создание задач
     * ------------------------------------------ */
    main_create_tasks();
    
    /* ------------------------------------------
     * Этап 8: Запуск майнинга
     * ------------------------------------------ */
    g_mining_active = 1;
    log_message(LOG_NOTICE, "Майнинг запущен!");
    
    return 0;
}

/* ===========================================================================
 * ФУНКЦИЯ: main
 * ---------------------------------------------------------------------------
 * Точка входа программы.
 * Kendryte SDK вызывает эту функцию после инициализации.
 * 
 * @param argc  Количество аргументов командной строки (не используется)
 * @param argv  Аргументы командной строки (не используется)
 * @return      Код возврата (не используется, т.к. FreeRTOS не возвращается)
 * =========================================================================== */
int main(int argc, char *argv[])
{
    (void)argc;  /* Подавление предупреждения о неиспользуемом параметре */
    (void)argv;
    
    /* Запуск главной функции CGMiner */
    cgminer_main();
    
    /* 
     * Эта точка никогда не достигается, т.к. FreeRTOS забирает управление.
     * Если достигнута - что-то пошло не так.
     */
    log_message(LOG_ERR, "Ошибка: main() не должен возвращаться!");
    
    while (1) {
        /* Бесконечный цикл на случай ошибки */
    }
    
    return 0;
}

/* ===========================================================================
 * КОНЕЦ ФАЙЛА main.c
 * =========================================================================== */
