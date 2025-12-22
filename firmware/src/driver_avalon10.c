/**
 * =============================================================================
 * @file    driver_avalon10.c
 * @brief   Драйвер хеш-плат Avalon A1126pro (ASIC)
 * =============================================================================
 * 
 * Этот файл содержит реализацию драйвера для управления ASIC чипами
 * хеш-плат Avalon. Драйвер обеспечивает:
 * 
 * - Инициализацию и обнаружение хеш-плат
 * - Отправку заданий на ASIC
 * - Получение найденных нонсов
 * - Управление питанием и частотой
 * - Мониторинг температуры и управление вентиляторами
 * - Функцию Smart Speed для автоматической оптимизации
 * 
 * Протокол обмена:
 * - Физический уровень: SPI
 * - Формат пакетов: [SYNC][LEN][CMD][DATA...][CHECKSUM]
 * 
 * @author  Reconstructed from Avalon A1126pro firmware
 * @version 1.0
 * =============================================================================
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>

#include "config.h"
#include "avalon10.h"
#include "miner.h"
#include "k210_hal.h"

/* FreeRTOS */
#ifdef USE_FREERTOS
#include "FreeRTOS.h"
#include "task.h"
#include "semphr.h"
#endif

/* =============================================================================
 * ЛОКАЛЬНЫЕ ПЕРЕМЕННЫЕ
 * ============================================================================= */

/** Массив обнаруженных устройств (хеш-плат) */
static avalon10_device_t devices[AVA10_MAX_DEVICES];

/** Количество обнаруженных устройств */
static int device_count = 0;

/** Текущая конфигурация драйвера */
static avalon10_config_t g_config = {
    .frequency = AVA10_DEFAULT_FREQUENCY,
    .voltage = AVA10_DEFAULT_VOLTAGE,
    .fan_speed = AVA10_DEFAULT_FAN_SPEED,
    .fan_mode = AVA10_FAN_MODE_AUTO,
    .temp_target = AVA10_TEMP_TARGET,
    .temp_max = AVA10_TEMP_OVERHEAT,
    .smart_speed_mode = AVA10_SMART_SPEED_AUTO,
    .ss_max_frequency = AVA10_SS_FREQ_MAX,
    .ss_min_frequency = AVA10_SS_FREQ_MIN,
    .ss_ntime_offset = AVA10_SS_NTIME_OFFSET,
    .nonce_mask = AVA10_DEFAULT_NONCE_MASK,
    .nonce_check = 1,
    .asic_count = AVA10_ASIC_COUNT,
    .module_count = AVA10_DEFAULT_MODULES,
    .led_mode = AVA10_LED_MODE_NORMAL
};

/** Мьютекс для защиты SPI */
static SemaphoreHandle_t spi_mutex = NULL;

/** Буферы для SPI обмена */
static uint8_t spi_tx_buf[AVA10_MAX_PACKET_SIZE] __attribute__((aligned(8)));
static uint8_t spi_rx_buf[AVA10_MAX_PACKET_SIZE] __attribute__((aligned(8)));

/* =============================================================================
 * ВСПОМОГАТЕЛЬНЫЕ ФУНКЦИИ
 * ============================================================================= */

/**
 * @brief Вычисление контрольной суммы CRC16
 * 
 * Используется для проверки целостности пакетов.
 * Полином: 0x1021 (CRC-16-CCITT)
 * 
 * @param data Указатель на данные
 * @param len Длина данных в байтах
 * @return CRC16 значение
 */
static uint16_t calc_crc16(const uint8_t *data, size_t len)
{
    uint16_t crc = 0xFFFF;
    
    for (size_t i = 0; i < len; i++) {
        crc ^= (uint16_t)data[i] << 8;
        for (int j = 0; j < 8; j++) {
            if (crc & 0x8000) {
                crc = (crc << 1) ^ 0x1021;
            } else {
                crc = crc << 1;
            }
        }
    }
    
    return crc;
}

/**
 * @brief Формирование пакета для отправки
 * 
 * Структура пакета:
 * | SYNC (2 байта) | LEN (1 байт) | CMD (1 байт) | DATA (N байт) | CRC16 (2 байта) |
 * 
 * @param buf Буфер для записи пакета
 * @param cmd Код команды
 * @param data Данные команды (может быть NULL)
 * @param data_len Длина данных
 * @return Общая длина пакета
 */
static int build_packet(uint8_t *buf, uint8_t cmd, const uint8_t *data, int data_len)
{
    int offset = 0;
    
    /* Синхробайты */
    buf[offset++] = AVA10_PKT_SYNC_1;
    buf[offset++] = AVA10_PKT_SYNC_2;
    
    /* Длина (CMD + DATA) */
    buf[offset++] = 1 + data_len;
    
    /* Код команды */
    buf[offset++] = cmd;
    
    /* Данные */
    if (data != NULL && data_len > 0) {
        memcpy(&buf[offset], data, data_len);
        offset += data_len;
    }
    
    /* Контрольная сумма */
    uint16_t crc = calc_crc16(&buf[2], 2 + data_len);  /* CRC от LEN до конца данных */
    buf[offset++] = (crc >> 8) & 0xFF;   /* CRC старший байт */
    buf[offset++] = crc & 0xFF;          /* CRC младший байт */
    
    return offset;
}

