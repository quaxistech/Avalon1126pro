/**
 * =============================================================================
 * @file    flash.c
 * @brief   Драйвер Flash памяти для Kendryte K210
 * =============================================================================
 * 
 * Этот файл реализует работу с SPI Flash памятью:
 * - Чтение/запись/стирание секторов
 * - OTA обновление прошивки
 * - Хранение конфигурации
 * - Резервное копирование
 * 
 * K210 использует SPI Flash с интерфейсом XIP (Execute In Place).
 * Прошивка выполняется непосредственно из Flash.
 * 
 * @author  Reconstructed from Avalon A1126pro firmware
 * @version 1.0
 * =============================================================================
 */

#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include "flash.h"
#include "k210_hal.h"

/* =============================================================================
 * РЕГИСТРЫ SPI FLASH
 * ============================================================================= */

/**
 * Базовый адрес контроллера SPI3 (для Flash)
 */
#define SPI3_BASE               0x52000000

/**
 * Регистры SPI контроллера
 */
#define SPI3_CTRLR0             (*(volatile uint32_t *)(SPI3_BASE + 0x00))
#define SPI3_CTRLR1             (*(volatile uint32_t *)(SPI3_BASE + 0x04))
#define SPI3_SSIENR             (*(volatile uint32_t *)(SPI3_BASE + 0x08))
#define SPI3_SER                (*(volatile uint32_t *)(SPI3_BASE + 0x10))
#define SPI3_BAUDR              (*(volatile uint32_t *)(SPI3_BASE + 0x14))
#define SPI3_TXFTLR             (*(volatile uint32_t *)(SPI3_BASE + 0x18))
#define SPI3_RXFTLR             (*(volatile uint32_t *)(SPI3_BASE + 0x1C))
#define SPI3_TXFLR              (*(volatile uint32_t *)(SPI3_BASE + 0x20))
#define SPI3_RXFLR              (*(volatile uint32_t *)(SPI3_BASE + 0x24))
#define SPI3_SR                 (*(volatile uint32_t *)(SPI3_BASE + 0x28))
#define SPI3_IMR                (*(volatile uint32_t *)(SPI3_BASE + 0x2C))
#define SPI3_ISR                (*(volatile uint32_t *)(SPI3_BASE + 0x30))
#define SPI3_DMACR              (*(volatile uint32_t *)(SPI3_BASE + 0x4C))
#define SPI3_DR                 (*(volatile uint32_t *)(SPI3_BASE + 0x60))
#define SPI3_XIP_CTRL           (*(volatile uint32_t *)(SPI3_BASE + 0xF0))

/* =============================================================================
 * КОМАНДЫ SPI FLASH
 * ============================================================================= */

/**
 * Стандартные команды SPI Flash (совместимы с W25Q64)
 */
#define CMD_WRITE_ENABLE        0x06    /* Разрешить запись */
#define CMD_WRITE_DISABLE       0x04    /* Запретить запись */
#define CMD_READ_STATUS_REG1    0x05    /* Читать регистр статуса 1 */
#define CMD_READ_STATUS_REG2    0x35    /* Читать регистр статуса 2 */
#define CMD_WRITE_STATUS_REG    0x01    /* Записать регистр статуса */
#define CMD_READ_DATA           0x03    /* Читать данные */
#define CMD_FAST_READ           0x0B    /* Быстрое чтение */
#define CMD_DUAL_READ           0x3B    /* Двойное чтение */
#define CMD_QUAD_READ           0x6B    /* Четверное чтение */
#define CMD_PAGE_PROGRAM        0x02    /* Программирование страницы */
#define CMD_SECTOR_ERASE        0x20    /* Стереть сектор (4KB) */
#define CMD_BLOCK_ERASE_32K     0x52    /* Стереть блок (32KB) */
#define CMD_BLOCK_ERASE_64K     0xD8    /* Стереть блок (64KB) */
#define CMD_CHIP_ERASE          0xC7    /* Стереть весь чип */
#define CMD_READ_ID             0x9F    /* Читать JEDEC ID */
#define CMD_READ_UID            0x4B    /* Читать уникальный ID */
#define CMD_POWER_DOWN          0xB9    /* Режим энергосбережения */
#define CMD_RELEASE_PD          0xAB    /* Выход из энергосбережения */
#define CMD_ENABLE_RESET        0x66    /* Разрешить сброс */
#define CMD_RESET_DEVICE        0x99    /* Сброс устройства */

