# Архитектура прошивки Avalon A1126pro

## 📐 Общая структура

Прошивка Avalon A1126pro построена на базе **FreeRTOS** и использует многозадачную архитектуру для параллельной обработки различных функций майнера.

```
┌─────────────────────────────────────────────────────────────────────────┐
│                         ПРИКЛАДНОЙ УРОВЕНЬ                              │
├──────────────┬──────────────┬──────────────┬─────────────┬─────────────┤
│   STRATUM    │    POOL      │    API       │    HTTP     │   MONITOR   │
│   Протокол   │  Менеджер    │   Сервер     │   Сервер    │   Система   │
│  stratum.c   │   pool.c     │   api.c      │             │             │
├──────────────┴──────────────┴──────────────┴─────────────┴─────────────┤
│                      РАБОТА С ЗАДАНИЯМИ (work.c)                        │
│        • Формирование заголовка блока (80 байт)                         │
│        • Вычисление Merkle Root                                         │
│        • SHA256d верификация                                            │
├─────────────────────────────────────────────────────────────────────────┤
│                      ДРАЙВЕР ASIC (avalon10.c)                          │
│        • SPI протокол связи с чипами                                    │
│        • Управление частотой и напряжением                              │
│        • Мониторинг температуры                                         │
│        • PWM управление вентиляторами                                   │
├────────────────────┬────────────────────┬───────────────────────────────┤
│    СЕТЬ (network)  │   FLASH (w25qxx)   │   КОНФИГ (config)             │
│    lwIP + DM9051   │     W25Q64 8MB     │   Настройки                   │
├────────────────────┴────────────────────┴───────────────────────────────┤
│                  ЭМУЛЯЦИЯ (mock_hardware.c)                             │
│        • Эмуляция Flash (8 MB в RAM)                                    │
│        • Эмуляция сокетов со Stratum ответами                           │
│        • Эмуляция 4 ASIC модулей                                        │
│        • Программный SHA256 для тестов                                  │
├─────────────────────────────────────────────────────────────────────────┤
│                           FreeRTOS                                       │
│        • Многозадачность (7+ задач)                                     │
│        • Очереди сообщений                                              │
│        • Мьютексы и семафоры                                            │
├─────────────────────────────────────────────────────────────────────────┤
│                      KENDRYTE K210 SDK                                   │
│        • SPI, I2C, UART, GPIO, PWM                                      │
│        • DMA контроллер                                                 │
│        • Аппаратный SHA256                                              │
└─────────────────────────────────────────────────────────────────────────┘
```

---

## 🔄 Потоки данных

### 1. Получение работы от пула

```
┌──────────┐    TCP/IP    ┌──────────┐    JSON     ┌──────────┐
│   Пул    │ ──────────▶ │ network  │ ─────────▶ │ stratum  │
│ Stratum  │              │   .c     │             │   .c     │
└──────────┘              └──────────┘             └──────────┘
                                                       │
                                                       ▼
┌──────────┐  Block Hdr  ┌──────────┐   Work_t    ┌──────────┐
│ avalon10 │ ◀────────── │  work.c  │ ◀────────── │  Queue   │
│   .c     │              │          │             │ FreeRTOS │
└──────────┘              └──────────┘             └──────────┘
```

### 2. Отправка найденных nonce

```
┌──────────┐   NONCE     ┌──────────┐   Submit    ┌──────────┐
│ ASIC     │ ──────────▶ │ avalon10 │ ─────────▶ │ stratum  │
│ Chips    │   (SPI)     │   .c     │             │   .c     │
└──────────┘              └──────────┘             └──────────┘
                                                       │
                                                       ▼
┌──────────┐    TCP/IP   ┌──────────┐    JSON     ┌──────────┐
│   Пул    │ ◀────────── │ network  │ ◀───────── │  pool.c  │
│ Stratum  │              │   .c     │             │          │
└──────────┘              └──────────┘             └──────────┘
```

---

## 📋 Задачи FreeRTOS

### Таблица задач