/**
 * @brief Парсинг принятого пакета
 * 
 * Проверяет синхробайты и контрольную сумму.
 * 
 * @param buf Буфер с принятыми данными
 * @param len Длина буфера
 * @param cmd [out] Код команды
 * @param data [out] Указатель на данные в буфере
 * @param data_len [out] Длина данных
 * @return 0 при успехе, <0 при ошибке
 */
static int parse_packet(const uint8_t *buf, int len, uint8_t *cmd, 
                        uint8_t **data, int *data_len)
{
    /* Проверяем минимальную длину */
    if (len < 6) {
        return -1;  /* Слишком короткий пакет */
    }
    
    /* Проверяем синхробайты */
    if (buf[0] != AVA10_PKT_SYNC_1 || buf[1] != AVA10_PKT_SYNC_2) {
        return -2;  /* Неверные синхробайты */
    }
    
    /* Извлекаем длину */
    int pkt_len = buf[2];
    if (pkt_len + 5 > len) {
        return -3;  /* Недостаточно данных */
    }
    
    /* Проверяем CRC */
    uint16_t crc_calc = calc_crc16(&buf[2], pkt_len + 1);
    uint16_t crc_recv = ((uint16_t)buf[3 + pkt_len] << 8) | buf[4 + pkt_len];
    
    if (crc_calc != crc_recv) {
        return -4;  /* Ошибка CRC */
    }
    
    /* Извлекаем данные */
    *cmd = buf[3];
    *data = (uint8_t *)&buf[4];
    *data_len = pkt_len - 1;
    
    return 0;
}

/**
 * @brief Отправка/приём пакета по SPI
 * 
 * Выполняет полнодуплексный обмен по SPI.
 * Использует мьютекс для защиты от одновременного доступа.
 * 
 * @param device Индекс устройства (выбор CS)
 * @param tx_data Данные для отправки
 * @param rx_data Буфер для приёма
 * @param len Длина обмена
 * @return 0 при успехе, <0 при ошибке
 */
static int spi_transfer(int device, const uint8_t *tx_data, uint8_t *rx_data, int len)
{
    int ret = 0;
    
    /* Захватываем мьютекс SPI */
    if (xSemaphoreTake(spi_mutex, pdMS_TO_TICKS(1000)) != pdTRUE) {
        applog(LOG_ERR, "SPI mutex timeout");
        return -1;
    }
    
    /* Выбираем устройство (Chip Select) */
    int cs_pin;
    switch (device) {
        case 0: cs_pin = SPI_CHIP_SELECT_0; break;
        case 1: cs_pin = SPI_CHIP_SELECT_1; break;
        case 2: cs_pin = SPI_CHIP_SELECT_2; break;
        case 3: cs_pin = SPI_CHIP_SELECT_3; break;
        default:
            xSemaphoreGive(spi_mutex);
            return -2;
    }
    
    /* Активируем CS (низкий уровень) */
    gpio_set_pin(cs_pin, GPIO_PV_LOW);
    
    /* Небольшая задержка для стабилизации */
    delay_us(AVA10_SPI_SETUP_DELAY);
    
    /* Выполняем SPI обмен */
    if (spi_send_receive(SPI_HASH_DEVICE, tx_data, rx_data, len) != 0) {
        ret = -3;
    }
    
    /* Задержка перед отпусканием CS */
    delay_us(AVA10_SPI_HOLD_DELAY);
    
    /* Деактивируем CS (высокий уровень) */
    gpio_set_pin(cs_pin, GPIO_PV_HIGH);
    
    /* Освобождаем мьютекс */
    xSemaphoreGive(spi_mutex);
    
    return ret;
}

/* =============================================================================
 * ОСНОВНЫЕ ФУНКЦИИ ДРАЙВЕРА
 * ============================================================================= */

/**
 * @brief Инициализация драйвера Avalon10
 * 
 * Выполняет:
 * - Создание объектов синхронизации
 * - Инициализацию SPI
 * - Настройку GPIO пинов
 * - Начальную конфигурацию
 * 
 * @return 0 при успехе, <0 при ошибке
 */
