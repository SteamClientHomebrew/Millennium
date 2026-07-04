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
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

/*
 * millennium_luavm — standalone child process for a single plugin backend.
 *
 * Gets forked by the parent, connects back over the unix socket it was given,
 * spins up a LuaJIT VM, runs the plugin's main.lua, and then sits in an event
 * loop handling RPCs from the parent. If we segfault the parent keeps running.
 */

#include "rpc.h"
#include "crash_handler.h"
#include "lua_api.h"
#include "millennium/star_parser.h"
#include "millennium/plugin_manager.h"
#include "millennium/types.h"
#include "millennium/plugin_ipc.h"

#include <format>
#include <lua.hpp>
#include <luajit.h>
#include <nlohmann/json.hpp>

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <string>
#include <unordered_map>

#ifdef _WIN32
#include <winsock2.h>
#include <windows.h>
#include <afunix.h>
#else
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>
#if defined(__linux__)
#include <sys/prctl.h>
#endif
#endif

rpc_client* g_rpc = nullptr;

static std::string parse_lua_shim(const std::string& plg_path)
{
    std::ifstream f(plg_path, std::ios::binary);
    if (!f) return {};

    uint8_t len_buf[4];
    if (!f.read(reinterpret_cast<char*>(len_buf), 4)) return {};
    const uint32_t shim_len =
        static_cast<uint32_t>(len_buf[0]) | (static_cast<uint32_t>(len_buf[1]) << 8) | (static_cast<uint32_t>(len_buf[2]) << 16) | (static_cast<uint32_t>(len_buf[3]) << 24);

    static constexpr uint32_t SHIM_MAX_SIZE = 4u * 1024u * 1024u;
    if (shim_len == 0 || shim_len > SHIM_MAX_SIZE) return {};

    std::string shim(shim_len, '\0');
    if (!f.read(shim.data(), shim_len)) return {};
    return shim;
}

static int lua_millennium_decompress(lua_State* L)
{
    size_t src_len;
    const char* src = luaL_checklstring(L, 1, &src_len);
    try {
        auto out = star_decompress(reinterpret_cast<const uint8_t*>(src), src_len);
        lua_pushlstring(L, reinterpret_cast<const char*>(out.data()), out.size());
        return 1;
    } catch (const std::exception& e) {
        return luaL_error(L, "MILLENNIUM_DECOMPRESS: %s", e.what());
    }
}

static lua_State* g_L = nullptr;
static std::string g_plugin_name;
std::string g_backend_dir;
std::string g_backend_file;
bool g_plugin_is_v2 = false;
std::unordered_map<std::string, AssetEntry> g_asset_index;

