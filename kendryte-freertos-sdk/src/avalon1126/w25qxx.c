/**
 * =============================================================================
 * @file    w25qxx.c
 * @brief   Avalon A1126pro - Драйвер W25Q64 SPI Flash (реализация)
 * @version 1.0
 * @date    2024
 * =============================================================================
 * 
 * ОПИСАНИЕ:
 * Реализация драйвера SPI флеш-памяти W25Q64.
 * 
 * КОМАНДЫ W25QXX:
 * - 0x9F - Read JEDEC ID
 * - 0x03 - Read Data
 * - 0x02 - Page Program
 * - 0x20 - Sector Erase (4KB)
 * - 0x52 - Block Erase 32KB
 * - 0xD8 - Block Erase 64KB
 * - 0x06 - Write Enable
 * - 0x04 - Write Disable
 * - 0x05 - Read Status Register 1
 * - 0x35 - Read Status Register 2
 * - 0x01 - Write Status Register
 * 
 * =============================================================================
 */

#include <stdio.h>
#include <string.h>

#include <FreeRTOS.h>
#include <task.h>

#include "w25qxx.h"
#include "cgminer.h"
#include "mock_hardware.h"

/* SDK SPI driver */
#ifndef MOCK_SPI_FLASH
#include <devices.h>
#include <hal.h>
#endif

static const char *TAG = "W25QXX";

/* ===========================================================================
 * КОНСТАНТЫ КОМАНД W25QXX
 * =========================================================================== */

#define W25QXX_CMD_WRITE_ENABLE         0x06
#define W25QXX_CMD_WRITE_DISABLE        0x04
#define W25QXX_CMD_READ_STATUS_REG1     0x05
#define W25QXX_CMD_READ_STATUS_REG2     0x35
#define W25QXX_CMD_WRITE_STATUS_REG     0x01
#define W25QXX_CMD_READ_DATA            0x03
#define W25QXX_CMD_FAST_READ            0x0B
#define W25QXX_CMD_PAGE_PROGRAM         0x02
#define W25QXX_CMD_SECTOR_ERASE         0x20    /* 4KB */
#define W25QXX_CMD_BLOCK_ERASE_32K      0x52
#define W25QXX_CMD_BLOCK_ERASE_64K      0xD8
#define W25QXX_CMD_CHIP_ERASE           0xC7
#define W25QXX_CMD_READ_JEDEC_ID        0x9F
#define W25QXX_CMD_POWER_DOWN           0xB9
#define W25QXX_CMD_RELEASE_POWER_DOWN   0xAB

/* Status Register 1 bits */
#define W25QXX_SR1_BUSY                 0x01
#define W25QXX_SR1_WEL                  0x02

/* ===========================================================================
 * ЛОКАЛЬНЫЕ ПЕРЕМЕННЫЕ
 * =========================================================================== */

#ifndef MOCK_SPI_FLASH
static handle_t spi_handle = 0;
static handle_t spi_device = 0;
#endif

static int w25qxx_initialized = 0;
static uint32_t w25qxx_chip_id = 0;

/* ===========================================================================
 * НИЗКОУРОВНЕВЫЕ ФУНКЦИИ SPI
 * =========================================================================== */

#if MOCK_SPI_FLASH

/* Mock режим - используем эмуляцию */
static void w25qxx_spi_init(void)
{
    mock_flash_init();
}

static uint8_t w25qxx_spi_read_byte(void)
{
    return 0xFF;
}

static void w25qxx_spi_write_byte(uint8_t data)
{
    (void)data;
}

static void w25qxx_spi_transfer(const uint8_t *tx, uint8_t *rx, size_t len)
{
    if (rx) {
        memset(rx, 0xFF, len);
    }
}

#else

/* Реальный режим - используем SDK */
static void w25qxx_spi_init(void)
{
    /* SPI0 для Flash, CS0 */
    spi_handle = io_open("/dev/spi0");
    if (spi_handle) {
        spi_device = spi_get_device(spi_handle, SPI_MODE_0, SPI_FF_STANDARD, 0, 8);
        spi_dev_set_clock_rate(spi_device, 25000000);  /* 25 MHz */
    }
}

static void w25qxx_cs_low(void)
{
    /* CS управляется автоматически драйвером */
}

static void w25qxx_cs_high(void)
{
    /* CS управляется автоматически драйвером */
}

static void w25qxx_spi_transfer(const uint8_t *tx, uint8_t *rx, size_t len)
{
    if (spi_device) {
        spi_dev_transfer_sequential(spi_device, tx, len, rx, len);
    }
}

