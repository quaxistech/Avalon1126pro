/**
 * =============================================================================
 * @file    fpga_loader.c
 * @brief   Avalon A1126pro - FPGA Bitstream Loader
 * @version 1.0
 * @date    2024
 * =============================================================================
 * 
 * ОПИСАНИЕ:
 * Реализация драйвера для загрузки bitstream в FPGA.
 * Текущая версия - stub для совместимости.
 * 
 * =============================================================================
 */

#include <stdio.h>
#include <string.h>

#include "fpga_loader.h"
#include "cgminer.h"
#include "mock_hardware.h"
#include "w25qxx.h"

/* FreeRTOS */
#include "FreeRTOS.h"
#include "task.h"

static const char *TAG = "FPGA";

/* ===========================================================================
 * ЛОКАЛЬНЫЕ ПЕРЕМЕННЫЕ
 * =========================================================================== */

static fpga_info_t fpga_info = {0};
static bool fpga_initialized = false;

/* ===========================================================================
 * ВСПОМОГАТЕЛЬНЫЕ ФУНКЦИИ
 * =========================================================================== */

/**
 * @brief Вычисление CRC32
 */
__attribute__((unused))
static uint32_t calc_crc32(const uint8_t *data, uint32_t len)
{
    uint32_t crc = 0xFFFFFFFF;
    
    for (uint32_t i = 0; i < len; i++) {
        crc ^= data[i];
        for (int j = 0; j < 8; j++) {
            crc = (crc >> 1) ^ (0xEDB88320 & -(crc & 1));
        }
    }
    
    return ~crc;
}

/* ===========================================================================
 * ОСНОВНЫЕ ФУНКЦИИ
 * =========================================================================== */

int fpga_init(void)
{
    if (fpga_initialized) {
        return FPGA_OK;
    }
    
    log_message(LOG_INFO, "%s: Инициализация FPGA loader (stub)", TAG);
    
    /* Проверяем состояние - в stub режиме считаем FPGA сконфигурированным */
    fpga_info.state = FPGA_STATE_CONFIGURED;
    fpga_info.bitstream_valid = false;
    
    fpga_initialized = true;
    
    log_message(LOG_INFO, "%s: FPGA loader инициализирован (stub)", TAG);
    
    return FPGA_OK;
}

int fpga_reset(void)
{
    if (!fpga_initialized) {
        return FPGA_ERR_INIT;
    }
    
    log_message(LOG_INFO, "%s: Сброс FPGA (stub)", TAG);
    
    fpga_info.state = FPGA_STATE_RESET;
    vTaskDelay(pdMS_TO_TICKS(100));
    fpga_info.state = FPGA_STATE_CONFIGURED;
    
    return FPGA_OK;
}

bool fpga_check_bitstream(void)
{
    fpga_bitstream_header_t header;
    
    if (!fpga_initialized) {
        return false;
    }
    
    /* Читаем заголовок из flash */
    if (w25qxx_read(FPGA_BITSTREAM_ADDR, (uint8_t *)&header, sizeof(header)) != 0) {
        log_message(LOG_WARNING, "%s: Ошибка чтения flash", TAG);
        fpga_info.bitstream_valid = false;
        return false;
    }
    
    /* Проверяем magic */
    if (header.magic != FPGA_BITSTREAM_MAGIC) {
        log_message(LOG_DEBUG, "%s: Bitstream не найден (magic=0x%08X)", 
                    TAG, (unsigned)header.magic);
        fpga_info.bitstream_valid = false;
        return false;
    }
    
    /* Проверяем версию */
    if (header.version != FPGA_HEADER_VERSION) {
        log_message(LOG_WARNING, "%s: Неподдерживаемая версия: %u", 
                    TAG, (unsigned)header.version);
        fpga_info.bitstream_valid = false;
        return false;
    }
    
    /* Проверяем размер */
    if (header.size == 0 || header.size > FPGA_BITSTREAM_MAX_SIZE) {
        log_message(LOG_WARNING, "%s: Неверный размер: %u", TAG, (unsigned)header.size);
        fpga_info.bitstream_valid = false;
        return false;
    }
    
    /* Сохраняем информацию */
    fpga_info.bitstream_size = header.size;
    fpga_info.bitstream_crc = header.crc32;
    memset(fpga_info.device_name, 0, sizeof(fpga_info.device_name));
    memcpy(fpga_info.device_name, header.device, sizeof(fpga_info.device_name) - 1);
    fpga_info.bitstream_valid = true;
    
    log_message(LOG_INFO, "%s: Bitstream OK: %s, %u bytes", 
                TAG, fpga_info.device_name, (unsigned)fpga_info.bitstream_size);
    
    return true;
}

