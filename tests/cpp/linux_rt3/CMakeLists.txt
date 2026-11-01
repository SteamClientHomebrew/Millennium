cmake_minimum_required(VERSION 3.21)

if(CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
    project(MillenniumLinuxRt3Tests LANGUAGES C CXX)
    enable_testing()
endif()

get_filename_component(_millennium_root "${CMAKE_CURRENT_SOURCE_DIR}/../../.." ABSOLUTE)
find_package(X11 REQUIRED)

add_library(millennium_rt3_test_payload SHARED payload.c)
add_executable(millennium_rt3_test_client client.c)

add_library(millennium_rt3_test_proxy SHARED "${_millennium_root}/src/bootstrap/linux/libmillennium_bootstrap_x64.c")
target_include_directories(millennium_rt3_test_proxy PRIVATE ${X11_INCLUDE_DIR} ${X11_Xtst_INCLUDE_PATH})
target_link_libraries(millennium_rt3_test_proxy PRIVATE ${CMAKE_DL_LIBS})
add_dependencies(millennium_rt3_test_proxy millennium_rt3_test_payload)
target_compile_definitions(millennium_rt3_test_proxy PRIVATE
    MILLENNIUM_X64_PATH="$<TARGET_FILE:millennium_rt3_test_payload>"
    MILLENNIUM_HHX64_PATH="$<TARGET_FILE:millennium_rt3_test_payload>")

set(_rt3_fixtures "${CMAKE_CURRENT_BINARY_DIR}/fixtures")
foreach(_fixture client updater forwarding webhelper)
    file(MAKE_DIRECTORY "${_rt3_fixtures}/${_fixture}")
endforeach()
add_custom_command(TARGET millennium_rt3_test_client POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E make_directory "${_rt3_fixtures}/client/steamrt64" "${_rt3_fixtures}/updater/steamrt64"
    COMMAND ${CMAKE_COMMAND} -E copy "$<TARGET_FILE:millennium_rt3_test_client>" "${_rt3_fixtures}/client/steamrt64/steam"
    COMMAND ${CMAKE_COMMAND} -E copy "$<TARGET_FILE:millennium_rt3_test_client>" "${_rt3_fixtures}/updater/steamrt64/steam"
    COMMAND ${CMAKE_COMMAND} -E copy "$<TARGET_FILE:millennium_rt3_test_client>" "${_rt3_fixtures}/forwarding/other"
    COMMAND ${CMAKE_COMMAND} -E copy "$<TARGET_FILE:millennium_rt3_test_client>" "${_rt3_fixtures}/webhelper/steamwebhelper")

function(millennium_add_rt3_bootstrap_test _name _fixture _executable _pass_regex)
    add_test(NAME ${_name}
        COMMAND ${CMAKE_COMMAND} -E env "LD_PRELOAD=$<TARGET_FILE:millennium_rt3_test_proxy>"
            "${_rt3_fixtures}/${_fixture}/${_executable}" ${ARGN})
    set_tests_properties(${_name} PROPERTIES
        TIMEOUT 15
        WORKING_DIRECTORY "${_rt3_fixtures}/${_fixture}"
        PASS_REGULAR_EXPRESSION "${_pass_regex}")
endfunction()

millennium_add_rt3_bootstrap_test(rt3_bootstrap_client client steamrt64/steam "MILLENNIUM_TEST_SUMMARY starts=1 stops=1")
millennium_add_rt3_bootstrap_test(rt3_bootstrap_updater-ui updater steamrt64/steam "SteamRT3 bootstrap: forwarding XTest only" -child-update-ui)
millennium_add_rt3_bootstrap_test(rt3_bootstrap_updater-socket updater steamrt64/steam "SteamRT3 bootstrap: forwarding XTest only" -child-update-ui-socket)
millennium_add_rt3_bootstrap_test(rt3_bootstrap_forwarding forwarding other "SteamRT3 bootstrap: forwarding XTest only")
millennium_add_rt3_bootstrap_test(rt3_bootstrap_webhelper webhelper steamwebhelper "Loaded hhx64 from")
set_tests_properties(rt3_bootstrap_updater-ui rt3_bootstrap_updater-socket rt3_bootstrap_forwarding rt3_bootstrap_webhelper
    PROPERTIES FAIL_REGULAR_EXPRESSION "MILLENNIUM_TEST_STARTED;MILLENNIUM_TEST_STOPPED")

if(TARGET libmillennium_pvs64)
    set(_pvs64_target libmillennium_pvs64)
else()
    add_executable(millennium_pvs64_test_binary "${_millennium_root}/src/bootstrap/linux/millennium_pvs64.cc")
    target_compile_features(millennium_pvs64_test_binary PRIVATE cxx_std_17)
    set(_pvs64_target millennium_pvs64_test_binary)
endif()

add_executable(millennium_pvs64_test pvs64_test.cc)
target_compile_features(millennium_pvs64_test PRIVATE cxx_std_17)

foreach(_mode absolute path no-command)
    add_test(NAME pvs64_${_mode} COMMAND millennium_pvs64_test "$<TARGET_FILE:${_pvs64_target}>" ${_mode})
    set_tests_properties(pvs64_${_mode} PROPERTIES
        TIMEOUT 20
        ENVIRONMENT "TMPDIR=${CMAKE_CURRENT_BINARY_DIR}")
endforeach()
