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

#include "millennium/http_hooks.h"
#include "millennium/auth.h"
#include "millennium/core_ipc.h"
#include "millennium/csp_bypass.h"
#include "millennium/encode.h"
#include "millennium/init.h"
#include "millennium/mime_types.h"
#include "millennium/urlp.h"
#include "millennium/virtfs.h"

#include <chrono>
#include <thread>
#include <unordered_set>
#include <utility>
#include <nlohmann/json.hpp>

using namespace nlohmann;

std::atomic<unsigned long long> g_hookedModuleId{ 0 };

// Millennium will not load JavaScript into the following URLs to favor user safety.
// This is a list of URLs that may have sensitive information or are not safe to load JavaScript into.
static const std::vector<std::string> g_blackListedUrls = { "https://checkout\\.steampowered\\.com/.*" };

// clang-format off
// Millennium will not hook the following URLs to favor user safety. (Neither JavaScript nor CSS will be injected into these URLs.)
static const std::unordered_set<std::string> g_doNotHook = {
    /** Ignore paypal related content */
    R"(https?:\/\/(?:[\w-]+\.)*paypal\.com\/[^\s"']*)", 
    R"(https?:\/\/(?:[\w-]+\.)*paypalobjects\.com\/[^\s"']*)", 
    R"(https?:\/\/(?:[\w-]+\.)*recaptcha\.net\/[^\s"']*)",

    /** Ignore youtube related content */
    R"(https?://(?:[\w-]+\.)*(?:youtube(?:-nocookie)?|youtu|ytimg|googlevideo|googleusercontent|studioyoutube)\.com/[^\s"']*)",
    R"(https?://(?:[\w-]+\.)*youtu\.be/[^\s"']*)"
};
// clang-format on

// Thread-safe hook list operations
void HttpHookManager::SetHookList(const std::shared_ptr<std::vector<HookType>>& hookList)
{
    std::unique_lock<std::shared_mutex> lock(m_hookListMutex);
    m_hookListPtr = hookList;
}

std::vector<HttpHookManager::HookType> HttpHookManager::GetHookListCopy() const
{
    std::shared_lock<std::shared_mutex> lock(m_hookListMutex);
    return m_hookListPtr ? *m_hookListPtr : std::vector<HookType>();
}

void HttpHookManager::AddHook(const HookType& hook) const
{
    std::unique_lock<std::shared_mutex> lock(m_hookListMutex);
    if (m_hookListPtr) {
        m_hookListPtr->push_back(hook);
    }
}

bool HttpHookManager::RemoveHook(const unsigned long long hookId) const
{
    std::unique_lock<std::shared_mutex> lock(m_hookListMutex);

    if (!m_hookListPtr) {
        return false; // Nothing to remove
    }

    const size_t originalSize = m_hookListPtr->size();

    const auto newEnd = std::ranges::remove_if(*m_hookListPtr, [hookId](const HookType& hook) { return hook.id == hookId; }).begin();
    m_hookListPtr->erase(newEnd, m_hookListPtr->end());

    return m_hookListPtr->size() < originalSize; // Return true if something was removed
}

// Thread-safe request management
void HttpHookManager::AddRequest(const WebHookItem& request) const
{
    std::unique_lock<std::shared_mutex> lock(m_requestMapMutex);
    m_requestMap->push_back(request);
}

template <typename Func> requires IteratorBoolable<Func, std::vector<HttpHookManager::WebHookItem>::iterator>
void HttpHookManager::ProcessRequests(Func processor)
{
    std::unique_lock<std::shared_mutex> lock(m_requestMapMutex);
    for (auto it = m_requestMap->begin(); it != m_requestMap->end();) {
        if (processor(it)) {
            it = m_requestMap->erase(it);
        } else {
            ++it;
        }
    }
}

// Thread-safe socket communication
void HttpHookManager::PostGlobalMessage(const nlohmann::json& message) const
{
    std::lock_guard<std::mutex> lock(m_socketMutex);
    Sockets::PostGlobal(message);
}

