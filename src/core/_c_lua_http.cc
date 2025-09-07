#include <curl/curl.h>
#include <lua.hpp>
#include <stdlib.h>
#include <string.h>

typedef struct
{
    char* data;
    size_t size;
} HTTPResponse;

static size_t WriteCallback(void* contents, size_t size, size_t nmemb, HTTPResponse* response)
{
    size_t realsize = size * nmemb;
    char* ptr = (char*)realloc(response->data, response->size + realsize + 1);

    if (ptr == NULL)
    {
        return 0; // Out of memory
    }

    response->data = ptr;
    memcpy(&(response->data[response->size]), contents, realsize);
    response->size += realsize;
    response->data[response->size] = 0;

    return realsize;
}

static size_t HeaderCallback(char* buffer, size_t size, size_t nitems, void* userdata)
{
    lua_State* L = (lua_State*)userdata;
    size_t realsize = size * nitems;

    lua_pushstring(L, "response_headers");
    lua_gettable(L, LUA_REGISTRYINDEX);

    if (lua_istable(L, -1))
    {
        char* header_line = (char*)malloc(realsize + 1);
        memcpy(header_line, buffer, realsize);
        header_line[realsize] = '\0';

        char* end = header_line + strlen(header_line) - 1;
        while (end > header_line && (*end == '\r' || *end == '\n'))
        {
            *end = '\0';
            end--;
        }

        if (strlen(header_line) > 0 && strchr(header_line, ':') != NULL)
        {
            char* colon = strchr(header_line, ':');
            *colon = '\0';
            char* value = colon + 1;

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
    CURL* curl;
    CURLcode res;
    HTTPResponse response = {0};
    struct curl_slist* headers = NULL;

    const char* url = luaL_checkstring(L, 1);

    const char* method = "GET";
    const char* data = NULL;
    long timeout = 30;
    int follow_redirects = 1;
    int verify_ssl = 1;
    const char* user_agent = "Millennium/1.0";
    const char* auth_user = NULL;
    const char* auth_pass = NULL;
    const char* proxy = NULL;

    if (lua_istable(L, 2))
    {
        lua_getfield(L, 2, "method");
        if (lua_isstring(L, -1))
        {
            method = lua_tostring(L, -1);
        }
        lua_pop(L, 1);

        lua_getfield(L, 2, "data");
        if (lua_isstring(L, -1))
        {
            data = lua_tostring(L, -1);
        }
        lua_pop(L, 1);

        lua_getfield(L, 2, "timeout");
        if (lua_isnumber(L, -1))
        {
            timeout = lua_tointeger(L, -1);
        }
        lua_pop(L, 1);

        lua_getfield(L, 2, "follow_redirects");
        if (lua_isboolean(L, -1))
        {
            follow_redirects = lua_toboolean(L, -1);
        }
        lua_pop(L, 1);

        lua_getfield(L, 2, "verify_ssl");
        if (lua_isboolean(L, -1))
        {
            verify_ssl = lua_toboolean(L, -1);
        }
        lua_pop(L, 1);

        lua_getfield(L, 2, "user_agent");
        if (lua_isstring(L, -1))
        {
            user_agent = lua_tostring(L, -1);
        }
        lua_pop(L, 1);

        lua_getfield(L, 2, "auth");
        if (lua_istable(L, -1))
        {
            lua_getfield(L, -1, "user");
            if (lua_isstring(L, -1))
            {
                auth_user = lua_tostring(L, -1);
            }
            lua_pop(L, 1);

            lua_getfield(L, -1, "pass");
            if (lua_isstring(L, -1))
            {
                auth_pass = lua_tostring(L, -1);
            }
            lua_pop(L, 1);
        }
        lua_pop(L, 1);

        lua_getfield(L, 2, "proxy");
        if (lua_isstring(L, -1))
        {
            proxy = lua_tostring(L, -1);
        }
        lua_pop(L, 1);

        lua_getfield(L, 2, "headers");
        if (lua_istable(L, -1))
        {
            lua_pushnil(L);
            while (lua_next(L, -2) != 0)
            {
                if (lua_isstring(L, -2) && lua_isstring(L, -1))
                {
                    const char* key = lua_tostring(L, -2);
                    const char* value = lua_tostring(L, -1);

                    size_t header_len = strlen(key) + strlen(value) + 3;
                    char* header = (char*)malloc(header_len);
                    snprintf(header, header_len, "%s: %s", key, value);

                    headers = curl_slist_append(headers, header);
                    free(header);
                }
                lua_pop(L, 1);
            }
        }
        lua_pop(L, 1);
    }

    curl = curl_easy_init();
    if (!curl)
    {
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

    if (strcmp(method, "POST") == 0)
    {
        curl_easy_setopt(curl, CURLOPT_POST, 1L);
        if (data)
        {
            curl_easy_setopt(curl, CURLOPT_POSTFIELDS, data);
            curl_easy_setopt(curl, CURLOPT_POSTFIELDSIZE, strlen(data));
        }
    }
    else if (strcmp(method, "PUT") == 0)
    {
        curl_easy_setopt(curl, CURLOPT_CUSTOMREQUEST, "PUT");
        if (data)
        {
            curl_easy_setopt(curl, CURLOPT_POSTFIELDS, data);
            curl_easy_setopt(curl, CURLOPT_POSTFIELDSIZE, strlen(data));
        }
    }
    else if (strcmp(method, "DELETE") == 0)
    {
        curl_easy_setopt(curl, CURLOPT_CUSTOMREQUEST, "DELETE");
    }
    else if (strcmp(method, "PATCH") == 0)
    {
        curl_easy_setopt(curl, CURLOPT_CUSTOMREQUEST, "PATCH");
        if (data)
        {
            curl_easy_setopt(curl, CURLOPT_POSTFIELDS, data);
            curl_easy_setopt(curl, CURLOPT_POSTFIELDSIZE, strlen(data));
        }
    }
    else if (strcmp(method, "HEAD") == 0)
    {
        curl_easy_setopt(curl, CURLOPT_NOBODY, 1L);
    }
    else if (strcmp(method, "OPTIONS") == 0)
    {
        curl_easy_setopt(curl, CURLOPT_CUSTOMREQUEST, "OPTIONS");
    }

    if (auth_user && auth_pass)
    {
        char* auth_string = (char*)malloc(strlen(auth_user) + strlen(auth_pass) + 2);
        sprintf(auth_string, "%s:%s", auth_user, auth_pass);
        curl_easy_setopt(curl, CURLOPT_USERPWD, auth_string);
        curl_easy_setopt(curl, CURLOPT_HTTPAUTH, CURLAUTH_BASIC);
        free(auth_string);
    }

    if (proxy)
    {
        curl_easy_setopt(curl, CURLOPT_PROXY, proxy);
    }

    if (headers)
    {
        curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
    }

    res = curl_easy_perform(curl);

    long response_code = 0;
    curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &response_code);

    curl_slist_free_all(headers);
    curl_easy_cleanup(curl);

    lua_pushstring(L, "response_headers");
    lua_pushnil(L);
    lua_settable(L, LUA_REGISTRYINDEX);

    if (res != CURLE_OK)
    {
        if (response.data)
        {
            free(response.data);
        }
        lua_pushnil(L);
        lua_pushstring(L, curl_easy_strerror(res));
        return 2;
    }

    lua_newtable(L);
    lua_pushstring(L, "body");
    if (response.data)
    {
        lua_pushlstring(L, response.data, response.size);
        free(response.data);
    }
    else
    {
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

    if (lua_istable(L, 2))
    {
        lua_pushnil(L);
        while (lua_next(L, 2) != 0)
        {
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
    const char* data = NULL;

    if (lua_isstring(L, 2))
    {
        data = lua_tostring(L, 2);
    }

    lua_pushstring(L, url);

    lua_newtable(L);
    lua_pushstring(L, "method");
    lua_pushstring(L, "POST");
    lua_settable(L, -3);

    if (data)
    {
        lua_pushstring(L, "data");
        lua_pushstring(L, data);
        lua_settable(L, -3);
    }

    if (lua_istable(L, 3))
    {
        lua_pushnil(L);
        while (lua_next(L, 3) != 0)
        {
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
    const char* data = NULL;

    if (lua_isstring(L, 2))
    {
        data = lua_tostring(L, 2);
    }

    lua_pushstring(L, url);

    lua_newtable(L);
    lua_pushstring(L, "method");
    lua_pushstring(L, "PUT");
    lua_settable(L, -3);

    if (data)
    {
        lua_pushstring(L, "data");
        lua_pushstring(L, data);
        lua_settable(L, -3);
    }

    if (lua_istable(L, 3))
    {
        lua_pushnil(L);
        while (lua_next(L, 3) != 0)
        {
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

    if (lua_istable(L, 2))
    {
        lua_pushnil(L);
        while (lua_next(L, 2) != 0)
        {
            lua_pushvalue(L, -2);
            lua_insert(L, -2);
            lua_settable(L, -4);
        }
    }

    return Lua_HttpRequest(L);
}

static const luaL_Reg httpFunctions[] = {
    {"request", Lua_HttpRequest},
    {"get",     Lua_HttpGet    },
    {"post",    Lua_HttpPost   },
    {"put",     Lua_HttpPut    },
    {"delete",  Lua_HttpDelete },
    {NULL,      NULL           }
};

extern "C" int Lua_OpenHttpLibrary(lua_State* L)
{
    curl_global_init(CURL_GLOBAL_DEFAULT);
    luaL_newlib(L, httpFunctions);
    return 1;
}