message(STATUS "[Millennium] Configuring & verifying dependencies...")

cmake_minimum_required(VERSION 3.21)
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>" CACHE STRING "" FORCE)

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
    
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -m32")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -m32")
    set(CMAKE_ASM_FLAGS "${CMAKE_ASM_FLAGS} -m32")

    set(LUAJIT_BUILD_FLAGS "-m32" CACHE STRING "LuaJIT build flags" FORCE)
    set(LUAJIT_TARGET_FLAGS "-m32" CACHE STRING "LuaJIT target flags" FORCE)
endif()

# =====================================
# Dep Build Configuration
# =====================================

# Ensure all libraries are position independent for static linking
set(CMAKE_POSITION_INDEPENDENT_CODE ON CACHE BOOL "" FORCE)

# General settings
set(BUILD_TESTS                 OFF CACHE BOOL     "Disable building tests")
set(BUILD_EXAMPLES              OFF CACHE BOOL     "Disable building examples")
set(BUILD_SHARED_LIBS           OFF CACHE BOOL     "Build static libraries by default" FORCE)
set(BUILD_STATIC_LIBS           ON  CACHE BOOL     "Build static libraries by default" FORCE)
set(CMAKE_CXX_STANDARD          17  CACHE STRING   "C++ standard to use" FORCE)
set(CMAKE_C_STANDARD            11  CACHE STRING   "C standard to use" FORCE)
set(CMAKE_CXX_STANDARD_REQUIRED ON  CACHE BOOL     "Enforce C++ standard" FORCE)
set(CMAKE_C_STANDARD_REQUIRED   ON  CACHE BOOL     "Enforce C standard" FORCE)
set(USE_INTERNAL_FPCONV         ON  CACHE BOOL     "Use internal strtod() / g_fmt() code for performance")
set(NGHTTP2_STATICLIB           ON  CACHE BOOL     "Use static nghttp2 library")
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
set(CURL_USE_LIBPSL 		    OFF CACHE BOOL     "Disable libpsl")
set(FMT_HEADER_ONLY             ON  CACHE BOOL     "" FORCE)
set(FMT_DOC                     OFF CACHE BOOL     "" FORCE)
set(FMT_TEST                    OFF CACHE BOOL     "" FORCE)
set(FMT_INSTALL                 ON  CACHE BOOL     "" FORCE)
set(MULTIPLE_THREADS 		    OFF CACHE BOOL     "Disable multithreading support in lua cjson")

set(BUILD_CLI  			        OFF CACHE BOOL     "Disable building CLI in libgit2")

set(MZ_BZIP2                    OFF CACHE BOOL     "Disable bzip2 support in minizip-ng")
set(MZ_FETCH_LIBS               OFF CACHE BOOL     "Disable fetching third-party libs in minizip-ng" FORCE)
set(MZ_ZLIB                     ON  CACHE BOOL     "Enable zlib support in minizip-ng" FORCE)

set(USE_BUNDLED_ZLIB 		    OFF CACHE BOOL     "Use bundled zlib in libgit2" FORCE)
set(USE_COMPRESSION 		    OFF CACHE STRING   "zlib" FORCE)

set(ZLIB_COMPAT                 ON CACHE BOOL      "Enable zlib compatibility mode in zlib-ng" FORCE)
set(ZLIB_ALIASES                ON CACHE BOOL      "Enable zlib alias symbols in zlib-ng" FORCE)
set(ZLIB_ENABLE_TESTS           OFF CACHE BOOL     "Disable building tests in zlib-ng" FORCE)
set(ZLIBNG_ENABLE_TESTS 	    OFF CACHE BOOL     "Disable building tests in zlib-ng" FORCE)

set(RTLIBCFG 			"static" CACHE STRING      "Disable rtlibcfg in curl" FORCE)

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
    CURL_STATICLIB
)

# Patch the minimum cmake version in a "submodule".
# Mingw decided they would force the new version of cmake which completely breaks older deps
# Only fix is a shim or fixing it up stream :shrug:

function(millennium_process_package input_path)
    if(EXISTS "${input_path}/CMakeLists.txt")
        file(READ "${input_path}/CMakeLists.txt" CMAKELISTS_CONTENTS)
        string(REGEX REPLACE "cmake_minimum_required[ \t]*\\([ \t]*VERSION[^)]*\\)" "cmake_minimum_required(VERSION 3.21)" CMAKELISTS_CONTENTS "${CMAKELISTS_CONTENTS}")
        file(WRITE "${input_path}/CMakeLists.txt" "${CMAKELISTS_CONTENTS}")
    else()
        message(WARNING "Could not find ${input_path} to patch.")
    endif()
