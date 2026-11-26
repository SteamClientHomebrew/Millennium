#ifdef __APPLE__
#include "cef_cdp_bridge.h"

#include <dispatch/dispatch.h>
#include <dlfcn.h>
#include <errno.h>
#include <fcntl.h>
#include <os/log.h>
#include <stdatomic.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>

// get_host / add_dev_tools_message_observer / send_dev_tools_message are
// browser-process methods and must run on the CEF UI thread: work reaches it
// through cef_post_task. Everything else (connecting, socket reads, writes) runs
// on a private serial dispatch queue, so the UI thread never blocks on the runtime.
static int (*cef_post_task)(int thread_id, cef_task_t* task);

static struct
{
    os_log_t log;
    dispatch_queue_t queue;
    dispatch_io_t channel;  // runtime socket, as a stream
    cef_browser_host_t* host;
    cef_registration_t* registration;
    char* frame;  // partial frame from the runtime; touched only on `queue`
    size_t frame_len;
} g;

// Ref counting for our own CEF objects. `type` holds `iface` (whose first member
// is the CEF base) and an atomic `refs`; `destroy` runs when the last ref goes.
#define CEF_REFCOUNTED(prefix, type, destroy)                                                                                                                                       \
    static void prefix##_add_ref(cef_base_ref_counted_t* self)                                                                                                                     \
    {                                                                                                                                                                              \
        atomic_fetch_add(&((type*)self)->refs, 1);                                                                                                                                 \
    }                                                                                                                                                                              \
    static int prefix##_release(cef_base_ref_counted_t* self)                                                                                                                      \
    {                                                                                                                                                                              \
        if (atomic_fetch_sub(&((type*)self)->refs, 1) != 1) return 0;                                                                                                              \
        destroy((type*)self);                                                                                                                                                      \
        return 1;                                                                                                                                                                  \
    }                                                                                                                                                                              \
    static int prefix##_has_one_ref(cef_base_ref_counted_t* self)                                                                                                                  \
    {                                                                                                                                                                              \
        return atomic_load(&((type*)self)->refs) == 1;                                                                                                                             \
    }                                                                                                                                                                              \
    static int prefix##_has_at_least_one_ref(cef_base_ref_counted_t* self)                                                                                                         \
    {                                                                                                                                                                              \
        return atomic_load(&((type*)self)->refs) >= 1;                                                                                                                             \
    }                                                                                                                                                                              \
    static type* prefix##_alloc(void)                                                                                                                                              \
    {                                                                                                                                                                              \
        type* object = calloc(1, sizeof(type));                                                                                                                                    \
        object->iface.base.size = sizeof(object->iface);                                                                                                                           \
        object->iface.base.add_ref = prefix##_add_ref;                                                                                                                             \
        object->iface.base.release = prefix##_release;                                                                                                                             \
        object->iface.base.has_one_ref = prefix##_has_one_ref;                                                                                                                     \
        object->iface.base.has_at_least_one_ref = prefix##_has_at_least_one_ref;                                                                                                   \
        atomic_store(&object->refs, 1);                                                                                                                                            \
        return object;                                                                                                                                                             \
    }

// --- observer: CEF -> runtime -----------------------------------------------

typedef struct
{
    cef_dev_tools_message_observer_t iface;
    _Atomic int refs;
} observer_t;

static void observer_destroy(observer_t* o)
{
    free(o);
}
CEF_REFCOUNTED(observer, observer_t, observer_destroy)

// Called on the UI thread; the write is queued on the stream, which keeps order.
static int on_dev_tools_message(cef_dev_tools_message_observer_t* self, cef_browser_t* browser, const void* message, size_t size)
{
    (void)self;
    (void)browser;
    char* frame = malloc(size + 1);
    memcpy(frame, message, size);
    frame[size] = '\0';
    dispatch_data_t data = dispatch_data_create(frame, size + 1, g.queue, DISPATCH_DATA_DESTRUCTOR_FREE);
    dispatch_io_write(g.channel, 0, data, g.queue, ^(bool done, dispatch_data_t rest, int error) {
        (void)rest;
        if (done && error && error != ECANCELED) os_log_error(g.log, "write to the runtime failed: %{errno}d", error);
    });
    dispatch_release(data);
    return 1;  // handled; skip the parsed method/event callbacks
}

// --- tasks: work posted to the CEF UI thread ---------------------------------

typedef struct cdp_task_t
{
    cef_task_t iface;
    _Atomic int refs;
    void (*run)(struct cdp_task_t*);
    cef_browser_t* browser;  // start task; owns a ref
    char* data;              // send tasks; owns the buffer
    size_t len;
} cdp_task_t;

static void task_destroy(cdp_task_t* t)
{
    free(t->data);
    free(t);
}
CEF_REFCOUNTED(task, cdp_task_t, task_destroy)

static void task_execute(cef_task_t* self)
{
    cdp_task_t* t = (cdp_task_t*)self;
    t->run(t);
}

