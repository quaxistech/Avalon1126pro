/**
 * =============================================================================
 * @file    work.c
 * @brief   Avalon A1126pro - Управление заданиями (реализация)
 * @version 1.0
 * @date    2024
 * =============================================================================
 * 
 * ОПИСАНИЕ:
 * Управление рабочими заданиями для майнинга Bitcoin.
 * Формирование заголовка блока, расчёт Merkle Root, SHA256.
 * 
 * СТРУКТУРА ЗАГОЛОВКА БЛОКА (80 байт):
 * [0-3]   version     - Версия блока (little-endian)
 * [4-35]  prev_hash   - Хэш предыдущего блока
 * [36-67] merkle_root - Корень дерева Меркла
 * [68-71] ntime       - Время блока (little-endian)
 * [72-75] nbits       - Сложность (little-endian)
 * [76-79] nonce       - Nonce для поиска (little-endian)
 * 
 * =============================================================================
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "work.h"
#include "cgminer.h"
#include "mock_hardware.h"

static const char *TAG = "Work";

/* ===========================================================================
 * SHA256 ФУНКЦИИ
 * =========================================================================== */

/* Используем программный SHA256 из mock_hardware.c */
extern void mock_sha256(const uint8_t *data, size_t len, uint8_t *hash);
extern void mock_sha256d(const uint8_t *data, size_t len, uint8_t *hash);

/**
 * @brief Вычисление SHA256d (двойной SHA256)
 */
static void sha256d(const uint8_t *data, size_t len, uint8_t *hash)
{
    mock_sha256d(data, len, hash);
}

/**
 * @brief Однократный SHA256
 */
static void sha256(const uint8_t *data, size_t len, uint8_t *hash)
{
    mock_sha256(data, len, hash);
}

/* ===========================================================================
 * ВСПОМОГАТЕЛЬНЫЕ ФУНКЦИИ
 * =========================================================================== */

/**
 * @brief Запись 32-битного значения в little-endian
 */
static void write_le32(uint8_t *buf, uint32_t val)
{
    buf[0] = val & 0xFF;
    buf[1] = (val >> 8) & 0xFF;
    buf[2] = (val >> 16) & 0xFF;
    buf[3] = (val >> 24) & 0xFF;
}

/**
 * @brief Чтение 32-битного значения из little-endian
 */
static uint32_t read_le32(const uint8_t *buf)
{
    return buf[0] | (buf[1] << 8) | (buf[2] << 16) | (buf[3] << 24);
}

/**
 * @brief Реверс массива байт (локальная версия)
 */
static void work_reverse_bytes(uint8_t *data, size_t len)
{
    for (size_t i = 0; i < len / 2; i++) {
        uint8_t tmp = data[i];
        data[i] = data[len - 1 - i];
        data[len - 1 - i] = tmp;
    }
}

/**
 * @brief Реверс 32-байтного хэша (для Bitcoin)
 */
static void reverse_hash(uint8_t *hash)
{
    work_reverse_bytes(hash, 32);
}

/* ===========================================================================
 * ФУНКЦИИ РАБОТЫ С MERKLE ROOT
 * =========================================================================== */

/**
 * @brief Построение Merkle Root
 * 
 * Merkle Root строится следующим образом:
 * 1. Сначала хэшируется coinbase транзакция
 * 2. Затем последовательно комбинируется с merkle_branch
 * 
 * @param work Указатель на работу
 * @param merkle_root Буфер для результата (32 байта)
 */
