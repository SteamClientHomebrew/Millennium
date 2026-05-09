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

#include "mep_server.h"

#ifdef _WIN32
#include <winsock2.h>
#include <afunix.h>
#pragma comment(lib, "ws2_32.lib")
#else
#if defined(__APPLE__)
#include <sys/endian.h>
#else
#include <endian.h>
#endif
#include <fcntl.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>
#endif

#include <atomic>
#include <cerrno>
#include <cstring>
#include <memory>
#include <mutex>
#include <string>
#include <system_error>
#include <thread>
#include <unordered_map>

#ifdef _WIN32

static inline uint32_t le32toh_compat(uint32_t x)
{
    return x; /* x86 is LE */
}
static inline uint32_t htole32_compat(uint32_t x)
{
    return x;
}

#else

static inline uint32_t le32toh_compat(uint32_t x)
{
    return le32toh(x);
}
static inline uint32_t htole32_compat(uint32_t x)
{
    return htole32(x);
}

#endif

namespace mep
{

namespace
{

using socket_t = server::socket_t;

bool recv_all(socket_t fd, void* buf, std::size_t n)
{
    std::size_t total = 0;
    while (total < n) {
#ifdef _WIN32
        int r = ::recv(fd, static_cast<char*>(buf) + total, static_cast<int>(n - total), 0);
#else
        ssize_t r = ::recv(fd, static_cast<char*>(buf) + total, n - total, 0);
#endif
        if (r <= 0) return false;
        total += static_cast<std::size_t>(r);
    }
    return true;
}

bool send_all(socket_t fd, const void* buf, std::size_t n)
{
    std::size_t total = 0;
    while (total < n) {
#ifdef _WIN32
        int s = ::send(fd, static_cast<const char*>(buf) + total, static_cast<int>(n - total), 0);
#else
        ssize_t s = ::send(fd, static_cast<const char*>(buf) + total, n - total, MSG_NOSIGNAL);
#endif
        if (s <= 0) return false;
        total += static_cast<std::size_t>(s);
    }
    return true;
}

void set_noinherit([[maybe_unused]] socket_t fd)
{
#ifdef _WIN32
#else
    ::fcntl(fd, F_SETFD, FD_CLOEXEC);
#endif
}

void close_socket(socket_t fd)
{
#ifdef _WIN32
    ::closesocket(fd);
#else
    ::close(fd);
#endif
}

struct client_subscriptions
{
    std::mutex mutex;
    std::unordered_map<std::string, std::function<void()>> cancel_map;
    std::atomic<int> id_counter{ 0 };

    std::string subscribe(std::function<void()> cancel_fn, socket_t client_fd)
    {
        std::string id = "sub-" + std::to_string(client_fd) + "-" + std::to_string(++id_counter);
        std::lock_guard<std::mutex> lock(mutex);
        cancel_map[id] = std::move(cancel_fn);
        return id;
    }

    bool unsubscribe(const std::string& id)
    {
        std::lock_guard<std::mutex> lock(mutex);
        auto it = cancel_map.find(id);
        if (it == cancel_map.end()) return false;
        it->second();
        cancel_map.erase(it);
        return true;
    }

