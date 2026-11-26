// millennium — the macOS command for Millennium. See cli/README.md.

#if !defined(__APPLE__) || !defined(__aarch64__)
#error "millennium targets arm64 macOS only."
#endif

#include <CommonCrypto/CommonDigest.h>
#include <copyfile.h>
#include <removefile.h>
#include <crt_externs.h>
#include <dirent.h>
#include <dlfcn.h>
#include <errno.h>
#include <libgen.h>
#include <libproc.h>
#include <limits.h>
#include <mach-o/dyld.h>
#include <mach/mach.h>
#include <mach/mach_vm.h>
#include <mach/thread_status.h>
#include <os/log.h>
#include <spawn.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#include "native.h"

#define STEAM_MAIN_SUFFIX "/Steam.AppBundle/Steam/Contents/MacOS/steam_osx"
#define STEAM_BUNDLE_ID "com.valvesoftware.steam"
#define BOOTSTRAP_NAME "libmillennium_bootstrap.dylib"
#define DEBUG_MARKER ".cef-enable-remote-debugging"
#define PATH_LINK "/usr/local/bin/millennium"
#define AGENT_LABEL "com.millennium.loader"
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

// Match steam_osx by its executable path. /Applications/Steam.app shares the
// bundle id and executable name but is only the launcher, and a CEF helper may
// carry the path in its arguments.
static int is_steam_main(pid_t pid, char* out_path, size_t out_size)
{
    char path[PROC_PIDPATHINFO_MAXSIZE];
    int len = proc_pidpath(pid, path, sizeof(path));
    size_t suffix = strlen(STEAM_MAIN_SUFFIX);
    if (len <= 0 || (size_t)len < suffix || strcmp(path + len - suffix, STEAM_MAIN_SUFFIX) != 0) return 0;
    if (out_path && out_size) snprintf(out_path, out_size, "%s", path);
    return 1;
}

static pid_t find_steam(char* out_path, size_t out_size)
{
    pid_t pids[4096];
    int n = proc_listallpids(pids, sizeof(pids));
    for (int i = 0; i < n; i++) {
        if (pids[i] > 0 && is_steam_main(pids[i], out_path, out_size)) return pids[i];
    }
    return 0;
}

static void drop_debug_marker(const char* steam_path)
{
    char dir[PATH_MAX];
    snprintf(dir, sizeof(dir), "%s", steam_path);
    char marker[PATH_MAX];
    snprintf(marker, sizeof(marker), "%s/%s", dirname(dir), DEBUG_MARKER);
    unlink(marker);  // keep Steam's TCP debug port closed
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
    // The fault means the pthread running dlopen exists; it no longer needs this
    // thread or its stack. The path stays mapped for dlopen to read.
    kr = mach_msg(&req.Head, MACH_RCV_MSG | MACH_RCV_TIMEOUT, 0, sizeof(req), exc, 8000, MACH_PORT_NULL);
    thread_terminate(th);
    mach_port_deallocate(mach_task_self(), th);
    mach_port_mod_refs(mach_task_self(), exc, MACH_PORT_RIGHT_RECEIVE, -1);
    mach_vm_deallocate(task, stack, 0x200000);
    mach_port_deallocate(mach_task_self(), task);

    if (kr != KERN_SUCCESS) {
        fprintf(stderr, "millennium: load did not complete (no sentinel fault)\n");
        return 1;
    }
    return 0;
}

