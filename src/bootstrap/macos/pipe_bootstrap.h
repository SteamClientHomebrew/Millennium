#pragma once

#include <cerrno>
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
        if (count >= 4094) return next(path, argv, envp);
        args[count] = argv[count];
    }
    args[count++] = const_cast<char*>("--remote-debugging-pipe");
    args[count] = nullptr;

    int fds[4] = { -1, -1, -1, -1 };
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
    if (ok) ok = pipe(fds) == 0;
    if (ok) {
        fds[0] = private_fd(fds[0]);
        fds[1] = private_fd(fds[1]);
        ok = fds[0] >= 0 && fds[1] >= 0 && pipe(fds + 2) == 0;
    }
    if (ok) {
        fds[2] = private_fd(fds[2]);
        fds[3] = private_fd(fds[3]);
        ok = fds[2] >= 0 && fds[3] >= 0;
    }

    bool mapped = false;
    if (ok) {
        mapped = true;
        ok = dup2(fds[0], 3) >= 0 && dup2(fds[3], 4) >= 0;
    }
    if (ok) {
        char byte = 1;
        iovec iov{ &byte, 1 };
        alignas(cmsghdr) char control[CMSG_SPACE(2 * sizeof(int))] = {};
        msghdr msg{};
        msg.msg_iov = &iov;
        msg.msg_iovlen = 1;
        msg.msg_control = control;
        msg.msg_controllen = sizeof(control);
        auto* rights = CMSG_FIRSTHDR(&msg);
        rights->cmsg_level = SOL_SOCKET;
        rights->cmsg_type = SCM_RIGHTS;
        rights->cmsg_len = CMSG_LEN(2 * sizeof(int));
        const int endpoints[2] = { fds[2], fds[1] };
        memcpy(CMSG_DATA(rights), endpoints, sizeof(endpoints));
        int result;
        do {
            result = sendmsg(sender, &msg, 0);
        } while (result < 0 && errno == EINTR);
        ok = result == 1;
    }
    for (int fd : fds)
        if (fd >= 0) close(fd);
    int result = -1;
    if (ok) result = next(path, args, envp);
    const int error = errno;
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
