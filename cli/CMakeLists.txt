cmake_minimum_required(VERSION 3.5.0)
project(CLI)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Optimize for size
# set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} -Os -ffunction-sections -fdata-sections -fno-rtti")
# set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -s -static -Wl,--gc-sections")
# set(CMAKE_INTERPROCEDURAL_OPTIMIZATION TRUE)

set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -m32")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -m32")

include_directories(
  ${CMAKE_SOURCE_DIR}/vendor/ini/src
  ${CMAKE_SOURCE_DIR}/vendor/nlohmann/include
  ${CMAKE_SOURCE_DIR}/vendor/fmt/include
  ${CMAKE_SOURCE_DIR}/cli
  ${CMAKE_SOURCE_DIR}/src
)
find_package(CURL REQUIRED)

if (WIN32 AND NOT GITHUB_ACTION_BUILD)
  set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "C:/Program Files (x86)/Steam/ext/bin")
elseif(UNIX AND NOT GITHUB_ACTION_BUILD)
  set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "$ENV{HOME}/.millennium/ext/bin")
endif()

add_compile_definitions(MILLENNIUM_CLI FMT_HEADER_ONLY)

# Add your source files here
set(SOURCES 
  main.cc
  ${CMAKE_SOURCE_DIR}/src/sys/settings.cc
  ${CMAKE_SOURCE_DIR}/src/sys/io.cc
  ${CMAKE_SOURCE_DIR}/src/sys/log.cc
)
find_package(CLI11 CONFIG REQUIRED)

add_executable(CLI ${SOURCES})
set_target_properties(CLI PROPERTIES COMPILE_FLAGS "-m32" LINK_FLAGS "-m32")
target_compile_options (CLI PRIVATE -m32 )

find_program(WINDRES windres)
if (WINDRES)
    add_custom_command(
        OUTPUT ${CMAKE_BINARY_DIR}/version-cli.o
        COMMAND ${WINDRES} -i ${CMAKE_SOURCE_DIR}/scripts/cli.version.rc -o ${CMAKE_BINARY_DIR}/version-cli.o
        DEPENDS ${CMAKE_SOURCE_DIR}/scripts/cli.version.rc
    )

    add_custom_target(resource-cli DEPENDS ${CMAKE_BINARY_DIR}/version-cli.o)
    add_dependencies(CLI resource-cli)
    
    target_link_libraries(CLI PRIVATE ${CMAKE_BINARY_DIR}/version-cli.o)
endif()

install(TARGETS CLI DESTINATION /usr/local/bin)

if(CMAKE_COMPILER_IS_GNUCC OR CMAKE_COMPILER_IS_GNUCXX)

  # set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Os -ffunction-sections -fdata-sections")
  # set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} Wl,--gc-sections -s")
endif()

target_link_libraries(CLI PRIVATE CLI11::CLI11 CURL::libcurl)
set_target_properties(CLI PROPERTIES OUTPUT_NAME "millennium")