int avalon10_init(void)
{
    applog(LOG_INFO, "Инициализация драйвера Avalon10...");
    
    /* Создаём мьютекс для SPI */
    spi_mutex = xSemaphoreCreateMutex();
    if (spi_mutex == NULL) {
        applog(LOG_ERR, "Не удалось создать SPI мьютекс");
        return -1;
    }
    
    /* Очищаем массив устройств */
    memset(devices, 0, sizeof(devices));
    device_count = 0;
    
    /* Настройка GPIO для управления питанием хеш-плат */
    gpio_set_pin(PIN_HASH_POWER_EN, GPIO_PV_LOW);   /* Питание выключено */
    gpio_set_pin(PIN_HASH_RESET, GPIO_PV_LOW);      /* Сброс активен */
    gpio_set_pin(PIN_HASH_CHAIN_EN, GPIO_PV_LOW);   /* Цепь выключена */
    
    /* Задержка для стабилизации */
    vTaskDelay(pdMS_TO_TICKS(100));
    
    /* Включаем питание */
    gpio_set_pin(PIN_HASH_POWER_EN, GPIO_PV_HIGH);
    vTaskDelay(pdMS_TO_TICKS(AVA10_POWER_ON_DELAY));
    
    /* Снимаем сброс */
    gpio_set_pin(PIN_HASH_RESET, GPIO_PV_HIGH);
    vTaskDelay(pdMS_TO_TICKS(AVA10_RESET_DELAY));
    
    /* Включаем цепь хеш-плат */
    gpio_set_pin(PIN_HASH_CHAIN_EN, GPIO_PV_HIGH);
    vTaskDelay(pdMS_TO_TICKS(AVA10_CHAIN_INIT_DELAY));
    
    applog(LOG_INFO, "Драйвер Avalon10 инициализирован");
    
    return 0;
}

/**
 * @brief Обнаружение хеш-плат
 * 
 * Сканирует все возможные адреса и обнаруживает подключённые хеш-платы.
 * Для каждой платы получает информацию о версии и количестве чипов.
 * 
 * @return Количество обнаруженных хеш-плат
 */
int avalon10_detect(void)
{
    applog(LOG_INFO, "Поиск хеш-плат Avalon10...");
    
    device_count = 0;
    
    /* Сканируем все возможные слоты */
    for (int slot = 0; slot < AVA10_MAX_DEVICES; slot++) {
        applog(LOG_DEBUG, "Проверка слота %d...", slot);
        
        /* Формируем запрос Device Info */
        int pkt_len = build_packet(spi_tx_buf, AVA10_P_DETECT, NULL, 0);
        
        /* Очищаем буфер приёма */
        memset(spi_rx_buf, 0, sizeof(spi_rx_buf));
        
        /* Отправляем запрос */
        if (spi_transfer(slot, spi_tx_buf, spi_rx_buf, pkt_len + 64) != 0) {
            continue;  /* Ошибка - пропускаем слот */
        }
        
        /* Ищем ответ в буфере */
        for (int i = 0; i < 64; i++) {
            if (spi_rx_buf[i] == AVA10_PKT_SYNC_1 && 
                spi_rx_buf[i+1] == AVA10_PKT_SYNC_2) {
                
                uint8_t cmd;
                uint8_t *data;
                int data_len;
                
                if (parse_packet(&spi_rx_buf[i], 64 - i, &cmd, &data, &data_len) == 0) {
                    if (cmd == AVA10_P_DETECT && data_len >= 8) {
                        /* Найдено устройство! */
                        avalon10_device_t *dev = &devices[device_count];
                        
                        dev->slot = slot;
                        dev->is_present = true;
                        dev->asic_count = data[0];
                        dev->module_count = data[1];
                        dev->hw_version = (data[2] << 8) | data[3];
                        dev->fw_version = (data[4] << 8) | data[5];
                        memcpy(dev->serial, &data[6], 8);
                        
                        /* Инициализируем параметры по умолчанию */
                        dev->frequency = g_config.frequency;
                        dev->voltage = g_config.voltage;
                        dev->fan_speed = g_config.fan_speed;
                        dev->temp_current = 0;
                        dev->temp_max = 0;
                        dev->hashrate = 0;
                        
                        applog(LOG_INFO, "Обнаружена хеш-плата в слоте %d:", slot);
                        applog(LOG_INFO, "  ASIC чипов: %d", dev->asic_count);
                        applog(LOG_INFO, "  Модулей: %d", dev->module_count);
                        applog(LOG_INFO, "  HW версия: 0x%04X", dev->hw_version);
                        applog(LOG_INFO, "  FW версия: 0x%04X", dev->fw_version);
                        
                        device_count++;
                        break;
                    }
                }
            }
        }
        
        if (device_count >= AVA10_MAX_DEVICES) {
            break;  /* Достигнут максимум устройств */
        }
    }
    
    applog(LOG_INFO, "Обнаружено хеш-плат: %d", device_count);
    
    return device_count;
}

/**
 * @brief Настройка частоты ASIC
 * 
 * @param device Индекс устройства
 * @param frequency Частота в МГц (AVA10_FREQ_MIN - AVA10_FREQ_MAX)
 * @return 0 при успехе, <0 при ошибке
 */
