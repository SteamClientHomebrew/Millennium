cmake_minimum_required(VERSION 3.10)
project(preload)

set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED True)

string(TIMESTAMP CURRENT_YEAR "%Y")
set(VERSION_DESCRIPTION  "A bootstrapper utility for Millennium")
set(VERSION_COMPANY      "Steam Homebrew")
set(VERSION_COPYRIGHT    "Steam Homebrew ${TIMESTAMP}")
set(VERSION_FILENAME     "${MILLENNIUM_BOOTLOADER_OUTPUT_NAME}")
set(VERSION_PRODUCT      "Millennium Bootstrapper")
set(VERSION_TRADEMARK1   "All Rights Reserved")

set(VERSION_FILE "${CMAKE_BINARY_DIR}/${MILLENNIUM_BOOTLOADER_OUTPUT_NAME}.rc")
configure_file(${MILLENNIUM_BASE}/scripts/version.rc.in ${VERSION_FILE} @ONLY)

set(VERSION_SOURCE_FILES
    "main.c"
    ${VERSION_FILE}
)

list(APPEND VERSION_SOURCE_FILES ${MILLENNIUM_BASE}/src/include/millennium/export.wsock32.def)
add_compile_definitions(MILLENNIUM_64BIT)

add_library(preload SHARED ${VERSION_SOURCE_FILES})
target_link_libraries(preload wsock32)

if (NOT GITHUB_ACTION_BUILD)
    include(${MILLENNIUM_BASE}/scripts/cmake/find_steam_path.cmake)
    find_steam_path(preload)
endif()

set_target_properties(preload PROPERTIES
    PREFIX ""
    OUTPUT_NAME "${MILLENNIUM_BOOTLOADER_OUTPUT_NAME}"
    SUFFIX ""
)

add_subdirectory(legacy)
