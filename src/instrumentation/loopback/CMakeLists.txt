cmake_minimum_required(VERSION 3.10)
project(hhx64 CXX)
include(${MILLENNIUM_BASE}/scripts/cmake/millennium_version.cmake)

string(TIMESTAMP CURRENT_YEAR "%Y")
set(VERSION_COMPANY      "Steam Homebrew")
set(VERSION_COPYRIGHT    "Steam Homebrew ${TIMESTAMP}")
set(VERSION_TRADEMARK1   "All Rights Reserved")
set(VERSION_DESCRIPTION  "A hook utility library for Millennium")
set(VERSION_FILENAME     "${MILLENNIUM_HOOK_HELPER_OUTPUT_NAME}")
set(VERSION_PRODUCT      "Millennium Webhelper Hook")
set(VERSION_FILETYPE     "VFT_DLL")

set(VERSION_FILE "${CMAKE_BINARY_DIR}/${MILLENNIUM_HOOK_HELPER_OUTPUT_NAME}.rc")

if(WIN32)
    configure_file(${MILLENNIUM_BASE}/scripts/version.rc.in ${VERSION_FILE} @ONLY)
endif()

set(CMAKE_C_STANDARD 99)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

if(UNIX AND NOT WIN32)
    # The parent project targets i686 (-m32), but hhx64 must be a native 64-bit
    # shared library because Steam's CEF renderer process is 64-bit.
    string(REPLACE "-m32" "" CMAKE_C_FLAGS   "${CMAKE_C_FLAGS}")
    string(REPLACE "-m32" "" CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS}")
    string(REPLACE "-m32" "" CMAKE_EXE_LINKER_FLAGS    "${CMAKE_EXE_LINKER_FLAGS}")
    string(REPLACE "-m32" "" CMAKE_SHARED_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS}")
    add_compile_options(-m64)
    add_link_options(-m64)
endif()
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(BUILD_STATIC_LIBS ON)
set(CMAKE_WINDOWS_EXPORT_ALL_SYMBOLS OFF)

include_directories("${MILLENNIUM_BASE}/src/include")
include_directories("${snare_SOURCE_DIR}")

set(SOURCE_FILES
    main.cc
    resource_transformer.cc
    resource_reader.cc
    resource_handler.cc
    resource_query_parser.cc
    resource_matcher.cc
    ${MILLENNIUM_BASE}/src/state/shared_memory.cc
)

if(WIN32)
    list(APPEND SOURCE_FILES "${MILLENNIUM_BASE}/src/include/millennium/export.version.def")
    list(APPEND SOURCE_FILES "${VERSION_FILE}")
elseif(APPLE)
    enable_language(OBJCXX)
    list(APPEND SOURCE_FILES "${MILLENNIUM_BASE}/src/platform/macos_window_style.mm")
endif()

add_library(hhx64 SHARED ${SOURCE_FILES})

if(CMAKE_C_COMPILER_ID STREQUAL "GNU" OR CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
    set_source_files_properties(main.cc PROPERTIES COMPILE_OPTIONS "-Wno-stringop-overflow")
endif()

if(WIN32)
    target_link_libraries(hhx64 wsock32)
endif()

if(APPLE)
    target_link_libraries(hhx64 "-framework AppKit" "-framework QuartzCore")
endif()

include(FetchContent)
FetchContent_GetProperties(re2)
add_subdirectory("${re2_SOURCE_DIR}" "${re2_BINARY_DIR}")

target_link_libraries(hhx64 re2)
set_target_properties(hhx64 PROPERTIES
    PREFIX ""
    OUTPUT_NAME "${MILLENNIUM_HOOK_HELPER_OUTPUT_NAME}"
    SUFFIX ""
)

if(UNIX AND NOT APPLE)
    set_target_properties(hhx64 PROPERTIES LINK_FLAGS "-Wl,--no-undefined")
    set_target_properties(hhx64 PROPERTIES POSITION_INDEPENDENT_CODE ON)
elseif(WIN32)
    target_link_libraries(hhx64 version)

    if(NOT GITHUB_ACTION_BUILD)
        include(${MILLENNIUM_BASE}/scripts/cmake/find_steam_path.cmake)
        find_steam_path(hhx64)
    endif()
endif()
