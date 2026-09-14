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

#include <thread>
#include "millennium/macos_cdp_pipe.h"
#include "millennium/millennium_lifecycle.h"

namespace
{
macos_cdp::broker pipe_broker;

bool terminating()
{
    return millennium_lifecycle::get().terminate.flag.load();
}
} // namespace

// Called before StartMillennium by both macOS bootstrap implementations.
extern "C" __attribute__((visibility("default"))) int MillenniumAcceptPipeBroker(int fd)
{
    if (pipe_broker.fd >= 0) return -1;
    pipe_broker.fd = fcntl(fd, F_DUPFD_CLOEXEC, 10);
    return pipe_broker.fd < 0 ? -1 : 0;
}

void socket_utils::connect_socket(std::shared_ptr<socket_utils::socket_t> props)
{
    if (pipe_broker.fd < 0 || !props->on_connect) {
        LOG_ERROR("[{}] macOS CDP pipe broker is not configured.", props->name);
        return;
    }

    while (!terminating()) {
        std::shared_ptr<macos_cdp::connection> conn;
        while (!terminating() && !conn) {
            pollfd wait{ pipe_broker.fd, POLLIN, 0 };
            const int result = poll(&wait, 1, 100);
            if (result < 0 && errno == EINTR) continue;
            if (result < 0 || (wait.revents & (POLLERR | POLLHUP | POLLNVAL))) return;
            if (wait.revents & POLLIN) conn = macos_cdp::receive(pipe_broker.fd);
        }
        if (!conn || terminating()) return;

        auto cdp = std::make_shared<cdp_client>([conn](const std::string& payload)
        {
            return conn->send(payload, terminating);
        });
        logger.log("[{}] CDP pipe transport connected.", props->name);

        // Read before on_connect: initialization may synchronously wait for CDP.
        std::thread reader([conn, cdp]()
        {
            std::string buffer;
            char chunk[8192];
            while (!terminating() && !conn->stopped.load() && cdp->is_active()) {
                // A queued handoff does not prove exec succeeded. Keep the
                // current Helper connected until its own pipe closes.
                pollfd wait{ conn->read_fd, POLLIN, 0 };
                const int result = poll(&wait, 1, 100);
                if (result < 0 && errno == EINTR) continue;
                if (result < 0) break;
                if (!wait.revents) continue;
                const auto count = read(conn->read_fd, chunk, sizeof(chunk));
                if (count < 0 && (errno == EINTR || errno == EAGAIN)) continue;
                if (count <= 0) break;
                buffer.append(chunk, static_cast<size_t>(count));
                size_t offset = 0;
                size_t end;
                while ((end = buffer.find('\0', offset)) != std::string::npos) {
                    if (end > offset) cdp->handle_message(buffer.substr(offset, end - offset));
                    offset = end + 1;
                }
                buffer.erase(0, offset);
            }
            conn->stopped.store(true);
            cdp->shutdown();
        });

        try {
            props->on_connect(cdp);
        } catch (const std::exception& e) {
            LOG_ERROR("[{}] Exception in onConnect: {}", props->name, e.what());
            conn->stopped.store(true);
        } catch (...) {
            LOG_ERROR("[{}] Unknown exception in onConnect.", props->name);
            conn->stopped.store(true);
        }
        reader.join();
        logger.log("[{}] CDP pipe disconnected; waiting for a new Helper.", props->name);
    }
}

#else
void socket_utils::connect_socket(std::shared_ptr<socket_utils::socket_t> socket_props)
{
    LOG_ERROR("[{}] CDP pipe transport is not available on this platform.", socket_props->name);
}
#endif
