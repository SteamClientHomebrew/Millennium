#pragma once
// Point a libc symbol (execv/execve) at our own function at runtime.
//
// When the bootstrap is dlopen'd into an already-running steam_osx, the
// load-time DYLD_INTERPOSE in dylib_bootstrap.cc no longer applies. So we rebind
// here instead: find the live address of the target and overwrite the
// __DATA*/__AUTH* pointer slots that hold it. steam_osx then calls our execv
// when it relaunches the "Steam Helper" browser — the point where the bootstrap
// adds --remote-debugging-pipe.

#include <dlfcn.h>
#include <mach/mach.h>
#include <mach/mach_vm.h>
#include <mach-o/dyld.h>
#include <mach-o/loader.h>
#include <stdint.h>
#include <string.h>
#include <unistd.h>

// Overwrite every pointer slot currently holding `target` with `replacement`,
// across all loaded images; returns how many changed. `target` is typically
// dlsym(RTLD_DEFAULT, "execv"). Call once at bootstrap startup.
static inline int millennium_rebind_by_value(void* target, void* replacement)
{
    if (!target || !replacement) return 0;
    int patched = 0;
    const long page = getpagesize();
    for (uint32_t i = 0; i < _dyld_image_count(); i++) {
        const struct mach_header_64* h = (const struct mach_header_64*)_dyld_get_image_header(i);
        if (!h || h->magic != MH_MAGIC_64) continue;
        const intptr_t slide = _dyld_get_image_vmaddr_slide(i);
        const struct load_command* lc = (const struct load_command*)((uintptr_t)h + sizeof(*h));
        for (uint32_t c = 0; c < h->ncmds; c++) {
            if (lc->cmd == LC_SEGMENT_64) {
                const struct segment_command_64* s = (const struct segment_command_64*)lc;
                // GOT slots live in __DATA, __DATA_CONST, __AUTH, __AUTH_CONST.
                if (strncmp(s->segname, "__DATA", 6) == 0 || strncmp(s->segname, "__AUTH", 6) == 0) {
                    void** words = (void**)((uintptr_t)slide + s->vmaddr);
                    const long n = (long)(s->vmsize / sizeof(void*));
                    for (long j = 0; j < n; j++) {
                        if (words[j] != target) continue;
                        const uintptr_t base = (uintptr_t)&words[j] & ~(uintptr_t)(page - 1);
                        if (mach_vm_protect(mach_task_self(), base, page, 0,
                                            VM_PROT_READ | VM_PROT_WRITE | VM_PROT_COPY) == KERN_SUCCESS) {
                            words[j] = replacement;
                            patched++;
                        }
                    }
                }
            }
            lc = (const struct load_command*)((uintptr_t)lc + lc->cmdsize);
        }
    }
    return patched;
}
