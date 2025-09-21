cmake_minimum_required(VERSION 3.10)

if(WIN32 AND CMAKE_BUILD_TYPE STREQUAL "Release")
  set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -m32 -fvisibility=hidden -Oz -ffunction-sections -fdata-sections -fno-stack-protector -fno-unwind-tables -fno-asynchronous-unwind-tables -fmerge-all-constants")
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -m32 -fvisibility=hidden -Oz -ffunction-sections -fdata-sections -fno-stack-protector -fno-unwind-tables -fno-asynchronous-unwind-tables -fmerge-all-constants")
  set(CMAKE_WINDOWS_EXPORT_ALL_SYMBOLS OFF)
  
  add_compile_definitions(WIN32_LEAN_AND_MEAN)
  add_compile_definitions(NOMINMAX)
endif()

project(preload)

set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED True)

set(CMAKE_C_FLAGS_RELEASE "${CMAKE_C_FLAGS_RELEASE} -static-libgcc")
set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} -static-libgcc -static-libstdc++")

if(WIN32 AND NOT GITHUB_ACTION_BUILD)
  set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "C:/Program Files (x86)/Steam")
  set(LIBRARY_OUTPUT_DIRECTORY "C:/Program Files (x86)/Steam")
elseif(UNIX AND NOT GITHUB_ACTION_BUILD)
  set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "$ENV{HOME}/.millennium/")
  set(CMAKE_LIBRARY_OUTPUT_DIRECTORY "$ENV{HOME}/.millennium/")
endif()

add_library(preload SHARED main.c exports.def)
target_link_libraries(preload ${CMAKE_BINARY_DIR}/version.o)

set_target_properties(preload PROPERTIES 
  OUTPUT_NAME "version"
  PREFIX ""
  NO_EXPORT TRUE
  LINK_FLAGS_RELEASE "-s -Wl,--gc-sections -Wl,--strip-all -Wl,--no-seh"
)