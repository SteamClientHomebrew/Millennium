/**
 * ==================================================
 *   _____ _ _ _             _
 *  |     |_| | |___ ___ ___|_|_ _ _____
 *  | | | | | | | -_|   |   | | | |     |
 *  |_|_|_|_|_|_|___|_|_|_|_|_|___|_|_|_|
 *
 * ==================================================
 *
 * Copyright (c) 2025 Project Millennium
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

#include "hhx64/cef_def.h"
#include "hhx64/fread.h"
#include "hhx64/log.h"
#include "hhx64/urlp.h"

#include <assert.h>
#include <atomic>
#include <cstdlib>
#include <string.h>
#ifdef __linux__
#include <dlfcn.h>
#endif
#include <time.h>

#ifdef _WIN32
#define plat_strdup _strdup
#else
#define plat_strdup strdup
#endif

typedef struct
{
    cef_resource_request_handler_t handler;
    char* url;
    std::atomic<int> ref_count;
} steamloopback_request_handler_t;

typedef struct
{
    cef_resource_handler_t handler;
    char* url;
    char* data;
    size_t size;
    size_t pos;
    std::atomic<int> ref_count;
} steamloopback_resource_handler_t;

/** fwd decl */
int handle_loopback_request(const char* url, char** data, uint32_t* size);
void* create_steamloopback_resource_handler(const char* url);
extern "C" int find_file_matches(char* file_content, uint32_t size, char* local_path, char** out_file_content, uint32_t* out_file_size);

_cef_client_t* orig_c = nullptr;
_cef_request_handler_t* (*original_get_request_handler)(void*) = nullptr;
_cef_resource_request_handler_t* (*orig_get_resource)(void*, void*, void*, _cef_request_t*, int, int, void*, int*) = nullptr;

void CEF_CALLBACK lb_res_add_ref(void* base)
{
    steamloopback_resource_handler_t* handler = static_cast<steamloopback_resource_handler_t*>(base);
    handler->ref_count.fetch_add(1);
}

int CEF_CALLBACK lb_res_release(void* base)
{
    steamloopback_resource_handler_t* handler = static_cast<steamloopback_resource_handler_t*>(base);
    if (handler->ref_count.fetch_sub(1) == 1) {
        free(handler->url);
        free(handler->data);
        free(handler);
        return 1;
    }
    return 0;
}

int CEF_CALLBACK lb_res_has_one_ref(void* base)
{
    const steamloopback_resource_handler_t* handler = static_cast<steamloopback_resource_handler_t*>(base);
    return handler->ref_count.load() == 1;
}

int CEF_CALLBACK lb_res_has_at_least_one_ref(void* base)
{
    const steamloopback_resource_handler_t* handler = static_cast<steamloopback_resource_handler_t*>(base);
    return handler->ref_count.load() >= 1;
}

int CEF_CALLBACK lb_open(void* self, void* request, int* handle_request, void* callback)
{
    *handle_request = 1;
    return 1;
}

void CEF_CALLBACK lb_req_add_ref(void* base)
{
    steamloopback_request_handler_t* handler = static_cast<steamloopback_request_handler_t*>(base);
    handler->ref_count.fetch_add(1);
}

int CEF_CALLBACK lb_req_release(void* base)
{
    steamloopback_request_handler_t* handler = static_cast<steamloopback_request_handler_t*>(base);
    if (handler->ref_count.fetch_sub(1) == 1) {
        free(handler->url);
        free(handler);
        return 1;
    }
    return 0;
}

int CEF_CALLBACK lb_req_has_one_ref(void* base)
{
    const steamloopback_request_handler_t* handler = static_cast<steamloopback_request_handler_t*>(base);
    return handler->ref_count.load() == 1;
}

int CEF_CALLBACK lb_req_has_at_least_one_ref(void* base)
{
    const steamloopback_request_handler_t* handler = static_cast<steamloopback_request_handler_t*>(base);
    return handler->ref_count.load() >= 1;
}