static plugin_ipc::socket_fd connect_to_parent(const char* socket_path)
{
#ifdef _WIN32
    WSADATA wsa;
    if (WSAStartup(MAKEWORD(2, 2), &wsa) != 0) {
        fprintf(stderr, "[lua-host] WSAStartup failed\n");
        return plugin_ipc::INVALID_FD;
    }
#endif

    plugin_ipc::socket_fd fd = static_cast<plugin_ipc::socket_fd>(::socket(AF_UNIX, SOCK_STREAM, 0));
#ifdef _WIN32
    if (fd == INVALID_SOCKET) {
#else
    if (fd < 0) {
#endif
        fprintf(stderr, "[lua-host] socket() failed\n");
        return plugin_ipc::INVALID_FD;
    }

    sockaddr_un addr{};
    addr.sun_family = AF_UNIX;
    strncpy(addr.sun_path, socket_path, sizeof(addr.sun_path) - 1);

    if (::connect(fd, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) < 0) {
        fprintf(stderr, "[lua-host] connect(%s) failed\n", socket_path);
        plugin_ipc::close_fd(fd);
        return plugin_ipc::INVALID_FD;
    }

    return fd;
}

static void register_preloaded_module(lua_State* L, const char* name, lua_CFunction func)
{
    lua_pushcclosure(L, func, 0);
    lua_setfield(L, -2, name);
}

/* pull the patches table out of the plugin def and ship it over to the parent
   so it can write them into the shared memory arena for the loopback hook. */
static void send_patches_to_parent(lua_State* L)
{
    lua_getfield(L, -1, "patches");
    if (!lua_istable(L, -1)) {
        lua_pop(L, 1);
        return;
    }

    json patches_json = json::array();
    int patch_count = static_cast<int>(lua_objlen(L, -1));

    for (int i = 1; i <= patch_count; ++i) {
        lua_rawgeti(L, -1, i);

        json patch;

        lua_getfield(L, -1, "file");
        if (lua_isstring(L, -1))
            patch["file"] = lua_tostring(L, -1);
        else
            patch["file"] = "";
        lua_pop(L, 1);

        lua_getfield(L, -1, "find");
        if (lua_isstring(L, -1))
            patch["find"] = lua_tostring(L, -1);
        else
            patch["find"] = "";
        lua_pop(L, 1);

        json transforms = json::array();
        lua_getfield(L, -1, "transforms");
        if (lua_istable(L, -1)) {
            int tc = static_cast<int>(lua_objlen(L, -1));
            for (int j = 1; j <= tc; ++j) {
                lua_rawgeti(L, -1, j);
                json t;

                lua_getfield(L, -1, "match");
                if (lua_isstring(L, -1))
                    t["match"] = lua_tostring(L, -1);
                else
                    t["match"] = "";
                lua_pop(L, 1);

                lua_getfield(L, -1, "replace");
                if (lua_isstring(L, -1))
                    t["replace"] = lua_tostring(L, -1);
                else
                    t["replace"] = "";
                lua_pop(L, 1);

                transforms.push_back(std::move(t));
                lua_pop(L, 1);
            }
        }
        lua_pop(L, 1);

        patch["transforms"] = std::move(transforms);
        patches_json.push_back(std::move(patch));

        lua_pop(L, 1); /* pop patch table entry */
    }

    lua_pop(L, 1); /* pop "patches" table */

    if (!patches_json.empty()) {
        try {
            const json params = {
                { "plugin_name", g_plugin_name },
                { "patches",     patches_json  }
            };

            g_rpc->call(plugin_ipc::child_method::PATCHES, params);
        } catch (const std::exception& e) {
            log_runtime_error(std::string("failed to send patches: ") + e.what());
        }
    }
}

/* parent wants us to call a Lua function — this is the FFI hot path */
static json handle_evaluate(lua_State* L, const json& params)
{
    if (!params.contains("methodName") || !params["methodName"].is_string()) {
        return {
            { "success", false                },
            { "error",   "missing methodName" }
        };
    }

    std::string methodName = params["methodName"];
    std::vector<json> argValues;

    if (params.contains("argumentList")) {
        const auto& list = params["argumentList"];
        if (list.is_object() || list.is_array()) {
            for (auto it = list.begin(); it != list.end(); ++it) {
                argValues.push_back(it.value());
            }
        }
    }

    /* support "table:method" syntax */
    size_t colonPos = methodName.find(':');
    bool isMethod = (colonPos != std::string::npos);
    int numArgs = static_cast<int>(argValues.size());

    if (isMethod) {
        std::string tableName = methodName.substr(0, colonPos);
        std::string funcName = methodName.substr(colonPos + 1);

        lua_getglobal(L, tableName.c_str());
        if (!lua_istable(L, -1)) {
            lua_pop(L, 1);
            return {
                { "success", false                           },
                { "error",   "table not found: " + tableName }
            };
        }

        lua_getfield(L, -1, funcName.c_str());
        if (!lua_isfunction(L, -1)) {
            lua_pop(L, 2);
            return {
                { "success", false                             },
                { "error",   "method not found: " + methodName }
            };
        }

        lua_pushvalue(L, -2); /* push table as self */
        lua_remove(L, -3);    /* remove original table */
        numArgs += 1;
    } else {
        lua_getglobal(L, methodName.c_str());
        if (!lua_isfunction(L, -1)) {
            lua_pop(L, 1);
            return {
                { "success", false                               },
                { "error",   "function not found: " + methodName }
            };
        }
    }

    /* push arguments */
    for (const auto& arg : argValues) {
        if (arg.is_string())
            lua_pushstring(L, arg.get<std::string>().c_str());
        else if (arg.is_boolean())
            lua_pushboolean(L, arg.get<bool>());
        else if (arg.is_number_integer())
            lua_pushinteger(L, arg.get<lua_Integer>());
        else if (arg.is_number_float())
            lua_pushnumber(L, arg.get<lua_Number>());
        else
            lua_pushnil(L);
    }

    if (lua_pcall(L, numArgs, 1, 0) != LUA_OK) {
        const char* err = lua_tostring(L, -1);
        std::string errMsg = err ? err : "unknown Lua error";
        lua_pop(L, 1);
        return {
            { "success", false  },
            { "error",   errMsg }
        };
    }

    /* read result */
    json result;
    result["success"] = true;

    if (lua_isstring(L, -1)) {
        result["value"] = lua_tostring(L, -1);
        result["value_type"] = "string";
    } else if (lua_isboolean(L, -1)) {
        result["value"] = static_cast<bool>(lua_toboolean(L, -1));
        result["value_type"] = "boolean";
    } else if (lua_isnumber(L, -1)) {
        result["value"] = lua_tonumber(L, -1);
        result["value_type"] = "number";
    } else {
        result["value"] = nullptr;
        result["value_type"] = "nil";
    }

    lua_pop(L, 1);
    return result;
}

/**
 * Handle "on_frontend_loaded" notification from the parent.
 */
static json handle_frontend_loaded(lua_State* L)
{
    lua_getglobal(L, "MILLENNIUM_PLUGIN_DEFINITION");
    if (!lua_istable(L, -1)) {
        lua_pop(L, 1);
        return {
            { "ok", false }
        };
    }

    lua_getfield(L, -1, "on_frontend_loaded");
    if (!lua_isfunction(L, -1)) {
        lua_pop(L, 2);
        return {
            { "ok", true }
        }; /* not an error if not defined */
    }

    if (lua_pcall(L, 0, 0, 0) != LUA_OK) {
        const char* err = lua_tostring(L, -1);
        log_lua_error("on_frontend_loaded error", err);
        lua_pop(L, 1);
    }

    lua_pop(L, 1);
    return {
        { "ok", true }
    };
}

static bool g_on_unload_called = false;

/**
 * Handle "shutdown" request: call on_unload, cleanup, and exit.
 */
static json handle_shutdown(lua_State* L)
{
    if (g_on_unload_called) {
        return {
            { "ok", true }
        };
    }
    g_on_unload_called = true;

    lua_getglobal(L, "MILLENNIUM_PLUGIN_DEFINITION");
    if (lua_istable(L, -1)) {
        lua_getfield(L, -1, "on_unload");
        if (lua_isfunction(L, -1)) {
            if (lua_pcall(L, 0, 0, 0) != LUA_OK) {
                const char* err = lua_tostring(L, -1);
                log_lua_error("on_unload error", err);
                lua_pop(L, 1);
            }
        } else {
            lua_pop(L, 1);
        }
    }
    lua_pop(L, 1);

    return {
        { "ok", true }
    };
}

int main(int argc, char* argv[])
{
    if (argc >= 2 && (strcmp(argv[1], "--version") == 0 || strcmp(argv[1], "-v") == 0)) {
        printf("%s %s (%s)\n", MILLENNIUM_RTB_NAME, MILLENNIUM_VERSION, GIT_COMMIT_HASH);
        return 0;
    }

    if (argc < 2) {
#ifdef _WIN32
        SetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);
        MessageBoxA(nullptr,
                    "This is an internal Millennium runtime component.\n"
                    "It is not meant to be run directly.\n\n"
                    "Please launch Steam with Millennium instead.",
                    MILLENNIUM_RTB_NAME, MB_OK | MB_ICONINFORMATION);
#else
        fprintf(stderr,
                "%s: This is an internal Millennium runtime component.\n"
                "It is not meant to be run directly. Please launch Steam with Millennium instead.\n",
                MILLENNIUM_RTB_NAME);
#endif
        return 1;
    }

    const char* socket_path = argv[1];

#if defined(__linux__)
    /*
     * the parent thread that called fork() may be an ephemeral MEP handler thread
     * that exits after processing plugin.restart. prctl(PR_SET_PDEATHSIG) is tied
     * to the forking thread (not the process), so when that thread exits we'd get
     * a spurious SIGTERM. Clear it immediately, socket EOF is our death signal.
     */
    prctl(PR_SET_PDEATHSIG, 0);
#endif

    plugin_ipc::socket_fd fd = connect_to_parent(socket_path);
    if (fd == plugin_ipc::INVALID_FD) {
        return 1;
    }

    rpc_client rpc(fd);
    g_rpc = &rpc;

    json init_msg;
    {
        if (!plugin_ipc::read_msg(fd, init_msg)) {
            fprintf(stderr, "[lua-host] failed to read init message\n");
            plugin_ipc::close_fd(fd);
            return 1;
        }

        if (init_msg.value("method", "") != plugin_ipc::parent_method::INIT) {
            fprintf(stderr, "[lua-host] expected init message, got something else\n");
            plugin_ipc::close_fd(fd);
            return 1;
        }
    }

    int init_id = init_msg.value("id", -1);
    json init_params = init_msg.value("params", json::object());

    g_plugin_name = init_params.value("plugin_name", "");
    g_backend_dir = init_params.value("backend_dir", "");
    g_backend_file = init_params.value("backend_file", "");
    g_plugin_is_v2 = (init_params.value("plugin_format", plugin_manager::plugin_format::loose_files) == plugin_manager::plugin_format::star);
    std::string g_backend_entry = init_params.value("backend_entry", "");
    std::string steam_path = init_params.value("steam_path", "");
    std::string crash_dump_dir = init_params.value("crash_dump_dir", "");
    unsigned int steam_pid = init_params.value("steam_pid", 0u);

    install_crash_handler(g_plugin_name.c_str(), g_backend_file.c_str(), steam_path.c_str(), crash_dump_dir.c_str(), steam_pid);

    lua_State* L = luaL_newstate();
    if (!L) {
        fprintf(stderr, "[lua-host] failed to create Lua state\n");

        json err_resp = {
            { "type",  plugin_ipc::TYPE_RESPONSE    },
            { "id",    init_id                      },
            { "error", "failed to create Lua state" }
        };
        plugin_ipc::write_msg(fd, err_resp);
        plugin_ipc::close_fd(fd);
        return 1;
    }

    g_L = L;
    luaL_openlibs(L);
    
    /* Disable JIT by default. Plugins must call jit.on() to enable it. */
    luaJIT_setmode(L, 0, LUAJIT_MODE_ENGINE | LUAJIT_MODE_OFF);

    lua_pushcfunction(L, lua_millennium_decompress);
    lua_setglobal(L, "MILLENNIUM_DECOMPRESS");

    lua_pushstring(L, g_plugin_name.c_str());
    lua_setglobal(L, "MILLENNIUM_PLUGIN_SECRET_NAME");

    lua_pushstring(L, g_backend_dir.c_str());
    lua_setglobal(L, "MILLENNIUM_PLUGIN_SECRET_BACKEND_ABSOLUTE");

    lua_getglobal(L, "package");
    lua_getfield(L, -1, "path");
    std::string currentPath = lua_tostring(L, -1);
    std::string newPath = g_backend_dir + "/?.lua;" + currentPath;
    lua_pop(L, 1);
    lua_pushstring(L, newPath.c_str());
    lua_setfield(L, -2, "path");
    lua_pop(L, 1);

    lua_getglobal(L, "package");
    lua_getfield(L, -1, "preload");

    register_preloaded_module(L, "json", luaopen_cjson);
    register_preloaded_module(L, "millennium", luaopen_millennium_lib);
    register_preloaded_module(L, "http", luaopen_http_lib);
    register_preloaded_module(L, "utils", luaopen_utils_lib);
    register_preloaded_module(L, "logger", luaopen_logger_lib);
    register_preloaded_module(L, "fs", luaopen_fs_lib);
    register_preloaded_module(L, "regex", luaopen_regex_lib);
    register_preloaded_module(L, "datetime", luaopen_datetime_lib);

    lua_pop(L, 2);

    {
        auto lua_fail = [&](const char* ctx, const char* err) -> int
        {
            log_lua_error(ctx, err);
            json err_resp = {
                { "type",  plugin_ipc::TYPE_RESPONSE },
                { "id",    init_id                   },
                { "error", err ? err : ctx           }
            };
            plugin_ipc::write_msg(fd, err_resp);
            lua_close(L);
            plugin_ipc::close_fd(fd);
            return 1;
        };

        if (g_plugin_is_v2) {
            /* bootstrap shim from .star header, parse backend section, run main.lua */
            std::string shim_lua = parse_lua_shim(g_backend_file);
            if (shim_lua.empty()) return lua_fail("shim extraction", "failed to extract shim from .star file");

            if (luaL_loadbuffer(L, shim_lua.c_str(), shim_lua.size(), std::format("=({})", g_plugin_name).c_str()) != LUA_OK) {
                const char* err = lua_tostring(L, -1);
                return lua_fail("shim load", err);
            }
            if (lua_pcall(L, 0, 1, 0) != LUA_OK) {
                const char* err = lua_tostring(L, -1);
                return lua_fail("shim exec", err);
            }

            /* call parse_fn(plg_path) to get the section table */
            lua_pushstring(L, g_backend_file.c_str());
            if (lua_pcall(L, 1, 1, 0) != LUA_OK) {
                const char* err = lua_tostring(L, -1);
                return lua_fail("shim parse", err);
            }

            lua_getfield(L, -1, "backend");
            if (!lua_isfunction(L, -1)) {
                lua_pop(L, 2);
                return lua_fail("shim backend", "sections.backend is not a function");
            }
            if (lua_pcall(L, 0, 1, 0) != LUA_OK) {
                const char* err = lua_tostring(L, -1);
                lua_pop(L, 1);
                return lua_fail("shim backend()", err);
            }

            /* collect all backend sub-entries */
            struct lua_entry_t
            {
                std::string name, src;
            };
            std::vector<lua_entry_t> all_entries;

            int entries_idx = lua_gettop(L);
            size_t entry_count = lua_objlen(L, entries_idx);

            for (size_t i = 1; i <= entry_count; ++i) {
                lua_rawgeti(L, entries_idx, static_cast<int>(i));
                lua_getfield(L, -1, "name");
                lua_getfield(L, -2, "data");
                const char* raw_name = lua_tostring(L, -2);
                size_t dlen = 0;
                const char* dptr = lua_tolstring(L, -1, &dlen);
                if (raw_name && dptr) all_entries.push_back({ std::string(raw_name), std::string(dptr, dlen) });
                lua_pop(L, 3);
            }

            lua_pop(L, 2); /* pop entries + sections */

            for (const auto& e : init_params.value("asset_index", json::array())) {
                AssetEntry ae;
                ae.file_offset         = e.value("file_offset",         size_t{0});
                ae.compressed_length   = e.value("compressed_length",   size_t{0});
                ae.uncompressed_length = e.value("uncompressed_length", size_t{0});
                g_asset_index[e.value("name", "")] = ae;
            }

            /* register non-entry files into package.preload */
            const std::filesystem::path backend_root = std::filesystem::path(g_backend_entry).parent_path();

            auto lua_name_from_path = [&](const std::string& packed_name) -> std::string
            {
                std::filesystem::path rel = std::filesystem::path(packed_name).lexically_relative(backend_root);
                rel.replace_extension();
                std::string mod;
                for (const auto& part : rel) {
                    const std::string s = part.string();
                    if (s.empty() || s == "." || s == "..") return {};
                    if (!mod.empty()) mod += '.';
                    mod += s;
                }
                return mod;
            };

            lua_getglobal(L, "package");
            lua_getfield(L, -1, "preload");
            for (const auto& e : all_entries) {
                if (e.name == g_backend_entry) continue;
                const std::string mod = lua_name_from_path(e.name);
                if (mod.empty()) continue;
                const std::string chunk_label = "=(packed/" + e.name + ")";
                if (luaL_loadbuffer(L, e.src.c_str(), e.src.size(), chunk_label.c_str()) == LUA_OK)
                    lua_setfield(L, -2, mod.c_str());
                else
                    lua_pop(L, 1); /* discard error and skip */
            }
            lua_pop(L, 2); /* pop preload + package */

            std::string main_lua_src;
            for (const auto& e : all_entries) {
                if (e.name == g_backend_entry) {
                    main_lua_src = e.src;
                    break;
                }
            }

            if (main_lua_src.empty()) return lua_fail("shim backend", ("entry not found in backend section: " + g_backend_entry).c_str());

            const std::string chunk_name = "=(packed/" + g_backend_entry + ")";
            if (luaL_loadbuffer(L, main_lua_src.c_str(), main_lua_src.size(), chunk_name.c_str()) != LUA_OK) {
                const char* err = lua_tostring(L, -1);
                return lua_fail("packed entry load", err);
            }
            if (lua_pcall(L, 0, 1, 0) != LUA_OK) {
                const char* err = lua_tostring(L, -1);
                return lua_fail("packed entry exec", err);
            }
        } else {
            /* Legacy plugin: load from file path */
            if (luaL_loadfile(L, g_backend_file.c_str()) != LUA_OK || lua_pcall(L, 0, 1, 0) != LUA_OK) {
                const char* err = lua_tostring(L, -1);
                return lua_fail(g_backend_file.c_str(), err);
            }
        }
    }

    /* save the returned table as MILLENNIUM_PLUGIN_DEFINITION */
    lua_pushvalue(L, -1);
    lua_setglobal(L, "MILLENNIUM_PLUGIN_DEFINITION");

    if (!lua_istable(L, -1)) {
        fprintf(stderr, "[lua-host] main.lua must return a table\n");

        json err_resp = {
            { "type",  plugin_ipc::TYPE_RESPONSE      },
            { "id",    init_id                        },
            { "error", "main.lua must return a table" }
        };
        plugin_ipc::write_msg(fd, err_resp);
        lua_close(L);
        plugin_ipc::close_fd(fd);
        return 1;
    }

    /* send patches to parent */
    send_patches_to_parent(L);

    /* call on_load */
    lua_getfield(L, -1, "on_load");
    if (lua_isfunction(L, -1)) {
        if (lua_pcall(L, 0, 0, 0) != LUA_OK) {
            const char* err = lua_tostring(L, -1);
            log_lua_error("on_load error", err);
            lua_pop(L, 1);
        }
    } else {
        lua_pop(L, 1);
    }

    lua_pop(L, 1); /* pop the plugin definition table */

    /* respond to init request */
    {
        json init_resp = {
            { "type",   plugin_ipc::TYPE_RESPONSE },
            { "id",     init_id                   },
            { "result", { { "ok", true } }        }
        };
        plugin_ipc::write_msg(fd, init_resp);
    }

    /* enter event loop */
    rpc.run([L](const std::string& method, const json& params) -> json
    {
        if (method == plugin_ipc::parent_method::EVALUATE) {
            return handle_evaluate(L, params);
        }

        if (method == plugin_ipc::parent_method::ON_FRONTEND_LOADED) {
            return handle_frontend_loaded(L);
        }

        if (method == plugin_ipc::parent_method::GET_METRICS) {
            size_t heap_kb = static_cast<size_t>(lua_gc(L, LUA_GCCOUNT, 0));
            size_t heap_extra = static_cast<size_t>(lua_gc(L, LUA_GCCOUNTB, 0));
            return {
                { "heap_bytes", heap_kb * 1024 + heap_extra }
            };
        }

        if (method == plugin_ipc::parent_method::CONFIG_CHANGED) {
            std::string key = params.value("key", "");
            json value = params.value("value", json(nullptr));
            dispatch_config_change(L, key, value);
            return {
                { "ok", true }
            };
        }

        if (method == plugin_ipc::parent_method::SHUTDOWN) {
            auto result = handle_shutdown(L);
            /* after responding, the event loop will exit because the parent closes the socket */
            return result;
        }

        return {
            { "error", "unknown method: " + method }
        };
    });

    /* If the parent died without sending SHUTDOWN (crash, ExitProcess, etc.)
       the event loop exits on socket EOF.  Still call on_unload so the plugin
       gets a chance to persist state. */
    handle_shutdown(L);

    /* cleanup */
    lua_close(L);
    plugin_ipc::close_fd(fd);
    g_rpc = nullptr;
    g_L = nullptr;

    return 0;
}