static void build_merkle_root(work_t *work, uint8_t *merkle_root)
{
    uint8_t coinbase[512];
    uint8_t hash[32];
    uint8_t combined[64];
    int coinbase_len = 0;
    
    /* Собираем coinbase транзакцию:
     * coinbase = coinbase1 + extranonce1 + extranonce2 + coinbase2
     *
     * coinbase1 содержит начало транзакции до места вставки extranonce
     * extranonce1 - уникальный идентификатор от пула (обычно 4 байта)
     * extranonce2 - наш счётчик для генерации уникальных coinbase (4-8 байт)
     * coinbase2 - завершение транзакции
     */
    memcpy(coinbase, work->coinbase1, work->coinbase1_len);
    coinbase_len = work->coinbase1_len;
    
    /* Добавляем extranonce1 от пула */
    if (work->extranonce1_len > 0) {
        memcpy(coinbase + coinbase_len, work->extranonce1, work->extranonce1_len);
        coinbase_len += work->extranonce1_len;
    }
    
    /* Добавляем extranonce2 (наш счётчик) */
    memcpy(coinbase + coinbase_len, work->nonce2, work->nonce2_len);
    coinbase_len += work->nonce2_len;
    
    memcpy(coinbase + coinbase_len, work->coinbase2, work->coinbase2_len);
    coinbase_len += work->coinbase2_len;
    
    /* Хэшируем coinbase */
    sha256d(coinbase, coinbase_len, hash);
    
    /* Комбинируем с merkle branch */
    for (int i = 0; i < work->merkle_count; i++) {
        /* hash = sha256d(hash + merkle_branch[i]) */
        memcpy(combined, hash, 32);
        memcpy(combined + 32, work->merkle_branch[i], 32);
        sha256d(combined, 64, hash);
    }
    
    memcpy(merkle_root, hash, 32);
}

/* ===========================================================================
 * ФУНКЦИИ РАБОТЫ
 * =========================================================================== */

/**
 * @brief Создание новой работы
 */
work_t *create_work(void)
{
    work_t *work = (work_t *)calloc(1, sizeof(work_t));
    if (work) {
        work->timestamp = time(NULL);
        work->difficulty = 1.0;
        work->nonce2_len = 4;  /* По умолчанию 4 байта */
        memset(work->nonce2, 0, sizeof(work->nonce2));
    }
    return work;
}

/**
 * @brief Освобождение работы
 */
void free_work(work_t *work)
{
    if (work) {
        free(work);
    }
}

/**
 * @brief Формирование 80-байтного заголовка блока
 * 
 * Заголовок блока Bitcoin:
 * - version (4 байта, little-endian)
 * - prev_hash (32 байта, reversed)
 * - merkle_root (32 байта, reversed)
 * - ntime (4 байта, little-endian)
 * - nbits (4 байта, little-endian)
 * - nonce (4 байта, little-endian) - заполняется при поиске
 */
int work_to_header(work_t *work)
{
    if (!work) return -1;
    
    uint8_t *header = work->header;
    uint8_t merkle_root[32];
    
    memset(header, 0, 128);
    
    /* [0-3] Version */
    write_le32(header, work->version);
    
    /* [4-35] Previous block hash (reversed) */
    memcpy(header + 4, work->prevhash, 32);
    /* Bitcoin использует reversed hash */
    /* В stratum prevhash уже в правильном формате */
    
    /* [36-67] Merkle root */
    build_merkle_root(work, merkle_root);
    memcpy(work->merkle_root, merkle_root, 32);
    memcpy(header + 36, merkle_root, 32);
    
    /* [68-71] Time */
    write_le32(header + 68, work->ntime);
    
    /* [72-75] Bits (difficulty) */
    write_le32(header + 72, work->nbits);
    
    /* [76-79] Nonce - будет заполнен при поиске */
    write_le32(header + 76, 0);
    
    /* Вычисляем target из nbits */
    work_calculate_target(work);
    
    log_message(LOG_DEBUG, "%s: Заголовок сформирован, version=0x%08X, nbits=0x%08X", 
               TAG, work->version, work->nbits);
    
    return 0;
}

/**
 * @brief Вычисление целевого хэша (target) из nbits
 * 
 * nbits - это компактное представление target:
 * Первый байт - экспонента
 * Остальные 3 байта - мантисса
 * target = mantissa * 2^(8*(exponent-3))
 */
