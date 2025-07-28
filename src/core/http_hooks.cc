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

#include "http_hooks.h"
#include <nlohmann/json.hpp>
#include "loader.h"
#include "ffi.h"
#include "encoding.h"
#include "http.h"
#include <unordered_set>
#include "csp_bypass.h"
#include "url_parser.h"
#include "env.h"
#include <secure_socket.h>
#include "ipc.h"
#include <thread>
#include <chrono>
#include <ranges>

using namespace nlohmann;

/**
 * Enum representing the different types of files.
 * 
 * @enum {number}
 * @readonly
 * @property {number} StyleSheet - Represents a CSS file.
 * @property {number} JavaScript - Represents a JavaScript file.
 * @property {number} Json - Represents a JSON file.
 * @property {number} Python - Represents a Python file.
 * @property {number} Other - Represents other file types.
 */
enum eFileType
{
    css,
    js,
    json,
    py,
    ttf,
    otf,
    woff,
    woff2,
    png,
    jpeg,
    jpg,
    gif,
    webp,
    svg,
    html,
    unknown,
};

/**
 * A map that associates each file type from the `eFileType` enum to its corresponding MIME type.
 * 
 * - `StyleSheet` maps to "text/css"
 * - `JavaScript` maps to "application/javascript"
 * - `Json` maps to "application/json"
 * - `Python` maps to "text/x-python"
 * - `Other` maps to "text/plain"
 */
static std::map<eFileType, std::string> fileTypes 
{
    { eFileType::css,     "text/css"               },
    { eFileType::js,      "application/javascript" },
    { eFileType::json,    "application/json"       },
    { eFileType::py,      "text/x-python"          },
    { eFileType::ttf,     "font/ttf"               },
    { eFileType::otf,     "font/otf"               },
    { eFileType::woff,    "font/woff"              },
    { eFileType::woff2,   "font/woff2"             },
    { eFileType::png,     "image/png"              },
    { eFileType::jpeg,    "image/jpeg"             },
    { eFileType::jpg,     "image/jpg"              },
    { eFileType::gif,     "image/gif"              },
    { eFileType::webp,    "image/webp"             },
    { eFileType::svg,     "image/svg+xml"          },
    { eFileType::html,    "text/html"              },
    { eFileType::unknown, "text/plain"             },
};

/**
 * Checks if the file type is a binary file.
 * 
 * @param {eFileType} fileType - The type of the file to check.
 * @returns {boolean} - `true` if the file type is binary, `false` otherwise.
 */
static constexpr bool IsBinaryFile(eFileType fileType)
{
    return fileType == eFileType::ttf 
        || fileType == eFileType::otf 
        || fileType == eFileType::woff 
        || fileType == eFileType::woff2 
        || fileType == eFileType::gif 
        || fileType == eFileType::png 
        || fileType == eFileType::jpeg 
        || fileType == eFileType::jpg 
        || fileType == eFileType::webp 
        || fileType == eFileType::svg 
        || fileType == eFileType::html 
        || fileType == eFileType::unknown;
}

/**
 * Evaluates the file type based on the file extension.
 */
static const eFileType EvaluateFileType(std::filesystem::path filePath)
{
    const std::string extension = filePath.extension().string();

    if      (extension == ".css"  ) { return eFileType::css;   }
    else if (extension == ".js"   ) { return eFileType::js;    }
    else if (extension == ".json" ) { return eFileType::json;  }
    else if (extension == ".py"   ) { return eFileType::py;    }
    else if (extension == ".ttf"  ) { return eFileType::ttf;   }
    else if (extension == ".otf"  ) { return eFileType::otf;   }
    else if (extension == ".woff" ) { return eFileType::woff;  }
    else if (extension == ".woff2") { return eFileType::woff2; }
    else if (extension == ".gif"  ) { return eFileType::gif;   }
    else if (extension == ".png"  ) { return eFileType::png;   }
    else if (extension == ".jpeg" ) { return eFileType::jpeg;  }
    else if (extension == ".jpg"  ) { return eFileType::jpg;   }
    else if (extension == ".webp" ) { return eFileType::webp;  }
    else if (extension == ".svg"  ) { return eFileType::svg;   }
    else if (extension == ".html" ) { return eFileType::html;  }
    
    else
    {
        return eFileType::unknown;
    }
}

