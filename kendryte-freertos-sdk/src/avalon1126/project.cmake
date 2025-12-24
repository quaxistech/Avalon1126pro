# project.cmake for avalon1126
# Custom include directories and libraries

# lwIP include directories
target_include_directories(${PROJECT_NAME} PRIVATE
    ${SDK_ROOT}/third_party/lwip/src/include
)

# Link lwIP
target_link_libraries(${PROJECT_NAME}
    lwipcore
)

# Define macros
target_compile_definitions(${PROJECT_NAME} PRIVATE
    CGMINER_VERSION="4.11.1"
    API_VERSION="3.7"
    AVALON10_DRIVER=1
)

# Mock hardware definitions (for testing without real hardware)
# Set USE_MOCK_HARDWARE=ON in cmake to enable
if(USE_MOCK_HARDWARE)
    target_compile_definitions(${PROJECT_NAME} PRIVATE
        MOCK_HARDWARE=1
        MOCK_SPI_FLASH=1
        MOCK_NETWORK=1
        MOCK_ASIC=1
    )
    message(STATUS "avalon1126: Building with mock hardware support")
endif()