/**
 * Биты регистра статуса
 */
#define STATUS_BUSY             0x01    /* Операция выполняется */
#define STATUS_WEL              0x02    /* Запись разрешена */
#define STATUS_BP0              0x04    /* Block Protect 0 */
#define STATUS_BP1              0x08    /* Block Protect 1 */
#define STATUS_BP2              0x10    /* Block Protect 2 */
#define STATUS_TB               0x20    /* Top/Bottom Protect */
#define STATUS_SEC              0x40    /* Sector Protect */
#define STATUS_SRP              0x80    /* Status Register Protect */

/* =============================================================================
 * КОНСТАНТЫ РАЗМЕТКИ FLASH
 * ============================================================================= */

/**
 * Размеры областей Flash памяти
 */
#define FLASH_SIZE              (8 * 1024 * 1024)       /* 8 МБ */
#define FLASH_SECTOR_SIZE       4096                     /* 4 КБ */
#define FLASH_PAGE_SIZE         256                      /* 256 байт */
#define FLASH_BLOCK_SIZE        (64 * 1024)              /* 64 КБ */

/**
 * Адреса XIP (Execute In Place)
 * Flash отображается в адресное пространство начиная с 0x90000000
 */
#define XIP_BASE                0x90000000

/* =============================================================================
 * ГЛОБАЛЬНЫЕ ПЕРЕМЕННЫЕ
 * ============================================================================= */

/**
 * Информация о Flash чипе
 */
static flash_info_t g_flash_info;

/**
 * Флаг инициализации
 */
static bool g_flash_initialized = false;

/* =============================================================================
 * НИЗКОУРОВНЕВЫЕ ФУНКЦИИ SPI
 * ============================================================================= */

/**
 * @brief   Выбор Flash чипа (активация CS)
 */
static void flash_select(void) {
    SPI3_SER = 0x01;    /* Активируем CS0 */
}

/**
 * @brief   Освобождение Flash чипа (деактивация CS)
 */
static void flash_deselect(void) {
    SPI3_SER = 0x00;    /* Деактивируем все CS */
}

/**
 * @brief   Отправка байта по SPI
 * @param   data: байт для отправки
 * @return  полученный байт
 */
static uint8_t spi_transfer(uint8_t data) {
    /* Ждём освобождения TX FIFO */
    while (SPI3_TXFLR >= 32);
    
    /* Отправляем байт */
    SPI3_DR = data;
    
    /* Ждём получения данных */
    while (SPI3_RXFLR == 0);
    
    /* Читаем и возвращаем ответ */
    return (uint8_t)SPI3_DR;
}

/**
 * @brief   Чтение регистра статуса Flash
 * @return  значение регистра статуса
 */
static uint8_t flash_read_status(void) {
    uint8_t status;
    
    flash_select();
    spi_transfer(CMD_READ_STATUS_REG1);
    status = spi_transfer(0xFF);
    flash_deselect();
    
    return status;
}

/**
 * @brief   Ожидание завершения операции Flash
 * @param   timeout_ms: тайм-аут в миллисекундах
 * @return  true если операция завершена, false при тайм-ауте
 */
static bool flash_wait_ready(uint32_t timeout_ms) {
    uint32_t start = hal_get_tick();
    
    while ((hal_get_tick() - start) < timeout_ms) {
        if ((flash_read_status() & STATUS_BUSY) == 0) {
            return true;
        }
        hal_delay_us(100);
    }
    
    return false;
}

