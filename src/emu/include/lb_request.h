
#include "log.h"
#include <assert.h>
#include <stdatomic.h>
#include <stdlib.h>
#include <string.h>
#include <dlfcn.h>
#include "cef_def.h"

/**
 * lazily load a cef function from the PLT/GOT/IAT.
 * since we don't actually want the dependency of linking against steams cef,
 * we just load it during runtime.
 */
#define CEF_LAZY_LOAD(func_name, return_type, param_types)                                                                                                                         \
    static return_type(*lazy_##func_name) param_types = NULL;                                                                                                                      \
    if (!lazy_##func_name) {                                                                                                                                                       \
        lazy_##func_name = dlsym(RTLD_NEXT, #func_name);                                                                                                                           \
    }

typedef struct
{
    cef_resource_request_handler_t handler;
    char* url;
    atomic_int ref_count;
} steamloopback_request_handler_t;

typedef struct
{
    cef_resource_handler_t handler;
    char* url;
    char* data;
    size_t size;
    size_t pos;
    atomic_int ref_count;
} steamloopback_resource_handler_t;

/** fwd decl */
int handle_loopback_request(const char* url, char** data, uint32_t* size);
void* create_steamloopback_resource_handler(const char* url);

static struct _cef_client_t* orig_c = NULL;
static struct _cef_request_handler_t* (*original_get_request_handler)(void*) = NULL;
static struct _cef_resource_request_handler_t* (*orig_get_resource)(void*, void*, void*, struct _cef_request_t*, int, int, void*, int*) = NULL;

void CEF_CALLBACK lb_res_add_ref(void* base)
{
    steamloopback_resource_handler_t* handler = (steamloopback_resource_handler_t*)base;
    atomic_fetch_add(&handler->ref_count, 1);
}

int CEF_CALLBACK lb_res_release(void* base)
{
    steamloopback_resource_handler_t* handler = (steamloopback_resource_handler_t*)base;
    if (atomic_fetch_sub(&handler->ref_count, 1) == 1) {
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
    return atomic_load(&handler->ref_count) == 1;
}

int CEF_CALLBACK lb_res_has_at_least_one_ref(void* base)
{
    steamloopback_resource_handler_t* handler = (steamloopback_resource_handler_t*)base;
    return atomic_load(&handler->ref_count) >= 1;
}

int CEF_CALLBACK lb_open(void* self, void* request, int* handle_request, void* callback)
{
    *handle_request = 1;
    return 1;
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

int CEF_CALLBACK lb_read(void* self, void* data_out, int bytes_to_read, int* bytes_read, void* callback)
{
    steamloopback_resource_handler_t* handler = (steamloopback_resource_handler_t*)self;
    size_t available = handler->size - handler->pos;
    *bytes_read = (bytes_to_read < available) ? bytes_to_read : available;

    if (*bytes_read > 0) {
        memcpy(data_out, handler->data + handler->pos, *bytes_read);
        handler->pos += *bytes_read;
    }

    return (*bytes_read > 0);
}

cef_resource_request_handler_t* create_steamloopback_request_handler(const char* url)
{
    steamloopback_request_handler_t* handler = calloc(1, sizeof(steamloopback_request_handler_t));

    handler->url = strdup(url);
    atomic_init(&handler->ref_count, 1);

    char* p = handler->handler._base;
    *(size_t*)(p + 0x0) = sizeof(cef_resource_request_handler_t);
    *(void (**)(void*))(p + sizeof(size_t) + sizeof(void*) * 0) = (void*)lb_req_add_ref;
    *(int (**)(void*))(p + sizeof(size_t) + sizeof(void*) * 1) = (void*)lb_req_release;
    *(int (**)(void*))(p + sizeof(size_t) + sizeof(void*) * 2) = (void*)lb_req_has_one_ref;
    *(int (**)(void*))(p + sizeof(size_t) + sizeof(void*) * 3) = (void*)lb_req_has_at_least_one_ref;

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
    struct timespec start, end;
    clock_gettime(CLOCK_MONOTONIC, &start);
    steamloopback_resource_handler_t* handler = calloc(1, sizeof(steamloopback_resource_handler_t));

    char* data = NULL;
    uint32_t size = 0;
    if (handle_loopback_request(url, &data, &size) != 0 || !data) {
        log_error("Failed to handle loopback request\n");
        free(handler);
        return NULL;
    }

    clock_gettime(CLOCK_MONOTONIC, &end);
    double elapsed = (end.tv_sec - start.tv_sec) + (end.tv_nsec - start.tv_nsec) / 1e9;
    log_info("[hook] took %.3f ms to process %s\n", elapsed * 1000, url);

    atomic_init(&handler->ref_count, 1);

    handler->data = data;
    handler->size = size;
    handler->url = strdup(url);
    handler->pos = 0;

    /** setup handler base */
    char* p = handler->handler._base;
    *(size_t*)(p + 0x0) = sizeof(cef_resource_handler_t);
    *(void (**)(void*))(p + sizeof(size_t) + sizeof(void*) * 0) = (void*)lb_res_add_ref;
    *(int (**)(void*))(p + sizeof(size_t) + sizeof(void*) * 1) = (void*)lb_res_release;
    *(int (**)(void*))(p + sizeof(size_t) + sizeof(void*) * 2) = (void*)lb_res_has_one_ref;
    *(int (**)(void*))(p + sizeof(size_t) + sizeof(void*) * 3) = (void*)lb_res_has_at_least_one_ref;

    handler->handler.get_response_headers = lb_get_response_headers;
    handler->handler.open = lb_open;
    handler->handler.read = lb_read;
    handler->handler.cancel = NULL;
    handler->handler.process_request = NULL;
    handler->handler.skip = NULL;
    return &handler->handler;
}
