set(CMAKE_POSITION_INDEPENDENT_CODE ON  CACHE BOOL     "Enable position independent code" FORCE)
set(BUILD_SHARED_LIBS               OFF CACHE BOOL     "Build static libraries by default" FORCE)
set(BUILD_STATIC_LIBS               ON  CACHE BOOL     "Build static libraries by default" FORCE)

set(BUILD_TESTS                     OFF CACHE BOOL     "Disable building tests" FORCE)
set(BUILD_EXAMPLES                  OFF CACHE BOOL     "Disable building examples" FORCE)
set(CMAKE_CXX_STANDARD_REQUIRED     ON  CACHE BOOL     "Enforce C++ standard" FORCE)
set(CMAKE_C_STANDARD_REQUIRED       ON  CACHE BOOL     "Enforce C standard" FORCE)
set(NGHTTP2_STATICLIB               ON  CACHE BOOL     "Use static nghttp2 library" FORCE)
set(BUILD_CURL_EXE                  OFF CACHE BOOL     "" FORCE)
set(CURL_USE_LIBSSH2                OFF CACHE BOOL     "" FORCE)
set(CURL_STATICLIB                  ON  CACHE BOOL     "Build curl as static library" FORCE)
set(CURL_USE_SECTRANSP              OFF CACHE BOOL     "Disable SecureTransport" FORCE)
set(CURL_USE_MBEDTLS                OFF CACHE BOOL     "Disable mbedTLS" FORCE)
set(CURL_USE_WOLFSSL                OFF CACHE BOOL     "Disable wolfSSL" FORCE)
set(CURL_USE_BEARSSL                OFF CACHE BOOL     "Disable BearSSL" FORCE)
set(CURL_DISABLE_INSTALL            ON  CACHE BOOL     "Disable curl install" FORCE)
set(CURL_DISABLE_LDAP               ON  CACHE BOOL     "Disable LDAP" FORCE)
set(CURL_DISABLE_LDAPS              ON  CACHE BOOL     "Disable LDAPS" FORCE)
set(CURL_DISABLE_RTSP               ON  CACHE BOOL     "Disable RTSP" FORCE)
set(CURL_DISABLE_PROXY              OFF CACHE BOOL     "Disable proxy support" FORCE)
set(CURL_DISABLE_DICT               ON  CACHE BOOL     "Disable DICT" FORCE)
set(CURL_DISABLE_TELNET             ON  CACHE BOOL     "Disable TELNET" FORCE)
set(CURL_DISABLE_TFTP               ON  CACHE BOOL     "Disable TFTP" FORCE)
set(CURL_DISABLE_POP3               ON  CACHE BOOL     "Disable POP3" FORCE)
set(CURL_DISABLE_IMAP               ON  CACHE BOOL     "Disable IMAP" FORCE)
set(CURL_DISABLE_SMTP               ON  CACHE BOOL     "Disable SMTP" FORCE)
set(CURL_DISABLE_GOPHER             ON  CACHE BOOL     "Disable Gopher" FORCE)
set(CURL_USE_LIBPSL 		        OFF CACHE BOOL     "Disable libpsl" FORCE)
set(MZ_BZIP2                        OFF CACHE BOOL     "Disable bzip2 support in minizip-ng" FORCE)
set(MZ_FETCH_LIBS                   OFF CACHE BOOL     "Disable fetching third-party libs in minizip-ng" FORCE)
set(MZ_ZLIB                         ON  CACHE BOOL     "Enable zlib support in minizip-ng" FORCE)
set(MZ_ICONV                        OFF CACHE BOOL     "Disable iconv in minizip-ng" FORCE)
set(ZLIB_COMPAT                     ON  CACHE BOOL     "Enable zlib compatibility mode in zlib-ng" FORCE)
set(ZLIB_ALIASES                    ON  CACHE BOOL     "Enable zlib alias symbols in zlib-ng" FORCE)
set(ZLIB_ENABLE_TESTS               OFF CACHE BOOL     "Disable building tests in zlib-ng" FORCE)
set(ZLIBNG_ENABLE_TESTS 	        OFF CACHE BOOL     "Disable building tests in zlib-ng" FORCE)
set(RE2_BUILD_TESTING               OFF CACHE BOOL     "Disable building tests in re2" FORCE)

