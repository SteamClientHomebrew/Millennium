/**
 * ==================================================
 *   _____ _ _ _             _
 *  |     |_| | |___ ___ ___|_|_ _ _____
 *  | | | | | | | -_|   |   | | | |     |
 *  |_|_|_|_|_|_|___|_|_|_|_|_|___|_|_|_|
 *
 * ==================================================
 *
 * Copyright (c) 2025 Project Millennium
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
#include "millennium/thread_pool.h"

#include <websocketpp/client.hpp>
#include <websocketpp/config/asio_no_tls_client.hpp>
#include <nlohmann/json.hpp>
#include <functional>
#include <unordered_map>
#include <shared_mutex>
#include <future>
#include <atomic>
#include <thread>
#include <condition_variable>
#include <deque>
#include <string>
#include <memory>

class cdp_client : public std::enable_shared_from_this<cdp_client>
{
  public:
    using WSClient = websocketpp::client<websocketpp::config::asio_client>;
    using json = nlohmann::json;
    using EventCallback = std::function<void(const json&)>;
    using ErrorCallback = std::function<void(const std::string&, const std::exception&)>;

    explicit cdp_client(WSClient::connection_ptr conn);
    ~cdp_client();

    cdp_client(const cdp_client&) = delete;
    cdp_client& operator=(const cdp_client&) = delete;
    cdp_client(cdp_client&&) = delete;
    cdp_client& operator=(cdp_client&&) = delete;

    std::future<json> send(const std::string& method, const json& params = json::object(), std::chrono::milliseconds timeout = std::chrono::seconds(30));
    std::future<json> send_host(const std::string& method, const json& params = json::object(), std::optional<std::string> sessionId = std::nullopt,
                                std::chrono::milliseconds timeout = std::chrono::seconds(30));

    void on(const std::string& event, EventCallback callback);
    void off(const std::string& event);

    void set_error_handler(ErrorCallback handler);
    void handle_message(const std::string& payload);

    void shutdown();
    void set_shared_js_session_id(const std::string& sessionId)
    {
        sharedJsContextSessionId = sessionId;
    }

    bool is_active() const
    {
        return !m_shutdown.load(std::memory_order_acquire);
    }

  private:
    std::string sharedJsContextSessionId;

    struct PendingRequest
    {
        std::promise<json> promise;
        std::chrono::steady_clock::time_point timestamp;
        std::chrono::milliseconds timeout;
        std::atomic<bool> completed{ false };
    };

    struct QueuedResponse
    {
        json message;
        std::chrono::steady_clock::time_point timestamp;
    };

    WSClient::connection_ptr m_conn;
    std::atomic<int> m_next_id{ 1 };
    std::atomic<bool> m_shutdown{ false };

    std::mutex m_requests_mutex, m_error_mutex, m_send_mutex, m_queue_mutex, m_cleanup_mutex;
    std::unordered_map<int, std::shared_ptr<PendingRequest>> m_pending_requests;
    std::shared_mutex m_events_mutex;
    std::unordered_map<std::string, std::shared_ptr<EventCallback>> m_event_callbacks;
    ErrorCallback m_error_handler;
    std::unordered_map<int, QueuedResponse> m_queued_responses;
    const size_t m_queued_response_limit{ 1000 };
    std::thread m_cleanup_thread;
    std::condition_variable m_cleanup_cv;

    std::deque<std::string> m_incoming_queue;
    std::mutex m_incoming_mutex;
    std::condition_variable m_incoming_cv;
    std::thread m_incoming_worker;
    const size_t m_incoming_queue_limit{ 1000 };

    std::shared_ptr<thread_pool> m_callback_pool;

    void cleanup_loop();
    void cleanup_stale_requests();
    void invoke_error_handler(const std::string& context, const std::exception& e);
    bool check_queued_response(int id, std::shared_ptr<PendingRequest>& pending);

    void process_message(const std::string& payload);
};