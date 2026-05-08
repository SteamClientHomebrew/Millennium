/**
 * ==================================================
 *   _____ _ _ _             _
 *  |     |_| | |___ ___ ___|_|_ _ _____
 *  | | | | | | | -_|   |   | | | |     |
 *  |_|_|_|_|_|_|___|_|_|_|_|_|___|_|_|_|
 *
 * ==================================================
 *
 * Copyright (c) 2026 Project Millennium
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

#include <catch2/catch_test_macros.hpp>
#include <filesystem>
#include <lua.hpp>
#include <string>

extern "C" int luaopen_regex_lib(lua_State *L);
extern "C" int luaopen_datetime_lib(lua_State *L);
extern "C" int luaopen_fs_lib(lua_State *L);
extern "C" int luaopen_utils_lib(lua_State *L);
extern "C" int luaopen_cjson(lua_State *L);

namespace fs = std::filesystem;

namespace {
void register_module(lua_State *L, const char *name, lua_CFunction loader) {
  lua_pushcfunction(L, loader);
  lua_setfield(L, -2, name);
}

lua_State *setup_lua_state() {
  lua_State *L = luaL_newstate();
  REQUIRE(L != nullptr);
  luaL_openlibs(L);

  const auto stub_backend_dir =
      (fs::temp_directory_path() / "millennium-lua-test-backend").string();
  lua_pushstring(L, stub_backend_dir.c_str());
  lua_setglobal(L, "MILLENNIUM_PLUGIN_SECRET_BACKEND_ABSOLUTE");

  lua_getglobal(L, "package");
  lua_getfield(L, -1, "preload");

  register_module(L, "regex", luaopen_regex_lib);
  register_module(L, "datetime", luaopen_datetime_lib);
  register_module(L, "fs", luaopen_fs_lib);
  register_module(L, "utils", luaopen_utils_lib);
  register_module(L, "json", luaopen_cjson);

  lua_pop(L, 2);
  return L;
}

void run_lua_file(lua_State *L, const fs::path &path) {
  const auto path_str = path.string();
  if (luaL_dofile(L, path_str.c_str()) != LUA_OK) {
    const char *err = lua_tostring(L, -1);
    std::string message = err ? err : "unknown Lua error";
    lua_pop(L, 1);
    FAIL("Lua test failed in " << path_str << ":\n  " << message);
  }
}
} // namespace

TEST_CASE("Lua host API: every case file in tests/lua/cases/ runs cleanly",
          "[lua]") {
  const fs::path cases_dir{MILLENNIUM_LUA_TEST_CASES_DIR};
  REQUIRE(fs::is_directory(cases_dir));

  bool found_any = false;
  for (const auto &entry : fs::directory_iterator(cases_dir)) {
    if (entry.path().extension() != ".lua")
      continue;
    found_any = true;

    DYNAMIC_SECTION("case: " << entry.path().filename().string()) {
      lua_State *L = setup_lua_state();
      run_lua_file(L, entry.path());
      lua_close(L);
    }
  }

  REQUIRE(
      found_any); // empty cases/ dir is a configuration bug, not a passing test
}
