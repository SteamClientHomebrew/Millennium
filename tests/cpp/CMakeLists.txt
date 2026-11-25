set(TEST_SOURCES
  ffi_recorder_test.cc
  test_encoding.cc
  test_target_url.cc
  test_sdk_ready.cc
  test_theme_class_remapper.cc
  ${CMAKE_SOURCE_DIR}/src/mep/ffi_recorder.cc
  ${CMAKE_SOURCE_DIR}/src/engine/target_url.cc
  ${CMAKE_SOURCE_DIR}/src/mep/sdk_ready_bus.cc
  ${CMAKE_SOURCE_DIR}/src/engine/theme_class_remapper.cc
)

add_executable(millennium_cpp_tests ${TEST_SOURCES})
target_compile_features(millennium_cpp_tests PRIVATE cxx_std_23)

target_include_directories(millennium_cpp_tests PRIVATE
  ${CMAKE_SOURCE_DIR}/src
  ${CMAKE_SOURCE_DIR}/src/include
)

target_link_libraries(millennium_cpp_tests PRIVATE Catch2::Catch2WithMain libcurl nlohmann_json::nlohmann_json)

include(Catch)
catch_discover_tests(millennium_cpp_tests)

add_subdirectory(cdp_shutdown)

if(UNIX)
  add_subdirectory(posix_lifecycle)
  add_subdirectory(logger_concurrency)
  add_subdirectory(child_shutdown)
endif()

if(CMAKE_SYSTEM_NAME STREQUAL "Linux" AND (MILLENNIUM_LINUX_X64 OR MILLENNIUM_NIX_64BIT))
  add_subdirectory(linux_rt3)
endif()

if(UNIX AND MILLENNIUM_BUILD_MAIN)
  add_executable(millennium_cdp_tests test_cdp_shutdown.cc)
  target_compile_features(millennium_cdp_tests PRIVATE cxx_std_23)
  target_include_directories(millennium_cdp_tests PRIVATE ${CMAKE_SOURCE_DIR}/src/include)
  target_link_libraries(millennium_cdp_tests PRIVATE Millennium Catch2::Catch2WithMain nlohmann_json::nlohmann_json)
  add_test(NAME millennium_cdp_tests COMMAND millennium_cdp_tests)
  set_tests_properties(millennium_cdp_tests PROPERTIES TIMEOUT 15)
endif()

if(APPLE AND MILLENNIUM_BUILD_MAIN)
  add_executable(millennium_macos_connector_tests test_macos_cdp_connector.cc)
  target_compile_features(millennium_macos_connector_tests PRIVATE cxx_std_23)
  target_include_directories(millennium_macos_connector_tests PRIVATE ${CMAKE_SOURCE_DIR}/src/include)
  target_link_libraries(millennium_macos_connector_tests PRIVATE Millennium Catch2::Catch2WithMain nlohmann_json::nlohmann_json)
  # Each connector test owns the process-wide socket paths and termination state.
  foreach(test_name IN ITEMS
      "macOS connector exits without a Helper"
      "macOS CDP can connect and stop with a full loopback socket"
      "macOS CDP accepts a new Helper after the previous one exits")
    add_test(NAME "${test_name}" COMMAND millennium_macos_connector_tests "${test_name}")
    set_tests_properties("${test_name}" PROPERTIES TIMEOUT 15)
  endforeach()
endif()

add_executable(sdk_health sdk_health.cc)
target_compile_features(sdk_health PRIVATE cxx_std_23)
target_link_libraries(sdk_health PRIVATE nlohmann_json::nlohmann_json)
