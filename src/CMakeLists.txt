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
    "LJ_HASJIT"
    "_WEBSOCKETPP_CPP11_THREAD_"
    "_WEBSOCKETPP_CPP11_TYPE_TRAITS_"
    "_WEBSOCKETPP_CPP11_RANDOM_DEVICE_"
    "ASIO_STANDALONE"
    "ASIO_HAS_STD_INVOKE_RESULT"
    "FMT_HEADER_ONLY"
    "CURL_STATICLIB"
    __HOOK_HELPER_OUTPUT_ABSPATH__="$<TARGET_FILE:hhx64>"
    __DBG_MILLENNIUM_OUTPUT_DIR__="$<TARGET_FILE_DIR:Millennium>"
)

if (WIN32)
    add_compile_definitions("LJ_TARGET_X64")
elseif(UNIX AND NOT APPLE)
    add_compile_definitions("LJ_TARGET_X86")
endif()

include_directories(
    ${MILLENNIUM_BASE}/src
    ${MILLENNIUM_BASE}/src/include
    ${MILLENNIUM_BASE}/src/vendor/subhook
    ${MILLENNIUM_BASE}/src/vendor/plthook
)

set(SOURCE_FILES
    runtime/lua/lua_api.cc
    runtime/lua/lua_http.cc
    runtime/lua/lua_interop.cc
    runtime/lua/lua_logger.cc
    runtime/lua/lua_utils.cc
    runtime/lua/lua_filesystem.cc
    runtime/lua/lua_regex.cc
    runtime/lua/lua_datetime.cc
    runtime/lua/lua_hook_api.cc
    runtime/python/python_api.cc
    runtime/python/python_gil.cc
    runtime/python/python_interop.cc
    runtime/python/python_logger.cc
    runtime/python/python_stdout_hook.cc
    engine/auth.cc
    engine/backend_init.cc
    engine/backend_mgr.cc
    engine/cdp_api.cc
    engine/core_ipc.cc
    engine/ffi_binder.cc
    engine/http_hooks.cc
    engine/plugin_loader.cc
    engine/lifecycle.cc
    engine/millennium_updater.cc
    engine/plugin_webkit_store.cc
    engine/plugin_webkit_world_mgr.cc
    engine/thread_pool.cc
    util/semver.cc
    util/zip.cc
    backend/css_parser.cc
    backend/default_cfg.cc
    backend/entry_point.cc
    backend/library_updater.cc
    backend/plugin_mgr.cc
    backend/scan.cc
    backend/sys_accent_col.cc
    backend/theme_cfg.cc
    backend/theme_mgr.cc
    backend/webkit.cc
    instrumentation/steam_hooks.cc
    instrumentation/smem.cc
    system/config.cc
    system/environment.cc
    system/logger.cc
    system/filesystem.cc
    millennium.cc
)

if(WIN32)
    configure_file(${MILLENNIUM_BASE}/scripts/version.rc.in ${CMAKE_BINARY_DIR}/millennium_version.rc @ONLY)

    list(APPEND SOURCE_FILES "platform/windows.cc")
    list(APPEND SOURCE_FILES "${CMAKE_BINARY_DIR}/millennium_version.rc")
    if(CMAKE_BUILD_TYPE STREQUAL "Release")
        list(APPEND SOURCE_FILES "${MILLENNIUM_BASE}/scripts/resources.rc")
    endif()
elseif(UNIX)
    list(APPEND SOURCE_FILES "platform/linux.cc")
    list(APPEND SOURCE_FILES "platform/linux_legacy.cc")
    list(APPEND SOURCE_FILES "${MILLENNIUM_BASE}/src/vendor/subhook/subhook_x86.c")
    list(APPEND SOURCE_FILES "${MILLENNIUM_BASE}/src/vendor/subhook/subhook_linux.c")
    list(APPEND SOURCE_FILES "${MILLENNIUM_BASE}/src/vendor/plthook/plthook_elf.c")
endif()

if(WIN32)
    add_subdirectory(bootstrap/win32)
elseif(UNIX AND NOT APPLE AND NOT NIX_BUILD)
    add_subdirectory(bootstrap/linux)
elseif(UNIX AND APPLE)
    add_subdirectory(bootstrap/macos)
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

if(CMAKE_BUILD_TYPE STREQUAL "Debug")
    set(MILLENNIUM_SDK_DEVELOPMENT_MODE_ASSETS      "${MILLENNIUM_BASE}/src/sdk/packages/loader/build")
    set(MILLENNIUM_FRONTEND_DEVELOPMENT_MODE_ASSETS "${MILLENNIUM_BASE}/src")

    add_compile_definitions(
             MILLENNIUM_SDK_DEVELOPMENT_MODE_ASSETS="${MILLENNIUM_SDK_DEVELOPMENT_MODE_ASSETS}"
        MILLENNIUM_FRONTEND_DEVELOPMENT_MODE_ASSETS="${MILLENNIUM_FRONTEND_DEVELOPMENT_MODE_ASSETS}"
    )
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

include(${MILLENNIUM_BASE}/scripts/cmake/detect_aur_helper.cmake)

if(WIN32)
    include_directories(${MILLENNIUM_BASE}/src/vendor/python/win32)
elseif(APPLE)
    set(python_path "$ENV{HOME}/.pyenv/versions/3.11.8")
    include_directories("${python_path}/include/python3.11")
    set(MILLENNIUM__PYTHON_ENV "${python_path}")
    set(LIBPYTHON_RUNTIME_PATH "${python_path}/lib/libpython3.11.dylib")
    message(STATUS "[Millennium] Using python library ${LIBPYTHON_RUNTIME_PATH}")
