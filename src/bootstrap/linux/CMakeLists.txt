cmake_minimum_required(VERSION 3.10)
project(unix-hooks LANGUAGES CXX)
set(CMAKE_C_STANDARD 11)

add_library(unix-hooks SHARED libmillennium_bootstrap.c)

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
    OUTPUT_NAME "libXtst.so.6"
    SUFFIX ""
)

if(NOT GITHUB_ACTION_BUILD)
    set(steam_ubuntu64 "$ENV{HOME}/.steam/steam/ubuntu12_64")
    if(EXISTS "${steam_ubuntu64}")
        if(CMAKE_BUILD_TYPE STREQUAL "Debug")
            add_custom_command(TARGET bootstrap64 POST_BUILD
                COMMAND ${CMAKE_COMMAND} -E create_symlink
                    $<TARGET_FILE:bootstrap64>
                    "${steam_ubuntu64}/libXtst.so.6"
                COMMENT "Symlinking libXtst.so.6 bootstrap into ubuntu12_64"
            )
        else()
            add_custom_command(TARGET bootstrap64 POST_BUILD
                COMMAND ${CMAKE_COMMAND} -E copy_if_different
                    $<TARGET_FILE:bootstrap64>
                    "${steam_ubuntu64}/libXtst.so.6"
                COMMENT "Copying libXtst.so.6 bootstrap into ubuntu12_64"
            )
        endif()
    endif()
endif()
