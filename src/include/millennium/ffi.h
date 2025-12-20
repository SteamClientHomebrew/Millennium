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
#include "millennium/init.h"
#include "millennium/logger.h"

#include <nlohmann/json.hpp>
#include <vector>
#include <shared_mutex>
#include <mutex>
#include <deque>
#include <chrono>
#include <atomic>
#include <condition_variable>
#include <unordered_map>
#include <thread>

#include <Python.h>
#include <lua.hpp>

class PythonGIL : public std::enable_shared_from_this<PythonGIL>
{
  private:
    PyGILState_STATE m_interpreterGIL{};
    PyThreadState* m_interpreterThreadState = nullptr;
    PyInterpreterState* m_mainInterpreter = nullptr;

  public:
    void HoldAndLockGIL();
    void HoldAndLockGILOnThread(PyThreadState* threadState);
    void ReleaseAndUnLockGIL();

    PythonGIL();
    ~PythonGIL();
};

enum FFI_Type
{
    Boolean,
    String,
    JSON,
    Integer,
    Error,
    UnknownType // non-atomic ADT's
};

static const std::unordered_map<nlohmann::json::value_t, FFI_Type> FFIMap_t = {
    { nlohmann::json::value_t::boolean,         FFI_Type::Boolean },
    { nlohmann::json::value_t::string,          FFI_Type::String  },
    { nlohmann::json::value_t::number_integer,  FFI_Type::Integer },
    { nlohmann::json::value_t::number_unsigned, FFI_Type::Integer },
    { nlohmann::json::value_t::object,          FFI_Type::JSON    },
    { nlohmann::json::value_t::array,           FFI_Type::JSON    }
};

struct EvalResult
{
    FFI_Type type;
    std::string plain;
};

namespace Lua
{
EvalResult LockAndInvokeMethod(std::string pluginName, nlohmann::json script);
void CallFrontEndLoaded(std::string pluginName);
} // namespace Lua

namespace Python
{
EvalResult LockGILAndInvokeMethod(std::string pluginName, nlohmann::json script);

std::tuple<std::string, std::string> ActiveExceptionInformation();

EvalResult LockGILAndInvokeMethod(std::string pluginName, nlohmann::json script);
void CallFrontEndLoaded(std::string pluginName);
} // namespace Python

struct JsEvalResult
{
    nlohmann::basic_json<> json;
    bool successfulCall;
};

namespace JavaScript
{

enum Types
{
    Boolean,
    String,
    Integer
};

struct JsFunctionConstructTypes
{
    std::string pluginName;
    Types type;
};

JsEvalResult ExecuteOnSharedJsContext(std::string javaScriptEval);
const std::string ConstructFunctionCall(const char* value, const char* methodName, std::vector<JavaScript::JsFunctionConstructTypes> params);

int Lua_EvaluateFromSocket(std::string script, lua_State* L);
PyObject* Py_EvaluateFromSocket(std::string script);
} // namespace JavaScript

using EventHandler = std::function<void(const nlohmann::json&, const std::string&)>;

#pragma once
#include <unordered_map>
#include <vector>
#include <mutex>
#include <shared_mutex>
#include <string>
#include <functional>
#include <memory>
#include <atomic>
#include <deque>
#include <chrono>
#include <thread>
#include <condition_variable>
#include <nlohmann/json.hpp>

using EventHandler = std::function<void(const nlohmann::json&, const std::string&)>;

class CefSocketDispatcher : public Singleton<CefSocketDispatcher>
{
    friend class Singleton<CefSocketDispatcher>;

  private:
    struct ListenerInfo
    {
        std::string name;
        EventHandler handler;
        std::atomic<bool> active{ true };
        uint64_t lastProcessedMessageId{ 0 };

        ListenerInfo(std::string n, EventHandler h) : name(std::move(n)), handler(std::move(h))
        {
        }
    };

    using ListenerPtr = std::shared_ptr<ListenerInfo>;

    struct Message
    {
        nlohmann::json data;
        std::chrono::steady_clock::time_point timestamp;
        uint64_t id;
        std::string event;
        ListenerPtr deliveryListener;

