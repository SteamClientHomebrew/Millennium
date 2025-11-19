cmake_minimum_required(VERSION 3.10)
message(STATUS "[hhx64] Using compiler: ${CMAKE_CXX_COMPILER}")

include(FetchContent)

project(hhx64 CXX)
if(WIN32)
    project(hhx64-log-viewer CXX)
endif()

set(CMAKE_C_STANDARD 99)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)
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

if(WIN32)
    message(STATUS "[hhx64] fetching minhook...")
    FetchContent_Declare(minhook GIT_REPOSITORY https://github.com/TsudaKageyu/minhook GIT_TAG v1.3.4)
    FetchContent_MakeAvailable(minhook)
endif()

include_directories("${MILLENNIUM_BASE}/src/include")

add_library(hhx64 SHARED
    main.cc
    patch.cc
    match.cc
    smem.cc
    urlp.cc
    fread.cc
    loopback.cc
    ${MILLENNIUM_BASE}/scripts/exports.def
)
target_link_libraries(hhx64 re2)
if(WIN32)
    add_executable(hhx64-log-viewer log_viewer.cc)
    target_link_libraries(hhx64 minhook)
endif()

if(UNIX)
    set_target_properties(hhx64 PROPERTIES LINK_FLAGS "-Wl,--no-undefined")
    set_target_properties(hhx64 PROPERTIES POSITION_INDEPENDENT_CODE ON)
    set_target_properties(hhx64 PROPERTIES OUTPUT_NAME "millennium_hh")
    set_target_properties(hhx64 PROPERTIES PREFIX "lib")
    set_target_properties(hhx64 PROPERTIES SUFFIX "x64.so")
elseif(WIN32)
    target_link_libraries(hhx64 version)

    if(NOT GITHUB_ACTION_BUILD)
        include(${MILLENNIUM_BASE}/scripts/cmake/find_steam_path.cmake)

        if (out_steam_path)
            set_target_properties(hhx64 PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${out_steam_path}" LIBRARY_OUTPUT_DIRECTORY "${out_steam_path}" ARCHIVE_OUTPUT_DIRECTORY "${out_steam_path}")
        endif()
    endif()

    set_target_properties(hhx64 PROPERTIES PREFIX "")
    set_target_properties(hhx64 PROPERTIES OUTPUT_NAME "millennium")
    set_target_properties(hhx64 PROPERTIES SUFFIX ".hhx64.dll")
endif()
