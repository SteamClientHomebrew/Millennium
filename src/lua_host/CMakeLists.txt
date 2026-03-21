cmake_minimum_required(VERSION 3.21)

set(LUA_HOST_SOURCES
    main.cc
    rpc.cc
    crash_handler.cc
    ${MILLENNIUM_BASE}/src/shared/crash_report.cc
    ${MILLENNIUM_BASE}/src/shared/crash_handler_core.cc
    lua_millennium_rpc.cc
    lua_config_rpc.cc
    lua_logger_rpc.cc
    lua_http.cc
    lua_utils.cc
    lua_filesystem.cc
    lua_regex.cc
    lua_datetime.cc
)

add_executable(${MILLENNIUM_RTB_NAME} ${LUA_HOST_SOURCES})

target_compile_definitions(${MILLENNIUM_RTB_NAME} PRIVATE
    MILLENNIUM_RTB_NAME="${MILLENNIUM_RTB_NAME}"
)

target_include_directories(${MILLENNIUM_RTB_NAME} PRIVATE
    ${MILLENNIUM_BASE}/src
    ${MILLENNIUM_BASE}/src/include
)

target_link_libraries(${MILLENNIUM_RTB_NAME}
    luajit::lib
    nlohmann_json
    fmt::fmt
)

if(NOT UNIX OR APPLE)
    target_link_libraries(${MILLENNIUM_RTB_NAME} cjson)
endif()

# Always ship with debug info so crash dumps are useful
if(MSVC)
    target_compile_options(${MILLENNIUM_RTB_NAME} PRIVATE /Z7)
    target_link_options(${MILLENNIUM_RTB_NAME} PRIVATE /DEBUG)
    target_link_libraries(${MILLENNIUM_RTB_NAME} dbghelp ws2_32)
else()
    target_compile_options(${MILLENNIUM_RTB_NAME} PRIVATE -g)
    target_link_options(${MILLENNIUM_RTB_NAME} PRIVATE -rdynamic)
endif()

set_target_properties(${MILLENNIUM_RTB_NAME} PROPERTIES
    OUTPUT_NAME "${MILLENNIUM_RTB_NAME}"
)

if(NOT GITHUB_ACTION_BUILD)
    include(${MILLENNIUM_BASE}/scripts/cmake/find_steam_path.cmake)
    find_steam_path(${MILLENNIUM_RTB_NAME})
endif()
