message(STATUS "[Millennium] Configuring & verifying dependencies...")

cmake_minimum_required(VERSION 3.10...3.21)
include(ExternalProject)
include(FetchContent)

# Add _GNU_SOURCE globally for POSIX compliance (needed for strncasecmp, mkstemp, etc.)
if(UNIX AND NOT APPLE)
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -D_GNU_SOURCE")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -D_GNU_SOURCE")

    # Force 32-bit library paths to prevent mixing 64-bit libraries
    set(CMAKE_LIBRARY_PATH "/usr/lib32;/lib32" CACHE STRING "32-bit library search paths" FORCE)
    set(CMAKE_FIND_ROOT_PATH "/usr/lib32;/lib32" CACHE STRING "32-bit find root path" FORCE)
    # Explicitly disable zlib-ng to prevent conflicts
    set(ZLIBNG_FOUND FALSE CACHE BOOL "Disable zlib-ng detection" FORCE)
    set(ZLIBNG_LIBRARY "" CACHE FILEPATH "Empty zlib-ng library path" FORCE)

    # Ensure LuaJIT is built with 32-bit flags
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -m32")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -m32")
    set(CMAKE_ASM_FLAGS "${CMAKE_ASM_FLAGS} -m32")

    # Also set LUAJIT specific variables for the wrapper
    set(LUAJIT_BUILD_FLAGS "-m32" CACHE STRING "LuaJIT build flags" FORCE)
    set(LUAJIT_TARGET_FLAGS "-m32" CACHE STRING "LuaJIT target flags" FORCE)
endif()

# =====================================
# Dep Build Configuration
# =====================================

# General settings
set(BUILD_TESTS                 OFF CACHE BOOL     "Disable building tests")
set(BUILD_EXAMPLES              OFF CACHE BOOL     "Disable building examples")

set(BUILD_SHARED_LIBS           OFF CACHE BOOL     "Build static libraries by default")
set(CMAKE_CXX_STANDARD          17  CACHE STRING   "C++ standard to use" FORCE)
set(CMAKE_C_STANDARD            11  CACHE STRING   "C standard to use" FORCE)
set(CMAKE_CXX_STANDARD_REQUIRED ON  CACHE BOOL     "Enforce C++ standard" FORCE)
set(CMAKE_C_STANDARD_REQUIRED   ON  CACHE BOOL     "Enforce C standard" FORCE)

set(USE_INTERNAL_FPCONV         ON  CACHE BOOL     "Use internal strtod() / g_fmt() code for performance")
set(NGHTTP2_STATICLIB           ON  CACHE BOOL     "Use static nghttp2 library")

# Ensure all libraries are position independent for static linking
set(CMAKE_POSITION_INDEPENDENT_CODE ON CACHE BOOL "" FORCE)

if(WIN32)
    set(CURL_USE_SCHANNEL           ON  CACHE INTERNAL "" FORCE)
    set(CURL_WINDOWS_SSPI           ON  CACHE INTERNAL "" FORCE)
    set(CURL_USE_OPENSSL            OFF CACHE BOOL     "Use OpenSSL for TLS/SSL" FORCE)
else()
    set(CURL_USE_OPENSSL            ON  CACHE BOOL     "Use OpenSSL for TLS/SSL" FORCE)
    set(CURL_USE_SCHANNEL           OFF CACHE INTERNAL "" FORCE)
    set(CURL_WINDOWS_SSPI           OFF CACHE INTERNAL "" FORCE)
    set(REGEX_BACKEND               "builtin" CACHE STRING "Use builtin regex instead of PCRE" FORCE)
endif()

set(CURL_USE_SECTRANSP          OFF CACHE BOOL     "Disable SecureTransport")
set(CURL_USE_MBEDTLS            OFF CACHE BOOL     "Disable mbedTLS")
set(CURL_USE_WOLFSSL            OFF CACHE BOOL     "Disable wolfSSL")
set(CURL_USE_BEARSSL            OFF CACHE BOOL     "Disable BearSSL")
set(CURL_STATICLIB              ON  CACHE BOOL     "Build curl as static library")
set(CURL_DISABLE_INSTALL        ON  CACHE BOOL     "Disable curl install")
set(CURL_DISABLE_LDAP           ON  CACHE BOOL     "Disable LDAP")
set(CURL_DISABLE_LDAPS          ON  CACHE BOOL     "Disable LDAPS")
set(CURL_DISABLE_RTSP           ON  CACHE BOOL     "Disable RTSP")
set(CURL_DISABLE_PROXY          OFF CACHE BOOL     "Disable proxy support")
set(CURL_DISABLE_DICT           ON  CACHE BOOL     "Disable DICT")
set(CURL_DISABLE_TELNET         ON  CACHE BOOL     "Disable TELNET")
set(CURL_DISABLE_TFTP           ON  CACHE BOOL     "Disable TFTP")
set(CURL_DISABLE_POP3           ON  CACHE BOOL     "Disable POP3")
set(CURL_DISABLE_IMAP           ON  CACHE BOOL     "Disable IMAP")
set(CURL_DISABLE_SMTP           ON  CACHE BOOL     "Disable SMTP")
set(CURL_DISABLE_GOPHER         ON  CACHE BOOL     "Disable Gopher")

