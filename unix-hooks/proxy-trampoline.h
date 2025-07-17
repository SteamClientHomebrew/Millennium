#ifndef PROXY_TRAMPOLINE_H
#define PROXY_TRAMPOLINE_H
#include <cstddef>
#include <X11/Xlib.h>
#include <X11/extensions/XTest.h>
#include <dlfcn.h>
#include <sys/time.h>
#include <ctime>
#include "logger.h"

void* g_originalXTstInstance = NULL;

#define HOOK_FUNC(name, ret_type, params, args) \
ret_type name params { \
    static ret_type (*orig) params = NULL; \
    if (!orig) { \
        orig = (ret_type (*) params)dlsym(g_originalXTstInstance, #name); \
        if (!orig) { \
            LOG_ERROR("Cannot find original %s", #name); \
            return False; \
        } \
    } \
    return orig args; \
}

HOOK_FUNC(XTestFakeButtonEvent, int, (Display *dpy, unsigned int button, Bool is_press, unsigned long delay), (dpy, button, is_press, delay))
HOOK_FUNC(XTestFakeKeyEvent, int, (Display *dpy, unsigned int keycode, Bool is_press, unsigned long delay), (dpy, keycode, is_press, delay))
HOOK_FUNC(XTestQueryExtension, int, (Display *dpy, int *event_basep, int *error_basep, int *major_versionp, int *minor_versionp), (dpy, event_basep, error_basep, major_versionp, minor_versionp))
HOOK_FUNC(XTestFakeRelativeMotionEvent, int, (Display *dpy, int x, int y, unsigned long delay), (dpy, x, y, delay))
HOOK_FUNC(XTestFakeMotionEvent, int, (Display *dpy, int screen_number, int x, int y, unsigned long delay), (dpy, screen_number, x, y, delay))
#endif // PROXY_TRAMPOLINE_H