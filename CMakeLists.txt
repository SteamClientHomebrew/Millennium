cmake_minimum_required(VERSION 3.21)
project(build_slave)
include(ExternalProject)

# Reverse the MSVC platform from the compiler path cmake set. 
get_filename_component(COMPILER_DIR   ${CMAKE_CXX_COMPILER} DIRECTORY)
get_filename_component(COMPILER_DIR   ${COMPILER_DIR}       DIRECTORY)
get_filename_component(COMPILER_DIR   ${COMPILER_DIR}       DIRECTORY)
get_filename_component(COMPILER_DIR   ${COMPILER_DIR}       DIRECTORY)

# Get the base directory of the MSVC installation
get_filename_component(BASE_DIRECTORY ${COMPILER_DIR}       DIRECTORY)
get_filename_component(BASE_DIRECTORY ${BASE_DIRECTORY}     DIRECTORY)
get_filename_component(BASE_DIRECTORY ${BASE_DIRECTORY}     DIRECTORY)
get_filename_component(BASE_DIRECTORY ${BASE_DIRECTORY}     DIRECTORY)

set(MSVC_NINJA "${BASE_DIRECTORY}/Common7/IDE/CommonExtensions/Microsoft/CMake/Ninja/ninja.exe")
message(STATUS "[MSVC] Using Ninja at: ${MSVC_NINJA}")

set(COMMON_CMAKE_ARGS
    -DCMAKE_BUILD_TYPE=${CMAKE_BUILD_TYPE}
    -DCMAKE_CXX_STANDARD=${CMAKE_CXX_STANDARD}
    -DCMAKE_C_STANDARD=${CMAKE_C_STANDARD}
    -DCMAKE_MAKE_PROGRAM=${MSVC_NINJA}
    -DCMAKE_EXPORT_COMPILE_COMMANDS=${CMAKE_EXPORT_COMPILE_COMMANDS}
    -DMILLENNIUM_BASE=${CMAKE_SOURCE_DIR}
)

# MSVC-specific toolchain args for building Millennium in 32-bit mode
set(MSVC_TOOLCHAIN_ARGS
    -DCMAKE_TOOLCHAIN_FILE=${CMAKE_CURRENT_SOURCE_DIR}/scripts/cmake/x86_msvc_toolchain.cmake
    -DMSVC_BASE_PATH=${COMPILER_DIR}
    -DMSVC_SDK_VERSION=${CMAKE_VS_WINDOWS_TARGET_PLATFORM_VERSION}
    -DMSVC_TOOLSET_VERSION=${MSVC_TOOLSET_VERSION}
)

ExternalProject_Add(millennium
    SOURCE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/src
    PREFIX ${CMAKE_BINARY_DIR}/millennium
    CMAKE_GENERATOR "Ninja"
    CMAKE_ARGS ${COMMON_CMAKE_ARGS} ${MSVC_TOOLCHAIN_ARGS}
    INSTALL_COMMAND ""
    BUILD_ALWAYS 1
)

ExternalProject_Add(hhx64
    SOURCE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/src/hhx64
    PREFIX ${CMAKE_BINARY_DIR}/hhx64
    CMAKE_GENERATOR "Ninja"
    CMAKE_ARGS ${COMMON_CMAKE_ARGS}
    INSTALL_COMMAND ""
    BUILD_ALWAYS 1
)