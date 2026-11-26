/**
 * ==================================================
 *   _____ _ _ _             _
 *  |     |_| | |___ ___ ___|_|_ _ _____
 *  | | | | | | | -_|   |   | | | |     |
 *  |_|_|_|_|_|_|___|_|_|_|_|_|___|_|_|_|
 *
 * ==================================================
 *
 * Copyright (c) 2026 Project Millennium
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

#include "millennium/cdp_connector.h"
#include "millennium/logger.h"

#ifdef _WIN32
#include <windows.h>
#include <mutex>
#include <condition_variable>
#include <thread>
#include "millennium/millennium_lifecycle.h"
#include "millennium/steam_hooks.h"

extern HANDLE g_cdp_pipe_read;
extern HANDLE g_cdp_pipe_write;
extern std::mutex g_cdp_pipe_mutex;
extern std::condition_variable g_cdp_pipe_cv;
extern std::atomic<bool> g_cdp_pipes_ready;

static bool pipe_send(HANDLE hWrite, const std::string& payload)
{
    std::string msg = payload;
    msg.push_back('\0');

    const char* data = msg.data();
    DWORD remaining = static_cast<DWORD>(msg.size());

    while (remaining > 0) {
        DWORD written = 0;
        if (!WriteFile(hWrite, data, remaining, &written, nullptr)) {
            LOG_ERROR("CDP pipe write failed (error {})", GetLastError());
            return false;
        }
        data += written;
        remaining -= written;
    }
    return true;
}

static void pipe_read_loop(HANDLE hRead, std::shared_ptr<cdp_client> cdp)
{
    std::string buffer;
    char chunk[4096];

    while (cdp->is_active()) {
        DWORD bytesRead = 0;
        if (!ReadFile(hRead, chunk, sizeof(chunk), &bytesRead, nullptr)) {
            DWORD err = GetLastError();
            if (err == ERROR_BROKEN_PIPE) {
                logger.log("CDP pipe closed by remote end.");
            } else if (err == ERROR_OPERATION_ABORTED) {
                logger.log("CDP pipe read cancelled.");
            } else {
                LOG_ERROR("CDP pipe read failed (error {})", err);
            }
            break;
        }

        if (bytesRead == 0) {
            continue;
        }

        buffer.append(chunk, bytesRead);

        /** extract null-terminated messages */
        size_t pos;
        while ((pos = buffer.find('\0')) != std::string::npos) {
            std::string message = buffer.substr(0, pos);
            buffer.erase(0, pos + 1);

            if (!message.empty()) {
                cdp->handle_message(message);
            }
        }
    }
}

void socket_utils::connect_socket(std::shared_ptr<socket_utils::socket_t> socket_props)
{
    std::string name = socket_props->name;
    auto on_connect_cb = socket_props->on_connect;

    {
        std::unique_lock<std::mutex> lock(g_cdp_pipe_mutex);
        g_cdp_pipe_cv.wait(lock, []
        {
            return g_cdp_pipes_ready.load();
        });
    }

    if (g_cdp_pipe_read == INVALID_HANDLE_VALUE || g_cdp_pipe_write == INVALID_HANDLE_VALUE) {
        LOG_ERROR("[{}] CDP pipes are not available. Cannot connect.", name);
        return;
    }

    if (!on_connect_cb) {
        LOG_ERROR("[{}] Invalid event handlers. Connection aborted.", name);
        return;
    }

    HANDLE hWrite = g_cdp_pipe_write;
    auto cdp = std::make_shared<cdp_client>([hWrite](const std::string& payload) -> bool
    {
        return pipe_send(hWrite, payload);
    });

    logger.log("[{}] CDP pipe transport connected.", name);

    /** hand off from the drain thread to the real CDP client. */
    stop_pipe_drain();

    /** start the read loop on a background thread so messages flow immediately */
    std::thread read_thread([hRead = g_cdp_pipe_read, cdp, name]()
    {
        pipe_read_loop(hRead, cdp);
        logger.log("[{}] CDP pipe read loop exited.", name);
    });

    try {
        on_connect_cb(cdp);
    } catch (const std::exception& e) {
        LOG_ERROR("[{}] Exception in onConnect: {}", name, e.what());
    }

    HANDLE read_thread_handle = read_thread.native_handle();
    std::thread terminate_watcher([cdp, read_thread_handle]()
    {
        millennium_lifecycle::get().terminate.wait();
        CancelSynchronousIo(read_thread_handle);
        cdp->shutdown();
    });

    if (read_thread.joinable()) {
        read_thread.join();
    }

    cdp->shutdown();

    if (terminate_watcher.joinable()) {
        terminate_watcher.join();
    }

    logger.log("Disconnected from [{}] module...", name);
}

