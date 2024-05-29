cmake_minimum_required(VERSION 3.5.0)
project(Millennium LANGUAGES CXX)

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -lunistring -liconv -lbrotlicommon -lcurl -lssh2 -lssl -lcrypto -lz -static-libgcc -static-libstdc++ -lstdc++ -lpthread")

set(BUILD_SHARED_LIBS OFF)
add_subdirectory(vendor/boxer)

# Ensure zlib paths are correctly set
# set(ZLIB_ROOT vendor/minizip-ng/third_party/zlib)
# add_subdirectory(${ZLIB_ROOT})

# Include directories
# add_subdirectory(vendor/minizip-ng)
# include_directories(${ZLIB_ROOT})

if(CMAKE_COMPILER_IS_GNUCC OR CMAKE_COMPILER_IS_GNUCXX)
  add_compile_options(-Wno-format-security) 
endif()


include(FetchContent)
FetchContent_Declare(cpr GIT_REPOSITORY https://github.com/libcpr/cpr.git GIT_TAG 3b15fa82ea74739b574d705fea44959b58142eb8)
FetchContent_MakeAvailable(cpr)

# add_compile_options(-g -Og -lz)
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
  # CURL_STATICLIB 
  FMT_HEADER_ONLY
  _WEBSOCKETPP_CPP11_THREAD_
  ASIO_HAS_STD_INVOKE_RESULT
  _WEBSOCKETPP_CPP11_TYPE_TRAITS_
  _WEBSOCKETPP_CPP11_RANDOM_DEVICE_
  ASIO_STANDALONE
  _CRT_SECURE_NO_WARNINGS
)

# find_package(minizip REQUIRED)
find_package(Python COMPONENTS Development)
find_package(Threads REQUIRED)

if (Python_FOUND)
  message(STATUS "PYTHON  include directory: ${Python_INCLUDE_DIRS}")
  message(STATUS "PYTHON  libraries: ${Python_LIBRARIES}")
else()
  message("Python was not found. download & install here: https://www.python.org/ftp/python/3.11.8/python-3.11.8.exe")
endif()

message(STATUS "LIBGIT  include directory: ${LIBGIT2_INCLUDE_DIRS}")
message(STATUS "LIBGIT  libraries: ${LIBGIT2_LIBRARIES}")

include_directories(
  ${Python_INCLUDE_DIRS}
  ${LIBGIT2_INCLUDE_DIRS}
  "src/__builtins__"
)

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

set_target_properties(Millennium PROPERTIES OUTPUT_NAME "user32")
set_target_properties(Millennium PROPERTIES PREFIX "")

if(WIN32)
  # link windows defined socket headers
  target_link_libraries(Millennium 
    Ws2_32.lib
    wsock32
    ${CMAKE_SOURCE_DIR}/vendor/win32/python311.lib
    ${CMAKE_SOURCE_DIR}/vendor/win32/git2.lib
  )
elseif(UNIX)
  target_link_libraries(Millennium 
    ${Python_LIBRARIES}
    git2
  )
endif()

target_link_libraries(Millennium
  Boxer
  cpr::cpr
  Threads::Threads
)