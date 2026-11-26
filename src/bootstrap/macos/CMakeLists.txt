cmake_minimum_required(VERSION 3.16)
project(MacOSBootstrap LANGUAGES C CXX OBJC)

set(CMAKE_CXX_STANDARD 17)

foreach(required_var IN ITEMS MILLENNIUM_OUTPUT_NAME MILLENNIUM_BOOTLOADER_OUTPUT_NAME MILLENNIUM_HOOK_HELPER_OUTPUT_NAME MILLENNIUM_CHILD_HOOK_OUTPUT_NAME)
    if(NOT DEFINED ${required_var})
        message(FATAL_ERROR "${required_var} must be defined before configuring src/bootstrap/macos")
    endif()
endforeach()

add_library(macos-bootstrap SHARED dylib_bootstrap.cc)
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

add_library(macos-child-hook SHARED child_hook.c)
set_target_properties(macos-child-hook PROPERTIES
    PREFIX ""
    OUTPUT_NAME "${MILLENNIUM_CHILD_HOOK_OUTPUT_NAME}"
    SUFFIX ""
)

# Inserted into every process steam_osx spawns, and dyld aborts a launch whose
# inserted library has no matching slice. On Apple silicon those processes can
# be arm64, arm64e (Apple system binaries) or x86_64 (under Rosetta).
if(CMAKE_OSX_ARCHITECTURES STREQUAL "arm64")
    set_target_properties(macos-child-hook PROPERTIES OSX_ARCHITECTURES "arm64;arm64e;x86_64")
endif()

# The `millennium` command (see cli/README.md), ad-hoc signed with cs.debugger.
add_executable(millennium-cli cli/millennium.c cli/native.m)
set_target_properties(millennium-cli PROPERTIES OUTPUT_NAME "millennium")
set_source_files_properties(cli/native.m PROPERTIES LANGUAGE OBJC COMPILE_OPTIONS -fobjc-arc)
target_link_libraries(millennium-cli PRIVATE "-framework AppKit")
add_custom_command(TARGET millennium-cli POST_BUILD
    COMMAND codesign --force --entitlements "${CMAKE_CURRENT_SOURCE_DIR}/cli/millennium.entitlements" -s - "$<TARGET_FILE:millennium-cli>"
    COMMENT "Ad-hoc signing millennium with cs.debugger entitlement"
)
