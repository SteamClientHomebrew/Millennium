# Project definition
cmake_minimum_required(VERSION 3.5)
project(Boxer VERSION 1.0.0 LANGUAGES CXX)

set(LINUX FALSE)
if (${CMAKE_SYSTEM_NAME} MATCHES "Linux")
   set(LINUX TRUE)
endif()

# Source files
set(SRC_DIR "${CMAKE_CURRENT_SOURCE_DIR}/src")
set(INC_DIR "${CMAKE_CURRENT_SOURCE_DIR}/include")
set(SOURCES "${INC_DIR}/boxer/boxer.h")
if (APPLE)
   list(APPEND SOURCES
      "${SRC_DIR}/boxer_mac.mm"
   )
elseif (WIN32)
   list(APPEND SOURCES
      "${SRC_DIR}/boxer_win.cpp"
   )
elseif (LINUX)
   list(APPEND SOURCES
      "${SRC_DIR}/boxer_linux.cpp"
   )
endif ()

# Library definition and features
add_library(${PROJECT_NAME} ${SOURCES})
target_include_directories(${PROJECT_NAME} PUBLIC "${INC_DIR}")
target_compile_features(${PROJECT_NAME} PUBLIC cxx_strong_enums cxx_nullptr)

# Platform-specific dependencies
if (APPLE)
   find_library(COCOA_LIBRARY Cocoa)
   target_link_libraries(${PROJECT_NAME} PUBLIC ${COCOA_LIBRARY})
elseif (LINUX)
   find_package(PkgConfig REQUIRED)
   pkg_check_modules(GTK3 REQUIRED gtk+-3.0)
   target_link_libraries(${PROJECT_NAME} PUBLIC ${GTK3_LIBRARIES})
   target_include_directories(${PROJECT_NAME} PRIVATE ${GTK3_INCLUDE_DIRS})
endif ()

# Symbol exporting
if (BUILD_SHARED_LIBS)
   target_compile_definitions(${PROJECT_NAME} PRIVATE "BOXER_BUILD_DLL")
   target_compile_definitions(${PROJECT_NAME} INTERFACE "BOXER_DLL")
endif ()

# Build options
option(BOXER_BUILD_EXAMPLES "Build example programs" OFF)
if (BOXER_BUILD_EXAMPLES)
   add_subdirectory("examples")
endif()