if(WIN32)
    set(CURL_USE_SCHANNEL           ON  CACHE INTERNAL "" FORCE)
    set(CURL_WINDOWS_SSPI           ON  CACHE INTERNAL "" FORCE)
    set(CURL_USE_OPENSSL            OFF CACHE BOOL     "Use OpenSSL for TLS/SSL" FORCE)
else()
    set(CURL_USE_OPENSSL            ON  CACHE BOOL     "Use OpenSSL for TLS/SSL" FORCE)
    set(CURL_USE_SCHANNEL           OFF CACHE INTERNAL "" FORCE)
    set(CURL_WINDOWS_SSPI           OFF CACHE INTERNAL "" FORCE)
endif()

if(UNIX AND NOT APPLE AND CMAKE_C_FLAGS MATCHES "-m32")
    set(CMAKE_FIND_LIBRARY_CUSTOM_LIB_SUFFIX "32" CACHE STRING "Find 32-bit libraries" FORCE)

    set(_pc32 "/usr/lib32/pkgconfig:/usr/lib/i386-linux-gnu/pkgconfig")
    if(DEFINED ENV{PKG_CONFIG_PATH} AND NOT "$ENV{PKG_CONFIG_PATH}" STREQUAL "")
        set(ENV{PKG_CONFIG_PATH} "${_pc32}:$ENV{PKG_CONFIG_PATH}")
    else()
        set(ENV{PKG_CONFIG_PATH} "${_pc32}")
    endif()
endif()


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

message("")
message(STATUS "[MLNM] Configuring dependencies...")

if(DISTRO_NIX)
    find_package(ZLIB       REQUIRED)
    find_package(CURL       REQUIRED)
    find_package(minizip-ng REQUIRED)

    find_path(NLOHMANN_JSON_INCLUDE_DIR "nlohmann/json.hpp" REQUIRED)
    add_library(nlohmann_json INTERFACE)
    target_include_directories(nlohmann_json INTERFACE "${NLOHMANN_JSON_INCLUDE_DIR}")

    add_library(libcurl ALIAS CURL::libcurl)
    add_library(minizip ALIAS MINIZIP::minizip-ng)

    FetchContent_Declare(luajit SOURCE_DIR "${MILLENNIUM_BASE}/thirdparty/forks/luajit" SOURCE_SUBDIR fakedir)
    FetchContent_MakeAvailable(luajit)

    set(LUA_INCLUDE_DIR  luajit::header)
    set(LUA_INCLUDE_DIRS luajit::header)
    set(LUA_LIBRARY      luajit::lib   )
    set(LUA_LIBRARIES    luajit::lib   )

    include_directories(SYSTEM
        "${luajit_BINARY_DIR}"
        "${luajit_SOURCE_DIR}/src"
    )

    millennium_process_package("${luajit_SOURCE_DIR}")

    if(EXISTS "${luajit_SOURCE_DIR}/LuaJIT.cmake")
        file(READ "${luajit_SOURCE_DIR}/LuaJIT.cmake" _luajit_content)
        string(REPLACE
            [[COMMAND git show -s --format=${GIT_FORMAT} > ${CMAKE_CURRENT_BINARY_DIR}/luajit_relver.txt]]
            [[COMMAND ${CMAKE_COMMAND} -E copy ${LUAJIT_DIR}/.relver ${CMAKE_CURRENT_BINARY_DIR}/luajit_relver.txt]]
            _luajit_content "${_luajit_content}"
        )
        file(WRITE "${luajit_SOURCE_DIR}/LuaJIT.cmake" "${_luajit_content}")
        unset(_luajit_content)
    endif()

    set(_saved_c_flags   "${CMAKE_C_FLAGS}")
    set(_saved_cxx_flags "${CMAKE_CXX_FLAGS}")
    string(APPEND CMAKE_C_FLAGS   " -w")
    string(APPEND CMAKE_CXX_FLAGS " -w")

    add_subdirectory("${luajit_SOURCE_DIR}" "${luajit_BINARY_DIR}")

    set(CMAKE_C_FLAGS   "${_saved_c_flags}")
    set(CMAKE_CXX_FLAGS "${_saved_cxx_flags}")