int avalon10_set_frequency(int device, int frequency)
{
    if (device < 0 || device >= device_count) {
        return -1;
    }
    
    if (frequency < AVA10_FREQ_MIN || frequency > AVA10_FREQ_MAX) {
        applog(LOG_WARNING, "Частота %d МГц вне диапазона", frequency);
        return -2;
    }
    
    /* Формируем пакет настройки частоты */
    uint8_t data[4];
    data[0] = (frequency >> 8) & 0xFF;   /* Старший байт частоты */
    data[1] = frequency & 0xFF;          /* Младший байт частоты */
    data[2] = 0;                         /* Зарезервировано */
    data[3] = 0;                         /* Зарезервировано */
    
    int pkt_len = build_packet(spi_tx_buf, AVA10_P_SET_FREQ, data, sizeof(data));
    
    if (spi_transfer(devices[device].slot, spi_tx_buf, spi_rx_buf, pkt_len) != 0) {
        applog(LOG_ERR, "Ошибка отправки команды настройки частоты");
        return -3;
    }
    
    /* Ждём применения настройки */
    vTaskDelay(pdMS_TO_TICKS(50));
    
    devices[device].frequency = frequency;
    applog(LOG_INFO, "Устройство %d: частота установлена %d МГц", device, frequency);
    
    return 0;
}

/**
 * @brief Настройка напряжения питания ASIC
 * 
 * @param device Индекс устройства
 * @param voltage Напряжение в мВ
 * @return 0 при успехе, <0 при ошибке
 */
int avalon10_set_voltage(int device, int voltage)
{
    if (device < 0 || device >= device_count) {
        return -1;
    }
    
    if (voltage < AVA10_VOLTAGE_MIN || voltage > AVA10_VOLTAGE_MAX) {
        applog(LOG_WARNING, "Напряжение %d мВ вне диапазона", voltage);
        return -2;
    }
    
    /* Формируем пакет настройки напряжения */
    uint8_t data[4];
    data[0] = (voltage >> 8) & 0xFF;
    data[1] = voltage & 0xFF;
    data[2] = 0;
    data[3] = 0;
    
    int pkt_len = build_packet(spi_tx_buf, AVA10_P_SET_VOLT, data, sizeof(data));
    
    if (spi_transfer(devices[device].slot, spi_tx_buf, spi_rx_buf, pkt_len) != 0) {
        return -3;
    }
    
    vTaskDelay(pdMS_TO_TICKS(100));  /* Ждём стабилизации напряжения */
    
    devices[device].voltage = voltage;
    applog(LOG_INFO, "Устройство %d: напряжение установлено %d мВ", device, voltage);
    
    return 0;
}

/**
 * @brief Отправка задания на майнинг
 * 
 * Формирует и отправляет пакет с данными блока для хеширования.
 * Задание рассылается на все обнаруженные хеш-платы.
 * 
 * @param job Указатель на структуру задания
 * @return 0 при успехе, <0 при ошибке
 */
int avalon10_send_job(const avalon10_job_t *job)
{
    if (job == NULL) {
        return -1;
    }
    
    if (device_count == 0) {
        applog(LOG_WARNING, "Нет обнаруженных устройств для отправки задания");
        return -2;
    }
    
    applog(LOG_DEBUG, "Отправка задания ID: %02X%02X%02X%02X",
           job->job_id[0], job->job_id[1], job->job_id[2], job->job_id[3]);
    
    /* ---------------------------------------------------------------------
     * Формируем пакет задания (JOB packet)
     * 
     * Структура данных задания:
     * [0-3]   job_id       - ID задания
     * [4-35]  prev_hash    - Хеш предыдущего блока (32 байта)
     * [36-39] version      - Версия блока (4 байта, little-endian)
     * [40-43] nbits        - Сложность (4 байта)
     * [44-47] ntime        - Метка времени (4 байта)
     * [48-51] nonce_start  - Начальный нонс для поиска
     * [52-55] nonce_end    - Конечный нонс
     * [56-87] target       - Целевой хеш (32 байта)
     * --------------------------------------------------------------------- */
    
    uint8_t job_data[88];
    int offset = 0;
    
    /* Job ID */
    memcpy(&job_data[offset], job->job_id, 4);
    offset += 4;
    
    /* Previous hash (в правильном порядке байт для ASIC) */
    for (int i = 0; i < 32; i += 4) {
        job_data[offset++] = job->prev_hash[i + 3];
        job_data[offset++] = job->prev_hash[i + 2];
        job_data[offset++] = job->prev_hash[i + 1];
        job_data[offset++] = job->prev_hash[i + 0];
    }
    
    /* Version (little-endian) */
    job_data[offset++] = (job->version >> 0) & 0xFF;
    job_data[offset++] = (job->version >> 8) & 0xFF;
    job_data[offset++] = (job->version >> 16) & 0xFF;
    job_data[offset++] = (job->version >> 24) & 0xFF;
    
    /* nBits */
    job_data[offset++] = (job->nbits >> 0) & 0xFF;
    job_data[offset++] = (job->nbits >> 8) & 0xFF;
    job_data[offset++] = (job->nbits >> 16) & 0xFF;
    job_data[offset++] = (job->nbits >> 24) & 0xFF;
    
    /* nTime */
    job_data[offset++] = (job->ntime >> 0) & 0xFF;
    job_data[offset++] = (job->ntime >> 8) & 0xFF;
    job_data[offset++] = (job->ntime >> 16) & 0xFF;
    job_data[offset++] = (job->ntime >> 24) & 0xFF;
    
    /* Nonce range start */
    job_data[offset++] = (job->nonce_start >> 0) & 0xFF;
    job_data[offset++] = (job->nonce_start >> 8) & 0xFF;
    job_data[offset++] = (job->nonce_start >> 16) & 0xFF;
    job_data[offset++] = (job->nonce_start >> 24) & 0xFF;
    
    /* Nonce range end */
    job_data[offset++] = (job->nonce_end >> 0) & 0xFF;
    job_data[offset++] = (job->nonce_end >> 8) & 0xFF;
    job_data[offset++] = (job->nonce_end >> 16) & 0xFF;
    job_data[offset++] = (job->nonce_end >> 24) & 0xFF;
    
    /* Target */
    memcpy(&job_data[offset], job->target, 32);
    offset += 32;
    
    /* Формируем пакет */
    int pkt_len = build_packet(spi_tx_buf, AVA10_P_JOB, job_data, offset);
    
    /* Отправляем на все устройства */
    int success_count = 0;
    for (int i = 0; i < device_count; i++) {
        if (devices[i].is_present) {
            if (spi_transfer(devices[i].slot, spi_tx_buf, spi_rx_buf, pkt_len) == 0) {
                devices[i].has_job = true;
                success_count++;
            }
        }
    }
    
    if (success_count == 0) {
        applog(LOG_ERR, "Не удалось отправить задание ни на одно устройство");
        return -3;
    }
    
    applog(LOG_DEBUG, "Задание отправлено на %d устройств", success_count);
    return 0;
}

