cmake_minimum_required(VERSION 3.21)
project(MillenniumBuildSlave)
include(ExternalProject)

if(WIN32)
    include(scripts/cmake/msvc.cmake)
elseif(UNIX)
    include(scripts/cmake/gcc.cmake)
endif()

message(STATUS "Ninja compiler path: ${USER_CMAKE_MAKE_PROGRAM}")
message(STATUS "X86 toolchain args: ${X86_TOOLCHAIN_ARGS}")

set(COMMON_CMAKE_ARGS
    -DCMAKE_BUILD_TYPE=${CMAKE_BUILD_TYPE}
    -DCMAKE_CXX_STANDARD=${CMAKE_CXX_STANDARD}
    -DCMAKE_C_STANDARD=${CMAKE_C_STANDARD}
    -DCMAKE_MAKE_PROGRAM=${USER_CMAKE_MAKE_PROGRAM}
    -DCMAKE_EXPORT_COMPILE_COMMANDS=${CMAKE_EXPORT_COMPILE_COMMANDS}
    -DMILLENNIUM_BASE=${CMAKE_SOURCE_DIR}
    -DGITHUB_ACTION_BUILD=${GITHUB_ACTION_BUILD}
    -DCMAKE_CXX_FLAGS_RELEASE=${CMAKE_CXX_FLAGS_RELEASE}
    -DCMAKE_SHARED_LINKER_FLAGS_RELEASE=${CMAKE_SHARED_LINKER_FLAGS_RELEASE}
)

ExternalProject_Add(millennium_x86
    SOURCE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/src
    PREFIX ${CMAKE_BINARY_DIR}
    CMAKE_GENERATOR "Ninja"
    CMAKE_ARGS ${COMMON_CMAKE_ARGS} ${X86_TOOLCHAIN_ARGS} -DLJ_COMPILE_TARGET_X64=OFF -DMILLENNIUM_32BIT=ON
    INSTALL_COMMAND ""
    BUILD_ALWAYS 1
)

if (WIN32)
    ExternalProject_Add(millennium_x64
        SOURCE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/src
        PREFIX ${CMAKE_BINARY_DIR}
        CMAKE_GENERATOR "Ninja"
        CMAKE_ARGS ${COMMON_CMAKE_ARGS} -DLJ_COMPILE_TARGET_X64=ON -DMILLENNIUM_32BIT=OFF
        INSTALL_COMMAND ""
        BUILD_ALWAYS 1
    )
endif()

ExternalProject_Add(hhx64
    SOURCE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/src/hhx64
    PREFIX ${CMAKE_BINARY_DIR}
    CMAKE_GENERATOR "Ninja"
    CMAKE_ARGS ${COMMON_CMAKE_ARGS}
    INSTALL_COMMAND ""
    BUILD_ALWAYS 1
)

find_package(Git QUIET)

if(GIT_FOUND AND EXISTS "${PROJECT_SOURCE_DIR}/.git")
    execute_process(COMMAND ${GIT_EXECUTABLE} submodule update --init --recursive
            WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
            RESULT_VARIABLE GIT_SUBMOD_RESULT)
    if(NOT GIT_SUBMOD_RESULT EQUAL "0")
        message(FATAL_ERROR "git submodule update --init --recursive failed with ${GIT_SUBMOD_RESULT}, please checkout submodules")
    endif()
endif()

find_package(pnpm QUIET)
if (PNPM_FOUND AND EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/sdk" AND EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/src/frontend")
    add_custom_command(
        COMMAND pnpm install && pnpm run build
        WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/sdk"
    )
    add_custom_command(
        COMMAND pnpm install && pnpm run build
        WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/src/frontend"
    )
endif()