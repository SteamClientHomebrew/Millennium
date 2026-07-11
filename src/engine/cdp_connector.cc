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

#include <unistd.h>
#include <fcntl.h>
#include <thread>
#include <chrono>
#include <atomic>
#include <mutex>
#include <memory>
#include <curl/curl.h>
#include <curl/websockets.h>
#include <nlohmann/json.hpp>
#include "millennium/millennium_lifecycle.h"
#include "millennium/http.h"

/*
 * macOS uses a TCP remote-debugging PORT rather than the fd pipe transport that
 * Windows/Linux use (--remote-debugging-pipe). The steam_osx wrapper enables
 * Steam's CEF remote debugging and passes -devtools-port, so CEF listens on
 * 127.0.0.1:<MILLENNIUM_DEBUG_PORT> as a standard Chromium devtools endpoint. We
 * fetch the current browser target from /json/version each connect attempt (the
 * port is fixed but the browser GUID changes across steamwebhelper restarts, so a
 * fresh fetch keeps reconnects correct) and connect to its webSocketDebuggerUrl
 * over a real WebSocket (libcurl), where each text frame carries one CDP message.
 */
namespace
{
constexpr const char* kDebugPortEnv = "MILLENNIUM_DEBUG_PORT";

/* Force the ws URL's authority to 127.0.0.1:<port>: CEF may report the endpoint
   under a different host/port than the externally reachable devtools port. */
std::string force_ws_authority(const std::string& ws_url, const std::string& port)
{
    const std::string scheme = "ws://";
    if (ws_url.rfind(scheme, 0) != 0) {
        return ws_url;
    }
    const size_t path_start = ws_url.find('/', scheme.size());
    const std::string path = (path_start == std::string::npos) ? "" : ws_url.substr(path_start);
    return scheme + "127.0.0.1:" + port + path;
}

/* Resolve the CEF browser websocket URL by fetching /json/version fresh. CEF comes
   up after the engine, so retry with backoff until it answers or we are terminating.
   Fetching fresh (vs a cached file) keeps reconnects correct after a helper restart. */
std::string resolve_browser_ws_url(const std::string& name)
{
    const char* port_env = getenv(kDebugPortEnv);
    if (!port_env || port_env[0] == '\0') {
        LOG_ERROR("[{}] {} is not set; cannot resolve CEF websocket endpoint.", name, kDebugPortEnv);
        return {};
    }
    const std::string port(port_env);
    const std::string version_url = "http://127.0.0.1:" + port + "/json/version";

    for (int attempt = 0; attempt < 240; ++attempt) {
        if (millennium_lifecycle::get().terminate.flag.load()) {
            return {};
        }

        try {
            /* single-shot, fail-fast (no internal retry, short timeout) so a dead
               port doesn't block; we own the backoff/terminate loop here. */
            const std::string body = Http::Get(version_url.c_str(), false, 2L);
            if (!body.empty()) {
                const nlohmann::json parsed = nlohmann::json::parse(body, nullptr, false);
                if (parsed.is_object() && parsed.contains("webSocketDebuggerUrl")) {
                    const std::string ws_url = parsed["webSocketDebuggerUrl"].get<std::string>();
                    if (!ws_url.empty()) {
                        return force_ws_authority(ws_url, port);
                    }
                }
            }
        } catch (const std::exception&) {
            /* CEF not up yet / connection refused — fall through to backoff. */
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(250));
    }

    return {};
}

/* A libcurl easy handle is not safe for concurrent use, but the CDP client sends
   from its own threads while our recv loop reads on this thread. Serialize every
   curl_ws_send/curl_ws_recv on the handle with one mutex; the socket is made
   non-blocking so recv returns CURLE_AGAIN promptly and never starves sends. */
struct ws_connection
{
    CURL* curl = nullptr;
    std::mutex mtx;
};

/* Send one full CDP message as a single WebSocket text frame (no '\0' terminator;
   that delimiter is specific to the CEF pipe wire format and would corrupt frames). */
bool ws_send_text(const std::shared_ptr<ws_connection>& conn, const std::string& payload)
{
    const char* data = payload.data();
    size_t remaining = payload.size();

    while (remaining > 0) {
        size_t sent = 0;
        CURLcode res;
        {
            std::lock_guard<std::mutex> lock(conn->mtx);
            res = curl_ws_send(conn->curl, data, remaining, &sent, 0, CURLWS_TEXT);
        }
        if (res == CURLE_AGAIN) {
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
            continue;
        }
        if (res != CURLE_OK) {
            LOG_ERROR("CDP websocket send failed: {}", curl_easy_strerror(res));
            return false;
        }
        data += sent;
        remaining -= sent;
    }
    return true;
}
} // namespace

