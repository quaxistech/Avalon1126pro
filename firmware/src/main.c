/**
 * =============================================================================
 * @file    main.c
 * @brief   Главная точка входа прошивки Avalon A1126pro
 * =============================================================================
 * 
 * Этот файл содержит функцию main() и инициализацию всей системы.
 * Майнер запускается на процессоре Kendryte K210 под управлением FreeRTOS.
 * 
 * Последовательность запуска:
 * 1. Инициализация аппаратуры K210
 * 2. Инициализация FreeRTOS
 * 3. Запуск задач (майнер, сеть, веб-сервер и т.д.)
 * 4. Основной цикл планировщика
 * 
 * @author  Reconstructed from Avalon A1126pro firmware
 * @version 1.0
 * =============================================================================
 */

/* Стандартные заголовки */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>

/* Заголовки проекта */
#include "config.h"
#include "miner.h"
#include "avalon10.h"
#include "k210_hal.h"
#include "stratum.h"
#include "http_server.h"
#include "network.h"

/* FreeRTOS заголовки */
#ifdef USE_FREERTOS
#include "FreeRTOS.h"
#include "task.h"
#include "semphr.h"
#include "queue.h"
#endif

/* =============================================================================
 * ГЛОБАЛЬНЫЕ ПЕРЕМЕННЫЕ
 * ============================================================================= */

/** Статус инициализации системы */
static bool g_system_initialized = false;

/** Флаг активности майнинга */
bool mining_active = false;

/** Мьютекс для защиты критических секций */
static SemaphoreHandle_t g_mutex = NULL;

/** Очередь заданий для майнера */
static QueueHandle_t g_work_queue = NULL;

/** Очередь найденных нонсов */
static QueueHandle_t g_nonce_queue = NULL;

/** Хендлы задач FreeRTOS */
static TaskHandle_t g_task_miner = NULL;        /**< Задача майнера */
static TaskHandle_t g_task_network = NULL;      /**< Сетевая задача */
static TaskHandle_t g_task_http = NULL;         /**< HTTP сервер */
static TaskHandle_t g_task_monitor = NULL;      /**< Мониторинг системы */
static TaskHandle_t g_task_stratum = NULL;      /**< Stratum клиент */

/* =============================================================================
 * ПРОТОТИПЫ ЛОКАЛЬНЫХ ФУНКЦИЙ
 * ============================================================================= */

static void system_init(void);
static void create_tasks(void);
static void task_miner(void *pvParameters);
static void task_network(void *pvParameters);
static void task_http_server(void *pvParameters);
static void task_system_monitor(void *pvParameters);
static void task_stratum_client(void *pvParameters);
static void print_startup_banner(void);

/* =============================================================================
 * ТОЧКА ВХОДА
 * ============================================================================= */

/**
 * @brief Главная функция - точка входа программы
 * 
 * Выполняет полную инициализацию системы и запускает планировщик FreeRTOS.
 * После запуска планировщика эта функция больше не выполняется.
 * 
 * @return Никогда не возвращает управление
 */
