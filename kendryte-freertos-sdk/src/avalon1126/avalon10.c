/**
 * =============================================================================
 * @file    avalon10.c
 * @brief   Avalon A1126pro - Драйвер ASIC Avalon10 (реализация)
 * @version 1.0
 * @date    2024
 * =============================================================================
 * 
 * ОПИСАНИЕ:
 * Реализация драйвера ASIC чипов Avalon10 для майнера A1126pro.
 * Управляет модулями, чипами, температурой и хэшрейтом.
 * 
 * ОРИГИНАЛЬНЫЕ ФУНКЦИИ:
 * - FUN_ram_80004e86 @ 0x80004e86 - avalon10_init (инициализация)
 * - FUN_ram_80005184 @ 0x80005184 - avalon10_detect (обнаружение)
 * - FUN_ram_800051ca @ 0x800051ca - avalon10_poll (опрос)
 * - FUN_ram_80004f14 @ 0x80004f14 - avalon10_parse_pkg (парсинг пакетов)
 * 
 * ПРОТОКОЛ СВЯЗИ:
 * Связь с ASIC осуществляется через SPI интерфейс.
 * Формат пакета: [MAGIC:2][TYPE:1][LEN:1][DATA:N][CRC32:4]
 * 
 * =============================================================================
 */

/* ===========================================================================
 * ПОДКЛЮЧАЕМЫЕ ЗАГОЛОВОЧНЫЕ ФАЙЛЫ
 * =========================================================================== */

#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include <FreeRTOS.h>
#include <task.h>

#include "avalon10.h"
#include "cgminer.h"
#include "pool.h"
#include "work.h"
#include "stratum.h"
#include "mock_hardware.h"

/* SDK заголовки для SPI */
#ifndef MOCK_ASIC
#include <spi.h>
#include <fpioa.h>
#include <gpiohs.h>
#include <pwm.h>
#endif

/* ===========================================================================
 * ЛОКАЛЬНЫЕ КОНСТАНТЫ
 * =========================================================================== */

/**
 * @brief Тег для логирования
 */
static const char *TAG = "Avalon10";

/**
 * @brief Строки состояния модуля
 * Используются в avalon10_state_str()
 */
static const char *state_strings[] = {
    "не обнаружен",     /* AVALON10_MODULE_STATE_NONE */
    "инициализация",    /* AVALON10_MODULE_STATE_INIT */
    "готов",            /* AVALON10_MODULE_STATE_READY */
    "майнинг",          /* AVALON10_MODULE_STATE_MINING */
    "ошибка",           /* AVALON10_MODULE_STATE_ERROR */
    "перегрев",         /* AVALON10_MODULE_STATE_OVERHEAT */
    "неизвестно"
};

/* ===========================================================================
 * КОНФИГУРАЦИЯ SPI ДЛЯ ASIC
 * =========================================================================== */

#ifndef MOCK_ASIC
/* SPI устройство для ASIC */
#define ASIC_SPI_DEVICE     SPI_DEVICE_0
#define ASIC_SPI_CLK_RATE   10000000   /* 10 MHz */

/* GPIO для выбора модулей (CS lines) */
#define ASIC_CS_PIN_BASE    20  /* GPIO20-23 для 4 модулей */

/* PWM для вентиляторов */
#define FAN_PWM_DEVICE      PWM_DEVICE_0
#define FAN_PWM_CHANNEL_0   PWM_CHANNEL_0
#define FAN_PWM_CHANNEL_1   PWM_CHANNEL_1
#define FAN_PWM_FREQ        25000   /* 25 kHz */
#endif

/* ===========================================================================
 * ЛОКАЛЬНЫЕ ПЕРЕМЕННЫЕ
 * =========================================================================== */

/**
 * @brief Буфер для отправки пакетов
 */
static uint8_t tx_buffer[AVALON10_PKT_TOTAL_LEN];

/**
 * @brief Буфер для приёма пакетов
 */
static uint8_t rx_buffer[AVALON10_PKT_TOTAL_LEN];

/* ===========================================================================
 * НИЗКОУРОВНЕВЫЕ SPI ФУНКЦИИ
 * =========================================================================== */

#ifndef MOCK_ASIC
/**
 * @brief Флаг инициализации SPI (только для реального железа)
 */
static int spi_initialized = 0;
/**
 * @brief Инициализация SPI для связи с ASIC
 */
