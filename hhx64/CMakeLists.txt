cmake_minimum_required(VERSION 3.10)
message(STATUS "[hhx64] Using compiler: ${CMAKE_CXX_COMPILER}")

include(FetchContent)

project(hhx64 CXX)
project(hhx64-log-viewer CXX)

set(CMAKE_C_STANDARD 99)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(BUILD_STATIC_LIBS ON)
set(ABSL_ENABLE_INSTALL ON)
set(ABSL_PROPAGATE_CXX_STD ON)
set(CMAKE_WINDOWS_EXPORT_ALL_SYMBOLS OFF)

message(STATUS "[hhx64] fetching abseil...")
FetchContent_Declare(abseil GIT_REPOSITORY https://github.com/abseil/abseil-cpp GIT_TAG 20240722.0)
FetchContent_MakeAvailable(abseil)

message(STATUS "[hhx64] fetching re2...")
FetchContent_Declare(re2 GIT_REPOSITORY https://github.com/google/re2 GIT_TAG 2025-11-05)
FetchContent_MakeAvailable(re2)

message(STATUS "[hhx64] fetching minhook...")
FetchContent_Declare(minhook GIT_REPOSITORY https://github.com/TsudaKageyu/minhook GIT_TAG v1.3.4)
FetchContent_MakeAvailable(minhook)

include_directories(include)
include_directories(./)

add_library(hhx64 SHARED
    src/main.cc
    src/patch.cc
    src/match.cc
    src/smrq_handler.cc
    src/smem.cc
    src/urlp.cc
    src/fread.cc
    ../scripts/exports.def
)

add_executable(hhx64-log-viewer src/log_viewer.cc)
target_link_libraries(hhx64 re2 minhook)

if(UNIX)
    set_target_properties(hhx64 PROPERTIES LINK_FLAGS "-Wl,--no-undefined")
    set_target_properties(hhx64 PROPERTIES POSITION_INDEPENDENT_CODE ON)
    set_target_properties(hhx64 PROPERTIES LIBRARY_OUTPUT_DIRECTORY "$ENV{HOME}/.steam/steam/ubuntu12_64")
    set_target_properties(hhx64 PROPERTIES OUTPUT_NAME "millennium_hhx64")
    set_target_properties(hhx64 PROPERTIES PREFIX "lib")
    set_target_properties(hhx64 PROPERTIES SUFFIX "_x64.so")
elseif(WIN32)
    target_link_libraries(hhx64 version)
    set(BUILD_PATH "C:/Program Files (x86)/Steam/bin/cef/cef.win7x64")

    set_target_properties(hhx64 PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY "${BUILD_PATH}"
        LIBRARY_OUTPUT_DIRECTORY "${BUILD_PATH}"
        ARCHIVE_OUTPUT_DIRECTORY "${BUILD_PATH}"
    )

    foreach(CONFIG ${CMAKE_CONFIGURATION_TYPES})
        string(TOUPPER ${CONFIG} CONFIG_UPPER)
        set_target_properties(hhx64 PROPERTIES
            RUNTIME_OUTPUT_DIRECTORY_${CONFIG_UPPER} "${BUILD_PATH}"
            LIBRARY_OUTPUT_DIRECTORY_${CONFIG_UPPER} "${BUILD_PATH}"
            ARCHIVE_OUTPUT_DIRECTORY_${CONFIG_UPPER} "${BUILD_PATH}"
        )
    endforeach()

    set_target_properties(hhx64 PROPERTIES OUTPUT_NAME "version") # hhx64
    set_target_properties(hhx64 PROPERTIES PREFIX "")
endif()

