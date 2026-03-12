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
extern snare_inline_t g_win32_cef_hook;
#endif

extern cef_resource_request_handler_t* create_steamloopback_request_handler(const char* url);
extern struct _cef_client_t* orig_c;
extern struct _cef_request_handler_t* (*original_get_request_handler)(void*);
extern struct _cef_resource_request_handler_t* (*orig_get_resource)(void*, void*, void*, struct _cef_request_t*, int, int, void*, int*);

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

    if (urlp_should_block_lb_req(url)) {
        cef_resource_request_handler_t* custom_handler = create_steamloopback_request_handler(url->str);
        if (url && lazy_cef_string_userfree_utf8_free) lazy_cef_string_userfree_utf8_free((cef_string_userfree_utf8_t)url);
        return custom_handler;
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
    snare_inline_remove(g_win32_cef_hook);
    auto orig = reinterpret_cast<int(__cdecl*)(const void*, struct _cef_client_t*, void*, const void*, void*, void*)>(snare_inline_get_src(g_win32_cef_hook));
    int result = orig(_1, c, _3, _4, _5, _6);
    snare_inline_install(g_win32_cef_hook);
    return result;
#elif __linux__
    if (!g_cef_hook) {
        fprintf(stderr, "cef_browser_host_create_browser: hook not installed, call dropped\n");
        return 0;
    }

    /* temporarily remove the prologue patch to call the original without recursing */
    void* src = snare_inline_get_src(g_cef_hook);
    page_rwx(src);
    snare_inline_remove(g_cef_hook);
    page_rx(src);
    auto orig = reinterpret_cast<decltype(&tramp_cef_browser_host_create_browser)>(src);
    int result = orig(_1, c, _3, _4, _5, _6);
    page_rwx(src);
    snare_inline_install(g_cef_hook);
    page_rx(src);
    return result;
#endif
}

#if defined(_WIN32)
#define SNARE_STATIC
#define SNARE_IMPLEMENTATION
#include "libsnare.h"
#include "instrumentation/logger.h"
#define fn(x) #x

/** the function name we want to tramp */
#define hook_fn_name fn(tramp_cef_browser_host_create_browser)
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