std::atomic<unsigned long long> g_hookedModuleId{0};

// Millennium will not load JavaScript into the following URLs to favor user safety.
// This is a list of URLs that may have sensitive information or are not safe to load JavaScript into.
static const std::vector<std::string> g_blackListedUrls = {
    "https://checkout\\.steampowered\\.com/.*"
};

// Millennium will not hook the following URLs to favor user safety. (Neither JavaScript nor CSS will be injected into these URLs.)
static const std::unordered_set<std::string> g_doNotHook = {
    R"(https?:\/\/(?:[\w-]+\.)*paypal\.com\/[^\s"']*)",
    R"(https?:\/\/(?:[\w-]+\.)*paypalobjects\.com\/[^\s"']*)",
    R"(https?:\/\/(?:[\w-]+\.)*recaptcha\.net\/[^\s"']*)",
};

// Thread-safe singleton implementation
HttpHookManager& HttpHookManager::get() 
{
    static HttpHookManager instance;
    return instance;
}

// Thread-safe hook list operations
void HttpHookManager::SetHookList(std::shared_ptr<std::vector<HookType>> hookList)
{
    std::unique_lock<std::shared_mutex> lock(m_hookListMutex);
    m_hookListPtr = hookList;
}

std::vector<HttpHookManager::HookType> HttpHookManager::GetHookListCopy() const
{
    std::shared_lock<std::shared_mutex> lock(m_hookListMutex);
    return m_hookListPtr ? *m_hookListPtr : std::vector<HookType>();
}

void HttpHookManager::AddHook(const HookType& hook)
{
    std::unique_lock<std::shared_mutex> lock(m_hookListMutex);
    if (m_hookListPtr) {
        m_hookListPtr->push_back(hook);
    }
}

bool HttpHookManager::RemoveHook(unsigned long long moduleId)
{
    std::unique_lock<std::shared_mutex> lock(m_hookListMutex);
    
    if (!m_hookListPtr) {
        return false; // Nothing to remove
    }
    
    size_t originalSize = m_hookListPtr->size();
    
    auto newEnd = std::remove_if(m_hookListPtr->begin(), m_hookListPtr->end(),
        [moduleId](const HookType& hook) {
            return hook.id == moduleId;
        });
    
    m_hookListPtr->erase(newEnd, m_hookListPtr->end());
    
    return m_hookListPtr->size() < originalSize; // Return true if something was removed
}

// Thread-safe request management
void HttpHookManager::AddRequest(const WebHookItem& request)
{
    std::unique_lock<std::shared_mutex> lock(m_requestMapMutex);
    m_requestMap->push_back(request);
}

template<typename Func>
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
void HttpHookManager::PostGlobalMessage(const nlohmann::json& message)
{
    std::lock_guard<std::mutex> lock(m_socketMutex);
    Sockets::PostGlobal(message);
}

// Exception throttling
bool HttpHookManager::ShouldLogException()
{
    std::lock_guard<std::mutex> lock(m_exceptionTimeMutex);
    auto now = std::chrono::system_clock::now();
    if (now - m_lastExceptionTime > std::chrono::seconds(5)) {
        m_lastExceptionTime = now;
        return true;
    }
    return false;
}

void HttpHookManager::SetupGlobalHooks() 
{
    PostGlobalMessage({
        { "id", 3242 }, { "method", "Fetch.enable" },
        { "params", {
            { "patterns", {
                { { "urlPattern", "*" }, { "resourceType", "Document" }, { "requestStage", "Response" } },     
                { { "urlPattern", fmt::format("{}*", this->m_ipcHookAddress      ) }, { "requestStage", "Request" } },
                { { "urlPattern", fmt::format("{}*", this->m_ftpHookAddress      ) }, { "requestStage", "Request" } },
                /** Maintain backwards compatibility for themes that explicitly rely on this url */
                { { "urlPattern", fmt::format("{}*", this->m_oldHookAddress      ) }, { "requestStage", "Request" } },
                { { "urlPattern", fmt::format("{}*", this->m_javaScriptVirtualUrl) }, { "requestStage", "Request" } },
                { { "urlPattern", fmt::format("{}*", this->m_styleSheetVirtualUrl) }, { "requestStage", "Request" } }
            }
        }}}
    });
}

