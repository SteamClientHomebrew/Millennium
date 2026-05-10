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

#pragma once

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <winsock2.h>
#endif

#include <nlohmann/json.hpp>

#include <cstdint>
#include <cstring>
#include <vector>

#ifndef _WIN32
#include <sys/socket.h>
#include <unistd.h>
#endif

/**
 * millennium (parent) <-> child plugin IPC protocol.
 *
 * format: [4-byte LE length][msgpack payload]
 * msgpack (via nlohmann) instead of JSON text — same schema, ~2x less
 * wire overhead and no parse/stringify cost for the hot path.
 */
namespace plugin_ipc
{
constexpr const char* TYPE_REQUEST = "req";
constexpr const char* TYPE_RESPONSE = "res";
constexpr const char* TYPE_NOTIFY = "notify";

namespace child_method
{
constexpr const char* READY = "ready";
constexpr const char* CALL_FRONTEND_METHOD = "call_frontend_method";
constexpr const char* ADD_BROWSER_CSS = "add_browser_css";
constexpr const char* ADD_BROWSER_JS = "add_browser_js";
constexpr const char* REMOVE_BROWSER_MODULE = "remove_browser_module";
constexpr const char* LOG = "log";
constexpr const char* VERSION = "version";
constexpr const char* STEAM_PATH = "steam_path";
constexpr const char* INSTALL_PATH = "install_path";
constexpr const char* IS_PLUGIN_ENABLED = "is_plugin_enabled";
constexpr const char* CMP_VERSION = "cmp_version";
constexpr const char* PATCHES = "patches";
constexpr const char* HTTP_REQUEST = "http_request";
constexpr const char* HTTP_DOWNLOAD = "http_download";
constexpr const char* CONFIG_GET = "config_get";
constexpr const char* CONFIG_SET = "config_set";
constexpr const char* CONFIG_DELETE = "config_delete";
constexpr const char* CONFIG_GET_ALL = "config_get_all";
} // namespace child_method

namespace parent_method
{
constexpr const char* INIT = "init";
constexpr const char* EVALUATE = "evaluate";
constexpr const char* ON_FRONTEND_LOADED = "on_frontend_loaded";
constexpr const char* GET_METRICS = "get_metrics";
constexpr const char* SHUTDOWN = "shutdown";
constexpr const char* CONFIG_CHANGED = "config_changed";
} // namespace parent_method

#ifdef _WIN32
using socket_fd = SOCKET;
constexpr socket_fd INVALID_FD = INVALID_SOCKET;
#else
using socket_fd = int;
constexpr socket_fd INVALID_FD = -1;
#endif

static constexpr uint32_t MAX_FRAME_SIZE = 16u * 1024u * 1024u; /* 16 MB */

inline bool recv_all(socket_fd fd, void* buf, size_t n)
{
    size_t total = 0;
    while (total < n) {
#ifdef _WIN32
        int r = ::recv(fd, static_cast<char*>(buf) + total, static_cast<int>(n - total), 0);
#else
        ssize_t r = ::recv(fd, static_cast<char*>(buf) + total, n - total, 0);
#endif
        if (r <= 0) return false;
        total += static_cast<size_t>(r);
    }
    return true;
}

inline bool send_all(socket_fd fd, const void* buf, size_t n)
{
    size_t total = 0;
    while (total < n) {
#ifdef _WIN32
        int s = ::send(fd, static_cast<const char*>(buf) + total, static_cast<int>(n - total), 0);
#else
        ssize_t s = ::send(fd, static_cast<const char*>(buf) + total, n - total, MSG_NOSIGNAL);
#endif
        if (s <= 0) return false;
        total += static_cast<size_t>(s);
    }
    return true;
}

/* x86/x64 is always LE so these are no-ops, but kept for clarity */
inline uint32_t to_le32(uint32_t x)
{
    return x;
}
inline uint32_t from_le32(uint32_t x)
{
    return x;
}

inline bool read_frame(socket_fd fd, std::vector<uint8_t>& out)
{
    uint32_t len_le = 0;
    if (!recv_all(fd, &len_le, sizeof(len_le))) return false;

    uint32_t len = from_le32(len_le);
    if (len == 0 || len > MAX_FRAME_SIZE) return false;

    out.resize(len);
    return recv_all(fd, out.data(), len);
}

inline bool write_frame(socket_fd fd, const std::vector<uint8_t>& payload)
{
    uint32_t len_le = to_le32(static_cast<uint32_t>(payload.size()));
    return send_all(fd, &len_le, sizeof(len_le)) && send_all(fd, payload.data(), payload.size());
}

/* convenience wrappers so callers don't have to think about serialization */
inline bool read_msg(socket_fd fd, nlohmann::json& out)
{
    std::vector<uint8_t> buf;
    if (!read_frame(fd, buf)) return false;
    out = nlohmann::json::from_msgpack(buf, true, false);
    return !out.is_discarded();
}

inline bool write_msg(socket_fd fd, const nlohmann::json& msg)
{
    return write_frame(fd, nlohmann::json::to_msgpack(msg));
}

inline void close_fd(socket_fd fd)
{
#ifdef _WIN32
    ::closesocket(fd);
#else
    ::close(fd);
#endif
}

} // namespace plugin_ipc