| # | Задача | Приоритет | Стек | Назначение |
|---|--------|-----------|------|------------|
| 1 | `watchdog_task` | 6 (высокий) | 2 KB | Сторожевой таймер, аварийный сброс |
| 2 | `stratum_send_task` | 5 | 4 KB | Отправка Stratum сообщений на пул |
| 3 | `stratum_recv_task` | 5 | 4 KB | Приём Stratum сообщений от пула |
| 4 | `mining_task` | 4 | 4 KB | Отправка работы на ASIC чипы |
| 5 | `monitor_task` | 4 | 4 KB | Мониторинг температуры, управление вентиляторами |
| 6 | `api_task` | 3 | 8 KB | CGMiner API сервер (порт 4028) |
| 7 | `http_task` | 3 | 8 KB | HTTP веб-интерфейс (порт 80) |
| 8 | `led_task` | 2 (низкий) | 1 KB | Управление светодиодами |

### Приоритеты

- **6** - Критические задачи (watchdog)
- **5** - Высокоприоритетные (сеть, протокол)
- **4** - Обычные задачи (майнинг, мониторинг)
- **3** - Фоновые сервисы (API, HTTP)
- **2** - Вспомогательные (индикация)

---

## 🔌 Аппаратные интерфейсы

### SPI

| Устройство | SPI# | CLK (MHz) | Назначение |
|------------|------|-----------|------------|
| W25Q64 Flash | SPI2 | 25 | Хранение конфигурации |
| DM9051 Ethernet | SPI1 | 20 | Сетевой интерфейс |
| ASIC Avalon10 | SPI0 | 10 | Связь с чипами |

### GPIO

| GPIO | Функция | Описание |
|------|---------|----------|
| 0-3 | ASIC CS | Chip Select для 4 модулей |
| 4 | LED Status | Индикатор статуса |
| 5 | LED Error | Индикатор ошибки |
| 6 | DM9051 INT | Прерывание Ethernet |
| 7 | DM9051 RST | Сброс Ethernet |
| 8-9 | FAN PWM | ШИМ вентиляторов |
| 10-11 | I2C | Датчики температуры |

### PWM

| Канал | Частота | Назначение |
|-------|---------|------------|
| PWM0_CH0 | 25 kHz | Вентилятор 1 |
| PWM0_CH1 | 25 kHz | Вентилятор 2 |

---

## 📦 Структуры данных

### work_t - Рабочее задание

```c
typedef struct work {
    /* Идентификация */
    char     job_id[64];          // ID задания от пула
    uint64_t work_id;             // Внутренний ID
    
    /* Параметры блока */
    uint32_t version;             // Версия блока Bitcoin
    uint32_t ntime;               // Время блока
    uint32_t nbits;               // Сложность (compact)
    double   difficulty;          // Сложность (float)
    
    /* Хэши */
    uint8_t  prevhash[32];        // Хэш предыдущего блока
    uint8_t  merkle_root[32];     // Корень Merkle дерева
    uint8_t  target[32];          // Целевой хэш
    
    /* Coinbase транзакция */
    uint8_t  coinbase1[256];      // Начало coinbase
    int      coinbase1_len;
    uint8_t  coinbase2[256];      // Конец coinbase
    int      coinbase2_len;
    
    /* Nonce */
    uint8_t  nonce2[8];           // Extranonce2
    int      nonce2_len;
    
    /* Merkle branch */
    uint8_t  merkle_branch[12][32]; // Путь в дереве
    int      merkle_count;
    
    /* Заголовок блока (80 байт) */
    uint8_t  header[128];
    
    /* Метаданные */
    time_t   timestamp;           // Время создания
    int      pool_no;             // Номер пула
} work_t;
```

### avalon10_info_t - Состояние ASIC

```c
typedef struct avalon10_info {
    /* Общее состояние */
    int      miner_count;         // Количество модулей
    int      chip_count;          // Всего чипов
    int      enabled;             // Флаг включения
    int      mining;              // Флаг майнинга
    
    /* Производительность */
    double   total_hashrate;      // Общий хэшрейт (H/s)
    uint64_t total_nonces;        // Всего найдено nonce
    uint64_t total_hw_errors;     // Аппаратные ошибки
    
    /* Информация о модулях */
    avalon10_module_t modules[4]; // До 4 модулей
    
    /* Настройки */
    int      frequency[2];        // Частоты [мин, макс]
    int      voltage;             // Напряжение (mV)
    int      fan_pwm[2];          // ШИМ вентиляторов (%)
    
    /* Температура */
    int      temp_target;         // Целевая температура
    int      temp_overheat;       // Температура перегрева
    int      max_temp;            // Текущая максимальная
} avalon10_info_t;
```

### pool_t - Пул майнинга

