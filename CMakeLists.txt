cmake_minimum_required(VERSION 3.21)
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>" CACHE STRING "Static MSVC Runtime Library" FORCE)
set(MILLENNIUM_BASE            ${CMAKE_SOURCE_DIR}                     CACHE PATH   "Base path to Millennium source"   )

project(MillenniumBuildSlave)
add_compile_definitions(MILLENNIUM_$<IF:$<CONFIG:Debug>,DEBUG,RELEASE> MILLENNIUM_64BIT)

if(WIN32)
    # Windows build names
    set(MILLENNIUM_OUTPUT_NAME                     "millennium.dll"                )
    set(MILLENNIUM_HOOK_HELPER_OUTPUT_NAME         "millennium.hhx64.dll"          )
    set(MILLENNIUM_BOOTLOADER_OUTPUT_NAME          "wsock32.dll"                   )
    set(MILLENNIUM_BOOTLOADER_OUTPUT_NAME_LEGACY   "version.dll"                   )
else()
    # Linux build names
    set(MILLENNIUM_OUTPUT_NAME                     "libmillennium_x86.so"          )
    set(MILLENNIUM_HOOK_HELPER_OUTPUT_NAME         "libmillennium_hhx64.so"        )
    set(MILLENNIUM_BOOTLOADER_OUTPUT_NAME          "libmillennium_bootstrap_86x.so")
endif()

add_compile_definitions(__MILLENNIUM_OUTPUT_NAME__="${MILLENNIUM_OUTPUT_NAME}")
add_compile_definitions(__MILLENNIUM_HOOK_HELPER_OUTPUT_NAME__="${MILLENNIUM_HOOK_HELPER_OUTPUT_NAME}")
add_compile_definitions(__MILLENNIUM_BOOTLOADER_OUTPUT_NAME__="${MILLENNIUM_BOOTLOADER_OUTPUT_NAME}")

add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/src)
add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/src/instrumentation)
