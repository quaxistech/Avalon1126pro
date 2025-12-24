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
 * @brief Буфер для отправки пакетов (40 байт = размер avalon10_pkg_t)
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
 * @brief Расчёт CRC16-CCITT для пакета
 * 
 * Полином: 0x1021 (CRC-CCITT)
 * Используется протоколом Avalon для проверки целостности данных.
 * Таблица и алгоритм соответствуют CGMiner crc16.c
 * 
 * @param data      Указатель на данные
 * @param len       Длина данных
 * @return          Значение CRC16
 */
static const uint16_t crc16_table[256] = {
    0x0000, 0x1021, 0x2042, 0x3063, 0x4084, 0x50A5, 0x60C6, 0x70E7,
    0x8108, 0x9129, 0xA14A, 0xB16B, 0xC18C, 0xD1AD, 0xE1CE, 0xF1EF,
    0x1231, 0x0210, 0x3273, 0x2252, 0x52B5, 0x4294, 0x72F7, 0x62D6,
    0x9339, 0x8318, 0xB37B, 0xA35A, 0xD3BD, 0xC39C, 0xF3FF, 0xE3DE,
    0x2462, 0x3443, 0x0420, 0x1401, 0x64E6, 0x74C7, 0x44A4, 0x5485,
    0xA56A, 0xB54B, 0x8528, 0x9509, 0xE5EE, 0xF5CF, 0xC5AC, 0xD58D,
    0x3653, 0x2672, 0x1611, 0x0630, 0x76D7, 0x66F6, 0x5695, 0x46B4,
    0xB75B, 0xA77A, 0x9719, 0x8738, 0xF7DF, 0xE7FE, 0xD79D, 0xC7BC,
    0x48C4, 0x58E5, 0x6886, 0x78A7, 0x0840, 0x1861, 0x2802, 0x3823,
    0xC9CC, 0xD9ED, 0xE98E, 0xF9AF, 0x8948, 0x9969, 0xA90A, 0xB92B,
    0x5AF5, 0x4AD4, 0x7AB7, 0x6A96, 0x1A71, 0x0A50, 0x3A33, 0x2A12,
    0xDBFD, 0xCBDC, 0xFBBF, 0xEB9E, 0x9B79, 0x8B58, 0xBB3B, 0xAB1A,
    0x6CA6, 0x7C87, 0x4CE4, 0x5CC5, 0x2C22, 0x3C03, 0x0C60, 0x1C41,
    0xEDAE, 0xFD8F, 0xCDEC, 0xDDCD, 0xAD2A, 0xBD0B, 0x8D68, 0x9D49,
    0x7E97, 0x6EB6, 0x5ED5, 0x4EF4, 0x3E13, 0x2E32, 0x1E51, 0x0E70,
    0xFF9F, 0xEFBE, 0xDFDD, 0xCFFC, 0xBF1B, 0xAF3A, 0x9F59, 0x8F78,
    0x9188, 0x81A9, 0xB1CA, 0xA1EB, 0xD10C, 0xC12D, 0xF14E, 0xE16F,
    0x1080, 0x00A1, 0x30C2, 0x20E3, 0x5004, 0x4025, 0x7046, 0x6067,
    0x83B9, 0x9398, 0xA3FB, 0xB3DA, 0xC33D, 0xD31C, 0xE37F, 0xF35E,
    0x02B1, 0x1290, 0x22F3, 0x32D2, 0x4235, 0x5214, 0x6277, 0x7256,
    0xB5EA, 0xA5CB, 0x95A8, 0x8589, 0xF56E, 0xE54F, 0xD52C, 0xC50D,
    0x34E2, 0x24C3, 0x14A0, 0x0481, 0x7466, 0x6447, 0x5424, 0x4405,
    0xA7DB, 0xB7FA, 0x8799, 0x97B8, 0xE75F, 0xF77E, 0xC71D, 0xD73C,
    0x26D3, 0x36F2, 0x0691, 0x16B0, 0x6657, 0x7676, 0x4615, 0x5634,
    0xD94C, 0xC96D, 0xF90E, 0xE92F, 0x99C8, 0x89E9, 0xB98A, 0xA9AB,
    0x5844, 0x4865, 0x7806, 0x6827, 0x18C0, 0x08E1, 0x3882, 0x28A3,
    0xCB7D, 0xDB5C, 0xEB3F, 0xFB1E, 0x8BF9, 0x9BD8, 0xABBB, 0xBB9A,
    0x4A75, 0x5A54, 0x6A37, 0x7A16, 0x0AF1, 0x1AD0, 0x2AB3, 0x3A92,
    0xFD2E, 0xED0F, 0xDD6C, 0xCD4D, 0xBDAA, 0xAD8B, 0x9DE8, 0x8DC9,
    0x7C26, 0x6C07, 0x5C64, 0x4C45, 0x3CA2, 0x2C83, 0x1CE0, 0x0CC1,
    0xEF1F, 0xFF3E, 0xCF5D, 0xDF7C, 0xAF9B, 0xBFBA, 0x8FD9, 0x9FF8,
    0x6E17, 0x7E36, 0x4E55, 0x5E74, 0x2E93, 0x3EB2, 0x0ED1, 0x1EF0
};

