cmake_minimum_required(VERSION 3.14)
project(Millennium LANGUAGES CXX)

add_subdirectory(vendor/boxer)

if(CMAKE_COMPILER_IS_GNUCC OR CMAKE_COMPILER_IS_GNUCXX)
  add_compile_options(-Wno-format-security) 
endif()

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -g -Og -lz -std=c++17")

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_C_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_C_STANDARD_REQUIRED ON)

include_directories(
  ${CMAKE_SOURCE_DIR}/src
  ${CMAKE_SOURCE_DIR}/vendor/fmt/include
  ${CMAKE_SOURCE_DIR}/vendor/asio/asio/include
  ${CMAKE_SOURCE_DIR}/vendor/nlohmann/include
  ${CMAKE_SOURCE_DIR}/vendor/websocketpp
  ${CMAKE_SOURCE_DIR}/vendor/incbin
)

add_compile_definitions(
  CURL_STATICLIB 
  FMT_HEADER_ONLY
  _WEBSOCKETPP_CPP11_THREAD_
  ASIO_HAS_STD_INVOKE_RESULT
  _WEBSOCKETPP_CPP11_TYPE_TRAITS_
  _WEBSOCKETPP_CPP11_RANDOM_DEVICE_
  ASIO_STANDALONE
  _CRT_SECURE_NO_WARNINGS
)

# if cmake can't find python, set the path manually. 
# set(Python_ROOT_DIR "C:/Program Files (x86)/Python311-32")
# set(Python_FIND_STRATEGY LOCATION)
find_package(Python COMPONENTS Development)

if (Python_FOUND)
  message(STATUS "PYTHON  include directory: ${Python_INCLUDE_DIRS}")
  message(STATUS "PYTHON  libraries: ${Python_LIBRARIES}")
else()
  message("Python was not found. download & install here: https://www.python.org/ftp/python/3.11.8/python-3.11.8.exe")
endif()

find_package(PkgConfig REQUIRED)
pkg_search_module(LIBGIT2 REQUIRED libgit2)

message(STATUS "LIBGIT  include directory: ${LIBGIT2_INCLUDE_DIRS}")
message(STATUS "LIBGIT  libraries: ${LIBGIT2_LIBRARIES}")

find_package(CURL REQUIRED)
message(STATUS "LIBCURL include directory: ${CURL_INCLUDE_DIR}")
message(STATUS "LIBCURL libraries: ${CURL_LIBRARIES}")

# include python libcurl statically.
include_directories(
  ${Python_INCLUDE_DIRS}
  ${CURL_INCLUDE_DIR}
  ${LIBGIT2_INCLUDE_DIRS}
  "src/__builtins__"
)

# target_include_directories(Millennium PRIVATE "src/__builtins__")

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "C:/Program Files (x86)/Steam")
set(CMAKE_BUILD_TYPE Debug)

add_library(Millennium SHARED
  "src/core/main.cpp"
  "src/core/loader.cpp"
  "src/core/backend/co_spawn.cpp"
  "src/core/impl/c_python.cpp"
  "src/core/impl/javascript.cpp"
  "src/utilities/log.cpp"
  "src/core/ipc/pipe.cpp"
  "src/generic/stream_parser.cc"
  "src/__builtins__/executor.cc"
  "src/deps/module.cc"
  "src/core/hooks/web_load.cc"
  "src/core/co_initialize/co_stub.cc"
)

# Set debug flags
# target_compile_options(Millennium PRIVATE -g -O0)

set_target_properties(Millennium PROPERTIES OUTPUT_NAME "User32")
set_target_properties(Millennium PROPERTIES PREFIX "")
# set_target_properties(Millennium PROPERTIES LINK_FLAGS "-static-libstdc++")

if(WIN32)
  # link windows defined socket headers
  target_link_libraries(Millennium "Ws2_32.lib" "wsock32")
endif()

target_link_libraries(Millennium
  # ${Python_LIBRARIES}
  "C:/Users/Admin/Downloads/Python-3.11.8/Python-3.11.8/PCbuild/win32/python311.lib"
  ${CURL_LIBRARIES}
  ${LIBGIT2_LIBRARIES}
  Boxer
) 