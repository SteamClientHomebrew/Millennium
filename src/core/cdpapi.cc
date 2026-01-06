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

#include "millennium/cdpapi.h"
#include "millennium/logger.h"
#include "millennium/thread_pool.h"
#include <condition_variable>
#include <deque>
#include <memory>
#include <unordered_map>

cdp_client::cdp_client(WSClient::connection_ptr conn) : m_conn(std::move(conn)), m_callback_pool(std::make_shared<thread_pool>(4))
{
    m_cleanup_thread = std::thread(&cdp_client::cleanup_loop, this);
    m_incoming_worker = std::thread([this]()
    {
        while (!m_shutdown.load(std::memory_order_acquire)) {
            std::string payload;
            {
                std::unique_lock<std::mutex> lock(m_incoming_mutex);
                m_incoming_cv.wait(lock, [this] { return m_shutdown.load(std::memory_order_acquire) || !m_incoming_queue.empty(); });

                if (!m_incoming_queue.empty()) {
                    payload = std::move(m_incoming_queue.front());
                    m_incoming_queue.pop_front();
                }
            }

            if (payload.empty()) {
                continue;
            }

            try {
                this->process_message(payload);
            } catch (const std::exception& e) {
                invoke_error_handler("Processing incoming message", e);
            } catch (...) {
                LOG_ERROR("cdp_client: Unknown exception processing incoming message");
                invoke_error_handler("Processing incoming message", std::runtime_error("Unknown exception in message processing"));
            }
        }

        std::deque<std::string> remaining;
        {
            std::lock_guard<std::mutex> lock(m_incoming_mutex);
            remaining.swap(m_incoming_queue);
        }
        for (auto& p : remaining) {
            try {
                this->process_message(p);
            } catch (const std::exception& e) {
                LOG_ERROR(std::string("cdp_client: Exception processing remaining message: ") + e.what());
            } catch (...) {
                LOG_ERROR("cdp_client: Unknown exception processing remaining message");
            }
        }
    });
}

cdp_client::~cdp_client()
{
    shutdown();
}

void cdp_client::shutdown()
{
    bool expected = false;
    if (!m_shutdown.compare_exchange_strong(expected, true, std::memory_order_acq_rel)) {
        return; /** already shutdown */
    }

    m_incoming_cv.notify_all();
    if (m_incoming_worker.joinable()) m_incoming_worker.join();
    {
        std::lock_guard<std::mutex> lock(m_cleanup_mutex);
        m_cleanup_cv.notify_all();
    }
    if (m_cleanup_thread.joinable()) m_cleanup_thread.join();
    if (m_callback_pool) m_callback_pool->shutdown();

    std::unique_lock<std::mutex> lock(m_requests_mutex);
    for (auto& [id, req] : m_pending_requests) {
        if (!req->completed.exchange(true, std::memory_order_acq_rel)) {
            try {
                req->promise.set_exception(std::make_exception_ptr(std::runtime_error("CDPClient shutdown")));
            } catch (...) {
                LOG_ERROR("Failed to set exception on pending request during shutdown");
            }
        }
    }
    m_pending_requests.clear();
    {
        std::lock_guard<std::mutex> queue_lock(m_queue_mutex);
        m_queued_responses.clear();
    }
    {
        std::unique_lock<std::shared_mutex> events_lock(m_events_mutex);
        m_event_callbacks.clear();
    }
}

void cdp_client::set_error_handler(ErrorCallback handler)
{
    std::lock_guard<std::mutex> lock(m_error_mutex);
    m_error_handler = std::move(handler);
}

std::future<cdp_client::json> cdp_client::send(const std::string& method, const json& params, std::chrono::milliseconds timeout)
{
    return this->send_host(method, params, sharedJsContextSessionId, timeout);
}

