cmake_minimum_required(VERSION 3.10)
project(unix-hooks LANGUAGES CXX)
set(CMAKE_C_STANDARD 11)

add_library(unix-hooks SHARED main.c)

if (CMAKE_BUILD_TYPE STREQUAL "Debug")
    target_compile_definitions(unix-hooks PRIVATE MILLENNIUM_RUNTIME_PATH="$<TARGET_FILE:Millennium>")
endif()

# Ensure symbols are exported properly for library interposition
target_compile_options(unix-hooks PRIVATE -fvisibility=hidden)
target_link_options(unix-hooks PRIVATE -Wl,--no-undefined -ldl)

set_target_properties(unix-hooks PROPERTIES OUTPUT_NAME "millennium_bootstrap")
set_target_properties(unix-hooks PROPERTIES PREFIX "lib")
set_target_properties(unix-hooks PROPERTIES SUFFIX "_86x.so")

set_target_properties(unix-hooks PROPERTIES
    PREFIX ""
    OUTPUT_NAME "${MILLENNIUM_BOOTLOADER_OUTPUT_NAME}"
    SUFFIX ""
)