```c
typedef struct pool {
    /* Идентификация */
    int      pool_no;             // Номер пула
    int      priority;            // Приоритет (0 = высший)
    
    /* Подключение */
    char     url[256];            // URL пула
    char     host[128];           // Хост
    int      port;                // Порт
    char     user[128];           // Имя воркера
    char     pass[128];           // Пароль
    
    /* Состояние */
    int      state;               // POOL_STATE_*
    int      sock;                // Дескриптор сокета
    
    /* Stratum данные */
    char     extranonce1[32];     // Extranonce1 от пула
    int      nonce1_len;
    int      nonce2_len;          // Размер extranonce2
    int      msg_id;              // ID сообщений
    double   diff;                // Сложность от пула
    
    /* Статистика */
    uint64_t accepted;            // Принятые shares
    uint64_t rejected;            // Отклонённые shares
    time_t   last_work_time;      // Время последней работы
} pool_t;
```

---

## 🔐 Протокол связи с ASIC

### Формат пакета

```
┌─────────┬──────────┬──────────┬──────────┬──────────┐
│ MAGIC   │   TYPE   │   LEN    │   DATA   │   CRC32  │
│ 2 байта │  1 байт  │  1 байт  │  N байт  │  4 байта │
│  0x4156 │          │          │  до 88   │          │
└─────────┴──────────┴──────────┴──────────┴──────────┘
     │          │          │          │          │
     └── "AV"   │          │          │          └── Проверка целостности
               │          │          │
               │          │          └── Полезные данные
               │          │
               │          └── Длина DATA (0-88)
               │
               └── Тип команды/ответа
```

### Типы команд

| Код | Название | Описание |
|-----|----------|----------|
| 0x01 | WORK | Отправка задания на чип |
| 0x10 | NONCE | Найден nonce (от ASIC) |
| 0x11 | STATUS | Запрос статуса |
| 0x20 | SET_FREQ | Установка частоты |
| 0x21 | SET_VOLTAGE | Установка напряжения |
| 0x30 | RESET | Сброс модуля |
| 0x31 | DETECT | Обнаружение модулей |

### Пример WORK пакета (отправка задания)

```
Offset  Data                                    Описание
------  ----                                    --------
0x00    41 56                                   Magic "AV"
0x02    01                                      Type = WORK
0x03    50                                      Len = 80 (заголовок блока)
0x04    [80 байт заголовка блока]               Data
0x54    XX XX XX XX                             CRC32
```

### Пример NONCE пакета (ответ от ASIC)

```
Offset  Data                                    Описание
------  ----                                    --------
0x00    41 56                                   Magic "AV"
0x02    10                                      Type = NONCE
0x03    08                                      Len = 8
0x04    XX XX XX XX                             Nonce (4 байта)
0x08    YY YY YY YY                             Job ID (4 байта)
0x0C    ZZ ZZ ZZ ZZ                             CRC32
```

---

## 📊 Алгоритм майнинга Bitcoin

### Заголовок блока (80 байт)

```
┌──────────────────────────────────────────────────────────────┐
│                    BLOCK HEADER (80 bytes)                    │
├─────────────┬──────────────────────────────────────────────────┤
│  Offset     │  Field                                          │
├─────────────┼──────────────────────────────────────────────────┤
│  0-3        │  Version (4 bytes, little-endian)               │
│  4-35       │  Previous Block Hash (32 bytes, reversed)       │
│  36-67      │  Merkle Root (32 bytes, reversed)               │
│  68-71      │  Timestamp (4 bytes, little-endian)             │
│  72-75      │  Bits / Difficulty (4 bytes, little-endian)     │
│  76-79      │  Nonce (4 bytes, little-endian) ← ИЩЕМ!         │
└─────────────┴──────────────────────────────────────────────────┘
```

### Процесс поиска

```
1. Получаем данные от пула (mining.notify)
   ↓
2. Формируем coinbase транзакцию:
   coinbase = coinbase1 + extranonce1 + extranonce2 + coinbase2
   ↓
3. Вычисляем Merkle Root:
   hash = SHA256d(coinbase)
   for each branch in merkle_branch:
       hash = SHA256d(hash + branch)
   merkle_root = hash
   ↓
4. Собираем заголовок блока (80 байт)
   ↓
5. Отправляем на ASIC чипы
   ↓
6. ASIC перебирает nonce (0x00000000 - 0xFFFFFFFF)
   ↓
7. Для каждого nonce:
   block_hash = SHA256d(header + nonce)
   if block_hash < target:
       НАЙДЕНО! → Отправляем на пул
```

