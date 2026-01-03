cmake_minimum_required(VERSION 3.10)
project(preload)

set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED True)

set(BUILD_PREFIX "")
set(BUILD_NAME   "wsock32")
set(BUILD_SUFFIX ".dll")

set(BUILD_FULLNAME  "${BUILD_PREFIX}${BUILD_NAME}${BUILD_SUFFIX}")

set(VERSION_DESCRIPTION  "A bootstrapper utility for Millennium")
set(VERSION_COMPANY      "Steam Homebrew")
set(VERSION_COPYRIGHT    "Steam Homebrew 2025")
set(VERSION_FILENAME     "${BUILD_FULLNAME}")
set(VERSION_PRODUCT      "Millennium Bootstrapper")
set(VERSION_TRADEMARK1   "All Rights Reserved")

configure_file(${MILLENNIUM_BASE}/scripts/version.rc.in ${CMAKE_BINARY_DIR}/version.rc @ONLY)

set(WSOCK_SOURCE_FILES
    main.c
    ${CMAKE_BINARY_DIR}/version.rc
)

list(APPEND WSOCK_SOURCE_FILES ${MILLENNIUM_BASE}/src/include/millennium/export.wsock32.def)
add_compile_definitions(MILLENNIUM_64BIT)

add_library(preload SHARED ${WSOCK_SOURCE_FILES})
target_link_libraries(preload wsock32)

if (NOT GITHUB_ACTION_BUILD)
    include(${MILLENNIUM_BASE}/scripts/cmake/find_steam_path.cmake)
    find_steam_path(preload)
endif()

set_target_properties(preload PROPERTIES PREFIX      "${BUILD_PREFIX}")
set_target_properties(preload PROPERTIES OUTPUT_NAME "${BUILD_NAME}"  )
set_target_properties(preload PROPERTIES SUFFIX      "${BUILD_SUFFIX}")

project(preload-legacy)

set(VERSION_SOURCE_FILES
    "version.c"
    ${CMAKE_BINARY_DIR}/version.rc
)

list(APPEND VERSION_SOURCE_FILES ${MILLENNIUM_BASE}/src/include/millennium/export.version.def)
add_compile_definitions(MILLENNIUM_64BIT)

add_library(preload-legacy SHARED ${VERSION_SOURCE_FILES})
target_link_libraries(preload-legacy version)

if (NOT GITHUB_ACTION_BUILD)
    include(${MILLENNIUM_BASE}/scripts/cmake/find_steam_path.cmake)
    find_steam_path(preload-legacy)
endif()
 
set_target_properties(preload-legacy PROPERTIES PREFIX              "")
set_target_properties(preload-legacy PROPERTIES OUTPUT_NAME  "version")
set_target_properties(preload-legacy PROPERTIES SUFFIX          ".dll")
