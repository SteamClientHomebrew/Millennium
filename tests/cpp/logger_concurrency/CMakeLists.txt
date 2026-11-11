cmake_minimum_required(VERSION 3.21)

if(CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
  project(MillenniumLoggerConcurrencyTest LANGUAGES CXX)
  enable_testing()
endif()

if(NOT UNIX)
  return()
endif()

get_filename_component(_millennium_root "${CMAKE_CURRENT_SOURCE_DIR}/../../.." ABSOLUTE)
find_package(Threads REQUIRED)
add_executable(millennium_logger_concurrency_test
  logger_concurrency_test.cc
  "${_millennium_root}/src/system/logger.cc"
)
target_compile_features(millennium_logger_concurrency_test PRIVATE cxx_std_23)
target_compile_definitions(millennium_logger_concurrency_test PRIVATE GIT_COMMIT_HASH="test" MILLENNIUM_VERSION="test")
target_include_directories(millennium_logger_concurrency_test PRIVATE "${_millennium_root}/src/include")
target_link_libraries(millennium_logger_concurrency_test PRIVATE Threads::Threads)

foreach(_mode buffer registry listeners)
  file(MAKE_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/logs/${_mode}")
  add_test(NAME logger_concurrency_${_mode} COMMAND millennium_logger_concurrency_test ${_mode})
  set_tests_properties(logger_concurrency_${_mode} PROPERTIES
    TIMEOUT 15
    WORKING_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/logs/${_mode}"
  )
endforeach()