/**
 * @brief   Разрешение записи во Flash
 */
static void flash_write_enable(void) {
    flash_select();
    spi_transfer(CMD_WRITE_ENABLE);
    flash_deselect();
    
    /* Ждём установки флага WEL */
    while ((flash_read_status() & STATUS_WEL) == 0);
}

/* =============================================================================
 * ИНИЦИАЛИЗАЦИЯ
 * ============================================================================= */

/**
 * @brief   Инициализация Flash памяти
 * @return  0 при успехе, код ошибки при неудаче
 * 
 * Выполняет:
 * 1. Настройку SPI контроллера
 * 2. Чтение ID микросхемы
 * 3. Определение размера и параметров
 */
int flash_init(void) {
    uint8_t id[3];
    
    /*
     * Настройка SPI3 для работы с Flash
     * - Режим SPI: Mode 0 (CPOL=0, CPHA=0)
     * - Размер кадра: 8 бит
     * - Частота: системная / 4
     */
    SPI3_SSIENR = 0;    /* Отключаем SPI для настройки */
    
    SPI3_CTRLR0 = (0x07 << 16) |    /* Размер кадра = 8 бит */
                  (0x00 << 6);       /* Режим передачи = только TX/RX */
    
    SPI3_BAUDR = 4;     /* Делитель частоты */
    
    SPI3_TXFTLR = 0;    /* Порог TX FIFO */
    SPI3_RXFTLR = 0;    /* Порог RX FIFO */
    
    SPI3_IMR = 0;       /* Отключаем все прерывания */
    
    SPI3_SSIENR = 1;    /* Включаем SPI */
    
    /*
     * Выход из режима энергосбережения (на случай если был активен)
     */
    flash_select();
    spi_transfer(CMD_RELEASE_PD);
    flash_deselect();
    hal_delay_us(50);   /* Ждём пробуждения */
    
    /*
     * Чтение JEDEC ID
     */
    flash_select();
    spi_transfer(CMD_READ_ID);
    id[0] = spi_transfer(0xFF);  /* Manufacturer ID */
    id[1] = spi_transfer(0xFF);  /* Memory Type */
    id[2] = spi_transfer(0xFF);  /* Capacity */
    flash_deselect();
    
    /*
     * Сохраняем информацию о чипе
     */
    g_flash_info.manufacturer_id = id[0];
    g_flash_info.memory_type = id[1];
    g_flash_info.capacity_id = id[2];
    
    /*
     * Определяем размер по ID ёмкости
     * 0x14 = 1MB, 0x15 = 2MB, 0x16 = 4MB, 0x17 = 8MB, 0x18 = 16MB
     */
    if (id[2] >= 0x14 && id[2] <= 0x18) {
        g_flash_info.total_size = 1 << (id[2] - 0x14 + 20);
    } else {
        g_flash_info.total_size = FLASH_SIZE;   /* По умолчанию 8MB */
    }
    
    g_flash_info.sector_size = FLASH_SECTOR_SIZE;
    g_flash_info.page_size = FLASH_PAGE_SIZE;
    g_flash_info.block_size = FLASH_BLOCK_SIZE;
    g_flash_info.sector_count = g_flash_info.total_size / FLASH_SECTOR_SIZE;
    
    g_flash_initialized = true;
    
    return 0;
}

/* =============================================================================
 * ЧТЕНИЕ ДАННЫХ
 * ============================================================================= */

/**
 * @brief   Чтение данных из Flash
 * @param   address: адрес во Flash
 * @param   data: буфер для данных
 * @param   length: количество байт
 * @return  количество прочитанных байт
 * 
 * Использует команду быстрого чтения для максимальной скорости.
 */