// Exception throttling
bool HttpHookManager::ShouldLogException()
{
    std::lock_guard<std::mutex> lock(m_exceptionTimeMutex);
    const auto now = std::chrono::system_clock::now();
    if (now - m_lastExceptionTime > std::chrono::seconds(5)) {
        m_lastExceptionTime = now;
        return true;
    }
    return false;
}

void HttpHookManager::SetupGlobalHooks()
{
    PostGlobalMessage({
        { "id",     3242                                                                                                    },
        { "method", "Fetch.enable"                                                                                          },
        { "params",
         { { "patterns",
              { { { "urlPattern", "*" }, { "resourceType", "Document" }, { "requestStage", "Response" } },
                { { "urlPattern", fmt::format("{}*", this->m_ipcHookAddress) }, { "requestStage", "Request" } },
                { { "urlPattern", fmt::format("{}*", this->m_ftpHookAddress) }, { "requestStage", "Request" } },
                /** Maintain backwards compatibility for themes that explicitly rely on this url */
                { { "urlPattern", fmt::format("{}*", this->m_oldHookAddress) }, { "requestStage", "Request" } },
                { { "urlPattern", fmt::format("{}*", this->m_javaScriptVirtualUrl) }, { "requestStage", "Request" } },
                { { "urlPattern", fmt::format("{}*", this->m_styleSheetVirtualUrl) }, { "requestStage", "Request" } } } } } }
    });
}

bool HttpHookManager::IsGetBodyCall(const nlohmann::basic_json<>& message) const
{
    const std::string requestUrl = message["params"]["request"]["url"].get<std::string>();

    return requestUrl.find(this->m_javaScriptVirtualUrl) != std::string::npos || requestUrl.find(this->m_styleSheetVirtualUrl) != std::string::npos ||
           requestUrl.find(this->m_oldHookAddress) != std::string::npos || requestUrl.find(this->m_ftpHookAddress) != std::string::npos;
}

bool HttpHookManager::IsIpcCall(const nlohmann::basic_json<>& message) const
{
    const std::string requestUrl = message["params"]["request"]["url"].get<std::string>();
    return requestUrl.find(this->m_ipcHookAddress) != std::string::npos;
}

std::filesystem::path HttpHookManager::ConvertToLoopBack(const std::string& requestUrl) const
{
    std::string url = requestUrl;
    const std::initializer_list<const char*> addresses = { this->m_ftpHookAddress, this->m_javaScriptVirtualUrl, this->m_styleSheetVirtualUrl, this->m_oldHookAddress };

    for (std::string addr : addresses) {
        const size_t pos = url.find(addr);
        if (pos != std::string::npos) {
            url.erase(pos, addr.length());
            break;
        }
    }

    return std::filesystem::path(PathFromUrl(url));
}

