#pragma once
// CEF C-API vtable layout for the in-process DevTools path, transcribed from the
// 6478 branch (Chromium 126) capi headers — Steam ships CEF 126.0.6478.183. Only
// the slots up to the methods we call are typed; the rest are padding that pins
// the offsets. Browser and host pointers come from CEF (we only call them); the
// observer is ours (we implement and ref-count it).

#include <stddef.h>

// The layout below is only valid for this CEF major; the bridge checks
// cef_version_info(0) against it before touching a browser.
#define CEF_LAYOUT_MAJOR 126

typedef struct _cef_base_ref_counted_t
{
    size_t size;
    void(*add_ref)(struct _cef_base_ref_counted_t* self);
    int (*release)(struct _cef_base_ref_counted_t* self);
    int (*has_one_ref)(struct _cef_base_ref_counted_t* self);
    int (*has_at_least_one_ref)(struct _cef_base_ref_counted_t* self);
} cef_base_ref_counted_t;

struct _cef_browser_host_t;
struct _cef_dev_tools_message_observer_t;
struct _cef_registration_t;

// _cef_browser_t: base, is_valid, get_host, …; only get_host is used.
typedef struct _cef_browser_t
{
    cef_base_ref_counted_t base;
    void* is_valid;
    struct _cef_browser_host_t*(*get_host)(struct _cef_browser_t* self);
} cef_browser_t;

// _cef_browser_host_t: 24 methods (get_browser … has_dev_tools) sit between the
// base and the DevTools calls; the padding puts those calls at their real offsets.
typedef struct _cef_browser_host_t
{
    cef_base_ref_counted_t base;
    void* _pad[24];
    int (*send_dev_tools_message)(struct _cef_browser_host_t* self, const void* message, size_t message_size);
    int (*execute_dev_tools_method)(struct _cef_browser_host_t* self, int message_id, const void* method, void* params);
    struct _cef_registration_t*(*add_dev_tools_message_observer)(struct _cef_browser_host_t* self, struct _cef_dev_tools_message_observer_t* observer);
} cef_browser_host_t;

typedef struct _cef_dev_tools_message_observer_t
{
    cef_base_ref_counted_t base;
    int (*on_dev_tools_message)(struct _cef_dev_tools_message_observer_t* self, cef_browser_t* browser, const void* message, size_t message_size);
    void (*on_dev_tools_method_result)(struct _cef_dev_tools_message_observer_t* self, cef_browser_t* browser, int message_id, int success, const void* result,
                                       size_t result_size);
    void (*on_dev_tools_event)(struct _cef_dev_tools_message_observer_t* self, cef_browser_t* browser, const void* method, const void* params, size_t params_size);
    void (*on_dev_tools_agent_attached)(struct _cef_dev_tools_message_observer_t* self, cef_browser_t* browser);
    void (*on_dev_tools_agent_detached)(struct _cef_dev_tools_message_observer_t* self, cef_browser_t* browser);
} cef_dev_tools_message_observer_t;

// Returned by add_dev_tools_message_observer; release() unregisters.
typedef struct _cef_registration_t
{
    cef_base_ref_counted_t base;
} cef_registration_t;

// Browser-process methods must run on the UI thread; cef_post_task(TID_UI, task)
// gets us there. TID_UI is the first cef_thread_id_t, i.e. 0.
#define CEF_TID_UI 0
typedef struct _cef_task_t
{
    cef_base_ref_counted_t base;
    void (*execute)(struct _cef_task_t* self);
} cef_task_t;