#elif __linux__

#include <unistd.h>
#include <poll.h>
#include <sys/eventfd.h>
#include <mutex>
#include <condition_variable>
#include <thread>
#include "millennium/millennium_lifecycle.h"

extern int g_cdp_pipe_read_fd;
extern int g_cdp_pipe_write_fd;
extern int g_cdp_pipe_change_efd;
extern std::mutex g_cdp_pipe_mutex;
extern std::condition_variable g_cdp_pipe_cv;
extern std::atomic<bool> g_cdp_pipes_ready;

static bool pipe_send(int fd, const std::string& payload)
{
    std::string msg = payload;
    msg.push_back('\0');

    const char* data = msg.data();
    ssize_t remaining = static_cast<ssize_t>(msg.size());

    while (remaining > 0) {
        ssize_t written = write(fd, data, static_cast<size_t>(remaining));
        if (written < 0) {
            LOG_ERROR("CDP pipe write failed (errno {})", errno);
            return false;
        }
        data += written;
        remaining -= written;
    }
    return true;
}

static void pipe_read_loop(int data_fd, int cancel_fd, int pipe_change_fd, std::shared_ptr<cdp_client> cdp)
{
    std::string buffer;
    char chunk[4096];

    struct pollfd pfds[3] = {
        { data_fd,        POLLIN, 0 },
        { cancel_fd,      POLLIN, 0 },
        { pipe_change_fd, POLLIN, 0 },
    };
    const int nfds = (pipe_change_fd >= 0) ? 3 : 2;

    while (cdp->is_active()) {
        int r = poll(pfds, nfds, -1);
        if (r < 0) {
            if (errno == EINTR) continue;
            LOG_ERROR("CDP pipe poll failed (errno {})", errno);
            break;
        }

        if (pfds[1].revents & POLLIN) {
            logger.log("CDP pipe read cancelled.");
            break;
        }

        if (pipe_change_fd >= 0 && (pfds[2].revents & POLLIN)) {
            uint64_t val;
            [[maybe_unused]] ssize_t _ = read(pipe_change_fd, &val, sizeof(val));
            logger.log("CDP pipe replaced (steamwebhelper restarted), reconnecting...");
            cdp->shutdown();
            break;
        }

        if (pfds[0].revents & (POLLHUP | POLLERR | POLLNVAL)) {
            logger.log("CDP pipe closed or error.");
            break;
        }

        if (!(pfds[0].revents & POLLIN)) continue;

        ssize_t n = read(data_fd, chunk, sizeof(chunk));
        if (n < 0) {
            LOG_ERROR("CDP pipe read failed (errno {})", errno);
            break;
        }
        if (n == 0) {
            logger.log("CDP pipe closed by remote end.");
            break;
        }

        buffer.append(chunk, static_cast<size_t>(n));

        size_t pos;
        while ((pos = buffer.find('\0')) != std::string::npos) {
            std::string message = buffer.substr(0, pos);
            buffer.erase(0, pos + 1);
            if (!message.empty()) {
                cdp->handle_message(message);
            }
        }
    }
}