void socket_utils::connect_socket(std::shared_ptr<socket_utils::socket_t> socket_props)
{
    std::string name = socket_props->name;
    auto on_connect_cb = socket_props->on_connect;

    if (!on_connect_cb) {
        LOG_ERROR("[{}] Invalid event handlers. Connection aborted.", name);
        return;
    }

    const std::string ws_url = resolve_browser_ws_url(name);
    if (ws_url.empty()) {
        if (!millennium_lifecycle::get().terminate.flag.load()) {
            LOG_ERROR("[{}] Timed out resolving the CEF websocket endpoint.", name);
        }
        return;
    }

    CURL* curl = curl_easy_init();
    if (!curl) {
        LOG_ERROR("[{}] Failed to initialize curl handle.", name);
        return;
    }

    curl_easy_setopt(curl, CURLOPT_URL, ws_url.c_str());
    curl_easy_setopt(curl, CURLOPT_CONNECT_ONLY, 2L);   /* 2 = establish a WebSocket connection */
    curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT, 5L); /* fail fast if the port is dead (e.g. helper restarted) */
    curl_easy_setopt(curl, CURLOPT_NOSIGNAL, 1L);

    const CURLcode connect_result = curl_easy_perform(curl);
    if (connect_result != CURLE_OK) {
        LOG_ERROR("[{}] CDP websocket handshake failed ({}): {}", name, ws_url, curl_easy_strerror(connect_result));
        curl_easy_cleanup(curl);
        return;
    }

    /* Make the underlying socket non-blocking so curl_ws_recv returns CURLE_AGAIN
       instead of blocking while holding the shared handle mutex (which would starve sends). */
    curl_socket_t sockfd = CURL_SOCKET_BAD;
    if (curl_easy_getinfo(curl, CURLINFO_ACTIVESOCKET, &sockfd) == CURLE_OK && sockfd != CURL_SOCKET_BAD) {
        const int fl = fcntl(sockfd, F_GETFL, 0);
        if (fl != -1) {
            fcntl(sockfd, F_SETFL, fl | O_NONBLOCK);
        }
    }

    auto conn = std::make_shared<ws_connection>();
    conn->curl = curl;

    auto cdp = std::make_shared<cdp_client>([conn](const std::string& payload) -> bool
    {
        return ws_send_text(conn, payload);
    });

    logger.log("[{}] CDP websocket transport connected.", name);

    try {
        on_connect_cb(cdp);
    } catch (const std::exception& e) {
        LOG_ERROR("[{}] Exception in onConnect: {}", name, e.what());
    }

    auto connection_closed = std::make_shared<std::atomic<bool>>(false);

    std::thread terminate_watcher([cdp, connection_closed]()
    {
        std::unique_lock<std::mutex> lk(millennium_lifecycle::get().terminate.mtx);
        millennium_lifecycle::get().terminate.cv.wait(lk, [&]
        {
            return millennium_lifecycle::get().terminate.flag.load() || connection_closed->load();
        });

        if (millennium_lifecycle::get().terminate.flag.load()) {
            cdp->shutdown();
        }
    });

    std::string message;
    char chunk[8192];

    while (cdp->is_active()) {
        size_t received = 0;
        const struct curl_ws_frame* meta = nullptr;
        CURLcode res;
        {
            std::lock_guard<std::mutex> lock(conn->mtx);
            res = curl_ws_recv(conn->curl, chunk, sizeof(chunk), &received, &meta);
        }

        if (res == CURLE_AGAIN) {
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
            continue;
        }
        if (res != CURLE_OK) {
            logger.log("[{}] CDP websocket closed: {}", name, curl_easy_strerror(res));
            break;
        }
        if (!meta) {
            continue;
        }
        if (meta->flags & CURLWS_CLOSE) {
            logger.log("[{}] CDP websocket closed by remote end.", name);
            break;
        }
        /* Control frames (ping/pong) carry no CDP payload; libcurl auto-replies to pings. */
        if (meta->flags & (CURLWS_PING | CURLWS_PONG)) {
            continue;
        }

        if (received > 0) {
            message.append(chunk, received);
        }

        /* bytesleft == 0 means this fragment is fully received; CURLWS_CONT means more
           fragments follow, so only dispatch once the final fragment has arrived. */
        if (meta->bytesleft == 0 && !(meta->flags & CURLWS_CONT)) {
            if (!message.empty()) {
                cdp->handle_message(message);
                message.clear();
            }
        }
    }

    cdp->shutdown();

    connection_closed->store(true);
    millennium_lifecycle::get().terminate.cv.notify_all();

    if (terminate_watcher.joinable()) {
        terminate_watcher.join();
    }

    curl_easy_cleanup(curl);
    logger.log("Disconnected from [{}] module...", name);
}

#else
void socket_utils::connect_socket(std::shared_ptr<socket_utils::socket_t> socket_props)
{
    LOG_ERROR("[{}] CDP pipe transport is not available on this platform.", socket_props->name);
}
#endif
