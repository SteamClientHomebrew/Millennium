cmake_minimum_required(VERSION 3.14)
project(Millennium LANGUAGES CXX)

add_subdirectory(vendor/boxer)

if(CMAKE_COMPILER_IS_GNUCC OR CMAKE_COMPILER_IS_GNUCXX)
  add_compile_options(-Wno-format-security) 
endif()

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -g -lz -std=c++17")

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_C_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_C_STANDARD_REQUIRED ON)

include_directories(
  ${CMAKE_SOURCE_DIR}/src
  ${CMAKE_SOURCE_DIR}/vendor/fmt/include
  ${CMAKE_SOURCE_DIR}/vendor/asio/asio/include
  ${CMAKE_SOURCE_DIR}/vendor/nlohmann/include
  ${CMAKE_SOURCE_DIR}/vendor/fmt/include
  ${CMAKE_SOURCE_DIR}/vendor/websocketpp
  ${CMAKE_SOURCE_DIR}/vendor/http
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

find_package(CURL REQUIRED)
message(STATUS "CURL include directory: ${CURL_INCLUDE_DIR}")
message(STATUS "CURL libraries: ${CURL_LIBRARIES}")

# include python and libcurl statically.
include_directories(
  "C:\\Program Files (x86)\\Python311-32\\include"
  ${CURL_INCLUDE_DIR}
  ${LIBXML2_INCLUDE_DIR}
  "C:/msys64/usr/include"
)

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
  "src/deps/python.cc"
  "src/core/hooks/web_load.cc"
)

# Set debug flags
# target_compile_options(Millennium PRIVATE -g -O0)

set_target_properties(Millennium PROPERTIES OUTPUT_NAME "User32")
set_target_properties(Millennium PROPERTIES PREFIX "")
set_target_properties(Millennium PROPERTIES LINK_FLAGS "-static-libstdc++")

target_link_libraries(Millennium
  "Ws2_32.lib" "wsock32"
  # ${Python_LIBRARIES}
  "C:/Users/Admin/Downloads/Python-3.11.8/Python-3.11.8/PCbuild/win32/python311.lib"
  "C:/msys64/mingw32/lib/libminizip.dll.a"
  ${CURL_LIBRARIES} # link curl
  ${LIBXML2_LIBRARIES}
  "C:/msys64/usr/lib/libmyhtml_static.a"
  Boxer
) 