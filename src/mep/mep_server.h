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

#include "mep_router.h"
#include <atomic>
#include <cstdint>
#include <string>
#include <thread>
#include <vector>

namespace mep
{

#ifdef _WIN32
static constexpr uint16_t DEFAULT_TCP_PORT = 7780;
#else
static constexpr const char* DEFAULT_SOCKET_PATH = "/tmp/millennium-mep.sock";
#endif

static constexpr std::size_t MAX_MESSAGE_SIZE = 4u * 1024u * 1024u;

class server
{
  public:
#ifdef _WIN32
    explicit server(router& router, uint16_t port = DEFAULT_TCP_PORT);
#else
    explicit server(router& router, std::string socket_path = DEFAULT_SOCKET_PATH);
#endif
    ~server();

    void start();
    void stop();

    bool is_running() const noexcept;

    using socket_t = int;

  private:
    void accept_loop();
    void handle_client(socket_t client_fd);

    bool read_frame(socket_t fd, std::vector<uint8_t>& out) const;
    bool write_frame(socket_t fd, const std::vector<uint8_t>& data) const;

    router& m_router;
#ifdef _WIN32
    uint16_t m_port;
#else
    std::string m_socket_path;
#endif
    socket_t m_server_fd = -1;
    std::atomic<bool> m_running{ false };
    std::thread m_accept_thread;
};

} // namespace mep