int fpga_load_bitstream(void)
{
    if (!fpga_initialized) {
        return FPGA_ERR_INIT;
    }
    
    log_message(LOG_INFO, "%s: Загрузка bitstream (stub)", TAG);
    
    /* В stub режиме просто проверяем наличие */
    if (!fpga_check_bitstream()) {
        return FPGA_ERR_NO_BITSTREAM;
    }
    
    /* Эмулируем загрузку */
    fpga_info.state = FPGA_STATE_PROGRAMMING;
    vTaskDelay(pdMS_TO_TICKS(100));
    
    fpga_info.state = FPGA_STATE_CONFIGURED;
    fpga_info.load_time_ms = 100;
    fpga_info.load_count++;
    
    log_message(LOG_INFO, "%s: FPGA сконфигурирован (stub)", TAG);
    
    return FPGA_OK;
}

int fpga_load_from_buffer(const uint8_t *data, uint32_t size)
{
    if (!fpga_initialized) {
        return FPGA_ERR_INIT;
    }
    
    if (!data || size == 0 || size > FPGA_BITSTREAM_MAX_SIZE) {
        return FPGA_ERR_INVALID_BITSTREAM;
    }
    
    log_message(LOG_INFO, "%s: Загрузка bitstream из буфера (%u bytes) (stub)", 
                TAG, (unsigned)size);
    
    /* Эмулируем загрузку */
    fpga_info.state = FPGA_STATE_PROGRAMMING;
    vTaskDelay(pdMS_TO_TICKS(100));
    
    fpga_info.state = FPGA_STATE_CONFIGURED;
    fpga_info.load_time_ms = 100;
    fpga_info.load_count++;
    
    return FPGA_OK;
}

int fpga_write_bitstream(const uint8_t *data, uint32_t size)
{
    fpga_bitstream_header_t header;
    
    if (!fpga_initialized) {
        return FPGA_ERR_INIT;
    }
    
    if (!data || size == 0 || size > FPGA_BITSTREAM_MAX_SIZE) {
        return FPGA_ERR_INVALID_BITSTREAM;
    }
    
    log_message(LOG_INFO, "%s: Запись bitstream во flash (%u bytes)", TAG, (unsigned)size);
    
    /* Формируем заголовок */
    memset(&header, 0, sizeof(header));
    header.magic = FPGA_BITSTREAM_MAGIC;
    header.version = FPGA_HEADER_VERSION;
    header.size = size;
    header.crc32 = calc_crc32(data, size);
    header.timestamp = 0;
    strncpy(header.device, "XC7A35T", sizeof(header.device) - 1);
    strncpy(header.build_info, "A1126pro", sizeof(header.build_info) - 1);
    
    /* Стираем и записываем */
    w25qxx_erase_sector(FPGA_BITSTREAM_ADDR);
    w25qxx_write(FPGA_BITSTREAM_ADDR, (uint8_t *)&header, sizeof(header));
    
    /* Записываем данные порциями */
    uint32_t offset = FPGA_BITSTREAM_ADDR + sizeof(header);
    uint32_t remaining = size;
    const uint8_t *ptr = data;
    
    while (remaining > 0) {
        uint32_t to_write = (remaining > 256) ? 256 : remaining;
        w25qxx_write(offset, ptr, to_write);
        offset += to_write;
        ptr += to_write;
        remaining -= to_write;
    }
    
    /* Обновляем информацию */
    fpga_info.bitstream_size = size;
    fpga_info.bitstream_crc = header.crc32;
    fpga_info.bitstream_valid = true;
    
    log_message(LOG_INFO, "%s: Bitstream записан, CRC32=0x%08X", TAG, (unsigned)header.crc32);
    
    return FPGA_OK;
}

int fpga_erase_bitstream(void)
{
    if (!fpga_initialized) {
        return FPGA_ERR_INIT;
    }
    
    log_message(LOG_INFO, "%s: Стирание bitstream", TAG);
    
    /* Стираем первый сектор */
    w25qxx_erase_sector(FPGA_BITSTREAM_ADDR);
    
    fpga_info.bitstream_valid = false;
    fpga_info.bitstream_size = 0;
    fpga_info.bitstream_crc = 0;
    
    return FPGA_OK;
}

bool fpga_is_ready(void)
{
    if (!fpga_initialized) {
        return false;
    }
    
    return (fpga_info.state == FPGA_STATE_CONFIGURED);
}

const fpga_info_t *fpga_get_info(void)
{
    return &fpga_info;
}

const char *fpga_error_string(int error_code)
{
    switch (error_code) {
        case FPGA_OK:                   return "OK";
        case FPGA_ERR_INIT:             return "Не инициализирован";
        case FPGA_ERR_TIMEOUT:          return "Таймаут";
        case FPGA_ERR_FLASH_READ:       return "Ошибка чтения flash";
        case FPGA_ERR_INVALID_BITSTREAM:return "Неверный bitstream";
        case FPGA_ERR_VERIFY:           return "Ошибка верификации";
        case FPGA_ERR_NOT_DONE:         return "DONE не поднялся";
        case FPGA_ERR_NO_BITSTREAM:     return "Bitstream не найден";
        default:                        return "Неизвестная ошибка";
    }
}