int main(void)
{
    /* Отключаем прерывания на время инициализации */
    __disable_irq();
    
    /* =========================================================================
     * ЭТАП 1: Инициализация аппаратуры
     * ========================================================================= */
    
    /* Инициализация системы (тактирование, память, периферия) */
    system_init();
    
    /* Выводим приветственный баннер */
    print_startup_banner();
    
    applog(LOG_INFO, "Запуск системы инициализации...");
    
    /* =========================================================================
     * ЭТАП 2: Инициализация компонентов
     * ========================================================================= */
    
    /* Загрузка конфигурации из Flash */
    applog(LOG_INFO, "Загрузка конфигурации...");
    if (avalon10_load_config() != 0) {
        applog(LOG_WARNING, "Конфигурация не найдена, используем значения по умолчанию");
        avalon10_reset_config();
    }
    
    /* Инициализация драйвера Avalon10 */
    applog(LOG_INFO, "Инициализация драйвера Avalon10...");
    if (avalon10_init() != 0) {
        applog(LOG_ERR, "Ошибка инициализации Avalon10!");
        /* Критическая ошибка - мигаем красным светодиодом */
        led_error_blink();
        while(1) { /* Зависаем */ }
    }
    
    /* Обнаружение хеш-плат */
    applog(LOG_INFO, "Поиск хеш-плат...");
    int miners_found = avalon10_detect();
    if (miners_found == 0) {
        applog(LOG_ERR, "Хеш-платы не обнаружены!");
        led_error_blink();
        /* Продолжаем - платы могут появиться позже */
    } else {
        applog(LOG_INFO, "Обнаружено хеш-плат: %d", miners_found);
    }
    
    /* Инициализация сетевого стека */
    applog(LOG_INFO, "Инициализация сети...");
    if (network_init() != 0) {
        applog(LOG_ERR, "Ошибка инициализации сети!");
        /* Не критично - продолжаем без сети */
    }
    
    /* =========================================================================
     * ЭТАП 3: Создание объектов синхронизации
     * ========================================================================= */
    
    applog(LOG_INFO, "Создание объектов синхронизации...");
    
    /* Создаём мьютекс для критических секций */
    g_mutex = xSemaphoreCreateMutex();
    if (g_mutex == NULL) {
        applog(LOG_ERR, "Ошибка создания мьютекса!");
        while(1) { }
    }
    
    /* Создаём очередь для заданий майнинга */
    g_work_queue = xQueueCreate(MAX_JOB_QUEUE_SIZE, sizeof(work_t *));
    if (g_work_queue == NULL) {
        applog(LOG_ERR, "Ошибка создания очереди заданий!");
        while(1) { }
    }
    
    /* Создаём очередь для найденных нонсов */
    g_nonce_queue = xQueueCreate(MAX_NONCE_QUEUE_SIZE, sizeof(avalon10_nonce_t));
    if (g_nonce_queue == NULL) {
        applog(LOG_ERR, "Ошибка создания очереди нонсов!");
        while(1) { }
    }
    
    /* =========================================================================
     * ЭТАП 4: Создание задач FreeRTOS
     * ========================================================================= */
    
    applog(LOG_INFO, "Создание задач FreeRTOS...");
    create_tasks();
    
    /* =========================================================================
     * ЭТАП 5: Финальная инициализация
     * ========================================================================= */
    
    /* Включаем прерывания */
    __enable_irq();
    
    /* Устанавливаем флаг готовности */
    g_system_initialized = true;
    
    /* Включаем зелёный светодиод - система готова */
    led_set_status(LED_STATUS_READY);
    
    applog(LOG_INFO, "===========================================");
    applog(LOG_INFO, "Система готова к работе!");
    applog(LOG_INFO, "===========================================");
    
    /* =========================================================================
     * ЭТАП 6: Запуск планировщика
     * ========================================================================= */
    
    /* Запускаем планировщик FreeRTOS
     * После этого вызова управление передаётся планировщику
     * и функция main() больше не выполняется */
    vTaskStartScheduler();
    
    /* Сюда мы никогда не должны попасть */
    applog(LOG_ERR, "Критическая ошибка: планировщик завершился!");
    while(1) {
        led_error_blink();
    }
    
    return 0;
}

/* =============================================================================
 * ИНИЦИАЛИЗАЦИЯ СИСТЕМЫ
 * ============================================================================= */

/**
 * @brief Полная инициализация аппаратуры K210
 * 
 * Выполняет настройку:
 * - Системного тактирования (PLL)
 * - GPIO пинов
 * - SPI для связи с ASIC
 * - UART для отладки
 * - I2C для датчиков
 * - PWM для вентиляторов
 * - Watchdog таймера
 */
