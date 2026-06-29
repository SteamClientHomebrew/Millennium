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

/**
 * This module exists for one sole reason. To enable support for millennium's request hook on steamloopback.host requests,
 * we actually have to internally hook and block steam from even serving it in the first place. This is because steams
 * internal hooks take precedence over CDP hooks. Since resource requests handlers (the api behind)
 * steamloopback.host intercept the request before its sent to the chrome devtools protocol, we need to intercept it here,
 * block it, and tell CEF to forward the message to the browser instead of letting steam handle it.
 * This way we can successfully hook steamloopback.host requests with Fetch.enable as the internal interception no longer has
 * precedence as its no-opped.
 */

#include "instrumentation/chromium.h"
#include "instrumentation/patch_engine.h"

#ifdef _WIN32
#define plat_strdup _strdup
#else
#define plat_strdup strdup
#endif

#include <limits.h>
#include <atomic>
#include <cstdarg>
#include <cstdio>

static void fatal(const char* fmt, ...)
{
    va_list args;
    va_start(args, fmt);
    fprintf(stderr, "\033[31m");
    vfprintf(stderr, fmt, args);
    fprintf(stderr, "\033[0m\n");
    va_end(args);
}

#ifdef __linux__
#include <linux/limits.h>
#include <dlfcn.h>
#include <link.h>
#include <cstring>
#include <sys/mman.h>
#include <unistd.h>
#define SNARE_STATIC
#define SNARE_IMPLEMENTATION
#include "libsnare.h"

extern "C" int tramp_cef_browser_host_create_browser(const void*, struct _cef_client_t*, void*, const void*, void*, void*);
static snare_inline_t g_cef_hook = nullptr;

static void page_rx(void* addr)
{
    long ps = sysconf(_SC_PAGESIZE);
    mprotect(reinterpret_cast<void*>(reinterpret_cast<uintptr_t>(addr) & ~(ps - 1)), ps, PROT_READ | PROT_EXEC);
}

static void page_rwx(void* addr)
{
    long ps = sysconf(_SC_PAGESIZE);
    mprotect(reinterpret_cast<void*>(reinterpret_cast<uintptr_t>(addr) & ~(ps - 1)), ps, PROT_READ | PROT_WRITE | PROT_EXEC);
}

static int find_cef_fn(struct dl_phdr_info* info, size_t, void* data)
{
    if (!info->dlpi_name || !strstr(info->dlpi_name, "libcef")) return 0;

    void* h = dlopen(info->dlpi_name, RTLD_NOLOAD | RTLD_NOW);
    if (!h) return 0;

    void** out = static_cast<void**>(data);
    *out = dlsym(h, "cef_browser_host_create_browser");
    dlclose(h);

    return *out ? 1 : 0; /* stop iterating once found */
}

static void install_hook(void)
{
    void* cef_fn = nullptr;
    dl_iterate_phdr(find_cef_fn, &cef_fn);

    if (!cef_fn) {
        fatal("hook: cef_browser_host_create_browser not found in libcef");
        return;
    }

    g_cef_hook = snare_inline_new(cef_fn, reinterpret_cast<void*>(tramp_cef_browser_host_create_browser));
    if (!g_cef_hook) {
        fatal("hook: snare_inline_new failed");
        return;
    }

    if (snare_inline_install(g_cef_hook) < 0) {
        fatal("hook: snare_inline_install failed");
        snare_inline_free(g_cef_hook);
        g_cef_hook = nullptr;
        return;
    }

    page_rx(cef_fn);
}

__attribute__((constructor)) static void init(void)
{
    install_hook();
}
#endif

#ifdef _WIN32
#define SNARE_STATIC
#define SNARE_IMPLEMENTATION
#ifdef __clang__
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wdeprecated-declarations"
#endif
#include "libsnare.h"
#ifdef __clang__
#pragma clang diagnostic pop
#endif
extern snare_inline_t g_win32_cef_hook;
#endif

extern cef_resource_request_handler_t* create_steamloopback_request_handler(const char* url);
extern struct _cef_client_t* orig_c;
extern struct _cef_request_handler_t* (*original_get_request_handler)(void*);
extern struct _cef_resource_request_handler_t* (*orig_get_resource)(void*, void*, void*, struct _cef_request_t*, int, int, void*, int*);

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
    const char* mime_type;
    std::atomic<int> ref_count;
} steamloopback_resource_handler_t;

void* create_steamloopback_resource_handler(const char* url);

struct _cef_client_t* orig_c = NULL;
struct _cef_request_handler_t* (*original_get_request_handler)(void*) = NULL;
struct _cef_resource_request_handler_t* (*orig_get_resource)(void*, void*, void*, struct _cef_request_t*, int, int, void*, int*) = NULL;