        Message(std::string e, nlohmann::json d, uint64_t msgId) : data(std::move(d)), timestamp(std::chrono::steady_clock::now()), id(msgId), event(std::move(e))
        {
        }
    };

    using MessagePtr = std::shared_ptr<Message>;

    CefSocketDispatcher() : workerRunning(true)
    {
        workerThread = std::thread([this]() { this->workerLoop(); });
    }

    ~CefSocketDispatcher()
    {
/**
 * deconstructor's aren't used on windows as the dll loader lock causes dead locks.
 * we free from the main function instead
 * */
#if defined(__linux__) || defined(MILLENNIUM_32BIT)
        this->Shutdown();
#endif
    }

    std::unordered_map<std::string, std::vector<ListenerPtr>> eventListeners;
    mutable std::shared_mutex listenersMtx;

    std::unordered_map<std::string, std::deque<MessagePtr>> messageHistory;
    mutable std::mutex historyMtx;

    std::deque<MessagePtr> pendingMessages;
    mutable std::mutex pendingMtx;
    std::condition_variable pendingCV;

    std::thread workerThread;
    std::atomic<bool> workerRunning{ true };

    std::atomic<uint64_t> messageCounter{ 0 };

    size_t maxHistoryPerEvent{ 1000 };
    std::chrono::minutes maxMessageAge{ 60 };

    void workerLoop()
    {
        while (workerRunning.load()) {
            std::vector<MessagePtr> messagesToProcess;

            {
                std::unique_lock<std::mutex> lock(pendingMtx);
                pendingCV.wait(lock, [this] { return !pendingMessages.empty() || !workerRunning.load(); });

                if (!workerRunning.load() && pendingMessages.empty()) {
                    break;
                }

                while (!pendingMessages.empty()) {
                    messagesToProcess.push_back(std::move(pendingMessages.front()));
                    pendingMessages.pop_front();
                }
            }

            for (const auto& message : messagesToProcess) {
                processMessage(message);
            }
        }
    }

    void processMessage(const MessagePtr& message)
    {
        if (message->event.compare(0, 18, "__DELIVER_HISTORY__") == 0) {
            std::string realEvent = message->event.substr(18);
            if (message->deliveryListener && message->deliveryListener->active.load()) {
                deliverHistoricalMessages(realEvent, message->deliveryListener);
            }
            return;
        }

        {
            std::lock_guard<std::mutex> lock(historyMtx);
            messageHistory[message->event].push_back(message);

            if (message->id % 100 == 0) {
                cleanupOldMessages();
            }
        }

        std::vector<ListenerPtr> currentListeners;
        {
            std::shared_lock<std::shared_mutex> lock(listenersMtx);
            auto eventIt = eventListeners.find(message->event);
            if (eventIt != eventListeners.end()) {
                currentListeners = eventIt->second;
            }
        }

        for (const auto& listener : currentListeners) {
            safeInvokeHandler(listener, message);
        }
    }

    void deliverHistoricalMessages(const std::string& event, ListenerPtr listener)
    {
        std::vector<MessagePtr> messagesToDeliver;

        {
            std::lock_guard<std::mutex> lock(historyMtx);
            auto historyIt = messageHistory.find(event);
            if (historyIt != messageHistory.end()) {
                for (const auto& msg : historyIt->second) {
                    if (msg->id > listener->lastProcessedMessageId) {
                        messagesToDeliver.push_back(msg);
                    }
                }
            }
        }

        for (const auto& msg : messagesToDeliver) {
            if (!listener->active.load()) {
                break;
            }
            safeInvokeHandler(listener, msg);
        }
    }

    void safeInvokeHandler(const ListenerPtr& listener, const MessagePtr& message)
    {
        if (!listener->active.load() || message->id <= listener->lastProcessedMessageId) {
            return;
        }

        try {
            listener->handler(message->data, listener->name);
            listener->lastProcessedMessageId = message->id;
        } catch (...) {
        }
    }

    void cleanupOldMessages()
    {
        auto now = std::chrono::steady_clock::now();

        for (auto& [event, messages] : messageHistory) {
            while (!messages.empty() && (now - messages.front()->timestamp) > maxMessageAge) {
                messages.pop_front();
            }

            while (messages.size() > maxHistoryPerEvent) {
                messages.pop_front();
            }
        }
    }

