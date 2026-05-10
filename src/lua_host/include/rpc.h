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

#include "millennium/plugin_ipc.h"
#include <nlohmann/json.hpp>
#include <atomic>
#include <functional>
#include <mutex>
#include <string>

/**
 * Child-side RPC client — single-threaded, blocking.
 *
 * When we need something from the parent (like calling a frontend method), we just
 * block and read frames until our response shows up. This works because the parent
 * never sends us a new request while we're already handling one.
 */
class rpc_client
{
  public:
    using request_handler = std::function<nlohmann::json(const std::string& method, const nlohmann::json& params)>;

    explicit rpc_client(plugin_ipc::socket_fd fd);
    ~rpc_client();

    nlohmann::json call(const std::string& method, const nlohmann::json& params = nullptr);
    void notify(const std::string& method, const nlohmann::json& params = nullptr);
    void run(request_handler handler);
    bool connected() const
    {
        return m_connected.load();
    }

  private:
    bool read_message(nlohmann::json& out);
    bool send_message(const nlohmann::json& msg);
    void respond(int id, const nlohmann::json& result);
    void respond_error(int id, const std::string& error);

    plugin_ipc::socket_fd m_fd;
    std::atomic<bool> m_connected{ true };
    std::atomic<int> m_next_id{ 1 };
    std::mutex m_write_mutex;

    std::vector<nlohmann::json> m_deferred_notifications;
};
