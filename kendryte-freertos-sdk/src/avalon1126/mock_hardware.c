/**
 * =============================================================================
 * @file    mock_hardware.c
 * @brief   Avalon A1126pro - Реализация эмуляции оборудования
 * @version 1.0
 * @date    2024
 * =============================================================================
 * 
 * ОПИСАНИЕ:
 * Реализация функций эмуляции для тестирования без реального железа.
 * 
 * =============================================================================
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "mock_hardware.h"
#include "cgminer.h"

static const char *TAG = "Mock";

/* ===========================================================================
 * MOCK FLASH IMPLEMENTATION
 * =========================================================================== */

#if MOCK_SPI_FLASH

static uint8_t *mock_flash_memory = NULL;
static int mock_flash_initialized = 0;

void mock_flash_init(void)
{
    if (mock_flash_initialized) return;
    
    mock_flash_memory = (uint8_t *)calloc(1, MOCK_FLASH_SIZE);
    if (mock_flash_memory) {
        /* Заполняем 0xFF как в реальной flash */
        memset(mock_flash_memory, 0xFF, MOCK_FLASH_SIZE);
        mock_flash_initialized = 1;
        log_message(LOG_INFO, "%s: Flash эмуляция инициализирована (%d MB)", 
                    TAG, MOCK_FLASH_SIZE / 1024 / 1024);
    }
}

uint32_t mock_flash_read_id(void)
{
    return MOCK_FLASH_ID;
}

int mock_flash_read(uint32_t addr, uint8_t *buf, uint32_t len)
{
    if (!mock_flash_initialized || !buf) return -1;
    if (addr + len > MOCK_FLASH_SIZE) return -1;
    
    memcpy(buf, mock_flash_memory + addr, len);
    return 0;
}

int mock_flash_write(uint32_t addr, const uint8_t *buf, uint32_t len)
{
    if (!mock_flash_initialized || !buf) return -1;
    if (addr + len > MOCK_FLASH_SIZE) return -1;
    
    /* Flash может только сбрасывать биты (1->0), не устанавливать */
    for (uint32_t i = 0; i < len; i++) {
        mock_flash_memory[addr + i] &= buf[i];
    }
    return 0;
}

int mock_flash_erase_sector(uint32_t addr)
{
    if (!mock_flash_initialized) return -1;
    
    /* Выравниваем по границе сектора */
    addr &= ~(MOCK_FLASH_SECTOR_SIZE - 1);
    if (addr >= MOCK_FLASH_SIZE) return -1;
    
    /* Стирание устанавливает все биты в 1 */
    memset(mock_flash_memory + addr, 0xFF, MOCK_FLASH_SECTOR_SIZE);
    return 0;
}

#endif /* MOCK_SPI_FLASH */

/* ===========================================================================
 * MOCK NETWORK IMPLEMENTATION
 * =========================================================================== */

#if MOCK_NETWORK

static mock_socket_t mock_sockets[MOCK_SOCKET_MAX];
static int mock_network_initialized = 0;

/* Шаблоны Stratum ответов для эмуляции */
static const char *mock_stratum_subscribe_response = 
    "{\"id\":1,\"result\":[[[\"mining.notify\",\"subscription_id\"],"
    "\"extranonce1\",4],null],\"error\":null}\n";

static const char *mock_stratum_authorize_response = 
    "{\"id\":2,\"result\":true,\"error\":null}\n";

static const char *mock_stratum_notify = 
    "{\"id\":null,\"method\":\"mining.notify\","
    "\"params\":[\"job_001\","
    "\"00000000000000000000000000000000000000000000000000000000deadbeef\","
    "\"01000000010000000000000000000000000000000000000000000000000000000000000000ffffffff\","
    "\"ffffffff01\","
    "[],"
    "\"20000000\","
    "\"1a0ffff0\","
    "\"5f5e1000\","
    "true]}\n";

static const char *mock_stratum_difficulty = 
    "{\"id\":null,\"method\":\"mining.set_difficulty\",\"params\":[1024]}\n";

void mock_network_init(void)
{
    if (mock_network_initialized) return;
    
    memset(mock_sockets, 0, sizeof(mock_sockets));
    mock_network_initialized = 1;
    log_message(LOG_INFO, "%s: Network эмуляция инициализирована", TAG);
}

