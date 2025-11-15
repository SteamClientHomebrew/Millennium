cmake_minimum_required(VERSION 3.21)
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>" CACHE STRING "" FORCE)

include(ExternalProject)
include(FetchContent)
project(Millennium LANGUAGES C CXX)
include(../scripts/cmake/millennium_version.cmake)

add_compile_definitions(
    "_CRT_SECURE_NO_WARNINGS"    
    "USE_INTERNAL_FPCONV"
    "NGHTTP2_STATICLIB"
    "LJ_TARGET_X86"
    "LJ_HASJIT"
    "_WEBSOCKETPP_CPP11_THREAD_"
    "_WEBSOCKETPP_CPP11_TYPE_TRAITS_"
    "_WEBSOCKETPP_CPP11_RANDOM_DEVICE_"
    "ASIO_STANDALONE"
    "ASIO_HAS_STD_INVOKE_RESULT"
    "FMT_HEADER_ONLY"
    "CURL_STATICLIB"
)

include_directories(
    ${CMAKE_SOURCE_DIR}
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}/../vendor/fmt/include
    ${CMAKE_SOURCE_DIR}/../vendor/asio/asio/include
    ${CMAKE_SOURCE_DIR}/../vendor/nlohmann/include
    ${CMAKE_SOURCE_DIR}/../vendor/websocketpp
    ${CMAKE_SOURCE_DIR}/../vendor/crow/include
    ${CMAKE_SOURCE_DIR}/../vendor/ini/src
    ${CMAKE_SOURCE_DIR}/../vendor/incbin
    ${CMAKE_SOURCE_DIR}/../vendor/subhook
    ${CMAKE_SOURCE_DIR}/../vendor/plthook
    ${CMAKE_SOURCE_DIR}/emu/include
)

set(SOURCE_FILES
    runtime/libjs_interop.cc
    runtime/liblj_api.cc
    runtime/liblj_http.cc
    runtime/liblj_interop.cc
    runtime/liblj_logger.cc
    runtime/liblj_utils.cc
    runtime/liblj_fs.cc
    runtime/liblj_regex.cc
    runtime/liblj_datetime.cc
    runtime/libpy_api.cc
    runtime/libpy_gil.cc
    runtime/libpy_interop.cc
    runtime/libpy_logger.cc
    runtime/libpy_stdout_fwd.cc
    runtime/liblj_vfs_hook_api.cc
    core/auth.cc
    core/backend_init.cc
    core/backend_mgr.cc
    core/core_ipc.cc
    core/http_hook.cc
    core/init.cc
    core/lifecycle.cc
    core/millennium_api.cc
    core/millennium_updater.cc
    util/semver.cc
    util/zip.cc
    backend/css_parser.cc
    backend/default_cfg.cc
    backend/entry_point.cc
    backend/ipc_handler.cc
    backend/library_updater.cc
    backend/plugin_mgr.cc
    backend/scan.cc
    backend/sys_accent_col.cc
    backend/theme_cfg.cc
    backend/theme_mgr.cc
    backend/webkit.cc
    hooks/steam_hooks.cc
    ../hhx64/src/smem.cc
    sys/cfg.cc
    sys/env.cc
    sys/log.cc
    sys/sysfs.cc
    main.cc
)

if(WIN32)
    list(APPEND SOURCE_FILES "plat/init_win32.cc")
    if(CMAKE_BUILD_TYPE STREQUAL "Release")
        list(APPEND SOURCE_FILES "scripts/resources.rc")
    endif()
elseif(UNIX)
    list(APPEND SOURCE_FILES "plat/init_posix.cc")
    list(APPEND SOURCE_FILES "plat/init_posix_deprecated.cc")
    list(APPEND SOURCE_FILES "../vendor/subhook/subhook_x86.c")
    list(APPEND SOURCE_FILES "../vendor/subhook/subhook_linux.c")
    list(APPEND SOURCE_FILES "../vendor/plthook/plthook_elf.c")
endif()

if(WIN32)
    add_subdirectory(boot/win32)
elseif(UNIX AND NOT APPLE AND NOT NIX_BUILD)
    add_subdirectory(boot/linux)
elseif(UNIX AND APPLE)
    add_subdirectory(boot/macos)
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

include(../scripts/cmake/bootstrap_deps.cmake)

if(CMAKE_BUILD_TYPE STREQUAL "Debug")
    set(MILLENNIUM_SDK_DEVELOPMENT_MODE_ASSETS      "${CMAKE_SOURCE_DIR}/../sdk/typescript-packages/loader/build")
    set(MILLENNIUM_FRONTEND_DEVELOPMENT_MODE_ASSETS "${CMAKE_SOURCE_DIR}")

    add_compile_definitions(
             MILLENNIUM_SDK_DEVELOPMENT_MODE_ASSETS="${MILLENNIUM_SDK_DEVELOPMENT_MODE_ASSETS}"
        MILLENNIUM_FRONTEND_DEVELOPMENT_MODE_ASSETS="${MILLENNIUM_FRONTEND_DEVELOPMENT_MODE_ASSETS}"
    )
endif()