void CEF_CALLBACK lb_res_add_ref(void* base)
{
    steamloopback_resource_handler_t* handler = (steamloopback_resource_handler_t*)base;
    handler->ref_count.fetch_add(1);
}

int CEF_CALLBACK lb_res_release(void* base)
{
    steamloopback_resource_handler_t* handler = (steamloopback_resource_handler_t*)base;
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
    steamloopback_resource_handler_t* handler = (steamloopback_resource_handler_t*)base;
    return handler->ref_count.load() == 1;
}

int CEF_CALLBACK lb_res_has_at_least_one_ref(void* base)
{
    steamloopback_resource_handler_t* handler = (steamloopback_resource_handler_t*)base;
    return handler->ref_count.load() >= 1;
}

int CEF_CALLBACK lb_open(void* self, void* request, int* handle_request, void* callback)
{
    *handle_request = 1;
    return 1;
}

void CEF_CALLBACK lb_req_add_ref(void* base)
{
    steamloopback_request_handler_t* handler = (steamloopback_request_handler_t*)base;
    handler->ref_count.fetch_add(1);
}

int CEF_CALLBACK lb_req_release(void* base)
{
    steamloopback_request_handler_t* handler = (steamloopback_request_handler_t*)base;
    if (handler->ref_count.fetch_sub(1) == 1) {
        free(handler->url);
        free(handler);
        return 1;
    }
    return 0;
}

int CEF_CALLBACK lb_req_has_one_ref(void* base)
{
    steamloopback_request_handler_t* handler = (steamloopback_request_handler_t*)base;
    return handler->ref_count.load() == 1;
}

int CEF_CALLBACK lb_req_has_at_least_one_ref(void* base)
{
    steamloopback_request_handler_t* handler = (steamloopback_request_handler_t*)base;
    return handler->ref_count.load() >= 1;
}

void* CEF_CALLBACK lb_on_before_load_hdl(void* self, void* browser, void* frame, void* request, void* callback)
{
    return (void*)1; /** continue */
}

void* CEF_CALLBACK lb_get_resource_hdl(void* self, void* browser, void* frame, void* request)
{
    steamloopback_request_handler_t* handler = (steamloopback_request_handler_t*)self;
    return (void*)create_steamloopback_resource_handler(handler->url);
}

static void set_cef_header(struct _cef_response_t* response, const char* name, const char* value)
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

void CEF_CALLBACK lb_get_response_headers(struct _cef_resource_handler_t* self, struct _cef_response_t* response, int64_t* response_length, cef_string_t* redirectUrl)
{
    steamloopback_resource_handler_t* handler = (steamloopback_resource_handler_t*)self;
    CEF_LAZY_LOAD(cef_string_utf8_set, int, (const char*, size_t, cef_string_utf8_t*, int));
    CEF_LAZY_LOAD(cef_string_utf8_clear, void, (cef_string_utf8_t*));

    response->set_status(response, 200);

    const char* mime = handler->mime_type ? handler->mime_type : "application/octet-stream";
    cef_string_t mime_str = {};
    lazy_cef_string_utf8_set(mime, strlen(mime), &mime_str, 1);
    response->set_mime_type(response, &mime_str);
    lazy_cef_string_utf8_clear(&mime_str);

    set_cef_header(response, "X-Millennium-Hooked", "1");
    set_cef_header(response, "Cache-Control", "no-cache"); /** steam uses no-cache, and they internally health-check for this header */

    *response_length = handler->size;
}

int CEF_CALLBACK lb_read(void* self, void* data_out, int bytes_to_read, int* bytes_read, void* callback)
{
    steamloopback_resource_handler_t* handler = (steamloopback_resource_handler_t*)self;
    size_t available = handler->size - handler->pos;
    *bytes_read = (int)((bytes_to_read < available) ? bytes_to_read : available);

    if (*bytes_read > 0) {
        memcpy(data_out, handler->data + handler->pos, *bytes_read);
        handler->pos += *bytes_read;
    }

    return (*bytes_read > 0);
}