int mock_socket_create(void)
{
    for (int i = 0; i < MOCK_SOCKET_MAX; i++) {
        if (!mock_sockets[i].active) {
            memset(&mock_sockets[i], 0, sizeof(mock_socket_t));
            mock_sockets[i].active = 1;
            return i;
        }
    }
    return -1;
}

int mock_socket_connect(int sock, const char *host, int port)
{
    if (sock < 0 || sock >= MOCK_SOCKET_MAX) return -1;
    if (!mock_sockets[sock].active) return -1;
    
    strncpy(mock_sockets[sock].host, host, sizeof(mock_sockets[sock].host) - 1);
    mock_sockets[sock].port = port;
    mock_sockets[sock].connected = 1;
    
    log_message(LOG_DEBUG, "%s: Mock подключение к %s:%d", TAG, host, port);
    
    return 0;
}

int mock_socket_send(int sock, const void *data, size_t len)
{
    if (sock < 0 || sock >= MOCK_SOCKET_MAX) return -1;
    if (!mock_sockets[sock].connected) return -1;
    
    mock_socket_t *s = &mock_sockets[sock];
    
    /* Сохраняем в TX буфер */
    if (s->tx_len + len <= sizeof(s->tx_buffer)) {
        memcpy(s->tx_buffer + s->tx_len, data, len);
        s->tx_len += len;
    }
    
    /* Анализируем отправленные данные и генерируем ответ */
    const char *str = (const char *)data;
    
    if (strstr(str, "mining.subscribe")) {
        /* Отвечаем на subscribe */
        mock_socket_inject_rx(sock, mock_stratum_subscribe_response, 
                             strlen(mock_stratum_subscribe_response));
    }
    else if (strstr(str, "mining.authorize")) {
        /* Отвечаем на authorize и отправляем difficulty + notify */
        mock_socket_inject_rx(sock, mock_stratum_authorize_response,
                             strlen(mock_stratum_authorize_response));
        mock_socket_inject_rx(sock, mock_stratum_difficulty,
                             strlen(mock_stratum_difficulty));
        mock_socket_inject_rx(sock, mock_stratum_notify,
                             strlen(mock_stratum_notify));
    }
    else if (strstr(str, "mining.submit")) {
        /* Принимаем шару с 90% вероятностью */
        static const char *accept = "{\"id\":3,\"result\":true,\"error\":null}\n";
        static const char *reject = "{\"id\":3,\"result\":false,\"error\":[21,\"stale\",null]}\n";
        
        if ((rand() % 100) < 90) {
            mock_socket_inject_rx(sock, accept, strlen(accept));
        } else {
            mock_socket_inject_rx(sock, reject, strlen(reject));
        }
    }
    
    return (int)len;
}

int mock_socket_recv(int sock, void *buf, size_t len)
{
    if (sock < 0 || sock >= MOCK_SOCKET_MAX) return -1;
    if (!mock_sockets[sock].connected) return -1;
    
    mock_socket_t *s = &mock_sockets[sock];
    
    if (s->rx_pos >= s->rx_len) {
        return 0;  /* Нет данных */
    }
    
    int available = s->rx_len - s->rx_pos;
    int to_read = (available < (int)len) ? available : (int)len;
    
    memcpy(buf, s->rx_buffer + s->rx_pos, to_read);
    s->rx_pos += to_read;
    
    /* Если прочитали всё - сбрасываем буфер */
    if (s->rx_pos >= s->rx_len) {
        s->rx_pos = 0;
        s->rx_len = 0;
    }
    
    return to_read;
}

void mock_socket_close(int sock)
{
    if (sock >= 0 && sock < MOCK_SOCKET_MAX) {
        mock_sockets[sock].active = 0;
        mock_sockets[sock].connected = 0;
    }
}

void mock_socket_inject_rx(int sock, const void *data, size_t len)
{
    if (sock < 0 || sock >= MOCK_SOCKET_MAX) return;
    
    mock_socket_t *s = &mock_sockets[sock];
    
    if (s->rx_len + len <= sizeof(s->rx_buffer)) {
        memcpy(s->rx_buffer + s->rx_len, data, len);
        s->rx_len += len;
    }
}

