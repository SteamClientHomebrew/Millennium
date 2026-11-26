cmake_minimum_required(VERSION 3.21)

include(FetchContent)
if(DISTRO_NIX)
    set(FETCHCONTENT_FULLY_DISCONNECTED ON)
    FetchContent_Declare(Catch2 SOURCE_DIR "${MILLENNIUM_BASE}/deps/catch2")
else()
    FetchContent_Declare(Catch2
    URL "file://${MILLENNIUM_BASE}/thirdparty/catch2-v3.5.3.tar.gz"
    DOWNLOAD_EXTRACT_TIMESTAMP TRUE
  )
endif()
set(CMAKE_MESSAGE_LOG_LEVEL WARNING)
FetchContent_MakeAvailable(Catch2)
set(CMAKE_MESSAGE_LOG_LEVEL STATUS)
list(APPEND CMAKE_MODULE_PATH ${catch2_SOURCE_DIR}/extras)

add_subdirectory(cpp)

if(MILLENNIUM_BUILD_MAIN)
    add_subdirectory(lua)
    if(APPLE)
        add_test(NAME macos_runtime_update
            COMMAND /bin/bash "${CMAKE_CURRENT_SOURCE_DIR}/macos/runtime_update.sh"
                    "$<TARGET_FILE_DIR:Millennium>")
    endif()
endif()

if(MILLENNIUM_BUILD_TESTS)
    add_subdirectory("${CMAKE_SOURCE_DIR}/src/typescript/tests/ffi" ffi)
endif()

if(MILLENNIUM_RUN_TESTS_ON_BUILD)
    set(_run_tests_deps millennium_cpp_tests millennium_cdp_shutdown_test)
    if(UNIX)
        list(APPEND _run_tests_deps millennium_posix_lifecycle_test millennium_logger_concurrency_test millennium_child_shutdown_test)
    endif()
    if(TARGET millennium_cdp_tests)
        list(APPEND _run_tests_deps millennium_cdp_tests)
    endif()
    if(TARGET millennium_macos_connector_tests)
        list(APPEND _run_tests_deps millennium_macos_connector_tests)
    endif()
    if(MILLENNIUM_BUILD_MAIN)
        list(APPEND _run_tests_deps millennium_lua_tests Millennium)
        if(APPLE)
            list(APPEND _run_tests_deps macos_runtime)
        endif()
    endif()
    if(CMAKE_SYSTEM_NAME STREQUAL "Linux" AND (MILLENNIUM_LINUX_X64 OR MILLENNIUM_NIX_64BIT))
        list(APPEND _run_tests_deps millennium_rt3_test_proxy millennium_rt3_test_client millennium_pvs64_test)
        if(TARGET libmillennium_pvs64)
            list(APPEND _run_tests_deps libmillennium_pvs64)
        else()
            list(APPEND _run_tests_deps millennium_pvs64_test_binary)
        endif()
    endif()

    add_custom_target(run_all_tests ALL
    COMMAND ${CMAKE_COMMAND}
      -D "CTEST=${CMAKE_CTEST_COMMAND}"
      -D "CONFIG=$<CONFIG>"
      -D "WORKING_DIR=${CMAKE_BINARY_DIR}"
      -P "${CMAKE_SOURCE_DIR}/scripts/cmake/run_tests.cmake"
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
    DEPENDS ${_run_tests_deps}
    COMMENT "[MLNM] Testing Millennium API..."
    USES_TERMINAL
  )
endif()
