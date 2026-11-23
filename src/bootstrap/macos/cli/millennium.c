// millennium — the macOS command for Millennium.
//
//   millennium install [--from DIR] setup + self-sign + load on every launch
//   millennium setup [--from DIR]   copy the built runtime into place
//   millennium load                 load Millennium into the running Steam
//   millennium watch                load on every Steam launch (stays resident)
//   millennium status               show runtime + Steam state
//   millennium uninstall            remove the staged runtime (keeps plugins/config)
//
// `install` is what the one-line installer (curl … | bash) runs after downloading
// the build; it ad-hoc self-signs here on the user's machine (no Apple account),
// so the first load only ever asks for one system authorization.
//
// Like the Windows and Linux builds, Millennium runs as a dylib loaded into the
// Steam client. On macOS `load` loads it into the steam_osx you already have
// open, the same way a debugger loads a library into a running process: take the
// process's task port and have it call its own dlopen. It needs
// com.apple.security.cs.debugger (see millennium.entitlements); no sudo, and no
// Steam file is touched.

#if !defined(__APPLE__) || !defined(__aarch64__)
#error "millennium targets arm64 macOS only."
#endif

#include <copyfile.h>
#include <dirent.h>
#include <dlfcn.h>
#include <stdarg.h>
#include <libgen.h>
#include <libproc.h>
#include <mach/mach.h>
#include <mach/mach_vm.h>
#include <mach/thread_status.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#define STEAM_MAIN_SUFFIX "/Steam.AppBundle/Steam/Contents/MacOS/steam_osx"
#define DEBUG_MARKER ".cef-enable-remote-debugging"
#define RETURN_SENTINEL 0xdead0000dead0000ull  // dlopen returns here and faults; we catch it

extern NDR_record_t NDR_record;

#pragma pack(4)
typedef struct {
    mach_msg_header_t Head;
    mach_msg_body_t body;
    mach_msg_port_descriptor_t thread;
    mach_msg_port_descriptor_t task;
    NDR_record_t NDR;
    exception_type_t exception;
    mach_msg_type_number_t codeCnt;
    int64_t code[2];
    char trailer[64];
} exc_request_t;
#pragma pack()

static const char* runtime_dir(void)
{
    static char dir[PATH_MAX];
    if (!dir[0]) {
        const char* home = getenv("HOME");
        snprintf(dir, sizeof(dir), "%s/Library/Application Support/Millennium/runtime", home ? home : "");
    }
    return dir;
}

static void mkdirs(const char* path)
{
    char buf[PATH_MAX];
    snprintf(buf, sizeof(buf), "%s", path);
    for (char* p = buf + 1; *p; p++) {
        if (*p != '/') continue;
        *p = '\0';
        mkdir(buf, 0755);
        *p = '/';
    }
    mkdir(buf, 0755);
}

// Match steam_osx by its executable path, so a CEF helper that merely carries
// that path in its arguments is not mistaken for the main process.
static pid_t find_steam(char* out_path, size_t out_size)
{
    pid_t pids[4096];
    int n = proc_listallpids(pids, sizeof(pids));
    char path[PROC_PIDPATHINFO_MAXSIZE];
    size_t suffix = strlen(STEAM_MAIN_SUFFIX);
    for (int i = 0; i < n; i++) {
        if (pids[i] <= 0) continue;
        int len = proc_pidpath(pids[i], path, sizeof(path));
        if (len <= 0 || (size_t)len < suffix) continue;
        if (strcmp(path + len - suffix, STEAM_MAIN_SUFFIX) != 0) continue;
        if (out_path && out_size) snprintf(out_path, out_size, "%s", path);
        return pids[i];
    }
    return 0;
}

static void drop_debug_marker(const char* steam_path)
{
    char dir[PATH_MAX];
    snprintf(dir, sizeof(dir), "%s", steam_path);
    char marker[PATH_MAX];
    snprintf(marker, sizeof(marker), "%s/%s", dirname(dir), DEBUG_MARKER);
    unlink(marker);  // keep the TCP debug port closed; the pipe is all we use
}

