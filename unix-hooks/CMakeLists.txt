cmake_minimum_required(VERSION 3.10)

project(unix-hooks LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_library(unix-hooks SHARED main.cc)

if(CMAKE_BUILD_TYPE STREQUAL "Debug")
    set_target_properties(unix-hooks PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY "$ENV{HOME}/.local/share/Steam/ubuntu12_32"
        LIBRARY_OUTPUT_DIRECTORY "$ENV{HOME}/.local/share/Steam/ubuntu12_32"
    )
endif()

set_target_properties(unix-hooks PROPERTIES OUTPUT_NAME "Xtst")
set_target_properties(unix-hooks PROPERTIES PREFIX "lib")
set_target_properties(unix-hooks PROPERTIES SUFFIX ".so.6")