int mock_socket_get_tx(int sock, void *buf, size_t max_len)
{
    if (sock < 0 || sock >= MOCK_SOCKET_MAX) return -1;
    
    mock_socket_t *s = &mock_sockets[sock];
    int to_copy = (s->tx_len < (int)max_len) ? s->tx_len : (int)max_len;
    
    memcpy(buf, s->tx_buffer, to_copy);
    return to_copy;
}

#endif /* MOCK_NETWORK */

/* ===========================================================================
 * MOCK ASIC IMPLEMENTATION
 * =========================================================================== */

#if MOCK_ASIC

static mock_asic_module_t mock_modules[MOCK_ASIC_MODULES];
static int mock_asic_initialized = 0;

void mock_asic_init(void)
{
    if (mock_asic_initialized) return;
    
    memset(mock_modules, 0, sizeof(mock_modules));
    
    /* Инициализируем модули с реалистичными значениями */
    for (int i = 0; i < MOCK_ASIC_MODULES; i++) {
        mock_modules[i].detected = 1;
        mock_modules[i].enabled = 1;
        mock_modules[i].temp_in = 450 + (rand() % 100);    /* 45.0-55.0°C */
        mock_modules[i].temp_out = 550 + (rand() % 100);   /* 55.0-65.0°C */
        mock_modules[i].freq = 500;
        mock_modules[i].voltage = 780;
        mock_modules[i].fan_speed = 50;
        mock_modules[i].nonce_counter = 0;
        mock_modules[i].last_nonce_time = 0;
    }
    
    mock_asic_initialized = 1;
    log_message(LOG_INFO, "%s: ASIC эмуляция инициализирована (%d модулей)", 
                TAG, MOCK_ASIC_MODULES);
}

int mock_asic_spi_transfer(int module_id, const uint8_t *tx, uint8_t *rx, size_t len)
{
    if (module_id < 0 || module_id >= MOCK_ASIC_MODULES) return -1;
    if (!mock_modules[module_id].detected) return -1;
    
    /* Заполняем rx нулями по умолчанию */
    if (rx) {
        memset(rx, 0, len);
    }
    
    return 0;
}

void mock_asic_set_temperature(int module_id, int16_t temp_in, int16_t temp_out)
{
    if (module_id >= 0 && module_id < MOCK_ASIC_MODULES) {
        mock_modules[module_id].temp_in = temp_in;
        mock_modules[module_id].temp_out = temp_out;
    }
}

int mock_asic_poll_nonce(int module_id, uint32_t *nonce)
{
    if (module_id < 0 || module_id >= MOCK_ASIC_MODULES) return 0;
    if (!mock_modules[module_id].enabled) return 0;
    if (!nonce) return 0;
    
    mock_asic_module_t *m = &mock_modules[module_id];
    
    /* Генерируем nonce с заданной частотой */
    uint32_t now = (uint32_t)time(NULL);
    if (now > m->last_nonce_time) {
        m->last_nonce_time = now;
        m->nonce_counter++;
        
        /* Генерируем случайный nonce */
        *nonce = (uint32_t)rand() ^ ((uint32_t)rand() << 16);
        
        /* Обновляем температуру (немного случайно) */
        m->temp_in += (rand() % 3) - 1;
        m->temp_out += (rand() % 3) - 1;
        
        /* Ограничиваем температуру */
        if (m->temp_in < 400) m->temp_in = 400;
        if (m->temp_in > 1000) m->temp_in = 1000;
        if (m->temp_out < 500) m->temp_out = 500;
        if (m->temp_out > 1050) m->temp_out = 1050;
        
        return 1;
    }
    
    return 0;
}

mock_asic_module_t *mock_asic_get_module(int module_id)
{
    if (module_id >= 0 && module_id < MOCK_ASIC_MODULES) {
        return &mock_modules[module_id];
    }
    return NULL;
}

/**
 * @brief Эмуляция отправки пакета на ASIC
 */
int mock_asic_send(int module_id, const uint8_t *data, size_t len)
{
    if (module_id < 0 || module_id >= MOCK_ASIC_MODULES) return -1;
    if (!mock_modules[module_id].detected) return -1;
    
    /* Сохраняем пакет для анализа (для отладки) */
    mock_asic_module_t *m = &mock_modules[module_id];
    if (len <= sizeof(m->last_tx_pkg)) {
        memcpy(m->last_tx_pkg, data, len);
        m->last_tx_len = len;
    }
    
    return 0;
}