static int load_into(pid_t pid, const char* dylib)
{
    void* dlopen_addr = dlsym(RTLD_DEFAULT, "dlopen");
    void* spawn_addr = dlsym(RTLD_DEFAULT, "pthread_create_from_mach_thread");
    if (!dlopen_addr || !spawn_addr) {
        fprintf(stderr, "millennium: cannot resolve dlopen / pthread_create_from_mach_thread\n");
        return 1;
    }

    mach_port_t task = MACH_PORT_NULL;
    kern_return_t kr = task_for_pid(mach_task_self(), pid, &task);
    if (kr != KERN_SUCCESS) {
        fprintf(stderr, "millennium: task_for_pid(%d) failed: %s — is the binary signed with cs.debugger?\n",
                pid, mach_error_string(kr));
        return 1;
    }

    // the path of the dylib to load
    size_t len = strlen(dylib) + 1;
    mach_vm_address_t path_region = 0;
    mach_vm_allocate(task, &path_region, 0x1000, VM_FLAGS_ANYWHERE);
    mach_vm_write(task, path_region, (vm_offset_t)dylib, (mach_msg_type_number_t)len);

    mach_vm_address_t stack = 0;
    mach_vm_allocate(task, &stack, 0x200000, VM_FLAGS_ANYWHERE);

    // a fresh thread, suspended until the fault handler is armed
    thread_act_t th = MACH_PORT_NULL;
    kr = thread_create(task, &th);
    if (kr != KERN_SUCCESS) {
        fprintf(stderr, "millennium: thread_create failed: %s\n", mach_error_string(kr));
        return 1;
    }

    mach_port_t exc = MACH_PORT_NULL;
    mach_port_allocate(mach_task_self(), MACH_PORT_RIGHT_RECEIVE, &exc);
    mach_port_insert_right(mach_task_self(), exc, exc, MACH_MSG_TYPE_MAKE_SEND);
    thread_set_exception_ports(th, EXC_MASK_BAD_ACCESS | EXC_MASK_BAD_INSTRUCTION, exc,
                               EXCEPTION_DEFAULT | MACH_EXCEPTION_CODES, ARM_THREAD_STATE64);

    // Have a real pthread call dlopen(path): pthread_create_from_mach_thread
    // gives it proper TLS. x2 = start = dlopen, x3 = arg = path; lr = sentinel so
    // the return faults and the handler below can reap this thread.
    arm_thread_state64_t state;
    memset(&state, 0, sizeof(state));
    __darwin_arm_thread_state64_set_pc_fptr(state, spawn_addr);
    state.__x[0] = path_region + 0x800;  // scratch tid slot in the same page
    state.__x[1] = 0;
    state.__x[2] = (uint64_t)dlopen_addr;
    state.__x[3] = path_region;
    state.__lr = RETURN_SENTINEL;
    state.__sp = (stack + 0x200000 - 0x1000) & ~0xFULL;
    thread_set_state(th, ARM_THREAD_STATE64, (thread_state_t)&state, ARM_THREAD_STATE64_COUNT);
    thread_resume(th);

    exc_request_t req;
    memset(&req, 0, sizeof(req));
    kr = mach_msg(&req.Head, MACH_RCV_MSG | MACH_RCV_TIMEOUT, 0, sizeof(req), exc, 8000, MACH_PORT_NULL);
    if (kr == KERN_SUCCESS) usleep(1500 * 1000);  // let dlopen finish (runs the bootstrap constructor)

    thread_terminate(th);
    mach_port_deallocate(mach_task_self(), exc);

    if (kr != KERN_SUCCESS) {
        fprintf(stderr, "millennium: load did not complete (no sentinel fault)\n");
        return 1;
    }
    return 0;
}

static int bootstrap_path(char* out, size_t size)
{
    snprintf(out, size, "%s/libmillennium_bootstrap.dylib", runtime_dir());
    return access(out, R_OK) == 0;
}

static int cmd_load(void)
{
    char boot[PATH_MAX];
    if (!bootstrap_path(boot, sizeof(boot))) {
        fprintf(stderr, "millennium: no staged runtime — run `millennium setup` first\n");
        return 1;
    }
    char steam_path[PATH_MAX];
    pid_t pid = find_steam(steam_path, sizeof(steam_path));
    if (!pid) {
        fprintf(stderr, "millennium: Steam is not running — open it first\n");
        return 1;
    }
    drop_debug_marker(steam_path);
    printf("millennium: loading into steam_osx (pid %d)\n", pid);
    if (load_into(pid, boot) != 0) return 1;
    printf("millennium: loaded\n");
    return 0;
}

