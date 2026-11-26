#ifdef __APPLE__
#include "cef_cdp_bridge.h"

#include <dlfcn.h>
#include <errno.h>
#include <fcntl.h>
#include <os/log.h>
#include <pthread.h>
#include <stdatomic.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>

// get_host / add_dev_tools_message_observer / send_dev_tools_message are
// browser-process methods and must run on the CEF UI thread, so the browser is
// captured on whatever thread the hook fires on, then handed to the UI thread
// via cef_post_task.
static int (*cef_post_task)(int thread_id, cef_task_t* task);

static struct
{
    os_log_t log;
    int fd;  // runtime socket
    cef_browser_host_t* host;
    cef_registration_t* registration;
    pthread_mutex_t write_lock;
} g = { .fd = -1, .write_lock = PTHREAD_MUTEX_INITIALIZER };

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

static int write_all(const void* buf, size_t n)
{
    const char* p = buf;
    while (n) {
        ssize_t w = write(g.fd, p, n);
        if (w > 0) {
            p += w;
            n -= (size_t)w;
        } else if (w < 0 && errno == EINTR) {
            continue;
        } else {
            return -1;
        }
    }
    return 0;
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

static int on_dev_tools_message(cef_dev_tools_message_observer_t* self, cef_browser_t* browser, const void* message, size_t size)
{
    (void)self;
    (void)browser;
    pthread_mutex_lock(&g.write_lock);
    char nul = '\0';
    if (write_all(message, size) == 0) write_all(&nul, 1);
    pthread_mutex_unlock(&g.write_lock);
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

static cdp_task_t* task_new(void (*run)(cdp_task_t*))
{
    cdp_task_t* t = task_alloc();
    t->iface.execute = task_execute;
    t->run = run;
    return t;
}

// CEF adopts the task's reference and releases it after running.
static void post(cdp_task_t* t)
{
    cef_post_task(CEF_TID_UI, &t->iface);
}

static void run_send(cdp_task_t* t)
{
    g.host->send_dev_tools_message(g.host, t->data, t->len);
}

static void post_send(const char* data, size_t len)
{
    cdp_task_t* t = task_new(run_send);
    t->data = malloc(len);
    memcpy(t->data, data, len);
    t->len = len;
    post(t);
}

// --- reader: runtime socket -> CEF ------------------------------------------

static void* reader_loop(void* arg)
{
    (void)arg;
    char* buf = NULL;
    size_t cap = 0, len = 0;
    char chunk[8192];
    for (;;) {
        ssize_t got = read(g.fd, chunk, sizeof(chunk));
        if (got < 0 && errno == EINTR) continue;
        if (got <= 0) break;
        if (len + (size_t)got > cap) {
            cap = (len + (size_t)got) * 2;
            buf = realloc(buf, cap);
        }
        memcpy(buf + len, chunk, (size_t)got);
        len += (size_t)got;
        size_t start = 0;
        for (char* end; (end = memchr(buf + start, '\0', len - start));) {
            size_t nul = (size_t)(end - buf);
            if (nul > start) post_send(buf + start, nul - start);
            start = nul + 1;
        }
        memmove(buf, buf + start, len - start);
        len -= start;
    }
    free(buf);
    os_log(g.log, "runtime closed the CDP socket");
    return NULL;
}

// --- start, on the UI thread ------------------------------------------------

static void run_start(cdp_task_t* t)
{
    cef_browser_t* browser = t->browser;
    g.host = browser->get_host(browser);
    browser->base.release(&browser->base);

    // CEF adopts the observer's reference; the registration keeps it alive.
    observer_t* observer = observer_alloc();
    observer->iface.on_dev_tools_message = on_dev_tools_message;  // CEF skips the NULL callbacks
    g.registration = g.host->add_dev_tools_message_observer(g.host, &observer->iface);

    pthread_t reader;
    pthread_create(&reader, NULL, reader_loop, NULL);
    pthread_detach(reader);
    os_log(g.log, "CDP bridge attached");
}

// The runtime may not have bound its socket yet when the first resource request
// arrives, so retry for up to 10 s.
static int connect_runtime(const char* path)
{
    struct sockaddr_un addr = { .sun_family = AF_UNIX };
    snprintf(addr.sun_path, sizeof(addr.sun_path), "%s", path);
    for (int attempt = 0; attempt < 100; ++attempt) {
        int fd = socket(AF_UNIX, SOCK_STREAM, 0);
        if (fd < 0) return -1;
        fcntl(fd, F_SETFD, FD_CLOEXEC);  // keep it out of the renderers
        if (connect(fd, (struct sockaddr*)&addr, sizeof(addr)) == 0) return fd;
        close(fd);
        usleep(100 * 1000);
    }
    return -1;
}

typedef struct
{
    cef_browser_t* browser;
    char path[sizeof(((struct sockaddr_un*)0)->sun_path)];
} start_args_t;

static void* connect_worker(void* arg)
{
    start_args_t* a = arg;
    g.fd = connect_runtime(a->path);
    if (g.fd < 0) {
        os_log_error(g.log, "could not reach the runtime at %{public}s (%{public}s)", a->path, strerror(errno));
        a->browser->base.release(&a->browser->base);
    } else {
        cdp_task_t* t = task_new(run_start);
        t->browser = a->browser;  // the task takes over our reference
        post(t);
    }
    free(a);
    return NULL;
}

int millennium_cdp_bridge_start(cef_browser_t* browser, const char* socket_path)
{
    g.log = os_log_create("app.steambrew.millennium", "cdp");

    // libcef is already in the process; its C exports resolve through the global scope.
    int (*version_info)(int entry) = dlsym(RTLD_DEFAULT, "cef_version_info");
    cef_post_task = dlsym(RTLD_DEFAULT, "cef_post_task");
    if (!version_info || !cef_post_task) {
        os_log_error(g.log, "CEF exports are missing; the CDP bridge stays off");
        return -1;
    }
    const int major = version_info(0);
    if (major != CEF_LAYOUT_MAJOR) {
        os_log_error(g.log, "CEF %d is not the CEF %d layout this bridge was built for; the CDP bridge stays off", major, CEF_LAYOUT_MAJOR);
        return -1;
    }

    start_args_t* a = calloc(1, sizeof(*a));
    if (!a) return -1;
    a->browser = browser;
    snprintf(a->path, sizeof(a->path), "%s", socket_path);
    browser->base.add_ref(&browser->base);  // released by connect_worker or the start task

    pthread_t worker;
    if (pthread_create(&worker, NULL, connect_worker, a) != 0) {
        browser->base.release(&browser->base);
        free(a);
        return -1;
    }
    pthread_detach(worker);
    return 0;
}
#endif