static void system_init(void)
{
    /* ---------------------------------------------------------------------
     * Настройка тактирования
     * --------------------------------------------------------------------- */
    
    /* Инициализация системного контроллера */
    sysctl_init();
    
    /* Настройка PLL для максимальной производительности */
    sysctl_pll_set_freq(SYSCTL_PLL0, K210_PLL0_FREQ);   /* 800 МГц */
    sysctl_pll_set_freq(SYSCTL_PLL1, K210_PLL1_FREQ);   /* 400 МГц */
    
    /* Установка частоты CPU */
    sysctl_cpu_set_freq(K210_CPU_FREQ);                  /* 400 МГц */
    
    /* ---------------------------------------------------------------------
     * Включение периферийных модулей
     * --------------------------------------------------------------------- */
    
    sysctl_clock_enable(SYSCTL_CLOCK_GPIO);
    sysctl_clock_enable(SYSCTL_CLOCK_SPI0);
    sysctl_clock_enable(SYSCTL_CLOCK_I2C0);
    sysctl_clock_enable(SYSCTL_CLOCK_UART1);
    sysctl_clock_enable(SYSCTL_CLOCK_TIMER0);
    sysctl_clock_enable(SYSCTL_CLOCK_TIMER1);
    
    /* ---------------------------------------------------------------------
     * Настройка GPIO
     * --------------------------------------------------------------------- */
    
    gpio_init();
    
    /* Выходы для управления хеш-платами */
    gpio_set_drive_mode(PIN_HASH_RESET, GPIO_DM_OUTPUT);
    gpio_set_drive_mode(PIN_HASH_CHAIN_EN, GPIO_DM_OUTPUT);
    gpio_set_drive_mode(PIN_HASH_POWER_EN, GPIO_DM_OUTPUT);
    
    /* Выходы для светодиодов */
    gpio_set_drive_mode(PIN_LED_STATUS, GPIO_DM_OUTPUT);
    gpio_set_drive_mode(PIN_LED_ERROR, GPIO_DM_OUTPUT);
    gpio_set_drive_mode(PIN_LED_NETWORK, GPIO_DM_OUTPUT);
    
    /* Начальное состояние - всё выключено */
    gpio_set_pin(PIN_HASH_RESET, GPIO_PV_LOW);
    gpio_set_pin(PIN_HASH_CHAIN_EN, GPIO_PV_LOW);
    gpio_set_pin(PIN_HASH_POWER_EN, GPIO_PV_LOW);
    gpio_set_pin(PIN_LED_STATUS, GPIO_PV_LOW);
    gpio_set_pin(PIN_LED_ERROR, GPIO_PV_LOW);
    gpio_set_pin(PIN_LED_NETWORK, GPIO_PV_LOW);
    
    /* ---------------------------------------------------------------------
     * Настройка UART для отладки
     * --------------------------------------------------------------------- */
    
    uart_init(UART_DEBUG_NUM);
    uart_configure(UART_DEBUG_NUM, UART_DEBUG_BAUDRATE, 
                   UART_BITWIDTH_8BIT, UART_STOP_1, UART_PARITY_NONE);
    
    /* ---------------------------------------------------------------------
     * Настройка SPI для связи с ASIC
     * --------------------------------------------------------------------- */
    
    /* Настройка SPI пинов через FPIOA */
    fpioa_set_function(PIN_SPI_MOSI, FUNC_SPI0_D0);
    fpioa_set_function(PIN_SPI_MISO, FUNC_SPI0_D1);
    fpioa_set_function(PIN_SPI_SCLK, FUNC_SPI0_SCLK);
    fpioa_set_function(PIN_SPI_CS0, FUNC_SPI0_SS0);
    fpioa_set_function(PIN_SPI_CS1, FUNC_SPI0_SS1);
    fpioa_set_function(PIN_SPI_CS2, FUNC_SPI0_SS2);
    fpioa_set_function(PIN_SPI_CS3, FUNC_SPI0_SS3);
    
    /* Инициализация SPI */
    spi_init(SPI_HASH_DEVICE, SPI_WORK_MODE_0, SPI_FF_STANDARD, 
             SPI_HASH_BITS, 0);
    spi_set_clk_rate(SPI_HASH_DEVICE, SPI_HASH_BAUDRATE);
    
    /* ---------------------------------------------------------------------
     * Настройка I2C для датчиков температуры
     * --------------------------------------------------------------------- */
    
    fpioa_set_function(PIN_I2C_SDA, FUNC_I2C0_SDA);
    fpioa_set_function(PIN_I2C_SCL, FUNC_I2C0_SCLK);
    
    i2c_init(I2C_SENSOR_NUM, I2C_ADDR_TEMP_SENSOR, 7, I2C_SENSOR_SPEED);
    
    /* ---------------------------------------------------------------------
     * Настройка PWM для вентиляторов
     * --------------------------------------------------------------------- */
    
    fpioa_set_function(PIN_FAN_PWM0, FUNC_TIMER0_TOGGLE1);
    fpioa_set_function(PIN_FAN_PWM1, FUNC_TIMER0_TOGGLE2);
    
    timer_init(TIMER_DEVICE_0);
    timer_set_interval(TIMER_DEVICE_0, TIMER_CHANNEL_0, 
                       1000000000 / AVA10_FAN_PWM_FREQ);  /* Период ШИМ */
    
    /* ---------------------------------------------------------------------
     * Настройка Watchdog
     * --------------------------------------------------------------------- */
    
#ifdef USE_WATCHDOG
    wdt_init(WDT_DEVICE_0, WDT_TIMEOUT_MS, NULL);
    wdt_start(WDT_DEVICE_0);
#endif
    
    /* ---------------------------------------------------------------------
     * Инициализация контроллера прерываний
     * --------------------------------------------------------------------- */
    
    plic_init();
}