endfunction()

# ====================================
# Fetch and configure dependencies
# ====================================
message(STATUS "[Millennium] Fetching dependencies...")

FetchContent_Declare(zlib          GIT_REPOSITORY https://github.com/zlib-ng/zlib-ng               GIT_TAG 2.2.5             SOURCE_SUBDIR fakedir GIT_SHALLOW TRUE GIT_PROGRESS TRUE)
FetchContent_Declare(luajit        GIT_REPOSITORY https://github.com/SteamClientHomebrew/LuaJIT    GIT_TAG v2.1              SOURCE_SUBDIR fakedir GIT_SHALLOW TRUE GIT_PROGRESS TRUE)
FetchContent_Declare(lua_cjson     GIT_REPOSITORY https://github.com/SteamClientHomebrew/LuaJSON   GIT_TAG master            SOURCE_SUBDIR fakedir GIT_SHALLOW TRUE GIT_PROGRESS TRUE)
FetchContent_Declare(minhook       GIT_REPOSITORY https://github.com/TsudaKageyu/minhook           GIT_TAG v1.3.4            SOURCE_SUBDIR fakedir GIT_SHALLOW TRUE GIT_PROGRESS TRUE)
FetchContent_Declare(mini          GIT_REPOSITORY https://github.com/metayeti/mINI                 GIT_TAG 0.9.18            SOURCE_SUBDIR fakedir GIT_SHALLOW TRUE GIT_PROGRESS TRUE)
FetchContent_Declare(websocketpp   GIT_REPOSITORY https://github.com/zaphoyd/websocketpp           GIT_TAG 0.8.2             SOURCE_SUBDIR fakedir GIT_SHALLOW TRUE GIT_PROGRESS TRUE)
FetchContent_Declare(fmt           GIT_REPOSITORY https://github.com/fmtlib/fmt                    GIT_TAG 12.0.0            SOURCE_SUBDIR fakedir GIT_SHALLOW TRUE GIT_PROGRESS TRUE)
FetchContent_Declare(nlohmann_json GIT_REPOSITORY https://github.com/nlohmann/json                 GIT_TAG v3.12.0           SOURCE_SUBDIR fakedir GIT_SHALLOW TRUE GIT_PROGRESS TRUE)
FetchContent_Declare(libgit2       GIT_REPOSITORY https://github.com/libgit2/libgit2               GIT_TAG v1.9.1            SOURCE_SUBDIR fakedir GIT_SHALLOW TRUE GIT_PROGRESS TRUE)
FetchContent_Declare(minizip_ng    GIT_REPOSITORY https://github.com/zlib-ng/minizip-ng            GIT_TAG 4.0.10            SOURCE_SUBDIR fakedir GIT_SHALLOW TRUE GIT_PROGRESS TRUE)
FetchContent_Declare(curl          GIT_REPOSITORY https://github.com/curl/curl                     GIT_TAG curl-8_13_0       SOURCE_SUBDIR fakedir GIT_SHALLOW TRUE GIT_PROGRESS TRUE)
FetchContent_Declare(incbin        GIT_REPOSITORY https://github.com/graphitemaster/incbin         GIT_TAG main              SOURCE_SUBDIR fakedir GIT_SHALLOW TRUE GIT_PROGRESS TRUE)
FetchContent_Declare(asio          GIT_REPOSITORY https://github.com/chriskohlhoff/asio            GIT_TAG asio-1-30-0       SOURCE_SUBDIR fakedir GIT_SHALLOW TRUE GIT_PROGRESS TRUE)

# Download all deps without configuring them
set(DEPENDENCIES zlib luajit incbin websocketpp fmt nlohmann_json minizip_ng libgit2 mINI lua_cjson curl asio)

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
include_directories("${CMAKE_SOURCE_DIR}/src/emu/include")

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
    millennium_process_package("${minhook_SOURCE_DIR}/")
endif()

# Let cmake finally configure after we've processed them
add_subdirectory("${zlib_SOURCE_DIR}"          "${zlib_BINARY_DIR}"         )

set(ZLIB_ROOT "${zlib_BINARY_DIR}" CACHE PATH "" FORCE)
set(ZLIB_FOUND TRUE CACHE INTERNAL "")
set(ZLIB_LIBRARY "${zlib_BINARY_DIR}/Release/zlibstatic.lib" CACHE INTERNAL "")
set(ZLIB_LIBRARIES "${zlib_BINARY_DIR}/Release/zlibstatic.lib" CACHE INTERNAL "")
set(ZLIB_INCLUDE_DIRS "${zlib_SOURCE_DIR};${zlib_BINARY_DIR}" CACHE INTERNAL "")
set(ZLIB_INCLUDE_DIR "${zlib_SOURCE_DIR};${zlib_BINARY_DIR}" CACHE INTERNAL "")

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
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/version")
configure_file(${CMAKE_SOURCE_DIR}/src/include/millennium/version.h.in ${CMAKE_BINARY_DIR}/version.h)
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

# MinGW optimizations
if(MINGW)
    add_compile_options($<$<COMPILE_LANGUAGE:CXX>:-fvisibility-inlines-hidden>)

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

# Release build optimizations
if(UNIX AND CMAKE_BUILD_TYPE STREQUAL "Release")
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
    ${CMAKE_SOURCE_DIR}/vendor/subhook
    ${CMAKE_SOURCE_DIR}/vendor/plthook
    ${CMAKE_SOURCE_DIR}/src/emu/include
)

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
    src/runtime/liblj_vfs_hook_api.cc
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
    src/emu/src/smem.cc
    src/sys/cfg.cc
    src/sys/env.cc
    src/sys/log.cc
    src/sys/sysfs.cc
    src/main.cc
)

# include(ExternalProject)

# ExternalProject_Add(mwh_64bit
#     SOURCE_DIR        ${CMAKE_CURRENT_SOURCE_DIR}/src/emu
#     BINARY_DIR        ${CMAKE_CURRENT_BINARY_DIR}/emu-build

#     # Optional: still set generator string for readability (not strictly needed with explicit configure)
#     CMAKE_GENERATOR   "Visual Studio 17 2022"

#     # Explicit configure command — guarantees -A x64 is passed
#     CONFIGURE_COMMAND
#       ${CMAKE_COMMAND}
#         -S ${CMAKE_CURRENT_SOURCE_DIR}/src/emu
#         -B ${CMAKE_CURRENT_BINARY_DIR}/emu-build
#         -G "Visual Studio 17 2022"
#         -A x64
#         -DCMAKE_BUILD_TYPE=${CMAKE_BUILD_TYPE}
#         -Dpcre2_SOURCE_DIR=${pcre2_SOURCE_DIR}
#         -Dpcre2_BINARY_DIR=${CMAKE_CURRENT_BINARY_DIR}/emu-build/pcre2
#         -Dhyperscan_SOURCE_DIR=${hyperscan_SOURCE_DIR}
#         -Dhyperscan_BINARY_DIR=${CMAKE_CURRENT_BINARY_DIR}/emu-build/hyperscan
#         -DBUILD_UNIT_TESTS=OFF
#         -DBUILD_EXAMPLES=OFF
#         -DBUILD_BENCHMARKS=OFF

#     # Build using the subproject build dir and chosen config
#     BUILD_COMMAND
#       ${CMAKE_COMMAND} --build ${CMAKE_CURRENT_BINARY_DIR}/emu-build --config ${CMAKE_BUILD_TYPE} -- /m

#     # No install step
#     INSTALL_COMMAND ""

#     BUILD_ALWAYS 1
# )

if(WIN32)
    list(APPEND SOURCE_FILES "src/plat/init_win32.cc")
    if(CMAKE_BUILD_TYPE STREQUAL "Release")
        list(APPEND SOURCE_FILES "scripts/resources.rc")
    endif()
elseif(UNIX)
    list(APPEND SOURCE_FILES "src/plat/init_posix.cc")
    list(APPEND SOURCE_FILES "src/plat/init_posix_deprecated.cc")

    list(APPEND SOURCE_FILES "vendor/subhook/subhook_x86.c")
    list(APPEND SOURCE_FILES "vendor/subhook/subhook_linux.c")
    list(APPEND SOURCE_FILES "vendor/plthook/plthook_elf.c")
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
set(HEADER_CONTENT "${HEADER_CONTENT}#pragma once\n\n")
set(RC_CONTENT "// Auto-generated resource file\n#include <windows.h>\n\n")

if(CMAKE_BUILD_TYPE STREQUAL "Release")
    # Platform-specific includes
    set(HEADER_CONTENT "${HEADER_CONTENT}#ifdef _WIN32\n")
    set(HEADER_CONTENT "${HEADER_CONTENT}#include <windows.h>\n")
    set(HEADER_CONTENT "${HEADER_CONTENT}#include <string>\n\n")
    set(HEADER_CONTENT "${HEADER_CONTENT}inline std::string LoadResourceData(const char* name) {\n")
    set(HEADER_CONTENT "${HEADER_CONTENT}    HRSRC hRes = FindResourceA(NULL, name, RT_RCDATA);\n")
    set(HEADER_CONTENT "${HEADER_CONTENT}    if (!hRes) return \"\";\n")
    set(HEADER_CONTENT "${HEADER_CONTENT}    HGLOBAL hData = LoadResource(NULL, hRes);\n")
    set(HEADER_CONTENT "${HEADER_CONTENT}    if (!hData) return \"\";\n")
    set(HEADER_CONTENT "${HEADER_CONTENT}    DWORD size = SizeofResource(NULL, hRes);\n")
    set(HEADER_CONTENT "${HEADER_CONTENT}    const char* data = static_cast<const char*>(LockResource(hData));\n")
    set(HEADER_CONTENT "${HEADER_CONTENT}    return std::string(data, size);\n")
    set(HEADER_CONTENT "${HEADER_CONTENT}}\n")
    set(HEADER_CONTENT "${HEADER_CONTENT}#else\n")
    set(HEADER_CONTENT "${HEADER_CONTENT}#define INCBIN_PREFIX\n")
    set(HEADER_CONTENT "${HEADER_CONTENT}#include \"incbin.h\"\n")
    set(HEADER_CONTENT "${HEADER_CONTENT}#endif\n\n")
    
    # Frontend binary
    set(HEADER_CONTENT "${HEADER_CONTENT}/** Millennium frontend library */\n")
    set(HEADER_CONTENT "${HEADER_CONTENT}#ifdef _WIN32\n")
    set(HEADER_CONTENT "${HEADER_CONTENT}#define MILLENNIUM_FRONTEND_BIN_Data LoadResourceData(\"MILLENNIUM_FRONTEND_BIN\")\n")
    set(HEADER_CONTENT "${HEADER_CONTENT}#else\n")
    set(HEADER_CONTENT "${HEADER_CONTENT}INCTXT(MILLENNIUM_FRONTEND_BIN_, \"../../../build/frontend.bin\");\n")
    set(HEADER_CONTENT "${HEADER_CONTENT}#endif\n\n")
    
    # API binary
    set(HEADER_CONTENT "${HEADER_CONTENT}/** Millennium API library */\n")
    set(HEADER_CONTENT "${HEADER_CONTENT}#ifdef _WIN32\n")
    set(HEADER_CONTENT "${HEADER_CONTENT}#define MILLENNIUM_API_BIN_Data LoadResourceData(\"MILLENNIUM_API_BIN\")\n")
    set(HEADER_CONTENT "${HEADER_CONTENT}#else\n")
    set(HEADER_CONTENT "${HEADER_CONTENT}INCTXT(MILLENNIUM_API_BIN_, \"../../../sdk/typescript-packages/loader/build/millennium.js\");\n")
    set(HEADER_CONTENT "${HEADER_CONTENT}#endif\n\n")
    
    set(HEADER_CONTENT "${HEADER_CONTENT}/** Millennium API chunk files */\n")
    
    # Add resources to RC file
    set(RC_CONTENT "${RC_CONTENT}MILLENNIUM_FRONTEND_BIN RCDATA \"${CMAKE_SOURCE_DIR}/build/frontend.bin\"\n")
    set(RC_CONTENT "${RC_CONTENT}MILLENNIUM_API_BIN RCDATA \"${CMAKE_SOURCE_DIR}/sdk/typescript-packages/loader/build/millennium.js\"\n\n")
endif()

set(MAP_ENTRIES "")
set(CHUNK_DEFS "")

foreach(chunk_file ${CHUNK_FILES})
    get_filename_component(filename "${chunk_file}" NAME)
    if(filename MATCHES "\\.map$")
        continue()
    endif()
    file(RELATIVE_PATH rel_path "${CMAKE_SOURCE_DIR}" "${chunk_file}")
    filename_to_identifier("${filename}" var_name)
    
    if(CMAKE_BUILD_TYPE STREQUAL "Release")
        # Chunk definitions
        set(CHUNK_DEFS "${CHUNK_DEFS}#ifdef _WIN32\n")
        set(CHUNK_DEFS "${CHUNK_DEFS}#define ${var_name}Data LoadResourceData(\"${var_name}\")\n")
        set(CHUNK_DEFS "${CHUNK_DEFS}#else\n")
        set(CHUNK_DEFS "${CHUNK_DEFS}INCTXT(${var_name}, \"../../../${rel_path}\");\n")
        set(CHUNK_DEFS "${CHUNK_DEFS}#endif\n\n")
        
        set(MAP_ENTRIES "${MAP_ENTRIES}\t{ fmt::format(\"https://millennium.ftp/{}/chunks/${filename}\", GetScrambledApiPathToken()), []() { return ${var_name}Data; } },\n")
        
        # Add to RC file
        set(RC_CONTENT "${RC_CONTENT}${var_name} RCDATA \"${chunk_file}\"\n")
    else()
        set(MAP_ENTRIES "${MAP_ENTRIES}\t{ fmt::format(\"https://millennium.ftp/{}/chunks/${filename}\", GetScrambledApiPathToken()), []() { return SystemIO::ReadFileSync(MILLENNIUM_ROOT \"/${rel_path}\"); } },\n")
    endif()
endforeach()

set(HEADER_CONTENT "${HEADER_CONTENT}${CHUNK_DEFS}\n")
set(HEADER_CONTENT "${HEADER_CONTENT}// clang-format off\n")
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

if(CMAKE_BUILD_TYPE STREQUAL "Release" AND WIN32)
    file(WRITE "${CMAKE_SOURCE_DIR}/scripts/resources.rc" "${RC_CONTENT}")
endif()

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
    target_compile_options(Millennium PRIVATE
        -Wall
        -Wextra
        -Wpedantic
        -Werror
    )

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

if(UNIX)
    set_target_properties(Millennium PROPERTIES COMPILE_FLAGS "-m32" LINK_FLAGS "-m32")
    target_compile_options(Millennium PRIVATE -m32)
endif()

if(WIN32)
    set_target_properties(Millennium PROPERTIES OUTPUT_NAME "millennium")
    set_target_properties(Millennium PROPERTIES PREFIX "")
    set_target_properties(Millennium PROPERTIES NO_EXPORT TRUE)

    if(extracted_path)
        set_target_properties(Millennium PROPERTIES
            RUNTIME_OUTPUT_DIRECTORY "${extracted_path}"
            LIBRARY_OUTPUT_DIRECTORY "${extracted_path}"
            ARCHIVE_OUTPUT_DIRECTORY "${extracted_path}"
        )
        # Override per-configuration output directories
        foreach(CONFIG ${CMAKE_CONFIGURATION_TYPES})
            string(TOUPPER ${CONFIG} CONFIG_UPPER)
            set_target_properties(Millennium PROPERTIES
                RUNTIME_OUTPUT_DIRECTORY_${CONFIG_UPPER} "${extracted_path}"
                LIBRARY_OUTPUT_DIRECTORY_${CONFIG_UPPER} "${extracted_path}"
                ARCHIVE_OUTPUT_DIRECTORY_${CONFIG_UPPER} "${extracted_path}"
            )
        endforeach()
    endif()
elseif(UNIX AND NOT APPLE)
    set_target_properties(Millennium PROPERTIES COMPILE_FLAGS "-m32" LINK_FLAGS "-m32")
    target_compile_options(Millennium PRIVATE -m32)
    
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

set_property(TARGET Millennium PROPERTY MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")

if(WIN32)
    if(GITHUB_ACTION_BUILD)
        set(PYTHON_LIB "${CMAKE_SOURCE_DIR}/build/python/python311.lib")
    else()
        set(PYTHON_LIB ${CMAKE_SOURCE_DIR}/vendor/python/python311.lib)

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
        luajit::lib
        libcurl
        minizip
        cjson
        libgit2package
        nlohmann_json
        fmt::fmt
        mINI
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
        # -Wl,--start-group iconv unistring psl idn2 ssl crypto z ssh2 -Wl,--end-group
    )

    target_link_options(Millennium PRIVATE
        /NODEFAULTLIB:LIBCMT
    )

elseif(UNIX)
    target_link_libraries(Millennium
        luajit::lib
        libcurl
        minizip
        cjson
        libgit2package
        nlohmann_json
        fmt::fmt
        mINI
    )

    target_link_options(Millennium PRIVATE -Wl,--no-undefined)

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

