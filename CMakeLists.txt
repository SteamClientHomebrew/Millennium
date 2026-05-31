cmake_minimum_required(VERSION 3.21)
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>" CACHE STRING "Static MSVC Runtime Library" FORCE)
set(MILLENNIUM_BASE            ${CMAKE_SOURCE_DIR})

project(MillenniumBuildSlave)
add_compile_definitions(MILLENNIUM_$<IF:$<CONFIG:Debug>,DEBUG,RELEASE> MILLENNIUM_64BIT)

if(WIN32)
    # Windows build names
    set(MILLENNIUM_OUTPUT_NAME                     "millennium.dll"                )
    set(MILLENNIUM_HOOK_HELPER_OUTPUT_NAME         "millennium.hhx64.dll"          )
    set(MILLENNIUM_BOOTLOADER_OUTPUT_NAME          "millennium.bootstrap64.dll"    )
elseif(APPLE)
    # macOS build names
    set(MILLENNIUM_OUTPUT_NAME                     "libmillennium.dylib"           )
    set(MILLENNIUM_HOOK_HELPER_OUTPUT_NAME         "libmillennium_hhx64.dylib"     )
    set(MILLENNIUM_BOOTLOADER_OUTPUT_NAME          "libmillennium_bootstrap.dylib" )
    set(MILLENNIUM_CHILD_HOOK_OUTPUT_NAME          "libmillennium_child_hook.dylib")
else()
    # Linux build names
    set(MILLENNIUM_OUTPUT_NAME                     "libmillennium_x86.so"          )
    set(MILLENNIUM_HOOK_HELPER_OUTPUT_NAME         "libmillennium_hhx64.so"        )
    set(MILLENNIUM_BOOTLOADER_OUTPUT_NAME          "libmillennium_bootstrap_x86.so")
endif()

# Millennium lua host that runs the backend in a sandbox.
if(WIN32)
    set(MILLENNIUM_RTB_NAME "millennium.luavm64")
    set(MILLENNIUM_CRASH_HANDLER_NAME "millennium.crashhandler64")
else()
    set(MILLENNIUM_RTB_NAME "libmillennium_luavm_x86")
endif()

add_compile_definitions(__MILLENNIUM_OUTPUT_NAME__="${MILLENNIUM_OUTPUT_NAME}")
add_compile_definitions(__MILLENNIUM_HOOK_HELPER_OUTPUT_NAME__="${MILLENNIUM_HOOK_HELPER_OUTPUT_NAME}")
add_compile_definitions(__MILLENNIUM_BOOTLOADER_OUTPUT_NAME__="${MILLENNIUM_BOOTLOADER_OUTPUT_NAME}")
if(DEFINED MILLENNIUM_CHILD_HOOK_OUTPUT_NAME)
    add_compile_definitions(__MILLENNIUM_CHILD_HOOK_OUTPUT_NAME__="${MILLENNIUM_CHILD_HOOK_OUTPUT_NAME}")
endif()

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}")
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}")

include(FetchContent)
set(snare_SOURCE_DIR "${CMAKE_SOURCE_DIR}/thirdparty")

option(MILLENNIUM_BUILD_MAIN          "Build main Millennium library and tools"            ON)
option(MILLENNIUM_BUILD_TESTS         "Build testing suite for Millennium"                 ON)
option(MILLENNIUM_RUN_TESTS_ON_BUILD  "Run native tests automatically after building them" ON)
if(CMAKE_BUILD_TYPE STREQUAL "Debug")
    set(_build_to_steam_default ON)
else()
    set(_build_to_steam_default OFF)
endif()
option(MILLENNIUM_BUILD_TO_STEAM_PATH "Build Millennium to Steam path" ${_build_to_steam_default})
option(MILLENNIUM_NIX_64BIT           "Build only the 64-bit Nix targets (hhx64 + bootstrap64 + pvs64)" OFF)

if(MILLENNIUM_BUILD_MAIN)
    if(NOT MILLENNIUM_NIX_64BIT)
        add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/src)
    endif()

    if(NOT DISTRO_NIX OR MILLENNIUM_NIX_64BIT)
        add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/src/instrumentation/loopback)
    endif()

    if(MILLENNIUM_NIX_64BIT)
        add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/src/bootstrap/linux)
    endif()
endif()

if(MILLENNIUM_BUILD_TESTS)
    enable_testing()
    add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/tests)
endif()
