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

#include "instrumentation/loopback_ipc.h"
#include "instrumentation/patch_registry.h"
#include "mep/patch_stream_recorder.h"
#include "mep/patch_update_notifier.h"
#include "millennium/logger.h"
#include <nlohmann/json.hpp>

#include <atomic>
#include <memory>
#include <mutex>
#include <thread>
#include <vector>

#ifdef _WIN32
#include <windows.h>
#elif defined(__APPLE__)
#include <unistd.h>
#include <libkern/OSByteOrder.h>
#define htole32(x) OSSwapHostToLittleInt32(x)
#define le32toh(x) OSSwapLittleToHostInt32(x)
#else
#include <unistd.h>
#include <endian.h>
#endif

using json = nlohmann::json;
using namespace mep;

#ifdef _WIN32
using fd_t = HANDLE;
static const fd_t INVALID_FD = INVALID_HANDLE_VALUE;

static inline uint32_t to_le32(uint32_t x)
{
    return x;
}
static inline uint32_t from_le32(uint32_t x)
{
    return x;
}

static bool write_all(fd_t fd, const void* buf, size_t n)
{
    DWORD written;
    return WriteFile(fd, buf, static_cast<DWORD>(n), &written, nullptr) && written == static_cast<DWORD>(n);
}
static bool read_all(fd_t fd, void* buf, size_t n)
{
    DWORD got;
    return ReadFile(fd, buf, static_cast<DWORD>(n), &got, nullptr) && got == static_cast<DWORD>(n);
}
static void close_fd(fd_t fd)
{
    CloseHandle(fd);
}

#else
using fd_t = int;
static const fd_t INVALID_FD = -1;

static inline uint32_t to_le32(uint32_t x)
{
    return htole32(x);
}
static inline uint32_t from_le32(uint32_t x)
{
    return le32toh(x);
}

static bool write_all(fd_t fd, const void* buf, size_t n)
{
    const char* p = static_cast<const char*>(buf);
    while (n > 0) {
        ssize_t w = ::write(fd, p, n);
        if (w <= 0) return false;
        p += w;
        n -= static_cast<size_t>(w);
    }
    return true;
}
static bool read_all(fd_t fd, void* buf, size_t n)
{
    char* p = static_cast<char*>(buf);
    while (n > 0) {
        ssize_t r = ::read(fd, p, n);
        if (r <= 0) return false;
        p += r;
        n -= static_cast<size_t>(r);
    }
    return true;
}
static void close_fd(fd_t fd)
{
    ::close(fd);
}
#endif

static bool write_frame(fd_t fd, const std::vector<uint8_t>& payload)
{
    uint32_t len_le = to_le32(static_cast<uint32_t>(payload.size()));
    return write_all(fd, &len_le, sizeof(len_le)) && write_all(fd, payload.data(), payload.size());
}

static bool read_frame(fd_t fd, std::vector<uint8_t>& out)
{
    uint32_t len_le = 0;
    if (!read_all(fd, &len_le, sizeof(len_le))) return false;
    const uint32_t len = from_le32(len_le);
    if (len == 0 || len > 8u * 1024u * 1024u) return false;
    out.resize(len);
    return read_all(fd, out.data(), len);
}

static std::vector<uint8_t> build_patch_list_payload()
{
    return json::to_msgpack(json{
        { "type",    "patch_list"             },
        { "patches", patch_registry_to_json() }
    });
}

struct LoopbackConn
{
    std::atomic<bool> alive{ true };
    std::mutex write_mu;
    fd_t write_fd = INVALID_FD;
    fd_t read_fd = INVALID_FD;
    int notifier_id = -1;
};

static bool conn_write_frame(LoopbackConn& conn, const std::vector<uint8_t>& payload)
{
    std::lock_guard<std::mutex> lock(conn.write_mu);
    if (conn.write_fd == INVALID_FD) return false;
    return write_frame(conn.write_fd, payload);
}

static void conn_close_write(LoopbackConn& conn)
{
    std::lock_guard<std::mutex> lock(conn.write_mu);
    if (conn.write_fd != INVALID_FD) {
        close_fd(conn.write_fd);
        conn.write_fd = INVALID_FD;
    }
}

static void reader_loop(std::shared_ptr<LoopbackConn> conn)
{
    std::vector<uint8_t> buf;
    while (conn->alive.load()) {
        if (!read_frame(conn->read_fd, buf)) break;

        json msg;
        try {
            msg = json::from_msgpack(buf);
        } catch (...) {
            continue;
        }

        auto type_it = msg.find("type");
        if (type_it == msg.end() || !type_it->is_string()) continue;
        const std::string type = type_it->get<std::string>();

        if (type == "log") {
            const std::string lvl = msg.value("level", "info");
            const std::string message = msg.value("message", "");
            if (lvl == "error")
                logger.print(" PATCHER ", message, COL_MAGENTA);
            else if (lvl == "warn")
                logger.print(" PATCHER ", message, COL_MAGENTA);
            else
                logger.print(" PATCHER ", message, COL_MAGENTA);
        } else if (type == "patch_event") {
            patch_event ev;
            ev.event_type = msg.value("event_type", "");
            ev.plugin_name = msg.value("plugin", "");
            ev.other_plugin = msg.value("other_plugin", "");
            ev.filename = msg.value("filename", "");
            ev.detail = msg.value("detail", "");
            ev.find_pattern = msg.value("find_pattern", "");
            ev.transform_patterns = msg.value("transform_patterns", "");
            ev.before_text = msg.value("before_text", "");
            ev.after_text = msg.value("after_text", "");
            ev.timestamp_ms = msg.value("timestamp_us", uint64_t{ 0 });
            if (!ev.event_type.empty()) patch_stream_recorder::instance().notify(ev);
        }
    }

    conn->alive.store(false);

    if (conn->notifier_id >= 0) {
        patch_update_notifier::instance().remove_listener(conn->notifier_id);
        conn->notifier_id = -1;
    }

    conn_close_write(*conn);
    close_fd(conn->read_fd);
    conn->read_fd = INVALID_FD;
}

void register_loopback_conn(fd_t write_fd, fd_t read_fd)
{
    auto conn = std::make_shared<LoopbackConn>();
    conn->write_fd = write_fd;
    conn->read_fd = read_fd;

    auto payload = build_patch_list_payload();
    if (!conn_write_frame(*conn, payload)) {
        logger.warn("loopback IPC: initial patch list write failed, dropping connection");
        close_fd(write_fd);
        close_fd(read_fd);
        return;
    }

    conn->notifier_id = patch_update_notifier::instance().add_listener([conn]()
    {
        if (!conn->alive.load()) return;
        auto p = build_patch_list_payload();
        if (!conn_write_frame(*conn, p)) {
            conn->alive.store(false);
            conn_close_write(*conn);
        }
    });

    std::thread([conn = std::move(conn)]() mutable
    {
        reader_loop(std::move(conn));
    }).detach();
}