static int cmd_watch(void)
{
    char boot[PATH_MAX];
    if (!bootstrap_path(boot, sizeof(boot))) {
        fprintf(stderr, "millennium: no staged runtime — run `millennium setup` first\n");
        return 1;
    }
    printf("millennium: watching for Steam — open it normally and it loads\n");
    pid_t loaded = 0;
    for (;;) {
        char steam_path[PATH_MAX];
        pid_t pid = find_steam(steam_path, sizeof(steam_path));
        if (pid && pid != loaded) {
            drop_debug_marker(steam_path);
            if (load_into(pid, boot) == 0) {
                printf("millennium: loaded into pid %d\n", pid);
                loaded = pid;
            }
        } else if (!pid) {
            loaded = 0;  // Steam closed; arm for the next launch
        }
        sleep(2);
    }
}

static const char* kPayload[] = {
    "libmillennium.dylib",
    "libmillennium_bootstrap.dylib",
    "libmillennium_child_hook.dylib",
    "libmillennium_hhx64.dylib",
    "millennium",
};

static int copy_one(const char* src_dir, const char* name, const char* dst_dir)
{
    char src[PATH_MAX], dst[PATH_MAX];
    snprintf(src, sizeof(src), "%s/%s", src_dir, name);
    snprintf(dst, sizeof(dst), "%s/%s", dst_dir, name);
    if (copyfile(src, dst, NULL, COPYFILE_ALL | COPYFILE_UNLINK) != 0) {
        fprintf(stderr, "millennium: missing build artifact %s\n", src);
        return 1;
    }
    return 0;
}

static int cmd_setup(const char* from)
{
    char self[PATH_MAX];
    char from_buf[PATH_MAX];
    if (!from) {
        char argv0[PATH_MAX];
        uint32_t n = sizeof(argv0);
        extern int _NSGetExecutablePath(char*, uint32_t*);
        if (_NSGetExecutablePath(argv0, &n) == 0 && realpath(argv0, self)) {
            snprintf(from_buf, sizeof(from_buf), "%s", dirname(self));
            from = from_buf;
        }
    }
    if (!from) {
        fprintf(stderr, "millennium: pass --from <build-dir>\n");
        return 1;
    }

    const char* dst = runtime_dir();
    mkdirs(dst);

    for (size_t i = 0; i < sizeof(kPayload) / sizeof(*kPayload); i++) {
        if (copy_one(from, kPayload[i], dst) != 0) return 1;
    }

    // luavm's name is arch-dependent; take whatever matches.
    int have_luavm = 0;
    DIR* d = opendir(from);
    if (d) {
        struct dirent* e;
        while ((e = readdir(d))) {
            if (strncmp(e->d_name, "libmillennium_luavm", 19) == 0 || strncmp(e->d_name, "millennium.luavm", 16) == 0) {
                if (copy_one(from, e->d_name, dst) == 0) have_luavm = 1;
            }
        }
        closedir(d);
    }
    if (!have_luavm) {
        fprintf(stderr, "millennium: missing libmillennium_luavm_* in %s\n", from);
        return 1;
    }

    char p[PATH_MAX];
    snprintf(p, sizeof(p), "%s/millennium", dst);
    chmod(p, 0755);
    printf("millennium: runtime staged in %s\n", dst);
    return 0;
}

static int cmd_status(void)
{
    char boot[PATH_MAX];
    printf("runtime: %s\n", bootstrap_path(boot, sizeof(boot)) ? runtime_dir() : "not staged");
    pid_t pid = find_steam(NULL, 0);
    printf("steam:   %s\n", pid ? "running" : "not running");
    return 0;
}

// install = stage, clear the download quarantine, ad-hoc self-sign (no Apple
// account needed), and register a LaunchAgent that loads on every Steam launch.

static const char* kAgentLabel = "com.millennium.loader";

static void agent_plist_path(char* out, size_t size)
{
    const char* home = getenv("HOME");
    snprintf(out, size, "%s/Library/LaunchAgents/%s.plist", home ? home : "", kAgentLabel);
}

