#ifndef PROXY_TRAMPOLINE_H
#define PROXY_TRAMPOLINE_H
#include "logger.h"
#include <X11/Xlib.h>
#include <X11/extensions/XTest.h>
#include <cstddef>
#include <ctime>
#include <dlfcn.h>
#include <sys/time.h>

#ifdef __cplusplus
extern "C"
{
#endif

#define HOOK_FUNC(name, ret_type, params, args)                                                                                                                                    \
    __attribute__((visibility("default"))) ret_type name params                                                                                                                    \
    {                                                                                                                                                                              \
        static ret_type(*orig) params = NULL;                                                                                                                                      \
        if (!orig) {                                                                                                                                                               \
            orig = (ret_type(*) params)dlsym(g_originalXTstInstance, #name);                                                                                                       \
            if (!orig) {                                                                                                                                                           \
                LOG_ERROR("Cannot find original %s", #name);                                                                                                                       \
                return False;                                                                                                                                                      \
            }                                                                                                                                                                      \
        }                                                                                                                                                                          \
        return orig args;                                                                                                                                                          \
    }

#ifdef __cplusplus
}
#endif

#endif // PROXY_TRAMPOLINE_H
