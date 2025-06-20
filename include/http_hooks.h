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
#include <thread>
#include <queue>
#include <condition_variable>
#include <mutex>
#include <shared_mutex>
#include <atomic>
#include <functional>
#include <memory>
#include <vector>
#include <string>
#include <regex>
#include <filesystem>
#include <chrono>
#include <nlohmann/json.hpp>

extern std::atomic<unsigned long long> g_hookedModuleId;

class HttpHookManager
{
public:
    static HttpHookManager& get();
    
    enum TagTypes {
        STYLESHEET,
        JAVASCRIPT
    };
    
    struct HookType {
        std::string path;
        std::regex urlPattern;
        TagTypes type;
        unsigned long long id;
    };
    
    enum RedirectType {
        REDIRECT = 301,
        MOVED_PERMANENTLY = 302,
        FOUND = 303,
        TEMPORARY_REDIRECT = 307,
        PERMANENT_REDIRECT = 308
    };

    void DispatchSocketMessage(nlohmann::basic_json<> message);
    void SetupGlobalHooks();
    void AddHook(const HookType& hook);
    bool RemoveHook(unsigned long long hookId);

    // Thread-safe hook list operations
    void SetHookList(std::shared_ptr<std::vector<HookType>> hookList);
    std::vector<HookType> GetHookListCopy() const;

    // Delete copy constructor and assignment operator for singleton
    HttpHookManager(const HttpHookManager&) = delete;
    HttpHookManager& operator=(const HttpHookManager&) = delete;

private:
    HttpHookManager();
    ~HttpHookManager();

    class ThreadPool {
    public:
        ThreadPool(size_t numThreads = 4);
        ~ThreadPool();
        
        template<typename F>
        void enqueue(F&& f);
        void shutdown();
        
    private:
        std::vector<std::thread> workers;
        std::queue<std::function<void()>> tasks;
        std::mutex queueMutex;
        std::condition_variable condition;
        std::atomic<bool> stop{false};
    };
    
    std::unique_ptr<ThreadPool> m_threadPool;
    
    // Thread synchronization
    mutable std::shared_mutex m_hookListMutex;
    mutable std::shared_mutex m_requestMapMutex;
    mutable std::mutex m_socketMutex;
    mutable std::mutex m_configMutex;
    mutable std::mutex m_exceptionTimeMutex;
    

    std::atomic<long long> hookMessageId{-69};
    
    // Exception throttling
    std::chrono::time_point<std::chrono::system_clock> m_lastExceptionTime;
    
    const char* m_ftpHookAddress       = "https://millennium.ftp/";
    const char* m_ipcHookAddress       = "https://millennium.ipc/";

    /** Maintain backwards compatibility for themes that explicitly rely on this url */
    const char* m_oldHookAddress       = "https://pseudo.millennium.app/";
    const char* m_javaScriptVirtualUrl = "https://js.millennium.app/";
    const char* m_styleSheetVirtualUrl = "https://css.millennium.app/";
    
    // Protected data structures
    std::shared_ptr<std::vector<HookType>> m_hookListPtr;
    
    struct WebHookItem {
        long long id;
        std::string requestId;
        std::string type;
        nlohmann::basic_json<> message;
    };
    std::shared_ptr<std::vector<WebHookItem>> m_requestMap;
    
    // Private methods
    bool IsIpcCall(const nlohmann::basic_json<>& message);
    bool IsGetBodyCall(const nlohmann::basic_json<>& message);
    std::string HandleCssHook(const std::string& body);
    std::string HandleJsHook(const std::string& body);
    const std::string PatchDocumentContents(const std::string& requestUrl, const std::string& original);
    void HandleHooks(const nlohmann::basic_json<>& message);
    void RetrieveRequestFromDisk(const nlohmann::basic_json<>& message);
    void GetResponseBody(const nlohmann::basic_json<>& message);
    void HandleIpcMessage(nlohmann::json message);
    std::filesystem::path ConvertToLoopBack(const std::string& requestUrl);
    
    // Thread-safe utilities
    void PostGlobalMessage(const nlohmann::json& message);
    bool ShouldLogException();
    void AddRequest(const WebHookItem& request);
    template<typename Func>
    void ProcessRequests(Func processor);
};