static uint16_t calc_crc16(const uint8_t *data, size_t len)
{
    uint16_t crc = 0;
    
    while (len-- > 0) {
        crc = crc16_table[((crc >> 8) ^ (*data++)) & 0xFF] ^ (crc << 8);
    }
    
    return crc;
}

/**
 * @brief Формирование пакета для отправки на ASIC (протокол Avalon4+)
 * 
 * Формат пакета:
 * [0]     - Head1 ('C')
 * [1]     - Head2 ('N')
 * [2]     - Type - тип команды
 * [3]     - Opt - опция
 * [4]     - Idx - индекс пакета
 * [5]     - Cnt - количество пакетов
 * [6-37]  - Data[32]
 * [38-39] - CRC16-CCITT по data[32]
 * 
 * @param pkg       Указатель на структуру пакета
 * @param type      Тип пакета (AVALON10_P_*)
 * @param idx       Индекс пакета
 * @param cnt       Количество пакетов
 */
static void build_pkg(avalon10_pkg_t *pkg, uint8_t type, uint8_t idx, uint8_t cnt)
{
    uint16_t crc;
    
    pkg->head[0] = AVALON10_PKT_HEAD1;
    pkg->head[1] = AVALON10_PKT_HEAD2;
    pkg->type = type;
    pkg->opt = 0;
    pkg->idx = idx;
    pkg->cnt = cnt;
    
    /* CRC16 вычисляется только по полю data[32] */
    crc = calc_crc16(pkg->data, AVALON10_PKG_DATA_LEN);
    
    pkg->crc[0] = (crc >> 8) & 0xff;
    pkg->crc[1] = crc & 0xff;
}

/**
 * @brief Отправка пакета на модуль
 * 
 * Использует SPI для связи с ASIC или mock функции для тестирования.
 * Размер пакета фиксирован: 40 байт (AVALON10_PKG_SIZE)
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
    memcpy(tx_buffer, pkg, AVALON10_PKG_SIZE);
    
#if MOCK_ASIC
    /* Используем mock функции для тестирования */
    return mock_asic_send(module_id, tx_buffer, AVALON10_PKG_SIZE);
#else
    /* Реальная отправка через SPI */
    /* TODO: Реализовать для реального железа */
    (void)module_id;
    log_message(LOG_WARNING, "%s: SPI send not implemented", TAG);
    return -1;
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
    uint16_t crc_expected, crc_actual;
    
    if (module_id < 0 || module_id >= AVALON10_DEFAULT_MODULARS) {
        return -1;
    }
    
#if MOCK_ASIC
    /* Используем mock функции для тестирования */
    int ret = mock_asic_recv(module_id, rx_buffer, AVALON10_PKG_SIZE, timeout);
    if (ret < 0) {
        return -1;
    }
    memcpy(pkg, rx_buffer, AVALON10_PKG_SIZE);
#else
    /* Реальный приём через SPI */
    /* TODO: Реализовать для реального железа */
    (void)timeout;
    log_message(LOG_WARNING, "%s: SPI recv not implemented", TAG);
    return -1;