bool HttpHookManager::IsGetBodyCall(const nlohmann::basic_json<>& message) 
{
    std::string requestUrl = message["params"]["request"]["url"].get<std::string>();

    return requestUrl.find(this->m_javaScriptVirtualUrl) != std::string::npos
        || requestUrl.find(this->m_styleSheetVirtualUrl) != std::string::npos
        || requestUrl.find(this->m_oldHookAddress)       != std::string::npos
        || requestUrl.find(this->m_ftpHookAddress)       != std::string::npos;
}

bool HttpHookManager::IsIpcCall(const nlohmann::basic_json<>& message) 
{
    std::string requestUrl = message["params"]["request"]["url"].get<std::string>();
    return requestUrl.find(this->m_ipcHookAddress) != std::string::npos;
}

std::filesystem::path HttpHookManager::ConvertToLoopBack(const std::string& requestUrl)
{
    std::string url = requestUrl;
    
    size_t newPos  = url.find(this->m_ftpHookAddress);
    size_t jsPos   = url.find(this->m_javaScriptVirtualUrl);
    size_t cssPos  = url.find(this->m_styleSheetVirtualUrl);
    size_t oldPos  = url.find(this->m_oldHookAddress);
 
    if      (newPos != std::string::npos) url.erase(newPos, std::string(this->m_ftpHookAddress).length());
    else if (jsPos != std::string::npos ) url.erase(jsPos, std::string(this->m_javaScriptVirtualUrl).length());
    else if (cssPos != std::string::npos) url.erase(cssPos, std::string(this->m_styleSheetVirtualUrl).length());
    else if (oldPos != std::string::npos) url.erase(oldPos, std::string(this->m_oldHookAddress).length());

    return std::filesystem::path(PathFromUrl(url));
}

void HttpHookManager::RetrieveRequestFromDisk(const nlohmann::basic_json<>& message)
{
    std::string fileContent;
    std::filesystem::path localFilePath = this->ConvertToLoopBack(message["params"]["request"]["url"]);
    std::ifstream localFileStream(localFilePath);

    bool bFailedRead = !localFileStream.is_open();
    if (bFailedRead)
    {
        LOG_ERROR("failed to retrieve file '{}' info from disk.", localFilePath.string());
    }

    uint16_t    responseCode    = bFailedRead ? 404 : 200;
    std::string responseMessage = bFailedRead ? "millennium couldn't read " + localFilePath.string() : "OK millennium";
    eFileType   fileType        = EvaluateFileType(localFilePath.string());

    if (IsBinaryFile(fileType)) 
    {
        try
        {
            fileContent = Base64Encode(SystemIO::ReadFileBytesSync(localFilePath.string()));
        }
        catch(const std::exception& error)
        {
            LOG_ERROR("Failed to read file bytes from disk: {}", error.what());
            bFailedRead = true; /** Force fail even if the file exists. */
        }       
    } 
    else 
    {
        fileContent = Base64Encode(std::string(
            std::istreambuf_iterator<char>(localFileStream),
            std::istreambuf_iterator<char>()
        ));
    }

    const auto responseHeaders = nlohmann::json::array
    ({
        { {"name", "Access-Control-Allow-Origin"}, {"value", "*"} },
        { {"name", "Content-Type"}, {"value", fileTypes[fileType]} }
    });

    PostGlobalMessage({
        { "id", 63453 },
        { "method", "Fetch.fulfillRequest" },
        { "params", {
            { "responseCode", responseCode },
            { "requestId", message["params"]["requestId"] },
            { "responseHeaders", responseHeaders },
            { "responsePhrase", responseMessage },
            { "body", fileContent }
        }}
    });
}

