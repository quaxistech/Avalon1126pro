# CMakeLists.txt for Avalon1126 CGMiner
# Target: Kendryte K210 with FreeRTOS
# Восстановлено из декомпиляции donor_dump.bin

cmake_minimum_required(VERSION 3.0)

# Project name
project(avalon1126)

# Source files
set(AVALON1126_SOURCES
    main.c
    avalon10.c
    pool.c
    stratum.c
    api.c
    network.c
    work.c
    config.c
    w25qxx.c
)

# Header files directory
include_directories(${CMAKE_CURRENT_LIST_DIR})
include_directories(${SDK_ROOT}/third_party/lwip/src/include)
include_directories(${SDK_ROOT}/third_party/lwip/src/include/lwip)
include_directories(${SDK_ROOT}/third_party/lwip/port)

# Create executable
add_executable(${PROJECT_NAME} ${AVALON1126_SOURCES})

# Link libraries
target_link_libraries(${PROJECT_NAME}
    freertos
    bsp
    drivers
    lwipcore
)

# Set compile options
target_compile_options(${PROJECT_NAME} PRIVATE
    -Wall
    -Wextra
    -O2
    -g
)

# Define preprocessor macros
target_compile_definitions(${PROJECT_NAME} PRIVATE
    __riscv64
    CGMINER_VERSION="4.11.1"
    AVALON10_DRIVER=1
)
