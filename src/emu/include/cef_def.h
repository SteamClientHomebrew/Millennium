#pragma once
#include <stddef.h>
#include <stdint.h>
#if defined(OS_WIN)
#define CEF_CALLBACK __stdcall
#else
#define CEF_CALLBACK
#endif

/**
 * highly confident steam uses UTF-8 strings
 * throughout the web-helper. I've yet to see any
 * 2 byte UNICODE character sequences anywhere.
 * regardless, all of the functions we interact with are UTF-8,
 * so we should be safe :)
 */
typedef struct _cef_string_utf8_t
{
    char* str;     /** utf-8 */
    size_t length; /** 0x8 bytes */
    /* deconstructor function, called once manually
     * freed or on no more refs from the base counter */
    void (*dtor)(char* str);
} cef_string_utf8_t;

typedef char cef_char_t;
typedef cef_string_utf8_t cef_string_t;
typedef cef_string_utf8_t* cef_string_userfree_utf8_t;
typedef cef_string_userfree_utf8_t cef_string_userfree_t;

/** cef request structure */
typedef struct _cef_request_t
{
    /** base member ref, appears to have size_t for size, and 4 other membfunctions */
    char _base[sizeof(size_t) + sizeof(void*) * 0x4];
    char _1[sizeof(char) * 0x8 * 1 /** just one memb */];

    /** caller must free the returned string */
    cef_string_userfree_t(CEF_CALLBACK* get_url)(struct _cef_request_t* self);
} cef_request_t;

typedef struct _cef_response_t
{
    /** base member ref, appears to have size_t for size, and 4 other memb functions */
    char _base[sizeof(size_t) + sizeof(void*) * 0x4];

    /** we can just assume all functions are void* (0x8) bytes as that's the function pointer size on 64bit */
    char _1[sizeof(char) * 0x8 * 4]; /** 4 padded memb functions */
    void (*set_status)(struct _cef_response_t* self, int status);
    char _2[sizeof(char) * 0x8 * 3]; /** 3 padded memb functions */
    void (*set_mime_type)(struct _cef_response_t* self, const cef_string_t* mimeType);
    char _3[sizeof(char) * 0x8 * 3]; /** 3 padded memb functions */
    void (*set_header_by_name)(struct _cef_response_t* self, const cef_string_t* name, const cef_string_t* value, int overwrite);
    char _4[sizeof(char) * 0x8 * 2]; /** 2 padded memb functions */
    cef_string_userfree_t (*get_url)(struct _cef_response_t* self);
} cef_response_t;

/**
 * obviously we don't want type the entire struct
 * so we can just pad the struct so the offset of get_resource_request_handler is
 * the exact same relative from the base address.
 *
 * as the web-helper is 64bit, using 64bit pointers (8 byte size_t):
 * base = 0x28
 * _1 memb = 0x30
 * _1 memb = 0x38
 *
 * offset of get_resource_request_handler => 0x38 (56 bytes)
 */
typedef struct _cef_request_handler_t
{
    /** base member ref, appears to have size_t for size, and 4 other member functions */
    char _base[sizeof(size_t) + sizeof(void*) * 0x4];
    /** pad member functions */
    char _1[sizeof(char) * 0x8];
    char _2[sizeof(char) * 0x8];

    void*(CEF_CALLBACK* get_resource_request_handler)(void* _1, void* _2, void* _3, void* req, int _5, int _6, void* _7, int* _8);
} cef_request_handler_t;

/**
 * only function we are concerned with here is get_request_handler, everything else
 * can just be a padded nullptr. all other values are already set by steam's actual cef client
 * so no calls to non existing functions will actually happen during runtime
 */
typedef struct _cef_client_t
{
    /** base ref count */
    char _base[sizeof(size_t) + sizeof(void*) * 0x4];
    /** pad memb functions to properly align offsets */
    char _padd[sizeof(char) * 0x8 * 17];

    struct _cef_request_handler_t*(CEF_CALLBACK* get_request_handler)(void* self);
} cef_client_t;

typedef struct _cef_resource_handler_t
{
    /** base ref count */
    char _base[sizeof(size_t) + sizeof(void*) * 0x4];

    int(CEF_CALLBACK* open)(void* self, void* request, int* handle_request, void* callback);
    int(CEF_CALLBACK* process_request)(void* self, void* request, void* callback);
    void(CEF_CALLBACK* get_response_headers)(struct _cef_resource_handler_t* self, struct _cef_response_t* response, int64_t* response_length, cef_string_t* redirectUrl);
    int(CEF_CALLBACK* skip)(void* self, int64_t bytes_to_skip, int64_t* bytes_skipped, void* callback);
    int(CEF_CALLBACK* read)(void* self, void* data_out, int bytes_to_read, int* bytes_read, void* callback);
    int(CEF_CALLBACK* read_response)(void* self, void* data_out, int bytes_to_read, int* bytes_read, void* callback);
    void(CEF_CALLBACK* cancel)(void* self);
} cef_resource_handler_t;

typedef struct _cef_resource_request_handler_t
{
    /** base member ref, appears to have size_t for size, and 4 other member functions */
    char _base[sizeof(size_t) + sizeof(void*) * 0x4];

    /**
     * all other padded functions are useless in our case, but we still need to NULL them when constructing
     * it might be safe to pad them and let cef handle it, but I don't care enough to try
     */
    void*(CEF_CALLBACK* _1)(void* self, void*, void*, void*);
    void*(CEF_CALLBACK* on_before_resource_load)(void* self, void* browser, void* frame, void* request, void* callback);
    void*(CEF_CALLBACK* get_resource_handler)(void* self, void* browser, void* frame, void* request);
    void(CEF_CALLBACK* _2)(void* self, void*, void*, void*, void*, void*);
    int(CEF_CALLBACK* _3)(void* self, void*, void*, void*, void*);
    void*(CEF_CALLBACK* _4)(void* self, void*, void*, void*, void*);
    void(CEF_CALLBACK* _5)(void* self, void*, void*, void*, void*, void*, int64_t);
    void(CEF_CALLBACK* _6)(void* self, void*, void*, void*, int*);
} cef_resource_request_handler_t;
