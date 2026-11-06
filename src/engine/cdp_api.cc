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

#include "millennium/cdp_api.h"
#include "millennium/logger.h"
#include "millennium/thread_pool.h"
#include <condition_variable>
#include <deque>
#include <memory>
#include <unordered_map>
#include <limits>

cdp_client::cdp_client(send_fn sender) : m_sender(std::move(sender)), m_callback_pool(std::make_shared<thread_pool>(4))
{
    /** start the cleanup thread worker */
    m_cleanup_thread = std::thread(&cdp_client::cleanup_loop, this);
    /** start the incoming message worker */
    m_incoming_worker = std::thread([this]()
    {
        while (!m_shutdown.load(std::memory_order_acquire)) {
            std::string payload;
            {
                std::unique_lock<std::mutex> lock(m_incoming_mutex);
                /** wait for incoming messages or shutdown signal */
                m_incoming_cv.wait(lock, [this]
                {
                    return m_shutdown.load(std::memory_order_acquire) || !m_incoming_queue.empty();
                });

                if (!m_incoming_queue.empty()) {
                    payload = std::move(m_incoming_queue.front());
                    m_incoming_queue.pop_front();
                    m_incoming_space_cv.notify_one(); /** signal space available for backpressure */
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

        /** process remaining messages on shutdown */
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

    if (m_incoming_worker.joinable()) m_incoming_worker.detach();
    if (m_cleanup_thread.joinable()) m_cleanup_thread.detach();
}

/**
 * Shutdown the connection to the CDP endpoint and clean up resources.
 */
void cdp_client::shutdown()
{
    bool expected = false;
    if (!m_shutdown.compare_exchange_strong(expected, true, std::memory_order_acq_rel)) {
        return; /** already shutdown */
    }

    /** Callbacks can be blocked in send_host().get(). Release them before joining workers. */
    std::unordered_map<int, std::shared_ptr<async_request>> pending;
    {
        std::lock_guard<std::mutex> lock(m_requests_mutex);
        pending.swap(m_pending_requests);
        m_shared_js_session_id.clear();
    }
    for (auto& [id, request] : pending) {
        fail_request(request, std::make_exception_ptr(std::runtime_error("CDPClient shutdown")));
    }

    /** wake up all waiting threads */
    m_incoming_cv.notify_all();
    m_incoming_space_cv.notify_all();
    m_session_cv.notify_all();

    if (m_incoming_worker.joinable()) m_incoming_worker.join();

    {
        std::lock_guard<std::mutex> lock(m_cleanup_mutex);
        m_cleanup_cv.notify_all();
    }
    if (m_cleanup_thread.joinable()) m_cleanup_thread.join();
    if (m_callback_pool) m_callback_pool->shutdown();

    /** clear all event callbacks */
    {
        std::unique_lock<std::shared_mutex> events_lock(m_events_mutex);
        m_event_callbacks.clear();
        m_token_to_event.clear();
    }

    logger.log("Successfully shut down cdp_client...");
}

/**
 * Set the error handler callback
 */
void cdp_client::set_error_handler(error_callback handler)
{
    std::lock_guard<std::mutex> lock(m_error_mutex);
    m_error_handler = std::move(handler);
}

void cdp_client::fail_request(const std::shared_ptr<async_request>& request, std::exception_ptr error)
{
    if (!request->completed.exchange(true, std::memory_order_acq_rel)) {
        request->promise.set_exception(std::move(error));
    }
}

bool cdp_client::owner_closed_locked(const async_request& request) const
{
    return (!request.target_id.empty() && m_closed_targets.count(request.target_id)) ||
           (!request.session_id.empty() && m_closed_sessions.count(request.session_id)) ||
           (!request.child_session_id.empty() && m_closed_sessions.count(request.child_session_id));
}

std::exception_ptr cdp_client::cancellation_locked(const async_request& request, const std::string& reason, std::optional<int> code) const
{
    std::string target = request.target_id;
    const std::string session = request.child_session_id.empty() ? request.session_id : request.child_session_id;
    auto it = m_sessions.find(session);
    if (target.empty() && it != m_sessions.end()) target = it->second.target_id;
    return std::make_exception_ptr(cdp_cancelled(reason, code, request.method, target, session));
}

bool cdp_client::bind_session_locked(const std::string& session, const std::string& target, const std::string& parent)
{
    if (session.empty()) return false;
    if (m_closed_sessions.count(session) || m_closed_targets.count(target) || (!parent.empty() && m_closed_sessions.count(parent))) {
        m_closed_sessions.insert(session);
        return false;
    }
    m_sessions[session] = {target, parent};
    return true;
}

void cdp_client::invalidate_locked(const std::string& target, const std::string& session, cancelled_requests& cancelled)
{
    if (!target.empty()) m_closed_targets.insert(target);
    if (!session.empty()) m_closed_sessions.insert(session);
    bool changed;
    do {
        changed = false;
        for (const auto& [id, owner] : m_sessions) {
            if (m_closed_targets.count(owner.target_id) || m_closed_sessions.count(owner.parent_session_id)) {
                changed |= m_closed_sessions.insert(id).second;
            }
        }
    } while (changed);
    if (m_closed_sessions.count(m_shared_js_session_id)) m_shared_js_session_id.clear();
    for (auto it = m_pending_requests.begin(); it != m_pending_requests.end();) {
        if (owner_closed_locked(*it->second)) {
            cancelled.push_back(it->second);
            it = m_pending_requests.erase(it);
        } else {
            ++it;
        }
    }
}

void cdp_client::process_lifecycle(const std::string& method, const json& params, const std::string& parent)
{
    const auto string_field = [](const json& value, const char* key) -> std::string
    {
        auto it = value.find(key);
        return it != value.end() && it->is_string() ? it->get<std::string>() : std::string{};
    };
    cancelled_requests cancelled;
    std::vector<std::exception_ptr> errors;
    {
        std::lock_guard<std::mutex> lock(m_requests_mutex);
        if (method == "Target.targetDestroyed") {
            invalidate_locked(string_field(params, "targetId"), "", cancelled);
        } else if (method == "Target.detachedFromTarget") {
            invalidate_locked("", string_field(params, "sessionId"), cancelled);
        } else if (method == "Target.attachedToTarget") {
            const auto child = string_field(params, "sessionId");
            const auto info = params.find("targetInfo");
            const auto target = info != params.end() ? string_field(*info, "targetId") : std::string{};
            if (!bind_session_locked(child, target, parent)) invalidate_locked("", child, cancelled);
        }
        for (const auto& request : cancelled) errors.push_back(cancellation_locked(*request, "CDP owner closed"));
    }
    for (size_t i = 0; i < cancelled.size(); ++i) fail_request(cancelled[i], errors[i]);
}

void cdp_client::set_shared_js_session_id(const std::string& sessionId)
{
    {
        std::lock_guard<std::mutex> lock(m_requests_mutex);
        if (m_shutdown.load(std::memory_order_acquire) || m_closed_sessions.count(sessionId)) {
            async_request request;
            request.session_id = sessionId;
            std::rethrow_exception(cancellation_locked(request, "SharedJSContext session closed"));
        }
        m_shared_js_session_id = sessionId;
    }
    m_session_cv.notify_all();
}

std::string cdp_client::get_shared_js_session_id() const
{
    std::lock_guard<std::mutex> lock(m_requests_mutex);
    return m_shared_js_session_id;
}

void cdp_client::wait_for_shared_js_session()
{
    std::unique_lock<std::mutex> lock(m_requests_mutex);
    m_session_cv.wait(lock, [this] { return !m_shared_js_session_id.empty() || m_shutdown.load(std::memory_order_acquire); });
}

bool cdp_client::is_session_active(const std::string& session_id) const
{
    std::lock_guard<std::mutex> lock(m_requests_mutex);
    return !session_id.empty() && !m_shutdown.load(std::memory_order_acquire) && !m_closed_sessions.count(session_id);
}

bool cdp_client::is_target_active(const std::string& target_id) const
{
    std::lock_guard<std::mutex> lock(m_requests_mutex);
    return !target_id.empty() && !m_shutdown.load(std::memory_order_acquire) && !m_closed_targets.count(target_id);
}

/**
 * Send a CDP message to the SharedJSContext session if available.
 * This function assumes we've already found the sessionId of the SharedJSContext.
 */
std::future<json> cdp_client::send(const std::string& method, const json& params, std::chrono::milliseconds timeout)
{
    auto session = get_shared_js_session_id();
    if (session.empty()) {
        std::promise<json> promise;
        auto future = promise.get_future();
        promise.set_exception(std::make_exception_ptr(cdp_cancelled("SharedJSContext session is not available", std::nullopt, method, "", "")));
        return future;
    }
    /** forward the message to the host session with proper sessionId */
    return send_host(method, params, session, timeout);
}

std::future<json> cdp_client::send_host(const std::string& method, const json& params, std::optional<std::string> sessionId, std::chrono::milliseconds timeout)
{
    int id = m_next_id.fetch_add(1, std::memory_order_relaxed);
    auto pending = std::make_shared<async_request>();
    pending->method = method;
    pending->session_id = sessionId.value_or("");
    if (params.contains("targetId") && params["targetId"].is_string()) pending->target_id = params["targetId"].get<std::string>();
    if (method.rfind("Target.", 0) == 0 && params.contains("sessionId") && params["sessionId"].is_string()) {
        pending->child_session_id = params["sessionId"].get<std::string>();
    }
    pending->timestamp = std::chrono::steady_clock::now();
    pending->timeout = timeout;
    auto future = pending->promise.get_future();
    /** construct the underlying CDP message */
    json message = {{"id", id}, {"method", method}, {"params", params}};
    /**
     * add the sessionId of the request. this has to be done here as
     * this api only exposes the params, and sessionId needs to be top level
     */
    if (sessionId.has_value()) message["sessionId"] = *sessionId;
    const std::string payload = message.dump();
    std::exception_ptr error;
    /** Check shutdown and register atomically with respect to draining pending requests. */
    {
        std::lock_guard<std::mutex> lock(m_requests_mutex);
        if (m_shutdown.load(std::memory_order_acquire)) {
            error = std::make_exception_ptr(std::runtime_error("Client shutdown"));
        } else if (owner_closed_locked(*pending)) {
            error = cancellation_locked(*pending, "CDP owner closed");
        } else {
            /** register req before sending it to avoid race conditions or deadlocking */
            m_pending_requests[id] = pending;
        }
    }
    if (error) {
        fail_request(pending, error);
        return future;
    }
    std::lock_guard<std::mutex> send_lock(m_send_mutex);
    {
        std::lock_guard<std::mutex> lock(m_requests_mutex);
        if (!m_pending_requests.count(id)) return future;
    }
    try {
        if (!m_sender(payload)) error = std::make_exception_ptr(std::runtime_error("Send failed"));
    } catch (...) {
        error = std::current_exception();
    }
    if (error) {
        /** remove from pending and fail the promise */
        bool owned;
        {
            std::lock_guard<std::mutex> lock(m_requests_mutex);
            owned = m_pending_requests.erase(id) != 0;
        }
        if (owned) fail_request(pending, error);
    }
    return future;
}

/**
 * Listen for a method event from the CDP endpoint.
 * The callback will be invoked on a thread pool thread, meaning it's safe to do blocking operations,
 * or call back into the cdp_client without deadlocking the socket thread.
 * Returns a token for targeted removal via off(int).
 */
int cdp_client::on(const std::string& event, event_callback callback)
{
    int token = m_next_event_token.fetch_add(1, std::memory_order_relaxed);

    if (m_shutdown.load(std::memory_order_acquire)) {
        return token;
    }

    std::unique_lock<std::shared_mutex> lock(m_events_mutex);
    m_event_callbacks[event].push_back({ token, std::make_shared<event_callback>(std::move(callback)) });
    m_token_to_event[token] = event;
    return token;
}

/**
 * Remove a specific event listener by its registration token.
 */
void cdp_client::off(int token)
{
    std::unique_lock<std::shared_mutex> lock(m_events_mutex);
    auto tok_it = m_token_to_event.find(token);
    if (tok_it == m_token_to_event.end()) return;

    const std::string event = tok_it->second;
    m_token_to_event.erase(tok_it);

    auto ev_it = m_event_callbacks.find(event);
    if (ev_it == m_event_callbacks.end()) return;

    auto& listeners = ev_it->second;
    listeners.erase(std::remove_if(listeners.begin(), listeners.end(),
                                   [token](const event_listener& l)
    {
        return l.token == token;
    }),
                    listeners.end());
    if (listeners.empty()) {
        m_event_callbacks.erase(ev_it);
    }
}

/**
 * Remove ALL listeners for an event.
 */
void cdp_client::off_all(const std::string& event)
{
    std::unique_lock<std::shared_mutex> lock(m_events_mutex);
    auto ev_it = m_event_callbacks.find(event);
    if (ev_it == m_event_callbacks.end()) return;

    for (const auto& listener : ev_it->second) {
        m_token_to_event.erase(listener.token);
    }
    m_event_callbacks.erase(ev_it);
}

/**
 * Handle an incoming message from the CDP endpoint.
 */
void cdp_client::handle_message(const std::string& payload)
{
    if (m_shutdown.load(std::memory_order_acquire)) {
        return;
    }

    std::unique_lock<std::mutex> lock(m_incoming_mutex);

    m_incoming_space_cv.wait(lock, [this]
    {
        return m_shutdown.load(std::memory_order_acquire) || m_incoming_queue.size() < m_incoming_queue_limit;
    });

    if (m_shutdown.load(std::memory_order_acquire)) {
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

    if (message.contains("id") && message["id"].is_number_integer() && message["id"] >= std::numeric_limits<int>::min() && message["id"] <= std::numeric_limits<int>::max()) {
        int id = message["id"].get<int>();
        std::shared_ptr<async_request> pending;

        std::exception_ptr error;
        cancelled_requests cancelled;
        std::vector<std::exception_ptr> cancelled_errors;
        {
            std::lock_guard<std::mutex> lock(m_requests_mutex);
            auto it = m_pending_requests.find(id);
            if (it != m_pending_requests.end()) {
                pending = it->second;
                m_pending_requests.erase(it);
                if (message.contains("error")) {
                    const auto& detail = message["error"];
                    std::string text = "CDP Error";
                    std::optional<int> code;
                    if (detail.contains("message") && detail["message"].is_string()) text = detail["message"].get<std::string>();
                    if (detail.contains("code") && detail["code"].is_number_integer() && detail["code"] >= std::numeric_limits<int>::min() && detail["code"] <= std::numeric_limits<int>::max()) code = detail["code"].get<int>();
                    const auto session = pending->child_session_id.empty() ? pending->session_id : pending->child_session_id;
                    const bool target_gone = text == "No target with given id found";
                    const bool session_gone = text == "No session with given id" || text == "Session with given id not found.";
                    if ((target_gone && (!pending->target_id.empty() || !session.empty())) || (session_gone && !session.empty())) {
                        invalidate_locked(target_gone ? pending->target_id : "", (target_gone && !pending->target_id.empty()) ? "" : session, cancelled);
                        error = cancellation_locked(*pending, text, code);
                    } else {
                        error = std::make_exception_ptr(cdp_protocol_error(text, code, pending->method, pending->target_id, session));
                    }
                } else if (pending->method == "Target.attachToTarget" && message.contains("result") && message["result"].contains("sessionId") &&
                           message["result"]["sessionId"].is_string()) {
                    const auto session = message["result"]["sessionId"].get<std::string>();
                    if (!bind_session_locked(session, pending->target_id, pending->session_id)) {
                        invalidate_locked("", session, cancelled);
                        pending->child_session_id = session;
                        error = cancellation_locked(*pending, "CDP attach owner closed");
                    }
                }
                for (const auto& request : cancelled) cancelled_errors.push_back(cancellation_locked(*request, "CDP owner closed"));
            }
        }
        for (size_t i = 0; i < cancelled.size(); ++i) fail_request(cancelled[i], cancelled_errors[i]);
        if (pending) {
            if (error) {
                fail_request(pending, error);
            } else if (!pending->completed.exchange(true, std::memory_order_acq_rel)) {
                if (message.contains("result")) pending->promise.set_value(message["result"]);
                else pending->promise.set_exception(std::make_exception_ptr(std::runtime_error("Invalid CDP response")));
            }
        }
    } else if (message.contains("method") && message["method"].is_string()) {
        std::string method = message["method"].get<std::string>();

        json params = message.contains("params") ? message["params"] : json::object();
        const std::string session = message.contains("sessionId") && message["sessionId"].is_string() ? message["sessionId"].get<std::string>() : "";
        process_lifecycle(method, params, session);
        if (!session.empty() && !params.contains("sessionId")) params["sessionId"] = session;

        std::vector<std::shared_ptr<event_callback>> callbacks;
        {
            std::shared_lock<std::shared_mutex> lock(m_events_mutex);
            auto it = m_event_callbacks.find(method);
            if (it != m_event_callbacks.end()) {
                for (const auto& listener : it->second) {
                    callbacks.push_back(listener.callback);
                }
            }
        }

        for (const auto& callback : callbacks) {
            if (m_callback_pool) {
                m_callback_pool->enqueue([this, callback, params, session, method]()
                {
                    try {
                        if (!session.empty() && method.rfind("Target.", 0) != 0 && !is_session_active(session)) return;
                        (*callback)(params);
                    } catch (const cdp_cancelled&) {
                        return;
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

/**
 * Cleanup loop running in a separate thread.
 */
void cdp_client::cleanup_loop()
{
    while (!m_shutdown.load(std::memory_order_acquire)) {
        {
            std::unique_lock<std::mutex> lock(m_cleanup_mutex);
            m_cleanup_cv.wait_for(lock, std::chrono::seconds(1), [this]
            {
                return m_shutdown.load(std::memory_order_acquire);
            });
        }

        if (!m_shutdown.load(std::memory_order_acquire)) {
            cleanup_stale_requests();
        }
    }
}

/**
 * Cleanup stale requests that have timed out.
 * This method is called periodically by the cleanup thread.
 * It checks for requests that have exceeded their timeout and
 * sets an exception on their promises.
 */
void cdp_client::cleanup_stale_requests()
{
    auto now = std::chrono::steady_clock::now();
    std::vector<std::pair<int, std::shared_ptr<async_request>>> to_timeout;

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
}

/**
 * Invoke the error handler callback safely.
 * This method catches any exceptions thrown by the error handler itself.
 */
void cdp_client::invoke_error_handler(const std::string& context, const std::exception& e)
{
    error_callback handler;
    {
        std::lock_guard<std::mutex> lock(m_error_mutex);
        handler = m_error_handler;
    }
    if (handler) {
        try {
            handler(context, e);
        } catch (...) {
            LOG_ERROR("Error handler threw an exception");
        }
    }
}
