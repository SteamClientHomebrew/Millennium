cmake_minimum_required(VERSION 3.21)

# Also allow this regression test to run without building Millennium or its dependencies.
if(CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
  project(MillenniumPosixLifecycleTest LANGUAGES CXX)
  enable_testing()
endif()

if(NOT UNIX)
  return()
endif()

get_filename_component(_millennium_root "${CMAKE_CURRENT_SOURCE_DIR}/../../.." ABSOLUTE)
find_package(Threads REQUIRED)

add_executable(millennium_posix_lifecycle_test
  posix_lifecycle_test.cc
  "${_millennium_root}/src/platform/posix.cc"
  "${_millennium_root}/src/mep/crash_event_bus.cc"
  "${_millennium_root}/src/mep/sdk_ready_bus.cc"
  "${_millennium_root}/src/mep/ffi_recorder.cc"
  "${_millennium_root}/src/mep/patch_stream_recorder.cc"
)
target_compile_features(millennium_posix_lifecycle_test PRIVATE cxx_std_20)
# Stub only Steam-facing dependencies; exercise the real POSIX entry points and lifecycle.
target_include_directories(millennium_posix_lifecycle_test BEFORE PRIVATE
  "${CMAKE_CURRENT_SOURCE_DIR}/stubs"
  "${_millennium_root}/src/include"
)
target_link_libraries(millennium_posix_lifecycle_test PRIVATE Threads::Threads)

foreach(_mode return exit explicit-stop)
  add_test(NAME posix_lifecycle_${_mode} COMMAND millennium_posix_lifecycle_test ${_mode})
  set_tests_properties(posix_lifecycle_${_mode} PROPERTIES TIMEOUT 10)
endforeach()