/**
 * @brief Эмуляция приёма пакета от ASIC (формат Avalon4+)
 * 
 * Формирует ответ в зависимости от последней отправленной команды.
 * Формат пакета: head[2] + type + opt + idx + cnt + data[32] + crc[2] = 40 байт
 */
int mock_asic_recv(int module_id, uint8_t *data, size_t len, int timeout)
{
    if (module_id < 0 || module_id >= MOCK_ASIC_MODULES) return -1;
    if (!mock_modules[module_id].detected) return -1;
    if (len < 40) return -1;  /* Минимальный размер пакета */
    
    mock_asic_module_t *m = &mock_modules[module_id];
    
    /* Эмулируем формат ответа ASIC (Avalon4+ protocol) */
    /* [0-1] Head: 'C', 'N' */
    /* [2]   Type */
    /* [3]   Opt */
    /* [4]   Idx */
    /* [5]   Cnt */
    /* [6-37] Data[32] */
    /* [38-39] CRC16 */
    
    memset(data, 0, len);
    
    /* Header 'CN' */
    data[0] = 'C';  /* 0x43 */
    data[1] = 'N';  /* 0x4E */
    
    /* Определяем тип ответа по последней команде */
    uint8_t cmd_type = m->last_tx_pkg[2];
    
    switch (cmd_type) {
        case 0x10:  /* DETECT */
            data[2] = 0x10;  /* type */
            data[3] = 0;     /* opt */
            data[4] = 1;     /* idx */
            data[5] = 1;     /* cnt */
            /* Data[32]: Version info в data[6-37] */
            data[6] = 0x01;  /* major version */
            data[7] = 0x00;  /* minor version */
            data[8] = MOCK_ASIC_CHIPS_PER_MODULE >> 8;
            data[9] = MOCK_ASIC_CHIPS_PER_MODULE & 0xFF;
            break;
            
        case 0x11:  /* STATUS */
            data[2] = 0x11;
            data[3] = 0;
            data[4] = 1;
            data[5] = 1;
            /* Data[32]: Temperature, voltage, frequency в data[6-37] */
            data[6] = (m->temp_in >> 8) & 0xFF;
            data[7] = m->temp_in & 0xFF;
            data[8] = (m->temp_out >> 8) & 0xFF;
            data[9] = m->temp_out & 0xFF;
            data[10] = (m->voltage >> 8) & 0xFF;
            data[11] = m->voltage & 0xFF;
            data[12] = (m->freq >> 8) & 0xFF;
            data[13] = m->freq & 0xFF;
            break;
            
        case 0x12:  /* NONCE */
            data[2] = 0x12;
            data[3] = 0;
            data[4] = 1;
            data[5] = 1;
            /* Data[32]: Nonce data в data[6-37] */
            if ((rand() % 100) < 5) {  /* 5% шанс найти nonce */
                uint32_t nonce = (uint32_t)rand() ^ ((uint32_t)rand() << 16);
                data[6] = (nonce >> 24) & 0xFF;
                data[7] = (nonce >> 16) & 0xFF;
                data[8] = (nonce >> 8) & 0xFF;
                data[9] = nonce & 0xFF;
                data[10] = rand() % MOCK_ASIC_CHIPS_PER_MODULE;  /* Chip ID */
                data[11] = rand() % MOCK_ASIC_CORES_PER_CHIP;    /* Core ID */
                data[12] = 0x01;  /* Nonce found flag */
                m->nonce_counter++;
            } else {
                data[12] = 0x00;  /* No nonce */
            }
            break;
            
        case 0x20:  /* SET_FREQ */
            data[2] = 0x20;
            data[3] = 0;
            data[4] = 1;
            data[5] = 1;
            data[6] = 0x00;  /* Status: OK */
            /* Обновляем частоту из последней команды */
            if (m->last_tx_len >= 40) {
                m->freq = (m->last_tx_pkg[6] << 8) | m->last_tx_pkg[7];
            }
            break;
            
        case 0x21:  /* SET_VOLTAGE */
            data[2] = 0x21;
            data[3] = 0;
            data[4] = 1;
            data[5] = 1;
            data[6] = 0x00;  /* Status: OK */
            /* Обновляем напряжение из последней команды */
            if (m->last_tx_len >= 40) {
                m->voltage = (m->last_tx_pkg[6] << 8) | m->last_tx_pkg[7];
            }
            break;
            
        case 0x30:  /* WORK */
            data[2] = 0x30;
            data[3] = 0;
            data[4] = 1;
            data[5] = 1;
            data[6] = 0x00;  /* Status: Accepted */
            break;
            
        case 0xF0:  /* RESET */
            data[2] = 0xF0;
            data[3] = 0;
            data[4] = 1;
            data[5] = 1;
            data[6] = 0x00;  /* Status: OK */
            break;
            
        default:
            data[2] = cmd_type;
            data[3] = 0xFF;  /* Error opt */
            data[4] = 1;
            data[5] = 1;
            data[6] = 0xFF;  /* Unknown command */
            break;
    }
    
    /* Вычисляем CRC16-CCITT для data[32] (байты 6-37) */
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
    
    uint16_t crc = 0;
    for (int i = 0; i < 32; i++) {
        crc = (crc << 8) ^ crc16_table[((crc >> 8) ^ data[6 + i]) & 0xFF];
    }
    
    data[38] = (crc >> 8) & 0xFF;
    data[39] = crc & 0xFF;
    
    return 0;
}

