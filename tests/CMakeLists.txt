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
endif()

if(MILLENNIUM_BUILD_TESTS)
    add_subdirectory(ffi)
endif()

if(MILLENNIUM_RUN_TESTS_ON_BUILD)
    set(_run_tests_deps millennium_cpp_tests)
    if(MILLENNIUM_BUILD_MAIN)
        list(APPEND _run_tests_deps millennium_lua_tests Millennium)
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
