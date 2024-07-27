cmake_minimum_required(VERSION 3.5.0)
project(CLI)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_C_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_C_STANDARD_REQUIRED ON)

include_directories(
  ${CMAKE_SOURCE_DIR}/vendor/ini/src
)

# Add your source files here
set(SOURCES main.cc)

add_executable(CLI ${SOURCES})
set_target_properties(CLI PROPERTIES OUTPUT_NAME "millennium")