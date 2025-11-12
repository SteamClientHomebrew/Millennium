cmake_minimum_required(VERSION 3.10)
include(FetchContent)

project(mwh CXX)

set(CMAKE_C_STANDARD 99)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(BUILD_STATIC_LIBS ON)
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -m64")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -m64")
set(CMAKE_SHARED_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS} -m64")
set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -m64")
set(ABSL_ENABLE_INSTALL ON)
set(ABSL_PROPAGATE_CXX_STD ON)
set(CMAKE_WINDOWS_EXPORT_ALL_SYMBOLS OFF)

FetchContent_Declare(abseil GIT_REPOSITORY https://github.com/abseil/abseil-cpp GIT_TAG 20240722.0)
FetchContent_MakeAvailable(abseil)

FetchContent_Declare(re2 GIT_REPOSITORY https://github.com/google/re2 GIT_TAG 2025-11-05)
FetchContent_MakeAvailable(re2)

FetchContent_Declare(minhook GIT_REPOSITORY https://github.com/TsudaKageyu/minhook GIT_TAG v1.3.4)
FetchContent_MakeAvailable(minhook)

include_directories(include)
include_directories(./)

add_library(mwh SHARED
    src/main.cc
    src/patch.cc
    src/match.cc
    src/smrq_handler.cc
    src/smem.cc
    src/urlp.cc
    src/fread.cc
    ../../scripts/exports.def
)

target_link_libraries(mwh re2 minhook)
set_target_properties(mwh PROPERTIES LINK_FLAGS "-Wl,--no-undefined")

if(UNIX)
set_target_properties(mwh PROPERTIES POSITION_INDEPENDENT_CODE ON)
    set_target_properties(mwh PROPERTIES LIBRARY_OUTPUT_DIRECTORY "$ENV{HOME}/.steam/steam/ubuntu12_64")
    set_target_properties(mwh PROPERTIES OUTPUT_NAME "millennium_mwh")
    set_target_properties(mwh PROPERTIES PREFIX "lib")
    set_target_properties(mwh PROPERTIES SUFFIX "_x64.so")
elseif(WIN32)
    set_target_properties(mwh PROPERTIES RUNTIME_OUTPUT_DIRECTORY "C:/Program Files (x86)/Steam/bin/cef/cef.win7x64")
    set_target_properties(mwh PROPERTIES OUTPUT_NAME "version") # hhx64
    set_target_properties(mwh PROPERTIES PREFIX "")
endif()

