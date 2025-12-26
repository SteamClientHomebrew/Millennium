cmake_minimum_required(VERSION 3.10)
message(STATUS "[hhx64] Using compiler: ${CMAKE_CXX_COMPILER}")

include(FetchContent)
project(hhx64 CXX)
include(${MILLENNIUM_BASE}/scripts/cmake/millennium_version.cmake)

if(WIN32)
    set(BUILD_PREFIX "")
    set(BUILD_NAME "millennium")
    set(BUILD_SUFFIX ".hhx64.dll")
else()
    set(BUILD_PREFIX "lib")
    set(BUILD_NAME "millennium_hh")
    set(BUILD_SUFFIX "x64.so")
endif()

set(BUILD_FULLNAME "${BUILD_PREFIX}${BUILD_NAME}${BUILD_SUFFIX}")

set(VERSION_COMPANY      "Steam Homebrew")
set(VERSION_COPYRIGHT    "Steam Homebrew 2025")
set(VERSION_TRADEMARK1   "All Rights Reserved")
set(VERSION_DESCRIPTION  "A hook utility library for Millennium")
set(VERSION_FILENAME     "${BUILD_FULLNAME}")
set(VERSION_PRODUCT      "Millennium Webhelper Hook")

if(WIN32)
    project(hhx64-log-viewer CXX)
    configure_file(${MILLENNIUM_BASE}/scripts/version.rc.in ${CMAKE_BINARY_DIR}/version.rc @ONLY)
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

function(millennium_process_package input_path)
    if(EXISTS "${input_path}/CMakeLists.txt")
        file(READ "${input_path}/CMakeLists.txt" CMAKELISTS_CONTENTS)
        string(REGEX REPLACE "cmake_minimum_required[ \t]*\\([ \t]*VERSION[^)]*\\)" "cmake_minimum_required(VERSION 3.21)" CMAKELISTS_CONTENTS "${CMAKELISTS_CONTENTS}")
        file(WRITE "${input_path}/CMakeLists.txt" "${CMAKELISTS_CONTENTS}")
    else()
        message(WARNING "Could not find ${input_path} to patch.")
    endif()
endfunction()

FetchContent_Declare(abseil GIT_REPOSITORY https://github.com/abseil/abseil-cpp GIT_TAG 20240722.0 SOURCE_SUBDIR fakedir)
FetchContent_Declare(re2    GIT_REPOSITORY https://github.com/google/re2        GIT_TAG 2025-11-05 SOURCE_SUBDIR fakedir)

FetchContent_MakeAvailable(abseil re2)

millennium_process_package("${abseil_SOURCE_DIR}")
millennium_process_package("${re2_SOURCE_DIR}")

add_subdirectory("${abseil_SOURCE_DIR}" "${abseil_BINARY_DIR}")
add_subdirectory("${re2_SOURCE_DIR}"    "${re2_BINARY_DIR}"   )

if(WIN32)
    message(STATUS "[hhx64] fetching minhook...")
    FetchContent_Declare(minhook GIT_REPOSITORY https://github.com/TsudaKageyu/minhook GIT_TAG v1.3.4)
    FetchContent_MakeAvailable(minhook)
endif()

include_directories("${MILLENNIUM_BASE}/src/include")

set(SOURCE_FILES
    main.cc
    patch.cc
    match.cc
    smem.cc
    urlp.cc
    fread.cc
    loopback.cc
)

if(WIN32)
    list(APPEND SOURCE_FILES "${MILLENNIUM_BASE}/scripts/exports.def")
    list(APPEND SOURCE_FILES "${CMAKE_BINARY_DIR}/version.rc")
endif()

add_library(hhx64 SHARED ${SOURCE_FILES})

if(WIN32)
    add_executable(hhx64-log-viewer log_viewer.cc)
    target_link_libraries(hhx64 minhook wsock32)
endif()

target_link_libraries(hhx64 re2)

set_target_properties(hhx64 PROPERTIES PREFIX "${BUILD_PREFIX}")
set_target_properties(hhx64 PROPERTIES OUTPUT_NAME "${BUILD_NAME}")
set_target_properties(hhx64 PROPERTIES SUFFIX "${BUILD_SUFFIX}")

if(UNIX)
    set_target_properties(hhx64 PROPERTIES LINK_FLAGS "-Wl,--no-undefined")
    set_target_properties(hhx64 PROPERTIES POSITION_INDEPENDENT_CODE ON)
elseif(WIN32)
    target_link_libraries(hhx64 version)

    if(NOT GITHUB_ACTION_BUILD)
        include(${MILLENNIUM_BASE}/scripts/cmake/find_steam_path.cmake)

        if (out_steam_path)
            set_target_properties(hhx64 PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${out_steam_path}" LIBRARY_OUTPUT_DIRECTORY "${out_steam_path}" ARCHIVE_OUTPUT_DIRECTORY "${out_steam_path}")
        endif()
    endif()
endif()
