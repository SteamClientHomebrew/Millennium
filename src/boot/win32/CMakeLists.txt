cmake_minimum_required(VERSION 3.10)
project(preload)

set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED True)

add_library(preload SHARED main.c ${MILLENNIUM_BASE}/scripts/exports.def)

target_link_libraries(preload
    version
    # ${Millennium_BINARY_DIR}/version.o
)

if (NOT GITHUB_ACTION_BUILD)
    include(${MILLENNIUM_BASE}/scripts/cmake/find_steam_path.cmake)

    if (out_steam_path)
        set_target_properties(preload PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${out_steam_path}" LIBRARY_OUTPUT_DIRECTORY "${out_steam_path}" ARCHIVE_OUTPUT_DIRECTORY "${out_steam_path}")
    endif()
endif()

set_target_properties(preload PROPERTIES OUTPUT_NAME "version" PREFIX "" NO_EXPORT TRUE)
