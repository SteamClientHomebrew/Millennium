cmake_minimum_required(VERSION 3.10...3.21)
project(Millennium LANGUAGES C CXX)

# =====================================
# Version and Git Configuration
# =====================================

# Get version from file
file(STRINGS "${CMAKE_SOURCE_DIR}/version" VERSION_LINES LIMIT_COUNT 2)
list(GET VERSION_LINES 1 MILLENNIUM_VERSION)

# Add configure dependency on version file
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/version")

# Configure version header
configure_file(
    ${CMAKE_SOURCE_DIR}/src/include/millennium/version.h.in
    ${CMAKE_BINARY_DIR}/version.h
)

# Get git commit hash
execute_process(
    COMMAND git rev-parse HEAD
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
    OUTPUT_VARIABLE GIT_COMMIT_HASH
    OUTPUT_STRIP_TRAILING_WHITESPACE
)

message(STATUS "Millennium Version: ${MILLENNIUM_VERSION}")
message(STATUS "Git commit hash: ${GIT_COMMIT_HASH}")

# =====================================
# Build Configuration
# =====================================

set(BUILD_SHARED_LIBS OFF)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_C_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_C_STANDARD_REQUIRED ON)

# Global compile definitions
add_compile_definitions(
    GIT_COMMIT_HASH="${GIT_COMMIT_HASH}"
    MILLENNIUM_ROOT="${CMAKE_SOURCE_DIR}"
    MILLENNIUM_VERSION="${MILLENNIUM_VERSION}"
)

# Debug mode specific settings
if(CMAKE_BUILD_TYPE STREQUAL "Debug")
    set(MILLENNIUM_SDK_DEVELOPMENT_MODE_ASSETS "${CMAKE_SOURCE_DIR}/sdk/typescript-packages/loader/build")
    set(MILLENNIUM_FRONTEND_DEVELOPMENT_MODE_ASSETS "${CMAKE_SOURCE_DIR}/assets")
    
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
# Platform-Specific Compiler Flags
# =====================================

# 32-bit compilation (except Apple)
if(NOT APPLE)
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -m32")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -m32")
endif()

# Apple Debug with AddressSanitizer
if(APPLE AND CMAKE_BUILD_TYPE STREQUAL "Debug")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -g -fsanitize=address -fno-omit-frame-pointer")
endif()

# MinGW visibility settings
if(MINGW)
    set(CMAKE_CXX_VISIBILITY_PRESET hidden)
    set(CMAKE_C_VISIBILITY_PRESET hidden)
    set(CMAKE_VISIBILITY_INLINES_HIDDEN ON)
    set(CMAKE_SHARED_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS} -Wl,--exclude-libs,ALL")
    set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -Wl,--exclude-libs,ALL")
endif()

# Unix visibility settings (NOTE: Original logic is NOT UNIX)
if(NOT UNIX)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fvisibility=hidden")
endif()

# Release build optimizations
if(CMAKE_BUILD_TYPE STREQUAL "Release")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -s")
    set(CMAKE_WINDOWS_EXPORT_ALL_SYMBOLS OFF)
endif()

# =====================================
# Platform-Specific Subdirectories
# =====================================

if(UNIX AND NOT APPLE AND NOT NIX_BUILD)
    add_subdirectory(cli)
    add_subdirectory(unix-hooks)
elseif(UNIX AND APPLE)
    add_subdirectory(darwin)
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
    include_directories(${CMAKE_SOURCE_DIR}/vendor/python/win32 ${CMAKE_SOURCE_DIR}/vendor/lua/src)
    
elseif(APPLE)
    set(python_path "$ENV{HOME}/.pyenv/versions/3.11.8")
    include_directories("${python_path}/include/python3.11")
    set(MILLENNIUM__PYTHON_ENV "${python_path}")
    set(LIBPYTHON_RUNTIME_PATH "${python_path}/lib/libpython3.11.dylib")
    
else() # Linux
    if(NIX_BUILD)
        add_compile_definitions(
            _NIX_OS=ON
            __NIX_SELF_PATH="$ENV{out}"
            __NIX_SHIMS_PATH="$ENV{shims}"
            __NIX_ASSETS_PATH="$ENV{assets}"
        )
    endif()

    # Try to find Python 3.11 exactly
    find_package(Python 3.11 EXACT COMPONENTS Development)
    
    set(PYTHON_TEST_RESULT FALSE)
    if(Python_FOUND)
        try_compile(PYTHON_TEST_RESULT
            "${CMAKE_BINARY_DIR}"
            SOURCES "${CMAKE_CURRENT_LIST_DIR}/tests/FindPython_test.cc"
            LINK_LIBRARIES Python::Module)
        
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
endif()

message(STATUS "Set Python runtime library to ${LIBPYTHON_RUNTIME_PATH}")

# =====================================
# Windows Steam Path Detection
# =====================================

