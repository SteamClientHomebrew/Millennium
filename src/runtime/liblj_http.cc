/*
 * ==================================================
 *   _____ _ _ _             _
 *  |     |_| | |___ ___ ___|_|_ _ _____
 *  | | | | | | | -_|   |   | | | |     |
 *  |_|_|_|_|_|___|_|_|_|_|_|___|_|_|_|
 *
 * ==================================================
 *
 * Copyright (c) 2023 - 2026. Project Millennium
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

#include <cstdlib>
#include <cstring>
#include <lua.hpp>

#include <curl/curl.h>

typedef struct
{
    char* data;
    size_t size;
} HTTPResponse;

static size_t WriteCallback(const void* contents, const size_t size, const size_t nmemb, HTTPResponse* response)
{
    const size_t realsize = size * nmemb;
    char* ptr = static_cast<char*>(realloc(response->data, response->size + realsize + 1));

    if (ptr == nullptr) {
        return 0; // Out of memory
    }

    response->data = ptr;
    memcpy(&(response->data[response->size]), contents, realsize);
    response->size += realsize;
    response->data[response->size] = 0;

    return realsize;
}

static size_t HeaderCallback(const char* buffer, const size_t size, const size_t nitems, void* userdata)
{
    lua_State* L = static_cast<lua_State*>(userdata);
    const size_t realsize = size * nitems;

    lua_pushstring(L, "response_headers");
    lua_gettable(L, LUA_REGISTRYINDEX);

    if (lua_istable(L, -1)) {
        char* header_line = static_cast<char*>(malloc(realsize + 1));
        memcpy(header_line, buffer, realsize);
        header_line[realsize] = '\0';

        char* end = header_line + strlen(header_line) - 1;
        while (end > header_line && (*end == '\r' || *end == '\n')) {
            *end = '\0';
            end--;
        }

        if (strlen(header_line) > 0 && strchr(header_line, ':') != nullptr) {
            char* colon = strchr(header_line, ':');
            *colon = '\0';
            const char* value = colon + 1;

            while (*value == ' ')
                value++;

            lua_pushstring(L, header_line);
            lua_pushstring(L, value);
            lua_settable(L, -3);
        }

        free(header_line);
    }

    lua_pop(L, 1);
    return realsize;
}

static int Lua_HttpRequest(lua_State* L)
{
    HTTPResponse response = { nullptr, 0 };
    curl_slist* headers = nullptr;

    const char* url = luaL_checkstring(L, 1);

    const char* method = "GET";
    const char* data = nullptr;
    long timeout = 30;
    int follow_redirects = 1;
    int verify_ssl = 1;
    const char* user_agent = "Millennium/1.0";
    const char* auth_user = nullptr;
    const char* auth_pass = nullptr;
    const char* proxy = nullptr;

    if (lua_istable(L, 2)) {
        lua_getfield(L, 2, "method");
        if (lua_isstring(L, -1)) {
            method = lua_tostring(L, -1);
        }
        lua_pop(L, 1);

        lua_getfield(L, 2, "data");
        if (lua_isstring(L, -1)) {
            data = lua_tostring(L, -1);
        }
        lua_pop(L, 1);

        lua_getfield(L, 2, "timeout");
        if (lua_isnumber(L, -1)) {
            timeout = lua_tointeger(L, -1);
        }
        lua_pop(L, 1);

        lua_getfield(L, 2, "follow_redirects");
        if (lua_isboolean(L, -1)) {
            follow_redirects = lua_toboolean(L, -1);
        }
        lua_pop(L, 1);

        lua_getfield(L, 2, "verify_ssl");
        if (lua_isboolean(L, -1)) {
            verify_ssl = lua_toboolean(L, -1);
        }
        lua_pop(L, 1);

        lua_getfield(L, 2, "user_agent");
        if (lua_isstring(L, -1)) {
            user_agent = lua_tostring(L, -1);
        }
        lua_pop(L, 1);

        lua_getfield(L, 2, "auth");
        if (lua_istable(L, -1)) {
            lua_getfield(L, -1, "user");
            if (lua_isstring(L, -1)) {
                auth_user = lua_tostring(L, -1);
            }
            lua_pop(L, 1);

            lua_getfield(L, -1, "pass");
            if (lua_isstring(L, -1)) {
                auth_pass = lua_tostring(L, -1);
            }
            lua_pop(L, 1);
        }
        lua_pop(L, 1);

        lua_getfield(L, 2, "proxy");
        if (lua_isstring(L, -1)) {
            proxy = lua_tostring(L, -1);
        }
        lua_pop(L, 1);

        lua_getfield(L, 2, "headers");
        if (lua_istable(L, -1)) {
            lua_pushnil(L);
            while (lua_next(L, -2) != 0) {
                if (lua_isstring(L, -2) && lua_isstring(L, -1)) {
                    const char* key = lua_tostring(L, -2);
                    const char* value = lua_tostring(L, -1);

                    const size_t header_len = strlen(key) + strlen(value) + 3;
                    char* header = static_cast<char*>(malloc(header_len));
                    snprintf(header, header_len, "%s: %s", key, value);

                    headers = curl_slist_append(headers, header);
                    free(header);
                }
                lua_pop(L, 1);
            }
        }
        lua_pop(L, 1);
    }

    CURL * curl = curl_easy_init();
    if (!curl) {
        lua_pushnil(L);
        lua_pushstring(L, "Failed to initialize curl");
        return 2;
    }

    curl_easy_setopt(curl, CURLOPT_URL, url);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteCallback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, timeout);
    curl_easy_setopt(curl, CURLOPT_USERAGENT, user_agent);
    curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, follow_redirects ? 1L : 0L);
    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, verify_ssl ? 1L : 0L);
    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, verify_ssl ? 2L : 0L);

    lua_newtable(L);
    lua_pushstring(L, "response_headers");
    lua_pushvalue(L, -2);
    lua_settable(L, LUA_REGISTRYINDEX);

    curl_easy_setopt(curl, CURLOPT_HEADERFUNCTION, HeaderCallback);
    curl_easy_setopt(curl, CURLOPT_HEADERDATA, L);

    if (strcmp(method, "POST") == 0) {
        curl_easy_setopt(curl, CURLOPT_POST, 1L);
        if (data) {
            curl_easy_setopt(curl, CURLOPT_POSTFIELDS, data);
            curl_easy_setopt(curl, CURLOPT_POSTFIELDSIZE, strlen(data));
        }
    } else if (strcmp(method, "PUT") == 0) {
        curl_easy_setopt(curl, CURLOPT_CUSTOMREQUEST, "PUT");
        if (data) {
            curl_easy_setopt(curl, CURLOPT_POSTFIELDS, data);
            curl_easy_setopt(curl, CURLOPT_POSTFIELDSIZE, strlen(data));
        }
    } else if (strcmp(method, "DELETE") == 0) {
        curl_easy_setopt(curl, CURLOPT_CUSTOMREQUEST, "DELETE");
    } else if (strcmp(method, "PATCH") == 0) {
        curl_easy_setopt(curl, CURLOPT_CUSTOMREQUEST, "PATCH");
        if (data) {
            curl_easy_setopt(curl, CURLOPT_POSTFIELDS, data);
            curl_easy_setopt(curl, CURLOPT_POSTFIELDSIZE, strlen(data));
        }
    } else if (strcmp(method, "HEAD") == 0) {
        curl_easy_setopt(curl, CURLOPT_NOBODY, 1L);
    } else if (strcmp(method, "OPTIONS") == 0) {
        curl_easy_setopt(curl, CURLOPT_CUSTOMREQUEST, "OPTIONS");
    }

    if (auth_user && auth_pass) {
        char* auth_string = static_cast<char*>(malloc(strlen(auth_user) + strlen(auth_pass) + 2));
        sprintf(auth_string, "%s:%s", auth_user, auth_pass);
        curl_easy_setopt(curl, CURLOPT_USERPWD, auth_string);
        curl_easy_setopt(curl, CURLOPT_HTTPAUTH, CURLAUTH_BASIC);
        free(auth_string);
    }

    if (proxy) {
        curl_easy_setopt(curl, CURLOPT_PROXY, proxy);
    }

    if (headers) {
        curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
    }

    const CURLcode res = curl_easy_perform(curl);

    long response_code = 0;
    curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &response_code);

    curl_slist_free_all(headers);
    curl_easy_cleanup(curl);

    lua_pushstring(L, "response_headers");
    lua_pushnil(L);
    lua_settable(L, LUA_REGISTRYINDEX);

    if (res != CURLE_OK) {
        if (response.data) {
            free(response.data);
        }
        lua_pushnil(L);
        lua_pushstring(L, curl_easy_strerror(res));
        return 2;
    }

    lua_newtable(L);
    lua_pushstring(L, "body");
    if (response.data) {
        lua_pushlstring(L, response.data, response.size);
        free(response.data);
    } else {
        lua_pushstring(L, "");
    }
    lua_settable(L, -3);

    lua_pushstring(L, "status");
    lua_pushinteger(L, response_code);
    lua_settable(L, -3);

    lua_pushstring(L, "headers");
    lua_pushvalue(L, -3);
    lua_settable(L, -3);

    lua_remove(L, -2);

    return 1;
}

static int Lua_HttpGet(lua_State* L)
{
    const char* url = luaL_checkstring(L, 1);

    lua_pushstring(L, url);

    lua_newtable(L);
    lua_pushstring(L, "method");
    lua_pushstring(L, "GET");
    lua_settable(L, -3);

    if (lua_istable(L, 2)) {
        lua_pushnil(L);
        while (lua_next(L, 2) != 0) {
            lua_pushvalue(L, -2);
            lua_insert(L, -2);
            lua_settable(L, -4);
        }
    }

    return Lua_HttpRequest(L);
}

static int Lua_HttpPost(lua_State* L)
{
    const char* url = luaL_checkstring(L, 1);
    const char* data = nullptr;

    if (lua_isstring(L, 2)) {
        data = lua_tostring(L, 2);
    }

    lua_pushstring(L, url);

    lua_newtable(L);
    lua_pushstring(L, "method");
    lua_pushstring(L, "POST");
    lua_settable(L, -3);

    if (data) {
        lua_pushstring(L, "data");
        lua_pushstring(L, data);
        lua_settable(L, -3);
    }

    if (lua_istable(L, 3)) {
        lua_pushnil(L);
        while (lua_next(L, 3) != 0) {
            lua_pushvalue(L, -2);
            lua_insert(L, -2);
            lua_settable(L, -4);
        }
    }

    return Lua_HttpRequest(L);
}

static int Lua_HttpPut(lua_State* L)
{
    const char* url = luaL_checkstring(L, 1);
    const char* data = nullptr;

    if (lua_isstring(L, 2)) {
        data = lua_tostring(L, 2);
    }

    lua_pushstring(L, url);

    lua_newtable(L);
    lua_pushstring(L, "method");
    lua_pushstring(L, "PUT");
    lua_settable(L, -3);

    if (data) {
        lua_pushstring(L, "data");
        lua_pushstring(L, data);
        lua_settable(L, -3);
    }

    if (lua_istable(L, 3)) {
        lua_pushnil(L);
        while (lua_next(L, 3) != 0) {
            lua_pushvalue(L, -2);
            lua_insert(L, -2);
            lua_settable(L, -4);
        }
    }

    return Lua_HttpRequest(L);
}

static int Lua_HttpDelete(lua_State* L)
{
    const char* url = luaL_checkstring(L, 1);

    lua_pushstring(L, url);

    lua_newtable(L);
    lua_pushstring(L, "method");
    lua_pushstring(L, "DELETE");
    lua_settable(L, -3);

    if (lua_istable(L, 2)) {
        lua_pushnil(L);
        while (lua_next(L, 2) != 0) {
            lua_pushvalue(L, -2);
            lua_insert(L, -2);
            lua_settable(L, -4);
        }
    }

    return Lua_HttpRequest(L);
}

static const luaL_Reg httpFunctions[] = {
    { "request", Lua_HttpRequest },
    { "get",     Lua_HttpGet     },
    { "post",    Lua_HttpPost    },
    { "put",     Lua_HttpPut     },
    { "delete",  Lua_HttpDelete  },
    { nullptr,      nullptr            }
};

extern "C" int Lua_OpenHttpLibrary(lua_State* L)
{
    curl_global_init(CURL_GLOBAL_DEFAULT);
    luaL_newlib(L, httpFunctions);
    return 1;
}
