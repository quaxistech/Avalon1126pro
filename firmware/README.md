# Прошивка Avalon A1126pro

## Обзор проекта

Это реконструированный исходный код прошивки для ASIC майнера Avalon A1126pro.
Прошивка основана на CGMiner 4.11.1 и работает на процессоре Kendryte K210 (RISC-V).

## Архитектура системы

```
┌─────────────────────────────────────────────────────────────────┐
│                    Avalon A1126pro Firmware                     │
├─────────────────────────────────────────────────────────────────┤
│   Приложения                                                    │
│   ├── main.c          - Точка входа, инициализация             │
│   ├── stratum.c       - Протокол связи с пулом                 │
│   └── http_server.c   - Веб-интерфейс управления               │
├─────────────────────────────────────────────────────────────────┤
│   Драйверы                                                      │
│   ├── driver_avalon10.c - Драйвер ASIC чипов                   │
│   ├── fan_control.c     - Управление охлаждением               │
│   └── flash.c           - Работа с Flash памятью               │
├─────────────────────────────────────────────────────────────────┤
│   HAL (Hardware Abstraction Layer)                              │
│   ├── k210_hal.c      - Драйвер периферии K210                 │
│   └── network.c       - Сетевой стек lwIP                      │
├─────────────────────────────────────────────────────────────────┤
│   RTOS                                                          │
│   └── FreeRTOS        - Операционная система реального времени │
├─────────────────────────────────────────────────────────────────┤
│   Hardware                                                      │
│   └── Kendryte K210 (RISC-V 64-bit dual-core @ 400MHz)        │
└─────────────────────────────────────────────────────────────────┘
```

## Структура проекта

```
firmware/
├── CMakeLists.txt          # Система сборки
├── README.md               # Этот файл
├── config/
│   └── k210.ld             # Скрипт линковщика
├── include/
│   ├── avalon10.h          # Определения драйвера ASIC
│   ├── config.h            # Конфигурация системы
│   ├── fan_control.h       # Управление вентиляторами
│   ├── flash.h             # Flash память
│   ├── FreeRTOSConfig.h    # Конфигурация FreeRTOS
│   ├── http_server.h       # HTTP сервер
│   ├── k210_hal.h          # HAL для K210
│   ├── lwipopts.h          # Конфигурация lwIP
│   ├── miner.h             # Структуры CGMiner
│   ├── network.h           # Сетевой стек
│   ├── stratum.h           # Протокол Stratum
│   └── utils.h             # Утилиты
└── src/
    ├── startup.S           # Код запуска (ассемблер)
    ├── main.c              # Главный файл
    ├── driver_avalon10.c   # Драйвер ASIC
    ├── stratum.c           # Stratum клиент
    ├── http_server.c       # Веб-сервер
    ├── k210_hal.c          # HAL
    ├── network.c           # Сеть
    ├── flash.c             # Flash
    ├── fan_control.c       # Вентиляторы
    └── utils.c             # Утилиты
```

## Требования

### Тулчейн

Требуется кросс-компилятор RISC-V:

```bash
# Установка на Ubuntu/Debian
sudo apt-get install gcc-riscv64-unknown-elf

# Или скачайте с https://github.com/kendryte/kendryte-gnu-toolchain
```

### SDK

Требуется Kendryte FreeRTOS SDK:

```bash
git clone https://github.com/kendryte/kendryte-freertos-sdk.git
```

### Дополнительные зависимости

- CMake >= 3.10
- kflash (для прошивки)

```bash
pip install kflash
```

## Сборка

```bash
# Создаём директорию сборки
mkdir build && cd build

# Конфигурируем проект
cmake ..

# Собираем
make -j$(nproc)

# Результат: avalon_a1126pro_firmware.bin
```

## Прошивка устройства

```bash
# Через USB-UART
make flash

# Или вручную
kflash -p /dev/ttyUSB0 -b 1500000 avalon_a1126pro_firmware.bin
```

## Карта памяти Flash

| Область       | Смещение   | Размер  | Описание                    |
|---------------|------------|---------|----------------------------|
| Bootloader    | 0x000000   | 64KB    | Загрузчик kflash           |
| Slot A        | 0x010000   | 3.5MB   | Основная прошивка          |
| Slot B        | 0x380000   | 3.5MB   | Резерв для OTA             |
| Config        | 0x6F0000   | 64KB    | Конфигурация               |
| Web           | 0x700000   | 512KB   | Web-ресурсы                |
| Reserved      | 0x780000   | 512KB   | Зарезервировано            |

## Веб-интерфейс

После загрузки прошивки веб-интерфейс доступен по IP адресу устройства.

### API Endpoints

| Endpoint                | Метод | Описание               |
|-------------------------|-------|------------------------|
| /                       | GET   | Главная страница       |
| /get_miner_status.cgi   | GET   | Статус майнера (JSON)  |
| /set_miner_conf.cgi     | POST  | Установка конфигурации |
| /reboot.cgi             | POST  | Перезагрузка           |

### Пример получения статуса

```bash
curl http://192.168.1.100/get_miner_status.cgi
```

Ответ:
```json
{
  "hashrate": 12.5,
  "hashrate_unit": "TH/s",
  "temperature": 65,
  "fan_speed": 80,
  "uptime": 3600,
  "pool_status": "connected"
}
```

## Протокол Stratum

Прошивка поддерживает протокол Stratum v1 для связи с пулами:

- `mining.subscribe` - подписка на задания
- `mining.authorize` - авторизация воркера
- `mining.notify` - получение заданий
- `mining.submit` - отправка найденных решений
- `mining.set_difficulty` - установка сложности

## Задачи FreeRTOS

| Задача          | Приоритет | Стек   | Описание                    |
|-----------------|-----------|--------|----------------------------|
| miner_task      | 6         | 4KB    | Работа с ASIC чипами       |
| network_task    | 5         | 4KB    | Сетевой стек               |
| stratum_task    | 4         | 4KB    | Связь с пулом              |
| http_task       | 3         | 4KB    | HTTP сервер                |
| monitor_task    | 2         | 2KB    | Мониторинг и статистика    |

## Конфигурация

Настройки хранятся во Flash и включают:

- URL пула и воркера
- Сетевые параметры (IP, DHCP)
- Частота и напряжение ASIC
- Параметры охлаждения

## Отладка

### UART консоль

Отладочный вывод доступен через UART0:
- Скорость: 115200 бод
- Формат: 8N1

### Уровни логирования

```c
#define DEBUG_ERROR   1  // Только ошибки
#define DEBUG_WARNING 2  // + предупреждения
#define DEBUG_INFO    3  // + информация
#define DEBUG_DEBUG   4  // + отладка
#define DEBUG_TRACE   5  // Всё
```

## Лицензия

Этот код является реконструкцией оригинальной прошивки.
Используйте на свой страх и риск.

## Авторы

- Оригинальная прошивка: Canaan Creative
- Реконструкция: Исследовательский проект

## Ссылки

- [CGMiner](https://github.com/ckolivas/cgminer)
- [Kendryte K210](https://kendryte.com/)
- [FreeRTOS](https://www.freertos.org/)
- [lwIP](https://savannah.nongnu.org/projects/lwip/)