set(FMT_HEADER_ONLY             ON  CACHE BOOL     "" FORCE)
set(FMT_DOC                     OFF CACHE BOOL     "" FORCE)
set(FMT_TEST                    OFF CACHE BOOL     "" FORCE)
set(FMT_INSTALL                 ON  CACHE BOOL     "" FORCE)

set(MZ_BZIP2                    OFF CACHE BOOL     "Disable bzip2 support in minizip-ng")

add_compile_definitions(
    USE_INTERNAL_FPCONV

    # Ensure nghttp2 is statically linked at compile time for libcurl
    NGHTTP2_STATICLIB
    LJ_TARGET_X86
    LJ_HASJIT

    # make websocketpp use c++11 and standalone asio instead of boost
    _WEBSOCKETPP_CPP11_THREAD_
    _WEBSOCKETPP_CPP11_TYPE_TRAITS_
    _WEBSOCKETPP_CPP11_RANDOM_DEVICE_

    ASIO_STANDALONE
    ASIO_HAS_STD_INVOKE_RESULT

    FMT_HEADER_ONLY
    _CRT_SECURE_NO_WARNINGS
)

# Patch the minimum cmake version in a "submodule".
# Mingw decided they would force the new version of cmake which completely breaks older deps
# Only fix is a shim or fixing it up stream :shrug:

function(millennium_process_package input_path)
    if(EXISTS "${input_path}/CMakeLists.txt")
        file(READ "${input_path}/CMakeLists.txt" CMAKELISTS_CONTENTS)
        string(REGEX REPLACE "cmake_minimum_required[ \t]*\\([ \t]*VERSION[^)]*\\)" "cmake_minimum_required(VERSION 3.10...3.21)" CMAKELISTS_CONTENTS "${CMAKELISTS_CONTENTS}")
        file(WRITE "${input_path}/CMakeLists.txt" "${CMAKELISTS_CONTENTS}")
    else()
        message(WARNING "Could not find ${input_path} to patch.")
    endif()
endfunction()

# ====================================
# Fetch and configure dependencies
# ====================================
message(STATUS "[Millennium] Fetching dependencies...")

