cmake_minimum_required(VERSION 3.21)

if(CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
  project(MillenniumChildShutdownTest LANGUAGES CXX)
  enable_testing()
  find_package(nlohmann_json CONFIG REQUIRED)
endif()

if(NOT UNIX)
  return()
endif()

get_filename_component(_millennium_root "${CMAKE_CURRENT_SOURCE_DIR}/../../.." ABSOLUTE)
find_package(Threads REQUIRED)
add_executable(millennium_child_shutdown_test
  child_shutdown_test.cc
  "${_millennium_root}/src/engine/child_process.cc"
  "${_millennium_root}/src/mep/crash_event_bus.cc"
)
target_compile_features(millennium_child_shutdown_test PRIVATE cxx_std_23)
# Only replace logging; use the real PluginProcess, IPC framing and child reaping.
target_include_directories(millennium_child_shutdown_test BEFORE PRIVATE
  "${CMAKE_CURRENT_SOURCE_DIR}/stubs"
  "${_millennium_root}/src/include"
)
target_link_libraries(millennium_child_shutdown_test PRIVATE Threads::Threads nlohmann_json::nlohmann_json)

foreach(_mode zombie reaped sigterm stubborn)
  add_test(NAME child_shutdown_${_mode} COMMAND millennium_child_shutdown_test ${_mode})
  set_tests_properties(child_shutdown_${_mode} PROPERTIES TIMEOUT 15)
endforeach()