#endif /* MOCK_SPI_FLASH */

/* ===========================================================================
 * ВНУТРЕННИЕ ФУНКЦИИ
 * =========================================================================== */

/**
 * @brief Ожидание завершения операции записи/стирания
 */
static void w25qxx_wait_busy(void)
{
#if MOCK_SPI_FLASH
    /* Mock - не ждём */
    return;
#else
    uint8_t cmd = W25QXX_CMD_READ_STATUS_REG1;
    uint8_t status;
    int timeout = 10000;  /* 10 секунд максимум */
    
    do {
        spi_dev_transfer_sequential(spi_device, &cmd, 1, &status, 1);
        if (!(status & W25QXX_SR1_BUSY)) {
            break;
        }
        vTaskDelay(pdMS_TO_TICKS(1));
        timeout--;
    } while (timeout > 0);
    
    if (timeout == 0) {
        log_message(LOG_WARNING, "%s: Таймаут ожидания", TAG);
    }
#endif
}

/**
 * @brief Включение записи
 */
static void w25qxx_write_enable(void)
{
#if !MOCK_SPI_FLASH
    uint8_t cmd = W25QXX_CMD_WRITE_ENABLE;
    spi_dev_write(spi_device, &cmd, 1);
#endif
}

/* ===========================================================================
 * ПУБЛИЧНЫЕ ФУНКЦИИ
 * =========================================================================== */

/**
 * @brief Инициализация драйвера W25QXX
 */
int w25qxx_init(void)
{
    log_message(LOG_INFO, "%s: Инициализация...", TAG);
    
    /* Инициализация SPI */
    w25qxx_spi_init();
    
    /* Читаем JEDEC ID */
    w25qxx_chip_id = w25qxx_read_id();
    
    if (w25qxx_chip_id == 0 || w25qxx_chip_id == 0xFFFFFF) {
        log_message(LOG_ERR, "%s: Flash не обнаружен!", TAG);
        return -1;
    }
    
    /* Определяем тип чипа */
    const char *chip_name = "Unknown";
    switch (w25qxx_chip_id) {
        case 0xEF4014: chip_name = "W25Q80"; break;
        case 0xEF4015: chip_name = "W25Q16"; break;
        case 0xEF4016: chip_name = "W25Q32"; break;
        case 0xEF4017: chip_name = "W25Q64"; break;
        case 0xEF4018: chip_name = "W25Q128"; break;
        case 0xEF4019: chip_name = "W25Q256"; break;
    }
    
    log_message(LOG_INFO, "%s: Обнаружен %s (ID=0x%06X)", TAG, chip_name, w25qxx_chip_id);
    
    w25qxx_initialized = 1;
    return 0;
}

/**
 * @brief Чтение JEDEC ID
 */
uint32_t w25qxx_read_id(void)
{
#if MOCK_SPI_FLASH
    return mock_flash_read_id();
#else
    uint8_t tx[4] = { W25QXX_CMD_READ_JEDEC_ID, 0, 0, 0 };
    uint8_t rx[4] = { 0 };
    
    if (!spi_device) return 0;
    
    spi_dev_transfer_sequential(spi_device, tx, 4, rx, 4);
    
    return ((uint32_t)rx[1] << 16) | ((uint32_t)rx[2] << 8) | rx[3];
#endif
}

/**
 * @brief Чтение данных из Flash
 */
int w25qxx_read(uint32_t addr, uint8_t *buf, uint32_t len)
{
    if (!buf || !len) return -1;
    if (addr + len > W25QXX_TOTAL_SIZE) return -1;
    
#if MOCK_SPI_FLASH
    return mock_flash_read(addr, buf, len);
#else
    if (!spi_device) return -1;
    
    w25qxx_wait_busy();
    
    uint8_t cmd[4] = {
        W25QXX_CMD_READ_DATA,
        (addr >> 16) & 0xFF,
        (addr >> 8) & 0xFF,
        addr & 0xFF
    };
    
    /* Отправляем команду и читаем данные в одной транзакции */
    spi_dev_transfer_sequential(spi_device, cmd, 4, buf, len);
    
    return 0;
#endif
}

/**
 * @brief Запись данных в Flash (с автоматическим разбиением на страницы)
 */