static int asic_spi_init(void)
{
    if (spi_initialized) return 0;
    
    /* Настройка FPIOA для SPI */
    fpioa_set_function(ASIC_SPI_CLK_PIN, FUNC_SPI0_SCLK);
    fpioa_set_function(ASIC_SPI_MOSI_PIN, FUNC_SPI0_D0);
    fpioa_set_function(ASIC_SPI_MISO_PIN, FUNC_SPI0_D1);
    
    /* Настройка GPIO для CS */
    for (int i = 0; i < AVALON10_DEFAULT_MODULARS; i++) {
        gpiohs_set_drive_mode(ASIC_CS_PIN_BASE + i, GPIO_DM_OUTPUT);
        gpiohs_set_pin(ASIC_CS_PIN_BASE + i, GPIO_PV_HIGH);  /* CS high (deselect) */
    }
    
    /* Инициализация SPI */
    spi_init(ASIC_SPI_DEVICE, SPI_WORK_MODE_0, SPI_FF_STANDARD, 8, 0);
    spi_set_clk_rate(ASIC_SPI_DEVICE, ASIC_SPI_CLK_RATE);
    
    spi_initialized = 1;
    log_message(LOG_INFO, "%s: SPI инициализирован @ %d MHz", TAG, ASIC_SPI_CLK_RATE / 1000000);
    
    return 0;
}

/**
 * @brief Выбор модуля (CS low)
 */
static void asic_select(int module_id)
{
    if (module_id >= 0 && module_id < AVALON10_DEFAULT_MODULARS) {
        gpiohs_set_pin(ASIC_CS_PIN_BASE + module_id, GPIO_PV_LOW);
    }
}

/**
 * @brief Снятие выбора модуля (CS high)
 */
static void asic_deselect(int module_id)
{
    if (module_id >= 0 && module_id < AVALON10_DEFAULT_MODULARS) {
        gpiohs_set_pin(ASIC_CS_PIN_BASE + module_id, GPIO_PV_HIGH);
    }
}

/**
 * @brief SPI передача данных
 */
static int asic_spi_transfer(int module_id, const uint8_t *tx, uint8_t *rx, size_t len)
{
    asic_select(module_id);
    
    /* Full-duplex SPI transfer */
    spi_send_data_standard(ASIC_SPI_DEVICE, 0, NULL, 0, tx, len);
    spi_receive_data_standard(ASIC_SPI_DEVICE, 0, NULL, 0, rx, len);
    
    asic_deselect(module_id);
    
    return 0;
}
#endif /* !MOCK_ASIC */

/* ===========================================================================
 * ВСПОМОГАТЕЛЬНЫЕ ФУНКЦИИ
 * =========================================================================== */

/**
 * @brief Расчёт CRC32 для пакета
 * 
 * Полином: 0xEDB88320 (стандартный CRC32)
 * Используется для проверки целостности данных при связи с ASIC.
 * 
 * @param data      Указатель на данные
 * @param len       Длина данных
 * @return          Значение CRC32
 */
static uint32_t calc_crc32(const uint8_t *data, size_t len)
{
    uint32_t crc = 0xFFFFFFFF;
    
    for (size_t i = 0; i < len; i++) {
        crc ^= data[i];
        for (int j = 0; j < 8; j++) {
            crc = (crc >> 1) ^ (0xEDB88320 & -(crc & 1));
        }
    }
    
    return ~crc;
}

/**
 * @brief Формирование пакета для отправки на ASIC
 * 
 * Формат пакета:
 * [0-1]   - Magic (0x4156 "AV")
 * [2]     - Тип команды
 * [3]     - Длина данных
 * [4-N]   - Данные
 * [N-N+4] - CRC32
 * 
 * @param pkg       Указатель на структуру пакета
 * @param type      Тип пакета (AVALON10_P_*)
 * @param data      Указатель на данные
 * @param len       Длина данных
 */
static void build_pkg(avalon10_pkg_t *pkg, uint8_t type, const uint8_t *data, uint8_t len)
{
    memset(pkg, 0, sizeof(avalon10_pkg_t));
    
    pkg->magic = AVALON10_PKT_MAGIC;
    pkg->type = type;
    pkg->length = len;
    
    if (data && len > 0) {
        memcpy(pkg->data, data, len);
    }
    
    /* CRC считается по всем данным кроме самого CRC */
    pkg->crc32 = calc_crc32((uint8_t*)pkg, AVALON10_PKT_TOTAL_LEN - 4);
}

/**
 * @brief Отправка пакета на модуль
 * 
 * Использует SPI для связи с ASIC или mock функции для тестирования.
 * 
 * @param module_id ID модуля (0-3)
 * @param pkg       Указатель на пакет
 * @return          0 при успехе, -1 при ошибке
 */