void socket_utils::connect_socket(std::shared_ptr<socket_utils::socket_t> socket_props)
{
    std::string name = socket_props->name;
    auto on_connect_cb = socket_props->on_connect;

    {
        std::unique_lock<std::mutex> lock(g_cdp_pipe_mutex);
        g_cdp_pipe_cv.wait(lock, []
        {
            return g_cdp_pipes_ready.load();
        });
    }

    if (g_cdp_pipe_read_fd < 0 || g_cdp_pipe_write_fd < 0) {
        LOG_ERROR("[{}] CDP pipes are not available. Cannot connect.", name);
        return;
    }

    if (!on_connect_cb) {
        LOG_ERROR("[{}] Invalid event handlers. Connection aborted.", name);
        return;
    }

    int cancel_fd = eventfd(0, EFD_CLOEXEC);
    if (cancel_fd < 0) {
        LOG_ERROR("[{}] Failed to create cancel eventfd (errno {})", name, errno);
        return;
    }

    int read_fd = g_cdp_pipe_read_fd;
    int write_fd = g_cdp_pipe_write_fd;

    if (g_cdp_pipe_change_efd >= 0) {
        uint64_t val;
        [[maybe_unused]] ssize_t _ = read(g_cdp_pipe_change_efd, &val, sizeof(val));
    }

    auto cdp = std::make_shared<cdp_client>([write_fd](const std::string& payload) -> bool
    {
        return pipe_send(write_fd, payload);
    });

    logger.log("[{}] CDP pipe transport connected.", name);

    std::thread read_thread([read_fd, cancel_fd, cdp, name]()
    {
        pipe_read_loop(read_fd, cancel_fd, g_cdp_pipe_change_efd, cdp);
        logger.log("[{}] CDP pipe read loop exited.", name);
    });

    try {
        on_connect_cb(cdp);
    } catch (const std::exception& e) {
        LOG_ERROR("[{}] Exception in onConnect: {}", name, e.what());
    }

    auto connection_closed = std::make_shared<std::atomic<bool>>(false);

    std::thread terminate_watcher([cdp, cancel_fd, connection_closed]()
    {
        std::unique_lock<std::mutex> lk(millennium_lifecycle::get().terminate.mtx);
        millennium_lifecycle::get().terminate.cv.wait(lk, [&]
        {
            return millennium_lifecycle::get().terminate.flag.load() || connection_closed->load();
        });

        if (millennium_lifecycle::get().terminate.flag.load()) {
            const uint64_t val = 1;
            [[maybe_unused]] ssize_t _ = write(cancel_fd, &val, sizeof(val));
            cdp->shutdown();
        }
    });

    if (read_thread.joinable()) {
        read_thread.join();
    }

    cdp->shutdown();

    connection_closed->store(true);
    millennium_lifecycle::get().terminate.cv.notify_all();

    if (terminate_watcher.joinable()) {
        terminate_watcher.join();
    }

    close(cancel_fd);
    close(read_fd);
    close(write_fd);
    logger.log("Disconnected from [{}] module...", name);
}
#elif __APPLE__

#include <atomic>
#include <cerrno>
#include <cstdlib>
#include <cstring>
#include <fcntl.h>
#include <memory>
#include <poll.h>
#include <string>
#include <sys/socket.h>
#include <sys/un.h>
#include <thread>
#include <unistd.h>
#include "instrumentation/loopback_ipc.h"
#include "millennium/millennium_lifecycle.h"

// hhx64 in the CEF browser connects to two unix sockets the runtime listens on:
// CDP (NUL-delimited frames) and the loopback patch stream. Every wait below also
// watches a stop pipe that becomes readable on terminate, so nothing polls on a timer.
namespace
{
int listen_unix(const char* path)
{
    sockaddr_un addr{};
    addr.sun_family = AF_UNIX;
    if (!path || !path[0] || strlen(path) >= sizeof(addr.sun_path)) return -1;
    strcpy(addr.sun_path, path);

    const int fd = socket(AF_UNIX, SOCK_STREAM, 0);
    if (fd < 0) return -1;
    fcntl(fd, F_SETFD, FD_CLOEXEC);
    unlink(path);
    if (bind(fd, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) != 0 || listen(fd, 4) != 0) {
        close(fd);
        return -1;
    }
    return fd;
}

// Block until `fd` is readable (true) or the stop pipe fires or `fd` fails (false).
bool wait_readable(int fd, int stop_fd)
{
    pollfd fds[2] = {
        { fd,      POLLIN, 0 },
        { stop_fd, POLLIN, 0 }
    };
    for (;;) {
        const int result = poll(fds, 2, -1);
        if (result < 0 && errno == EINTR) continue;
        if (result < 0 || fds[1].revents) return false;
        if (fds[0].revents & POLLNVAL) return false;
        return true;  // POLLIN, or POLLHUP/POLLERR for the following read to report
    }
}

int accept_next(int listen_fd, int stop_fd)
{
    while (wait_readable(listen_fd, stop_fd)) {
        const int fd = accept(listen_fd, nullptr, nullptr);
        if (fd < 0 && (errno == EINTR || errno == EAGAIN || errno == ECONNABORTED)) continue;
        if (fd < 0) return -1;
        fcntl(fd, F_SETFD, FD_CLOEXEC);
        fcntl(fd, F_SETNOSIGPIPE, 1);
        return fd;
    }
    return -1;
}

// One browser connection at a time, until terminate.
void serve_loopback_socket(int listen_fd, int stop_fd)
{
    for (int conn; (conn = accept_next(listen_fd, stop_fd)) >= 0;) {
        // Non-blocking, so a full socket still yields to the stop pipe on shutdown.
        fcntl(conn, F_SETFL, O_NONBLOCK);
        // serve_loopback_conn closes both fds; hand it two refs to the one socket.
        const int write_fd = fcntl(conn, F_DUPFD_CLOEXEC, 10);
        if (write_fd < 0) {
            close(conn);
            continue;
        }
        serve_loopback_conn(write_fd, conn, stop_fd);
    }
}

// Read NUL-delimited CDP frames until the browser disconnects or the stop pipe fires.
void read_cdp(int conn, int stop_fd, const std::shared_ptr<cdp_client>& cdp)
{
    std::string buffer;
    char chunk[8192];
    while (cdp->is_active() && wait_readable(conn, stop_fd)) {
        const auto count = read(conn, chunk, sizeof(chunk));
        if (count < 0 && errno == EINTR) continue;
        if (count <= 0) break;
        buffer.append(chunk, static_cast<size_t>(count));
        size_t offset = 0;
        for (size_t end; (end = buffer.find('\0', offset)) != std::string::npos; offset = end + 1) {
            if (end > offset) cdp->handle_message(buffer.substr(offset, end - offset));
        }
        buffer.erase(0, offset);
    }
}

void serve_cdp_conn(int conn, int stop_fd, const std::shared_ptr<socket_utils::socket_t>& props)
{
    auto alive = std::make_shared<std::atomic<bool>>(true);
    auto cdp = std::make_shared<cdp_client>([conn, alive](const std::string& payload) -> bool
    {
        if (!alive->load()) return false;
        std::string message = payload;
        message.push_back('\0');
        for (size_t offset = 0; offset < message.size();) {
            const auto count = write(conn, message.data() + offset, message.size() - offset);
            if (count > 0) {
                offset += static_cast<size_t>(count);
            } else if (count < 0 && errno == EINTR) {
                continue;
            } else {
                alive->store(false);
                shutdown(conn, SHUT_RDWR);  // wakes the reader
                return false;
            }
        }
        return true;
    });
    logger.log("[{}] CDP socket transport connected.", props->name);

    // Read before on_connect: initialization may synchronously wait for CDP.
    std::thread reader([conn, stop_fd, cdp, alive]
    {
        read_cdp(conn, stop_fd, cdp);
        alive->store(false);
        cdp->shutdown();
    });

    try {
        props->on_connect(cdp);
    } catch (const std::exception& e) {
        LOG_ERROR("[{}] Exception in onConnect: {}", props->name, e.what());
        alive->store(false);
    } catch (...) {
        LOG_ERROR("[{}] Unknown exception in onConnect.", props->name);
        alive->store(false);
    }
    reader.join();
    close(conn);
    logger.log("[{}] CDP socket disconnected; waiting for a new Helper.", props->name);
}
} // namespace