int w25qxx_write(uint32_t addr, const uint8_t *buf, uint32_t len)
{
    if (!buf || !len) return -1;
    if (addr + len > W25QXX_TOTAL_SIZE) return -1;
    
#if MOCK_SPI_FLASH
    return mock_flash_write(addr, buf, len);
#else
    if (!spi_device) return -1;
    
    uint32_t remaining = len;
    uint32_t offset = 0;
    
    while (remaining > 0) {
        /* Вычисляем размер текущей записи (не более страницы, не пересекая границу) */
        uint32_t page_offset = (addr + offset) % W25QXX_PAGE_SIZE;
        uint32_t to_write = W25QXX_PAGE_SIZE - page_offset;
        if (to_write > remaining) to_write = remaining;
        
        w25qxx_wait_busy();
        w25qxx_write_enable();
        
        uint8_t cmd[4] = {
            W25QXX_CMD_PAGE_PROGRAM,
            ((addr + offset) >> 16) & 0xFF,
            ((addr + offset) >> 8) & 0xFF,
            (addr + offset) & 0xFF
        };
        
        /* Отправляем команду + данные */
        spi_dev_write(spi_device, cmd, 4);
        spi_dev_write(spi_device, buf + offset, to_write);
        
        offset += to_write;
        remaining -= to_write;
    }
    
    w25qxx_wait_busy();
    return 0;
#endif
}

/**
 * @brief Стирание сектора (4KB)
 */
int w25qxx_erase_sector(uint32_t addr)
{
    /* Выравниваем по границе сектора */
    addr &= ~(W25QXX_SECTOR_SIZE - 1);
    
    if (addr >= W25QXX_TOTAL_SIZE) return -1;
    
#if MOCK_SPI_FLASH
    return mock_flash_erase_sector(addr);
#else
    if (!spi_device) return -1;
    
    log_message(LOG_DEBUG, "%s: Стирание сектора 0x%06X", TAG, addr);
    
    w25qxx_wait_busy();
    w25qxx_write_enable();
    
    uint8_t cmd[4] = {
        W25QXX_CMD_SECTOR_ERASE,
        (addr >> 16) & 0xFF,
        (addr >> 8) & 0xFF,
        addr & 0xFF
    };
    
    spi_dev_write(spi_device, cmd, 4);
    w25qxx_wait_busy();
    
    return 0;
#endif
}

/**
 * @brief Стирание блока (64KB)
 */
int w25qxx_erase_block(uint32_t addr)
{
    /* Выравниваем по границе блока */
    addr &= ~(65536 - 1);
    
    if (addr >= W25QXX_TOTAL_SIZE) return -1;
    
#if MOCK_SPI_FLASH
    /* Стираем 16 секторов */
    for (int i = 0; i < 16; i++) {
        mock_flash_erase_sector(addr + i * W25QXX_SECTOR_SIZE);
    }
    return 0;
#else
    if (!spi_device) return -1;
    
    log_message(LOG_DEBUG, "%s: Стирание блока 0x%06X", TAG, addr);
    
    w25qxx_wait_busy();
    w25qxx_write_enable();
    
    uint8_t cmd[4] = {
        W25QXX_CMD_BLOCK_ERASE_64K,
        (addr >> 16) & 0xFF,
        (addr >> 8) & 0xFF,
        addr & 0xFF
    };
    
    spi_dev_write(spi_device, cmd, 4);
    w25qxx_wait_busy();
    
    return 0;
#endif
}

/**
 * @brief Полное стирание чипа
 */
int w25qxx_erase_chip(void)
{
    log_message(LOG_INFO, "%s: Полное стирание чипа...", TAG);
    
#if MOCK_SPI_FLASH
    for (uint32_t addr = 0; addr < W25QXX_TOTAL_SIZE; addr += W25QXX_SECTOR_SIZE) {
        mock_flash_erase_sector(addr);
    }
    return 0;
#else
    if (!spi_device) return -1;
    
    w25qxx_wait_busy();
    w25qxx_write_enable();
    
    uint8_t cmd = W25QXX_CMD_CHIP_ERASE;
    spi_dev_write(spi_device, &cmd, 1);
    
    /* Полное стирание занимает до 200 секунд */
    w25qxx_wait_busy();
    
    log_message(LOG_INFO, "%s: Стирание завершено", TAG);
    return 0;
#endif
}

/**
 * @brief Проверка инициализации
 */
int w25qxx_is_initialized(void)
{
    return w25qxx_initialized;
}

/**
 * @brief Получение ID чипа
 */
uint32_t w25qxx_get_chip_id(void)
{
    return w25qxx_chip_id;
}

/* ===========================================================================
 * КОНЕЦ ФАЙЛА w25qxx.c
 * =========================================================================== */