static int send_pkg(int module_id, const avalon10_pkg_t *pkg)
{
    if (module_id < 0 || module_id >= AVALON10_DEFAULT_MODULARS) {
        return -1;
    }
    
    /* Копируем пакет в буфер передачи */
    memcpy(tx_buffer, pkg, AVALON10_PKT_TOTAL_LEN);
    
#ifdef MOCK_ASIC
    /* Используем mock функции для тестирования */
    return mock_asic_send(module_id, tx_buffer, AVALON10_PKT_TOTAL_LEN);
#else
    /* Реальная отправка через SPI */
    if (!spi_initialized) {
        asic_spi_init();
    }
    
    asic_select(module_id);
    spi_send_data_standard(ASIC_SPI_DEVICE, 0, NULL, 0, tx_buffer, AVALON10_PKT_TOTAL_LEN);
    asic_deselect(module_id);
    
    return 0;
#endif
}

/**
 * @brief Приём пакета от модуля
 * 
 * @param module_id ID модуля (0-3)
 * @param pkg       Указатель для записи пакета
 * @param timeout   Таймаут в мс
 * @return          0 при успехе, -1 при ошибке или таймауте
 */
static int recv_pkg(int module_id, avalon10_pkg_t *pkg, int timeout)
{
    if (module_id < 0 || module_id >= AVALON10_DEFAULT_MODULARS) {
        return -1;
    }
    
#ifdef MOCK_ASIC
    /* Используем mock функции для тестирования */
    int ret = mock_asic_recv(module_id, rx_buffer, AVALON10_PKT_TOTAL_LEN, timeout);
    if (ret < 0) {
        return -1;
    }
    memcpy(pkg, rx_buffer, AVALON10_PKT_TOTAL_LEN);
#else
    /* Реальный приём через SPI */
    if (!spi_initialized) {
        asic_spi_init();
    }
    
    TickType_t start = xTaskGetTickCount();
    TickType_t timeout_ticks = pdMS_TO_TICKS(timeout);
    
    while ((xTaskGetTickCount() - start) < timeout_ticks) {
        asic_select(module_id);
        spi_receive_data_standard(ASIC_SPI_DEVICE, 0, NULL, 0, rx_buffer, AVALON10_PKT_TOTAL_LEN);
        asic_deselect(module_id);
        
        /* Проверяем magic */
        avalon10_pkg_t *tmp = (avalon10_pkg_t *)rx_buffer;
        if (tmp->magic == AVALON10_PKT_MAGIC) {
            memcpy(pkg, rx_buffer, AVALON10_PKT_TOTAL_LEN);
            break;
        }
        
        vTaskDelay(pdMS_TO_TICKS(1));
    }
    
    if (pkg->magic != AVALON10_PKT_MAGIC) {
        return -1;
    }
#endif
    
    /* Проверяем магическое число */
    if (pkg->magic != AVALON10_PKT_MAGIC) {
        return -1;
    }
    
    /* Проверяем CRC */
    uint32_t crc = calc_crc32((uint8_t*)pkg, AVALON10_PKT_TOTAL_LEN - 4);
    if (crc != pkg->crc32) {
        log_message(LOG_WARNING, "%s: CRC error module %d", TAG, module_id);
        return -1;
    }
    
    return 0;
}

/* ===========================================================================
 * ФУНКЦИИ ИНИЦИАЛИЗАЦИИ
 * =========================================================================== */

/**
 * @brief Обнаружение и инициализация одного модуля
 * 
 * Отправляет команду DETECT и ждёт ответа.
 * При успехе модуль переходит в состояние INIT.
 * 
 * @param info      Указатель на структуру информации
 * @param module_id ID модуля (0-3)
 * @return          1 если модуль найден, 0 если нет
 */
static int detect_module(avalon10_info_t *info, int module_id)
{
    avalon10_pkg_t pkg;
    avalon10_module_t *module = &info->modules[module_id];
    
    log_message(LOG_DEBUG, "%s: Поиск модуля %d...", TAG, module_id);
    
    /* Отправляем команду обнаружения */
    build_pkg(&pkg, AVALON10_P_DETECT, NULL, 0);
    
    if (send_pkg(module_id, &pkg) < 0) {
        return 0;
    }
    
    /* Ожидаем ответ */
    if (recv_pkg(module_id, &pkg, AVALON10_RESET_TIMEOUT_MS) < 0) {
        module->state = AVALON10_MODULE_STATE_NONE;
        return 0;
    }
    
    /* Модуль найден */
    module->module_id = module_id;
    module->state = AVALON10_MODULE_STATE_INIT;
    module->enabled = 1;
    
    log_message(LOG_INFO, "%s: Модуль %d обнаружен", TAG, module_id);
    
    return 1;
}

/**
 * @brief Инициализация чипов на модуле
 * 
 * Устанавливает начальные частоту и напряжение для каждого чипа.
 * 
 * @param info      Указатель на структуру информации
 * @param module_id ID модуля
 * @return          Количество инициализированных чипов
 */