void HttpHookManager::GetResponseBody(const nlohmann::basic_json<>& message)
{
    const RedirectType statusCode = message["params"]["responseStatusCode"].get<RedirectType>();

    const auto ContinueOriginalRequest = [this, &message]() {
        PostGlobalMessage({
            { "id", 0 },
            { "method", "Fetch.continueRequest" },
            { "params", { { "requestId", message["params"]["requestId"] } }}
        });
    };

    // Check if the request URL is a do-not-hook URL.
    for (const auto& doNotHook : g_doNotHook) 
    {
        if (std::regex_match(message["params"]["request"]["url"].get<std::string>(), std::regex(doNotHook))) 
        {
            ContinueOriginalRequest();
            return;
        }
    }

    // If the status code is a redirect, we just continue the request. 
    if (statusCode == REDIRECT || statusCode == MOVED_PERMANENTLY || statusCode == FOUND || 
        statusCode == TEMPORARY_REDIRECT || statusCode == PERMANENT_REDIRECT)
    {
        ContinueOriginalRequest();
    }
    else
    {
        long long currentMessageId = hookMessageId.fetch_sub(1) - 1;
        
        WebHookItem item = {
            currentMessageId,
            message.value("/params/requestId"_json_pointer, std::string{}),
            message.value("/params/resourceType"_json_pointer, std::string{}), 
            message
        };
        
        AddRequest(item);

        PostGlobalMessage({
            { "id", currentMessageId },
            { "method", "Fetch.getResponseBody" },
            { "params", { { "requestId", message["params"]["requestId"] } }}
        });
    }
}

const std::string HttpHookManager::PatchDocumentContents(const std::string& requestUrl, const std::string& original) 
{
    std::string patched = original;
    std::optional<std::string> millenniumPreloadPath = SystemIO::GetMillenniumPreloadPath();

    if (!millenniumPreloadPath.has_value()) 
    {
        LOG_ERROR("Missing webkit preload module. Please re-install Millennium.");
        #ifdef _WIN32
        MessageBoxA(NULL, "Missing webkit preload module. Please re-install Millennium.", "Millennium", MB_ICONERROR);
        #endif
        return patched;
    }

    // Get thread-safe copy of hook list
    auto hookList = GetHookListCopy();
    
    std::vector<std::string> scriptModules;
    std::string cssShimContent, scriptModuleArray;
    std::string linkPreloadsArray;

    for (const auto& hookItem : hookList) 
    {
        if (hookItem.type == TagTypes::STYLESHEET) 
        {
            if (!std::regex_match(requestUrl, hookItem.urlPattern)) 
                continue;

            cssShimContent.append(fmt::format("<link rel=\"stylesheet\" href=\"{}\">\n", UrlFromPath(m_ftpHookAddress, hookItem.path))); 
        }
        else if (hookItem.type == TagTypes::JAVASCRIPT) 
        {
            if (!std::regex_match(requestUrl, hookItem.urlPattern)) 
                continue;

            auto jsPath = UrlFromPath(this->m_ftpHookAddress, hookItem.path);
            scriptModules.push_back(jsPath);
            linkPreloadsArray.append(fmt::format("<link rel=\"modulepreload\" href=\"{}\" fetchpriority=\"high\">\n", jsPath));
        }
    }

    for (size_t i = 0; i < scriptModules.size(); i++)
    {
        scriptModuleArray.append(fmt::format("\"{}\"{}", scriptModules[i], (i == scriptModules.size() - 1 ? "" : ",")));
    }

    std::string strEnabledPlugins;
    std::unique_ptr<SettingsStore> settingsStore = std::make_unique<SettingsStore>();
    const auto& plugins = settingsStore->GetEnabledPlugins();

    for (size_t i = 0; i < plugins.size(); ++i)
    {
        strEnabledPlugins.append(fmt::format("'{}',", plugins[i].pluginName));
    }

    const std::string millenniumAuthToken = GetAuthToken();
    const std::string ftpPath = UrlFromPath(m_ftpHookAddress, millenniumPreloadPath.value_or(std::string()));
    const std::string scriptContent = fmt::format("(new module.default).StartPreloader('{}', [{}], [{}]);", millenniumAuthToken, scriptModuleArray, strEnabledPlugins);

    linkPreloadsArray.insert(0, fmt::format("<link rel=\"modulepreload\" href=\"{}\" fetchpriority=\"high\">\n", ftpPath));

    std::string importScript = fmt::format("import('{}').then(module => {{ {} }}).catch(error => window.location.reload())", ftpPath, scriptContent);
    std::string shimContent = fmt::format("{}<script type=\"module\" async id=\"millennium-injected\">{}</script>\n{}", linkPreloadsArray, importScript, cssShimContent);

    for (const auto& blackListedUrl : g_blackListedUrls)        
    {
        if (std::regex_match(requestUrl, std::regex(blackListedUrl))) 
        {
            shimContent = cssShimContent; // Remove all queried JavaScript from the page. 
        }
    }

    if (patched.find("<head>") == std::string::npos) 
    {
        return patched;
    }

    return patched.replace(patched.find("<head>"), 6, "<head>" + shimContent);
}