void* CEF_CALLBACK lb_on_before_load_hdl(void* self, void* browser, void* frame, void* request, void* callback)
{
    return reinterpret_cast<void*>(1); /** continue */
}

void* CEF_CALLBACK lb_get_resource_hdl(void* self, void* browser, void* frame, void* request)
{
    const steamloopback_request_handler_t* handler = static_cast<steamloopback_request_handler_t*>(self);
    return create_steamloopback_resource_handler(handler->url);
}

static void set_cef_header(_cef_response_t* response, const char* name, const char* value)
{
    CEF_LAZY_LOAD(cef_string_utf8_set, int, (const char*, size_t, cef_string_utf8_t*, int));
    CEF_LAZY_LOAD(cef_string_utf8_clear, void, (cef_string_utf8_t*));

    cef_string_t name_str = {}, value_str = {};
    lazy_cef_string_utf8_set(name, strlen(name), &name_str, 1);
    lazy_cef_string_utf8_set(value, strlen(value), &value_str, 1);
    response->set_header_by_name(response, &name_str, &value_str, 1);
    lazy_cef_string_utf8_clear(&name_str);
    lazy_cef_string_utf8_clear(&value_str);
}

void CEF_CALLBACK lb_get_response_headers(_cef_resource_handler_t* self, _cef_response_t* response, int64_t* response_length, cef_string_t* redirectUrl)
{
    const steamloopback_resource_handler_t* handler = reinterpret_cast<steamloopback_resource_handler_t*>(self);
    CEF_LAZY_LOAD(cef_string_utf8_set, int, (const char*, size_t, cef_string_utf8_t*, int));
    CEF_LAZY_LOAD(cef_string_utf8_clear, void, (cef_string_utf8_t*));

    assert(strstr(handler->url, ".js"));
    response->set_status(response, 200);

    cef_string_t mime_type = {};
    lazy_cef_string_utf8_set("application/javascript", 22, &mime_type, 1);
    response->set_mime_type(response, &mime_type);
    lazy_cef_string_utf8_clear(&mime_type);

    set_cef_header(response, "X-Millennium-Hooked", "1");
    set_cef_header(response, "Cache-Control", "no-cache"); /** steam uses no-cache, and they internally health-check for this header */

    *response_length = handler->size;
}

int CEF_CALLBACK lb_read(void* self, void* data_out, const int bytes_to_read, int* bytes_read, void* callback)
{
    steamloopback_resource_handler_t* handler = static_cast<steamloopback_resource_handler_t*>(self);
    const size_t available = handler->size - handler->pos;
    *bytes_read = static_cast<int>((bytes_to_read < available) ? bytes_to_read : available);

    if (*bytes_read > 0) {
        memcpy(data_out, handler->data + handler->pos, *bytes_read);
        handler->pos += *bytes_read;
    }

    return (*bytes_read > 0);
}

cef_resource_request_handler_t* create_steamloopback_request_handler(const char* url)
{
    steamloopback_request_handler_t* handler = static_cast<steamloopback_request_handler_t*>(calloc(1, sizeof(steamloopback_request_handler_t)));

    handler->url = plat_strdup(url);
    handler->ref_count.store(1);

    char* p = handler->handler._base;
    *reinterpret_cast<size_t*>(p + 0x0) = sizeof(cef_resource_request_handler_t);
    *reinterpret_cast<void (**)(void*)>(p + sizeof(size_t) + sizeof(void*) * 0x0) = static_cast<void (*)(void*)>(lb_req_add_ref);
    *reinterpret_cast<int (**)(void*)>(p + sizeof(size_t) + sizeof(void*) * 0x1) = static_cast<int (*)(void*)>(lb_req_release);
    *reinterpret_cast<int (**)(void*)>(p + sizeof(size_t) + sizeof(void*) * 0x2) = static_cast<int (*)(void*)>(lb_req_has_one_ref);
    *reinterpret_cast<int (**)(void*)>(p + sizeof(size_t) + sizeof(void*) * 0x3) = static_cast<int (*)(void*)>(lb_req_has_at_least_one_ref);

    handler->handler.on_before_resource_load = lb_on_before_load_hdl;
    handler->handler.get_resource_handler = lb_get_resource_hdl;
    handler->handler._1 = nullptr;
    handler->handler._2 = nullptr;
    handler->handler._3 = nullptr;
    handler->handler._4 = nullptr;
    handler->handler._5 = nullptr;
    handler->handler._6 = nullptr;
    return &handler->handler;
}

