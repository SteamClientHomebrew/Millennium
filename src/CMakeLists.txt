cmake_minimum_required(VERSION 3.21)

set(VERSION_COMPANY      "Steam Homebrew")
set(VERSION_COPYRIGHT    "Steam Homebrew 2025")
set(VERSION_TRADEMARK1   "All Rights Reserved")
set(VERSION_DESCRIPTION  "A plugin loader for the modern Steam Client")
set(VERSION_FILENAME     "${MILLENNIUM_OUTPUT_NAME}")
set(VERSION_PRODUCT      "Millennium")

set(CMAKE_CXX_STANDARD 17 CACHE STRING "C++ standard to use" FORCE)
set(CMAKE_C_STANDARD   11 CACHE STRING "C standard to use"   FORCE)

include(ExternalProject)
include(FetchContent)
project(Millennium LANGUAGES C CXX)
include(${MILLENNIUM_BASE}/scripts/cmake/millennium_version.cmake)

add_compile_definitions(
    "_CRT_SECURE_NO_WARNINGS"
    "USE_INTERNAL_FPCONV"
    "NGHTTP2_STATICLIB"
    "_WEBSOCKETPP_CPP11_THREAD_"
    "_WEBSOCKETPP_CPP11_TYPE_TRAITS_"
    "_WEBSOCKETPP_CPP11_RANDOM_DEVICE_"
    "ASIO_STANDALONE"
    "ASIO_HAS_STD_INVOKE_RESULT"
    "FMT_HEADER_ONLY"
    "CURL_STATICLIB"
    $<$<BOOL:${WIN32}>:WIN32_LEAN_AND_MEAN>
    $<$<BOOL:${WIN32}>:NOMINMAX>
    $<$<CONFIG:Debug>:__HOOK_HELPER_OUTPUT_ABSPATH__="$<TARGET_FILE:hhx64>">
    $<$<CONFIG:Debug>:__LUA_HOST_OUTPUT_ABSPATH__="$<TARGET_FILE:${MILLENNIUM_RTB_NAME}>">
    $<$<CONFIG:Debug>:__DBG_MILLENNIUM_OUTPUT_DIR__="$<TARGET_FILE_DIR:Millennium>">
)


include_directories(
    ${MILLENNIUM_BASE}/src
    ${MILLENNIUM_BASE}/src/include
)
include_directories(SYSTEM
    ${snare_SOURCE_DIR}
)

set(SOURCE_FILES
    ${MILLENNIUM_BASE}/src/lua_host/lua_interop.cc
    engine/auth.cc
    engine/backend_init.cc
    engine/backend_mgr.cc
    engine/child_process.cc
    engine/cdp_api.cc
    engine/cdp_connector.cc
    engine/cmdline_parse.cc
    engine/core_ipc.cc
    engine/ffi_binder.cc
    engine/http_hooks.cc
    engine/lifecycle.cc
    engine/millennium_updater.cc
    engine/plugin_loader.cc
    engine/plugin_manager.cc
    engine/plugin_webkit_store.cc
    engine/plugin_webkit_world_mgr.cc
    engine/thread_pool.cc
    util/semver.cc
    util/zip.cc
    bindings/css_parser.cc
    bindings/default_cfg.cc
    bindings/entry_point.cc
    bindings/library_updater.cc
    bindings/plugin_mgr.cc
    bindings/scan.cc
    bindings/sys_accent_col.cc
    bindings/theme_cfg.cc
    bindings/theme_mgr.cc
    bindings/webkit.cc
    instrumentation/internal/steam_hooks.cc
    state/shared_memory.cc
    shared/crash_report.cc
    shared/crash_handler.cc
    shared/crash_handler_core.cc
    system/config.cc
    system/environment.cc
    system/filesystem.cc
    system/health_check.cc
    system/logger.cc
    millennium.cc
    mep/mep_message.cc
    mep/mep_router.cc
    mep/mep_server.cc
    mep/mep_hooks.cc
    mep/ffi_recorder.cc
    mep/console_capture.cc
    mep/crash_event_bus.cc
)

if(WIN32)
    configure_file(${MILLENNIUM_BASE}/scripts/version.rc.in ${CMAKE_BINARY_DIR}/millennium_version.rc @ONLY)

    list(APPEND SOURCE_FILES "platform/windows.cc")
    list(APPEND SOURCE_FILES "${CMAKE_BINARY_DIR}/millennium_version.rc")
    if(CMAKE_BUILD_TYPE STREQUAL "Release")
        list(APPEND SOURCE_FILES "${MILLENNIUM_BASE}/scripts/resources.rc")
    endif()
