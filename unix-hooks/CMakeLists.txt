cmake_minimum_required(VERSION 3.10)

project(unix-hooks LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if (CMAKE_BUILD_TYPE STREQUAL "Debug")
  set(MILLENNIUM_RUNTIME_PATH "${CMAKE_SOURCE_DIR}/build/libmillennium_x86.so")
  add_compile_definitions(MILLENNIUM_RUNTIME_PATH="${MILLENNIUM_RUNTIME_PATH}")
endif()

add_library(unix-hooks SHARED main.cc)

set_target_properties(unix-hooks PROPERTIES OUTPUT_NAME "millennium_bootstrap")
set_target_properties(unix-hooks PROPERTIES PREFIX "lib")
set_target_properties(unix-hooks PROPERTIES SUFFIX "_86x.so")