void socket_utils::connect_socket(std::shared_ptr<socket_utils::socket_t> props)
{
    if (!props->on_connect) {
        LOG_ERROR("[{}] No CDP connection handler.", props->name);
        return;
    }

    const char* cdp_path = std::getenv("MILLENNIUM_CDP_SOCKET");
    const char* loopback_path = std::getenv("MILLENNIUM_LOOPBACK_SOCKET");
    const int cdp_fd = listen_unix(cdp_path);
    const int loopback_fd = listen_unix(loopback_path);
    int stop[2] = { -1, -1 };
    if (cdp_fd < 0 || loopback_fd < 0 || pipe(stop) != 0) {
        LOG_ERROR("[{}] Could not listen on '{}' and '{}'.", props->name, cdp_path ? cdp_path : "", loopback_path ? loopback_path : "");
        if (cdp_fd >= 0) close(cdp_fd);
        if (loopback_fd >= 0) close(loopback_fd);
        return;
    }
    for (int fd : stop) fcntl(fd, F_SETFD, FD_CLOEXEC);

    std::thread waker([wr = stop[1]]
    {
        millennium_lifecycle::get().terminate.wait();
        while (write(wr, "", 1) < 0 && errno == EINTR) {}
    });
    std::thread loopback(serve_loopback_socket, loopback_fd, stop[0]);

    for (int conn; (conn = accept_next(cdp_fd, stop[0])) >= 0;) {
        serve_cdp_conn(conn, stop[0], props);
    }

    // A listener only fails before terminate on a broken socket; hold the fds until
    // terminate so the waker's write lands on an open pipe.
    if (!millennium_lifecycle::get().terminate.flag.load()) LOG_ERROR("[{}] The CDP listener failed.", props->name);
    waker.join();
    loopback.join();
    for (int fd : { cdp_fd, loopback_fd, stop[0], stop[1] }) close(fd);
    unlink(cdp_path);
    unlink(loopback_path);
}

#else
void socket_utils::connect_socket(std::shared_ptr<socket_utils::socket_t> socket_props)
{
    LOG_ERROR("[{}] CDP pipe transport is not available on this platform.", socket_props->name);
}
#endif