/**
 * @brief Опрос ASIC на наличие найденных нонсов
 * 
 * Опрашивает все устройства и собирает найденные нонсы.
 * 
 * @param nonces [out] Массив для записи найденных нонсов
 * @param max_nonces Максимальное количество нонсов для записи
 * @return Количество найденных нонсов
 */
int avalon10_poll_nonces(avalon10_nonce_t *nonces, int max_nonces)
{
    if (nonces == NULL || max_nonces <= 0) {
        return 0;
    }
    
    int nonce_count = 0;
    
    /* Опрашиваем каждое устройство */
    for (int dev = 0; dev < device_count && nonce_count < max_nonces; dev++) {
        if (!devices[dev].is_present || !devices[dev].has_job) {
            continue;
        }
        
        /* Формируем запрос на получение нонсов */
        int pkt_len = build_packet(spi_tx_buf, AVA10_P_NONCE, NULL, 0);
        
        /* Очищаем буфер приёма */
        memset(spi_rx_buf, 0xFF, sizeof(spi_rx_buf));
        
        /* Отправляем запрос и получаем ответ */
        if (spi_transfer(devices[dev].slot, spi_tx_buf, spi_rx_buf, 128) != 0) {
            continue;
        }
        
        /* Ищем ответные пакеты с нонсами */
        for (int i = 0; i < 128 - 16 && nonce_count < max_nonces; i++) {
            if (spi_rx_buf[i] == AVA10_PKT_SYNC_1 && 
                spi_rx_buf[i+1] == AVA10_PKT_SYNC_2) {
                
                uint8_t cmd;
                uint8_t *data;
                int data_len;
                
                if (parse_packet(&spi_rx_buf[i], 128 - i, &cmd, &data, &data_len) == 0) {
                    if (cmd == AVA10_P_NONCE && data_len >= 12) {
                        /* -------------------------------------------------
                         * Структура данных нонса:
                         * [0-3]   nonce    - Найденный нонс (4 байта)
                         * [4-7]   nonce2   - Дополнительный нонс
                         * [8]     chip_id  - ID чипа
                         * [9]     module   - Номер модуля
                         * [10-11] reserved
                         * ------------------------------------------------- */
                        
                        avalon10_nonce_t *n = &nonces[nonce_count];
                        
                        n->nonce = ((uint32_t)data[3] << 24) |
                                   ((uint32_t)data[2] << 16) |
                                   ((uint32_t)data[1] << 8) |
                                   ((uint32_t)data[0]);
                        
                        n->nonce2 = ((uint32_t)data[7] << 24) |
                                    ((uint32_t)data[6] << 16) |
                                    ((uint32_t)data[5] << 8) |
                                    ((uint32_t)data[4]);
                        
                        n->chip_id = data[8];
                        n->module = data[9];
                        n->device = dev;
                        n->timestamp = get_ms_time();
                        
                        nonce_count++;
                        
                        /* Пропускаем уже обработанные байты */
                        i += spi_rx_buf[i + 2] + 4;
                    }
                }
            }
        }
    }
    
    return nonce_count;
}

/**
 * @brief Вычисление текущего хешрейта
 * 
 * Возвращает суммарный хешрейт всех устройств в H/s.
 * 
 * @return Хешрейт в H/s
 */
