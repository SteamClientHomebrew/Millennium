#pragma once

#include <cerrno>
#include <charconv>
#include <cstring>
#include <dlfcn.h>
#include <fcntl.h>
#include <sys/socket.h>
#include <unistd.h>

// Shared by the injected bootstrap and the legacy tier0 proxy. Only the fork
// child maps CEF's fixed descriptors; Steam's own descriptors remain untouched.
namespace millennium_pipe_bootstrap
{
static int sender = -1;
static pid_t owner = 0;

static int private_fd(int fd)
{
    if (fd < 0) return -1;
    const int result = fcntl(fd, F_DUPFD_CLOEXEC, 10);
    const int error = errno;
    close(fd);
    errno = error;
    return result;
}

static bool initialize(void* runtime)
{
    auto accept = reinterpret_cast<int (*)(int)>(dlsym(runtime, "MillenniumAcceptPipeBroker"));
    if (!accept) return false;
    int pair[2];
    if (socketpair(AF_UNIX, SOCK_DGRAM, 0, pair) != 0) return false;
    pair[0] = private_fd(pair[0]);
    pair[1] = private_fd(pair[1]);
    bool ok = pair[0] >= 0 && pair[1] >= 0;
    if (ok) ok = fcntl(pair[1], F_SETFL, O_NONBLOCK) == 0 && accept(pair[0]) == 0;
    if (pair[0] >= 0) close(pair[0]);
    if (!ok) {
        if (pair[1] >= 0) close(pair[1]);
        return false;
    }
    sender = pair[1];
    owner = getpid();
    return true;
}

static void shutdown()
{
    if (sender >= 0) close(sender);
    sender = -1;
}

static bool is_browser(const char* path, char* const argv[])
{
    if (!path || !argv) return false;
    const char* name = strrchr(path, '/');
    if (strcmp(name ? name + 1 : path, "Steam Helper") != 0) return false;
    for (size_t i = 0; argv[i]; ++i) {
        if (strncmp(argv[i], "--type=", 7) == 0 || strcmp(argv[i], "--type") == 0) return false;
    }
    return true;
}

using exec_fn = int (*)(const char*, char* const[], char* const[]);

static int exec(const char* path, char* const argv[], char* const envp[], exec_fn next)
{
    if (sender < 0 || getpid() == owner || !is_browser(path, argv)) return next(path, argv, envp);

    // Keep this FD handoff allocation-free. Leave oversized argv untouched.
    char* args[4096];
    size_t count = 0;
    for (; argv[count]; ++count) {
        if (count >= 4093) return next(path, argv, envp);
        args[count] = argv[count];
    }
    args[count++] = const_cast<char*>("--remote-debugging-pipe");
    char loopback_arg[64] = "--millennium-loopback-ipc-fds=";
    args[count++] = loopback_arg;
    args[count] = nullptr;

    int to_cdp[2] = { -1, -1 }, from_cdp[2] = { -1, -1 };
    int to_patcher[2] = { -1, -1 }, from_patcher[2] = { -1, -1 };
    int* pipes[] = { to_cdp, from_cdp, to_patcher, from_patcher };
    int saved[2] = { -1, -1 };
    int flags[2] = { -1, -1 };
    bool ok = true;
    for (int i = 0; i < 2; ++i) {
        flags[i] = fcntl(3 + i, F_GETFD);
        if (flags[i] >= 0) {
            saved[i] = fcntl(3 + i, F_DUPFD_CLOEXEC, 10);
            if (saved[i] < 0) ok = false;
        } else if (errno != EBADF) {
            ok = false;
        }
    }
    for (int* pair : pipes) {
        if (!ok || pipe(pair) != 0) {
            ok = false;
            break;
        }
        pair[0] = private_fd(pair[0]);
        pair[1] = private_fd(pair[1]);
        ok = pair[0] >= 0 && pair[1] >= 0;
    }

    const bool mapped = ok;
    if (mapped) {
        ok = dup2(to_cdp[0], 3) >= 0 && dup2(from_cdp[1], 4) >= 0 &&
             fcntl(to_patcher[0], F_SETFD, 0) == 0 && fcntl(from_patcher[1], F_SETFD, 0) == 0;
    }
    if (ok) {
        // Only CDP requires fixed descriptors. Keep Steam's other inherited fds in place.
        char* end = loopback_arg + sizeof(loopback_arg) - 1;
        char* pos = std::to_chars(loopback_arg + strlen(loopback_arg), end, to_patcher[0]).ptr;
        *pos++ = ',';
        *std::to_chars(pos, end, from_patcher[1]).ptr = '\0';
        char byte = 1;
        iovec iov{ &byte, 1 };
        alignas(cmsghdr) char control[CMSG_SPACE(4 * sizeof(int))] = {};
        msghdr msg{};
        msg.msg_iov = &iov;
        msg.msg_iovlen = 1;
        msg.msg_control = control;
        msg.msg_controllen = sizeof(control);
        auto* rights = CMSG_FIRSTHDR(&msg);
        rights->cmsg_level = SOL_SOCKET;
        rights->cmsg_type = SCM_RIGHTS;
        rights->cmsg_len = CMSG_LEN(4 * sizeof(int));
        // CDP read, CDP write, loopback read, loopback write (host side).
        const int endpoints[4] = { from_cdp[0], to_cdp[1], from_patcher[0], to_patcher[1] };
        memcpy(CMSG_DATA(rights), endpoints, sizeof(endpoints));
        int result;
        do {
            result = sendmsg(sender, &msg, 0);
        } while (result < 0 && errno == EINTR);
        ok = result == 1;
    }
    int result = -1;
    if (ok) result = next(path, args, envp);
    const int error = errno;
    for (int* pair : pipes) {
        if (pair[0] >= 0) close(pair[0]);
        if (pair[1] >= 0) close(pair[1]);
    }
    if (mapped) {
        for (int i = 0; i < 2; ++i) {
            if (saved[i] >= 0) {
                dup2(saved[i], 3 + i);
                fcntl(3 + i, F_SETFD, flags[i]);
            } else {
                close(3 + i);
            }
        }
    }
    for (int fd : saved)
        if (fd >= 0) close(fd);
    errno = error;
    return ok ? result : next(path, argv, envp);
}
} // namespace millennium_pipe_bootstrap
