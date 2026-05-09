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
#include "instrumentation/resource_query_parser.h"
#include "instrumentation/logger.h"
#include <atomic>
#include <limits.h>

#ifdef __linux__
#include <linux/limits.h>
#include <cstdio>
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
        fprintf(stderr, "hook: cef_browser_host_create_browser not found in libcef\n");
        return;
    }

    g_cef_hook = snare_inline_new(cef_fn, reinterpret_cast<void*>(tramp_cef_browser_host_create_browser));
    if (!g_cef_hook) {
        fprintf(stderr, "hook: snare_inline_new failed\n");
        return;
    }

    if (snare_inline_install(g_cef_hook) < 0) {
        fprintf(stderr, "hook: snare_inline_install failed\n");
        snare_inline_free(g_cef_hook);
        g_cef_hook = nullptr;
        return;
    }

    /* restore RX — snare leaves the page RWX which Chromium's sandbox detects and kills */
    page_rx(cef_fn);
    fprintf(stderr, "hook: installed on cef_browser_host_create_browser at %p\n", cef_fn);
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
    cef_response_filter_t filter;
    std::atomic<int> ref_count;
    char* pending; /** buffered output waiting to be flushed */
    size_t pending_size;
    size_t pending_pos;
    bool injected; /** true once the tag has been spliced in */
    char inject_tag[512];
    int inject_tag_len;
} html_inject_filter_t;

static void CEF_CALLBACK hif_add_ref(void* self)
{
    ((html_inject_filter_t*)self)->ref_count.fetch_add(1);
}
static int CEF_CALLBACK hif_release(void* self)
{
    auto* f = (html_inject_filter_t*)self;
    if (f->ref_count.fetch_sub(1) == 1) {
        free(f->pending);
        free(f);
        return 1;
    }
    return 0;
}
static int CEF_CALLBACK hif_has_one_ref(void* self)
{
    return ((html_inject_filter_t*)self)->ref_count.load() == 1;
}
static int CEF_CALLBACK hif_has_at_least_one_ref(void* self)
{
    return ((html_inject_filter_t*)self)->ref_count.load() >= 1;
}

static int CEF_CALLBACK hif_init_filter(cef_response_filter_t* self)
{
    return 1;
}

