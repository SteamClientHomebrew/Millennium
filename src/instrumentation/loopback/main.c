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

#ifdef __linux__
#define _GNU_SOURCE
#endif

#include "instrumentation/chromium.h"
#include "instrumentation/patch_engine.h"

#ifdef _WIN32
#define plat_strdup _strdup
#else
#define plat_strdup strdup
#endif

#include <limits.h>
#include <stdarg.h>
#include <stdatomic.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

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
#include <string.h>
#include <sys/mman.h>
#include <unistd.h>
#define SNARE_STATIC
#define SNARE_IMPLEMENTATION
#include "libsnare.h"

extern int tramp_cef_browser_host_create_browser(const void*, struct _cef_client_t*, void*, const void*, void*, void*);
static snare_inline_t g_cef_hook = NULL;

static void page_rx(void* addr)
{
    long ps = sysconf(_SC_PAGESIZE);
    mprotect((void*)((uintptr_t)addr & ~(ps - 1)), ps, PROT_READ | PROT_EXEC);
}

static void page_rwx(void* addr)
{
    long ps = sysconf(_SC_PAGESIZE);
    mprotect((void*)((uintptr_t)addr & ~(ps - 1)), ps, PROT_READ | PROT_WRITE | PROT_EXEC);
}

static int find_cef_fn(struct dl_phdr_info* info, size_t size, void* data)
{
    (void)size;
    if (!info->dlpi_name || !strstr(info->dlpi_name, "libcef")) return 0;

    void* h = dlopen(info->dlpi_name, RTLD_NOLOAD | RTLD_NOW);
    if (!h) return 0;

    void** out = (void**)data;
    *out = dlsym(h, "cef_browser_host_create_browser");
    dlclose(h);

    return *out ? 1 : 0; /* stop iterating once found */
}