void HttpHookManager::RetrieveRequestFromDisk(const basic_json<>& message) const
{
    std::string fileContent;
    eFileType fileType = unknown;

    uint16_t responseCode = 200;
    std::string responseMessage = "OK millennium";

    const std::string strRequestFile = message["params"]["request"]["url"];

    /** Handle internal virtual FS request (pull virtfs from memory) */
    const auto it = INTERNAL_FTP_CALL_DATA.find(strRequestFile);
    if (it != INTERNAL_FTP_CALL_DATA.end()) {
        fileType = js;
        fileContent = Base64Encode(it->second());

    }
    /** Handle normal disk request */
    else {
        std::filesystem::path localFilePath = this->ConvertToLoopBack(strRequestFile);
        std::ifstream localFileStream(localFilePath);

        bool bFailedRead = !localFileStream.is_open();
        if (bFailedRead) {
            responseCode = 404;
            responseMessage = "millennium couldn't read " + localFilePath.string();
            LOG_ERROR("failed to retrieve file '{}' info from disk.", localFilePath.string());
        }

        fileType = EvaluateFileType(localFilePath.string());

        if (IsBinaryFile(fileType)) {
            try {
                fileContent = Base64Encode(SystemIO::ReadFileBytesSync(localFilePath.string()));
            } catch (const std::exception& error) {
                LOG_ERROR("Failed to read file bytes from disk: {}", error.what());
                bFailedRead = true; /** Force fail even if the file exists. */
            }
        } else {
            fileContent = Base64Encode(std::string(std::istreambuf_iterator<char>(localFileStream), std::istreambuf_iterator<char>()));
        }
    }

    const auto responseHeaders = nlohmann::json::array({
        { { "name", "Access-Control-Allow-Origin" }, { "value", "*" }                 },
        { { "name", "Content-Type" },                { "value", fileTypes[fileType] } }
    });

    PostGlobalMessage({
        { "id",     63453                  },
        { "method", "Fetch.fulfillRequest" },
        { "params",
         { { "responseCode", responseCode },
            { "requestId", message["params"]["requestId"] },
            { "responseHeaders", responseHeaders },
            { "responsePhrase", responseMessage },
            { "body", fileContent } }      }
    });
}

void HttpHookManager::GetResponseBody(const nlohmann::basic_json<>& message)
{
    const RedirectType statusCode = message["params"]["responseStatusCode"].get<RedirectType>();
    const std::string requestUrl = message["params"]["request"]["url"].get<std::string>();

    const auto ContinueOriginalRequest = [this, &message]()
    {
        PostGlobalMessage({
            { "id",     0                                                   },
            { "method", "Fetch.continueRequest"                             },
            { "params", { { "requestId", message["params"]["requestId"] } } }
        });
    };

    // Check if the request URL is a do-not-hook URL.
    for (const auto& doNotHook : g_doNotHook) {
        if (std::regex_match(requestUrl, std::regex(doNotHook))) {
            ContinueOriginalRequest();
            return;
        }
    }

    // If the status code is a redirect, we just continue the request.
    if (statusCode == REDIRECT || statusCode == MOVED_PERMANENTLY || statusCode == FOUND || statusCode == TEMPORARY_REDIRECT || statusCode == PERMANENT_REDIRECT) {
        ContinueOriginalRequest();
    } else {
        long long currentMessageId = hookMessageId.fetch_sub(1) - 1;

        const WebHookItem item = { currentMessageId, message.value("/params/requestId"_json_pointer, std::string{}), message.value("/params/resourceType"_json_pointer, std::string{}),
                             message };

        AddRequest(item);

        PostGlobalMessage({
            { "id",     currentMessageId                                    },
            { "method", "Fetch.getResponseBody"                             },
            { "params", { { "requestId", message["params"]["requestId"] } } }
        });
    }
}

HttpHookManager::ProcessedHooks HttpHookManager::ProcessWebkitHooks(const std::string& requestUrl) const
{
    ProcessedHooks result;
    const auto hookList = GetHookListCopy();

    for (const auto& hookItem : hookList) {
        if (!std::regex_match(requestUrl, hookItem.urlPattern)) {
            continue;
        }

        if (hookItem.type == TagTypes::STYLESHEET) {
            result.cssContent.append(fmt::format("<link rel=\"stylesheet\" href=\"{}\">\n", UrlFromPath(m_ftpHookAddress, hookItem.path)));
        } else if (hookItem.type == TagTypes::JAVASCRIPT) {
            auto jsPath = UrlFromPath(m_ftpHookAddress, hookItem.path);
            result.scriptModules.push_back(jsPath);
            result.linkPreloads.append(fmt::format("<link rel=\"modulepreload\" href=\"{}\" fetchpriority=\"high\">\n", jsPath));
        }
    }

    return result;
}

std::string HttpHookManager::BuildScriptModuleArray(const std::vector<std::string>& scriptModules)
{
    std::string result;
    for (size_t i = 0; i < scriptModules.size(); ++i) {
        if (i > 0) {
            result.append(",");
        }
        result.append(fmt::format("\"{}\"", scriptModules[i]));
    }
    return result;
}