### SHA256d (двойной SHA256)

```
SHA256d(data) = SHA256(SHA256(data))
```

Используется для:
- Хэширования заголовка блока
- Вычисления Merkle Root
- Хэширования coinbase транзакции

---

## 🌐 Stratum протокол

### Жизненный цикл подключения

```
┌──────────┐                               ┌──────────┐
│  Майнер  │                               │   Пул    │
└────┬─────┘                               └────┬─────┘
     │                                          │
     │────── mining.subscribe ────────────────▶│
     │◀───── result: subscription_id ──────────│
     │                                          │
     │────── mining.authorize ────────────────▶│
     │◀───── result: true ─────────────────────│
     │                                          │
     │◀───── mining.set_difficulty ────────────│
     │                                          │
     │◀───── mining.notify ────────────────────│
     │        (работа)                          │
     │                                          │
     │────── mining.submit ───────────────────▶│
     │        (nonce найден)                    │
     │◀───── result: true/false ───────────────│
     │                                          │
```

### Формат сообщений

**mining.subscribe** (запрос):
```json
{"id": 1, "method": "mining.subscribe", "params": ["cgminer/4.11.1"]}
```

**mining.subscribe** (ответ):
```json
{
  "id": 1,
  "result": [
    [["mining.notify", "subscription_id"]],
    "extranonce1",
    4
  ],
  "error": null
}
```

**mining.notify** (уведомление):
```json
{
  "id": null,
  "method": "mining.notify",
  "params": [
    "job_id",           // ID задания
    "prevhash",         // Предыдущий хэш (hex)
    "coinbase1",        // Начало coinbase (hex)
    "coinbase2",        // Конец coinbase (hex)
    ["branch1", ...],   // Merkle branch (hex[])
    "version",          // Версия (hex)
    "nbits",            // Сложность (hex)
    "ntime",            // Время (hex)
    true                // Clean jobs
  ]
}
```

**mining.submit** (отправка):
```json
{
  "id": 4,
  "method": "mining.submit",
  "params": [
    "worker",           // Имя воркера
    "job_id",           // ID задания
    "extranonce2",      // Extranonce2 (hex)
    "ntime",            // Время (hex)
    "nonce"             // Найденный nonce (hex)
  ]
}
```

---

## 🔧 Конфигурация сборки

### CMake опции

| Опция | Значение | Описание |
|-------|----------|----------|
| `PROJ` | avalon1126 | Имя проекта |
| `USE_MOCK_HARDWARE` | ON/OFF | Включить эмуляцию |
| `MOCK_SPI_FLASH` | 1/0 | Эмуляция Flash |
| `MOCK_NETWORK` | 1/0 | Эмуляция сети |
| `MOCK_ASIC` | 1/0 | Эмуляция ASIC |

### Сборка для тестирования

```bash
cmake .. -DPROJ=avalon1126 -DUSE_MOCK_HARDWARE=ON
make -j$(nproc)
```

### Сборка для реального железа

```bash
cmake .. -DPROJ=avalon1126
make -j$(nproc)
```

---

## 📈 Статистика кода

| Файл | Строк | Описание |
|------|-------|----------|
| main.c | ~850 | Главная точка входа |
| avalon10.c | ~950 | Драйвер ASIC |
| stratum.c | ~760 | Stratum протокол |
| mock_hardware.c | ~650 | Эмуляция |
| network.c | ~560 | Сетевой стек |
| w25qxx.c | ~440 | Драйвер Flash |
| work.c | ~370 | Управление работой |
| api.c | ~390 | API сервер |
| pool.c | ~350 | Менеджер пулов |
| config.c | ~200 | Конфигурация |
| cgminer.h | ~400 | Общие определения |
| avalon10.h | ~680 | Заголовок ASIC |
| mock_hardware.h | ~250 | Заголовок эмуляции |
| **Всего** | **~7850** | **C-код** |

---

## 📚 Ссылки

- [CGMiner GitHub](https://github.com/ckolivas/cgminer)
- [Stratum Protocol](https://en.bitcoin.it/wiki/Stratum_mining_protocol)
- [Kendryte K210 Datasheet](https://kendryte.com)
- [FreeRTOS Documentation](https://freertos.org)
- [lwIP Documentation](https://www.nongnu.org/lwip/)