int flash_read(uint32_t address, void *data, size_t length) {
    uint8_t *buf = (uint8_t *)data;
    size_t i;
    
    if (!g_flash_initialized) {
        return -1;
    }
    
    if (address + length > g_flash_info.total_size) {
        return -2;  /* Выход за границы */
    }
    
    /*
     * Для XIP-режима можно читать напрямую из памяти
     * Это быстрее чем через SPI команды
     */
#if 1
    memcpy(data, (void *)(XIP_BASE + address), length);
    return length;
#else
    /*
     * Альтернативный метод - чтение через SPI команды
     */
    flash_select();
    
    /* Команда быстрого чтения */
    spi_transfer(CMD_FAST_READ);
    
    /* 24-битный адрес */
    spi_transfer((address >> 16) & 0xFF);
    spi_transfer((address >> 8) & 0xFF);
    spi_transfer(address & 0xFF);
    
    /* Фиктивный байт для быстрого чтения */
    spi_transfer(0xFF);
    
    /* Чтение данных */
    for (i = 0; i < length; i++) {
        buf[i] = spi_transfer(0xFF);
    }
    
    flash_deselect();
    
    return length;
#endif
}

/* =============================================================================
 * ЗАПИСЬ ДАННЫХ
 * ============================================================================= */

/**
 * @brief   Запись страницы во Flash
 * @param   address: адрес страницы (выровнен по 256 байт)
 * @param   data: данные для записи
 * @param   length: количество байт (макс 256)
 * @return  количество записанных байт
 * 
 * Flash можно записывать только постранично (256 байт).
 * Перед записью сектор должен быть очищен!
 */
static int flash_write_page(uint32_t address, const void *data, size_t length) {
    const uint8_t *buf = (const uint8_t *)data;
    size_t i;
    
    if (length > FLASH_PAGE_SIZE) {
        length = FLASH_PAGE_SIZE;
    }
    
    /*
     * Разрешаем запись
     */
    flash_write_enable();
    
    /*
     * Отправляем команду программирования
     */
    flash_select();
    
    spi_transfer(CMD_PAGE_PROGRAM);
    
    /* 24-битный адрес */
    spi_transfer((address >> 16) & 0xFF);
    spi_transfer((address >> 8) & 0xFF);
    spi_transfer(address & 0xFF);
    
    /* Данные */
    for (i = 0; i < length; i++) {
        spi_transfer(buf[i]);
    }
    
    flash_deselect();
    
    /*
     * Ждём завершения записи (типично 0.7мс, макс 3мс)
     */
    if (!flash_wait_ready(10)) {
        return -1;
    }
    
    return length;
}

/**
 * @brief   Запись данных во Flash
 * @param   address: начальный адрес
 * @param   data: данные для записи
 * @param   length: количество байт
 * @return  количество записанных байт, <0 при ошибке
 * 
 * Автоматически разбивает запись на страницы.
 * ВНИМАНИЕ: Сектора должны быть предварительно очищены!
 */
int flash_write(uint32_t address, const void *data, size_t length) {
    const uint8_t *buf = (const uint8_t *)data;
    size_t written = 0;
    size_t chunk;
    int result;
    
    if (!g_flash_initialized) {
        return -1;
    }
    
    if (address + length > g_flash_info.total_size) {
        return -2;
    }
    
    while (written < length) {
        /*
         * Вычисляем размер записи с учётом выравнивания по странице
         */
        chunk = FLASH_PAGE_SIZE - (address % FLASH_PAGE_SIZE);
        if (chunk > (length - written)) {
            chunk = length - written;
        }
        
        result = flash_write_page(address, buf, chunk);
        if (result < 0) {
            return result;
        }
        
        address += chunk;
        buf += chunk;
        written += chunk;
    }
    
    return written;
}

/* =============================================================================
 * СТИРАНИЕ ДАННЫХ
 * ============================================================================= */

/**
 * @brief   Стирание сектора Flash (4KB)
 * @param   address: адрес внутри сектора
 * @return  0 при успехе, <0 при ошибке
 * 
 * Стирает весь сектор, содержащий указанный адрес.
 * Типичное время: 45мс, максимум 400мс.
 */
