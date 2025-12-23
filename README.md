# Avalon A1126pro Firmware

Восстановленная прошивка для майнера Avalon A1126pro на базе процессора Kendryte K210.

## Характеристики

- **Процессор**: Kendryte K210 (RISC-V 64-bit rv64imafc, 400MHz, dual-core)
- **ASIC**: Avalon10 A1126
- **Чипов на модуль**: 114
- **Ядер на чип**: 72
- **Протокол**: Stratum (Bitcoin mining)
- **API**: CGMiner совместимый API (порт 4028)

## Структура проекта

```
├── avalon1126.bin          # Скомпилированная прошивка
├── avalon1126.elf          # ELF с отладочной информацией
├── donor_dump.bin          # Оригинальный дамп flash
├── decompiled/             # Декомпилированный код (справка)
└── kendryte-freertos-sdk/  # SDK с исходниками
    └── src/avalon1126/     # Исходный код проекта
        ├── main.c          # Точка входа
        ├── cgminer.h       # Общие определения
        ├── avalon10.c/h    # Драйвер ASIC
        ├── pool.c/h        # Управление пулами
        ├── stratum.c/h     # Stratum протокол
        ├── network.c/h     # Сетевой стек
        ├── work.c/h        # Управление работой
        ├── api.c/h         # CGMiner API
        ├── config.c/h      # Конфигурация
        └── w25qxx.c/h      # Flash драйвер
```

## Требования

- [Kendryte Toolchain](https://kendryte.com/downloads/) (GCC 8.2.0 для RISC-V)
- CMake 3.5+
- Make

## Сборка

1. Скачайте Kendryte toolchain и распакуйте в `kendryte-toolchain/`

2. Соберите проект:
```bash
cd kendryte-freertos-sdk
mkdir build && cd build
cmake .. -DPROJ=avalon1126 -DTOOLCHAIN=../../kendryte-toolchain/bin
make -j$(nproc)
```

3. Готовая прошивка: `build/avalon1126.bin`

## Прошивка

Используйте kflash или ISP режим K210:
```bash
kflash -p /dev/ttyUSB0 -b 1500000 avalon1126.bin
```

## API Команды

Порт: 4028 (TCP)

| Команда | Описание |
|---------|----------|
| `version` | Версия CGMiner |
| `summary` | Статистика майнера |
| `pools` | Информация о пулах |
| `devs` | Информация об устройствах |
| `config` | Конфигурация |
| `stats` | Детальная статистика |

Пример:
```bash
echo '{"command":"summary"}' | nc localhost 4028
```

## Конфигурация

Настройки хранятся в flash (W25Q64):
- Pool URL, User, Password
- Частота ASIC (400-500 MHz)
- Напряжение (700-900 mV)
- Целевая температура (75°C)
- Режим вентилятора

## Лицензия

Основано на CGMiner 4.11.1 (GPLv3)

## Авторы

Восстановлено путём реверс-инжиниринга из donor_dump.bin
