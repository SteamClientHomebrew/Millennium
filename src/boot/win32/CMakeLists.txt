cmake_minimum_required(VERSION 3.10)
project(preload)

set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED True)

set(BUILD_PREFIX    "")
set(BUILD_NAME      "version")
set(BUILD_SUFFIX    ".dll")
set(BUILD_FULLNAME  "${BUILD_PREFIX}${BUILD_NAME}${BUILD_SUFFIX}")

set(VERSION_DESCRIPTION  "A bootstrapper utility for Millennium")
set(VERSION_COMPANY      "Steam Homebrew")
set(VERSION_COPYRIGHT    "Steam Homebrew 2025")
set(VERSION_FILENAME     "${BUILD_FULLNAME}")
set(VERSION_PRODUCT      "Millennium Bootstrapper")
set(VERSION_TRADEMARK1   "All Rights Reserved")

configure_file(${MILLENNIUM_BASE}/scripts/version.rc.in ${CMAKE_BINARY_DIR}/version.rc @ONLY)
add_library(preload SHARED main.c ${MILLENNIUM_BASE}/scripts/exports.def ${CMAKE_BINARY_DIR}/version.rc)
target_link_libraries(preload version)

if (NOT GITHUB_ACTION_BUILD)
    include(${MILLENNIUM_BASE}/scripts/cmake/find_steam_path.cmake)

    if (out_steam_path)
        set_target_properties(preload PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${out_steam_path}" LIBRARY_OUTPUT_DIRECTORY "${out_steam_path}" ARCHIVE_OUTPUT_DIRECTORY "${out_steam_path}")
    endif()
endif()

set_target_properties(preload PROPERTIES OUTPUT_NAME "version" PREFIX "" NO_EXPORT TRUE)