int flash_erase_sector(uint32_t address) {
    if (!g_flash_initialized) {
        return -1;
    }
    
    /* Выравниваем адрес по границе сектора */
    address &= ~(FLASH_SECTOR_SIZE - 1);
    
    if (address >= g_flash_info.total_size) {
        return -2;
    }
    
    flash_write_enable();
    
    flash_select();
    
    spi_transfer(CMD_SECTOR_ERASE);
    
    spi_transfer((address >> 16) & 0xFF);
    spi_transfer((address >> 8) & 0xFF);
    spi_transfer(address & 0xFF);
    
    flash_deselect();
    
    /* Ждём завершения стирания */
    if (!flash_wait_ready(500)) {
        return -3;
    }
    
    return 0;
}

/**
 * @brief   Стирание блока Flash (64KB)
 * @param   address: адрес внутри блока
 * @return  0 при успехе, <0 при ошибке
 */
int flash_erase_block(uint32_t address) {
    if (!g_flash_initialized) {
        return -1;
    }
    
    address &= ~(FLASH_BLOCK_SIZE - 1);
    
    if (address >= g_flash_info.total_size) {
        return -2;
    }
    
    flash_write_enable();
    
    flash_select();
    
    spi_transfer(CMD_BLOCK_ERASE_64K);
    
    spi_transfer((address >> 16) & 0xFF);
    spi_transfer((address >> 8) & 0xFF);
    spi_transfer(address & 0xFF);
    
    flash_deselect();
    
    /* Блок стирается дольше: типично 150мс, макс 2с */
    if (!flash_wait_ready(3000)) {
        return -3;
    }
    
    return 0;
}

/**
 * @brief   Стирание всего чипа Flash
 * @return  0 при успехе, <0 при ошибке
 * 
 * ВНИМАНИЕ: Операция занимает много времени (до 200 секунд для 16MB)!
 */
int flash_erase_chip(void) {
    if (!g_flash_initialized) {
        return -1;
    }
    
    flash_write_enable();
    
    flash_select();
    spi_transfer(CMD_CHIP_ERASE);
    flash_deselect();
    
    /* Ждём очень долго */
    if (!flash_wait_ready(200000)) {
        return -2;
    }
    
    return 0;
}

/* =============================================================================
 * OTA ОБНОВЛЕНИЕ
 * ============================================================================= */

/**
 * @brief   Запись образа прошивки в резервный слот
 * @param   image: указатель на образ
 * @param   size: размер образа в байтах
 * @return  0 при успехе, <0 при ошибке
 * 
 * Записывает новую прошивку в слот B.
 * После успешной записи нужно вызвать flash_ota_switch().
 */
int flash_ota_write(const void *image, size_t size) {
    uint32_t address = FLASH_SLOT_B_OFFSET;
    uint32_t end = address + size;
    const uint8_t *data = (const uint8_t *)image;
    size_t written = 0;
    
    if (!g_flash_initialized) {
        return -1;
    }
    
    /* Проверяем размер */
    if (size > FLASH_SLOT_SIZE) {
        return -2;  /* Образ слишком большой */
    }
    
    /*
     * Стираем блоки в слоте B
     */
    while (address < end) {
        int result = flash_erase_block(address);
        if (result < 0) {
            return result;
        }
        address += FLASH_BLOCK_SIZE;
        
        /* Сбрасываем watchdog */
        hal_watchdog_feed();
    }
    
    /*
     * Записываем образ
     */
    address = FLASH_SLOT_B_OFFSET;
    while (written < size) {
        size_t chunk = (size - written > FLASH_PAGE_SIZE) ? 
                       FLASH_PAGE_SIZE : (size - written);
        
        int result = flash_write_page(address, data, chunk);
        if (result < 0) {
            return result;
        }
        
        address += chunk;
        data += chunk;
        written += chunk;
        
        /* Периодически сбрасываем watchdog */
        if ((written % FLASH_BLOCK_SIZE) == 0) {
            hal_watchdog_feed();
        }
    }
    
    /*
     * Верификация записи
     */
    const uint8_t *src = (const uint8_t *)image;
    const uint8_t *dst = (const uint8_t *)(XIP_BASE + FLASH_SLOT_B_OFFSET);
    
    for (size_t i = 0; i < size; i++) {
        if (src[i] != dst[i]) {
            return -3;  /* Ошибка верификации */
        }
    }
    
    return 0;
}

