# =============================================================================
# toolchain-riscv.cmake - Файл тулчейна для кросс-компиляции RISC-V
# =============================================================================
#
# Этот файл настраивает CMake для использования кросс-компилятора RISC-V.
# Используйте его при конфигурации:
#   cmake -DCMAKE_TOOLCHAIN_FILE=toolchain-riscv.cmake ..
#
# =============================================================================

# Система для которой собираем (embedded без ОС)
set(CMAKE_SYSTEM_NAME Generic)
set(CMAKE_SYSTEM_PROCESSOR riscv64)

# =============================================================================
# Путь к тулчейну
# =============================================================================

# Попробуйте найти тулчейн автоматически или укажите путь вручную
# Раскомментируйте и измените если тулчейн не в PATH:
# set(TOOLCHAIN_PATH "/opt/kendryte-toolchain/bin")
# set(CMAKE_FIND_ROOT_PATH ${TOOLCHAIN_PATH})

# Префикс имён утилит
set(CROSS_COMPILE "riscv64-unknown-elf-")

# =============================================================================
# Компиляторы
# =============================================================================

set(CMAKE_C_COMPILER ${CROSS_COMPILE}gcc)
set(CMAKE_CXX_COMPILER ${CROSS_COMPILE}g++)
set(CMAKE_ASM_COMPILER ${CROSS_COMPILE}gcc)

# =============================================================================
# Утилиты
# =============================================================================

set(CMAKE_OBJCOPY ${CROSS_COMPILE}objcopy CACHE INTERNAL "")
set(CMAKE_OBJDUMP ${CROSS_COMPILE}objdump CACHE INTERNAL "")
set(CMAKE_SIZE ${CROSS_COMPILE}size CACHE INTERNAL "")
set(CMAKE_AR ${CROSS_COMPILE}ar CACHE INTERNAL "")
set(CMAKE_RANLIB ${CROSS_COMPILE}ranlib CACHE INTERNAL "")
set(CMAKE_STRIP ${CROSS_COMPILE}strip CACHE INTERNAL "")

# =============================================================================
# Архитектура K210
# =============================================================================

# K210 поддерживает:
# - rv64imafdc (RV64GC) - базовый набор инструкций
# - Расширение "I" - целочисленные операции
# - Расширение "M" - умножение/деление
# - Расширение "A" - атомарные операции
# - Расширение "F" - одинарная точность FPU
# - Расширение "D" - двойная точность FPU
# - Расширение "C" - сжатые инструкции

set(ARCH_FLAGS "-march=rv64imafdc -mabi=lp64d -mcmodel=medany")

# =============================================================================
# Флаги компиляции
# =============================================================================

# Флаги C
set(CMAKE_C_FLAGS_INIT "${ARCH_FLAGS}")
set(CMAKE_C_FLAGS_DEBUG "-O0 -g3 -DDEBUG")
set(CMAKE_C_FLAGS_RELEASE "-O2 -DNDEBUG")
set(CMAKE_C_FLAGS_MINSIZEREL "-Os -DNDEBUG")
set(CMAKE_C_FLAGS_RELWITHDEBINFO "-O2 -g -DNDEBUG")

# Флаги C++
set(CMAKE_CXX_FLAGS_INIT "${ARCH_FLAGS}")
set(CMAKE_CXX_FLAGS_DEBUG "-O0 -g3 -DDEBUG")
set(CMAKE_CXX_FLAGS_RELEASE "-O2 -DNDEBUG")
set(CMAKE_CXX_FLAGS_MINSIZEREL "-Os -DNDEBUG")
set(CMAKE_CXX_FLAGS_RELWITHDEBINFO "-O2 -g -DNDEBUG")

# Флаги ассемблера
set(CMAKE_ASM_FLAGS_INIT "${ARCH_FLAGS} -x assembler-with-cpp")

# Флаги линковщика
set(CMAKE_EXE_LINKER_FLAGS_INIT "-nostartfiles -static")

# =============================================================================
# Настройки поиска библиотек
# =============================================================================

# Где искать библиотеки, заголовки и программы
set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_PACKAGE ONLY)

# =============================================================================
# Проверка компилятора
# =============================================================================

# Отключаем тестовую компиляцию (для кросс-компиляции)
set(CMAKE_C_COMPILER_WORKS 1)
set(CMAKE_CXX_COMPILER_WORKS 1)
set(CMAKE_TRY_COMPILE_TARGET_TYPE STATIC_LIBRARY)