  public:

    void SetMaxHistoryPerEvent(size_t maxHistory)
    {
        maxHistoryPerEvent = maxHistory;
    }

    void SetMaxMessageAge(std::chrono::minutes maxAge)
    {
        maxMessageAge = maxAge;
    }

    std::string OnMessage(const std::string& event, const std::string& name, EventHandler handler)
    {
        auto listener = std::make_shared<ListenerInfo>(name, std::move(handler));

        {
            std::unique_lock<std::shared_mutex> lock(listenersMtx);
            eventListeners[event].push_back(listener);
        }

        auto deliveryTask = std::make_shared<Message>("__DELIVER_HISTORY__" + event,
                                                      nlohmann::json{
                                                          { "listener_name", name }
        },
                                                      ++messageCounter);
        deliveryTask->deliveryListener = listener;

        {
            std::lock_guard<std::mutex> lock(pendingMtx);
            pendingMessages.push_front(deliveryTask);
        }
        pendingCV.notify_one();

        return name;
    }

    void RemoveListener(const std::string& event, const std::string& listenerId)
    {
        std::unique_lock<std::shared_mutex> lock(listenersMtx);

        auto eventIt = eventListeners.find(event);
        if (eventIt != eventListeners.end()) {
            auto& listeners = eventIt->second;

            listeners.erase(std::remove_if(listeners.begin(), listeners.end(),
                                           [&listenerId](const ListenerPtr& listener)
            {
                if (listener->name == listenerId) {
                    listener->active.store(false);
                    return true;
                }
                return false;
            }),
                            listeners.end());

            if (listeners.empty()) {
                eventListeners.erase(eventIt);
            }
        }
    }

    void EmitMessage(const std::string& event, const nlohmann::json& data)
    {
        uint64_t msgId = ++messageCounter;
        auto message = std::make_shared<Message>(event, data, msgId);

        {
            std::lock_guard<std::mutex> lock(pendingMtx);
            pendingMessages.push_back(message);
        }
        pendingCV.notify_one();
    }

    void Shutdown()
    {
        Logger.Log("Shutting down CefSocketDispatcher...");
        g_shouldTerminateMillennium->flag.store(true);

        if (workerRunning.load()) {
            workerRunning.store(false);
            pendingCV.notify_all();

            if (workerThread.joinable()) {
                workerThread.join();
            }
        }

        Logger.Log("CefSocketDispatcher shut down complete.");
    }

    void CleanupHistory()
    {
        std::lock_guard<std::mutex> lock(historyMtx);
        cleanupOldMessages();
    }

    size_t GetStoredMessageCount(const std::string& event) const
    {
        std::lock_guard<std::mutex> lock(historyMtx);
        auto it = messageHistory.find(event);
        return (it != messageHistory.end()) ? it->second.size() : 0;
    }

    size_t GetPendingMessageCount() const
    {
        std::lock_guard<std::mutex> lock(pendingMtx);
        return pendingMessages.size();
    }

    size_t GetTotalStoredMessages() const
    {
        std::lock_guard<std::mutex> lock(historyMtx);
        size_t total = 0;
        for (const auto& [event, messages] : messageHistory) {
            total += messages.size();
        }
        return total;
    }

    size_t GetListenerCount(const std::string& event) const
    {
        std::shared_lock<std::shared_mutex> lock(listenersMtx);
        auto it = eventListeners.find(event);
        return (it != eventListeners.end()) ? it->second.size() : 0;
    }

    std::vector<std::string> GetActiveEvents() const
    {
        std::shared_lock<std::shared_mutex> lock(listenersMtx);
        std::vector<std::string> events;
        events.reserve(eventListeners.size());

        for (const auto& pair : eventListeners) {
            if (!pair.second.empty()) {
                events.push_back(pair.first);
            }
        }

        return events;
    }

    std::vector<std::string> GetEventsWithHistory() const
    {
        std::lock_guard<std::mutex> lock(historyMtx);
        std::vector<std::string> events;
        events.reserve(messageHistory.size());

        for (const auto& pair : messageHistory) {
            if (!pair.second.empty()) {
                events.push_back(pair.first);
            }
        }

        return events;
    }
};
