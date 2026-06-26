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

#include "rpc.h"
#include "millennium/types.h"

#include <stdexcept>
#include <thread>
#include <vector>

rpc_client::rpc_client(plugin_ipc::socket_fd fd) : m_fd(fd)
{
}

rpc_client::~rpc_client()
{
    m_connected.store(false);
}

bool rpc_client::read_message(json& out)
{
    if (!plugin_ipc::read_msg(m_fd, out)) {
        m_connected.store(false);
        return false;
    }
    return true;
}

bool rpc_client::send_message(const json& msg)
{
    std::lock_guard<std::mutex> lock(m_write_mutex);
    if (!plugin_ipc::write_msg(m_fd, msg)) {
        m_connected.store(false);
        return false;
    }
    return true;
}

json rpc_client::call(const std::string& method, const json& params)
{
    /*
     * if we already know the link is dead, yield briefly before throwing so that
     * any caller looping on IPC errors doesn't busy-spin at full CPU.
     */
    if (!m_connected.load()) {
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
        throw std::runtime_error("rpc_client: not connected");
    }

    int id = m_next_id.fetch_add(1);

    json req = {
        { "type",   plugin_ipc::TYPE_REQUEST },
        { "id",     id                       },
        { "method", method                   },
        { "params", params                   }
    };

    if (!send_message(req)) {
        throw std::runtime_error("rpc_client: failed to send request");
    }

    /* notifications can interleave while we wait for our response — defer them */
    while (m_connected.load()) {
        json msg;
        if (!read_message(msg)) {
            throw std::runtime_error("rpc_client: disconnected while waiting for response");
        }

        std::string type = msg.value("type", "");

        if (type == plugin_ipc::TYPE_RESPONSE && msg.value("id", -1) == id) {
            if (msg.contains("error")) {
                throw std::runtime_error(msg["error"].get<std::string>());
            }
            return msg.value("result", json(nullptr));
        }

        if (type == plugin_ipc::TYPE_NOTIFY) {
            m_deferred_notifications.push_back(std::move(msg));
            continue;
        }

        /*
         * parent sent us a request while we were blocked (e.g. evaluate during call_frontend_method).
         * handle it inline to break the deadlock.
         */
        if (type == plugin_ipc::TYPE_REQUEST && m_handler) {
            int req_id = msg.value("id", -1);
            std::string method = msg.value("method", "");
            json params = msg.value("params", json(nullptr));
            try {
                respond(req_id, m_handler(method, params));
            } catch (const std::exception& e) {
                respond_error(req_id, e.what());
            }
            continue;
        }

        fprintf(stderr, "[lua-host] unexpected message while waiting for response id=%d: %s\n", id, msg.dump().c_str());
    }

    throw std::runtime_error("rpc_client: disconnected");
}

void rpc_client::notify(const std::string& method, const json& params)
{
    json msg = {
        { "type",   plugin_ipc::TYPE_NOTIFY },
        { "method", method                  },
        { "params", params                  }
    };
    send_message(msg);
}

void rpc_client::respond(int id, const json& result)
{
    json msg = {
        { "type",   plugin_ipc::TYPE_RESPONSE },
        { "id",     id                        },
        { "result", result                    }
    };
    send_message(msg);
}

void rpc_client::respond_error(int id, const std::string& error)
{
    json msg = {
        { "type",  plugin_ipc::TYPE_RESPONSE },
        { "id",    id                        },
        { "error", error                     }
    };
    send_message(msg);
}

void rpc_client::enqueue_coroutine(lua_State* co)
{
    m_pending_coroutines.push_back(co);
}

void rpc_client::watch_fd(uintptr_t fd, lua_State* co)
{
    m_fd_watches[fd] = co;
}

void rpc_client::resume_coroutine(lua_State* co)
{
    int rc = lua_resume(co, 0);
    if (rc != LUA_OK && rc != LUA_YIELD) {
        const char* err = lua_tostring(co, -1);
        log_lua_error("coroutine error", err);
        lua_pop(co, 1);
    }
}

void rpc_client::run(request_handler handler)
{
    m_handler = handler;

    auto dispatch_notification = [&](const json& notif)
    {
        try {
            handler(notif.value("method", ""), notif.value("params", json(nullptr)));
        } catch (...) {
        }
    };

    auto drain_deferred = [&]()
    {
        while (!m_deferred_notifications.empty()) {
            auto batch = std::move(m_deferred_notifications);
            m_deferred_notifications.clear();
            for (const auto& notif : batch) {
                dispatch_notification(notif);
            }
        }
    };

    auto drain_pending_coroutines = [&]()
    {
        while (!m_pending_coroutines.empty()) {
            auto batch = std::move(m_pending_coroutines);
            m_pending_coroutines.clear();
            for (lua_State* co : batch) {
                resume_coroutine(co);
            }
        }
    };

    while (m_connected.load()) {
        drain_pending_coroutines();

        /* build poll set: IPC fd first, then all plugin-watched fds */
        std::vector<plugin_ipc::poll_fd_t> pfds;
        pfds.reserve(1 + m_fd_watches.size());

        plugin_ipc::poll_fd_t ipc_pfd{};
        ipc_pfd.fd = static_cast<decltype(ipc_pfd.fd)>(m_fd);
        ipc_pfd.events = POLLIN;
        pfds.push_back(ipc_pfd);

        for (const auto& [fd, co] : m_fd_watches) {
            plugin_ipc::poll_fd_t pfd{};
            pfd.fd = static_cast<decltype(pfd.fd)>(fd);
            pfd.events = POLLIN;
            pfds.push_back(pfd);
        }

#ifdef _WIN32
        int ready = plugin_ipc::sys_poll(pfds.data(), static_cast<ULONG>(pfds.size()), -1);
#else
        int ready = plugin_ipc::sys_poll(pfds.data(), static_cast<nfds_t>(pfds.size()), -1);
#endif
        if (ready < 0) break;

        /* resume any coroutines whose fd is now readable */
        for (size_t i = 1; i < pfds.size(); ++i) {
            if (pfds[i].revents & POLLIN) {
                uintptr_t watched_fd = static_cast<uintptr_t>(pfds[i].fd);
                auto it = m_fd_watches.find(watched_fd);
                if (it != m_fd_watches.end()) {
                    lua_State* co = it->second;
                    m_fd_watches.erase(it);
                    resume_coroutine(co);
                }
            }
        }

        /* handle IPC message if ready */
        if (pfds[0].revents & (POLLHUP | POLLERR)) break;
        if (!(pfds[0].revents & POLLIN)) {
            drain_pending_coroutines();
            continue;
        }

        json msg;
        if (!read_message(msg)) break;

        std::string type = msg.value("type", "");

        if (type == plugin_ipc::TYPE_REQUEST) {
            int id = msg.value("id", -1);
            std::string method = msg.value("method", "");
            json params = msg.value("params", json(nullptr));

            try {
                json result = handler(method, params);
                respond(id, result);
            } catch (const std::exception& e) {
                respond_error(id, e.what());
            }
        } else if (type == plugin_ipc::TYPE_NOTIFY) {
            dispatch_notification(msg);
        }
        drain_deferred();
        drain_pending_coroutines();
    }
}