/**
 * @brief   Переключение на новую прошивку
 * @return  0 при успехе (но функция не вернётся при успехе)
 * 
 * Устанавливает флаг загрузки из слота B и перезагружает систему.
 */
int flash_ota_switch(void) {
    ota_header_t header;
    
    /*
     * Читаем заголовок из слота B для проверки
     */
    flash_read(FLASH_SLOT_B_OFFSET, &header, sizeof(header));
    
    /* Проверяем магическое число */
    if (header.magic != OTA_MAGIC) {
        return -1;  /* Некорректный образ */
    }
    
    /*
     * Записываем флаг OTA в конфигурацию
     */
    uint32_t ota_flag = OTA_FLAG_PENDING;
    flash_erase_sector(FLASH_CONFIG_OFFSET);
    flash_write(FLASH_CONFIG_OFFSET + FLASH_OTA_FLAG_OFFSET, 
                &ota_flag, sizeof(ota_flag));
    
    /*
     * Перезагрузка системы
     * После перезагрузки bootloader загрузит образ из слота B
     */
    hal_system_reset();
    
    /* Сюда не дойдём */
    return 0;
}

/**
 * @brief   Подтверждение успешного обновления
 * @return  0 при успехе
 * 
 * Вызывается после успешной загрузки новой прошивки.
 * Копирует образ из слота B в слот A.
 */
int flash_ota_commit(void) {
    uint32_t ota_flag;
    
    /*
     * Проверяем флаг OTA
     */
    flash_read(FLASH_CONFIG_OFFSET + FLASH_OTA_FLAG_OFFSET, 
               &ota_flag, sizeof(ota_flag));
    
    if (ota_flag != OTA_FLAG_PENDING) {
        return 0;   /* Нет ожидающего обновления */
    }
    
    /*
     * Копируем слот B в слот A
     */
    ota_header_t header;
    flash_read(FLASH_SLOT_B_OFFSET, &header, sizeof(header));
    
    size_t size = header.image_size + sizeof(header);
    uint32_t src_addr = FLASH_SLOT_B_OFFSET;
    uint32_t dst_addr = FLASH_SLOT_A_OFFSET;
    uint8_t buffer[FLASH_PAGE_SIZE];
    
    /* Стираем слот A */
    for (uint32_t addr = dst_addr; addr < dst_addr + size; 
         addr += FLASH_BLOCK_SIZE) {
        flash_erase_block(addr);
        hal_watchdog_feed();
    }
    
    /* Копируем данные */
    for (size_t offset = 0; offset < size; offset += FLASH_PAGE_SIZE) {
        size_t chunk = (size - offset > FLASH_PAGE_SIZE) ? 
                       FLASH_PAGE_SIZE : (size - offset);
        
        flash_read(src_addr + offset, buffer, chunk);
        flash_write(dst_addr + offset, buffer, chunk);
        
        if ((offset % FLASH_BLOCK_SIZE) == 0) {
            hal_watchdog_feed();
        }
    }
    
    /*
     * Сбрасываем флаг OTA
     */
    ota_flag = OTA_FLAG_NONE;
    flash_erase_sector(FLASH_CONFIG_OFFSET);
    flash_write(FLASH_CONFIG_OFFSET + FLASH_OTA_FLAG_OFFSET, 
                &ota_flag, sizeof(ota_flag));
    
    return 0;
}

/* =============================================================================
 * КОНФИГУРАЦИЯ
 * ============================================================================= */

/**
 * @brief   Чтение конфигурации из Flash
 * @param   config: указатель на структуру конфигурации
 * @return  0 при успехе, <0 при ошибке
 */
