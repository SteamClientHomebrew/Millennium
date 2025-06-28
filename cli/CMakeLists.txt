cmake_minimum_required(VERSION 3.10)

set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED True)

# Set the project name and language
project(cli)
include_directories(include)
include_directories(../vendor/fmt/include)

# Optimize the build
if(CMAKE_BUILD_TYPE STREQUAL "Release")
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -O3 -s -fdata-sections -ffunction-sections")
  set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -s -fdata-sections -ffunction-sections")
endif()

add_compile_definitions(
  "FMT_HEADER_ONLY"
)

find_package(CLI11 CONFIG REQUIRED)

# Add the executable
add_executable(cli src/main.cc)

target_link_libraries(cli PRIVATE CLI11::CLI11)
add_compile_definitions(MILLENNIUM_VERSION="${MILLENNIUM_VERSION}")

set_target_properties(cli PROPERTIES OUTPUT_NAME "millennium")
set_target_properties(cli PROPERTIES PREFIX "")

install(TARGETS cli DESTINATION bin)