std::future<cdp_client::json> cdp_client::send_host(const std::string& method, const json& params, std::optional<std::string> sessionId, std::chrono::milliseconds timeout)
{
    int id = m_next_id.fetch_add(1, std::memory_order_relaxed);

    auto pending = std::make_shared<PendingRequest>();
    pending->timestamp = std::chrono::steady_clock::now();
    pending->timeout = timeout;
    auto future = pending->promise.get_future();

    json message = {
        { "id",     id     },
        { "method", method },
        { "params", params }
    };

    if (sessionId.has_value()) {
        message["sessionId"] = sessionId.value();
    }

    std::string payload = message.dump();
    {
        std::lock_guard<std::mutex> send_lock(m_send_mutex);

        if (m_shutdown.load(std::memory_order_acquire)) {
            if (!pending->completed.exchange(true, std::memory_order_acq_rel)) {
                try {
                    pending->promise.set_exception(std::make_exception_ptr(std::runtime_error("Client shutdown")));
                } catch (...) {
                    LOG_ERROR("Failed to set exception on pending request due to client shutdown");
                }
            }
            return future;
        }

        {
            std::lock_guard<std::mutex> lock(m_requests_mutex);
            m_pending_requests[id] = pending;

            if (check_queued_response(id, pending)) {
                m_pending_requests.erase(id);
                return future;
            }
        }

        websocketpp::lib::error_code ec;
        ec = m_conn->send(payload, websocketpp::frame::opcode::text);

        if (ec) {
            std::lock_guard<std::mutex> lock(m_requests_mutex);
            m_pending_requests.erase(id);

            if (!pending->completed.exchange(true, std::memory_order_acq_rel)) {
                try {
                    pending->promise.set_exception(std::make_exception_ptr(std::runtime_error("Send failed: " + ec.message())));
                } catch (...) {
                    LOG_ERROR("Failed to set exception on pending request due to send failure");
                }
            }
        }
    }

    return future;
}

void cdp_client::on(const std::string& event, EventCallback callback)
{
    if (m_shutdown.load(std::memory_order_acquire)) {
        return;
    }

    std::unique_lock<std::shared_mutex> lock(m_events_mutex);
    m_event_callbacks[event] = std::make_shared<EventCallback>(std::move(callback));
}

void cdp_client::off(const std::string& event)
{
    std::unique_lock<std::shared_mutex> lock(m_events_mutex);
    m_event_callbacks.erase(event);
}

void cdp_client::handle_message(const std::string& payload)
{
    if (m_shutdown.load(std::memory_order_acquire)) {
        return;
    }

    std::lock_guard<std::mutex> lock(m_incoming_mutex);
    if (m_incoming_queue.size() >= m_incoming_queue_limit) {
        LOG_ERROR("cdp_client incoming queue full, dropping message");
        return;
    }
    m_incoming_queue.emplace_back(payload);
    m_incoming_cv.notify_one();
}

void cdp_client::process_message(const std::string& payload)
{
    if (m_shutdown.load(std::memory_order_acquire)) {
        return;
    }

    json message;
    try {
        message = json::parse(payload);
    } catch (const json::parse_error& e) {
        try {
            invoke_error_handler("JSON parse", e);
        } catch (...) {
            LOG_ERROR("Failed to invoke error handler for JSON parse exception");
        }
        return;
    }

    if (message.contains("id") && message["id"].is_number_integer()) {
        int id = message["id"].get<int>();
        std::shared_ptr<PendingRequest> pending;

        {
            std::lock_guard<std::mutex> lock(m_requests_mutex);
            auto it = m_pending_requests.find(id);
            if (it != m_pending_requests.end()) {
                pending = it->second;
                m_pending_requests.erase(it);
            }
        }

        if (pending) {
            if (!pending->completed.exchange(true, std::memory_order_acq_rel)) {
                try {
                    if (message.contains("error")) {
                        std::string error_msg = "CDP Error";
                        if (message["error"].contains("message") && message["error"]["message"].is_string()) {
                            error_msg = message["error"]["message"].get<std::string>();
                        }
                        pending->promise.set_exception(std::make_exception_ptr(std::runtime_error(error_msg)));
                    } else if (message.contains("result")) {
                        pending->promise.set_value(message["result"]);
                    } else {
                        pending->promise.set_exception(std::make_exception_ptr(std::runtime_error("Invalid CDP response")));
                    }
                } catch (...) {
                    LOG_ERROR("Failed to set value/exception on pending request");
                }
            }
        } else {
            std::lock_guard<std::mutex> lock(m_queue_mutex);
            if (m_queued_responses.size() < m_queued_response_limit) {
                m_queued_responses[id] = QueuedResponse{ message, std::chrono::steady_clock::now() };
            }
        }
    } else if (message.contains("method") && message["method"].is_string()) {

        std::string method = message["method"].get<std::string>();
        std::shared_ptr<EventCallback> callback;

        {
            std::shared_lock<std::shared_mutex> lock(m_events_mutex);
            auto it = m_event_callbacks.find(method);
            if (it != m_event_callbacks.end()) {
                callback = it->second;
            }
        }

        if (callback) {
            json params = message.contains("params") ? message["params"] : json::object();

            if (m_callback_pool) {
                m_callback_pool->enqueue([this, callback, params]()
                {
                    try {
                        (*callback)(params);
                    } catch (const std::exception& e) {
                        try {
                            invoke_error_handler("Event callback", e);
                        } catch (...) {
                            LOG_ERROR("Failed to invoke error handler for event callback exception");
                        }
                    } catch (...) {
                        try {
                            invoke_error_handler("Event callback", std::runtime_error("Unknown exception"));
                        } catch (...) {
                            LOG_ERROR("Failed to invoke error handler for event callback exception");
                        }
                    }
                });
            }
        }
    }
}

