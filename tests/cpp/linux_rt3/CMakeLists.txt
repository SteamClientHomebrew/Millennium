cmake_minimum_required(VERSION 3.21)

if(CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
    project(MillenniumLinuxRt3Tests LANGUAGES C CXX)
    enable_testing()
endif()

get_filename_component(_millennium_root "${CMAKE_CURRENT_SOURCE_DIR}/../../.." ABSOLUTE)
foreach(_flag CMAKE_C_FLAGS CMAKE_CXX_FLAGS CMAKE_EXE_LINKER_FLAGS CMAKE_SHARED_LINKER_FLAGS)
    string(REPLACE "-m32" "" ${_flag} "${${_flag}}")
endforeach()
add_compile_options(-m64)
add_link_options(-m64)

add_executable(millennium_uuid_test uuid_test.cc)
target_compile_features(millennium_uuid_test PRIVATE cxx_std_23)
target_include_directories(millennium_uuid_test PRIVATE "${_millennium_root}/src/include")

foreach(_mode classic grouped)
    add_test(NAME uuid_${_mode} COMMAND millennium_uuid_test ${_mode})
endforeach()

find_package(Python3 COMPONENTS Interpreter REQUIRED)
find_package(X11 REQUIRED)
find_program(_readelf NAMES readelf REQUIRED)
add_test(NAME linux_ci_native_environment
    COMMAND "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/ci_environment_test.py" "${_millennium_root}/.github/workflows/ci.yml")
set_tests_properties(linux_ci_native_environment PROPERTIES TIMEOUT 15 ENVIRONMENT "TMPDIR=${CMAKE_CURRENT_BINARY_DIR}")
foreach(_mode default custom nix legacy-nix disabled)
    add_test(NAME linux_runtime_paths_${_mode}
        COMMAND "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/runtime_paths_test.py" "${_millennium_root}" "${CMAKE_COMMAND}" "${_readelf}" ${_mode})
    set_tests_properties(linux_runtime_paths_${_mode} PROPERTIES TIMEOUT 60 ENVIRONMENT "TMPDIR=${CMAKE_CURRENT_BINARY_DIR}")
endforeach()
add_executable(millennium_rt3_test_client client.c)
add_library(millennium_rt3_test_payload SHARED payload.c)
add_library(millennium_rt3_test_proxy SHARED "${_millennium_root}/src/bootstrap/linux/libmillennium_bootstrap_x64.c")
target_include_directories(millennium_rt3_test_proxy PRIVATE ${X11_INCLUDE_DIR} ${X11_Xtst_INCLUDE_PATH})
target_link_libraries(millennium_rt3_test_proxy PRIVATE ${CMAKE_DL_LIBS})
add_dependencies(millennium_rt3_test_proxy millennium_rt3_test_payload)
target_compile_definitions(millennium_rt3_test_proxy PRIVATE
    MILLENNIUM_X64_PATH="$<TARGET_FILE:millennium_rt3_test_payload>"
    MILLENNIUM_HHX64_PATH="$<TARGET_FILE:millennium_rt3_test_payload>")

foreach(_mode client updater-ui updater-socket forwarding webhelper)
    add_test(NAME rt3_bootstrap_${_mode}
        COMMAND "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/bootstrap_test.py" "$<TARGET_FILE:millennium_rt3_test_proxy>" "$<TARGET_FILE:millennium_rt3_test_client>" ${_mode})
    set_tests_properties(rt3_bootstrap_${_mode} PROPERTIES TIMEOUT 15 ENVIRONMENT "TMPDIR=${CMAKE_CURRENT_BINARY_DIR}")
endforeach()
add_executable(millennium_pvs64_test_binary "${_millennium_root}/src/bootstrap/linux/millennium_pvs64.cc")
target_compile_features(millennium_pvs64_test_binary PRIVATE cxx_std_17)
target_compile_options(millennium_pvs64_test_binary PRIVATE -m64)
target_link_options(millennium_pvs64_test_binary PRIVATE -m64)

foreach(_mode absolute path no-command)
    add_test(NAME pvs64_${_mode}
        COMMAND "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/pvs64_test.py" "$<TARGET_FILE:millennium_pvs64_test_binary>" ${_mode})
    set_tests_properties(pvs64_${_mode} PROPERTIES TIMEOUT 15 ENVIRONMENT "TMPDIR=${CMAKE_CURRENT_BINARY_DIR}")
endforeach()
