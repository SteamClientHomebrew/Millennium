cmake_minimum_required(VERSION 3.21)
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>" CACHE STRING "Static MSVC Runtime Library" FORCE)
set(MILLENNIUM_BASE            ${CMAKE_SOURCE_DIR}                     CACHE PATH   "Base path to Millennium source"   )

project(MillenniumBuildSlave)
add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/src)
add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/src/hhx64)