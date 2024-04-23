# Project definition
cmake_minimum_required(VERSION 3.5)
project(Boxer-Examples VERSION 1.0.0 LANGUAGES CXX)

# Executable definition and features
add_executable(${PROJECT_NAME} "example.cpp")
target_link_libraries(${PROJECT_NAME} PRIVATE Boxer)
if (WIN32)
   target_compile_definitions(Boxer PUBLIC UNICODE)
endif (WIN32)