static void install_hook(void)
{
    void* cef_fn = NULL;
    dl_iterate_phdr(find_cef_fn, &cef_fn);

    if (!cef_fn) {
        fatal("hook: cef_browser_host_create_browser not found in libcef");
        return;
    }

    g_cef_hook = snare_inline_new(cef_fn, (void*)tramp_cef_browser_host_create_browser);
    if (!g_cef_hook) {
        fatal("hook: snare_inline_new failed");
        return;
    }

    if (snare_inline_install(g_cef_hook) < 0) {
        fatal("hook: snare_inline_install failed");
        snare_inline_free(g_cef_hook);
        g_cef_hook = NULL;
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

extern cef_resource_request_handler_t* create_steamloopback_request_handler(const char* url, cef_resource_request_handler_t* inner);
extern struct _cef_client_t* orig_c;
extern struct _cef_request_handler_t* (*original_get_request_handler)(void*);
extern struct _cef_resource_request_handler_t* (*orig_get_resource)(void*, void*, void*, struct _cef_request_t*, int, int, void*, int*);

typedef struct
{
    cef_resource_request_handler_t handler;
    char* url;
    cef_resource_request_handler_t* inner_req_handler;
    atomic_int ref_count;
} steamloopback_request_handler_t;

/**
 * 1:1 forwarding resource handler. we never buffer or touch
 * the bytes, just relay every call straight through to the real handler.
 */
typedef struct
{
    cef_resource_handler_t handler;
    cef_resource_handler_t* inner;
    char* url;
    atomic_int ref_count;
} steamloopback_passthrough_handler_t;

#define LB_DRAINING 0
#define LB_READY 1
#define LB_FAILED 2
#define LB_CANCELLED 3
#define LB_READ_CHUNK_SIZE 65536

/**
 * buffered/patching proxy resource handler
 */
typedef struct
{
    cef_resource_handler_t handler;
    cef_resource_handler_t* inner;
    cef_callback_t open_cb_to_inner;
    cef_resource_read_callback_t read_cb_to_inner;
    atomic_int open_cb_ref_count;
    atomic_int read_cb_ref_count;
    cef_callback_t* outer_open_cb;
    uint8_t* raw_data;
    size_t raw_len;
    size_t raw_cap;
    uint8_t* read_chunk;
    bool inner_opened;
    char* url;
    char* served_data;
    uint32_t served_size;
    const char* mime_type;
    size_t pos;
    bool sync_phase;
    int state;
    atomic_int ref_count;
} steamloopback_proxy_resource_handler_t;

extern cef_resource_handler_t* create_proxy_passthrough(const char* url, cef_resource_handler_t* inner);
extern cef_resource_handler_t* create_proxy_buffered(const char* url, cef_resource_handler_t* inner);

struct _cef_client_t* orig_c = NULL;
struct _cef_request_handler_t* (*original_get_request_handler)(void*) = NULL;
struct _cef_resource_request_handler_t* (*orig_get_resource)(void*, void*, void*, struct _cef_request_t*, int, int, void*, int*) = NULL;

/**
 * generic helpers for calling add_ref()/release() on any hand-rolled CEF
 * ref-counted object given just its base pointer
 */
static void cef_base_add_ref(void* obj)
{
    if (!obj) return;
    char* p = (char*)obj;
    void (*fn)(void*) = *(void (**)(void*))(p + sizeof(size_t) + sizeof(void*) * 0);
    if (fn) fn(obj);
}
static int cef_base_release(void* obj)
{
    if (!obj) return 0;
    char* p = (char*)obj;
    int (*fn)(void*) = *(int (**)(void*))(p + sizeof(size_t) + sizeof(void*) * 1);
    if (fn) return fn(obj);
    return 0;
}
void CEF_CALLBACK lb_req_add_ref(void* base)
{
    steamloopback_request_handler_t* handler = (steamloopback_request_handler_t*)base;
    atomic_fetch_add(&handler->ref_count, 1);
}
int CEF_CALLBACK lb_req_release(void* base)
{
    steamloopback_request_handler_t* handler = (steamloopback_request_handler_t*)base;
    if (atomic_fetch_sub(&handler->ref_count, 1) == 1) {
        cef_base_release(handler->inner_req_handler);
        free(handler->url);
        free(handler);
        return 1;
    }
    return 0;
}
int CEF_CALLBACK lb_req_has_one_ref(void* base)
{
    steamloopback_request_handler_t* handler = (steamloopback_request_handler_t*)base;
    return atomic_load(&handler->ref_count) == 1;
}
int CEF_CALLBACK lb_req_has_at_least_one_ref(void* base)
{
    steamloopback_request_handler_t* handler = (steamloopback_request_handler_t*)base;
    return atomic_load(&handler->ref_count) >= 1;
}
void* CEF_CALLBACK lb_on_before_load_hdl(void* self, void* browser, void* frame, void* request, void* callback)
{
    return (void*)1; /** continue */
}
void* CEF_CALLBACK lb_get_resource_hdl(void* self, void* browser, void* frame, void* request)
{
    steamloopback_request_handler_t* handler = (steamloopback_request_handler_t*)self;
    if (!handler->inner_req_handler) {
        return NULL;
    }

    void* steam_resource_handler = handler->inner_req_handler->get_resource_handler(handler->inner_req_handler, browser, frame, request);
    if (!steam_resource_handler) {
        return NULL; /** steam wants chromium's default network loader here. don't intercept */
    }

    if (lb_url_is_patchable(handler->url)) {
        return create_proxy_buffered(handler->url, (cef_resource_handler_t*)steam_resource_handler);
    } else {
        return create_proxy_passthrough(handler->url, (cef_resource_handler_t*)steam_resource_handler);
    }
}
static void set_cef_header(struct _cef_response_t* response, const char* name, const char* value)
{
    CEF_LAZY_LOAD(cef_string_utf8_set, int, (const char*, size_t, cef_string_utf8_t*, int));
    CEF_LAZY_LOAD(cef_string_utf8_clear, void, (cef_string_utf8_t*));

    cef_string_t name_str = { 0 }, value_str = { 0 };
    lazy_cef_string_utf8_set(name, strlen(name), &name_str, 1);
    lazy_cef_string_utf8_set(value, strlen(value), &value_str, 1);
    response->set_header_by_name(response, &name_str, &value_str, 1);
    lazy_cef_string_utf8_clear(&name_str);
    lazy_cef_string_utf8_clear(&value_str);
}
void CEF_CALLBACK lb_pass_add_ref(void* base)
{
    atomic_fetch_add(&((steamloopback_passthrough_handler_t*)base)->ref_count, 1);
}
int CEF_CALLBACK lb_pass_release(void* base)
{
    steamloopback_passthrough_handler_t* handler = (steamloopback_passthrough_handler_t*)base;
    if (atomic_fetch_sub(&handler->ref_count, 1) == 1) {
        cef_base_release(handler->inner);
        free(handler->url);
        free(handler);
        return 1;
    }
    return 0;
}
int CEF_CALLBACK lb_pass_has_one_ref(void* base)
{
    return atomic_load(&((steamloopback_passthrough_handler_t*)base)->ref_count) == 1;
}
int CEF_CALLBACK lb_pass_has_at_least_one_ref(void* base)
{
    return atomic_load(&((steamloopback_passthrough_handler_t*)base)->ref_count) >= 1;
}
int CEF_CALLBACK lb_pass_open(void* self, void* request, int* handle_request, void* callback)
{
    steamloopback_passthrough_handler_t* h = (steamloopback_passthrough_handler_t*)self;
    return h->inner->open(h->inner, request, handle_request, callback);
}
void CEF_CALLBACK lb_pass_get_response_headers(struct _cef_resource_handler_t* self, struct _cef_response_t* response, int64_t* response_length, cef_string_t* redirectUrl)
{
    steamloopback_passthrough_handler_t* h = (steamloopback_passthrough_handler_t*)self;
    h->inner->get_response_headers(h->inner, response, response_length, redirectUrl);
}
int CEF_CALLBACK lb_pass_read(void* self, void* data_out, int bytes_to_read, int* bytes_read, void* callback)
{
    steamloopback_passthrough_handler_t* h = (steamloopback_passthrough_handler_t*)self;
    return h->inner->read(h->inner, data_out, bytes_to_read, bytes_read, callback);
}
void CEF_CALLBACK lb_pass_cancel(void* self)
{
    steamloopback_passthrough_handler_t* h = (steamloopback_passthrough_handler_t*)self;
    if (h->inner->cancel) h->inner->cancel(h->inner);
}
cef_resource_handler_t* create_proxy_passthrough(const char* url, cef_resource_handler_t* inner)
{
    steamloopback_passthrough_handler_t* handler = (steamloopback_passthrough_handler_t*)calloc(1, sizeof(steamloopback_passthrough_handler_t));

    handler->inner = inner;
    handler->url = plat_strdup(url);
    atomic_store(&handler->ref_count, 1);

    char* p = handler->handler._base;
    *(size_t*)(p + 0x0) = sizeof(cef_resource_handler_t);
    *(void (**)(void*))(p + sizeof(size_t) + sizeof(void*) * 0) = (void (*)(void*))lb_pass_add_ref;
    *(int (**)(void*))(p + sizeof(size_t) + sizeof(void*) * 1) = (int (*)(void*))lb_pass_release;
    *(int (**)(void*))(p + sizeof(size_t) + sizeof(void*) * 2) = (int (*)(void*))lb_pass_has_one_ref;
    *(int (**)(void*))(p + sizeof(size_t) + sizeof(void*) * 3) = (int (*)(void*))lb_pass_has_at_least_one_ref;

    handler->handler.open = lb_pass_open;
    handler->handler.get_response_headers = lb_pass_get_response_headers;
    handler->handler.read = lb_pass_read;
    handler->handler.cancel = lb_pass_cancel;
    handler->handler.process_request = NULL;
    handler->handler.skip = NULL;
    handler->handler.read_response = NULL;
    return &handler->handler;
}
static steamloopback_proxy_resource_handler_t* lb_buf_from_open_cb(cef_callback_t* cb)
{
    return (steamloopback_proxy_resource_handler_t*)((char*)cb - offsetof(steamloopback_proxy_resource_handler_t, open_cb_to_inner));
}
static steamloopback_proxy_resource_handler_t* lb_buf_from_read_cb(cef_resource_read_callback_t* cb)
{
    return (steamloopback_proxy_resource_handler_t*)((char*)cb - offsetof(steamloopback_proxy_resource_handler_t, read_cb_to_inner));
}
/** append n bytes to h->raw_data, growing geometrically. false on OOM. */
static bool lb_buf_append(steamloopback_proxy_resource_handler_t* h, const uint8_t* data, size_t n)
{
    if (h->raw_len + n > h->raw_cap) {
        size_t new_cap = h->raw_cap ? h->raw_cap * 2 : 65536;
        while (new_cap < h->raw_len + n)
            new_cap *= 2;
        uint8_t* grown = (uint8_t*)realloc(h->raw_data, new_cap);
        if (!grown) return false;
        h->raw_data = grown;
        h->raw_cap = new_cap;
    }
    memcpy(h->raw_data + h->raw_len, data, n);
    h->raw_len += n;
    return true;
}
int CEF_CALLBACK lb_buf_release(void* base)
{
    steamloopback_proxy_resource_handler_t* h = (steamloopback_proxy_resource_handler_t*)base;
    int prev = atomic_fetch_sub(&h->ref_count, 1);
    if (prev == 1) {
        cef_base_release(h->inner);
        free(h->url);
        free(h->served_data);
        free(h->raw_data);
        free(h->read_chunk);
        free(h);
        return 1;
    }
    return 0;
}
void CEF_CALLBACK lb_buf_add_ref(void* base)
{
    atomic_fetch_add(&((steamloopback_proxy_resource_handler_t*)base)->ref_count, 1);
}
int CEF_CALLBACK lb_buf_has_one_ref(void* base)
{
    return atomic_load(&((steamloopback_proxy_resource_handler_t*)base)->ref_count) == 1;
}
int CEF_CALLBACK lb_buf_has_at_least_one_ref(void* base)
{
    return atomic_load(&((steamloopback_proxy_resource_handler_t*)base)->ref_count) >= 1;
}
void CEF_CALLBACK lb_buf_opencb_add_ref(void* base)
{
    atomic_fetch_add(&lb_buf_from_open_cb((cef_callback_t*)base)->open_cb_ref_count, 1);
}
int CEF_CALLBACK lb_buf_opencb_release(void* base)
{
    return atomic_fetch_sub(&lb_buf_from_open_cb((cef_callback_t*)base)->open_cb_ref_count, 1) == 1 ? 1 : 0;
}
int CEF_CALLBACK lb_buf_opencb_has_one_ref(void* base)
{
    return atomic_load(&lb_buf_from_open_cb((cef_callback_t*)base)->open_cb_ref_count) == 1;
}
int CEF_CALLBACK lb_buf_opencb_has_at_least_one_ref(void* base)
{
    return atomic_load(&lb_buf_from_open_cb((cef_callback_t*)base)->open_cb_ref_count) >= 1;
}
void CEF_CALLBACK lb_buf_readcb_add_ref(void* base)
{
    atomic_fetch_add(&lb_buf_from_read_cb((cef_resource_read_callback_t*)base)->read_cb_ref_count, 1);
}
int CEF_CALLBACK lb_buf_readcb_release(void* base)
{
    return atomic_fetch_sub(&lb_buf_from_read_cb((cef_resource_read_callback_t*)base)->read_cb_ref_count, 1) == 1 ? 1 : 0;
}
int CEF_CALLBACK lb_buf_readcb_has_one_ref(void* base)
{
    return atomic_load(&lb_buf_from_read_cb((cef_resource_read_callback_t*)base)->read_cb_ref_count) == 1;
}
int CEF_CALLBACK lb_buf_readcb_has_at_least_one_ref(void* base)
{
    return atomic_load(&lb_buf_from_read_cb((cef_resource_read_callback_t*)base)->read_cb_ref_count) >= 1;
}

static void lb_buf_advance(steamloopback_proxy_resource_handler_t* h);

/**
 * drain finished (successfully or not)
 * patch what we captured (if any) and resume CEF if we'd deferred, then drop the drain's self-held ref.
 * */
static void lb_buf_finish(steamloopback_proxy_resource_handler_t* h, bool success)
{
    if (success) {
        char* served = NULL;
        uint32_t size = 0;
        const char* mime = NULL;
        int rc = lb_patch_content(h->url, h->raw_data, (uint32_t)h->raw_len, &served, &size, &mime);
        if (rc == 0 && served) {
            h->served_data = served;
            h->served_size = size;
            h->mime_type = mime;
            h->state = LB_READY;
        } else {
            h->state = LB_FAILED;
        }
    } else {
        h->state = LB_FAILED;
    }

    free(h->read_chunk);
    h->read_chunk = NULL;
    free(h->raw_data);
    h->raw_data = NULL;
    h->raw_len = 0;
    h->raw_cap = 0;

    if (!h->sync_phase) {
        if (h->state == LB_READY)
            h->outer_open_cb->cont(h->outer_open_cb);
        else
            h->outer_open_cb->cancel(h->outer_open_cb);
    }

    lb_buf_release(h); /** drop the ref the drain took in lb_buf_open */
}

/**
 * actual drain loop: read from h->inner until EOF/error/deferred.
 * only ever called once h->inner_opened is true.
 */
static void lb_buf_advance(steamloopback_proxy_resource_handler_t* h)
{
    if (h->state == LB_CANCELLED) return;

    for (;;) {
        if (!h->read_chunk) h->read_chunk = (uint8_t*)malloc(LB_READ_CHUNK_SIZE);
        if (!h->read_chunk) {
            lb_buf_finish(h, false);
            return;
        }

        int bytes_read = 0;
        int rc = h->inner->read(h->inner, h->read_chunk, LB_READ_CHUNK_SIZE, &bytes_read, &h->read_cb_to_inner);

        if (rc && bytes_read > 0) {
            if (!lb_buf_append(h, h->read_chunk, (size_t)bytes_read)) {
                lb_buf_finish(h, false);
                return;
            }
            continue;
        }
        if (rc && bytes_read == 0) return; /** deferred, wait for lb_buf_readcb_cont */
        if (!rc && bytes_read == 0) {
            lb_buf_finish(h, true); /** EOF */
            return;
        }
        lb_buf_finish(h, false); /** failure (bytes_read < 0) */
        return;
    }
}

void CEF_CALLBACK lb_buf_opencb_cont(cef_callback_t* base)
{
    steamloopback_proxy_resource_handler_t* h = lb_buf_from_open_cb(base);
    if (h->state == LB_CANCELLED) return;
    h->inner_opened = true;
    lb_buf_advance(h);
}

void CEF_CALLBACK lb_buf_opencb_cancel(cef_callback_t* base)
{
    steamloopback_proxy_resource_handler_t* h = lb_buf_from_open_cb(base);
    if (h->state == LB_CANCELLED) return;
    lb_buf_finish(h, false);
}

void CEF_CALLBACK lb_buf_readcb_cont(cef_resource_read_callback_t* base, int bytes_read)
{
    steamloopback_proxy_resource_handler_t* h = lb_buf_from_read_cb(base);
    if (h->state == LB_CANCELLED) return;

    if (bytes_read > 0) {
        if (!lb_buf_append(h, h->read_chunk, (size_t)bytes_read)) {
            lb_buf_finish(h, false);
            return;
        }
        lb_buf_advance(h);
    } else if (bytes_read == 0) {
        lb_buf_finish(h, true);
    } else {
        lb_buf_finish(h, false);
    }
}

/**
 * eager drain:
 * fully drains h->inner (recursing through async continuations
 * as needed) before ever returning control past open(), since patching needs
 * the whole buffer anyway. keeps our own read() trivially synchronous.
 */
int CEF_CALLBACK lb_buf_open(void* self, void* request, int* handle_request, void* outer_callback)
{
    steamloopback_proxy_resource_handler_t* h = (steamloopback_proxy_resource_handler_t*)self;
    h->outer_open_cb = (cef_callback_t*)outer_callback;

    lb_buf_add_ref(h); /** drain owns its own ref across the async gap */
    h->sync_phase = true;

    int now_open = 0;
    int rc = h->inner->open(h->inner, request, &now_open, &h->open_cb_to_inner);
    if (!rc) {
        lb_buf_finish(h, false);
    } else if (!now_open) {
        /** deferred - lb_buf_opencb_cont/_cancel will resume us */
    } else {
        h->inner_opened = true;
        lb_buf_advance(h);
    }

    h->sync_phase = false;

    if (h->state == LB_READY) {
        *handle_request = 1;
        return 1;
    }
    if (h->state == LB_FAILED) {
        *handle_request = 1;
        return 0;
    }
    *handle_request = 0;
    return 1;
}

int CEF_CALLBACK lb_buf_read(void* self, void* data_out, int bytes_to_read, int* bytes_read, void* callback)
{
    steamloopback_proxy_resource_handler_t* h = (steamloopback_proxy_resource_handler_t*)self;
    size_t available = h->served_size - h->pos;
    *bytes_read = (int)((size_t)bytes_to_read < available ? (size_t)bytes_to_read : available);

    if (*bytes_read > 0) {
        memcpy(data_out, h->served_data + h->pos, *bytes_read);
        h->pos += *bytes_read;
    }

    return (*bytes_read > 0);
}

void CEF_CALLBACK lb_buf_get_response_headers(struct _cef_resource_handler_t* self, struct _cef_response_t* response, int64_t* response_length, cef_string_t* redirectUrl)
{
    steamloopback_proxy_resource_handler_t* h = (steamloopback_proxy_resource_handler_t*)self;
    CEF_LAZY_LOAD(cef_string_utf8_set, int, (const char*, size_t, cef_string_utf8_t*, int));
    CEF_LAZY_LOAD(cef_string_utf8_clear, void, (cef_string_utf8_t*));

    response->set_status(response, 200);

    const char* mime = h->mime_type ? h->mime_type : "application/octet-stream";
    cef_string_t mime_str = { 0 };
    lazy_cef_string_utf8_set(mime, strlen(mime), &mime_str, 1);
    response->set_mime_type(response, &mime_str);
    lazy_cef_string_utf8_clear(&mime_str);

    set_cef_header(response, "X-Millennium-Hooked", "1");
    set_cef_header(response, "Cache-Control", "no-cache"); /** steam uses no-cache, and they internally health-check for this header */

    *response_length = h->served_size;
}

void CEF_CALLBACK lb_buf_cancel(void* self)
{
    steamloopback_proxy_resource_handler_t* h = (steamloopback_proxy_resource_handler_t*)self;
    if (h->state == LB_DRAINING) {
        h->state = LB_CANCELLED;
        if (h->inner && h->inner->cancel) h->inner->cancel(h->inner);
    }
}

typedef struct
{
    size_t size;
    void (*add_ref)(void*);
    int (*release)(void*);
    int (*has_one_ref)(void*);
    int (*has_at_least_one_ref)(void*);
} base_vtable_t;

cef_resource_handler_t* create_proxy_buffered(const char* url, cef_resource_handler_t* inner)
{
    steamloopback_proxy_resource_handler_t* handler = (steamloopback_proxy_resource_handler_t*)calloc(1, sizeof(steamloopback_proxy_resource_handler_t));

    handler->inner = inner;
    handler->url = plat_strdup(url);
    handler->state = LB_DRAINING;

    atomic_store(&handler->ref_count, 1);
    atomic_store(&handler->open_cb_ref_count, 1);
    atomic_store(&handler->read_cb_ref_count, 1);

    base_vtable_t* p = (base_vtable_t*)&handler->handler._base;
    p->size = sizeof(cef_resource_handler_t);
    p->add_ref = lb_buf_add_ref;
    p->release = lb_buf_release;
    p->has_one_ref = lb_buf_has_one_ref;
    p->has_at_least_one_ref = lb_buf_has_at_least_one_ref;

    handler->handler.open = lb_buf_open;
    handler->handler.get_response_headers = lb_buf_get_response_headers;
    handler->handler.read = lb_buf_read;
    handler->handler.cancel = lb_buf_cancel;
    handler->handler.process_request = NULL;
    handler->handler.skip = NULL;
    handler->handler.read_response = NULL;

    /**
     * synthetic cb objects handed to inner->open()/inner->read().
     * each logically needs its own working base vtable.
     */
    base_vtable_t* ocb = (base_vtable_t*)&handler->open_cb_to_inner._base;
    ocb->size = sizeof(cef_callback_t);
    ocb->add_ref = lb_buf_opencb_add_ref;
    ocb->release = lb_buf_opencb_release;
    ocb->has_one_ref = lb_buf_opencb_has_one_ref;
    ocb->has_at_least_one_ref = lb_buf_opencb_has_at_least_one_ref;

    handler->open_cb_to_inner.cont = lb_buf_opencb_cont;
    handler->open_cb_to_inner.cancel = lb_buf_opencb_cancel;

    base_vtable_t* rcb = (base_vtable_t*)&handler->read_cb_to_inner._base;
    rcb->size = sizeof(cef_resource_read_callback_t);
    rcb->add_ref = lb_buf_readcb_add_ref;
    rcb->release = lb_buf_readcb_release;
    rcb->has_one_ref = lb_buf_readcb_has_one_ref;
    rcb->has_at_least_one_ref = lb_buf_readcb_has_at_least_one_ref;

    handler->read_cb_to_inner.cont = lb_buf_readcb_cont;
    return &handler->handler;
}

cef_resource_request_handler_t* create_steamloopback_request_handler(const char* url, cef_resource_request_handler_t* inner)
{
    steamloopback_request_handler_t* handler = (steamloopback_request_handler_t*)calloc(1, sizeof(steamloopback_request_handler_t));

    handler->url = plat_strdup(url);
    handler->inner_req_handler = inner;
    atomic_store(&handler->ref_count, 1);

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
    const char* url_str = (url && url->str) ? url->str : "";

    void* steam_req_handler = orig_get_resource(_1, _2, _3, request, _5, _6, _7, _8);
    if (!steam_req_handler) {
        if (url && lazy_cef_string_userfree_utf8_free) lazy_cef_string_userfree_utf8_free((cef_string_userfree_utf8_t)url);
        return NULL;
    }

    cef_resource_request_handler_t* custom_handler = create_steamloopback_request_handler(url_str, (cef_resource_request_handler_t*)steam_req_handler);
    if (url && lazy_cef_string_userfree_utf8_free) lazy_cef_string_userfree_utf8_free((cef_string_userfree_utf8_t)url);
    return custom_handler;
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
        orig_get_resource =
            (struct _cef_resource_request_handler_t * (*)(void*, void*, void*, struct _cef_request_t*, int, int, void*, int*)) handler->get_resource_request_handler;
    }

    /** redirect the resource request handler to our hooked one */
    handler->get_resource_request_handler = (void*(CEF_CALLBACK*)(void*, void*, void*, void*, int, int, void*, int*))hooked_get_resource;
    return handler;
}

/**
 * hooked cef_browser_host_create_browser.
 * as this module is loaded via LD_PRELOAD, its procedural link its directly replaced on the IAT/GOT inline.
 * thankfully since steam doesn't statically link against cef (not even sure if its possible), it allows us to trampoline
 * calls it makes to cef.
 */
int tramp_cef_browser_host_create_browser(const void* _1, struct _cef_client_t* c, void* _3, const void* _4, void* _5, void* _6)
{
    /** overwrite steams get request handler they provide. */
    if (c && c->get_request_handler && !orig_c) {
        orig_c = c;
        original_get_request_handler = c->get_request_handler;
        c->get_request_handler = (struct _cef_request_handler_t * (CEF_CALLBACK*)(void*)) hooked_get_request_handler;
    }

#ifdef _WIN32
    typedef int(__cdecl * cef_create_browser_fn)(const void*, struct _cef_client_t*, void*, const void*, void*, void*);
    cef_create_browser_fn orig = (cef_create_browser_fn)snare_inline_get_trampoline(g_win32_cef_hook);
    return orig(_1, c, _3, _4, _5, _6);
#elif __linux__
    if (!g_cef_hook) {
        fatal("cef_browser_host_create_browser: hook not installed, call dropped");
        return 0;
    }

    typedef int (*cef_create_browser_fn)(const void*, struct _cef_client_t*, void*, const void*, void*, void*);
    cef_create_browser_fn orig = (cef_create_browser_fn)snare_inline_get_trampoline(g_cef_hook);
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

void* get_module_base(void)
{
    HMODULE libcef_base_address = NULL;
    while (true) {
        if ((libcef_base_address = GetModuleHandleA(hook_target_dll))) break;
        Sleep(100);
    }
    return libcef_base_address;
}

void* get_fn_address(void)
{
    void* libcef_base_address = get_module_base();
    return (void*)GetProcAddress((HMODULE)libcef_base_address, hook_fn_name);
}

static snare_inline_t g_win32_cef_hook = NULL;

/**
 * called when the hook is loaded into the web-helper on windows
 * as we aren't using LD_PRELOAD on windows, we have to use snare to trampoline
 * we don't have precedence over the load order in IAT/GOT like on linux/macOS
 */
void win32_initialize_trampoline(void)
{
    void* proc = get_fn_address();
    if (!proc) {
        fatal("ordinate %s not found in %s", hook_fn_name, hook_target_dll);
        return;
    }

    g_win32_cef_hook = snare_inline_new(proc, (void*)&tramp_cef_browser_host_create_browser);
    if (!g_win32_cef_hook) {
        fatal("failed to create hook on %s...", hook_fn_name);
        return;
    }

    if (snare_inline_install(g_win32_cef_hook) < 0) {
        fatal("failed to install hook on %s...", hook_fn_name);
        snare_inline_free(g_win32_cef_hook);
        g_win32_cef_hook = NULL;
        return;
    }
}

/**
 * called when the hook is unloaded from the web-helper.
 */
void win32_uninitialize_trampoline(void)
{
    if (g_win32_cef_hook) {
        snare_inline_remove(g_win32_cef_hook);
        snare_inline_free(g_win32_cef_hook);
        g_win32_cef_hook = NULL;
    }
}

/**
 * called once the dll is loaded into the web-helper process.
 */
int __stdcall DllMain(HINSTANCE hinstDLL, DWORD reason, LPVOID lpReserved)
{
    (void)hinstDLL;
    (void)lpReserved;
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
