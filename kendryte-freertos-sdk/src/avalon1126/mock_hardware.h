/**
 * =============================================================================
 * @file    mock_hardware.h
 * @brief   Avalon A1126pro - Слой эмуляции оборудования для тестирования
 * @version 1.0
 * @date    2024
 * =============================================================================
 * 
 * ОПИСАНИЕ:
 * Заголовочный файл для эмуляции аппаратного обеспечения.
 * Позволяет тестировать прошивку без реального железа.
 * 
 * ИСПОЛЬЗОВАНИЕ:
 * Определите MOCK_HARDWARE=1 для включения эмуляции.
 * По умолчанию используется реальное оборудование.
 * 
 * =============================================================================
 */

#ifndef __MOCK_HARDWARE_H__
#define __MOCK_HARDWARE_H__

#include <stdint.h>
#include <stddef.h>

/* ===========================================================================
 * КОНФИГУРАЦИЯ ЭМУЛЯЦИИ
 * =========================================================================== */

#ifndef MOCK_HARDWARE
#define MOCK_HARDWARE       0       /* 0 = реальное железо, 1 = эмуляция */
#endif

#ifndef MOCK_NETWORK
#define MOCK_NETWORK        MOCK_HARDWARE
#endif

#ifndef MOCK_SPI_FLASH
#define MOCK_SPI_FLASH      MOCK_HARDWARE
#endif

#ifndef MOCK_ASIC
#define MOCK_ASIC           MOCK_HARDWARE
#endif

/* ===========================================================================
 * MOCK SPI FLASH (W25Q64)
 * =========================================================================== */

#if MOCK_SPI_FLASH

#define MOCK_FLASH_SIZE         (8 * 1024 * 1024)   /* 8 MB */
#define MOCK_FLASH_SECTOR_SIZE  4096
#define MOCK_FLASH_ID           0xEF4017            /* W25Q64 JEDEC ID */

/**
 * @brief Инициализация эмулятора Flash
 */
void mock_flash_init(void);

/**
 * @brief Чтение из эмулятора Flash
 */
int mock_flash_read(uint32_t addr, uint8_t *buf, uint32_t len);

/**
 * @brief Запись в эмулятор Flash
 */
int mock_flash_write(uint32_t addr, const uint8_t *buf, uint32_t len);

/**
 * @brief Стирание сектора в эмуляторе Flash
 */
int mock_flash_erase_sector(uint32_t addr);

/**
 * @brief Получить JEDEC ID
 */
uint32_t mock_flash_read_id(void);

#endif /* MOCK_SPI_FLASH */

/* ===========================================================================
 * MOCK NETWORK (TCP/IP)
 * =========================================================================== */

#if MOCK_NETWORK

#define MOCK_SOCKET_MAX         8

typedef struct mock_socket {
    int active;
    int connected;
    char host[256];
    int port;
    
    /* Буферы для эмуляции данных */
    uint8_t rx_buffer[4096];
    int rx_len;
    int rx_pos;
    
    uint8_t tx_buffer[4096];
    int tx_len;
} mock_socket_t;

/**
 * @brief Инициализация эмулятора сети
 */
void mock_network_init(void);

/**
 * @brief Создание mock сокета
 */
int mock_socket_create(void);

/**
 * @brief Подключение mock сокета
 */
int mock_socket_connect(int sock, const char *host, int port);

/**
 * @brief Отправка данных через mock сокет
 */
int mock_socket_send(int sock, const void *data, size_t len);

/**
 * @brief Приём данных из mock сокета
 */
int mock_socket_recv(int sock, void *buf, size_t len);

/**
 * @brief Закрытие mock сокета
 */
void mock_socket_close(int sock);

/**
 * @brief Внедрение данных для приёма (для тестирования)
 */
void mock_socket_inject_rx(int sock, const void *data, size_t len);

/**
 * @brief Получение отправленных данных (для тестирования)
 */
int mock_socket_get_tx(int sock, void *buf, size_t max_len);

#endif /* MOCK_NETWORK */

/* ===========================================================================
 * MOCK ASIC (Avalon10)
 * =========================================================================== */

#if MOCK_ASIC

#define MOCK_ASIC_MODULES           4
#define MOCK_ASIC_CHIPS             114
#define MOCK_ASIC_CHIPS_PER_MODULE  114
#define MOCK_ASIC_CORES_PER_CHIP    72
#define MOCK_NONCE_RATE_HZ          10      /* Частота генерации nonce */

typedef struct mock_asic_module {
    int detected;
    int enabled;
    int16_t temp_in;        /* Температура × 10 */
    int16_t temp_out;
    uint16_t freq;
    uint16_t voltage;
    uint8_t fan_speed;
    
    /* Счётчики для эмуляции */
    uint32_t nonce_counter;
    uint32_t last_nonce_time;
    
    /* Буфер последнего отправленного пакета */
    uint8_t last_tx_pkg[128];
    int last_tx_len;
} mock_asic_module_t;

/**
 * @brief Инициализация эмулятора ASIC
 */
void mock_asic_init(void);

/**
 * @brief Эмуляция SPI передачи
 */
int mock_asic_spi_transfer(int module_id, const uint8_t *tx, uint8_t *rx, size_t len);

/**
 * @brief Эмуляция отправки пакета на ASIC
 */
int mock_asic_send(int module_id, const uint8_t *data, size_t len);

/**
 * @brief Эмуляция приёма пакета от ASIC
 */
int mock_asic_recv(int module_id, uint8_t *data, size_t len, int timeout);

/**
 * @brief Установка температуры модуля (для тестирования)
 */
void mock_asic_set_temperature(int module_id, int16_t temp_in, int16_t temp_out);

/**
 * @brief Генерация фиктивного nonce
 */
int mock_asic_poll_nonce(int module_id, uint32_t *nonce);

/**
 * @brief Получение состояния mock модуля
 */
mock_asic_module_t *mock_asic_get_module(int module_id);

#endif /* MOCK_ASIC */

/* ===========================================================================
 * MOCK SHA256
 * =========================================================================== */

/**
 * @brief Software SHA256 для тестирования
 * Используется когда нет аппаратного SHA256
 */
void mock_sha256(const uint8_t *data, size_t len, uint8_t *hash);

/**
 * @brief Double SHA256 (SHA256(SHA256(data)))
 */
void mock_sha256d(const uint8_t *data, size_t len, uint8_t *hash);

/* ===========================================================================
 * ВСПОМОГАТЕЛЬНЫЕ ФУНКЦИИ
 * =========================================================================== */

/**
 * @brief Hex строка в байты
 */
int hex_to_bytes(const char *hex, uint8_t *bytes, size_t max_len);

/**
 * @brief Байты в hex строку
 */
void bytes_to_hex(const uint8_t *bytes, size_t len, char *hex);

/**
 * @brief Реверс байтов (для big-endian/little-endian)
 */
void reverse_bytes(uint8_t *data, size_t len);

#endif /* __MOCK_HARDWARE_H__ */