uint64_t avalon10_scanhash(void)
{
    uint64_t total_hashrate = 0;
    
    /* Запрашиваем статистику с каждого устройства */
    for (int dev = 0; dev < device_count; dev++) {
        if (!devices[dev].is_present) {
            continue;
        }
        
        /* Формируем запрос статистики */
        int pkt_len = build_packet(spi_tx_buf, AVA10_P_STATUS, NULL, 0);
        
        memset(spi_rx_buf, 0, sizeof(spi_rx_buf));
        
        if (spi_transfer(devices[dev].slot, spi_tx_buf, spi_rx_buf, 64) != 0) {
            continue;
        }
        
        /* Парсим ответ */
        for (int i = 0; i < 48; i++) {
            if (spi_rx_buf[i] == AVA10_PKT_SYNC_1 && 
                spi_rx_buf[i+1] == AVA10_PKT_SYNC_2) {
                
                uint8_t cmd;
                uint8_t *data;
                int data_len;
                
                if (parse_packet(&spi_rx_buf[i], 64 - i, &cmd, &data, &data_len) == 0) {
                    if (cmd == AVA10_P_STATUS && data_len >= 8) {
                        /* -------------------------------------------------
                         * Структура статуса:
                         * [0-7]   hashrate   - Хешрейт в H/s (8 байт, LE)
                         * ------------------------------------------------- */
                        
                        uint64_t hr = 0;
                        for (int j = 7; j >= 0; j--) {
                            hr = (hr << 8) | data[j];
                        }
                        
                        devices[dev].hashrate = hr;
                        total_hashrate += hr;
                        break;
                    }
                }
            }
        }
    }
    
    return total_hashrate;
}

/* =============================================================================
 * УПРАВЛЕНИЕ ТЕМПЕРАТУРОЙ И ВЕНТИЛЯТОРАМИ
 * ============================================================================= */

/**
 * @brief Чтение температуры с датчиков
 * 
 * @param device Индекс устройства
 * @return Температура в градусах Цельсия, или -1 при ошибке
 */
int avalon10_read_temperature(int device)
{
    if (device < 0 || device >= device_count) {
        return -1;
    }
    
    /* Формируем запрос температуры */
    int pkt_len = build_packet(spi_tx_buf, AVA10_P_TEMP, NULL, 0);
    
    memset(spi_rx_buf, 0, sizeof(spi_rx_buf));
    
    if (spi_transfer(devices[device].slot, spi_tx_buf, spi_rx_buf, 32) != 0) {
        return -1;
    }
    
    /* Ищем ответ */
    for (int i = 0; i < 24; i++) {
        if (spi_rx_buf[i] == AVA10_PKT_SYNC_1 && 
            spi_rx_buf[i+1] == AVA10_PKT_SYNC_2) {
            
            uint8_t cmd;
            uint8_t *data;
            int data_len;
            
            if (parse_packet(&spi_rx_buf[i], 32 - i, &cmd, &data, &data_len) == 0) {
                if (cmd == AVA10_P_TEMP && data_len >= 2) {
                    int temp = (int16_t)((data[0] << 8) | data[1]);
                    
                    /* Обновляем статистику */
                    devices[device].temp_current = temp;
                    if (temp > devices[device].temp_max) {
                        devices[device].temp_max = temp;
                    }
                    
                    return temp;
                }
            }
        }
    }
    
    return -1;
}

/**
 * @brief Установка скорости вентиляторов
 * 
 * @param speed Скорость в процентах (0-100)
 * @return 0 при успехе
 */
int avalon10_set_fan_speed(int speed)
{
    /* Ограничиваем диапазон */
    if (speed < AVA10_FAN_MIN_PWM) {
        speed = AVA10_FAN_MIN_PWM;
    }
    if (speed > AVA10_FAN_MAX_PWM) {
        speed = AVA10_FAN_MAX_PWM;
    }
    
    /* Преобразуем проценты в значение ШИМ */
    /* PWM период: 1000000 / AVA10_FAN_PWM_FREQ нс */
    /* Duty cycle = speed% от периода */
    uint32_t period = 1000000000 / AVA10_FAN_PWM_FREQ;  /* Период в нс */
    uint32_t duty = (period * speed) / 100;
    
    /* Устанавливаем ШИМ для вентиляторов */
    timer_set_enable(TIMER_DEVICE_0, TIMER_CHANNEL_0, 0);  /* Выключаем */
    pwm_set_frequency(PWM_DEVICE_0, PWM_CHANNEL_0, AVA10_FAN_PWM_FREQ, (double)speed / 100.0);
    pwm_set_frequency(PWM_DEVICE_1, PWM_CHANNEL_0, AVA10_FAN_PWM_FREQ, (double)speed / 100.0);
    timer_set_enable(TIMER_DEVICE_0, TIMER_CHANNEL_0, 1);  /* Включаем */
    
    g_config.fan_speed = speed;
    
    applog(LOG_DEBUG, "Скорость вентиляторов: %d%%", speed);
    
    return 0;
}

/**
 * @brief Обновление управления вентиляторами
 * 
 * Вызывается периодически из задачи мониторинга.
 * В автоматическом режиме регулирует скорость вентиляторов
 * в зависимости от температуры.
 */
