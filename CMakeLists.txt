cmake_minimum_required(VERSION 3.14)
project(Millennium LANGUAGES CXX)
add_subdirectory(vendor/boxer)

if(CMAKE_COMPILER_IS_GNUCC OR CMAKE_COMPILER_IS_GNUCXX)
  add_compile_options(-Wno-format-security) 
endif()

include(FetchContent)
FetchContent_Declare(cpr GIT_REPOSITORY https://github.com/libcpr/cpr.git GIT_TAG 3b15fa82ea74739b574d705fea44959b58142eb8)
FetchContent_MakeAvailable(cpr)

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -static-libgcc -static-libstdc++ -lwinpthread")

add_compile_options(-g -Og -lz)
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

find_package(Python COMPONENTS Development)

if (Python_FOUND)
  message(STATUS "PYTHON  include directory: ${Python_INCLUDE_DIRS}")
  message(STATUS "PYTHON  libraries: ${Python_LIBRARIES}")
else()
  message("Python was not found. download & install here: https://www.python.org/ftp/python/3.11.8/python-3.11.8.exe")
endif()

message(STATUS "LIBGIT  include directory: ${LIBGIT2_INCLUDE_DIRS}")
message(STATUS "LIBGIT  libraries: ${LIBGIT2_LIBRARIES}")

find_package(CURL REQUIRED)
message(STATUS "LIBCURL include directory: ${CURL_INCLUDE_DIR}")
message(STATUS "LIBCURL libraries: ${CURL_LIBRARIES}")

include_directories(
  ${Python_INCLUDE_DIRS}
  ${LIBGIT2_INCLUDE_DIRS}
  "src/__builtins__"
)

set(THREADS_PREFER_PTHREAD_FLAG ON)
find_package(Threads REQUIRED)

set(CMAKE_BUILD_TYPE Debug)
set(SOURCE_FILES 
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
  "src/deps/python.cc"
  "src/core/hooks/web_load.cc"
  "src/core/co_initialize/co_stub.cc"
)

if (WIN32)
  set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "C:/Program Files (x86)/Steam")
  add_library(Millennium SHARED "${SOURCE_FILES}")
  # add_executable(Millennium "${SOURCE_FILES}")
elseif(UNIX)
  add_executable(Millennium "${SOURCE_FILES}")
endif()

set_target_properties(Millennium PROPERTIES OUTPUT_NAME "User32")
set_target_properties(Millennium PROPERTIES PREFIX "")

if(WIN32)
  # link windows defined socket headers
  target_link_libraries(Millennium "Ws2_32.lib" "wsock32"
    "C:/Users/Admin/Downloads/Python-3.11.8/Python-3.11.8/PCbuild/win32/python311.lib"
  )
elseif(UNIX)
  target_link_libraries(Millennium ${Python_LIBRARIES})
endif()

target_link_libraries(Millennium
  "libminizip.dll.a"
  "C:/Users/Admin/Desktop/libgit2-build/libgit2/build/Debug/git2.lib"
  Boxer
  cpr::cpr
  Threads::Threads
) 