static int run(const char* fmt, ...)
{
    char cmd[2 * PATH_MAX];
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(cmd, sizeof(cmd), fmt, ap);
    va_end(ap);
    return system(cmd);
}

static int cmd_install(const char* from)
{
    if (cmd_setup(from) != 0) return 1;
    const char* dst = runtime_dir();
    const unsigned uid = (unsigned)getuid();

    run("xattr -dr com.apple.quarantine '%s' 2>/dev/null", dst);

    char ents[PATH_MAX];
    snprintf(ents, sizeof(ents), "%s/.entitlements.plist", dst);
    FILE* f = fopen(ents, "w");
    if (f) {
        fputs("<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
              "<plist version=\"1.0\"><dict>\n"
              "  <key>com.apple.security.cs.debugger</key><true/>\n"
              "</dict></plist>\n", f);
        fclose(f);
    }
    run("codesign -f -s - --entitlements '%s' '%s/millennium'", ents, dst);
    unlink(ents);

    char plist[PATH_MAX];
    agent_plist_path(plist, sizeof(plist));
    run("mkdir -p '%s/Library/LaunchAgents'", getenv("HOME"));
    f = fopen(plist, "w");
    if (f) {
        fprintf(f,
            "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
            "<plist version=\"1.0\"><dict>\n"
            "  <key>Label</key><string>%s</string>\n"
            "  <key>ProgramArguments</key><array><string>%s/millennium</string><string>watch</string></array>\n"
            "  <key>RunAtLoad</key><true/>\n"
            "  <key>KeepAlive</key><true/>\n"
            "</dict></plist>\n",
            kAgentLabel, dst);
        fclose(f);
    }
    run("launchctl bootout gui/%u '%s' 2>/dev/null; launchctl bootstrap gui/%u '%s' 2>/dev/null",
        uid, plist, uid, plist);

    printf("millennium: installed — open Steam and it loads automatically\n");
    return 0;
}

static int cmd_uninstall(void)
{
    char plist[PATH_MAX];
    agent_plist_path(plist, sizeof(plist));
    run("launchctl bootout gui/%u '%s' 2>/dev/null", (unsigned)getuid(), plist);
    unlink(plist);

    const char* dst = runtime_dir();
    for (size_t i = 0; i < sizeof(kPayload) / sizeof(*kPayload); i++) {
        char p[PATH_MAX];
        snprintf(p, sizeof(p), "%s/%s", dst, kPayload[i]);
        unlink(p);
    }
    DIR* d = opendir(dst);
    if (d) {
        struct dirent* e;
        while ((e = readdir(d))) {
            if (strncmp(e->d_name, "libmillennium_luavm", 19) == 0 || strncmp(e->d_name, "millennium.luavm", 16) == 0) {
                char p[PATH_MAX];
                snprintf(p, sizeof(p), "%s/%s", dst, e->d_name);
                unlink(p);
            }
        }
        closedir(d);
    }
    rmdir(dst);  // only succeeds if empty; plugins/config live elsewhere
    printf("millennium: removed staged runtime (plugins and config kept)\n");
    return 0;
}

static int usage(void)
{
    fprintf(stderr,
            "usage: millennium <command>\n"
            "  install [--from DIR] setup + self-sign + autostart on every Steam launch\n"
            "  setup [--from DIR]   stage the built runtime only\n"
            "  load                 load Millennium into the running Steam\n"
            "  watch                load on every Steam launch (stays resident)\n"
            "  status               show runtime + Steam state\n"
            "  uninstall            remove the staged runtime + autostart\n");
    return 2;
}

int main(int argc, char** argv)
{
    if (argc < 2) return usage();
    const char* cmd = argv[1];
    if (strcmp(cmd, "install") == 0 || strcmp(cmd, "setup") == 0) {
        const char* from = (argc >= 4 && strcmp(argv[2], "--from") == 0) ? argv[3] : NULL;
        return cmd[0] == 'i' ? cmd_install(from) : cmd_setup(from);
    }
    if (strcmp(cmd, "load") == 0) return cmd_load();
    if (strcmp(cmd, "watch") == 0) return cmd_watch();
    if (strcmp(cmd, "status") == 0) return cmd_status();
    if (strcmp(cmd, "uninstall") == 0) return cmd_uninstall();
    return usage();
}