void avalon10_update_fan_control(void)
{
    /* Читаем температуру со всех устройств и находим максимальную */
    int max_temp = 0;
    
    for (int i = 0; i < device_count; i++) {
        int temp = avalon10_read_temperature(i);
        if (temp > max_temp) {
            max_temp = temp;
        }
    }
    
    if (g_config.fan_mode == AVA10_FAN_MODE_AUTO) {
        /* ---------------------------------------------------------------------
         * Алгоритм автоматического управления вентиляторами:
         * 
         * Используем пропорциональное управление:
         * - Ниже target_temp - минимальная скорость
         * - Между target и max - линейное увеличение
         * - Выше max - максимальная скорость
         * --------------------------------------------------------------------- */
        
        int target_speed;
        
        if (max_temp <= g_config.temp_target) {
            /* Температура нормальная - минимальная скорость */
            target_speed = AVA10_FAN_MIN_PWM;
        }
        else if (max_temp >= g_config.temp_max) {
            /* Перегрев - максимальная скорость */
            target_speed = AVA10_FAN_MAX_PWM;
        }
        else {
            /* Линейная интерполяция */
            int temp_range = g_config.temp_max - g_config.temp_target;
            int temp_diff = max_temp - g_config.temp_target;
            int speed_range = AVA10_FAN_MAX_PWM - AVA10_FAN_MIN_PWM;
            
            target_speed = AVA10_FAN_MIN_PWM + 
                           (temp_diff * speed_range) / temp_range;
        }
        
        /* Плавное изменение скорости (защита от резких скачков) */
        int current_speed = g_config.fan_speed;
        if (target_speed > current_speed) {
            /* Увеличиваем быстро */
            target_speed = (current_speed + target_speed * 3) / 4;
        } else {
            /* Уменьшаем медленно */
            target_speed = (current_speed * 3 + target_speed) / 4;
        }
        
        avalon10_set_fan_speed(target_speed);
    }
    /* В ручном режиме вентиляторы управляются вручную */
}

/* =============================================================================
 * ФУНКЦИЯ SMART SPEED
 * ============================================================================= */

/**
 * @brief Обновление параметров Smart Speed
 * 
 * Функция автоматической оптимизации частоты в зависимости от:
 * - Температуры
 * - Количества аппаратных ошибок
 * - Эффективности хеширования
 */
void avalon10_update_smart_speed(void)
{
#ifdef USE_SMART_SPEED
    if (g_config.smart_speed_mode == AVA10_SMART_SPEED_OFF) {
        return;
    }
    
    /* Собираем статистику со всех устройств */
    for (int dev = 0; dev < device_count; dev++) {
        avalon10_device_t *d = &devices[dev];
        
        if (!d->is_present) {
            continue;
        }
        
        int current_freq = d->frequency;
        int target_freq = current_freq;
        
        /* ---------------------------------------------------------------------
         * Алгоритм Smart Speed:
         * 
         * 1. Если температура слишком высокая - снижаем частоту
         * 2. Если много HW ошибок - снижаем частоту
         * 3. Если всё хорошо и частота ниже максимума - повышаем
         * --------------------------------------------------------------------- */
        
        /* Проверка температуры */
        if (d->temp_current >= g_config.temp_max - 5) {
            /* Близко к перегреву - снижаем частоту */
            target_freq = current_freq - AVA10_SS_FREQ_STEP;
            applog(LOG_DEBUG, "Smart Speed [%d]: снижение частоты из-за температуры", dev);
        }
        else if (d->temp_current < g_config.temp_target - 5) {
            /* Температура низкая - можно повысить частоту */
            if (current_freq < g_config.ss_max_frequency) {
                target_freq = current_freq + AVA10_SS_FREQ_STEP;
                applog(LOG_DEBUG, "Smart Speed [%d]: повышение частоты", dev);
            }
        }
        
        /* Ограничиваем диапазон частоты */
        if (target_freq < g_config.ss_min_frequency) {
            target_freq = g_config.ss_min_frequency;
        }
        if (target_freq > g_config.ss_max_frequency) {
            target_freq = g_config.ss_max_frequency;
        }
        
        /* Применяем новую частоту если изменилась */
        if (target_freq != current_freq) {
            avalon10_set_frequency(dev, target_freq);
        }
    }
#endif
}

/* =============================================================================
 * ФУНКЦИИ КОНФИГУРАЦИИ
 * ============================================================================= */

/**
 * @brief Получение текущего статуса устройства
 * 
 * @param status [out] Структура для записи статуса
 * @return 0 при успехе
 */
int avalon10_get_device_status(avalon10_device_t *status)
{
    if (status == NULL || device_count == 0) {
        return -1;
    }
    
    /* Копируем данные первого устройства как пример */
    memcpy(status, &devices[0], sizeof(avalon10_device_t));
    
    /* Обновляем агрегированные данные */
    status->temp_current = 0;
    status->hashrate = 0;
    
    for (int i = 0; i < device_count; i++) {
        if (devices[i].temp_current > status->temp_current) {
            status->temp_current = devices[i].temp_current;
        }
        status->hashrate += devices[i].hashrate;
    }
    
    return 0;
}

/**
 * @brief Получение количества обнаруженных устройств
 */
int avalon10_get_device_count(void)
{
    return device_count;
}