void work_calculate_target(work_t *work)
{
    if (!work) return;
    
    uint32_t nbits = work->nbits;
    uint8_t exponent = (nbits >> 24) & 0xFF;
    uint32_t mantissa = nbits & 0x00FFFFFF;
    
    memset(work->target, 0, 32);
    
    if (exponent <= 3) {
        /* Mantissa сдвигается вправо */
        mantissa >>= 8 * (3 - exponent);
        work->target[0] = mantissa & 0xFF;
        work->target[1] = (mantissa >> 8) & 0xFF;
        work->target[2] = (mantissa >> 16) & 0xFF;
    } else {
        /* Mantissa сдвигается влево */
        int shift = exponent - 3;
        work->target[shift] = mantissa & 0xFF;
        work->target[shift + 1] = (mantissa >> 8) & 0xFF;
        work->target[shift + 2] = (mantissa >> 16) & 0xFF;
    }
    
    /* Вычисляем сложность (difficulty) */
    /* difficulty = max_target / target */
    /* Для упрощения используем приближение */
    if (mantissa > 0) {
        work->difficulty = 0x00FFFF / (double)mantissa;
        work->difficulty *= (double)(1ULL << (8 * (0x1D - exponent)));
    }
}

/**
 * @brief Проверка nonce
 * 
 * Вычисляет хэш заголовка с указанным nonce и проверяет
 * удовлетворяет ли он целевой сложности.
 * 
 * @param work  Указатель на работу
 * @param nonce Nonce для проверки
 * @return 1 если nonce валиден, 0 иначе
 */
int work_check_nonce(work_t *work, uint32_t nonce)
{
    if (!work) return 0;
    
    uint8_t header[80];
    uint8_t hash[32];
    
    /* Копируем заголовок */
    memcpy(header, work->header, 80);
    
    /* Устанавливаем nonce */
    write_le32(header + 76, nonce);
    
    /* Вычисляем SHA256d */
    sha256d(header, 80, hash);
    
    /* Проверяем против target */
    /* hash должен быть меньше target */
    /* Сравниваем с конца (big-endian comparison) */
    for (int i = 31; i >= 0; i--) {
        if (hash[i] < work->target[i]) {
            return 1;  /* Хэш меньше target - валидный */
        }
        if (hash[i] > work->target[i]) {
            return 0;  /* Хэш больше target - невалидный */
        }
    }
    
    return 1;  /* Хэш равен target - валидный (крайне редко) */
}

/**
 * @brief Формирование строк для submit
 * 
 * @param work      Указатель на работу
 * @param nonce     Найденный nonce
 * @param nonce2_hex    Буфер для extranonce2 (hex)
 * @param ntime_hex     Буфер для ntime (hex)
 * @param nonce_hex     Буфер для nonce (hex)
 */
void work_get_submit_data(work_t *work, uint32_t nonce,
                          char *nonce2_hex, char *ntime_hex, char *nonce_hex)
{
    if (!work) return;
    
    /* ExtraNonce2 */
    if (nonce2_hex) {
        for (int i = 0; i < work->nonce2_len; i++) {
            sprintf(nonce2_hex + i * 2, "%02x", work->nonce2[i]);
        }
        nonce2_hex[work->nonce2_len * 2] = '\0';
    }
    
    /* ntime */
    if (ntime_hex) {
        sprintf(ntime_hex, "%08x", work->ntime);
    }
    
    /* nonce */
    if (nonce_hex) {
        sprintf(nonce_hex, "%08x", nonce);
    }
}

/**
 * @brief Увеличение ExtraNonce2
 * 
 * Используется для генерации уникальных работ
 */
void work_increment_nonce2(work_t *work)
{
    if (!work) return;
    
    /* Увеличиваем nonce2 как little-endian число */
    for (int i = 0; i < work->nonce2_len; i++) {
        if (++work->nonce2[i] != 0) {
            break;
        }
    }
}

/**
 * @brief Клонирование работы
 */
work_t *clone_work(const work_t *work)
{
    if (!work) return NULL;
    
    work_t *new_work = create_work();
    if (new_work) {
        memcpy(new_work, work, sizeof(work_t));
        new_work->next = NULL;
    }
    
    return new_work;
}

/**
 * @brief Проверка, устарела ли работа
 */
int work_is_stale(const work_t *work)
{
    if (!work) return 1;
    
    /* Работа устарела если:
     * 1. Установлен флаг stale
     * 2. Прошло более 60 секунд с момента получения
     */
    if (work->stale) return 1;
    
    time_t now = time(NULL);
    if (now - work->timestamp > 60) return 1;
    
    return 0;
}

/* ===========================================================================
 * КОНЕЦ ФАЙЛА work.c
 * =========================================================================== */