static int bootstrap_path(char* out, size_t size)
{
    snprintf(out, size, "%s/current/%s", runtime_dir(), BOOTSTRAP_NAME);
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

static char g_boot[PATH_MAX];

static void on_steam_launch(pid_t pid)
{
    static os_log_t log;
    if (!log) log = os_log_create("app.steambrew.millennium", "agent");
    char steam_path[PATH_MAX];
    if (!is_steam_main(pid, steam_path, sizeof(steam_path))) return;
    drop_debug_marker(steam_path);
    if (load_into(pid, g_boot) == 0) {
        os_log(log, "loaded into steam_osx (pid %d)", pid);
    } else {
        os_log_error(log, "could not load into steam_osx (pid %d)", pid);
    }
}

// Sleeps until Steam launches; NSWorkspace wakes it, so there is no polling.
static int cmd_watch(void)
{
    if (!bootstrap_path(g_boot, sizeof(g_boot))) {
        fprintf(stderr, "millennium: no staged runtime — run `millennium setup` first\n");
        return 1;
    }
    printf("millennium: watching for Steam — open it normally and it loads\n");
    watch_app_launches(STEAM_BUNDLE_ID, on_steam_launch);
    return 0;
}

static const char* kPayload[] = {
    "libmillennium.dylib",
    "libmillennium_bootstrap.dylib",
    "libmillennium_child_hook.dylib",
    "libmillennium_hhx64.dylib",
    "millennium",
};

static int is_luavm(const char* name)
{
    return strncmp(name, "libmillennium_luavm", 19) == 0 || strncmp(name, "millennium.luavm", 16) == 0;
}

// Remove a file, symlink or directory tree.
static void remove_tree(const char* path)
{
    removefile(path, NULL, REMOVEFILE_RECURSIVE);
}

static int spawn(char* const argv[])
{
    pid_t pid;
    if (posix_spawnp(&pid, argv[0], NULL, NULL, argv, *_NSGetEnviron()) != 0) return -1;
    int status;
    while (waitpid(pid, &status, 0) < 0 && errno == EINTR) {}
    return status;
}

// The payload file names: kPayload plus whichever luavm the build produced.
static size_t collect_payload(const char* from, char list[][NAME_MAX + 1], size_t max)
{
    size_t n = 0;
    for (size_t i = 0; i < sizeof(kPayload) / sizeof(*kPayload) && n < max; i++) {
        snprintf(list[n++], NAME_MAX + 1, "%s", kPayload[i]);
    }
    DIR* d = opendir(from);
    if (d) {
        struct dirent* e;
        while ((e = readdir(d)) && n < max) {
            if (is_luavm(e->d_name)) snprintf(list[n++], NAME_MAX + 1, "%s", e->d_name);
        }
        closedir(d);
    }
    return n;
}

// A release is named by the SHA-256 of its payload (names and bytes), truncated
// to 64 bits — identical builds land in the same release directory.
static int payload_hash(const char* from, char list[][NAME_MAX + 1], size_t n, char out[17])
{
    CC_SHA256_CTX ctx;
    CC_SHA256_Init(&ctx);
    char buf[65536];
    for (size_t i = 0; i < n; i++) {
        char path[PATH_MAX];
        snprintf(path, sizeof(path), "%s/%s", from, list[i]);
        FILE* f = fopen(path, "rb");
        if (!f) {
            fprintf(stderr, "millennium: missing build artifact %s\n", path);
            return -1;
        }
        CC_SHA256_Update(&ctx, list[i], (CC_LONG)strlen(list[i]) + 1);
        size_t got;
        while ((got = fread(buf, 1, sizeof(buf), f)) > 0) CC_SHA256_Update(&ctx, buf, (CC_LONG)got);
        fclose(f);
    }
    unsigned char digest[CC_SHA256_DIGEST_LENGTH];
    CC_SHA256_Final(digest, &ctx);
    for (int i = 0; i < 8; i++) snprintf(out + i * 2, 3, "%02x", digest[i]);
    return 0;
}

static void point_current(const char* runtime, const char* release_name)
{
    char tmp[PATH_MAX], cur[PATH_MAX];
    snprintf(tmp, sizeof(tmp), "%s/.current.%d", runtime, getpid());
    snprintf(cur, sizeof(cur), "%s/current", runtime);
    unlink(tmp);
    symlink(release_name, tmp);  // relative, so the tree can be moved
    rename(tmp, cur);
}

// Drop every release but the current one, and any staging left by a crash.
static void prune(const char* runtime, const char* keep)
{
    DIR* d = opendir(runtime);
    if (!d) return;
    struct dirent* e;
    while ((e = readdir(d))) {
        int stale = (strncmp(e->d_name, "release-", 8) == 0 && strcmp(e->d_name, keep) != 0) ||
                    strncmp(e->d_name, ".stage.", 7) == 0 || strncmp(e->d_name, ".current.", 9) == 0;
        if (!stale) continue;
        char path[PATH_MAX];
        snprintf(path, sizeof(path), "%s/%s", runtime, e->d_name);
        remove_tree(path);
    }
    closedir(d);
}

// Resolve the build directory: --from, else the directory this binary runs from.
static int resolve_from(const char* from, char out[PATH_MAX])
{
    if (from) {
        snprintf(out, PATH_MAX, "%s", from);
        return 0;
    }
    char self[PATH_MAX], argv0[PATH_MAX];
    uint32_t n = sizeof(argv0);
    if (_NSGetExecutablePath(argv0, &n) != 0 || !realpath(argv0, self)) {
        fprintf(stderr, "millennium: pass --from <build-dir>\n");
        return -1;
    }
    snprintf(out, PATH_MAX, "%s", dirname(self));
    return 0;
}

static int cmd_setup(const char* from_arg)
{
    char from[PATH_MAX];
    if (resolve_from(from_arg, from) != 0) return 1;

    char list[16][NAME_MAX + 1];
    size_t n = collect_payload(from, list, 16);
    char hash[17] = { 0 };
    if (payload_hash(from, list, n, hash) != 0) return 1;

    const char* runtime = runtime_dir();
    char release_name[32], release[PATH_MAX];
    snprintf(release_name, sizeof(release_name), "release-%s", hash);
    snprintf(release, sizeof(release), "%s/%s", runtime, release_name);

    if (access(release, F_OK) != 0) {
        mkpath_np(runtime, 0755);
        char stage[PATH_MAX];
        snprintf(stage, sizeof(stage), "%s/.stage.%d", runtime, getpid());
        remove_tree(stage);
        mkdir(stage, 0755);
        for (size_t i = 0; i < n; i++) {
            char src[PATH_MAX], dst[PATH_MAX];
            snprintf(src, sizeof(src), "%s/%s", from, list[i]);
            snprintf(dst, sizeof(dst), "%s/%s", stage, list[i]);
            if (copyfile(src, dst, NULL, COPYFILE_ALL | COPYFILE_CLONE) != 0) {
                fprintf(stderr, "millennium: failed to copy %s\n", src);
                remove_tree(stage);
                return 1;
            }
        }
        char cli[PATH_MAX];
        snprintf(cli, sizeof(cli), "%s/millennium", stage);
        chmod(cli, 0755);
        if (rename(stage, release) != 0) {
            remove_tree(stage);
            return 1;
        }
    }

    point_current(runtime, release_name);
    prune(runtime, release_name);
    printf("millennium: staged %s\n", release_name);
    return 0;
}

static int cmd_status(void)
{
    char cur[PATH_MAX], target[PATH_MAX];
    snprintf(cur, sizeof(cur), "%s/current", runtime_dir());
    ssize_t len = readlink(cur, target, sizeof(target) - 1);
    if (len > 0) {
        target[len] = '\0';
        printf("runtime: %s\n", target);
    } else {
        printf("runtime: not staged\n");
    }
    printf("steam:   %s\n", find_steam(NULL, 0) ? "running" : "not running");
    return 0;
}

static void agent_plist_path(char* out, size_t size)
{
    snprintf(out, size, "%s/Library/LaunchAgents/%s.plist", getenv("HOME"), AGENT_LABEL);
}

static int cmd_install(const char* from)
{
    if (cmd_setup(from) != 0) return 1;
    const char* runtime = runtime_dir();
    char cli[PATH_MAX];
    snprintf(cli, sizeof(cli), "%s/current/millennium", runtime);

    spawn((char*[]){ "xattr", "-dr", "com.apple.quarantine", (char*)runtime, NULL });

    char ents[PATH_MAX];
    snprintf(ents, sizeof(ents), "%s/.entitlements.plist", runtime);
    write_debugger_entitlements(ents);
    spawn((char*[]){ "codesign", "-f", "-s", "-", "--entitlements", ents, cli, NULL });
    unlink(ents);

    unlink(PATH_LINK);
    if (symlink(cli, PATH_LINK) != 0) {
        fprintf(stderr, "millennium: could not link %s; invoke %s by path\n", PATH_LINK, cli);
    }

    char agents[PATH_MAX], plist[PATH_MAX], uid[16];
    snprintf(agents, sizeof(agents), "%s/Library/LaunchAgents", getenv("HOME"));
    mkpath_np(agents, 0755);
    agent_plist_path(plist, sizeof(plist));
    snprintf(uid, sizeof(uid), "gui/%u", (unsigned)getuid());
    write_agent_plist(plist, AGENT_LABEL, cli);
    spawn((char*[]){ "launchctl", "bootout", uid, plist, NULL });
    spawn((char*[]){ "launchctl", "bootstrap", uid, plist, NULL });

    printf("millennium: installed — open Steam and it loads automatically\n");
    return 0;
}

static int cmd_uninstall(void)
{
    char plist[PATH_MAX], uid[16];
    agent_plist_path(plist, sizeof(plist));
    snprintf(uid, sizeof(uid), "gui/%u", (unsigned)getuid());
    spawn((char*[]){ "launchctl", "bootout", uid, plist, NULL });
    unlink(plist);
    unlink(PATH_LINK);

    const char* runtime = runtime_dir();
    DIR* d = opendir(runtime);
    if (d) {
        struct dirent* e;
        while ((e = readdir(d))) {
            if (strncmp(e->d_name, "release-", 8) == 0 || strncmp(e->d_name, ".stage.", 7) == 0 ||
                strncmp(e->d_name, ".current.", 9) == 0 || strcmp(e->d_name, "current") == 0) {
                char p[PATH_MAX];
                snprintf(p, sizeof(p), "%s/%s", runtime, e->d_name);
                remove_tree(p);
            }
        }
        closedir(d);
    }
    rmdir(runtime);  // only if empty; plugins and config live elsewhere
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
            "  watch                load on every Steam launch (sleeps until Steam starts)\n"
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