std::string HttpHookManager::BuildEnabledPluginsString()
{
    std::string result;
    const auto settingsStore = std::make_unique<SettingsStore>();

    for (const auto& plugin : settingsStore->GetEnabledPlugins()) {
        result.append(fmt::format("'{}',", plugin.pluginName));
    }

    return result;
}

std::string HttpHookManager::CreateShimContent(const ProcessedHooks& hooks, const std::string& millenniumPreloadPath) const
{
    const std::string millenniumAuthToken = GetAuthToken();
    const std::string ftpPath = m_ftpHookAddress + millenniumPreloadPath;

    std::string scriptModuleArray = BuildScriptModuleArray(hooks.scriptModules);
    std::string enabledPlugins = BuildEnabledPluginsString();

    const std::string scriptContent = fmt::format("(new module.default).StartPreloader('{}', [{}], [{}]);", millenniumAuthToken, scriptModuleArray, enabledPlugins);

    std::string linkPreloads = hooks.linkPreloads;
    linkPreloads.insert(0, fmt::format("<link rel=\"modulepreload\" href=\"{}\" fetchpriority=\"high\">\n", ftpPath));

    const std::string importScript = fmt::format("import('{}').then(module => {{ {} }}).catch(error => window.location.reload())", ftpPath, scriptContent);

    return fmt::format("{}<script type=\"module\" async id=\"millennium-injected\">{}</script>\n{}", linkPreloads, importScript, hooks.cssContent);
}

bool HttpHookManager::IsUrlBlacklisted(const std::string& requestUrl)
{
    for (const auto& blacklistedUrl : g_blackListedUrls) {
        if (std::regex_match(requestUrl, std::regex(blacklistedUrl))) {
            return true;
        }
    }
    return false;
}

std::string HttpHookManager::InjectContentIntoHead(const std::string& original, const std::string& content)
{
    const size_t headPos = original.find("<head>");
    if (headPos == std::string::npos) {
        return original;
    }
    return original.substr(0, headPos + 6) + content + original.substr(headPos + 6);
}

std::string HttpHookManager::PatchDocumentContents(const std::string& requestUrl, const std::string& original) const
{
    const std::string millenniumPreloadPath = SystemIO::GetMillenniumPreloadPath();

    const ProcessedHooks hooks = ProcessWebkitHooks(requestUrl);
    std::string shimContent = CreateShimContent(hooks, millenniumPreloadPath);

    // Apply blacklist filtering - remove JavaScript for blacklisted URLs
    if (IsUrlBlacklisted(requestUrl)) {
        shimContent = hooks.cssContent; // Keep only CSS content
    }

    return InjectContentIntoHead(original, shimContent);
}

void HttpHookManager::HandleHooks(const nlohmann::basic_json<>& message)
{
    ProcessRequests([&](auto requestIterator) -> bool
    {
        try {
            auto [id, requestId, type, response] = (*requestIterator);

            int64_t messageId = message.value(json::json_pointer("/id"), static_cast<int64_t>(0));
            const bool base64Encoded = message.value(json::json_pointer("/result/base64Encoded"), false);

            if (messageId != id || (messageId == id && !base64Encoded)) {
                return false;
            }

            const std::string requestUrl = response.value(json::json_pointer("/params/request/url"), std::string{});
            const std::string responseBody = message.value(json::json_pointer("/result/body"), std::string{});

            if (requestUrl.empty() || responseBody.empty()) {
                return true;
            }

            const std::string patchedContent = this->PatchDocumentContents(requestUrl, Base64Decode(responseBody));
            BypassCSP();

            const int responseCode = response.value(json::json_pointer("/params/responseStatusCode"), 200);
            const std::string responseMessage = response.value(json::json_pointer("/params/responseStatusText"), std::string{ "OK" });
            nlohmann::json responseHeaders = response.value(json::json_pointer("/params/responseHeaders"), nlohmann::json::array());

            PostGlobalMessage({
                { "id",     63453                              },
                { "method", "Fetch.fulfillRequest"             },
                { "params",
                 { { "requestId", requestId },
                    { "responseCode", responseCode },
                    { "responseHeaders", responseHeaders },
                    { "responsePhrase", responseMessage.empty() ? "OK" : responseMessage },
                    { "body", Base64Encode(patchedContent) } } }
            });
            return true;
        } catch (const nlohmann::detail::exception& ex) {
            if (ShouldLogException()) LOG_ERROR("JSON error in HandleHooks -> {}", ex.what());
            return true;
        } catch (const std::exception& ex) {
            if (ShouldLogException()) LOG_ERROR("Error in HandleHooks -> {}", ex.what());
            return true;
        }
    });
}