void HttpHookManager::HandleHooks(const nlohmann::basic_json<>& message)
{
    ProcessRequests([&](auto requestIterator) -> bool
    {
        try
        {
            auto [id, requestId, type, response] = (*requestIterator);
           
            int64_t messageId = message.value(json::json_pointer("/id"), int64_t(0));
            bool base64Encoded = message.value(json::json_pointer("/result/base64Encoded"), false);
           
            if (messageId != id || (messageId == id && !base64Encoded)) {
                return false; 
            }
           
            std::string requestUrl = response.value(json::json_pointer("/params/request/url"), std::string{});
            std::string responseBody = message.value(json::json_pointer("/result/body"), std::string{});
           
            if (requestUrl.empty() || responseBody.empty()) {
                return true; 
            }
           
            const std::string patchedContent = this->PatchDocumentContents(requestUrl, Base64Decode(responseBody));
            BypassCSP();
           
            const int responseCode = response.value(json::json_pointer("/params/responseStatusCode"), 200);
            const std::string responseMessage = response.value(json::json_pointer("/params/responseStatusText"), std::string{"OK"});
            nlohmann::json responseHeaders = response.value(json::json_pointer("/params/responseHeaders"), nlohmann::json::array());
           
            PostGlobalMessage({
                { "id", 63453 },
                { "method", "Fetch.fulfillRequest" },
                { "params", {
                    { "requestId", requestId },
                    { "responseCode", responseCode },
                    { "responseHeaders", responseHeaders },
                    { "responsePhrase", responseMessage.empty() ? "OK" : responseMessage },
                    { "body", Base64Encode(patchedContent) }
                }}
            });
            return true;
        }
        catch (const nlohmann::detail::exception& ex)
        {
            if (ShouldLogException()) LOG_ERROR("JSON error in HandleHooks -> {}", ex.what());        
            return true; 
        }
        catch (const std::exception& ex)
        {
            if (ShouldLogException()) LOG_ERROR("Error in HandleHooks -> {}", ex.what());
            return true; 
        }
    });
}

void HttpHookManager::HandleIpcMessage(nlohmann::json message)
{
    nlohmann::json responseJson = {
        { "id", 63453 },
        { "method", "Fetch.fulfillRequest" },
        { "params", {
            { "responseCode", 200 },
            { "requestId", message["params"]["requestId"] },
            { "responseHeaders", nlohmann::json::array({ 
                { {"name", "Access-Control-Allow-Origin"},  {"value", "*"} },
                { {"name", "Access-Control-Allow-Headers"}, {"value", "Origin, X-Requested-With, X-Millennium-Auth, Content-Type, Accept, Authorization"} },
                { {"name", "Access-Control-Allow-Methods"}, {"value", "GET, POST, PUT, DELETE, OPTIONS"} },
                { {"name", "Access-Control-Max-Age"}, {"value", "86400"} },
                { {"name", "Content-Type"}, {"value", "application/json"} }
            }) },
            { "responsePhrase", "Millennium" }
        }}
    };

    /** If the HTTP method is OPTIONS, we don't need to require auth token */
    if (message.value(json::json_pointer("/params/request/method"), std::string{}) == "OPTIONS")
    {
        this->PostGlobalMessage(responseJson);
        return;
    }

    std::string authToken = message.value(json::json_pointer("/params/request/headers/X-Millennium-Auth"), std::string{});
    if (authToken.empty() || GetAuthToken() != authToken) 
    {
        LOG_ERROR("Invalid or missing X-Millennium-Auth in IPC request. Request: {}", message.dump());
        responseJson["params"]["responseCode"] = 401; // Unauthorized
        this->PostGlobalMessage(responseJson);
        return;
    }

    nlohmann::json postData;
    std::string strPostData = message.value(json::json_pointer("/params/request/postData"), std::string{});

    if (!strPostData.empty()) 
    {
        try 
        {
            postData = nlohmann::json::parse(strPostData);
        } 
        catch (const nlohmann::json::parse_error& e) 
        {
            postData = nlohmann::json::object();
        }
    } 
    else 
    {
        postData = nlohmann::json::object();
    }

    if (postData.is_null() || postData.empty())
    {
        LOG_ERROR("IPC request with no post data, this is not allowed.");
        responseJson["params"]["responseCode"] = 400;
        this->PostGlobalMessage(responseJson);
        return;
    }
    
    const auto result = IPCMain::HandleEventMessage(postData);
    responseJson["params"]["body"] = Base64Encode(result.dump());

    if (result.contains("error"))
    {
        LOG_ERROR("IPC error: {}", result.dump(4));

        if (result["type"] == IPCMain::ErrorType::AUTHENTICATION_ERROR)
        {
            responseJson["params"]["responseCode"] = 401;
        }
        else if (result["type"] == IPCMain::ErrorType::INTERNAL_ERROR)
        {
            responseJson["params"]["responseCode"] = 500; 
        }
    }

    this->PostGlobalMessage(responseJson);
}