// CEF adopts the task's reference and releases it after running.
static void post(void (*run)(cdp_task_t*), cef_browser_t* browser, const char* data, size_t len)
{
    cdp_task_t* t = task_alloc();
    t->iface.execute = task_execute;
    t->run = run;
    t->browser = browser;
    if (data) {
        t->data = malloc(len);
        memcpy(t->data, data, len);
        t->len = len;
    }
    cef_post_task(CEF_TID_UI, &t->iface);
}

static void run_send(cdp_task_t* t)
{
    g.host->send_dev_tools_message(g.host, t->data, t->len);
}

// --- reader: runtime -> CEF, on `queue` ---------------------------------------

// Append a chunk and post every complete NUL-terminated frame to the UI thread.
static void take_chunk(const char* bytes, size_t size)
{
    g.frame = realloc(g.frame, g.frame_len + size);
    memcpy(g.frame + g.frame_len, bytes, size);
    g.frame_len += size;

    size_t start = 0;
    for (char* end; (end = memchr(g.frame + start, '\0', g.frame_len - start));) {
        const size_t nul = (size_t)(end - g.frame);
        if (nul > start) post(run_send, NULL, g.frame + start, nul - start);
        start = nul + 1;
    }
    memmove(g.frame, g.frame + start, g.frame_len - start);
    g.frame_len -= start;
}

static void read_runtime(void)
{
    dispatch_io_read(g.channel, 0, SIZE_MAX, g.queue, ^(bool done, dispatch_data_t data, int error) {
        if (data) {
            dispatch_data_apply(data, ^bool(dispatch_data_t region, size_t offset, const void* bytes, size_t size) {
                (void)region;
                (void)offset;
                take_chunk(bytes, size);
                return true;
            });
        }
        if (done) {
            os_log(g.log, "runtime closed the CDP socket (%{errno}d)", error);
            dispatch_io_close(g.channel, DISPATCH_IO_STOP);
        }
    });
}

// --- start ----------------------------------------------------------------------

// On the UI thread: attach the observer, then start reading.
static void run_start(cdp_task_t* t)
{
    cef_browser_t* browser = t->browser;
    g.host = browser->get_host(browser);
    browser->base.release(&browser->base);

    // CEF adopts the observer's reference; the registration keeps it alive.
    observer_t* observer = observer_alloc();
    observer->iface.on_dev_tools_message = on_dev_tools_message;  // CEF skips the NULL callbacks
    g.registration = g.host->add_dev_tools_message_observer(g.host, &observer->iface);

    dispatch_async(g.queue, ^{
        read_runtime();
    });
    os_log(g.log, "CDP bridge attached");
}

// The runtime may not have bound its socket yet when the first resource request
// arrives, so retry every 100 ms for up to 10 s.
static void connect_runtime(cef_browser_t* browser, struct sockaddr_un addr, int attempts_left)
{
    const int fd = socket(AF_UNIX, SOCK_STREAM, 0);
    if (fd >= 0) fcntl(fd, F_SETFD, FD_CLOEXEC);  // keep it out of the renderers
    if (fd >= 0 && connect(fd, (struct sockaddr*)&addr, sizeof(addr)) == 0) {
        fcntl(fd, F_SETNOSIGPIPE, 1);
        g.channel = dispatch_io_create(DISPATCH_IO_STREAM, fd, g.queue, ^(int error) {
            (void)error;
            close(fd);
        });
        dispatch_io_set_low_water(g.channel, 1);
        post(run_start, browser, NULL, 0);  // the task takes over the browser ref
        return;
    }
    if (fd >= 0) close(fd);

    if (attempts_left == 0) {
        os_log_error(g.log, "could not reach the runtime at %{public}s", addr.sun_path);
        browser->base.release(&browser->base);
        return;
    }
    dispatch_after(dispatch_time(DISPATCH_TIME_NOW, 100 * NSEC_PER_MSEC), g.queue, ^{
        connect_runtime(browser, addr, attempts_left - 1);
    });
}

int millennium_cdp_bridge_start(cef_browser_t* browser, const char* socket_path, void* cef)
{
    g.log = os_log_create("app.steambrew.millennium", "cdp");

    int (*version_info)(int entry) = cef ? dlsym(cef, "cef_version_info") : NULL;
    cef_post_task = cef ? dlsym(cef, "cef_post_task") : NULL;
    if (!version_info || !cef_post_task) {
        os_log_error(g.log, "CEF exports are missing; the CDP bridge stays off");
        return -1;
    }
    const int major = version_info(0);
    if (major != CEF_LAYOUT_MAJOR) {
        os_log_error(g.log, "CEF %d is not the CEF %d layout this bridge was built for; the CDP bridge stays off", major, CEF_LAYOUT_MAJOR);
        return -1;
    }

    struct sockaddr_un addr = { .sun_family = AF_UNIX };
    snprintf(addr.sun_path, sizeof(addr.sun_path), "%s", socket_path);
    g.queue = dispatch_queue_create("app.steambrew.millennium.cdp", DISPATCH_QUEUE_SERIAL);
    browser->base.add_ref(&browser->base);  // released by the start task, or on giving up
    dispatch_async(g.queue, ^{
        connect_runtime(browser, addr, 100);
    });
    return 0;
}
#endif
