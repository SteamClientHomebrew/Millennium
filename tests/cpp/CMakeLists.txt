set(TEST_SOURCES
  ffi_recorder_test.cc
  test_target_url.cc
  test_sdk_ready.cc
  ${CMAKE_SOURCE_DIR}/src/mep/ffi_recorder.cc
  ${CMAKE_SOURCE_DIR}/src/engine/target_url.cc
  ${CMAKE_SOURCE_DIR}/src/mep/sdk_ready_bus.cc
)

add_executable(millennium_cpp_tests ${TEST_SOURCES})
if(APPLE)
  target_sources(millennium_cpp_tests PRIVATE test_macos_cdp_pipe.cc)
endif()
target_compile_features(millennium_cpp_tests PRIVATE cxx_std_23)

target_include_directories(millennium_cpp_tests PRIVATE
  ${CMAKE_SOURCE_DIR}/src
  ${CMAKE_SOURCE_DIR}/src/include
)

target_link_libraries(millennium_cpp_tests PRIVATE Catch2::Catch2WithMain libcurl nlohmann_json::nlohmann_json)

include(Catch)
catch_discover_tests(millennium_cpp_tests)

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
  # Each connector test owns the process-wide broker and termination state.
  foreach(test_name IN ITEMS
      "macOS connector exits without a Helper"
      "macOS failed handoff preserves the current Helper connection")
    add_test(NAME "${test_name}" COMMAND millennium_macos_connector_tests "${test_name}")
    set_tests_properties("${test_name}" PROPERTIES TIMEOUT 15)
  endforeach()
endif()

add_executable(sdk_health sdk_health.cc)
target_compile_features(sdk_health PRIVATE cxx_std_23)
target_link_libraries(sdk_health PRIVATE nlohmann_json::nlohmann_json)