static int init_chips(avalon10_info_t *info, int module_id)
{
    avalon10_module_t *module = &info->modules[module_id];
    int active_chips = 0;
    
    for (int i = 0; i < AVALON10_DEFAULT_MINER_CNT; i++) {
        avalon10_chip_t *chip = &module->chips[i];
        
        chip->chip_id = i;
        chip->enabled = 1;
        chip->freq = info->default_freq[0];
        chip->nonces = 0;
        chip->hw_errors = 0;
        chip->error_count = 0;
        
        active_chips++;
    }
    
    module->active_chips = active_chips;
    module->failed_chips = AVALON10_DEFAULT_MINER_CNT - active_chips;
    
    log_message(LOG_INFO, "%s: Модуль %d: %d чипов активно", 
                TAG, module_id, active_chips);
    
    return active_chips;
}

/**
 * @brief Инициализация драйвера Avalon10
 * 
 * Последовательность инициализации:
 * 1. Очистка структуры
 * 2. Установка значений по умолчанию
 * 3. Обнаружение модулей
 * 4. Инициализация чипов
 * 5. Применение настроек
 * 
 * Оригинальная функция: FUN_ram_80004e86
 * 
 * @param info      Указатель на структуру информации
 * @return          0 при успехе, -1 при ошибке
 */
int avalon10_init(avalon10_info_t *info)
{
    int modules_found = 0;
    
    log_message(LOG_INFO, "%s: Инициализация...", TAG);
    
    /* Очистка структуры */
    memset(info, 0, sizeof(avalon10_info_t));
    
    /* Установка значений по умолчанию */
    info->default_freq[0] = AVALON10_DEFAULT_FREQ_MIN;
    info->default_freq[1] = AVALON10_DEFAULT_FREQ_MAX;
    info->default_voltage = AVALON10_DEFAULT_VOLTAGE;
    info->fan_min = AVALON10_DEFAULT_FAN_MIN;
    info->fan_max = AVALON10_DEFAULT_FAN_MAX;
    info->temp_target = AVALON10_DEFAULT_TEMP_TARGET;
    info->temp_overheat = AVALON10_DEFAULT_TEMP_OVERHEAT;
    info->temp_cutoff = AVALON10_DEFAULT_TEMP_CUTOFF;
    
    /* Обнаружение модулей */
    for (int i = 0; i < AVALON10_DEFAULT_MODULARS; i++) {
        if (detect_module(info, i)) {
            modules_found++;
            
            /* Инициализация чипов модуля */
            init_chips(info, i);
            
            /* Установка частоты */
            for (int j = 0; j < AVALON10_FREQ_SLOTS; j++) {
                info->modules[i].freq[j] = info->default_freq[0];
            }
            
            /* Установка напряжения */
            info->modules[i].voltage = info->default_voltage;
            
            /* Модуль готов */
            info->modules[i].state = AVALON10_MODULE_STATE_READY;
        }
    }
    
    info->module_count = modules_found;
    info->active_modules = modules_found;
    info->initialized = 1;
    
    if (modules_found == 0) {
        log_message(LOG_ERR, "%s: Модули не найдены!", TAG);
        return -1;
    }
    
    log_message(LOG_INFO, "%s: Найдено %d модулей, %d чипов", 
                TAG, modules_found, 
                modules_found * AVALON10_DEFAULT_MINER_CNT);
    
    return 0;
}

/**
 * @brief Деинициализация драйвера
 * 
 * @param info      Указатель на структуру информации
 */
void avalon10_exit(avalon10_info_t *info)
{
    log_message(LOG_INFO, "%s: Деинициализация...", TAG);
    
    /* Останавливаем майнинг на всех модулях */
    for (int i = 0; i < AVALON10_DEFAULT_MODULARS; i++) {
        if (info->modules[i].state != AVALON10_MODULE_STATE_NONE) {
            info->modules[i].state = AVALON10_MODULE_STATE_READY;
        }
    }
    
    info->mining_enabled = 0;
    info->initialized = 0;
}

/**
 * @brief Сброс модуля
 * 
 * @param info      Указатель на структуру информации
 * @param module_id ID модуля (-1 = все)
 * @return          0 при успехе
 */
int avalon10_reset(avalon10_info_t *info, int module_id)
{
    avalon10_pkg_t pkg;
    int start, end;
    
    if (module_id < 0) {
        start = 0;
        end = AVALON10_DEFAULT_MODULARS;
    } else {
        start = module_id;
        end = module_id + 1;
    }
    
    build_pkg(&pkg, AVALON10_P_RESET, NULL, 0);
    
    for (int i = start; i < end; i++) {
        if (info->modules[i].state != AVALON10_MODULE_STATE_NONE) {
            log_message(LOG_INFO, "%s: Сброс модуля %d", TAG, i);
            send_pkg(i, &pkg);
            info->modules[i].state = AVALON10_MODULE_STATE_INIT;
        }
    }
    
    vTaskDelay(pdMS_TO_TICKS(1000));
    
    return 0;
}

