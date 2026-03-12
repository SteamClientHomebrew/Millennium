cmake_minimum_required(VERSION 3.10)
message(STATUS "[hhx64] Using compiler: ${CMAKE_CXX_COMPILER}")

include(FetchContent)
project(hhx64 CXX)
include(${MILLENNIUM_BASE}/scripts/cmake/millennium_version.cmake)

string(TIMESTAMP CURRENT_YEAR "%Y")
set(VERSION_COMPANY      "Steam Homebrew")
set(VERSION_COPYRIGHT    "Steam Homebrew ${TIMESTAMP}")
set(VERSION_TRADEMARK1   "All Rights Reserved")
set(VERSION_DESCRIPTION  "A hook utility library for Millennium")
set(VERSION_FILENAME     "${MILLENNIUM_HOOK_HELPER_OUTPUT_NAME}")
set(VERSION_PRODUCT      "Millennium Webhelper Hook")

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
set(ABSL_ENABLE_INSTALL ON)
set(ABSL_PROPAGATE_CXX_STD ON)
set(ABSL_MSVC_STATIC_RUNTIME ON)
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
if(DISTRO_NIX)
    FetchContent_Declare(abseil SOURCE_DIR "${MILLENNIUM_BASE}/deps/abseil"     SOURCE_SUBDIR fakedir)
    FetchContent_Declare(re2    SOURCE_DIR "${MILLENNIUM_BASE}/deps/re2"        SOURCE_SUBDIR fakedir)
else()
    FetchContent_Declare(abseil GIT_REPOSITORY https://github.com/abseil/abseil-cpp GIT_TAG 20240722.0 SOURCE_SUBDIR fakedir)
    FetchContent_Declare(re2    GIT_REPOSITORY https://github.com/google/re2        GIT_TAG 2025-11-05 SOURCE_SUBDIR fakedir)
endif()

FetchContent_MakeAvailable(abseil re2)

millennium_process_package("${abseil_SOURCE_DIR}")
millennium_process_package("${re2_SOURCE_DIR}")

add_subdirectory("${abseil_SOURCE_DIR}" "${abseil_BINARY_DIR}")
add_subdirectory("${re2_SOURCE_DIR}"    "${re2_BINARY_DIR}"   )


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

if(WIN32)
    target_link_libraries(hhx64 wsock32)
endif()

if(APPLE)
    target_link_libraries(hhx64 "-framework AppKit" "-framework QuartzCore")
endif()

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
