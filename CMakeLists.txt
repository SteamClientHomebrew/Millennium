cmake_minimum_required(VERSION 3.5.0)

# build boxer statically 
set(BUILD_SHARED_LIBS OFF)
cmake_policy(SET CMP0091 NEW)

set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
project(Millennium LANGUAGES CXX)

if (WIN32)
    # debug output paths
    set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "C:/Program Files (x86)/Steam")
    set(LIBRARY_OUTPUT_DIRECTORY "C:/Program Files (x86)/Steam")
endif()

set(PRIVATE_PRODUCT_NAME "Millennium [Steam Homebrew]")

# Set version information
add_compile_definitions(
    VER_COMPANYNAME_STR="Steam Homebrew."
    VER_PRIVATEBUILD=1.0.1
    VER_PRERELEASE=1.0.1
    VER_DEBUG=1.0.1
    VER_FILEVERSION_STR="2.1.6"
    VER_FILEVERSION=2,1,6
    VER_PRODUCTVERSION_STR="2.1.6"
    VER_PRODUCTVERSION=2,1,6
    VER_FILEDESCRIPTION_STR="A plugin loader for the modern Steam Client."
    VER_INTERNALNAME_STR="${PRIVATE_PRODUCT_NAME}"
    VER_LEGALCOPYRIGHT_STR="Copyright \(c\) Steam Homebrew."
    VER_LEGALTRADEMARKS1_STR="Copyright \(c\) Steam Homebrew."
    VER_LEGALTRADEMARKS2_STR="Copyright \(c\) Steam Homebrew."
    VER_ORIGINALFILENAME_STR="${PRIVATE_PRODUCT_NAME}"
    VER_PRODUCTNAME_STR="${PRIVATE_PRODUCT_NAME}"
)

# Compile as release and compile MSVC as MT (static)
set(CMAKE_BUILD_TYPE Release)
if (WIN32)
  set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} /MT")
  set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} /MTd")
elseif(UNIX)
  set(CMAKE_CXX_FLAGS "-fpermissive")
endif()

# build boxer (message-box lib)
add_subdirectory(vendor/boxer)

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
  CURL_STATICLIB # static lcurl

  # prevent websocketpp from using boost
  _WEBSOCKETPP_CPP11_THREAD_
  _WEBSOCKETPP_CPP11_TYPE_TRAITS_
  _WEBSOCKETPP_CPP11_RANDOM_DEVICE_

  ASIO_STANDALONE # static asio
  ASIO_HAS_STD_INVOKE_RESULT # prevent use of boost

  FMT_HEADER_ONLY # implicit static link
  _CRT_SECURE_NO_WARNINGS
)

find_package(cpr CONFIG REQUIRED) # used for web requests. 
find_package(Python REQUIRED COMPONENTS Interpreter Development) # used to run plugin backends
if (WIN32)
	find_package(unofficial-git2 CONFIG REQUIRED) # update millennium modules
endif()

include_directories(
  # ${Python_INCLUDE_DIRS}
  "C:/Program Files (x86)/Python311-32/include"
  ${LIBGIT2_INCLUDE_DIRS}
  "src/__builtins__"
)

set(SOURCE_FILES 
  "src/core/main.cpp"
  "src/core/loader.cpp"
  "src/core/py_controller/co_spawn.cpp"
  "src/core/ffi/c_python.cpp"
  "src/core/ffi/javascript.cpp"
  "src/core/ffi/gil.cc"
  "src/utilities/log.cpp"
  "src/core/ipc/pipe.cpp"
  "src/generic/stream_parser.cc"
  "src/__builtins__/executor.cc"
  "src/deps/module.cc"
  "src/core/hooks/web_load.cc"
  "src/core/co_initialize/co_stub.cc"
  version.rc
)

if (WIN32)
  add_library(Millennium SHARED "${SOURCE_FILES}")
  # add_executable(Millennium "${SOURCE_FILES}")
elseif(UNIX)
  add_executable(Millennium "${SOURCE_FILES}")
endif()

set_target_properties(Millennium PROPERTIES OUTPUT_NAME "user32")
set_target_properties(Millennium PROPERTIES PREFIX "")
set_target_properties(Millennium PROPERTIES NO_EXPORT TRUE)

if(MSVC)
    # prevent MSVC from generating .lib and .exp archives
    set_target_properties(Millennium PROPERTIES ARCHIVE_OUTPUT_NAME "" LINK_FLAGS "/NOEXP")
endif()

if(WIN32)
  # link windows defined socket headers
  target_link_libraries(Millennium
    Ws2_32.lib
    wsock32
    Boxer
    cpr::cpr
    # ${Python_LIBRARIES}
    # "D:/Development/millennium/vendor/win32/python311.lib"
    # "C:/Users/Admin/Downloads/Python/Python-3.11.8/PCbuild/win32/python311.lib"
    "C:/Users/Admin/Downloads/Python-3.11.8/Python-3.11.8/PCbuild/win32/python311.lib"
    unofficial::git2::git2
  )
elseif(UNIX)
  target_link_libraries(Millennium
      Boxer
      cpr::cpr
      git2
      Python::Python
  )
endif()