    void cancel_all()
    {
        std::lock_guard<std::mutex> lock(mutex);
        for (auto& [id, fn] : cancel_map)
            fn();
        cancel_map.clear();
    }
};

} // namespace

server::server(router& r, std::string socket_path) : m_router(r), m_socket_path(std::move(socket_path))
{
}

server::~server()
{
    stop();

    if (m_accept_thread.joinable()) m_accept_thread.detach();
}

void server::start()
{
    if (m_running.exchange(true)) return;

#ifdef _WIN32
    WSADATA wsa;
    if (WSAStartup(MAKEWORD(2, 2), &wsa) != 0) {
        m_running = false;
        throw std::system_error(WSAGetLastError(), std::system_category(), "mep: WSAStartup()");
    }

    ::DeleteFileA(m_socket_path.c_str());

    m_server_fd = static_cast<socket_t>(::WSASocketW(AF_UNIX, SOCK_STREAM, 0, nullptr, 0, WSA_FLAG_NO_HANDLE_INHERIT));
    if (m_server_fd == INVALID_SOCKET) {
        m_running = false;
        throw std::system_error(WSAGetLastError(), std::system_category(), "mep: WSASocket()");
    }
#else
    ::unlink(m_socket_path.c_str());

    m_server_fd = ::socket(AF_UNIX, SOCK_STREAM, 0);
    if (m_server_fd < 0) {
        m_running = false;
        throw std::system_error(errno, std::generic_category(), "mep: socket()");
    }

    set_noinherit(m_server_fd);
#endif

    sockaddr_un addr{};
    addr.sun_family = AF_UNIX;
    ::strncpy(addr.sun_path, m_socket_path.c_str(), sizeof(addr.sun_path) - 1);

#ifdef _WIN32
    if (::bind(m_server_fd, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) == SOCKET_ERROR) {
        close_socket(m_server_fd);
        m_server_fd = -1;
        m_running = false;
        throw std::system_error(WSAGetLastError(), std::system_category(), "mep: bind()");
    }

    if (::listen(m_server_fd, 16) == SOCKET_ERROR) {
        close_socket(m_server_fd);
        m_server_fd = -1;
        m_running = false;
        throw std::system_error(WSAGetLastError(), std::system_category(), "mep: listen()");
    }
#else
    if (::bind(m_server_fd, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) < 0) {
        close_socket(m_server_fd);
        m_server_fd = -1;
        m_running = false;
        throw std::system_error(errno, std::generic_category(), "mep: bind()");
    }

    if (::listen(m_server_fd, 16) < 0) {
        close_socket(m_server_fd);
        m_server_fd = -1;
        m_running = false;
        throw std::system_error(errno, std::generic_category(), "mep: listen()");
    }
#endif

    m_accept_thread = std::thread(&server::accept_loop, this);
}

void server::stop()
{
    if (!m_running.exchange(false)) return;

    if (m_server_fd >= 0) {
#ifdef _WIN32
        ::shutdown(m_server_fd, SD_BOTH);
        close_socket(m_server_fd);
        ::DeleteFileA(m_socket_path.c_str());
#else
        ::shutdown(m_server_fd, SHUT_RDWR);
        close_socket(m_server_fd);
        ::unlink(m_socket_path.c_str());
#endif
        m_server_fd = -1;
    }

    if (m_accept_thread.joinable()) m_accept_thread.join();

#ifdef _WIN32
    WSACleanup();
#endif
}

bool server::is_running() const noexcept
{
    return m_running.load();
}

void server::accept_loop()
{
    while (m_running) {
        socket_t client_fd = static_cast<socket_t>(::accept(m_server_fd, nullptr, nullptr));
        if (client_fd < 0) {
            if (!m_running) break;
            continue;
        }

        set_noinherit(client_fd);

        std::thread([this, client_fd]()
        {
            handle_client(client_fd);
            close_socket(client_fd);
        }).detach();
    }
}

void server::handle_client(socket_t fd)
{
    auto subs = std::make_shared<client_subscriptions>();
    auto ctx = std::make_shared<client_context>();

    ctx->push = [this, fd](const nlohmann::json& event) -> bool
    {
        return write_frame(fd, nlohmann::json::to_msgpack(event));
    };
    ctx->subscribe = [subs, fd](std::function<void()> cancel_fn) -> std::string
    {
        return subs->subscribe(std::move(cancel_fn), fd);
    };
    ctx->unsubscribe = [subs](const std::string& id) -> bool
    {
        return subs->unsubscribe(id);
    };

    std::vector<uint8_t> payload;
    while (m_running) {
        if (!read_frame(fd, payload)) break;

        nlohmann::json j;
        try {
            j = nlohmann::json::from_msgpack(payload);
        } catch (...) {
            write_frame(fd, nlohmann::json::to_msgpack(response_t::err("", "invalid msgpack").to_json()));
            break;
        }

        auto maybe_req = request_t::from_json(j);
        if (!maybe_req) {
            write_frame(fd, nlohmann::json::to_msgpack(response_t::err("", "malformed request").to_json()));
            break;
        }

        const response_t resp = m_router.dispatch(*maybe_req, ctx);
        if (!write_frame(fd, nlohmann::json::to_msgpack(resp.to_json()))) break;
    }

    subs->cancel_all();
}

bool server::read_frame(socket_t fd, std::vector<uint8_t>& out) const
{
    uint32_t len_le = 0;
    if (!recv_all(fd, &len_le, sizeof(len_le))) return false;

    const uint32_t len = le32toh_compat(len_le);
    if (len == 0 || len > MAX_MESSAGE_SIZE) return false;

    out.resize(len);
    return recv_all(fd, out.data(), len);
}

bool server::write_frame(socket_t fd, const std::vector<uint8_t>& payload) const
{
    const uint32_t len_le = htole32_compat(static_cast<uint32_t>(payload.size()));
    return send_all(fd, &len_le, sizeof(len_le)) && send_all(fd, payload.data(), payload.size());
}

} // namespace mep