bool cdp_client::check_queued_response(int id, std::shared_ptr<PendingRequest>& pending)
{
    std::lock_guard<std::mutex> lock(m_queue_mutex);
    auto it = m_queued_responses.find(id);
    if (it != m_queued_responses.end()) {
        json message = std::move(it->second.message);
        m_queued_responses.erase(it);

        if (!pending->completed.exchange(true, std::memory_order_acq_rel)) {
            try {
                if (message.contains("error")) {
                    std::string error_msg = "CDP Error";
                    if (message["error"].contains("message") && message["error"]["message"].is_string()) {
                        error_msg = message["error"]["message"].get<std::string>();
                    }
                    pending->promise.set_exception(std::make_exception_ptr(std::runtime_error(error_msg)));
                } else if (message.contains("result")) {
                    pending->promise.set_value(message["result"]);
                } else {
                    pending->promise.set_exception(std::make_exception_ptr(std::runtime_error("Invalid CDP response")));
                }
            } catch (...) {
                LOG_ERROR("Failed to set value/exception on pending request from queued response");
            }
        }
        return true;
    }
    return false;
}

void cdp_client::cleanup_loop()
{
    while (!m_shutdown.load(std::memory_order_acquire)) {
        {
            std::unique_lock<std::mutex> lock(m_cleanup_mutex);
            m_cleanup_cv.wait_for(lock, std::chrono::seconds(1), [this] { return m_shutdown.load(std::memory_order_acquire); });
        }

        if (!m_shutdown.load(std::memory_order_acquire)) {
            cleanup_stale_requests();
        }
    }
}

void cdp_client::cleanup_stale_requests()
{
    auto now = std::chrono::steady_clock::now();
    std::vector<std::pair<int, std::shared_ptr<PendingRequest>>> to_timeout;

    {
        std::lock_guard<std::mutex> lock(m_requests_mutex);
        for (auto it = m_pending_requests.begin(); it != m_pending_requests.end();) {
            auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(now - it->second->timestamp);

            if (elapsed > it->second->timeout) {
                to_timeout.push_back(*it);
                it = m_pending_requests.erase(it);
            } else {
                ++it;
            }
        }
    }

    for (auto& [id, pending] : to_timeout) {
        if (!pending->completed.exchange(true, std::memory_order_acq_rel)) {
            try {
                pending->promise.set_exception(std::make_exception_ptr(std::runtime_error("Request timeout")));
            } catch (...) {
                LOG_ERROR("Failed to set exception on pending request due to timeout");
            }
        }
    }

    {
        std::lock_guard<std::mutex> lock(m_queue_mutex);
        for (auto it = m_queued_responses.begin(); it != m_queued_responses.end();) {
            auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(now - it->second.timestamp);
            if (elapsed > std::chrono::seconds(30)) {
                it = m_queued_responses.erase(it);
            } else {
                ++it;
            }
        }
    }
}

void cdp_client::invoke_error_handler(const std::string& context, const std::exception& e)
{
    std::lock_guard<std::mutex> lock(m_error_mutex);
    if (m_error_handler) {
        try {
            m_error_handler(context, e);
        } catch (...) {
            LOG_ERROR("Error handler threw an exception");
        }
    }
}