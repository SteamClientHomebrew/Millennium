cmake_minimum_required(VERSION 3.10)

if(WIN32 AND CMAKE_BUILD_TYPE STREQUAL "Release")
  set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -m32 -fvisibility=hidden -Oz -ffunction-sections -fdata-sections -fno-stack-protector -fno-unwind-tables -fno-asynchronous-unwind-tables -fmerge-all-constants")
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -m32 -fvisibility=hidden -Oz -ffunction-sections -fdata-sections -fno-stack-protector -fno-unwind-tables -fno-asynchronous-unwind-tables -fmerge-all-constants")
  set(CMAKE_WINDOWS_EXPORT_ALL_SYMBOLS OFF)
  
  add_compile_definitions(WIN32_LEAN_AND_MEAN)
  add_compile_definitions(NOMINMAX)
endif()

project(preload)

set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED True)

set(CMAKE_C_FLAGS_RELEASE "${CMAKE_C_FLAGS_RELEASE} -static-libgcc")
set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} -static-libgcc -static-libstdc++")


add_library(preload SHARED main.c ../../../scripts/exports.def)
target_link_libraries(preload 
version
#${CMAKE_BINARY_DIR}/version.o
)

if(WIN32 AND NOT GITHUB_ACTION_BUILD)
    execute_process(COMMAND reg query "HKCU\\Software\\Valve\\Steam" /v "SteamPath" RESULT_VARIABLE result OUTPUT_VARIABLE steam_path ERROR_VARIABLE reg_error)
    if(result EQUAL 0)
        string(REGEX MATCH "[a-zA-Z]:/[^ ]+([ ]+[^ ]+)*" extracted_path "${steam_path}")
        string(REPLACE "\n" "" extracted_path "${extracted_path}")
        message(STATUS "[preload] Target build path: ${extracted_path}")
        set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${extracted_path})
        set(LIBRARY_OUTPUT_DIRECTORY ${extracted_path})
    else()
        message(WARNING "[preload] Failed to read Steam installation path from HKCU\\Software\\Valve\\Steam.")
    endif()

    if(extracted_path)
        set_target_properties(preload PROPERTIES
            RUNTIME_OUTPUT_DIRECTORY "${extracted_path}"
            LIBRARY_OUTPUT_DIRECTORY "${extracted_path}"
            ARCHIVE_OUTPUT_DIRECTORY "${extracted_path}"
        )
        # Override per-configuration output directories
        foreach(CONFIG ${CMAKE_CONFIGURATION_TYPES})
            string(TOUPPER ${CONFIG} CONFIG_UPPER)
            set_target_properties(preload PROPERTIES
                RUNTIME_OUTPUT_DIRECTORY_${CONFIG_UPPER} "${extracted_path}"
                LIBRARY_OUTPUT_DIRECTORY_${CONFIG_UPPER} "${extracted_path}"
                ARCHIVE_OUTPUT_DIRECTORY_${CONFIG_UPPER} "${extracted_path}"
            )
        endforeach()
    endif()
elseif(UNIX AND NOT GITHUB_ACTION_BUILD)
  set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "$ENV{HOME}/.millennium/")
  set(CMAKE_LIBRARY_OUTPUT_DIRECTORY "$ENV{HOME}/.millennium/")
endif()

set_target_properties(preload PROPERTIES 
  OUTPUT_NAME "version"
  PREFIX ""
  NO_EXPORT TRUE
)

if(UNIX)
  set_target_properties(preload PROPERTIES LINK_FLAGS_RELEASE "-s -Wl,--gc-sections -Wl,--strip-all -Wl,--no-seh")
endif()