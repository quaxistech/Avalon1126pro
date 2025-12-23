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
