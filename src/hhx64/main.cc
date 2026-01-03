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

/**
 * This module exists for one sole reason. To enable support for millennium's request hook on steamloopback.host requests,
 * we actually have to internally hook and block steam from even serving it in the first place. This is because steams
 * internal hooks take precedence over CDP hooks. Since resource requests handlers (the api behind)
 * steamloopback.host intercept the request before its sent to the chrome devtools protocol, we need to intercept it here,
 * block it, and tell CEF to forward the message to the browser instead of letting steam handle it.
 * This way we can successfully hook steamloopback.host requests with Fetch.enable as the internal interception no longer has
 * precedence as its no-opped.
 */

#include "hhx64/cef_def.h"
#include "hhx64/urlp.h"
#include "hhx64/log.h"

#ifdef __linux__
#include <unistd.h>
#endif

#ifdef _WIN32
static int(__cdecl* win32_cef_browser_host_create_browser)(const void*, struct _cef_client_t*, void*, const void*, void*, void*) = nullptr;
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
 * nullptr, preventing Steam's internal handlers from serving the content.
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
extern "C" int cef_browser_host_create_browser(const void* _1, struct _cef_client_t* c, void* _3, const void* _4, void* _5, void* _6)
{
    CEF_LAZY_LOAD(cef_browser_host_create_browser, int, (const void*, struct _cef_client_t*, void*, const void*, void*, void*));

    /** overwrite steams get request handler they provide. */
    if (c && c->get_request_handler && !orig_c) {
        orig_c = c;
        original_get_request_handler = c->get_request_handler;
        c->get_request_handler = reinterpret_cast<decltype(c->get_request_handler)>(hooked_get_request_handler);
    }

#ifdef _WIN32
    return win32_cef_browser_host_create_browser(_1, c, _3, _4, _5, _6);
#else
    return lazy_cef_browser_host_create_browser(_1, c, _3, _4, _5, _6);
#endif
}

#if defined(_WIN32)
#include <MinHook.h>
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

static bool g_has_initialized_minhook = false;

/**
 * called when the hook is loaded into the web-helper on windows
 * as we aren't using LD_PRELOAD on windows, we have to use min-hook to trampoline
 * we don't have precedence over the load order in IAT/GOT like on linux/macOS
 */
void win32_initialize_trampoline()
{
    if (MH_Initialize() != MH_OK) {
        log_error("failed to initialize min-hook...\n");
        return;
    }

    g_has_initialized_minhook = true;

    void* proc = get_fn_address();
    if (!proc) {
        log_error("ordinate %s not found in %s\n", hook_fn_name, hook_target_dll);
        return;
    }

    if (MH_CreateHook(proc, reinterpret_cast<void*>(&cef_browser_host_create_browser), reinterpret_cast<void**>(&win32_cef_browser_host_create_browser)) != MH_OK) {
        log_error("failed to create hook on %s...\n", hook_fn_name);
        return;
    }

    if (MH_EnableHook(proc) != MH_OK) {
        log_error("failed to enable hook on %s...\n", hook_fn_name);
    }

    log_info("successfully hooked %s in %s\n", hook_fn_name, hook_target_dll);
}

/**
 * called when the hook is unloaded from the web-helper.
 * we likely don't actually need to do this, but its good practice.
 */
void win32_uninitialize_trampoline()
{
    if (g_has_initialized_minhook) {
        MH_Uninitialize();
        g_has_initialized_minhook = false;
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