/**
 * @brief Загрузка конфигурации из Flash памяти
 * 
 * @return 0 при успехе, <0 если конфигурация не найдена
 */
int avalon10_load_config(void)
{
    avalon10_config_t loaded_config;
    
    /* Читаем конфигурацию из Flash */
    if (flash_read(FLASH_CONFIG_ADDR, (uint8_t *)&loaded_config, 
                   sizeof(loaded_config)) != 0) {
        return -1;
    }
    
    /* Проверяем magic number */
    if (loaded_config.magic != AVA10_CONFIG_MAGIC) {
        return -2;
    }
    
    /* Проверяем CRC */
    uint16_t crc = calc_crc16((uint8_t *)&loaded_config, 
                               sizeof(loaded_config) - sizeof(uint16_t));
    if (crc != loaded_config.crc) {
        return -3;
    }
    
    /* Применяем конфигурацию */
    memcpy(&g_config, &loaded_config, sizeof(g_config));
    
    applog(LOG_INFO, "Конфигурация загружена из Flash");
    applog(LOG_INFO, "  Частота: %d МГц", g_config.frequency);
    applog(LOG_INFO, "  Напряжение: %d мВ", g_config.voltage);
    applog(LOG_INFO, "  Вентилятор: %d%%", g_config.fan_speed);
    
    return 0;
}

/**
 * @brief Сохранение конфигурации во Flash память
 * 
 * @return 0 при успехе
 */
int avalon10_save_config(void)
{
    /* Устанавливаем magic number */
    g_config.magic = AVA10_CONFIG_MAGIC;
    
    /* Вычисляем CRC */
    g_config.crc = calc_crc16((uint8_t *)&g_config, 
                               sizeof(g_config) - sizeof(uint16_t));
    
    /* Стираем сектор Flash */
    if (flash_erase_sector(FLASH_CONFIG_ADDR) != 0) {
        applog(LOG_ERR, "Ошибка стирания Flash");
        return -1;
    }
    
    /* Записываем конфигурацию */
    if (flash_write(FLASH_CONFIG_ADDR, (uint8_t *)&g_config, 
                    sizeof(g_config)) != 0) {
        applog(LOG_ERR, "Ошибка записи Flash");
        return -2;
    }
    
    applog(LOG_INFO, "Конфигурация сохранена во Flash");
    
    return 0;
}

/**
 * @brief Сброс конфигурации на значения по умолчанию
 */
void avalon10_reset_config(void)
{
    g_config.frequency = AVA10_DEFAULT_FREQUENCY;
    g_config.voltage = AVA10_DEFAULT_VOLTAGE;
    g_config.fan_speed = AVA10_DEFAULT_FAN_SPEED;
    g_config.fan_mode = AVA10_FAN_MODE_AUTO;
    g_config.temp_target = AVA10_TEMP_TARGET;
    g_config.temp_max = AVA10_TEMP_OVERHEAT;
    g_config.smart_speed_mode = AVA10_SMART_SPEED_AUTO;
    g_config.ss_max_frequency = AVA10_SS_FREQ_MAX;
    g_config.ss_min_frequency = AVA10_SS_FREQ_MIN;
    g_config.ss_ntime_offset = AVA10_SS_NTIME_OFFSET;
    g_config.nonce_mask = AVA10_DEFAULT_NONCE_MASK;
    g_config.nonce_check = 1;
    g_config.asic_count = AVA10_ASIC_COUNT;
    g_config.module_count = AVA10_DEFAULT_MODULES;
    g_config.led_mode = AVA10_LED_MODE_NORMAL;
    
    applog(LOG_INFO, "Конфигурация сброшена на значения по умолчанию");
}

/* =============================================================================
 * ЗАВЕРШЕНИЕ РАБОТЫ
 * ============================================================================= */

/**
 * @brief Корректное завершение работы драйвера
 * 
 * Выключает питание хеш-плат и освобождает ресурсы.
 */
void avalon10_shutdown(void)
{
    applog(LOG_INFO, "Завершение работы драйвера Avalon10...");
    
    /* Останавливаем майнинг */
    for (int i = 0; i < device_count; i++) {
        devices[i].has_job = false;
    }
    
    /* Вентиляторы на минимум */
    avalon10_set_fan_speed(AVA10_FAN_MIN_PWM);
    
    /* Выключаем хеш-платы */
    gpio_set_pin(PIN_HASH_CHAIN_EN, GPIO_PV_LOW);
    vTaskDelay(pdMS_TO_TICKS(100));
    
    gpio_set_pin(PIN_HASH_RESET, GPIO_PV_LOW);
    vTaskDelay(pdMS_TO_TICKS(100));
    
    gpio_set_pin(PIN_HASH_POWER_EN, GPIO_PV_LOW);
    
    /* Освобождаем мьютекс */
    if (spi_mutex != NULL) {
        vSemaphoreDelete(spi_mutex);
        spi_mutex = NULL;
    }
    
    device_count = 0;
    
    applog(LOG_INFO, "Драйвер Avalon10 остановлен");
}
