set(LUA_TEST_API_SOURCES
  ${CMAKE_SOURCE_DIR}/src/lua_host/api/regex.cc
  ${CMAKE_SOURCE_DIR}/src/lua_host/api/datetime.cc
  ${CMAKE_SOURCE_DIR}/src/lua_host/api/filesystem.cc
  ${CMAKE_SOURCE_DIR}/src/lua_host/api/utils.cc
  ${CMAKE_SOURCE_DIR}/src/lua_host/api/json.c
)

add_executable(millennium_lua_tests
  main.cc
  ${LUA_TEST_API_SOURCES}
)

set_source_files_properties(
  ${CMAKE_SOURCE_DIR}/src/lua_host/api/json.c
  TARGET_DIRECTORY millennium_lua_tests
  PROPERTIES COMPILE_OPTIONS "$<IF:$<C_COMPILER_ID:MSVC>,/w,-w>"
)

target_compile_features(millennium_lua_tests PRIVATE cxx_std_23)

target_include_directories(millennium_lua_tests PRIVATE
  ${CMAKE_SOURCE_DIR}/src
  ${CMAKE_SOURCE_DIR}/src/include
  ${luajit_BINARY_DIR}
  ${luajit_SOURCE_DIR}/src
)

target_link_libraries(millennium_lua_tests PRIVATE
  Catch2::Catch2WithMain
  luajit::lib
)

target_compile_definitions(millennium_lua_tests PRIVATE
  MILLENNIUM_LUA_TEST_CASES_DIR="${CMAKE_CURRENT_SOURCE_DIR}/cases"
)

include(Catch)
catch_discover_tests(millennium_lua_tests)
