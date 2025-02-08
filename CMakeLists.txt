# get the millennium version from version file
file(STRINGS "${CMAKE_SOURCE_DIR}/version" VERSION_LINES LIMIT_COUNT 2)
list(GET VERSION_LINES 1 MILLENNIUM_VERSION)
set(MILLENNIUM_VERSION "${MILLENNIUM_VERSION}")

message(STATUS "Millennium Version: ${MILLENNIUM_VERSION}")

cmake_minimum_required(VERSION 3.5.0)
set(BUILD_SHARED_LIBS OFF)

# set c++ directives
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_C_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_C_STANDARD_REQUIRED ON)

if(NOT APPLE)
  # set 32-bit build
  set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS}   -m32")
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -m32")
endif()

# Strip binary on release builds
if(CMAKE_BUILD_TYPE STREQUAL "Release")
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fvisibility=hidden -s")
  set(CMAKE_WINDOWS_EXPORT_ALL_SYMBOLS OFF)
endif()

project(Millennium LANGUAGES CXX)

if(WIN32 AND NOT GITHUB_ACTION_BUILD)
  execute_process(
    COMMAND reg query "HKCU\\Software\\Valve\\Steam" /v "SteamPath"
    RESULT_VARIABLE result
    OUTPUT_VARIABLE steam_path
    ERROR_VARIABLE reg_error
  )

  if(result EQUAL 0)
    string(REGEX MATCH "[a-zA-Z]:/[^ ]+([ ]+[^ ]+)*" extracted_path "${steam_path}")
    string(REPLACE "\n" "" extracted_path "${extracted_path}")

    message(STATUS "Build Steam Path: ${extracted_path}")

    set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${extracted_path})
    set(LIBRARY_OUTPUT_DIRECTORY ${extracted_path})
  else()
    message(WARNING "Failed to read Steam installation path from HKCU\\Software\\Valve\\Steam.")
  endif()

elseif(UNIX AND NOT GITHUB_ACTION_BUILD)
  set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "/usr/lib/millennium/")
  set(CMAKE_LIBRARY_OUTPUT_DIRECTORY "/usr/lib/millennium/")
endif()

# Set version information
add_compile_definitions(MILLENNIUM_VERSION="${MILLENNIUM_VERSION}")

if(UNIX)
  set(CMAKE_CXX_FLAGS "-fpermissive")
endif()

include_directories(
  ${CMAKE_SOURCE_DIR}/src
  ${CMAKE_SOURCE_DIR}/include
  ${CMAKE_SOURCE_DIR}/vendor/fmt/include
  ${CMAKE_SOURCE_DIR}/vendor/asio/asio/include
  ${CMAKE_SOURCE_DIR}/vendor/nlohmann/include
  ${CMAKE_SOURCE_DIR}/vendor/websocketpp
  ${CMAKE_SOURCE_DIR}/vendor/crow/include
  ${CMAKE_SOURCE_DIR}/vendor/ini/src
)

add_compile_definitions(
  "CURL_STATICLIB"
  "_WEBSOCKETPP_CPP11_THREAD_"
  "_WEBSOCKETPP_CPP11_TYPE_TRAITS_"
  "_WEBSOCKETPP_CPP11_RANDOM_DEVICE_"
  "ASIO_STANDALONE"
  "ASIO_HAS_STD_INVOKE_RESULT"
  "FMT_HEADER_ONLY"
  "_CRT_SECURE_NO_WARNINGS"
)

if(WIN32)
  add_subdirectory(preload)
endif()

# Include Python headers
if(WIN32)
  include_directories(${CMAKE_SOURCE_DIR}/vendor/python/include)
elseif(UNIX)
  if(APPLE)
    include_directories("$ENV{HOME}/.pyenv/versions/3.11.8/include/python3.11")
  else()
    include_directories("$ENV{HOME}/Documents/LibPython/")
  endif()
endif()

set(SOURCE_FILES
  "src/main.cc"
  "src/core/loader.cc"
  "src/core/co_spawn.cc"
  "src/core/_c_py_logger.cc"
  "src/core/_c_py_interop.cc"
  "src/core/_c_py_gil.cc"
  "src/core/_c_py_api.cc"
  "src/core/_js_interop.cc"
  "src/core/co_stub.cc"
  "src/core/events.cc"
  "src/core/web_load.cc"
  "src/core/ipc_pipe.cc"
  "src/core/ftp_serv.cc"
  "src/sys/log.cc"
  "src/sys/sysfs.cc"
  "src/sys/settings.cc"
)

if(WIN32)
  add_library(Millennium SHARED "${SOURCE_FILES}")
elseif(UNIX)
  # add_executable(Millennium "${SOURCE_FILES}")
  # add_compile_definitions(MILLENNIUM_EXECUTABLE)
  add_library(Millennium SHARED "${SOURCE_FILES}")
  add_compile_definitions(MILLENNIUM_SHARED)
endif()

if(NOT APPLE)
  set_target_properties(Millennium PROPERTIES COMPILE_FLAGS "-m32" LINK_FLAGS "-m32")
  target_compile_options(Millennium PRIVATE -m32)
endif()

if(WIN32)
  set_target_properties(Millennium PROPERTIES OUTPUT_NAME "millennium")
  set_target_properties(Millennium PROPERTIES PREFIX "")
  set_target_properties(Millennium PROPERTIES NO_EXPORT TRUE)
endif()

if(MSVC)
  # prevent MSVC from generating .lib and .exp archives
  set_target_properties(Millennium PROPERTIES ARCHIVE_OUTPUT_NAME "" LINK_FLAGS "/NOEXP")
endif()

find_program(WINDRES windres)

if(WINDRES)
  add_custom_command(
    OUTPUT ${CMAKE_BINARY_DIR}/version.o
    COMMAND ${WINDRES} -i ${CMAKE_SOURCE_DIR}/scripts/version.rc -o ${CMAKE_BINARY_DIR}/version.o
    DEPENDS ${CMAKE_SOURCE_DIR}/scripts/version.rc
  )

  add_custom_target(resource DEPENDS ${CMAKE_BINARY_DIR}/version.o)
  add_dependencies(Millennium resource)
  target_link_libraries(Millennium ${CMAKE_BINARY_DIR}/version.o)
endif()

find_package(CURL REQUIRED) # used for web requests.
target_link_libraries(Millennium CURL::libcurl)

if(WIN32)
  target_link_libraries(Millennium wsock32 Iphlpapi)

  if(GITHUB_ACTION_BUILD)
    target_link_libraries(Millennium "D:/a/Millennium/Millennium/Python-3.11.8/PCbuild/win32/python311.lib")
  else()
    target_link_libraries(Millennium ${CMAKE_SOURCE_DIR}/vendor/python/python311.lib ${CMAKE_SOURCE_DIR}/vendor/python/python311_d.lib)
  endif()

elseif(UNIX)
  if(APPLE)
    target_link_libraries(Millennium "$ENV{HOME}/.pyenv/versions/3.11.8/lib/libpython3.11.dylib")
  else()
    if(GITHUB_ACTION_BUILD)
      target_link_libraries(Millennium "$ENV{HOME}/.millennium/libpython-3.11.8.so")
    else()
      target_link_libraries(Millennium "$ENV{HOME}/Documents/LibPython/libpython-3.11.8.so")
    endif()
  endif()
endif()