cef_resource_request_handler_t* create_steamloopback_request_handler(const char* url)
{
    steamloopback_request_handler_t* handler = (steamloopback_request_handler_t*)calloc(1, sizeof(steamloopback_request_handler_t));

    handler->url = plat_strdup(url);
    handler->ref_count.store(1);

    char* p = handler->handler._base;
    *(size_t*)(p + 0x0) = sizeof(cef_resource_request_handler_t);
    *(void (**)(void*))(p + sizeof(size_t) + sizeof(void*) * 0x0) = (void (*)(void*))lb_req_add_ref;
    *(int (**)(void*))(p + sizeof(size_t) + sizeof(void*) * 0x1) = (int (*)(void*))lb_req_release;
    *(int (**)(void*))(p + sizeof(size_t) + sizeof(void*) * 0x2) = (int (*)(void*))lb_req_has_one_ref;
    *(int (**)(void*))(p + sizeof(size_t) + sizeof(void*) * 0x3) = (int (*)(void*))lb_req_has_at_least_one_ref;

    handler->handler.on_before_resource_load = lb_on_before_load_hdl;
    handler->handler.get_resource_handler = lb_get_resource_hdl;
    handler->handler._1 = NULL;
    handler->handler._2 = NULL;
    handler->handler._3 = NULL;
    handler->handler._4 = NULL;
    handler->handler._5 = NULL;
    handler->handler._6 = NULL;
    return &handler->handler;
}

void* create_steamloopback_resource_handler(const char* url)
{
    steamloopback_resource_handler_t* handler = (steamloopback_resource_handler_t*)calloc(1, sizeof(steamloopback_resource_handler_t));

    char* data = NULL;
    uint32_t size = 0;
    const char* mime_type = NULL;
    if (lb_handle_request(url, &data, &size, &mime_type) != 0 || !data) {
        free(handler);
        return NULL;
    }

    handler->ref_count.store(1);
    handler->data = data;
    handler->size = size;
    handler->mime_type = mime_type;
    handler->url = plat_strdup(url);
    handler->pos = 0;

    char* p = handler->handler._base;
    *(size_t*)(p + 0x0) = sizeof(cef_resource_handler_t);
    *(void (**)(void*))(p + sizeof(size_t) + sizeof(void*) * 0) = (void (*)(void*))lb_res_add_ref;
    *(int (**)(void*))(p + sizeof(size_t) + sizeof(void*) * 1) = (int (*)(void*))lb_res_release;
    *(int (**)(void*))(p + sizeof(size_t) + sizeof(void*) * 2) = (int (*)(void*))lb_res_has_one_ref;
    *(int (**)(void*))(p + sizeof(size_t) + sizeof(void*) * 3) = (int (*)(void*))lb_res_has_at_least_one_ref;

    handler->handler.get_response_headers = lb_get_response_headers;
    handler->handler.open = lb_open;
    handler->handler.read = lb_read;
    handler->handler.cancel = NULL;
    handler->handler.process_request = NULL;
    handler->handler.skip = NULL;
    return &handler->handler;
}

/**
 * hook resource request handler to block steamloopback.host .js requests.
 *
 * This function wraps cef get_resource_request_handler to intercept all
 * resource requests. Requests to steamloopback.host .js files are blocked by
 * returning our own handler, preventing Steam's internal handlers from serving
 * the content so we can apply plugin patches.
 */
void* hooked_get_resource(void* _1, void* _2, void* _3, struct _cef_request_t* request, int _5, int _6, void* _7, int* _8)
{
    CEF_LAZY_LOAD(cef_string_userfree_utf8_free, void, (cef_string_userfree_utf8_t));
    cef_string_userfree_t url = request->get_url(request);

    if (url && url->str && lb_url_is_patchable(url->str) && lb_url_has_local_file(url->str)) {
        /** call Steam's handler to trigger any internal setup, then discard the result */
        if (orig_get_resource) {
            void* steam_handler = orig_get_resource(_1, _2, _3, request, _5, _6, _7, _8);
            if (steam_handler) {
                /** vtable: [size_t size] [add_ref] [release] ... */
                char* base = (char*)steam_handler;
                int (*release_fn)(void*) = *(int (**)(void*))(base + sizeof(size_t) + sizeof(void*));
                if (release_fn) release_fn(steam_handler);
            }
        }

        cef_resource_request_handler_t* custom_handler = create_steamloopback_request_handler(url->str);
        if (lazy_cef_string_userfree_utf8_free) lazy_cef_string_userfree_utf8_free((cef_string_userfree_utf8_t)url);
        return custom_handler;
    }

    if (url && lazy_cef_string_userfree_utf8_free) {
        lazy_cef_string_userfree_utf8_free((cef_string_userfree_utf8_t)url);
    }

    return orig_get_resource(_1, _2, _3, request, _5, _6, _7, _8);
}

/**
 * hook the request handler creation to hook resource request handling.
 *
 * This function wraps get_request_handler to inject our hooked version of
 * get_resource_request_handler into each request handler instance that
 * Steam creates. (not sure why they have multiple handlers for the same type of thing,
 * but maybe im just dumb)
 */
