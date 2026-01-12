/**
 * ==================================================
 *   _____ _ _ _             _
 *  |     |_| | |___ ___ ___|_|_ _ _____
 *  | | | | | | | -_|   |   | | | |     |
 *  |_|_|_|_|_|___|_|_|_|_|_|_|___|_|_|_|
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
#include "millennium/types.h"
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

/**
 * thread-safe chrome devtools protocol client wrapper.
 *
 * handles the cdp websocket connection with proper async request/response matching,
 * event callbacks, and backpressure to prevent message loss under load.
 *
 * this took way too long to program.
 */
class cdp_client : public std::enable_shared_from_this<cdp_client>
{
  public:
    using ws_client = websocketpp::client<websocketpp::config::asio_client>;
    using event_callback = std::function<void(const json&)>;
    using error_callback = std::function<void(const std::string&, const std::exception&)>;

    explicit cdp_client(ws_client::connection_ptr conn);
    ~cdp_client();

    cdp_client(const cdp_client&) = delete;
    cdp_client& operator=(const cdp_client&) = delete;
    cdp_client(cdp_client&&) = delete;
    cdp_client& operator=(cdp_client&&) = delete;

    /**
     * send a cdp command and get a future for the response.
     * uses the shared js context session by default.
     */
    std::future<json> send(const std::string& method, const json& params = json::object(), std::chrono::milliseconds timeout = std::chrono::seconds(30));

    /**
     * send a cdp command with optional session targeting.
     * pass nullopt for sessionId to target the browser itself (not a specific context).
     */
    std::future<json> send_host(const std::string& method, const json& params = json::object(), std::optional<std::string> sessionId = std::nullopt,
                                std::chrono::milliseconds timeout = std::chrono::seconds(30));

    /**
     * subscribe to cdp events by method name.
     * callbacks run on a thread pool, so they won't block message processing.
     */
    void on(const std::string& event, event_callback callback);

    /** unsubscribe from a cdp event */
    void off(const std::string& event);

    /**
     * set a handler for internal errors (parsing failures, callback exceptions, etc).
     * useful for logging/debugging without crashing.
     */
    void set_error_handler(error_callback handler);

    /**
     * called by websocket layer when messages arrive.
     * blocks if the incoming queue is full (backpressure).
     */
    void handle_message(const std::string& payload);

    /** gracefully shut down all threads and fail pending requests */
    void shutdown();

    /** set the session id for the shared js context (used by send()) */
    void set_shared_js_session_id(const std::string& sessionId)
    {
        m_shared_js_session_id = sessionId;
    }

    /** check if the client is still running */
    bool is_active() const
    {
        return !m_shutdown.load(std::memory_order_acquire);
    }

  private:
    /** session id for the main js context we're talking to */
    std::string m_shared_js_session_id;

    /** tracks a cdp command we sent and are waiting for a response to */
    struct async_request
    {
        std::promise<json> promise;
        std::chrono::steady_clock::time_point timestamp;
        std::chrono::milliseconds timeout;
        std::atomic<bool> completed{ false }; // prevents double-completing promises
    };

    ws_client::connection_ptr m_conn;
    std::atomic<int> m_next_id{ 1 }; // cdp message ids increment per request
    std::atomic<bool> m_shutdown{ false };

    /** pending requests waiting for responses */
    std::mutex m_requests_mutex;
    std::unordered_map<int, std::shared_ptr<async_request>> m_pending_requests;

    /** event subscriptions (e.g., "Page.frameNavigated") */
    std::shared_mutex m_events_mutex;
    std::unordered_map<std::string, std::shared_ptr<event_callback>> m_event_callbacks;

    /** optional error callback for non-fatal issues */
    std::mutex m_error_mutex;
    error_callback m_error_handler;

    /** prevents concurrent sends on the websocket */
    std::mutex m_send_mutex;

    /** background thread that times out stale requests */
    std::mutex m_cleanup_mutex;
    std::condition_variable m_cleanup_cv;
    std::thread m_cleanup_thread;

    /** incoming message queue with backpressure to prevent dropping messages */
    std::deque<std::string> m_incoming_queue;
    std::mutex m_incoming_mutex;
    std::condition_variable m_incoming_cv;       // wakes worker when messages arrive
    std::condition_variable m_incoming_space_cv; // wakes handle_message() when space opens up
    std::thread m_incoming_worker;
    static constexpr size_t m_incoming_queue_limit = 1000; // blocks at this size instead of dropping

    /** thread pool for running event callbacks without blocking message processing */
    std::shared_ptr<thread_pool> m_callback_pool;

    /** runs in m_cleanup_thread, periodically times out old requests */
    void cleanup_loop();
    void cleanup_stale_requests();

    /** safely invoke the error handler if set */
    void invoke_error_handler(const std::string& context, const std::exception& e);

    /** parse and dispatch incoming cdp messages (runs in m_incoming_worker) */
    void process_message(const std::string& payload);
};