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

void rpc_client::run(request_handler handler)
{
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

    while (m_connected.load()) {
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
    }
}
