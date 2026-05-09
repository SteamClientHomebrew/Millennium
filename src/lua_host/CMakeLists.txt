cmake_minimum_required(VERSION 3.21)

if(WIN32)
    include(${MILLENNIUM_BASE}/scripts/cmake/millennium_version.cmake)

    set(VERSION_COMPANY      "Steam Homebrew")
    set(VERSION_COPYRIGHT    "Steam Homebrew 2025")
    set(VERSION_TRADEMARK1   "All Rights Reserved")
    set(VERSION_DESCRIPTION  "Lua plugin runtime for Millennium")
    set(VERSION_FILENAME     "${MILLENNIUM_RTB_NAME}.exe")
    set(VERSION_PRODUCT      "Millennium Lua Runtime")
    set(VERSION_FILETYPE     "VFT_APP")

    set(VERSION_FILE "${CMAKE_BINARY_DIR}/${MILLENNIUM_RTB_NAME}.rc")
    configure_file(${MILLENNIUM_BASE}/scripts/version.rc.in ${VERSION_FILE} @ONLY)
endif()

set(LUA_HOST_SOURCES
    main.cc
    rpc.cc
    crash_handler.cc
    ${MILLENNIUM_BASE}/src/shared/crash_report.cc
    ${MILLENNIUM_BASE}/src/shared/crash_handler_core.cc
    api/millennium.cc
    api/config.cc
    api/logger.cc
    api/http.cc
    api/utils.cc
    api/filesystem.cc
    api/regex.cc
    api/datetime.cc
    api/json.c
)

if(WIN32)
    list(APPEND LUA_HOST_SOURCES ${VERSION_FILE})
endif()

add_executable(${MILLENNIUM_RTB_NAME} ${LUA_HOST_SOURCES})

target_compile_definitions(${MILLENNIUM_RTB_NAME} PRIVATE
    MILLENNIUM_RTB_NAME="${MILLENNIUM_RTB_NAME}"
)

target_include_directories(${MILLENNIUM_RTB_NAME} PRIVATE
    ${MILLENNIUM_BASE}/src
    ${MILLENNIUM_BASE}/src/include
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CMAKE_CURRENT_SOURCE_DIR}/api/cjson
)

target_link_libraries(${MILLENNIUM_RTB_NAME}
    luajit::lib
    nlohmann_json
)

# Suppress warnings on vendored cjson sources.
set_source_files_properties(
    api/cjson/lua_cjson.c
    api/cjson/strbuf.c
    api/cjson/fpconv.c
    TARGET_DIRECTORY ${MILLENNIUM_RTB_NAME}
    PROPERTIES COMPILE_OPTIONS "$<IF:$<C_COMPILER_ID:MSVC>,/w,-w>"
)

# Always ship with debug info so crash dumps are useful
if(MSVC)
    target_compile_options(${MILLENNIUM_RTB_NAME} PRIVATE /Z7)
    target_link_options(${MILLENNIUM_RTB_NAME} PRIVATE /DEBUG /ENTRY:mainCRTStartup
        "/MANIFESTDEPENDENCY:type='win32' name='Microsoft.Windows.Common-Controls' version='6.0.0.0' processorArchitecture='*' publicKeyToken='6595b64144ccf1df' language='*'"
    )
    target_link_libraries(${MILLENNIUM_RTB_NAME} dbghelp ws2_32)
else()
    target_compile_options(${MILLENNIUM_RTB_NAME} PRIVATE -g)
    target_link_options(${MILLENNIUM_RTB_NAME} PRIVATE -rdynamic)
endif()

set_target_properties(${MILLENNIUM_RTB_NAME} PROPERTIES
    OUTPUT_NAME "${MILLENNIUM_RTB_NAME}"
)

if(WIN32)
    set_target_properties(${MILLENNIUM_RTB_NAME} PROPERTIES WIN32_EXECUTABLE TRUE)
endif()

if(NOT GITHUB_ACTION_BUILD)
    include(${MILLENNIUM_BASE}/scripts/cmake/find_steam_path.cmake)
    find_steam_path(${MILLENNIUM_RTB_NAME})
endif()