/* ===========================================================================
 * ФУНКЦИИ ОПРОСА И СТАТИСТИКИ
 * =========================================================================== */

/**
 * @brief Опрос одного модуля
 * 
 * Запрашивает статус и nonce от модуля.
 * 
 * @param info      Указатель на структуру информации
 * @param module_id ID модуля
 * @return          Количество найденных nonce
 */
static int poll_module(avalon10_info_t *info, int module_id)
{
    avalon10_pkg_t pkg;
    avalon10_module_t *module = &info->modules[module_id];
    int nonces = 0;
    
    /* Запрос статуса */
    build_pkg(&pkg, AVALON10_P_STATUS, NULL, 0);
    
    if (send_pkg(module_id, &pkg) < 0) {
        module->poll_errors++;
        return 0;
    }
    
    if (recv_pkg(module_id, &pkg, AVALON10_POLL_TIMEOUT_MS) < 0) {
        module->poll_errors++;
        return 0;
    }
    
    module->poll_errors = 0;
    
    /* Парсинг ответа - обновление температуры */
    if (pkg.type == AVALON10_P_STATUS && pkg.length >= 8) {
        module->temp_in = (int16_t)((pkg.data[0] << 8) | pkg.data[1]);
        module->temp_out = (int16_t)((pkg.data[2] << 8) | pkg.data[3]);
    }
    
    /* Проверяем nonce */
    build_pkg(&pkg, AVALON10_P_NONCE, NULL, 0);
    
    if (send_pkg(module_id, &pkg) == 0 &&
        recv_pkg(module_id, &pkg, AVALON10_NONCE_TIMEOUT_MS) == 0) {
        
        if (pkg.type == AVALON10_P_NONCE && pkg.length >= 4) {
            /* Найден nonce от ASIC */
            uint32_t nonce = (pkg.data[0] << 24) | (pkg.data[1] << 16) |
                            (pkg.data[2] << 8) | pkg.data[3];
            
            /* Получаем текущую работу через stratum модуль */
            work_t *current_work = stratum_get_current_work();
            
            if (current_work && g_work_mutex) {
                if (xSemaphoreTake(g_work_mutex, pdMS_TO_TICKS(100)) == pdTRUE) {
                    /* Проверяем nonce через SHA256d */
                    if (work_check_nonce(current_work, nonce)) {
                        /* Nonce валиден - отправляем на пул */
                        current_work->nonce = nonce;
                        
                        /* Вызываем отправку через stratum */
                        if (g_current_pool && g_current_pool->stratum_active) {
                            if (stratum_submit_nonce(g_current_pool, current_work) == 0) {
                                module->accepted++;
                                log_message(LOG_INFO, "%s: Nonce 0x%08X отправлен на пул", 
                                           TAG, nonce);
                            } else {
                                log_message(LOG_WARNING, "%s: Ошибка отправки nonce", TAG);
                            }
                        }
                    } else {
                        /* Hardware error - nonce не прошёл проверку */
                        module->hw_errors++;
                        info->total_hw_errors++;
                        log_message(LOG_WARNING, "%s: HW Error: nonce 0x%08X не валиден", 
                                   TAG, nonce);
                    }
                    xSemaphoreGive(g_work_mutex);
                }
            }
            nonces++;
        }
    }
    
    module->last_poll = xTaskGetTickCount();
    
    return nonces;
}

/**
 * @brief Опрос всех модулей
 * 
 * Вызывается в основном цикле майнинга.
 * Оригинальная функция: FUN_ram_800051ca
 * 
 * @param info      Указатель на структуру информации
 * @return          Общее количество найденных nonce
 */
int avalon10_poll(avalon10_info_t *info)
{
    int total_nonces = 0;
    
    if (!info->initialized || !info->mining_enabled) {
        return 0;
    }
    
    for (int i = 0; i < AVALON10_DEFAULT_MODULARS; i++) {
        avalon10_module_t *module = &info->modules[i];
        
        if (module->state == AVALON10_MODULE_STATE_MINING) {
            total_nonces += poll_module(info, i);
        }
    }
    
    /* Обновляем общую статистику */
    info->total_accepted += total_nonces;
    info->uptime = xTaskGetTickCount() / 1000;
    info->last_update = xTaskGetTickCount();
    
    return total_nonces;
}

/**
 * @brief Чтение температуры со всех датчиков
 * 
 * @param info      Указатель на структуру информации
 * @return          Максимальная температура (°C × 10)
 */