/* =============================================================================
 * СОЗДАНИЕ ЗАДАЧ
 * ============================================================================= */

/**
 * @brief Создание всех задач FreeRTOS
 * 
 * Создаёт задачи для:
 * - Майнинга (высший приоритет)
 * - Сетевого стека
 * - HTTP сервера (веб-интерфейс)
 * - Stratum клиента (связь с пулом)
 * - Мониторинга системы
 */
static void create_tasks(void)
{
    BaseType_t ret;
    
    /* ----- Задача майнера ----- */
    /* Это главная задача - выполняет хеширование на ASIC чипах */
    ret = xTaskCreate(
        task_miner,                     /* Функция задачи */
        "miner",                        /* Имя для отладки */
        TASK_STACK_MINER,               /* Размер стека */
        NULL,                           /* Параметр */
        TASK_PRIORITY_HIGH,             /* Приоритет */
        &g_task_miner                   /* Хендл задачи */
    );
    if (ret != pdPASS) {
        applog(LOG_ERR, "Не удалось создать задачу майнера!");
    }
    
    /* ----- Сетевая задача ----- */
    /* Обрабатывает TCP/IP стек lwIP */
    ret = xTaskCreate(
        task_network,
        "network",
        TASK_STACK_NETWORK,
        NULL,
        TASK_PRIORITY_NORMAL,
        &g_task_network
    );
    if (ret != pdPASS) {
        applog(LOG_ERR, "Не удалось создать сетевую задачу!");
    }
    
    /* ----- HTTP сервер ----- */
    /* Веб-интерфейс для управления майнером */
    ret = xTaskCreate(
        task_http_server,
        "http",
        TASK_STACK_HTTP,
        NULL,
        TASK_PRIORITY_LOW,
        &g_task_http
    );
    if (ret != pdPASS) {
        applog(LOG_ERR, "Не удалось создать задачу HTTP!");
    }
    
    /* ----- Stratum клиент ----- */
    /* Связь с майнинг-пулом */
    ret = xTaskCreate(
        task_stratum_client,
        "stratum",
        TASK_STACK_NETWORK,
        NULL,
        TASK_PRIORITY_NORMAL,
        &g_task_stratum
    );
    if (ret != pdPASS) {
        applog(LOG_ERR, "Не удалось создать задачу Stratum!");
    }
    
    /* ----- Системный монитор ----- */
    /* Мониторинг температуры, управление вентиляторами, watchdog */
    ret = xTaskCreate(
        task_system_monitor,
        "monitor",
        TASK_STACK_MONITOR,
        NULL,
        TASK_PRIORITY_NORMAL,
        &g_task_monitor
    );
    if (ret != pdPASS) {
        applog(LOG_ERR, "Не удалось создать задачу мониторинга!");
    }
}

/* =============================================================================
 * ЗАДАЧА МАЙНЕРА
 * ============================================================================= */

/**
 * @brief Главная задача майнинга
 * 
 * Выполняет основной цикл майнинга:
 * 1. Получение задания из очереди
 * 2. Отправка задания на ASIC чипы
 * 3. Опрос ASIC на наличие нонсов
 * 4. Отправка найденных шар на пул
 * 
 * @param pvParameters Не используется
 */