int flash_config_read(miner_config_t *config) {
    uint32_t magic;
    
    if (!g_flash_initialized) {
        return -1;
    }
    
    /* Читаем магическое число */
    flash_read(FLASH_CONFIG_OFFSET, &magic, sizeof(magic));
    
    if (magic != CONFIG_MAGIC) {
        return -2;  /* Конфигурация не найдена или повреждена */
    }
    
    /* Читаем конфигурацию */
    flash_read(FLASH_CONFIG_OFFSET + sizeof(magic), 
               config, sizeof(miner_config_t));
    
    /* Проверяем контрольную сумму */
    uint32_t crc;
    flash_read(FLASH_CONFIG_OFFSET + sizeof(magic) + sizeof(miner_config_t), 
               &crc, sizeof(crc));
    
    uint32_t calc_crc = crc32(config, sizeof(miner_config_t));
    if (crc != calc_crc) {
        return -3;  /* CRC не совпадает */
    }
    
    return 0;
}

/**
 * @brief   Запись конфигурации во Flash
 * @param   config: указатель на структуру конфигурации
 * @return  0 при успехе, <0 при ошибке
 */
int flash_config_write(const miner_config_t *config) {
    uint32_t magic = CONFIG_MAGIC;
    uint32_t crc;
    
    if (!g_flash_initialized) {
        return -1;
    }
    
    /* Вычисляем контрольную сумму */
    crc = crc32(config, sizeof(miner_config_t));
    
    /* Стираем сектор конфигурации */
    int result = flash_erase_sector(FLASH_CONFIG_OFFSET);
    if (result < 0) {
        return result;
    }
    
    /* Записываем магическое число */
    result = flash_write(FLASH_CONFIG_OFFSET, &magic, sizeof(magic));
    if (result < 0) {
        return result;
    }
    
    /* Записываем конфигурацию */
    result = flash_write(FLASH_CONFIG_OFFSET + sizeof(magic), 
                        config, sizeof(miner_config_t));
    if (result < 0) {
        return result;
    }
    
    /* Записываем контрольную сумму */
    result = flash_write(FLASH_CONFIG_OFFSET + sizeof(magic) + 
                        sizeof(miner_config_t), &crc, sizeof(crc));
    
    return result;
}

/**
 * @brief   Сброс конфигурации на значения по умолчанию
 * @return  0 при успехе
 */
int flash_config_reset(void) {
    miner_config_t default_config = {
        .version = CONFIG_VERSION,
        .pool = {
            .url = "stratum+tcp://pool.example.com:3333",
            .worker = "worker1",
            .password = "x"
        },
        .network = {
            .dhcp = 1,
            .ip = {192, 168, 1, 100},
            .netmask = {255, 255, 255, 0},
            .gateway = {192, 168, 1, 1},
            .dns = {8, 8, 8, 8}
        },
        .miner = {
            .frequency = 700,
            .voltage = 850,
            .fan_speed = 80,
            .auto_fan = 1,
            .target_temp = 75
        }
    };
    
    return flash_config_write(&default_config);
}

/* =============================================================================
 * УТИЛИТЫ
 * ============================================================================= */

/**
 * @brief   Получение информации о Flash
 * @return  указатель на структуру с информацией
 */
const flash_info_t *flash_get_info(void) {
    return &g_flash_info;
}

/**
 * @brief   Вычисление CRC32
 * @param   data: данные
 * @param   length: длина данных
 * @return  CRC32
 */
static uint32_t crc32(const void *data, size_t length) {
    const uint8_t *buf = (const uint8_t *)data;
    uint32_t crc = 0xFFFFFFFF;
    
    for (size_t i = 0; i < length; i++) {
        crc ^= buf[i];
        for (int j = 0; j < 8; j++) {
            if (crc & 1) {
                crc = (crc >> 1) ^ 0xEDB88320;
            } else {
                crc >>= 1;
            }
        }
    }
    
    return ~crc;
}