static int CEF_CALLBACK hif_filter(cef_response_filter_t* self, void* data_in, size_t data_in_size, size_t* data_in_read, void* data_out, size_t data_out_size,
                                   size_t* data_out_written)
{
    auto* f = (html_inject_filter_t*)self;
    *data_in_read = 0;
    *data_out_written = 0;

    /** flush any pending output from a previous call */
    if (f->pending && f->pending_pos < f->pending_size) {
        size_t remaining = f->pending_size - f->pending_pos;
        size_t to_write = remaining < data_out_size ? remaining : data_out_size;
        memcpy(data_out, f->pending + f->pending_pos, to_write);
        f->pending_pos += to_write;
        *data_out_written = to_write;

        if (f->pending_pos >= f->pending_size) {
            free(f->pending);
            f->pending = NULL;
            f->pending_size = 0;
            f->pending_pos = 0;
        }

        /** don't consume new input yet - flush first */
        return CEF_RESPONSE_FILTER_NEED_MORE_DATA;
    }

    /** after injection, pass through directly */
    if (f->injected) {
        if (!data_in || data_in_size == 0) return CEF_RESPONSE_FILTER_DONE;
        size_t to_write = data_in_size < data_out_size ? data_in_size : data_out_size;
        memcpy(data_out, data_in, to_write);
        *data_in_read = to_write;
        *data_out_written = to_write;
        return CEF_RESPONSE_FILTER_NEED_MORE_DATA;
    }

    /** no more input and we never found <head> - just finish */
    if (!data_in || data_in_size == 0) {
        f->injected = true;
        return CEF_RESPONSE_FILTER_DONE;
    }

    /** buffer the chunk and look for <head...> */
    size_t old_size = f->pending_size;
    size_t new_size = old_size + data_in_size;
    char* buf = (char*)realloc(f->pending, new_size + 1);
    if (!buf) {
        /** OOM - pass through */
        *data_in_read = data_in_size;
        size_t to_write = data_in_size < data_out_size ? data_in_size : data_out_size;
        memcpy(data_out, data_in, to_write);
        *data_out_written = to_write;
        f->injected = true;
        return CEF_RESPONSE_FILTER_NEED_MORE_DATA;
    }
    memcpy(buf + old_size, data_in, data_in_size);
    buf[new_size] = '\0';
    f->pending = buf;
    f->pending_size = new_size;
    f->pending_pos = 0;
    *data_in_read = data_in_size;

    const char* head = strstr(f->pending, "<head");
    if (!head && new_size < 4096) {
        /** haven't found <head> yet and buffer is small - wait for more */
        *data_out_written = 0;
        return CEF_RESPONSE_FILTER_NEED_MORE_DATA;
    }

    if (head) {
        const char* head_close = strchr(head, '>');
        if (head_close) {
            head_close++; /** past '>' */
            size_t offset = head_close - f->pending;

            /** build new buffer: prefix + tag + suffix */
            size_t injected_size = f->pending_size + (size_t)f->inject_tag_len;
            char* injected = (char*)malloc(injected_size);
            if (injected) {
                memcpy(injected, f->pending, offset);
                memcpy(injected + offset, f->inject_tag, f->inject_tag_len);
                memcpy(injected + offset + f->inject_tag_len, f->pending + offset, f->pending_size - offset);
                free(f->pending);
                f->pending = injected;
                f->pending_size = injected_size;
                f->pending_pos = 0;
                log_info("Injected modulepreload tags into HTML response\n");
            }
        }
    }

    /** start flushing the (possibly modified) buffer */
    f->injected = true;
    size_t to_write = f->pending_size < data_out_size ? f->pending_size : data_out_size;
    memcpy(data_out, f->pending, to_write);
    f->pending_pos = to_write;
    *data_out_written = to_write;

    if (f->pending_pos >= f->pending_size) {
        free(f->pending);
        f->pending = NULL;
        f->pending_size = 0;
        f->pending_pos = 0;
    }

    return CEF_RESPONSE_FILTER_NEED_MORE_DATA;
}

static cef_response_filter_t* create_html_inject_filter(void)
{
    char* ftp_token = NULL;
#ifdef _WIN32
    size_t token_len = 0;
    if (_dupenv_s(&ftp_token, &token_len, "MILLENNIUM__FTP_TOKEN") != 0 || !ftp_token) {
        log_error("MILLENNIUM__FTP_TOKEN not set\n");
        return NULL;
    }
#else
    const char* env_val = getenv("MILLENNIUM__FTP_TOKEN");
    if (!env_val || !env_val[0]) {
        log_error("MILLENNIUM__FTP_TOKEN not set\n");
        return NULL;
    }
    ftp_token = strdup(env_val);
#endif

    auto* f = (html_inject_filter_t*)calloc(1, sizeof(html_inject_filter_t));
    f->ref_count.store(1);
    f->inject_tag_len = snprintf(f->inject_tag, sizeof(f->inject_tag),
                                 "<link rel=\"modulepreload\" href=\"https://millennium.ftp/%s/millennium.js\">"
                                 "<link rel=\"modulepreload\" href=\"https://millennium.ftp/%s/millennium-frontend.js\">",
                                 ftp_token, ftp_token);
    free(ftp_token);

    char* p = f->filter._base;
    *(size_t*)(p + 0x0) = sizeof(cef_response_filter_t);
    *(void (**)(void*))(p + sizeof(size_t) + sizeof(void*) * 0) = (void (*)(void*))hif_add_ref;
    *(int (**)(void*))(p + sizeof(size_t) + sizeof(void*) * 1) = (int (*)(void*))hif_release;
    *(int (**)(void*))(p + sizeof(size_t) + sizeof(void*) * 2) = (int (*)(void*))hif_has_one_ref;
    *(int (**)(void*))(p + sizeof(size_t) + sizeof(void*) * 3) = (int (*)(void*))hif_has_at_least_one_ref;

    f->filter.init_filter = hif_init_filter;
    f->filter.filter = hif_filter;
    return &f->filter;
}

