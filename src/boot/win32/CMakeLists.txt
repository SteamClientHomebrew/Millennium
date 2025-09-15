cmake_minimum_required(VERSION 3.10)

set(BUILD_SHARED_LIBS OFF)

if(WIN32 AND CMAKE_BUILD_TYPE STREQUAL "Release")
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fvisibility=hidden -s")
  set(CMAKE_WINDOWS_EXPORT_ALL_SYMBOLS OFF)
endif()

# Set the project name
project(preload)

include_directories(
  ${CMAKE_CURRENT_SOURCE_DIR}/include
  ${CMAKE_SOURCE_DIR}/include
)

# Specify the C++ standard
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED True)

if(WIN32 AND NOT GITHUB_ACTION_BUILD)
  # debug output paths
  set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "C:/Program Files (x86)/Steam")
  set(LIBRARY_OUTPUT_DIRECTORY "C:/Program Files (x86)/Steam")
elseif(UNIX AND NOT GITHUB_ACTION_BUILD)
  set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "$ENV{HOME}/.millennium/")
  set(CMAKE_LIBRARY_OUTPUT_DIRECTORY "$ENV{HOME}/.millennium/")
endif()

find_package(CURL REQUIRED) # used for web requests.

add_compile_definitions(
  CURL_STATICLIB
)

find_package(MinHook REQUIRED)

# Add the executable
add_library(preload SHARED main.cc exports.def)
target_link_libraries(preload ${CMAKE_BINARY_DIR}/version.o)
target_link_libraries(preload CURL::libcurl minhook::minhook shlwapi)

set_target_properties(preload PROPERTIES OUTPUT_NAME "version")
set_target_properties(preload PROPERTIES PREFIX "")
set_target_properties(preload PROPERTIES NO_EXPORT TRUE)