elseif(APPLE)
    enable_language(OBJCXX)
    list(APPEND SOURCE_FILES "platform/posix.cc")
    list(APPEND SOURCE_FILES "platform/macos.cc")
    list(APPEND SOURCE_FILES "platform/macos_menu.mm")
    list(APPEND SOURCE_FILES "platform/macos_window_style.mm")
    list(APPEND SOURCE_FILES "platform/macos_accent_color.mm")
elseif(UNIX)
    list(APPEND SOURCE_FILES "platform/posix.cc")
    list(APPEND SOURCE_FILES "platform/linux.cc")
    list(APPEND SOURCE_FILES "platform/linux_legacy.cc")
endif()

if(WIN32)
    add_subdirectory(bootstrap/win32)
elseif(UNIX AND NOT APPLE AND NOT NIX_BUILD)
    add_subdirectory(bootstrap/linux)
elseif(UNIX AND APPLE)
    add_subdirectory(bootstrap/macos)
endif()

add_subdirectory(lua_host)

if(WIN32)
    add_subdirectory(crash_handler)
endif()

if(UNIX AND NOT APPLE)
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -D_GNU_SOURCE")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -D_GNU_SOURCE")
    set(CMAKE_LIBRARY_PATH "/usr/lib32;/lib32" CACHE STRING "32-bit library search paths" FORCE)
    set(CMAKE_FIND_ROOT_PATH "/usr/lib32;/lib32" CACHE STRING "32-bit find root path" FORCE)
    set(ZLIBNG_FOUND FALSE CACHE BOOL "Disable zlib-ng detection" FORCE)
    set(ZLIBNG_LIBRARY "" CACHE FILEPATH "Empty zlib-ng library path" FORCE)
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -m32")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -m32")
    set(CMAKE_ASM_FLAGS "${CMAKE_ASM_FLAGS} -m32")
    set(LUAJIT_BUILD_FLAGS "-m32" CACHE STRING "LuaJIT build flags" FORCE)
    set(LUAJIT_TARGET_FLAGS "-m32" CACHE STRING "LuaJIT target flags" FORCE)
endif()

include(${MILLENNIUM_BASE}/scripts/cmake/bootstrap_deps.cmake)

target_include_directories(${MILLENNIUM_RTB_NAME} PRIVATE
    ${luajit_BINARY_DIR}
    ${luajit_SOURCE_DIR}/src
)

if(UNIX AND NOT APPLE)
    target_sources(${MILLENNIUM_RTB_NAME} PRIVATE
        ${lua_cjson_SOURCE_DIR}/lua_cjson.c
        ${lua_cjson_SOURCE_DIR}/strbuf.c
        ${lua_cjson_SOURCE_DIR}/g_fmt.c
        ${lua_cjson_SOURCE_DIR}/dtoa.c
    )
    target_include_directories(${MILLENNIUM_RTB_NAME} PRIVATE ${lua_cjson_SOURCE_DIR})
    target_compile_definitions(${MILLENNIUM_RTB_NAME} PRIVATE USE_INTERNAL_FPCONV)
    set_source_files_properties(
        ${lua_cjson_SOURCE_DIR}/lua_cjson.c
        TARGET_DIRECTORY ${MILLENNIUM_RTB_NAME}
        PROPERTIES COMPILE_OPTIONS "-w;-include;strings.h"
    )
elseif(WIN32 OR APPLE)
    target_link_libraries(${MILLENNIUM_RTB_NAME} cjson)
endif()

if(CMAKE_BUILD_TYPE STREQUAL "Debug")
    set(MILLENNIUM_SDK_DEVELOPMENT_MODE_ASSETS      "${MILLENNIUM_BASE}/src/typescript/sdk/packages/loader/build")
    set(MILLENNIUM_FRONTEND_DEVELOPMENT_MODE_ASSETS "${MILLENNIUM_BASE}/src")

    add_compile_definitions(
             MILLENNIUM_SDK_DEVELOPMENT_MODE_ASSETS="${MILLENNIUM_SDK_DEVELOPMENT_MODE_ASSETS}"
        MILLENNIUM_FRONTEND_DEVELOPMENT_MODE_ASSETS="${MILLENNIUM_FRONTEND_DEVELOPMENT_MODE_ASSETS}"
                                   MILLENNIUM_DEBUG=1
    )

    # Replace /Zi (shared PDB) with /Z7 (embed debug info per-object) to eliminate
    # PDB write races when compiling many TUs in parallel on MSVC.
    if(MSVC)
        string(REPLACE "/Zi" "/Z7" CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG}")
        string(REPLACE "/Zi" "/Z7" CMAKE_C_FLAGS_DEBUG   "${CMAKE_C_FLAGS_DEBUG}")
        set(CMAKE_CXX_FLAGS_DEBUG ${CMAKE_CXX_FLAGS_DEBUG} CACHE STRING "" FORCE)
        set(CMAKE_C_FLAGS_DEBUG   ${CMAKE_C_FLAGS_DEBUG}   CACHE STRING "" FORCE)
    endif()
