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
    set(MILLENNIUM_BOOTLOADER_OUTPUT_NAME          "libmillennium_bootstrap_86x.so")
endif()

# Millennium lua host that runs the backend in a sandbox.
if(WIN32)
    set(MILLENNIUM_RTB_NAME "millennium.luasb64" CACHE STRING "Lua sandbox executable name")
    set(MILLENNIUM_CRASH_HANDLER_NAME "millennium.crashhandler64" CACHE STRING "Crash handler executable name")
else()
    set(MILLENNIUM_RTB_NAME "millennium_luasb86" CACHE STRING "Lua sandbox executable name")
endif()

add_compile_definitions(__MILLENNIUM_OUTPUT_NAME__="${MILLENNIUM_OUTPUT_NAME}")
add_compile_definitions(__MILLENNIUM_HOOK_HELPER_OUTPUT_NAME__="${MILLENNIUM_HOOK_HELPER_OUTPUT_NAME}")
add_compile_definitions(__MILLENNIUM_BOOTLOADER_OUTPUT_NAME__="${MILLENNIUM_BOOTLOADER_OUTPUT_NAME}")
if(DEFINED MILLENNIUM_CHILD_HOOK_OUTPUT_NAME)
    add_compile_definitions(__MILLENNIUM_CHILD_HOOK_OUTPUT_NAME__="${MILLENNIUM_CHILD_HOOK_OUTPUT_NAME}")
endif()

# In release mode, put all build outputs directly in build/ so paths
# don't break if the source tree is moved.
if(CMAKE_BUILD_TYPE STREQUAL "Release")
    set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}")
    set(CMAKE_LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}")
endif()

include(FetchContent)
FetchContent_Declare(snare GIT_REPOSITORY https://github.com/shdwmtr/libsnare.h GIT_TAG main)
FetchContent_MakeAvailable(snare)

add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/src)
add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/src/instrumentation/loopback)