FetchContent_Declare(luajit        GIT_REPOSITORY https://github.com/SteamClientHomebrew/LuaJIT    GIT_TAG v2.1           SOURCE_SUBDIR fakedir)
FetchContent_Declare(lua_cjson     GIT_REPOSITORY https://github.com/SteamClientHomebrew/LuaJSON   GIT_TAG v1.0.2         SOURCE_SUBDIR fakedir)
FetchContent_Declare(minhook       GIT_REPOSITORY https://github.com/TsudaKageyu/minhook           GIT_TAG v1.3.4         SOURCE_SUBDIR fakedir)
FetchContent_Declare(mini          GIT_REPOSITORY https://github.com/metayeti/mINI                 GIT_TAG 0.9.18         SOURCE_SUBDIR fakedir)
FetchContent_Declare(websocketpp   GIT_REPOSITORY https://github.com/zaphoyd/websocketpp           GIT_TAG 0.8.2          SOURCE_SUBDIR fakedir)
FetchContent_Declare(fmt           GIT_REPOSITORY https://github.com/fmtlib/fmt                    GIT_TAG 12.0.0         SOURCE_SUBDIR fakedir)
FetchContent_Declare(nlohmann_json GIT_REPOSITORY https://github.com/nlohmann/json                 GIT_TAG v3.12.0        SOURCE_SUBDIR fakedir)
FetchContent_Declare(libgit2       GIT_REPOSITORY https://github.com/libgit2/libgit2               GIT_TAG v1.9.1         SOURCE_SUBDIR fakedir)
FetchContent_Declare(minizip_ng    GIT_REPOSITORY https://github.com/zlib-ng/minizip-ng            GIT_TAG 4.0.10         SOURCE_SUBDIR fakedir)
FetchContent_Declare(curl          GIT_REPOSITORY https://github.com/curl/curl                     GIT_TAG curl-8_13_0    SOURCE_SUBDIR fakedir)
FetchContent_Declare(incbin        GIT_REPOSITORY https://github.com/graphitemaster/incbin         GIT_TAG main           SOURCE_SUBDIR fakedir)
FetchContent_Declare(asio          GIT_REPOSITORY https://github.com/chriskohlhoff/asio            GIT_TAG asio-1-30-0    SOURCE_SUBDIR fakedir)

# Download all deps without configuring them
set(DEPENDENCIES luajit incbin websocketpp fmt nlohmann_json libgit2 minizip_ng mINI lua_cjson curl asio)

# Append min-hook only on Windows
if(WIN32)
    list(APPEND DEPENDENCIES minhook)
endif()

foreach(dep ${DEPENDENCIES})
    string(TIMESTAMP start_time "%s")
    FetchContent_MakeAvailable(${dep})
    string(TIMESTAMP end_time "%s")
    math(EXPR duration "${end_time} - ${start_time}")

    message(STATUS "[Millennium] ${dep} completed in ${duration} seconds")
endforeach()

# Setup LuaJIT variables for use by lua_cjson
set(LUA_INCLUDE_DIR  luajit::header)
set(LUA_INCLUDE_DIRS luajit::header)
set(LUA_LIBRARY      luajit::lib   )
set(LUA_LIBRARIES    luajit::lib   )

# Include directories for deps
include_directories("${libgit2_SOURCE_DIR}/include")
include_directories("${minizip_ng_SOURCE_DIR}")
include_directories("${incbin_SOURCE_DIR}")
include_directories("${asio_SOURCE_DIR}/asio/include")
include_directories("${luajit_BINARY_DIR}")
include_directories("${luajit_SOURCE_DIR}/src")

# Force all dependencies to use Millennium's cmake version (3.10 - 3.21)
millennium_process_package("${luajit_SOURCE_DIR}")
millennium_process_package("${curl_SOURCE_DIR}")
millennium_process_package("${websocketpp_SOURCE_DIR}")
millennium_process_package("${fmt_SOURCE_DIR}")
millennium_process_package("${nlohmann_json_SOURCE_DIR}")
millennium_process_package("${libgit2_SOURCE_DIR}")
millennium_process_package("${minizip_ng_SOURCE_DIR}")
millennium_process_package("${mini_SOURCE_DIR}")
millennium_process_package("${lua_cjson_SOURCE_DIR}")

if(WIN32)
    include_directories("${CMAKE_SOURCE_DIR}/vendor/pdcurses")
    millennium_process_package("${minhook_SOURCE_DIR}/")
endif()

# Let cmake finally configure after we've processed them
add_subdirectory("${luajit_SOURCE_DIR}"        "${luajit_BINARY_DIR}"       )
add_subdirectory("${curl_SOURCE_DIR}"          "${curl_BINARY_DIR}"         )
add_subdirectory("${websocketpp_SOURCE_DIR}"   "${websocketpp_BINARY_DIR}"  )
add_subdirectory("${fmt_SOURCE_DIR}"           "${fmt_BINARY_DIR}"          )
add_subdirectory("${nlohmann_json_SOURCE_DIR}" "${nlohmann_json_BINARY_DIR}")
add_subdirectory("${libgit2_SOURCE_DIR}"       "${libgit2_BINARY_DIR}"      )
add_subdirectory("${minizip_ng_SOURCE_DIR}"    "${minizip_ng_BINARY_DIR}"   )
add_subdirectory("${mini_SOURCE_DIR}"          "${mini_BINARY_DIR}"         )
add_subdirectory("${lua_cjson_SOURCE_DIR}"     "${lua_cjson_BINARY_DIR}"    )

if(WIN32)
    add_subdirectory("${minhook_SOURCE_DIR}"       "${minhook_BINARY_DIR}"      )
endif()

# Make sure LuaJIT is built before cjson
add_dependencies(cjson luajit::lib luajit::header)

# =====================================
# Begin Millennium config here
# =====================================

project(Millennium LANGUAGES C CXX)

# =====================================
# Version and Git Configuration
# =====================================

# Get Millennium version from version file
file(STRINGS "${CMAKE_SOURCE_DIR}/version" VERSION_LINES LIMIT_COUNT 2)
list(GET VERSION_LINES 1 MILLENNIUM_VERSION)

# Add configure dependency on version file
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/version")

# Configure version header
configure_file(${CMAKE_SOURCE_DIR}/src/include/millennium/version.h.in ${CMAKE_BINARY_DIR}/version.h)

# Get current git commit hash
execute_process(COMMAND git rev-parse HEAD WORKING_DIRECTORY ${CMAKE_SOURCE_DIR} OUTPUT_VARIABLE GIT_COMMIT_HASH OUTPUT_STRIP_TRAILING_WHITESPACE)

message(STATUS "[Millennium] Version: ${MILLENNIUM_VERSION}")
message(STATUS "[Millennium] Git commit hash: ${GIT_COMMIT_HASH}")

# Global compile definitions
add_compile_definitions(GIT_COMMIT_HASH="${GIT_COMMIT_HASH}" MILLENNIUM_ROOT="${CMAKE_SOURCE_DIR}" MILLENNIUM_VERSION="${MILLENNIUM_VERSION}")

# Debug mode specific settings
if(CMAKE_BUILD_TYPE STREQUAL "Debug")
    set(MILLENNIUM_SDK_DEVELOPMENT_MODE_ASSETS      "${CMAKE_SOURCE_DIR}/sdk/typescript-packages/loader/build")
    set(MILLENNIUM_FRONTEND_DEVELOPMENT_MODE_ASSETS "${CMAKE_SOURCE_DIR}/src")

    add_compile_definitions(
             MILLENNIUM_SDK_DEVELOPMENT_MODE_ASSETS="${MILLENNIUM_SDK_DEVELOPMENT_MODE_ASSETS}"
        MILLENNIUM_FRONTEND_DEVELOPMENT_MODE_ASSETS="${MILLENNIUM_FRONTEND_DEVELOPMENT_MODE_ASSETS}"
    )
endif()

# Nix OS detection
if(DEFINED ENV{NIX_OS})
    message(STATUS "Building Millennium for Nix")
    set(NIX_BUILD ON)
endif()

# =====================================
# Build Frontend Assets
# =====================================
# Find pnpm executable
find_program(PNPM_EXECUTABLE pnpm)
if(NOT PNPM_EXECUTABLE)
    message(FATAL_ERROR "pnpm is required but not found. Please install pnpm.")
endif()
message(STATUS "[Millennium] Found pnpm at: ${PNPM_EXECUTABLE}")

if(GITHUB_ACTION_BUILD)
    if (WIN32)
        find_program(BASH_EXECUTABLE bash)
        if(BASH_EXECUTABLE)
            set(PNPM_COMMAND ${BASH_EXECUTABLE} ${PNPM_EXECUTABLE})
        else()
            # Fallback to direct execution
            set(PNPM_COMMAND ${PNPM_EXECUTABLE})
        endif()
    elseif(UNIX)
        set(PNPM_COMMAND ${PNPM_EXECUTABLE})
    else()
        message(FATAL_ERROR "Unsupported platform for GitHub Action build.")
    endif()
else()
    set(PNPM_COMMAND ${PNPM_EXECUTABLE})
endif()


# =========================
# Build SDK
# =========================

if (CMAKE_BUILD_TYPE STREQUAL "Release")

    message(STATUS "[Millennium] Building SDK with ${PNPM_COMMAND}...")
    execute_process(
    COMMAND ${PNPM_COMMAND} install
    WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}/sdk"
    RESULT_VARIABLE SDK_INSTALL_RESULT
    OUTPUT_VARIABLE SDK_INSTALL_OUTPUT
    ERROR_VARIABLE SDK_INSTALL_ERROR
)

    if(NOT SDK_INSTALL_RESULT EQUAL 0)
        message(STATUS "[Millennium] SDK pnpm install stdout:\n${SDK_INSTALL_OUTPUT}")
        message(STATUS "[Millennium] SDK pnpm install stderr:\n${SDK_INSTALL_ERROR}")
        message(FATAL_ERROR "[Millennium] SDK pnpm install failed with exit code ${SDK_INSTALL_RESULT}")
    endif()

    execute_process(
    COMMAND ${PNPM_COMMAND} run build
    WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}/sdk"
    RESULT_VARIABLE SDK_BUILD_RESULT
    OUTPUT_VARIABLE SDK_BUILD_OUTPUT
    ERROR_VARIABLE SDK_BUILD_ERROR
)

    if(NOT SDK_BUILD_RESULT EQUAL 0)
        message(STATUS "[Millennium] SDK build stdout:\n${SDK_BUILD_OUTPUT}")
        message(STATUS "[Millennium] SDK build stderr:\n${SDK_BUILD_ERROR}")
        message(FATAL_ERROR "[Millennium] SDK build failed with exit code ${SDK_BUILD_RESULT}")
    endif()

    message(STATUS "[Millennium] SDK build completed successfully")

    # =========================
    # Build Frontend
    # =========================
    message(STATUS "[Millennium] Building frontend...")
    execute_process(
    COMMAND ${PNPM_COMMAND} install
    WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}/src/frontend"
    RESULT_VARIABLE FRONTEND_INSTALL_RESULT
    OUTPUT_VARIABLE FRONTEND_INSTALL_OUTPUT
    ERROR_VARIABLE FRONTEND_INSTALL_ERROR
)

    if(NOT FRONTEND_INSTALL_RESULT EQUAL 0)
        message(STATUS "[Millennium] Frontend pnpm install stdout:\n${FRONTEND_INSTALL_OUTPUT}")
        message(STATUS "[Millennium] Frontend pnpm install stderr:\n${FRONTEND_INSTALL_ERROR}")
        message(FATAL_ERROR "[Millennium] Frontend pnpm install failed with exit code ${FRONTEND_INSTALL_RESULT}")
    endif()

    execute_process(
    COMMAND ${PNPM_COMMAND} run build
    WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}/src/frontend"
    RESULT_VARIABLE FRONTEND_BUILD_RESULT
    OUTPUT_VARIABLE FRONTEND_BUILD_OUTPUT
    ERROR_VARIABLE FRONTEND_BUILD_ERROR
)

    if(NOT FRONTEND_BUILD_RESULT EQUAL 0)
        message(STATUS "[Millennium] Frontend build stdout:\n${FRONTEND_BUILD_OUTPUT}")
        message(STATUS "[Millennium] Frontend build stderr:\n${FRONTEND_BUILD_ERROR}")
        message(FATAL_ERROR "[Millennium] Frontend build failed with exit code ${FRONTEND_BUILD_RESULT}")
    endif()

    message(STATUS "[Millennium] Frontend build completed successfully")
endif()

# =====================================
# Platform-Specific Compiler Flags
# =====================================

# 32-bit compilation (except Apple)
if(NOT APPLE)
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -m32")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -m32")
endif()

# MinGW optimizations
if(MINGW)
    set(CMAKE_C_FLAGS_RELEASE "${CMAKE_C_FLAGS_RELEASE} -Os -ffunction-sections -fdata-sections")
    set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} -Os -ffunction-sections -fdata-sections")
    set(CMAKE_EXE_LINKER_FLAGS_RELEASE "${CMAKE_EXE_LINKER_FLAGS_RELEASE} -Wl,--gc-sections")
    set(CMAKE_CXX_FLAGS_DEBUG "-O0 -g")
    set(CMAKE_CXX_VISIBILITY_PRESET hidden)
    set(CMAKE_C_VISIBILITY_PRESET hidden)
    set(CMAKE_VISIBILITY_INLINES_HIDDEN ON)
    set(CMAKE_SHARED_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS} -Wl,--exclude-libs,ALL")
    set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -Wl,--exclude-libs,ALL")
endif()

if(NOT UNIX)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fvisibility=hidden")
endif()

# Release build optimizations
if(CMAKE_BUILD_TYPE STREQUAL "Release")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -s")
    set(CMAKE_WINDOWS_EXPORT_ALL_SYMBOLS OFF)

    set(CMAKE_BUILD_TYPE_INIT "Release")
    set(CMAKE_C_FLAGS_RELEASE "-DNDEBUG -Oz -ffunction-sections -fdata-sections")
    set(CMAKE_CXX_FLAGS_RELEASE "-DNDEBUG -Oz -ffunction-sections -fdata-sections")

    # Disable debug info completely
    set(CMAKE_C_FLAGS_DEBUG "")
    set(CMAKE_CXX_FLAGS_DEBUG "")
endif()

# =====================================
# Compile platform boot loader shims
# =====================================

if(WIN32)
    add_subdirectory(src/boot/win32)
elseif(UNIX AND NOT APPLE AND NOT NIX_BUILD)
    add_subdirectory(src/boot/linux)
elseif(UNIX AND APPLE)
    add_subdirectory(src/boot/macos)
endif()

# =====================================
# Linux Distribution Detection
# =====================================

find_program(LSB_RELEASE_EXEC lsb_release)
if(LSB_RELEASE_EXEC)
    execute_process(
        COMMAND ${LSB_RELEASE_EXEC} -is
        OUTPUT_VARIABLE LSB_RELEASE_ID_SHORT
        OUTPUT_STRIP_TRAILING_WHITESPACE
    )
    message(STATUS "LSB Release ID: ${LSB_RELEASE_ID_SHORT}")
endif()

# =====================================
# Python Configuration
# =====================================

# Helper function to detect AUR helper on Arch systems
function(detect_aur_helper result_var)
    set(AUR_HELPERS yay paru aurman pikaur pamac trizen pacaur aura)

    find_program(PACMAN_EXECUTABLE pacman)
    if(NOT PACMAN_EXECUTABLE)
        message(STATUS "Not running on an Arch-based system (pacman not found)")
        set(${result_var} "Couldn't find AUR helper. Please update Millennium manually." PARENT_SCOPE)
        return()
    endif()

    message(STATUS "Arch-based system detected")

    # Check for available AUR helpers
    foreach(helper ${AUR_HELPERS})
        find_program(${helper}_EXECUTABLE ${helper})
        if(${helper}_EXECUTABLE)
            if(helper STREQUAL "pamac")
                set(${result_var} "pamac upgrade millennium" PARENT_SCOPE)
            elseif(helper STREQUAL "aura")
                set(${result_var} "aura -Ayu millennium" PARENT_SCOPE)
            else()
                set(${result_var} "${helper} -Syu millennium" PARENT_SCOPE)
            endif()
            message(STATUS "AUR helper found: ${helper}")
            return()
        endif()
    endforeach()

    message(STATUS "No AUR helper found. Please update Millennium manually.")
    set(${result_var} "Couldn't find AUR helper. Please update Millennium manually." PARENT_SCOPE)
endfunction()

if(WIN32)
    include_directories(${CMAKE_SOURCE_DIR}/vendor/python/win32)

elseif(APPLE)
    set(python_path "$ENV{HOME}/.pyenv/versions/3.11.8")
    include_directories("${python_path}/include/python3.11")
    set(MILLENNIUM__PYTHON_ENV "${python_path}")
    set(LIBPYTHON_RUNTIME_PATH "${python_path}/lib/libpython3.11.dylib")
    message(STATUS "[Millennium] Using python library ${LIBPYTHON_RUNTIME_PATH}")
else() # Linux
    if(NIX_BUILD)
        add_compile_definitions(_NIX_OS=ON __NIX_SELF_PATH="$ENV{out}" __NIX_SHIMS_PATH="$ENV{shims}" __NIX_ASSETS_PATH="$ENV{assets}")
    endif()

    # Try to find Python 3.11 exactly
    find_package(Python 3.11 EXACT COMPONENTS Development)

    set(PYTHON_TEST_RESULT FALSE)
    if(Python_FOUND)
        file(WRITE "${CMAKE_BINARY_DIR}/millennium-FindPython_test.cc" "#include <Python.h>\nint main() { Py_Initialize(); Py_Finalize(); return 0; }")

        try_compile(PYTHON_TEST_RESULT
            "${CMAKE_BINARY_DIR}"
            SOURCES "${CMAKE_BINARY_DIR}/millennium-FindPython_test.cc"
            LINK_LIBRARIES Python::Module
        )

        if(PYTHON_TEST_RESULT)
            message(STATUS "Found suitable Python version ${Python_VERSION}")
            set(LIBPYTHON_RUNTIME_PATH ${Python_LIBRARIES})
            if(NOT Python_ROOT_DIR)
                cmake_path(GET Python_LIBRARY_DIRS PARENT_PATH Python_ROOT_DIR)
            endif()
            set(MILLENNIUM__PYTHON_ENV ${Python_ROOT_DIR})
        else()
            message(STATUS "Python ABI mismatch, rolling back to default")
        endif()
    else()
        message(STATUS "No Python package found, rolling back to default")
    endif()

    # Use default Python if system Python failed
    if(NOT PYTHON_TEST_RESULT)
        set(default_python_path "/opt/python-i686-3.11.8")
        set(MILLENNIUM__PYTHON_ENV "${default_python_path}")
        set(LIBPYTHON_RUNTIME_PATH "${default_python_path}/lib/libpython-3.11.8.so")

        if(DISTRO_ARCH OR LSB_RELEASE_ID_SHORT STREQUAL "Arch")
            include_directories("${default_python_path}/include/python3.11/")
            detect_aur_helper(MILLENNIUM__UPDATE_SCRIPT_PROMPT)
        else()
            include_directories("${CMAKE_SOURCE_DIR}/vendor/python/posix")
            set(MILLENNIUM__UPDATE_SCRIPT_PROMPT "curl -fsSL 'https://raw.githubusercontent.com/SteamClientHomebrew/Millennium/refs/heads/main/scripts/install.sh' | sh")
        endif()
    endif()
    message(STATUS "[Millennium] Using python library ${LIBPYTHON_RUNTIME_PATH}")
endif()

# =====================================
# Windows Steam Path Detection
# =====================================

# Only build to Steam path when not building releases on GitHub Actions
if(WIN32 AND NOT GITHUB_ACTION_BUILD)
    execute_process(COMMAND reg query "HKCU\\Software\\Valve\\Steam" /v "SteamPath" RESULT_VARIABLE result OUTPUT_VARIABLE steam_path ERROR_VARIABLE reg_error)

    if(result EQUAL 0)
        string(REGEX MATCH "[a-zA-Z]:/[^ ]+([ ]+[^ ]+)*" extracted_path "${steam_path}")
        string(REPLACE "\n" "" extracted_path "${extracted_path}")

        message(STATUS "[Millennium] Target build path: ${extracted_path}")

        set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${extracted_path})
        set(LIBRARY_OUTPUT_DIRECTORY ${extracted_path})
    else()
        message(WARNING "[Millennium] Failed to read Steam installation path from HKCU\\Software\\Valve\\Steam.")
    endif()
endif()

# =====================================
# Include Directories
# =====================================

include_directories(
    ${CMAKE_SOURCE_DIR}/src
    ${CMAKE_SOURCE_DIR}/src/include
    ${CMAKE_SOURCE_DIR}/vendor/fmt/include
    ${CMAKE_SOURCE_DIR}/vendor/asio/asio/include
    ${CMAKE_SOURCE_DIR}/vendor/nlohmann/include
    ${CMAKE_SOURCE_DIR}/vendor/websocketpp
    ${CMAKE_SOURCE_DIR}/vendor/crow/include
    ${CMAKE_SOURCE_DIR}/vendor/ini/src
    ${CMAKE_SOURCE_DIR}/vendor/incbin
)

# =====================================
# Source Files
# =====================================

set(SOURCE_FILES
    src/runtime/libjs_interop.cc
    src/runtime/liblj_api.cc
    src/runtime/liblj_http.cc
    src/runtime/liblj_interop.cc
    src/runtime/liblj_logger.cc
    src/runtime/liblj_utils.cc
    src/runtime/liblj_fs.cc
    src/runtime/liblj_regex.cc
    src/runtime/liblj_datetime.cc
    src/runtime/libpy_api.cc
    src/runtime/libpy_gil.cc
    src/runtime/libpy_interop.cc
    src/runtime/libpy_logger.cc
    src/runtime/libpy_stdout_fwd.cc
    src/core/auth.cc
    src/core/backend_init.cc
    src/core/backend_mgr.cc
    src/core/core_ipc.cc
    src/core/http_hook.cc
    src/core/init.cc
    src/core/lifecycle.cc
    src/core/millennium_api.cc
    src/core/millennium_updater.cc
    src/util/semver.cc
    src/util/zip.cc
    src/backend/css_parser.cc
    src/backend/default_cfg.cc
    src/backend/entry_point.cc
    src/backend/ipc_handler.cc
    src/backend/library_updater.cc
    src/backend/plugin_mgr.cc
    src/backend/scan.cc
    src/backend/sys_accent_col.cc
    src/backend/theme_cfg.cc
    src/backend/theme_mgr.cc
    src/backend/webkit.cc
    src/hooks/steam_hooks.cc
    src/sys/cfg.cc
    src/sys/env.cc
    src/sys/log.cc
    src/sys/sysfs.cc
    src/main.cc
)

if(WIN32)
    list(APPEND SOURCE_FILES "src/plat/init_win32.cc")
elseif(UNIX)
    list(APPEND SOURCE_FILES "src/plat/init_posix.cc")
    list(APPEND SOURCE_FILES "src/plat/init_posix_deprecated.cc")
    # list(APPEND SOURCE_FILES "src/hooks/third_party/subhook.c")
    list(APPEND SOURCE_FILES "src/hooks/third_party/subhook_x86.c")
    list(APPEND SOURCE_FILES "src/hooks/third_party/subhook_linux.c")
endif()

# =====================================
# Auto Generate FTP includes. This bundles the FTP files into memory directly.
# =====================================

# set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/assets/.millennium/Dist/index.js")
# set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/sdk/typescript-packages/loader/build/")

function(filename_to_identifier filename output_var)
    get_filename_component(name_without_ext "${filename}" NAME_WE)
    string(TOUPPER "${name_without_ext}" upper_name)
    string(REGEX REPLACE "[-.]" "_" clean_name "${upper_name}")
    if(filename MATCHES "\\.map$")
        set(${output_var} "MILLENNIUM_${clean_name}_MAP_" PARENT_SCOPE)
    else()
        set(${output_var} "MILLENNIUM_${clean_name}_BIN_" PARENT_SCOPE)
    endif()
endfunction()

file(GLOB CHUNK_FILES "${CMAKE_SOURCE_DIR}/sdk/typescript-packages/loader/build/chunks/*")
set(HEADER_CONTENT "/**\n * Auto-generated by Millennium during CMake configuration - do not edit manually :)\n */\n")

if(CMAKE_BUILD_TYPE STREQUAL "Release")
    set(HEADER_CONTENT "${HEADER_CONTENT}#define INCBIN_PREFIX\n")
    set(HEADER_CONTENT "${HEADER_CONTENT}#include \"incbin.h\"\n")
    set(HEADER_CONTENT "${HEADER_CONTENT}/** Millennium frontend library -- responsible for displaying Millennium UI, and loading themes */\n")
    set(HEADER_CONTENT "${HEADER_CONTENT}INCTXT(MILLENNIUM_FRONTEND_BIN_, \"../../../build/frontend.bin\");\n\n")
    set(HEADER_CONTENT "${HEADER_CONTENT}/** Millennium API library */\n")
    set(HEADER_CONTENT "${HEADER_CONTENT}INCTXT(MILLENNIUM_API_BIN_, \"../../../sdk/typescript-packages/loader/build/millennium.js\");\n")
    set(HEADER_CONTENT "${HEADER_CONTENT}/** Millennium API chunk files */\n")
endif()

set(MAP_ENTRIES "")
foreach(chunk_file ${CHUNK_FILES})
    get_filename_component(filename "${chunk_file}" NAME)
    if(filename MATCHES "\\.map$")
        continue()
    endif()
    file(RELATIVE_PATH rel_path "${CMAKE_SOURCE_DIR}" "${chunk_file}")
    filename_to_identifier("${filename}" var_name)

    if(CMAKE_BUILD_TYPE STREQUAL "Release")
        set(HEADER_CONTENT "${HEADER_CONTENT}INCTXT(${var_name}, \"../../../${rel_path}\");\n")
        set(MAP_ENTRIES "${MAP_ENTRIES}\t{ fmt::format(\"https://millennium.ftp/{}/chunks/${filename}\", GetScrambledApiPathToken()), []() { return ${var_name}Data; } },\n")
    else()
        set(MAP_ENTRIES "${MAP_ENTRIES}\t{ fmt::format(\"https://millennium.ftp/{}/chunks/${filename}\", GetScrambledApiPathToken()), []() { return SystemIO::ReadFileSync(MILLENNIUM_ROOT \"/${rel_path}\"); } },\n")
    endif()
endforeach()

set(HEADER_CONTENT "${HEADER_CONTENT}\n// clang-format off\n")
set(HEADER_CONTENT "${HEADER_CONTENT}std::unordered_map<std::string, std::function<const std::string()>> INTERNAL_FTP_CALL_DATA = {\n")

if(CMAKE_BUILD_TYPE STREQUAL "Release")
    set(HEADER_CONTENT "${HEADER_CONTENT}\t{ fmt::format(\"https://millennium.ftp/{}/millennium-frontend.js\", GetScrambledApiPathToken()), []() { return MILLENNIUM_FRONTEND_BIN_Data; } },\n")
    set(HEADER_CONTENT "${HEADER_CONTENT}\t{ fmt::format(\"https://millennium.ftp/{}/millennium.js\", GetScrambledApiPathToken()), []() { return MILLENNIUM_API_BIN_Data; } },\n")
else()
    set(HEADER_CONTENT "${HEADER_CONTENT}\t{ fmt::format(\"https://millennium.ftp/{}/millennium-frontend.js\", GetScrambledApiPathToken()), []() { return SystemIO::ReadFileSync(MILLENNIUM_ROOT \"/build/frontend.bin\"); } },\n")
    set(HEADER_CONTENT "${HEADER_CONTENT}\t{ fmt::format(\"https://millennium.ftp/{}/millennium.js\", GetScrambledApiPathToken()), []() { return SystemIO::ReadFileSync(MILLENNIUM_ROOT \"/sdk/typescript-packages/loader/build/millennium.js\"); } },\n")
endif()

set(HEADER_CONTENT "${HEADER_CONTENT}${MAP_ENTRIES}")
string(REGEX REPLACE ",\n$" "\n" HEADER_CONTENT "${HEADER_CONTENT}")
set(HEADER_CONTENT "${HEADER_CONTENT}};\n")
set(HEADER_CONTENT "${HEADER_CONTENT}// clang-format on\n")

file(MAKE_DIRECTORY "${CMAKE_SOURCE_DIR}/src/include/millennium")
file(WRITE "${CMAKE_SOURCE_DIR}/src/include/millennium/virtfs.h" "${HEADER_CONTENT}")

add_custom_command(
    OUTPUT "${CMAKE_SOURCE_DIR}/src/include/millennium/virtfs.h"
    COMMAND ${CMAKE_COMMAND} -P "${CMAKE_CURRENT_LIST_FILE}"
    DEPENDS ${CHUNK_FILES}
    COMMENT "Generating asset includes"
)

# =====================================
# Target Configuration
# =====================================

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

target_include_directories(Millennium SYSTEM PRIVATE
    ${websocketpp_SOURCE_DIR}
)

target_compile_options(Millennium PRIVATE
    -Wall
    -Wextra
    -Wpedantic
    -Werror
)

if(NOT APPLE)
    set_target_properties(Millennium PROPERTIES COMPILE_FLAGS "-m32" LINK_FLAGS "-m32")
    target_compile_options(Millennium PRIVATE -m32)
endif()

if(WIN32)
    set_target_properties(Millennium PROPERTIES OUTPUT_NAME "millennium")
    set_target_properties(Millennium PROPERTIES PREFIX "")
    set_target_properties(Millennium PROPERTIES NO_EXPORT TRUE)
elseif(UNIX AND NOT APPLE)
    set_target_properties(Millennium PROPERTIES OUTPUT_NAME "millennium")
    set_target_properties(Millennium PROPERTIES PREFIX "lib")
    set_target_properties(Millennium PROPERTIES SUFFIX "_x86.so")
endif()

# =====================================
# Add version resource for Windows builds
# =====================================

find_program(WINDRES windres)
if(WINDRES)
    add_custom_command(
        OUTPUT ${CMAKE_BINARY_DIR}/version.o
        COMMAND ${WINDRES} -i ${CMAKE_SOURCE_DIR}/scripts/version.rc -o ${CMAKE_BINARY_DIR}/version.o
        DEPENDS ${CMAKE_SOURCE_DIR}/scripts/version.rc
    )

    add_custom_target(resource DEPENDS ${CMAKE_BINARY_DIR}/version.o)
    add_dependencies(Millennium resource)
    target_link_libraries(Millennium ${CMAKE_BINARY_DIR}/version.o)
endif()

if(NIX_BUILD)
    find_package(OpenSSL REQUIRED)
    target_link_libraries(Millennium OpenSSL::SSL)
endif()


target_link_libraries(Millennium
    luajit::lib
    libcurl
    minizip
    cjson
    # libgit2 doesn't export a CMake target for some reason, so we link the static lib directly
    "${libgit2_BINARY_DIR}/libgit2.a"
    nlohmann_json
    fmt::fmt
    mINI
)

target_link_options(Millennium PRIVATE -Wl,--no-undefined)

if(WIN32)
    if(GITHUB_ACTION_BUILD)
        set(PYTHON_LIB "${CMAKE_SOURCE_DIR}/build/python/python311.lib")
    else()
        set(PYTHON_LIB ${CMAKE_SOURCE_DIR}/vendor/python/python311.lib ${CMAKE_SOURCE_DIR}/vendor/python/python311_d.lib)

        add_custom_command(
            OUTPUT "${extracted_path}/python311.dll"
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
                "${CMAKE_SOURCE_DIR}/vendor/python/python311.dll"
                "${extracted_path}/python311.dll"
            COMMENT "Adding Millennium python runtime to Steam directory"
        )

        add_custom_target(copy_python311_dll ALL
            DEPENDS "${extracted_path}/python311.dll"
            COMMENT "Ensuring python311.dll is present in Steam directory"
        )

        add_dependencies(Millennium copy_python311_dll)
    endif()

    target_link_libraries(Millennium
        minhook
        ${PYTHON_LIB}
        ws2_32
        crypt32
        winhttp
        rpcrt4
        secur32
        bcrypt
        iphlpapi
        dbghelp
        schannel
        normaliz
        winmm
        # curl dependencies
        -Wl,--start-group nghttp2 iconv unistring psl idn2 ssl crypto z ssh2 -Wl,--end-group
    )

elseif(UNIX)
    if(APPLE)
        target_link_libraries(Millennium "$ENV{HOME}/.pyenv/versions/3.11.8/lib/libpython3.11.dylib")
    else()
        if(PYTHON_TEST_RESULT)
            target_link_libraries(Millennium Python::Module)
        else()
            target_link_libraries(Millennium "/opt/python-i686-3.11.8/lib/libpython-3.11.8.so")
        endif()
    endif()
endif()