#endif
    
    /* Проверяем заголовок пакета */
    if (pkg->head[0] != AVALON10_PKT_HEAD1 || pkg->head[1] != AVALON10_PKT_HEAD2) {
        return -1;
    }
    
    /* Проверяем CRC16 */
    crc_expected = calc_crc16(pkg->data, AVALON10_PKG_DATA_LEN);
    crc_actual = ((uint16_t)pkg->crc[0] << 8) | pkg->crc[1];
    
    if (crc_expected != crc_actual) {
        log_message(LOG_WARNING, "%s: CRC16 error module %d (expected %04x, got %04x)", 
                    TAG, module_id, crc_expected, crc_actual);
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
    
    /* Подготавливаем пакет обнаружения */
    memset(&pkg, 0, sizeof(pkg));
    build_pkg(&pkg, AVALON10_P_DETECT, 1, 1);
    
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
    
    memset(&pkg, 0, sizeof(pkg));
    build_pkg(&pkg, AVALON10_P_RESET, 1, 1);
    
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
    memset(&pkg, 0, sizeof(pkg));
    build_pkg(&pkg, AVALON10_P_STATUS, 1, 1);
    
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
    if (pkg.type == AVALON10_P_STATUS) {
        module->temp_in = (int16_t)((pkg.data[0] << 8) | pkg.data[1]);
        module->temp_out = (int16_t)((pkg.data[2] << 8) | pkg.data[3]);
    }
    
    /* Проверяем nonce */
    memset(&pkg, 0, sizeof(pkg));
    build_pkg(&pkg, AVALON10_P_NONCE, 1, 1);
    
    if (send_pkg(module_id, &pkg) == 0 &&
        recv_pkg(module_id, &pkg, AVALON10_NONCE_TIMEOUT_MS) == 0) {
        
        if (pkg.type == AVALON10_P_NONCE) {
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
    
#if MOCK_ASIC
    /* В режиме эмуляции просто сохраняем значение */
    log_message(LOG_DEBUG, "%s: Вентилятор %d: %d%%", TAG, fan_id, speed);
#else
    /* Реальная установка PWM */
    /* TODO: Реализовать для реального железа */
    /* Duty cycle = speed (0-100%) -> PWM (0-1.0) */
    /* double duty = speed / 100.0; */
    log_message(LOG_DEBUG, "%s: Вентилятор %d: %d%% (PWM not implemented)", TAG, fan_id, speed);
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
    int start, end;
    uint32_t tmp;
    
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
    
    /* Подготавливаем пакет */
    memset(&pkg, 0, sizeof(pkg));
    tmp = freq;  /* Big-endian формат */
    pkg.data[0] = (tmp >> 24) & 0xFF;
    pkg.data[1] = (tmp >> 16) & 0xFF;
    pkg.data[2] = (tmp >> 8) & 0xFF;
    pkg.data[3] = tmp & 0xFF;
    
    build_pkg(&pkg, AVALON10_P_SET_FREQ, 1, 1);
    
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
    int start, end;
    uint32_t tmp;
    
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
    
    /* Подготавливаем пакет */
    memset(&pkg, 0, sizeof(pkg));
    tmp = voltage;  /* Big-endian формат */
    pkg.data[0] = (tmp >> 24) & 0xFF;
    pkg.data[1] = (tmp >> 16) & 0xFF;
    pkg.data[2] = (tmp >> 8) & 0xFF;
    pkg.data[3] = tmp & 0xFF;
    
    build_pkg(&pkg, AVALON10_P_SET_VOLTAGE, 1, 1);
    
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
 * Формирует пакеты с заголовком блока и отправляет на все активные модули.
 * Так как заголовок блока = 80 байт, а data в пакете = 32 байта,
 * заголовок разбивается на 3 пакета (32 + 32 + 16).
 * 
 * @param info      Указатель на структуру информации
 * @param work      Указатель на рабочее задание
 * @return          0 при успехе
 */
int avalon10_send_work(avalon10_info_t *info, work_t *work)
{
    avalon10_pkg_t pkg;
    int pkt_count = 3;  /* 80 байт / 32 = 3 пакета (32+32+16) */
    
    if (!work) {
        return -1;
    }
    
    for (int i = 0; i < AVALON10_DEFAULT_MODULARS; i++) {
        if (info->modules[i].state == AVALON10_MODULE_STATE_MINING) {
            
            /* Пакет 1: байты 0-31 заголовка */
            memset(&pkg, 0, sizeof(pkg));
            memcpy(pkg.data, work->header, 32);
            build_pkg(&pkg, AVALON10_P_WORK, 1, pkt_count);
            send_pkg(i, &pkg);
            
            /* Пакет 2: байты 32-63 заголовка */
            memset(&pkg, 0, sizeof(pkg));
            memcpy(pkg.data, work->header + 32, 32);
            build_pkg(&pkg, AVALON10_P_WORK, 2, pkt_count);
            send_pkg(i, &pkg);
            
            /* Пакет 3: байты 64-79 заголовка (16 байт) + padding */
            memset(&pkg, 0, sizeof(pkg));
            memcpy(pkg.data, work->header + 64, 16);
            build_pkg(&pkg, AVALON10_P_WORK, 3, pkt_count);
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