void HttpHookManager::DispatchSocketMessage(nlohmann::basic_json<> message)
{
    try 
    {
        if (message["method"] == "Fetch.requestPaused")
        {
            if (IsIpcCall(message)) 
            {
                if (m_threadPool) {
                    m_threadPool->enqueue([this, msg = std::move(message)]() {
                        this->HandleIpcMessage(std::move(msg));
                    });
                }
                else 
                {
                    LOG_ERROR("Thread pool is not initialized, cannot handle IPC message.");
                }
                return;
            }

            switch ((int)this->IsGetBodyCall(message))
            {
                case true:  { this->RetrieveRequestFromDisk(message); break; }
                case false: { this->GetResponseBody(message);         break; }
            }
        }

        this->HandleHooks(message);
    }
    catch (const nlohmann::detail::exception& ex) 
    {
        if (ShouldLogException()) 
        {
            LOG_ERROR("error dispatching socket message -> {}", ex.what());
        }
    }
    catch (const std::exception& ex) 
    {
        if (ShouldLogException()) 
        {
            LOG_ERROR("error dispatching socket message -> {}", ex.what());
        }
    }
}

HttpHookManager::ThreadPool::ThreadPool(size_t numThreads) 
{
    for (size_t i = 0; i < numThreads; ++i) 
    {
        workers.emplace_back([this] 
        {
            while (true) 
            {
                std::function<void()> task;
                {
                    std::unique_lock<std::mutex> lock(queueMutex);
                    condition.wait(lock, [this] { return stop || !tasks.empty(); });
                    
                    if (stop && tasks.empty()) return;
                    
                    task = std::move(tasks.front());
                    tasks.pop();
                }
                task();
            }
        });
    }
}

HttpHookManager::ThreadPool::~ThreadPool() 
{
    shutdown();
}

void HttpHookManager::ThreadPool::shutdown() 
{
    {
        std::unique_lock<std::mutex> lock(queueMutex);
        stop = true;
    }
    condition.notify_all();
    
    for (std::thread& worker : workers) 
    {
        if (worker.joinable()) 
        {
            worker.join();
        }
    }
}

template<typename F>
void HttpHookManager::ThreadPool::enqueue(F&& f) 
{
    {
        std::unique_lock<std::mutex> lock(queueMutex);
        if (stop) return; // Don't accept new tasks if shutting down
        tasks.emplace(std::forward<F>(f));
    }
    condition.notify_one();
}

HttpHookManager::HttpHookManager() : m_hookListPtr(std::make_shared<std::vector<HookType>>()), m_requestMap(std::make_shared<std::vector<WebHookItem>>()), m_lastExceptionTime{}, m_threadPool(std::make_unique<ThreadPool>(1))
{ }

HttpHookManager::~HttpHookManager() 
{ }