int avalon10_read_temperature(avalon10_info_t *info)
{
    int16_t max_temp = 0;
    
    for (int i = 0; i < AVALON10_DEFAULT_MODULARS; i++) {
        avalon10_module_t *module = &info->modules[i];
        
        if (module->state != AVALON10_MODULE_STATE_NONE) {
            /* Обновляем максимальную температуру */
            if (module->temp_in > module->temp_max) {
                module->temp_max = module->temp_in;
            }
            if (module->temp_out > module->temp_max) {
                module->temp_max = module->temp_out;
            }
            
            /* Средняя температура */
            module->temp_avg = (module->temp_in + module->temp_out) / 2;
            
            if (module->temp_max > max_temp) {
                max_temp = module->temp_max;
            }
        }
    }
    
    return max_temp;
}

/**
 * @brief Проверка перегрева
 * 
 * При превышении temp_overheat - снижаем частоту.
 * При превышении temp_cutoff - останавливаем майнинг.
 * 
 * @param info      Указатель на структуру информации
 */
void avalon10_check_overheat(avalon10_info_t *info)
{
    for (int i = 0; i < AVALON10_DEFAULT_MODULARS; i++) {
        avalon10_module_t *module = &info->modules[i];
        
        if (module->state == AVALON10_MODULE_STATE_NONE) {
            continue;
        }
        
        int temp = module->temp_max / 10;  /* Преобразуем из °C×10 в °C */
        
        if (temp >= info->temp_cutoff) {
            /* Критический перегрев - останавливаем */
            log_message(LOG_ERR, "%s: Модуль %d: КРИТИЧЕСКИЙ ПЕРЕГРЕВ %d°C!", 
                       TAG, i, temp);
            module->state = AVALON10_MODULE_STATE_OVERHEAT;
            info->overheat = 1;
        }
        else if (temp >= info->temp_overheat) {
            /* Перегрев - снижаем частоту */
            log_message(LOG_WARNING, "%s: Модуль %d: перегрев %d°C, снижение частоты", 
                       TAG, i, temp);
            
            for (int j = 0; j < AVALON10_FREQ_SLOTS; j++) {
                if (module->freq[j] > AVALON10_DEFAULT_FREQ_MIN) {
                    module->freq[j] -= AVALON10_FREQ_STEP;
                }
            }
        }
        else if (temp < info->temp_target && module->state == AVALON10_MODULE_STATE_OVERHEAT) {
            /* Температура в норме - возобновляем */
            log_message(LOG_INFO, "%s: Модуль %d: температура в норме %d°C", 
                       TAG, i, temp);
            module->state = AVALON10_MODULE_STATE_MINING;
            info->overheat = 0;
        }
    }
}

/**
 * @brief Расчёт и обновление хэшрейта
 * 
 * Хэшрейт рассчитывается по количеству nonce за единицу времени.
 * Формула: hashrate = nonces × difficulty × 2^32 / time
 * 
 * @param info      Указатель на структуру информации
 */
void avalon10_update_hashrate(avalon10_info_t *info)
{
    uint64_t total = 0;
    
    for (int i = 0; i < AVALON10_DEFAULT_MODULARS; i++) {
        avalon10_module_t *module = &info->modules[i];
        
        if (module->state == AVALON10_MODULE_STATE_MINING) {
            /* Приблизительный хэшрейт на основе активных чипов и частоты */
            /* Каждое ядро выдаёт около 10 GH/s на 500 MHz */
            uint64_t cores = module->active_chips * AVALON10_DEFAULT_ASIC_CORE;
            uint64_t freq = module->freq[0];
            module->hashrate = (cores * freq * 20000000ULL);  /* H/s */
            
            total += module->hashrate;
        }
    }
    
    info->total_hashrate = total;
}

/* ===========================================================================
 * УПРАВЛЕНИЕ ВЕНТИЛЯТОРАМИ
 * =========================================================================== */

/**
 * @brief Регулировка скорости вентиляторов
 * 
 * Простой пропорциональный регулятор.
 * При температуре ниже целевой - минимальные обороты.
 * При температуре выше целевой - увеличиваем пропорционально.
 * 
 * @param info      Указатель на структуру информации
 */
void avalon10_adjust_fan(avalon10_info_t *info)
{
    int max_temp = 0;
    
    /* Находим максимальную температуру */
    for (int i = 0; i < AVALON10_DEFAULT_MODULARS; i++) {
        if (info->modules[i].temp_max > max_temp) {
            max_temp = info->modules[i].temp_max;
        }
    }
    
    int temp = max_temp / 10;  /* °C */
    int speed;
    
    if (temp <= info->temp_target - 10) {
        speed = info->fan_min;
    }
    else if (temp >= info->temp_overheat) {
        speed = info->fan_max;
    }
    else {
        /* Линейная интерполяция */
        int range = info->temp_overheat - (info->temp_target - 10);
        int delta = temp - (info->temp_target - 10);
        speed = info->fan_min + 
                ((info->fan_max - info->fan_min) * delta) / range;
    }
    
    /* Применяем ко всем вентиляторам */
    for (int i = 0; i < AVALON10_FAN_COUNT; i++) {
        info->fan_pwm[i] = speed;
        avalon10_set_fan_speed(info, i, speed);
    }
}