else()
    set(THIRDPARTY_DIR "${MILLENNIUM_BASE}/thirdparty")
    FetchContent_Declare(zlib          URL "file://${THIRDPARTY_DIR}/zlib-2.2.5.tar.gz"           DOWNLOAD_EXTRACT_TIMESTAMP TRUE)
    FetchContent_Declare(luajit        SOURCE_DIR "${MILLENNIUM_BASE}/thirdparty/forks/luajit"      SOURCE_SUBDIR fakedir)
    FetchContent_Declare(nlohmann_json URL "file://${THIRDPARTY_DIR}/nlohmann_json-v3.12.0.tar.gz" DOWNLOAD_EXTRACT_TIMESTAMP TRUE SOURCE_SUBDIR fakedir)
    FetchContent_Declare(minizip_ng    URL "file://${THIRDPARTY_DIR}/minizip_ng-4.0.10.tar.gz"     DOWNLOAD_EXTRACT_TIMESTAMP TRUE SOURCE_SUBDIR fakedir)
    FetchContent_Declare(curl          URL "file://${THIRDPARTY_DIR}/curl-8_13_0.tar.gz"           DOWNLOAD_EXTRACT_TIMESTAMP TRUE SOURCE_SUBDIR fakedir)
    FetchContent_Declare(re2           GIT_REPOSITORY https://github.com/google/re2 GIT_TAG 2022-04-01 SOURCE_SUBDIR fakedir)

    set(DEPENDENCIES zlib luajit nlohmann_json minizip_ng curl re2)

    set(_saved_msg_level "${CMAKE_MESSAGE_LOG_LEVEL}")
    set(CMAKE_MESSAGE_LOG_LEVEL WARNING)

    foreach(dep ${DEPENDENCIES})
        string(TIMESTAMP start_time "%s")
        FetchContent_MakeAvailable(${dep})
        string(TIMESTAMP end_time "%s")
        math(EXPR duration "${end_time} - ${start_time}")

        set(CMAKE_MESSAGE_LOG_LEVEL "${_saved_msg_level}")
        set(CMAKE_MESSAGE_LOG_LEVEL WARNING)
    endforeach()

    if(MSVC)
        if(EXISTS "${curl_SOURCE_DIR}/CMake/PickyWarnings.cmake")
            file(READ "${curl_SOURCE_DIR}/CMake/PickyWarnings.cmake" _pw_content)
            string(REPLACE
                "  list(APPEND _picky \"-W4\")"
                "  if(PICKY_COMPILER)\n    list(APPEND _picky \"-W4\")\n  endif()"
                _pw_content "${_pw_content}"
            )
            file(WRITE "${curl_SOURCE_DIR}/CMake/PickyWarnings.cmake" "${_pw_content}")
            unset(_pw_content)
        endif()
        set(PICKY_COMPILER OFF CACHE BOOL "" FORCE)

        if(EXISTS "${minizip_ng_SOURCE_DIR}/CMakeLists.txt")
            file(READ "${minizip_ng_SOURCE_DIR}/CMakeLists.txt" _mz_content)
            string(REPLACE "add_compile_options($<$<CONFIG:Debug>:/W3>)" "" _mz_content "${_mz_content}")
            file(WRITE "${minizip_ng_SOURCE_DIR}/CMakeLists.txt" "${_mz_content}")
            unset(_mz_content)
        endif()
    endif()

    if(EXISTS "${curl_SOURCE_DIR}/CMakeLists.txt")
        file(READ "${curl_SOURCE_DIR}/CMakeLists.txt" _curl_content)
        string(REPLACE
            "message(WARNING \"Perl not found. Will not build manuals.\")"
            "# patched out by Millennium bootstrap_deps.cmake — manuals not shipped"
            _curl_content "${_curl_content}"
        )
        file(WRITE "${curl_SOURCE_DIR}/CMakeLists.txt" "${_curl_content}")
        unset(_curl_content)
    endif()

    set(LUA_INCLUDE_DIR  luajit::header)
    set(LUA_INCLUDE_DIRS luajit::header)
    set(LUA_LIBRARY      luajit::lib   )
    set(LUA_LIBRARIES    luajit::lib   )

    include_directories(SYSTEM
        "${minizip_ng_SOURCE_DIR}"
        "${luajit_BINARY_DIR}"
        "${luajit_SOURCE_DIR}/src"
        "${CMAKE_SOURCE_DIR}/../hhx64/include"
    )

    millennium_process_package("${luajit_SOURCE_DIR}")
    millennium_process_package("${curl_SOURCE_DIR}")

    if(EXISTS "${luajit_SOURCE_DIR}/LuaJIT.cmake")
        file(READ "${luajit_SOURCE_DIR}/LuaJIT.cmake" _luajit_content)
        string(REPLACE
            [[COMMAND git show -s --format=${GIT_FORMAT} > ${CMAKE_CURRENT_BINARY_DIR}/luajit_relver.txt]]
            [[COMMAND ${CMAKE_COMMAND} -E copy ${LUAJIT_DIR}/.relver ${CMAKE_CURRENT_BINARY_DIR}/luajit_relver.txt]]
            _luajit_content "${_luajit_content}"
        )
        file(WRITE "${luajit_SOURCE_DIR}/LuaJIT.cmake" "${_luajit_content}")
        unset(_luajit_content)
    endif()

    millennium_process_package("${nlohmann_json_SOURCE_DIR}")
    millennium_process_package("${minizip_ng_SOURCE_DIR}")
    millennium_process_package("${re2_SOURCE_DIR}")

    set(ZLIB_ROOT "${zlib_BINARY_DIR}")
    set(ZLIB_FOUND TRUE)
    if(WIN32)
        if(CMAKE_BUILD_TYPE STREQUAL "Debug")
            set(ZLIB_LIBRARY "${zlib_BINARY_DIR}/zlibstaticd${CMAKE_STATIC_LIBRARY_SUFFIX}")
        else()
            set(ZLIB_LIBRARY "${zlib_BINARY_DIR}/zlibstatic${CMAKE_STATIC_LIBRARY_SUFFIX}")
        endif()
    elseif(UNIX)
        set(ZLIB_LIBRARY "${zlib_BINARY_DIR}/libz${CMAKE_STATIC_LIBRARY_SUFFIX}")
    endif()

    set(ZLIB_LIBRARIES "${ZLIB_LIBRARY}")
    set(ZLIB_INCLUDE_DIRS "${zlib_SOURCE_DIR};${zlib_BINARY_DIR}")
    set(ZLIB_INCLUDE_DIR "${zlib_SOURCE_DIR};${zlib_BINARY_DIR}")

    set(_saved_c_flags   "${CMAKE_C_FLAGS}")
    set(_saved_cxx_flags "${CMAKE_CXX_FLAGS}")
    string(APPEND CMAKE_C_FLAGS   " -w")
    string(APPEND CMAKE_CXX_FLAGS " -w")

    add_subdirectory("${luajit_SOURCE_DIR}"        "${luajit_BINARY_DIR}"       )
    add_subdirectory("${curl_SOURCE_DIR}"          "${curl_BINARY_DIR}"         )
    add_subdirectory("${nlohmann_json_SOURCE_DIR}" "${nlohmann_json_BINARY_DIR}")
    add_subdirectory("${minizip_ng_SOURCE_DIR}"    "${minizip_ng_BINARY_DIR}"   )

    set(CMAKE_C_FLAGS   "${_saved_c_flags}")
    set(CMAKE_CXX_FLAGS "${_saved_cxx_flags}")
    set(CMAKE_MESSAGE_LOG_LEVEL "${_saved_msg_level}")

    add_dependencies(luajit zlib)
    add_dependencies(minizip zlib)
endif()
