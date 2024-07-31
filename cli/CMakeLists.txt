cmake_minimum_required(VERSION 3.5.0)
project(CLI)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include_directories(
  ${CMAKE_SOURCE_DIR}/vendor/ini/src
  ${CMAKE_SOURCE_DIR}/vendor/nlohmann/include
  ${CMAKE_SOURCE_DIR}/vendor/websocketpp
  ${CMAKE_SOURCE_DIR}/vendor/fmt/include
  ${CMAKE_SOURCE_DIR}/vendor/asio/asio/include
  ${CMAKE_SOURCE_DIR}/cli
  ${CMAKE_SOURCE_DIR}/src
)
find_package(CURL REQUIRED)

add_compile_definitions(
  MILLENNIUM_CLI
  FMT_HEADER_ONLY
  _WEBSOCKETPP_CPP11_THREAD_
  _WEBSOCKETPP_CPP11_TYPE_TRAITS_
  _WEBSOCKETPP_CPP11_RANDOM_DEVICE_
  ASIO_STANDALONE
  ASIO_HAS_STD_INVOKE_RESULT
)

# Add your source files here
set(SOURCES 
  main.cc
  ${CMAKE_SOURCE_DIR}/src/sys/settings.cc
  ${CMAKE_SOURCE_DIR}/src/sys/io.cc
  ${CMAKE_SOURCE_DIR}/src/sys/log.cc
)
find_package(CLI11 CONFIG REQUIRED)

add_executable(CLI ${SOURCES})
install(TARGETS CLI DESTINATION /usr/local/bin)

if(CMAKE_COMPILER_IS_GNUCC OR CMAKE_COMPILER_IS_GNUCXX)

  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Os -ffunction-sections -fdata-sections")
  # set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} Wl,--gc-sections -s")
endif()

target_link_libraries(CLI PRIVATE CLI11::CLI11 CURL::libcurl)
set_target_properties(CLI PROPERTIES OUTPUT_NAME "millennium")