#endif /* MOCK_ASIC */

/* ===========================================================================
 * SOFTWARE SHA256 IMPLEMENTATION
 * =========================================================================== */

/* SHA256 константы */
static const uint32_t sha256_k[64] = {
    0x428a2f98, 0x71374491, 0xb5c0fbcf, 0xe9b5dba5,
    0x3956c25b, 0x59f111f1, 0x923f82a4, 0xab1c5ed5,
    0xd807aa98, 0x12835b01, 0x243185be, 0x550c7dc3,
    0x72be5d74, 0x80deb1fe, 0x9bdc06a7, 0xc19bf174,
    0xe49b69c1, 0xefbe4786, 0x0fc19dc6, 0x240ca1cc,
    0x2de92c6f, 0x4a7484aa, 0x5cb0a9dc, 0x76f988da,
    0x983e5152, 0xa831c66d, 0xb00327c8, 0xbf597fc7,
    0xc6e00bf3, 0xd5a79147, 0x06ca6351, 0x14292967,
    0x27b70a85, 0x2e1b2138, 0x4d2c6dfc, 0x53380d13,
    0x650a7354, 0x766a0abb, 0x81c2c92e, 0x92722c85,
    0xa2bfe8a1, 0xa81a664b, 0xc24b8b70, 0xc76c51a3,
    0xd192e819, 0xd6990624, 0xf40e3585, 0x106aa070,
    0x19a4c116, 0x1e376c08, 0x2748774c, 0x34b0bcb5,
    0x391c0cb3, 0x4ed8aa4a, 0x5b9cca4f, 0x682e6ff3,
    0x748f82ee, 0x78a5636f, 0x84c87814, 0x8cc70208,
    0x90befffa, 0xa4506ceb, 0xbef9a3f7, 0xc67178f2
};

#define ROTR(x, n)  (((x) >> (n)) | ((x) << (32 - (n))))
#define CH(x, y, z) (((x) & (y)) ^ (~(x) & (z)))
#define MAJ(x, y, z) (((x) & (y)) ^ ((x) & (z)) ^ ((y) & (z)))
#define EP0(x)  (ROTR(x, 2) ^ ROTR(x, 13) ^ ROTR(x, 22))
#define EP1(x)  (ROTR(x, 6) ^ ROTR(x, 11) ^ ROTR(x, 25))
#define SIG0(x) (ROTR(x, 7) ^ ROTR(x, 18) ^ ((x) >> 3))
#define SIG1(x) (ROTR(x, 17) ^ ROTR(x, 19) ^ ((x) >> 10))

