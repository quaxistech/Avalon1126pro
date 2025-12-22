# =============================================================================
# Avalon A1126pro Firmware - Система сборки CMake
# =============================================================================
# Процессор: Kendryte K210 (RISC-V 64-bit, двухъядерный)
# SDK: Kendryte FreeRTOS SDK
# Компилятор: riscv64-unknown-elf-gcc
# =============================================================================

cmake_minimum_required(VERSION 3.10)

# Имя проекта
project(avalon_a1126pro_firmware
    VERSION 1.0.0
    DESCRIPTION "Прошивка для майнера Avalon A1126pro"
    LANGUAGES C CXX ASM
)

# =============================================================================
# Настройки компилятора для RISC-V K210
# =============================================================================

# Путь к тулчейну RISC-V (настройте под свою систему)
set(TOOLCHAIN_PREFIX "riscv64-unknown-elf-")

set(CMAKE_C_COMPILER "${TOOLCHAIN_PREFIX}gcc")
set(CMAKE_CXX_COMPILER "${TOOLCHAIN_PREFIX}g++")
set(CMAKE_ASM_COMPILER "${TOOLCHAIN_PREFIX}gcc")
set(CMAKE_OBJCOPY "${TOOLCHAIN_PREFIX}objcopy")
set(CMAKE_OBJDUMP "${TOOLCHAIN_PREFIX}objdump")
set(CMAKE_SIZE "${TOOLCHAIN_PREFIX}size")

# Флаги для K210 (RISC-V 64-bit с расширениями IMAFDC)
set(RISCV_FLAGS "-march=rv64imafdc -mabi=lp64d -mcmodel=medany")

# Флаги компиляции
set(CMAKE_C_FLAGS "${RISCV_FLAGS} -fno-common -ffunction-sections -fdata-sections -fstrict-volatile-bitfields")
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wall -Werror=all -Wno-error=unused-function -Wno-error=unused-variable")
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -O2 -g")

set(CMAKE_CXX_FLAGS "${CMAKE_C_FLAGS} -std=c++17")
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -std=gnu11")

# Флаги линковщика
set(CMAKE_EXE_LINKER_FLAGS "-nostartfiles -static -Wl,--gc-sections -Wl,-static")

# =============================================================================
# Пути к SDK Kendryte
# =============================================================================

set(SDK_ROOT "${CMAKE_SOURCE_DIR}/../kendryte-freertos-sdk")
set(SDK_LIB "${SDK_ROOT}/lib")

# Включаемые директории
include_directories(
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}/drivers
    ${SDK_LIB}/hal/include
    ${SDK_LIB}/bsp/include
    ${SDK_LIB}/freertos/include
    ${SDK_LIB}/freertos/portable/portmacro.h
    ${SDK_ROOT}/third_party/lwip/src/include
)

# =============================================================================
# Исходные файлы проекта
# =============================================================================

# Основные исходники прошивки
set(MAIN_SOURCES
    src/main.c                      # Точка входа и инициализация
    src/driver_avalon10.c           # Драйвер ASIC Avalon10
    src/stratum.c                   # Протокол Stratum для пулов
    src/http_server.c               # HTTP сервер и веб-интерфейс
    src/k210_hal.c                  # HAL для процессора K210
    src/network.c                   # Сетевой стек lwIP
    src/flash.c                     # Драйвер SPI Flash памяти
    src/fan_control.c               # Управление вентиляторами
    src/utils.c                     # Вспомогательные функции
)

# Стартовый код (ассемблер)
set(STARTUP_SOURCES
    src/startup.S                   # Код запуска RISC-V
)

# =============================================================================
# Создание исполняемого файла
# =============================================================================

add_executable(${PROJECT_NAME}
    ${MAIN_SOURCES}
    ${STARTUP_SOURCES}
)

# Скрипт линковки для K210
set(LINKER_SCRIPT "${CMAKE_SOURCE_DIR}/config/k210.ld")
set_target_properties(${PROJECT_NAME} PROPERTIES
    LINK_FLAGS "-T ${LINKER_SCRIPT}"
)

# =============================================================================
# Генерация бинарного файла прошивки
# =============================================================================

# Создание .bin файла из ELF
add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
    COMMAND ${CMAKE_OBJCOPY} -O binary 
            $<TARGET_FILE:${PROJECT_NAME}> 
            ${CMAKE_BINARY_DIR}/${PROJECT_NAME}.bin
    COMMENT "Генерация бинарного файла прошивки..."
)

# Вывод размера прошивки
add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
    COMMAND ${CMAKE_SIZE} $<TARGET_FILE:${PROJECT_NAME}>
    COMMENT "Размер прошивки:"
)

# =============================================================================
# Дополнительные цели
# =============================================================================

# Цель для прошивки устройства
add_custom_target(flash
    COMMAND kflash -p /dev/ttyUSB0 -b 1500000 ${CMAKE_BINARY_DIR}/${PROJECT_NAME}.bin
    DEPENDS ${PROJECT_NAME}
    COMMENT "Прошивка устройства через UART..."
)

# Цель для очистки
add_custom_target(clean_all
    COMMAND ${CMAKE_COMMAND} -E remove_directory ${CMAKE_BINARY_DIR}
    COMMENT "Полная очистка сборки..."
)

message(STATUS "============================================")
message(STATUS "Avalon A1126pro Firmware Build System")
message(STATUS "Версия: ${PROJECT_VERSION}")
message(STATUS "Процессор: Kendryte K210")
message(STATUS "============================================")