elseif(DISTRO_NIX)
    set(default_python_path "${NIX_PYTHON}")
    set(MILLENNIUM__PYTHON_ENV "${NIX_PYTHON}")
    set(LIBPYTHON_RUNTIME_PATH "${NIX_PYTHON}/lib/libpython-3.11.8.so")

    include_directories("${NIX_PYTHON}/include/python3.11/")
    detect_aur_helper(MILLENNIUM__UPDATE_SCRIPT_PROMPT)
    set(MILLENNIUM__UPDATE_SCRIPT_PROMPT "echo {}")

    message(STATUS "[Millennium] Using python library ${LIBPYTHON_RUNTIME_PATH}")
else()
    set(default_python_path "/opt/python-i686-3.11.8")
    set(MILLENNIUM__PYTHON_ENV "${default_python_path}")
    set(LIBPYTHON_RUNTIME_PATH "${default_python_path}/lib/libpython-3.11.8.so")

    if(DISTRO_ARCH OR LSB_RELEASE_ID_SHORT STREQUAL "Arch")
        include_directories("${default_python_path}/include/python3.11/")
        detect_aur_helper(MILLENNIUM__UPDATE_SCRIPT_PROMPT)
    elseif(NIX_BUILD OR LSB_RELEASE_ID_SHORT STREQUAL "NixOS")
        include_directories("${default_python_path}/include/python3.11/")
        set(MILLENNIUM__UPDATE_SCRIPT_PROMPT "nix flake update millennium")
    else()
        include_directories("${MILLENNIUM_BASE}/src/vendor/python/posix")
        set(MILLENNIUM__UPDATE_SCRIPT_PROMPT "curl -fsSL 'https://steambrew.app/install.sh' | bash")
    endif()
    message(STATUS "[Millennium] Setting MILLENNIUM__UPDATE_SCRIPT_PROMPT to: ${MILLENNIUM__UPDATE_SCRIPT_PROMPT}")
    message(STATUS "[Millennium] Using python library ${LIBPYTHON_RUNTIME_PATH}")
endif()

include(${MILLENNIUM_BASE}/scripts/cmake/find_steam_path.cmake)
include(${MILLENNIUM_BASE}/scripts/cmake/generate_virtfs.cmake)

if(WIN32)
    add_library(Millennium SHARED "${SOURCE_FILES}")
    target_compile_definitions(Millennium PRIVATE MILLENNIUM__UPDATE_SCRIPT_PROMPT="https://github.com/SteamClientHomebrew/Installer/releases/latest/download/MillenniumInstaller-Windows.exe")
elseif(UNIX)
    if(APPLE)
        add_executable(Millennium "${SOURCE_FILES}")
    else()
        add_library(Millennium SHARED "${SOURCE_FILES}")
    endif()

    add_compile_definitions(MILLENNIUM_SHARED)
    target_compile_definitions(Millennium PRIVATE MILLENNIUM__PYTHON_ENV="${MILLENNIUM__PYTHON_ENV}")
    target_compile_definitions(Millennium PRIVATE LIBPYTHON_RUNTIME_PATH="${LIBPYTHON_RUNTIME_PATH}")
    target_compile_definitions(Millennium PRIVATE MILLENNIUM__UPDATE_SCRIPT_PROMPT="${MILLENNIUM__UPDATE_SCRIPT_PROMPT}")
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

target_link_libraries(Millennium luajit::lib libcurl minizip cjson libgit2package nlohmann_json fmt::fmt mINI)

if(WIN32)
    set(PYTHON_LIB "${MILLENNIUM_BASE}/src/vendor/python/lib64/python311.lib")
    set(PYTHON_LIB_IN_PATH "${MILLENNIUM_BASE}/src/vendor/python/lib64/python311.dll")
    set(PYTHON_LIB_OUT_PATH "${out_steam_path}/python311.dll")

    if (NOT GITHUB_ACTION_BUILD)
        add_custom_command(
            OUTPUT ${PYTHON_LIB_OUT_PATH}
            COMMAND ${CMAKE_COMMAND} -E copy_if_different ${PYTHON_LIB_IN_PATH} ${PYTHON_LIB_OUT_PATH}
            COMMENT "Adding Millennium python runtime to Steam directory"
        )

        add_custom_target(copy_python311_dll ALL DEPENDS ${PYTHON_LIB_OUT_PATH} COMMENT "Ensuring python311.dll is present in Steam directory")
        add_dependencies(Millennium copy_python311_dll)
    endif()

    target_link_libraries(Millennium minhook ${PYTHON_LIB} user32 ws2_32 crypt32 winhttp rpcrt4 secur32 bcrypt iphlpapi dbghelp schannel normaliz winmm)
    if(CMAKE_BUILD_TYPE STREQUAL "Debug")
        target_link_options(Millennium PRIVATE /NODEFAULTLIB:LIBCMT)
    endif()
elseif(UNIX)
    target_link_options(Millennium PRIVATE -Wl,--no-undefined)

    if(APPLE)
        target_link_libraries(Millennium "$ENV{HOME}/.pyenv/versions/3.11.8/lib/libpython3.11.dylib")
    elseif(DISTRO_NIX)
        target_link_libraries(Millennium "${NIX_PYTHON}/lib/libpython-3.11.8.so")
    else()
        target_link_libraries(Millennium "/opt/python-i686-3.11.8/lib/libpython-3.11.8.so")
    endif()
endif()