void HttpHookManager::HandleIpcMessage(nlohmann::json message) const
{
    nlohmann::json headers = nlohmann::json::array({
        { { "name", "Access-Control-Allow-Origin" },  { "value", "*" }                                                                                },
        { { "name", "Access-Control-Allow-Headers" }, { "value", "Origin, X-Requested-With, X-Millennium-Auth, Content-Type, Accept, Authorization" } },
        { { "name", "Access-Control-Allow-Methods" }, { "value", "GET, POST, PUT, DELETE, OPTIONS" }                                                  },
        { { "name", "Access-Control-Max-Age" },       { "value", "86400" }                                                                            },
        { { "name", "Content-Type" },                 { "value", "application/json" }                                                                 }
    });

    nlohmann::json responseJson = {
        { "id",     63453                                                                                                                                            },
        { "method", "Fetch.fulfillRequest"                                                                                                                           },
        { "params", { { "responseCode", 200 }, { "requestId", message["params"]["requestId"] }, { "responseHeaders", headers }, { "responsePhrase", "Millennium" } } }
    };

    /** If the HTTP method is OPTIONS, we don't need to require auth token */
    if (message.value(json::json_pointer("/params/request/method"), std::string{}) == "OPTIONS") {
        this->PostGlobalMessage(responseJson);
        return;
    }

    const std::string authToken = message.value(json::json_pointer("/params/request/headers/X-Millennium-Auth"), std::string{});
    if (authToken.empty() || GetAuthToken() != authToken) {
        LOG_ERROR("Invalid or missing X-Millennium-Auth in IPC request. Request: {}", message.dump());
        responseJson["params"]["responseCode"] = 401; // Unauthorized
        this->PostGlobalMessage(responseJson);
        return;
    }

    nlohmann::json postData;
    const std::string strPostData = message.value(json::json_pointer("/params/request/postData"), std::string{});

    if (!strPostData.empty()) {
        try {
            postData = nlohmann::json::parse(strPostData);
        } catch ([[maybe_unused]] const nlohmann::json::parse_error& e) {
            postData = nlohmann::json::object();
        }
    } else {
        postData = nlohmann::json::object();
    }

    if (postData.is_null() || postData.empty()) {
        LOG_ERROR("IPC request with no post data, this is not allowed.");
        responseJson["params"]["responseCode"] = 400;
        this->PostGlobalMessage(responseJson);
        return;
    }

    const auto result = IPCMain::HandleEventMessage(postData);
    responseJson["params"]["body"] = Base64Encode(result.dump());

    if (result.contains("error")) {
        LOG_ERROR("IPC error: {}", result.dump(4));

        if (result["type"] == IPCMain::ErrorType::AUTHENTICATION_ERROR) {
            responseJson["params"]["responseCode"] = 401;
        } else if (result["type"] == IPCMain::ErrorType::INTERNAL_ERROR) {
            responseJson["params"]["responseCode"] = 500;
        }
    }

    this->PostGlobalMessage(responseJson);
}

