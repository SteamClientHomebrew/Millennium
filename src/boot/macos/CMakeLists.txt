cmake_minimum_required(VERSION 3.16)
project(MacOSBootstrap LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)

foreach(required_var IN ITEMS MILLENNIUM_OUTPUT_NAME MILLENNIUM_BOOTLOADER_OUTPUT_NAME MILLENNIUM_HOOK_HELPER_OUTPUT_NAME MILLENNIUM_CHILD_HOOK_OUTPUT_NAME)
    if(NOT DEFINED ${required_var})
        message(FATAL_ERROR "${required_var} must be defined before configuring src/boot/macos")
    endif()
endforeach()

add_library(macos-bootstrap SHARED src/bootstrap.cc)
target_compile_definitions(macos-bootstrap PRIVATE
    __MILLENNIUM_OUTPUT_NAME__="${MILLENNIUM_OUTPUT_NAME}"
    __MILLENNIUM_HOOK_HELPER_OUTPUT_NAME__="${MILLENNIUM_HOOK_HELPER_OUTPUT_NAME}"
    __MILLENNIUM_CHILD_HOOK_OUTPUT_NAME__="${MILLENNIUM_CHILD_HOOK_OUTPUT_NAME}"
)
set_target_properties(macos-bootstrap PROPERTIES
    PREFIX ""
    OUTPUT_NAME "${MILLENNIUM_BOOTLOADER_OUTPUT_NAME}"
    SUFFIX ""
)

add_library(macos-child-hook SHARED src/child_hook.cc)
target_compile_definitions(macos-child-hook PRIVATE
    __MILLENNIUM_HOOK_HELPER_OUTPUT_NAME__="${MILLENNIUM_HOOK_HELPER_OUTPUT_NAME}"
)
set_target_properties(macos-child-hook PROPERTIES
    PREFIX ""
    OUTPUT_NAME "${MILLENNIUM_CHILD_HOOK_OUTPUT_NAME}"
    SUFFIX ""
)

add_executable(SteamWrapper src/wrapper.cc)
target_compile_definitions(SteamWrapper PRIVATE
    __MILLENNIUM_BOOTLOADER_OUTPUT_NAME__="${MILLENNIUM_BOOTLOADER_OUTPUT_NAME}"
    __MILLENNIUM_OUTPUT_NAME__="${MILLENNIUM_OUTPUT_NAME}"
    __MILLENNIUM_HOOK_HELPER_OUTPUT_NAME__="${MILLENNIUM_HOOK_HELPER_OUTPUT_NAME}"
    __MILLENNIUM_CHILD_HOOK_OUTPUT_NAME__="${MILLENNIUM_CHILD_HOOK_OUTPUT_NAME}"
    MILLENNIUM_ROOT="${CMAKE_SOURCE_DIR}"
)
set_target_properties(SteamWrapper PROPERTIES
    PREFIX ""
    OUTPUT_NAME "steam_osx"
    SUFFIX ""
)
