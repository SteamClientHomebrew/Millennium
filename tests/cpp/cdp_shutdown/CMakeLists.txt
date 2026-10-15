cmake_minimum_required(VERSION 3.21)

if(CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
  project(MillenniumCdpShutdownTest LANGUAGES CXX)
  enable_testing()
  find_package(nlohmann_json CONFIG REQUIRED)
endif()

get_filename_component(_millennium_root "${CMAKE_CURRENT_SOURCE_DIR}/../../.." ABSOLUTE)
find_package(Threads REQUIRED)

add_executable(millennium_cdp_shutdown_test
  cdp_shutdown_test.cc
  "${_millennium_root}/src/engine/cdp_api.cc"
  "${_millennium_root}/src/engine/thread_pool.cc"
)
target_compile_features(millennium_cdp_shutdown_test PRIVATE cxx_std_20)
# Exercise the real CDP client and callback workers without starting Steam.
target_include_directories(millennium_cdp_shutdown_test BEFORE PRIVATE
  "${CMAKE_CURRENT_SOURCE_DIR}/stubs"
  "${_millennium_root}/src/include"
)
target_link_libraries(millennium_cdp_shutdown_test PRIVATE Threads::Threads nlohmann_json::nlohmann_json)

foreach(_mode callback pending concurrent)
  add_test(NAME cdp_shutdown_${_mode} COMMAND millennium_cdp_shutdown_test ${_mode})
  set_tests_properties(cdp_shutdown_${_mode} PROPERTIES TIMEOUT 10)
endforeach()