struct _cef_request_handler_t* hooked_get_request_handler(void* self)
{
    struct _cef_request_handler_t* handler = original_get_request_handler(self);
    if (!handler || !handler->get_resource_request_handler) {
        return handler;
    }

    /** save the first original request handler we find (not sure if this will cause issues, I guess we will have to wait and see) */
    if (!orig_get_resource) {
        orig_get_resource = reinterpret_cast<decltype(orig_get_resource)>(handler->get_resource_request_handler);
    }

    /** redirect the resource request handler to our hooked one */
    handler->get_resource_request_handler = reinterpret_cast<decltype(handler->get_resource_request_handler)>(hooked_get_resource);
    return handler;
}

/**
 * hooked cef_browser_host_create_browser.
 * as this module is loaded via LD_PRELOAD, its procedural link its directly replaced on the IAT/GOT inline.
 * thankfully since steam doesn't statically link against cef (not even sure if its possible), it allows us to trampoline
 * calls it makes to cef.
 */
extern "C" int tramp_cef_browser_host_create_browser(const void* _1, struct _cef_client_t* c, void* _3, const void* _4, void* _5, void* _6)
{
    /** overwrite steams get request handler they provide. */
    if (c && c->get_request_handler && !orig_c) {
        orig_c = c;
        original_get_request_handler = c->get_request_handler;
        c->get_request_handler = reinterpret_cast<decltype(c->get_request_handler)>(hooked_get_request_handler);
    }

#ifdef _WIN32
    auto orig = reinterpret_cast<int(__cdecl*)(const void*, struct _cef_client_t*, void*, const void*, void*, void*)>(snare_inline_get_trampoline(g_win32_cef_hook));
    return orig(_1, c, _3, _4, _5, _6);
#elif __linux__
    if (!g_cef_hook) {
        fatal("cef_browser_host_create_browser: hook not installed, call dropped");
        return 0;
    }

    auto orig = reinterpret_cast<decltype(&tramp_cef_browser_host_create_browser)>(snare_inline_get_trampoline(g_cef_hook));
    return orig(_1, c, _3, _4, _5, _6);
#else
    return 0;
#endif
}

#if defined(_WIN32)
#define fn(x) #x

/** the function name we want to tramp */
#define hook_fn_name fn(cef_browser_host_create_browser)
#define hook_target_dll "libcef.dll"

void* get_module_base()
{
    HMODULE libcef_base_address = nullptr;
    while (true) {
        if ((libcef_base_address = GetModuleHandleA(hook_target_dll))) break;
        Sleep(100);
    }
    return libcef_base_address;
}

void* get_fn_address()
{
    void* libcef_base_address = get_module_base();
    return reinterpret_cast<void*>(GetProcAddress((HMODULE)libcef_base_address, hook_fn_name));
}

static snare_inline_t g_win32_cef_hook = nullptr;

/**
 * called when the hook is loaded into the web-helper on windows
 * as we aren't using LD_PRELOAD on windows, we have to use snare to trampoline
 * we don't have precedence over the load order in IAT/GOT like on linux/macOS
 */
void win32_initialize_trampoline()
{
    void* proc = get_fn_address();
    if (!proc) {
        fatal("ordinate %s not found in %s", hook_fn_name, hook_target_dll);
        return;
    }

    g_win32_cef_hook = snare_inline_new(proc, reinterpret_cast<void*>(&tramp_cef_browser_host_create_browser));
    if (!g_win32_cef_hook) {
        fatal("failed to create hook on %s...", hook_fn_name);
        return;
    }

    if (snare_inline_install(g_win32_cef_hook) < 0) {
        fatal("failed to install hook on %s...", hook_fn_name);
        snare_inline_free(g_win32_cef_hook);
        g_win32_cef_hook = nullptr;
        return;
    }
}

/**
 * called when the hook is unloaded from the web-helper.
 */
void win32_uninitialize_trampoline()
{
    if (g_win32_cef_hook) {
        snare_inline_remove(g_win32_cef_hook);
        snare_inline_free(g_win32_cef_hook);
        g_win32_cef_hook = nullptr;
    }
}

/**
 * called once the dll is loaded into the web-helper process.
 */
int __stdcall DllMain(HINSTANCE, DWORD reason, LPVOID)
{
    switch (reason) {
        case DLL_PROCESS_ATTACH:
            win32_initialize_trampoline();
            break;
        case DLL_PROCESS_DETACH:
            win32_uninitialize_trampoline();
            break;
    }
    return TRUE;
}
#endif