/** saved original GetResourceResponseFilter from Steam's handler */
static void*(CEF_CALLBACK* orig_get_response_filter)(void*, void*, void*, void*, void*) = NULL;

static void* CEF_CALLBACK hooked_get_response_filter(void* self, void* browser, void* frame, void* request, void* response)
{
    cef_response_filter_t* filter = create_html_inject_filter();
    if (filter) return filter;

    /** fallback to Steam's original if we failed */
    if (orig_get_response_filter) return orig_get_response_filter(self, browser, frame, request, response);
    return NULL;
}

/** check if a URL is an HTML request to steamloopback.host */
static int is_steamloopback_html(cef_string_userfree_t url)
{
    if (!url || !url->str) return 0;
    const char* s = url->str;

    if (strncmp(s, "http", 4) != 0) return 0;
    s += 4;
    if (*s == 's') s++;
    if (strncmp(s, "://steamloopback.host", 21) != 0) return 0;

    const char* query = strchr(url->str, '?');
    const char* dot = strrchr(url->str, '.');
    if (!dot || (query && dot > query)) return 0;

    size_t ext_len = query ? (size_t)(query - dot) : strlen(dot);
    return ext_len == 5 && strncmp(dot, ".html", 5) == 0;
}

/**
 * hook resource request handler to block steamloopback.host requests.
 *
 * This function wraps cef get_resource_request_handler to intercept all
 * resource requests. Requests to steamloopback.host are blocked by returning
 * NULL, preventing Steam's internal handlers from serving the content.
 */
void* hooked_get_resource(void* _1, void* _2, void* _3, struct _cef_request_t* request, int _5, int _6, void* _7, int* _8)
{
    CEF_LAZY_LOAD(cef_string_userfree_utf8_free, void, (cef_string_userfree_utf8_t));
    cef_string_userfree_t url = request->get_url(request);

    if (url && url->str) {
        log_info("hooked_get_resource: %s\n", url->str);
    }

    if (urlp_should_block_lb_req(url)) {
        cef_resource_request_handler_t* custom_handler = create_steamloopback_request_handler(url->str);
        if (url && lazy_cef_string_userfree_utf8_free) lazy_cef_string_userfree_utf8_free((cef_string_userfree_utf8_t)url);
        return custom_handler;
    }

    /** for HTML files, let Steam's handler run but attach our response filter */
    if (is_steamloopback_html(url)) {
        if (url && lazy_cef_string_userfree_utf8_free) lazy_cef_string_userfree_utf8_free((cef_string_userfree_utf8_t)url);

        cef_resource_request_handler_t* handler = (cef_resource_request_handler_t*)orig_get_resource(_1, _2, _3, request, _5, _6, _7, _8);
        if (handler) {
            if (!orig_get_response_filter) {
                orig_get_response_filter = reinterpret_cast<decltype(orig_get_response_filter)>(handler->_4);
            }
            handler->_4 = reinterpret_cast<decltype(handler->_4)>(hooked_get_response_filter);
        }
        return handler;
    }

    /** free the requested url */
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
        fprintf(stderr, "cef_browser_host_create_browser: hook not installed, call dropped\n");
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
        log_error("ordinate %s not found in %s\n", hook_fn_name, hook_target_dll);
        return;
    }

    g_win32_cef_hook = snare_inline_new(proc, reinterpret_cast<void*>(&tramp_cef_browser_host_create_browser));
    if (!g_win32_cef_hook) {
        log_error("failed to create hook on %s...\n", hook_fn_name);
        return;
    }

    if (snare_inline_install(g_win32_cef_hook) < 0) {
        log_error("failed to install hook on %s...\n", hook_fn_name);
        snare_inline_free(g_win32_cef_hook);
        g_win32_cef_hook = nullptr;
        return;
    }

    log_info("successfully hooked %s in %s\n", hook_fn_name, hook_target_dll);
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