static void sha256_transform(uint32_t *state, const uint8_t *data)
{
    uint32_t a, b, c, d, e, f, g, h, t1, t2, m[64];
    int i;
    
    /* Подготовка сообщения */
    for (i = 0; i < 16; i++) {
        m[i] = ((uint32_t)data[i * 4] << 24) |
               ((uint32_t)data[i * 4 + 1] << 16) |
               ((uint32_t)data[i * 4 + 2] << 8) |
               ((uint32_t)data[i * 4 + 3]);
    }
    
    for (i = 16; i < 64; i++) {
        m[i] = SIG1(m[i - 2]) + m[i - 7] + SIG0(m[i - 15]) + m[i - 16];
    }
    
    a = state[0]; b = state[1]; c = state[2]; d = state[3];
    e = state[4]; f = state[5]; g = state[6]; h = state[7];
    
    for (i = 0; i < 64; i++) {
        t1 = h + EP1(e) + CH(e, f, g) + sha256_k[i] + m[i];
        t2 = EP0(a) + MAJ(a, b, c);
        h = g; g = f; f = e; e = d + t1;
        d = c; c = b; b = a; a = t1 + t2;
    }
    
    state[0] += a; state[1] += b; state[2] += c; state[3] += d;
    state[4] += e; state[5] += f; state[6] += g; state[7] += h;
}

void mock_sha256(const uint8_t *data, size_t len, uint8_t *hash)
{
    uint32_t state[8] = {
        0x6a09e667, 0xbb67ae85, 0x3c6ef372, 0xa54ff53a,
        0x510e527f, 0x9b05688c, 0x1f83d9ab, 0x5be0cd19
    };
    
    uint8_t block[64];
    size_t i;
    size_t remaining = len;
    const uint8_t *ptr = data;
    
    /* Обработка полных блоков */
    while (remaining >= 64) {
        sha256_transform(state, ptr);
        ptr += 64;
        remaining -= 64;
    }
    
    /* Padding */
    memset(block, 0, 64);
    memcpy(block, ptr, remaining);
    block[remaining] = 0x80;
    
    if (remaining >= 56) {
        sha256_transform(state, block);
        memset(block, 0, 64);
    }
    
    /* Длина в битах (big-endian) */
    uint64_t bits = len * 8;
    block[63] = bits & 0xff;
    block[62] = (bits >> 8) & 0xff;
    block[61] = (bits >> 16) & 0xff;
    block[60] = (bits >> 24) & 0xff;
    block[59] = (bits >> 32) & 0xff;
    block[58] = (bits >> 40) & 0xff;
    block[57] = (bits >> 48) & 0xff;
    block[56] = (bits >> 56) & 0xff;
    
    sha256_transform(state, block);
    
    /* Результат (big-endian) */
    for (i = 0; i < 8; i++) {
        hash[i * 4] = (state[i] >> 24) & 0xff;
        hash[i * 4 + 1] = (state[i] >> 16) & 0xff;
        hash[i * 4 + 2] = (state[i] >> 8) & 0xff;
        hash[i * 4 + 3] = state[i] & 0xff;
    }
}

void mock_sha256d(const uint8_t *data, size_t len, uint8_t *hash)
{
    uint8_t temp[32];
    mock_sha256(data, len, temp);
    mock_sha256(temp, 32, hash);
}

/* ===========================================================================
 * HELPER FUNCTIONS
 * =========================================================================== */

int hex_to_bytes(const char *hex, uint8_t *bytes, size_t max_len)
{
    size_t hex_len = strlen(hex);
    size_t byte_len = hex_len / 2;
    
    if (byte_len > max_len) byte_len = max_len;
    
    for (size_t i = 0; i < byte_len; i++) {
        unsigned int b;
        if (sscanf(hex + i * 2, "%2x", &b) != 1) {
            return -1;
        }
        bytes[i] = (uint8_t)b;
    }
    
    return (int)byte_len;
}

void bytes_to_hex(const uint8_t *bytes, size_t len, char *hex)
{
    for (size_t i = 0; i < len; i++) {
        sprintf(hex + i * 2, "%02x", bytes[i]);
    }
    hex[len * 2] = '\0';
}

void reverse_bytes(uint8_t *data, size_t len)
{
    for (size_t i = 0; i < len / 2; i++) {
        uint8_t tmp = data[i];
        data[i] = data[len - 1 - i];
        data[len - 1 - i] = tmp;
    }
}

/* ===========================================================================
 * КОНЕЦ ФАЙЛА mock_hardware.c
 * =========================================================================== */