/**
 * @brief Установка скорости вентилятора
 * 
 * @param info      Указатель на структуру информации
 * @param fan_id    ID вентилятора (0 или 1)
 * @param speed     Скорость (0-100%)
 * @return          0 при успехе
 */
int avalon10_set_fan_speed(avalon10_info_t *info, int fan_id, int speed)
{
    if (fan_id < 0 || fan_id >= AVALON10_FAN_COUNT) {
        return -1;
    }
    
    if (speed < 0) speed = 0;
    if (speed > 100) speed = 100;
    
#ifdef MOCK_ASIC
    /* В режиме эмуляции просто сохраняем значение */
    log_message(LOG_DEBUG, "%s: Вентилятор %d: %d%%", TAG, fan_id, speed);
#else
    /* Реальная установка PWM */
    /* Duty cycle = speed (0-100%) -> PWM (0-1.0) */
    double duty = speed / 100.0;
    
    if (fan_id == 0) {
        pwm_set_frequency(FAN_PWM_DEVICE, FAN_PWM_CHANNEL_0, FAN_PWM_FREQ, duty);
        pwm_set_enable(FAN_PWM_DEVICE, FAN_PWM_CHANNEL_0, 1);
    } else {
        pwm_set_frequency(FAN_PWM_DEVICE, FAN_PWM_CHANNEL_1, FAN_PWM_FREQ, duty);
        pwm_set_enable(FAN_PWM_DEVICE, FAN_PWM_CHANNEL_1, 1);
    }
#endif
    
    info->fan_pwm[fan_id] = speed;
    
    return 0;
}

/* ===========================================================================
 * УПРАВЛЕНИЕ ЧАСТОТОЙ И НАПРЯЖЕНИЕМ
 * =========================================================================== */

/**
 * @brief Установка частоты модуля
 * 
 * @param info      Указатель на структуру информации
 * @param module_id ID модуля (-1 = все)
 * @param freq      Частота (MHz)
 * @return          0 при успехе
 */
int avalon10_set_freq(avalon10_info_t *info, int module_id, int freq)
{
    avalon10_pkg_t pkg;
    uint8_t data[4];
    int start, end;
    
    /* Проверка диапазона */
    if (freq < AVALON10_DEFAULT_FREQ_MIN) freq = AVALON10_DEFAULT_FREQ_MIN;
    if (freq > AVALON10_DEFAULT_FREQ_MAX) freq = AVALON10_DEFAULT_FREQ_MAX;
    
    if (module_id < 0) {
        start = 0;
        end = AVALON10_DEFAULT_MODULARS;
    } else {
        start = module_id;
        end = module_id + 1;
    }
    
    data[0] = (freq >> 8) & 0xFF;
    data[1] = freq & 0xFF;
    data[2] = 0;
    data[3] = 0;
    
    build_pkg(&pkg, AVALON10_P_SET_FREQ, data, 4);
    
    for (int i = start; i < end; i++) {
        if (info->modules[i].state != AVALON10_MODULE_STATE_NONE) {
            send_pkg(i, &pkg);
            
            for (int j = 0; j < AVALON10_FREQ_SLOTS; j++) {
                info->modules[i].freq[j] = freq;
            }
            
            log_message(LOG_INFO, "%s: Модуль %d: частота %d MHz", TAG, i, freq);
        }
    }
    
    return 0;
}

/**
 * @brief Установка напряжения модуля
 * 
 * @param info      Указатель на структуру информации
 * @param module_id ID модуля (-1 = все)
 * @param voltage   Напряжение (mV)
 * @return          0 при успехе
 */
int avalon10_set_voltage(avalon10_info_t *info, int module_id, int voltage)
{
    avalon10_pkg_t pkg;
    uint8_t data[4];
    int start, end;
    
    /* Проверка диапазона */
    if (voltage < AVALON10_VOLTAGE_MIN) voltage = AVALON10_VOLTAGE_MIN;
    if (voltage > AVALON10_VOLTAGE_MAX) voltage = AVALON10_VOLTAGE_MAX;
    
    if (module_id < 0) {
        start = 0;
        end = AVALON10_DEFAULT_MODULARS;
    } else {
        start = module_id;
        end = module_id + 1;
    }
    
    data[0] = (voltage >> 8) & 0xFF;
    data[1] = voltage & 0xFF;
    data[2] = 0;
    data[3] = 0;
    
    build_pkg(&pkg, AVALON10_P_SET_VOLTAGE, data, 4);
    
    for (int i = start; i < end; i++) {
        if (info->modules[i].state != AVALON10_MODULE_STATE_NONE) {
            send_pkg(i, &pkg);
            info->modules[i].voltage = voltage;
            
            log_message(LOG_INFO, "%s: Модуль %d: напряжение %d mV", TAG, i, voltage);
        }
    }
    
    return 0;
}

