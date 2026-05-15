cmake_minimum_required(VERSION 3.10)
project(unix-hooks LANGUAGES CXX)
set(CMAKE_C_STANDARD 11)

if(NOT MILLENNIUM_NIX_64BIT)
    add_library(unix-hooks SHARED libmillennium_bootstrap.c)

    if (CMAKE_BUILD_TYPE STREQUAL "Debug")
        target_compile_definitions(unix-hooks PRIVATE MILLENNIUM_RUNTIME_PATH="$<TARGET_FILE:Millennium>")
    endif()

    target_compile_options(unix-hooks PRIVATE -fvisibility=hidden)
    target_link_options(unix-hooks PRIVATE -Wl,--no-undefined -ldl)

    set_target_properties(unix-hooks PROPERTIES
        PREFIX ""
        OUTPUT_NAME "${MILLENNIUM_BOOTLOADER_OUTPUT_NAME}"
        SUFFIX ""
    )
endif()

if(MILLENNIUM_NIX_64BIT OR NOT DISTRO_NIX)
    cmake_minimum_required(VERSION 3.10)
    project(bootstrap64 C)
    set(CMAKE_C_STANDARD 11)

    add_library(bootstrap64 SHARED libmillennium_hhx64.c)

    target_compile_options(bootstrap64 PRIVATE -fvisibility=hidden -m64)
    target_link_options(bootstrap64 PRIVATE -m64 -Wl,--no-undefined -ldl)

    if(CMAKE_BUILD_TYPE STREQUAL "Debug")
        target_compile_definitions(bootstrap64 PRIVATE MILLENNIUM_HHX64_PATH="$<TARGET_FILE:hhx64>")
    endif()

    target_compile_definitions(bootstrap64 PRIVATE MILLENNIUM_HHX64_NAME="${MILLENNIUM_HOOK_HELPER_OUTPUT_NAME}")

    set_target_properties(bootstrap64 PROPERTIES
        PREFIX ""
        OUTPUT_NAME "libmillennium_bootstrap_hhx64.so"
        SUFFIX ""
    )

    add_executable(libmillennium_pvs64 millennium_pvs64.cc)

    target_compile_options(libmillennium_pvs64 PRIVATE -m64 -Wall -Wextra)
    target_link_options(libmillennium_pvs64 PRIVATE -m64)

    set_target_properties(libmillennium_pvs64 PROPERTIES
        OUTPUT_NAME "libmillennium_pvs64"
        RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}"
    )
endif()