if(WIN32 AND NOT GITHUB_ACTION_BUILD)
    execute_process(
        COMMAND reg query "HKCU\\Software\\Valve\\Steam" /v "SteamPath"
        RESULT_VARIABLE result
        OUTPUT_VARIABLE steam_path
        ERROR_VARIABLE reg_error
    )

    if(result EQUAL 0)
        string(REGEX MATCH "[a-zA-Z]:/[^ ]+([ ]+[^ ]+)*" extracted_path "${steam_path}")
        string(REPLACE "\n" "" extracted_path "${extracted_path}")

        message(STATUS "Build Steam Path: ${extracted_path}")

        set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${extracted_path})
        set(LIBRARY_OUTPUT_DIRECTORY ${extracted_path})
    else()
        message(WARNING "Failed to read Steam installation path from HKCU\\Software\\Valve\\Steam.")
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

# Additional compile definitions
add_compile_definitions(
    "WINRT_NO_COROUTINES"
    "LJ_TARGET_X86"
    "LJ_HASJIT"
    "CURL_STATICLIB"
    "_WEBSOCKETPP_CPP11_THREAD_"
    "_WEBSOCKETPP_CPP11_TYPE_TRAITS_"
    "_WEBSOCKETPP_CPP11_RANDOM_DEVICE_"
    "ASIO_STANDALONE"
    "ASIO_HAS_STD_INVOKE_RESULT"
    "FMT_HEADER_ONLY"
    "_CRT_SECURE_NO_WARNINGS"
)

if(WIN32)
    add_subdirectory(src/boot/win32)
endif()

# =====================================
# Source Files
# =====================================

set(SOURCE_FILES
    src/backend/libjs_interop.cc
    src/backend/liblj_api.cc
    src/backend/liblj_http.cc
    src/backend/liblj_interop.cc
    src/backend/liblj_logger.cc
    src/backend/liblj_utils.cc
    src/backend/libpy_api.cc
    src/backend/libpy_gil.cc
    src/backend/libpy_interop.cc
    src/backend/libpy_logger.cc
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
    src/head/css_parser.cc
    src/head/default_cfg.cc
    src/head/entry_point.cc
    src/head/ipc_handler.cc
    src/head/library_updater.cc
    src/head/plugin_mgr.cc
    src/head/README.md
    src/head/scan.cc
    src/head/sys_accent_col.cc
    src/head/theme_cfg.cc
    src/head/theme_mgr.cc
    src/head/webkit.cc
    src/hooks/cefparam.cc
    src/sys/cfg.cc
    src/sys/env.cc
    src/sys/log.cc
    src/sys/sysfs.cc
    src/main.cc
	vendor/lua-cjson/lua_cjson.c
    vendor/lua-cjson/strbuf.c
    vendor/lua-cjson/fpconv.c
)

if(WIN32)
    list(APPEND SOURCE_FILES "src/plat/init_win32.cc")
elseif(UNIX)
    list(APPEND SOURCE_FILES "src/plat/init_posix.cc")
endif()

# =====================================
# Auto Generate FTP includes. This bundles the FTP files into memory directly.
# =====================================

set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/assets/.millennium/Dist/index.js")
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/sdk/typescript-packages/loader/build/")

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
set(HEADER_CONTENT "${HEADER_CONTENT}#define INCBIN_PREFIX\n")
set(HEADER_CONTENT "${HEADER_CONTENT}#include \"incbin.h\"\n")

set(HEADER_CONTENT "${HEADER_CONTENT}/** Millennium frontend library -- responsible for displaying Millennium UI, and loading themes */\n")
set(HEADER_CONTENT "${HEADER_CONTENT}INCTXT(MILLENNIUM_FRONTEND_BIN_, \"../../../assets/.millennium/Dist/index.js\");\n\n")

set(HEADER_CONTENT "${HEADER_CONTENT}/** Millennium API library */\n")
set(HEADER_CONTENT "${HEADER_CONTENT}INCTXT(MILLENNIUM_API_BIN_, \"../../../sdk/typescript-packages/loader/build/millennium.js\");\n")
set(HEADER_CONTENT "${HEADER_CONTENT}INCTXT(MILLENNIUM_API_BIN_MAP_, \"../../../sdk/typescript-packages/loader/build/millennium.js.map\");\n")

set(HEADER_CONTENT "${HEADER_CONTENT}/** Millennium API chunk files */\n")

set(MAP_ENTRIES "")
foreach(chunk_file ${CHUNK_FILES})
    file(RELATIVE_PATH rel_path "${CMAKE_SOURCE_DIR}" "${chunk_file}")
    get_filename_component(filename "${chunk_file}" NAME)
    filename_to_identifier("${filename}" var_name)
    
    set(HEADER_CONTENT "${HEADER_CONTENT}INCTXT(${var_name}, \"../../../${rel_path}\");\n")
    set(MAP_ENTRIES "${MAP_ENTRIES}\t{ fmt::format(\"https://millennium.ftp/{}/chunks/${filename}\", GetScrambledApiPathToken()), std::string(${var_name}Data) },\n")
endforeach()

