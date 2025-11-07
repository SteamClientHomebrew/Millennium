cmake_minimum_required(VERSION 3.10)
project(mwh C)
project(mwh-test C)

set(CMAKE_C_STANDARD 99)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(CMAKE_C_STANDARD_REQUIRED ON)

# Find zstd library
find_package(PkgConfig REQUIRED)
pkg_check_modules(ZSTD REQUIRED libzstd)

string(REGEX REPLACE "-m32" "" CMAKE_C_FLAGS "${CMAKE_C_FLAGS}")
string(REGEX REPLACE "-m32" "" CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS}")
string(REGEX REPLACE "-m32" "" CMAKE_SHARED_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS}")

set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -m64 -Wno-psabi")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -m64 -Wno-psabi")
set(CMAKE_SHARED_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS} -m64")
set(BUILD_STATIC_LIBS, ON)

add_subdirectory("${pcre2_SOURCE_DIR}"         "${pcre2_BINARY_DIR}"        )

message(STATUS "${CMAKE_C_FLAGS}")
message(STATUS "${CMAKE_CXX_FLAGS}")
message(STATUS "${CMAKE_SHARED_LINKER_FLAGS}")

include_directories(include)
include_directories(./)

include_directories("${vecscan_SOURCE_DIR}/src")
include_directories("${vecscan_BINARY_DIR}")

add_subdirectory("${vecscan_SOURCE_DIR}" "${vecscan_BINARY_DIR}")

add_compile_definitions(
    CEF_STRING_TYPE_UTF8
)

add_library(mwh SHARED
    src/main.c
    src/patch.c
    src/vecscan.c
    src/match.c
    src/smrq_handler.c
    src/smem.c
    src/urlp.c
    src/fread.c
)

add_executable(mwh-test src/test_regex.c)


target_link_libraries(mwh dl ${ZSTD_LIBRARIES} hs pcre2-8-static)
target_link_libraries(mwh-test hs)
target_include_directories(mwh PRIVATE ${ZSTD_INCLUDE_DIRS})

set_target_properties(mwh PROPERTIES LINK_FLAGS "-Wl,--no-undefined")
set_target_properties(mwh PROPERTIES POSITION_INDEPENDENT_CODE ON)
set_target_properties(mwh PROPERTIES LIBRARY_OUTPUT_DIRECTORY "$ENV{HOME}/.steam/steam/ubuntu12_64")

set_target_properties(mwh PROPERTIES OUTPUT_NAME "millennium_mwh")
set_target_properties(mwh PROPERTIES PREFIX "lib")
set_target_properties(mwh PROPERTIES SUFFIX "_x64.so")