static void task_miner(void *pvParameters)
{
    (void)pvParameters;
    
    work_t *current_work = NULL;
    avalon10_nonce_t nonces[16];
    int nonce_count;
    
    applog(LOG_INFO, "Задача майнера запущена на ядре %d", 
           (int)uxTaskGetCpuID());
    
    /* Ждём пока система полностью инициализируется */
    while (!g_system_initialized) {
        vTaskDelay(pdMS_TO_TICKS(100));
    }
    
    /* Главный цикл майнинга */
    for (;;) {
        /* -----------------------------------------------------------------
         * Получение нового задания
         * ----------------------------------------------------------------- */
        
        /* Проверяем очередь заданий (ждём максимум 100мс) */
        if (xQueueReceive(g_work_queue, &current_work, pdMS_TO_TICKS(100)) == pdTRUE) {
            
            if (current_work != NULL) {
                applog(LOG_DEBUG, "Получено новое задание: %s", 
                       current_work->job_id);
                
                /* Преобразуем work_t в формат avalon10_job_t */
                avalon10_job_t job;
                memset(&job, 0, sizeof(job));
                memcpy(job.job_id, current_work->job_id, 4);
                memcpy(job.prev_hash, current_work->data + 4, 32);
                job.version = current_work->version;
                job.nbits = current_work->nbits;
                job.ntime = current_work->ntime;
                memcpy(job.target, current_work->target, 32);
                
                /* Отправляем задание на ASIC */
                if (avalon10_send_job(&job) != 0) {
                    applog(LOG_ERR, "Ошибка отправки задания на ASIC");
                }
                
                mining_active = true;
            }
        }
        
        /* -----------------------------------------------------------------
         * Опрос ASIC на наличие нонсов
         * ----------------------------------------------------------------- */
        
        if (mining_active) {
            /* Опрашиваем ASIC на найденные нонсы */
            nonce_count = avalon10_poll_nonces(nonces, 16);
            
            for (int i = 0; i < nonce_count; i++) {
                applog(LOG_INFO, "Найден нонс: 0x%08X от чипа %d", 
                       nonces[i].nonce, nonces[i].chip_id);
                
                /* Проверяем валидность нонса */
                if (current_work != NULL) {
                    /* Устанавливаем найденный нонс */
                    current_work->nonce = nonces[i].nonce;
                    current_work->nonce2 = nonces[i].nonce2;
                    
                    /* Отправляем шару на пул */
                    if (submit_work(current_work)) {
                        applog(LOG_INFO, "Шара отправлена на пул");
                        total_accepted++;
                    } else {
                        applog(LOG_WARNING, "Шара отклонена");
                        total_rejected++;
                    }
                }
            }
            
            /* Обновляем статистику хешрейта */
            total_hashes += avalon10_scanhash();
        }
        
        /* -----------------------------------------------------------------
         * Небольшая задержка для планировщика
         * ----------------------------------------------------------------- */
        
        vTaskDelay(pdMS_TO_TICKS(AVA10_POLLING_DELAY));
    }
}

/* =============================================================================
 * СЕТЕВАЯ ЗАДАЧА
 * ============================================================================= */

/**
 * @brief Задача обработки сетевого стека
 * 
 * Выполняет периодическую обработку lwIP стека:
 * - Приём/отправка пакетов
 * - Обработка таймаутов
 * - ARP, DHCP и т.д.
 * 
 * @param pvParameters Не используется
 */
static void task_network(void *pvParameters)
{
    (void)pvParameters;
    
    applog(LOG_INFO, "Сетевая задача запущена");
    
    for (;;) {
        /* Обработка lwIP */
        network_poll();
        
        /* Проверка состояния сети */
        if (network_is_connected()) {
            led_set(PIN_LED_NETWORK, true);
        } else {
            /* Мигаем при отсутствии сети */
            static bool blink = false;
            blink = !blink;
            led_set(PIN_LED_NETWORK, blink);
        }
        
        /* Задержка между итерациями */
        vTaskDelay(pdMS_TO_TICKS(10));
    }
}

/* =============================================================================
 * HTTP СЕРВЕР
 * ============================================================================= */

/**
 * @brief Задача HTTP сервера (веб-интерфейс)
 * 
 * Обрабатывает входящие HTTP запросы для:
 * - Просмотра статуса майнера
 * - Изменения настроек
 * - Обновления прошивки
 * 
 * @param pvParameters Не используется
 */