if(UNIX AND CMAKE_BUILD_TYPE STREQUAL "Release")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -s")
    set(CMAKE_WINDOWS_EXPORT_ALL_SYMBOLS OFF)
    set(CMAKE_BUILD_TYPE_INIT "Release")
    set(CMAKE_C_FLAGS_RELEASE "-DNDEBUG -Oz -ffunction-sections -fdata-sections")
    set(CMAKE_CXX_FLAGS_RELEASE "-DNDEBUG -Oz -ffunction-sections -fdata-sections")
    set(CMAKE_C_FLAGS_DEBUG "")
    set(CMAKE_CXX_FLAGS_DEBUG "")
endif()

include(../scripts/cmake/detect_aur_helper.cmake)

if(WIN32)
    include_directories(${CMAKE_SOURCE_DIR}/../vendor/python/win32)
elseif(APPLE)
    set(python_path "$ENV{HOME}/.pyenv/versions/3.11.8")
    include_directories("${python_path}/include/python3.11")
    set(MILLENNIUM__PYTHON_ENV "${python_path}")
    set(LIBPYTHON_RUNTIME_PATH "${python_path}/lib/libpython3.11.dylib")
    message(STATUS "[Millennium] Using python library ${LIBPYTHON_RUNTIME_PATH}")
else()
    set(default_python_path "/opt/python-i686-3.11.8")
    set(MILLENNIUM__PYTHON_ENV "${default_python_path}")
    set(LIBPYTHON_RUNTIME_PATH "${default_python_path}/lib/libpython-3.11.8.so")

    if(DISTRO_ARCH OR LSB_RELEASE_ID_SHORT STREQUAL "Arch")
        include_directories("${default_python_path}/include/python3.11/")
        detect_aur_helper(MILLENNIUM__UPDATE_SCRIPT_PROMPT)
    else()
        include_directories("${CMAKE_SOURCE_DIR}/../vendor/python/posix")
        set(MILLENNIUM__UPDATE_SCRIPT_PROMPT "curl -fsSL 'https://raw.githubusercontent.com/SteamClientHomebrew/Millennium/refs/heads/main/scripts/install.sh' | sh")
    endif()
    message(STATUS "[Millennium] Using python library ${LIBPYTHON_RUNTIME_PATH}")
endif()

include(../scripts/cmake/find_steam_path.cmake)
include(../scripts/cmake/generate_virtfs.cmake)

if(WIN32)
    add_library(Millennium SHARED "${SOURCE_FILES}")
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
endif()

target_include_directories(Millennium SYSTEM PRIVATE ${websocketpp_SOURCE_DIR})

if(WIN32)
    set_target_properties(Millennium PROPERTIES OUTPUT_NAME "millennium")
    set_target_properties(Millennium PROPERTIES PREFIX "")
    set_target_properties(Millennium PROPERTIES NO_EXPORT TRUE)
elseif(UNIX AND NOT APPLE)
    set_target_properties(Millennium PROPERTIES COMPILE_FLAGS "-m32" LINK_FLAGS "-m32")
    target_compile_options(Millennium PRIVATE -m32 -Wall -Wextra -Wpedantic -Werror)
    
    set_target_properties(Millennium PROPERTIES OUTPUT_NAME "millennium")
    set_target_properties(Millennium PROPERTIES PREFIX "lib")
    set_target_properties(Millennium PROPERTIES SUFFIX "_x86.so")
endif()

include(../scripts/cmake/windres.cmake)
target_link_libraries(Millennium luajit::lib libcurl minizip cjson libgit2package nlohmann_json fmt::fmt mINI)

if(WIN32)
    if(GITHUB_ACTION_BUILD)
        set(PYTHON_LIB "${CMAKE_SOURCE_DIR}/build/python/python311.lib")
    else()
        set(PYTHON_LIB ${CMAKE_SOURCE_DIR}/../vendor/python/python311.lib)
        add_custom_command(
            OUTPUT "${out_steam_path}/python311.dll"
            COMMAND ${CMAKE_COMMAND} -E copy_if_different "${CMAKE_SOURCE_DIR}/../vendor/python/python311.dll" "${out_steam_path}/python311.dll"
            COMMENT "Adding Millennium python runtime to Steam directory"
        )

        add_custom_target(copy_python311_dll ALL DEPENDS "${out_steam_path}/python311.dll" COMMENT "Ensuring python311.dll is present in Steam directory")
        add_dependencies(Millennium copy_python311_dll)
    endif()

    target_link_libraries(Millennium minhook ${PYTHON_LIB} ws2_32 crypt32 winhttp rpcrt4 secur32 bcrypt iphlpapi dbghelp schannel normaliz winmm)
    target_link_options(Millennium PRIVATE /NODEFAULTLIB:LIBCMT)
elseif(UNIX)
    target_link_options(Millennium PRIVATE -Wl,--no-undefined)

    if(APPLE)
        target_link_libraries(Millennium "$ENV{HOME}/.pyenv/versions/3.11.8/lib/libpython3.11.dylib")
    else()
        target_link_libraries(Millennium "/opt/python-i686-3.11.8/lib/libpython-3.11.8.so")
    endif()
endif()