int handle_loopback_request(const char* url, char** data, uint32_t* size)
{
    *data = nullptr;
    *size = 0;
    char* out_file_data = nullptr;
    uint32_t out_file_size = 0;

    char *local_path = nullptr, *local_short_path = nullptr;
    fread_data file_data = { nullptr };
    uint8_t* buf = nullptr;

    if (urlp_path_from_lb(url, &local_path, &local_short_path) != 0) {
        log_error("Failed to get path from URL: %s\n", url);
        goto cleanup;
    }

    file_data = fread_file(local_path);
    if (!file_data.content) {
        log_error("Failed to read file: %s\n", local_path);
        goto cleanup;
    }

    find_file_matches(file_data.content, file_data.size, local_short_path, &out_file_data, &out_file_size);
    *data = out_file_data;
    *size = out_file_size;
    file_data.content = nullptr; /** transfer ownership */

cleanup:
    free(file_data.content);
    free(buf);
    free(local_path);
    free(local_short_path);

    return (*data != nullptr) ? 0 : -1;
}

void* create_steamloopback_resource_handler(const char* url)
{
#ifdef _WIN32
    LARGE_INTEGER freq, start, end;
    QueryPerformanceFrequency(&freq);
    QueryPerformanceCounter(&start);
#else
    timespec start = { 0 }, end = { 0 };
    clock_gettime(CLOCK_MONOTONIC, &start);
#endif
    steamloopback_resource_handler_t* handler = static_cast<steamloopback_resource_handler_t*>(calloc(1, sizeof(steamloopback_resource_handler_t)));

    char* data = nullptr;
    uint32_t size = 0;
    if (handle_loopback_request(url, &data, &size) != 0 || !data) {
        log_error("Failed to handle loopback request\n");
        free(handler);
        return nullptr;
    }

#ifdef _WIN32
    QueryPerformanceCounter(&end);
    double elapsed = (double)(end.QuadPart - start.QuadPart) * 1000.0 / freq.QuadPart;
#else
    clock_gettime(CLOCK_MONOTONIC, &end);
    const double elapsed = ((end.tv_sec - start.tv_sec) + (end.tv_nsec - start.tv_nsec) / 1e9) * 1000.0;
#endif
    log_info("[hook] took %.3f ms to process %s\n", elapsed, url);

    handler->ref_count.store(1);

    handler->data = data;
    handler->size = size;
    handler->url = plat_strdup(url);
    handler->pos = 0;

    /** setup handler base */
    char* p = handler->handler._base;
    *reinterpret_cast<size_t*>(p + 0x0) = sizeof(cef_resource_handler_t);
    *reinterpret_cast<void (**)(void*)>(p + sizeof(size_t) + sizeof(void*) * 0) = static_cast<void (*)(void*)>(lb_res_add_ref);
    *reinterpret_cast<int (**)(void*)>(p + sizeof(size_t) + sizeof(void*) * 1) = static_cast<int (*)(void*)>(lb_res_release);
    *reinterpret_cast<int (**)(void*)>(p + sizeof(size_t) + sizeof(void*) * 2) = static_cast<int (*)(void*)>(lb_res_has_one_ref);
    *reinterpret_cast<int (**)(void*)>(p + sizeof(size_t) + sizeof(void*) * 3) = static_cast<int (*)(void*)>(lb_res_has_at_least_one_ref);

    handler->handler.get_response_headers = lb_get_response_headers;
    handler->handler.open = lb_open;
    handler->handler.read = lb_read;
    handler->handler.cancel = nullptr;
    handler->handler.process_request = nullptr;
    handler->handler.skip = nullptr;
    return &handler->handler;
}