set(HEADER_CONTENT "${HEADER_CONTENT}\n// clang-format off\n")
set(HEADER_CONTENT "${HEADER_CONTENT}std::unordered_map<std::string, std::string> INTERNAL_FTP_CALL_DATA = {\n")
set(HEADER_CONTENT "${HEADER_CONTENT}\t{ fmt::format(\"https://millennium.ftp/{}/millennium-frontend.js\", GetScrambledApiPathToken()), std::string(MILLENNIUM_FRONTEND_BIN_Data) },\n")
set(HEADER_CONTENT "${HEADER_CONTENT}\t{ fmt::format(\"https://millennium.ftp/{}/millennium.js\", GetScrambledApiPathToken()), std::string(MILLENNIUM_API_BIN_Data) },\n")
set(HEADER_CONTENT "${HEADER_CONTENT}\t{ fmt::format(\"https://millennium.ftp/{}/millennium.js.map\", GetScrambledApiPathToken()), std::string(MILLENNIUM_API_BIN_MAP_Data) },\n")
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
# LuaJIT Build Configuration
# =====================================

if(MINGW)
    # Make sure bash in PATH. It doesn't matter which bash, MinGW and Git for Windows both provide one.
    # MinGW bash is at C:/msys64/usr/bin/bash.exe if you have MSYS2 installed.
    set(LUAJIT_BUILD_COMMAND bash -c "cd '${CMAKE_SOURCE_DIR}/vendor/lua' && mingw32-make BUILDMODE=static")
elseif(UNIX)
    set(LUAJIT_BUILD_COMMAND make -C "${CMAKE_SOURCE_DIR}/vendor/lua" HOST_CC="gcc -m32" BUILDMODE=static)
endif()

execute_process(
    COMMAND ${LUAJIT_BUILD_COMMAND}
    RESULT_VARIABLE LUAJIT_BUILD_RESULT
    OUTPUT_VARIABLE LUAJIT_BUILD_OUTPUT
    ERROR_VARIABLE LUAJIT_BUILD_ERROR
)

message(STATUS "LuaJIT build command:\n${LUAJIT_BUILD_COMMAND}")

if(NOT LUAJIT_BUILD_RESULT EQUAL 0)
    set(_luajit_fail_msg "Failed to build LuaJIT static library:\n")
    string(APPEND _luajit_fail_msg "Result code: ${LUAJIT_BUILD_RESULT}\n")
    string(APPEND _luajit_fail_msg "Error: ${LUAJIT_BUILD_ERROR}\n")
    string(APPEND _luajit_fail_msg "Output: ${LUAJIT_BUILD_OUTPUT}\n")
    if(LUAJIT_BUILD_ERROR STREQUAL "" AND LUAJIT_BUILD_OUTPUT STREQUAL "")
        string(APPEND _luajit_fail_msg "(No error or output was captured. Check your build environment and paths.)\n")
    endif()
    message(FATAL_ERROR "${_luajit_fail_msg}")
else()
    message(STATUS "LuaJIT static library built successfully.")
endif()

set(LUAJIT_DIR ${CMAKE_CURRENT_SOURCE_DIR}/vendor/lua/src)
set(LUAJIT_STATIC ${LUAJIT_DIR}/libluajit.a)


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

find_package(minizip-ng CONFIG REQUIRED) # used for extracting zip files
find_package(CURL REQUIRED) # used for web requests
find_package(PkgConfig REQUIRED)

# =====================================
# Link against libgit2
# =====================================

pkg_check_modules(LIBGIT2 REQUIRED libgit2)

include_directories(${LIBGIT2_INCLUDE_DIRS})
link_directories(${LIBGIT2_LIBRARY_DIRS})
add_definitions(${LIBGIT2_CFLAGS_OTHER})

if(WIN32)
    set(LIBGIT2_LIBRARIES
        ${CMAKE_SOURCE_DIR}/build/vcpkg_installed/x86-mingw-static/lib/libgit2.a
        ${CMAKE_SOURCE_DIR}/build/vcpkg_installed/x86-mingw-static/lib/libhttp_parser.a
        ${CMAKE_SOURCE_DIR}/build/vcpkg_installed/x86-mingw-static/lib/libpcre.a
        ${CMAKE_SOURCE_DIR}/build/vcpkg_installed/x86-mingw-static/lib/libzlib.a
    )
elseif(UNIX)
    set(LIBGIT2_LIBRARIES
        ${CMAKE_SOURCE_DIR}/build/vcpkg_installed/x86-linux/lib/libgit2.a
        ${CMAKE_SOURCE_DIR}/build/vcpkg_installed/x86-linux/lib/libhttp_parser.a
        ${CMAKE_SOURCE_DIR}/build/vcpkg_installed/x86-linux/lib/libpcre.a
        ${CMAKE_SOURCE_DIR}/build/vcpkg_installed/x86-linux/lib/libz.a
    )
endif()

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

    find_package(minhook CONFIG REQUIRED) # used for hooking functions in steam
    target_link_libraries(Millennium ${LIBGIT2_LIBRARIES} wsock32 Iphlpapi DbgHelp rpcrt4 winhttp ${PYTHON_LIB} minhook::minhook)

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

# Link required libraries for all platforms
target_link_libraries(Millennium MINIZIP::minizip-ng CURL::libcurl ${LUAJIT_STATIC} ${LIBGIT2_LIBRARIES})