static void task_http_server(void *pvParameters)
{
    (void)pvParameters;
    
    applog(LOG_INFO, "HTTP сервер запущен на порту %d", HTTP_SERVER_PORT);
    
    /* Инициализация HTTP сервера */
    http_server_init(HTTP_SERVER_PORT);
    
    for (;;) {
        /* Обработка HTTP запросов */
        http_server_poll();
        
        vTaskDelay(pdMS_TO_TICKS(50));
    }
}

/* =============================================================================
 * STRATUM КЛИЕНТ
 * ============================================================================= */

/**
 * @brief Задача Stratum клиента
 * 
 * Поддерживает соединение с майнинг-пулом:
 * - Подключение к пулу
 * - Получение заданий
 * - Отправка шар (shares)
 * - Обработка сообщений пула
 * 
 * @param pvParameters Не используется
 */
static void task_stratum_client(void *pvParameters)
{
    (void)pvParameters;
    
    applog(LOG_INFO, "Stratum клиент запущен");
    
    /* Ждём сетевое подключение */
    while (!network_is_connected()) {
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
    
    for (;;) {
        /* Проверяем подключение к пулу */
        if (current_pool == NULL || current_pool->state != POOL_CONNECTED) {
            /* Пытаемся подключиться к пулу */
            for (int i = 0; i < pool_count; i++) {
                if (pools[i].state == POOL_ENABLED) {
                    applog(LOG_INFO, "Подключение к пулу: %s", pools[i].url);
                    
                    if (pool_connect(&pools[i]) == 0) {
                        current_pool = &pools[i];
                        current_pool->state = POOL_CONNECTED;
                        applog(LOG_INFO, "Подключено к пулу: %s", pools[i].url);
                        break;
                    }
                }
            }
            
            if (current_pool == NULL || current_pool->state != POOL_CONNECTED) {
                applog(LOG_WARNING, "Не удалось подключиться к пулу, повтор через 5 сек");
                vTaskDelay(pdMS_TO_TICKS(POOL_RETRY_INTERVAL));
                continue;
            }
        }
        
        /* Обработка Stratum протокола */
        stratum_process(current_pool);
        
        vTaskDelay(pdMS_TO_TICKS(100));
    }
}

/* =============================================================================
 * СИСТЕМНЫЙ МОНИТОР
 * ============================================================================= */

/**
 * @brief Задача системного мониторинга
 * 
 * Выполняет:
 * - Мониторинг температуры
 * - Управление вентиляторами
 * - Защиту от перегрева
 * - Кормление watchdog
 * - Обновление статистики
 * 
 * @param pvParameters Не используется
 */
static void task_system_monitor(void *pvParameters)
{
    (void)pvParameters;
    
    applog(LOG_INFO, "Системный монитор запущен");
    
    uint32_t last_stats_update = 0;
    
    for (;;) {
        /* -----------------------------------------------------------------
         * Кормим watchdog
         * ----------------------------------------------------------------- */
        
#ifdef USE_WATCHDOG
        wdt_feed(WDT_DEVICE_0);
#endif
        
        /* -----------------------------------------------------------------
         * Мониторинг температуры
         * ----------------------------------------------------------------- */
        
        /* Обновляем данные температуры */
        avalon10_update_fan_control();
        
        /* Проверка на перегрев */
        avalon10_device_t status;
        avalon10_get_device_status(&status);
        
        if (status.temp_current >= AVA10_TEMP_OVERHEAT) {
            /* АВАРИЙНОЕ ОТКЛЮЧЕНИЕ! */
            applog(LOG_ERR, "ПЕРЕГРЕВ! Температура: %d°C", status.temp_current);
            
            /* Останавливаем майнинг */
            mining_active = false;
            
            /* Выключаем хеш-платы */
            gpio_set_pin(PIN_HASH_POWER_EN, GPIO_PV_LOW);
            
            /* Вентиляторы на максимум */
            avalon10_set_fan_speed(100);
            
            /* Мигаем красным */
            led_set(PIN_LED_ERROR, true);
            
            /* Ждём остывания */
            while (status.temp_current >= (AVA10_TEMP_TARGET + 10)) {
                vTaskDelay(pdMS_TO_TICKS(1000));
                avalon10_get_device_status(&status);
            }
            
            /* Восстанавливаем работу */
            applog(LOG_INFO, "Температура нормализовалась, продолжаем");
            gpio_set_pin(PIN_HASH_POWER_EN, GPIO_PV_HIGH);
            led_set(PIN_LED_ERROR, false);
            mining_active = true;
        }
        
        /* -----------------------------------------------------------------
         * Smart Speed обновление
         * ----------------------------------------------------------------- */
        
#ifdef USE_SMART_SPEED
        avalon10_update_smart_speed();
#endif
        
        /* -----------------------------------------------------------------
         * Обновление статистики (каждые 5 секунд)
         * ----------------------------------------------------------------- */
        
        uint32_t now = get_ms_time();
        if (now - last_stats_update >= 5000) {
            last_stats_update = now;
            update_statistics();
            
            /* Выводим краткую статистику в лог */
            applog(LOG_INFO, "HR: %.2f TH/s | Accepted: %u | Rejected: %u | Temp: %d°C",
                   (double)get_total_hashrate() / 1e12,
                   total_accepted,
                   total_rejected,
                   status.temp_current);
        }
        
        /* -----------------------------------------------------------------
         * Обновление светодиода статуса
         * ----------------------------------------------------------------- */
        
        if (mining_active && status.has_job) {
            /* Мигаем зелёным при активном майнинге */
            static bool blink = false;
            blink = !blink;
            led_set(PIN_LED_STATUS, blink);
        } else {
            /* Горит постоянно если ожидает работу */
            led_set(PIN_LED_STATUS, true);
        }
        
        /* Задержка 1 секунда */
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}

/* =============================================================================
 * ВСПОМОГАТЕЛЬНЫЕ ФУНКЦИИ
 * ============================================================================= */

/**
 * @brief Вывод приветственного баннера при запуске
 */
static void print_startup_banner(void)
{
    printf("\r\n");
    printf("=====================================================\r\n");
    printf("  _____            __              _____ _____ _____ \r\n");
    printf(" |  _  |_ _ ___   |  |   ___  ___ |  _  |   __|     |\r\n");
    printf(" |     | | | .'|  |  |__| . ||   ||   __|__   | | | |\r\n");
    printf(" |__|__|___|__,|  |_____|___||_|_||__|  |_____|_|_|_|\r\n");
    printf("                                                     \r\n");
    printf("=====================================================\r\n");
    printf("  Модель:    %s\r\n", DEVICE_MODEL);
    printf("  Прошивка:  %s v%s\r\n", FIRMWARE_NAME, FIRMWARE_VERSION);
    printf("  CGMiner:   %s\r\n", CGMINER_VERSION);
    printf("  Собрано:   %s %s\r\n", FIRMWARE_BUILD_DATE, FIRMWARE_BUILD_TIME);
    printf("  Процессор: Kendryte K210 @ %d MHz\r\n", K210_CPU_FREQ / 1000000);
    printf("=====================================================\r\n");
    printf("\r\n");
}

/* =============================================================================
 * ОБРАБОТЧИКИ ОШИБОК FreeRTOS
 * ============================================================================= */

/**
 * @brief Вызывается при переполнении стека задачи
 */
void vApplicationStackOverflowHook(TaskHandle_t xTask, char *pcTaskName)
{
    applog(LOG_ERR, "Переполнение стека в задаче: %s", pcTaskName);
    
    /* Аварийная остановка */
    __disable_irq();
    while(1) {
        led_error_blink();
    }
}

/**
 * @brief Вызывается при нехватке памяти
 */
void vApplicationMallocFailedHook(void)
{
    applog(LOG_ERR, "Нехватка памяти (malloc failed)!");
    
    __disable_irq();
    while(1) {
        led_error_blink();
    }
}

/**
 * @brief Задача простоя (idle task)
 * 
 * Вызывается когда нет активных задач.
 * Здесь можно реализовать режим пониженного энергопотребления.
 */
void vApplicationIdleHook(void)
{
    /* В режиме простоя можно переводить CPU в sleep mode
     * Но для майнера это не актуально - он всегда занят */
}