/* ===========================================================================
 * РАБОТА С ЗАДАНИЯМИ
 * =========================================================================== */

/**
 * @brief Отправка работы на все модули
 * 
 * Формирует пакет с заголовком блока и отправляет на все активные модули.
 * 
 * @param info      Указатель на структуру информации
 * @param work      Указатель на рабочее задание
 * @return          0 при успехе
 */
int avalon10_send_work(avalon10_info_t *info, work_t *work)
{
    avalon10_pkg_t pkg;
    
    if (!work) {
        return -1;
    }
    
    /* Формируем пакет с заголовком блока */
    build_pkg(&pkg, AVALON10_P_WORK, work->header, 80);
    
    for (int i = 0; i < AVALON10_DEFAULT_MODULARS; i++) {
        if (info->modules[i].state == AVALON10_MODULE_STATE_MINING) {
            send_pkg(i, &pkg);
        }
    }
    
    info->current_work = work;
    info->work_id++;
    
    return 0;
}

/**
 * @brief Проверка nonce
 * 
 * Вычисляет хэш блока с найденным nonce и проверяет,
 * удовлетворяет ли он целевой сложности.
 * 
 * @param work      Указатель на рабочее задание
 * @param nonce     Найденный nonce
 * @return          1 если nonce валиден, 0 иначе
 */
int avalon10_check_nonce(work_t *work, uint32_t nonce)
{
    if (!work) return 0;
    
    uint8_t header[80];
    uint8_t hash[32];
    
    /* 1. Копируем заголовок блока */
    memcpy(header, work->header, 80);
    
    /* 2. Вставляем nonce в позицию 76-79 (little-endian) */
    header[76] = nonce & 0xFF;
    header[77] = (nonce >> 8) & 0xFF;
    header[78] = (nonce >> 16) & 0xFF;
    header[79] = (nonce >> 24) & 0xFF;
    
    /* 3. Вычисляем SHA256d (двойной SHA256) */
    extern void mock_sha256d(const uint8_t *data, size_t len, uint8_t *hash);
    mock_sha256d(header, 80, hash);
    
    /* 4. Сравниваем с target (hash должен быть меньше target)
     * Bitcoin хэши сравниваются как big-endian числа,
     * поэтому сравниваем байты с конца (старшие байты первые) */
    for (int i = 31; i >= 0; i--) {
        if (hash[i] < work->target[i]) {
            /* hash < target - nonce валиден! */
            return 1;
        } else if (hash[i] > work->target[i]) {
            /* hash > target - nonce не подходит */
            return 0;
        }
        /* hash[i] == target[i] - продолжаем сравнение */
    }
    
    /* hash == target - валиден (крайне редкий случай) */
    return 1;
}

/* ===========================================================================
 * ОТЛАДОЧНЫЕ ФУНКЦИИ
 * =========================================================================== */

/**
 * @brief Вывод статистики модуля в лог
 * 
 * @param info      Указатель на структуру информации
 * @param module_id ID модуля
 */
void avalon10_print_stats(avalon10_info_t *info, int module_id)
{
    avalon10_module_t *module = &info->modules[module_id];
    
    log_message(LOG_INFO, "%s: === Модуль %d ===", TAG, module_id);
    log_message(LOG_INFO, "  Состояние: %s", avalon10_state_str(module->state));
    log_message(LOG_INFO, "  Чипов: %d активных, %d сбойных", 
               module->active_chips, module->failed_chips);
    log_message(LOG_INFO, "  Частота: %d MHz", module->freq[0]);
    log_message(LOG_INFO, "  Напряжение: %d mV", module->voltage);
    log_message(LOG_INFO, "  Температура: вход %.1f°C, выход %.1f°C", 
               module->temp_in / 10.0, module->temp_out / 10.0);
    log_message(LOG_INFO, "  Принято: %lu, Отклонено: %lu, HW ошибки: %lu",
               (unsigned long)module->accepted, 
               (unsigned long)module->rejected,
               (unsigned long)module->hw_errors);
}

/**
 * @brief Получение строки состояния модуля
 * 
 * @param state     Код состояния
 * @return          Строка состояния на русском
 */
const char *avalon10_state_str(int state)
{
    if (state < 0 || state > AVALON10_MODULE_STATE_OVERHEAT) {
        return state_strings[6];  /* "неизвестно" */
    }
    return state_strings[state];
}

/* ===========================================================================
 * КОНЕЦ ФАЙЛА avalon10.c
 * =========================================================================== */