void HttpHookManager::DispatchSocketMessage(nlohmann::basic_json<> message)
{
    try {
        if (message["method"] == "Fetch.requestPaused") {
            if (IsIpcCall(message)) {
                if (m_threadPool) {
                    m_threadPool->enqueue([this, msg = std::move(message)]() { this->HandleIpcMessage(std::move(msg)); });
                } else {
                    LOG_ERROR("Thread pool is not initialized, cannot handle IPC message.");
                }
                return;
            }

            if (this->IsGetBodyCall(message)) {
                this->RetrieveRequestFromDisk(message);
            } else {
                this->GetResponseBody(message);
            }
        }
        this->HandleHooks(message);
    } catch (const detail::exception& ex) {
        if (ShouldLogException()) {
            LOG_ERROR("error dispatching socket message -> {}", ex.what());
        }
    } catch (const std::exception& ex) {
        if (ShouldLogException()) {
            LOG_ERROR("error dispatching socket message -> {}", ex.what());
        }
    }
}

HttpHookManager::ThreadPool::ThreadPool(const size_t numThreads) : stop(false), shutdownCalled(false)
{
    for (size_t i = 0; i < numThreads; ++i) {
        workers.emplace_back([this]
        {
            while (true) {
                std::function<void()> task;
                {
                    std::unique_lock<std::mutex> lock(queueMutex);
                    condition.wait(lock, [this] { return stop || !tasks.empty(); });
                    if (stop && tasks.empty()) {
                        Logger.Log("ThreadPool worker exiting");
                        return;
                    }
                    if (!tasks.empty()) {
                        task = std::move(tasks.front());
                        tasks.pop();
                    } else {
                        continue;
                    }
                }
                try {
                    task();
                } catch (const std::exception& ex) {
                    Logger.Log(std::string("ThreadPool worker exception: ") + ex.what());
                }
            }
        });
    }
}

void HttpHookManager::Shutdown()
{
    if (m_shutdown.exchange(true)) {
        Logger.Log("HttpHookManager::shutdown() called more than once, ignoring.");
        return;
    }
    Logger.Log("Shutting down HttpHookManager...");
    if (m_threadPool) {
        m_threadPool->shutdown();
    }
    Logger.Log("HttpHookManager shut down successfully.");
}

void HttpHookManager::ThreadPool::shutdown()
{
    if (shutdownCalled.exchange(true)) {
        Logger.Log("ThreadPool::shutdown() called more than once, ignoring.");
        return;
    }
    Logger.Log("Shutting down thread pool...");
    {
        std::unique_lock<std::mutex> lock(queueMutex);
        stop = true;
        // Clear any remaining tasks to avoid executing after shutdown
        while (!tasks.empty())
            tasks.pop();
    }
    condition.notify_all();
    for (std::thread& worker : workers) {
        if (worker.joinable()) {
            Logger.Log("Joining thread pool worker...");
            worker.join();
        }
    }
    Logger.Log("Thread pool shut down successfully.");
}

template <typename F> void HttpHookManager::ThreadPool::enqueue(F&& f)
{
    if (stop || shutdownCalled) {
        Logger.Log("enqueue() called after shutdown, ignoring task.");
        return;
    }
    {
        std::unique_lock<std::mutex> lock(queueMutex);
        if (stop) return; // Don't accept new tasks if shutting down
        tasks.emplace(std::forward<F>(f));
    }
    condition.notify_one();
}

HttpHookManager::HttpHookManager()
    : m_threadPool(std::make_unique<ThreadPool>(1)), m_lastExceptionTime{}, m_hookListPtr(std::make_shared<std::vector<HookType>>()),
      m_requestMap(std::make_shared<std::vector<WebHookItem>>())
{
}

HttpHookManager::~HttpHookManager()
{
    /**
     * deconstructor's aren't used on windows as the dll loader lock causes dead locks.
     * we free from the main function instead
     * */
#if defined(__linux__) || defined(MILLENNIUM_32BIT)
    this->Shutdown();
#endif
}