endif()

if(UNIX AND CMAKE_BUILD_TYPE STREQUAL "Release")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS}")
    set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -s")
    set(CMAKE_WINDOWS_EXPORT_ALL_SYMBOLS OFF)
    set(CMAKE_BUILD_TYPE_INIT "Release")
    set(CMAKE_C_FLAGS_RELEASE "-DNDEBUG -Oz -ffunction-sections -fdata-sections")
    set(CMAKE_CXX_FLAGS_RELEASE "-DNDEBUG -Oz -ffunction-sections -fdata-sections")
    set(CMAKE_C_FLAGS_DEBUG "")
    set(CMAKE_CXX_FLAGS_DEBUG "")
endif()


include(${MILLENNIUM_BASE}/scripts/cmake/find_steam_path.cmake)

# Build TypeScript frontend during configure so that generate_virtfs.cmake
# can glob the chunk files and embed them via incbin.
if(CMAKE_BUILD_TYPE STREQUAL "Release" AND NOT DISTRO_NIX)
    find_program(BUN_EXECUTABLE bun REQUIRED)
    message(STATUS "[Millennium] Building TypeScript frontend...")
    execute_process(
        COMMAND ${BUN_EXECUTABLE} install
        WORKING_DIRECTORY "${MILLENNIUM_BASE}/src/typescript"
        RESULT_VARIABLE _bun_result
    )
    if(NOT _bun_result EQUAL 0)
        message(FATAL_ERROR "bun install failed")
    endif()
    execute_process(
        COMMAND ${BUN_EXECUTABLE} run build
        WORKING_DIRECTORY "${MILLENNIUM_BASE}/src/typescript"
        RESULT_VARIABLE _bun_result
    )
    if(NOT _bun_result EQUAL 0)
        message(FATAL_ERROR "bun run build failed")
    endif()
endif()

include(${MILLENNIUM_BASE}/scripts/cmake/generate_virtfs.cmake)

if(WIN32)
    add_library(Millennium SHARED "${SOURCE_FILES}")
elseif(UNIX)
    add_library(Millennium SHARED "${SOURCE_FILES}")

    add_compile_definitions(MILLENNIUM_SHARED)
    if(APPLE)
        target_compile_definitions(Millennium PRIVATE MILLENNIUM_MENU_ICON_PATH="${MILLENNIUM_BASE}/src/bootstrap/macos/AppIcon.icns")
    endif()
    if(DISTRO_NIX)
        target_compile_definitions(Millennium PRIVATE DISTRO_NIX)
    endif()
endif()



find_steam_path(Millennium)
target_include_directories(Millennium SYSTEM PRIVATE ${websocketpp_SOURCE_DIR})
set_target_properties(Millennium PROPERTIES
    PREFIX ""
    OUTPUT_NAME "${MILLENNIUM_OUTPUT_NAME}"
    SUFFIX ""
)

if(WIN32)
    set_target_properties(Millennium PROPERTIES NO_EXPORT TRUE)
elseif(UNIX AND NOT APPLE)
    set_target_properties(Millennium PROPERTIES COMPILE_FLAGS "-m32" LINK_FLAGS "-m32")
    target_compile_options(Millennium PRIVATE -m32 -Wall -Wextra -Wpedantic -Werror)
endif()

target_link_libraries(Millennium libcurl minizip nlohmann_json fmt::fmt)

if(WIN32)
    target_link_libraries(Millennium user32 ws2_32 crypt32 winhttp rpcrt4 secur32 bcrypt iphlpapi dbghelp schannel normaliz winmm)
    if(CMAKE_BUILD_TYPE STREQUAL "Debug")
        target_link_options(Millennium PRIVATE /NODEFAULTLIB:LIBCMT)
    endif()
elseif(APPLE)
    target_link_libraries(Millennium "-framework AppKit")
elseif(UNIX)
    target_link_options(Millennium PRIVATE -Wl,--no-undefined)
endif()
