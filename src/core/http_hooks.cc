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
#include "millennium/encode.h"
#include "millennium/env.h"
#include "millennium/init.h"
#include "millennium/mime_types.h"
#include "millennium/urlp.h"
#include "millennium/virtfs.h"
#include "millennium/types.h"

#include <chrono>
#include <nlohmann/json_fwd.hpp>
#include <thread>
#include <unordered_set>

std::atomic<unsigned long long> g_hookedModuleId{ 0 };

json make_headers(const std::vector<std::pair<std::string, std::string>>& headers)
{
    json j_headers = json::array();
    for (const auto& header : headers) {
        j_headers.push_back({
            { "name",  header.first  },
            { "value", header.second }
        });
    }
    return j_headers;
}

void network_hook_ctl::set_hook_list(std::shared_ptr<std::vector<hook_t>> hookList)
{
    std::unique_lock<std::shared_mutex> lock(m_hook_list_mtx);
    m_hook_list_ptr = hookList;
}

std::vector<network_hook_ctl::hook_t> network_hook_ctl::get_hook_list() const
{
    std::shared_lock<std::shared_mutex> lock(m_hook_list_mtx);
    return m_hook_list_ptr ? *m_hook_list_ptr : std::vector<hook_t>();
}

void network_hook_ctl::add_hook(const hook_t& hook)
{
    std::unique_lock<std::shared_mutex> lock(m_hook_list_mtx);
    if (m_hook_list_ptr) {
        m_hook_list_ptr->push_back(hook);
    }
}

bool network_hook_ctl::remove_hook(unsigned long long moduleId)
{
    std::unique_lock<std::shared_mutex> lock(m_hook_list_mtx);
    if (!m_hook_list_ptr) return false;

    size_t originalSize = m_hook_list_ptr->size();
    auto newEnd = std::remove_if(m_hook_list_ptr->begin(), m_hook_list_ptr->end(), [moduleId](const hook_t& hook) { return hook.id == moduleId; });
    m_hook_list_ptr->erase(newEnd, m_hook_list_ptr->end());
    return m_hook_list_ptr->size() < originalSize;
}

bool network_hook_ctl::is_vfs_request(const nlohmann::basic_json<>& message)
{
    return std::any_of(std::begin(m_ftpUrls), std::end(m_ftpUrls), [&](const auto& url)
    {
        /** check if requested url is a virtual fetch */
        return message["request"]["url"].get<std::string>().find(url) != std::string::npos;
    });
}

bool network_hook_ctl::is_ipc_request(const nlohmann::basic_json<>& message)
{
    return message["request"]["url"].get<std::string>().find(this->m_ipc_url) != std::string::npos;
}

std::filesystem::path network_hook_ctl::path_from_url(const std::string& requestUrl)
{
    std::string_view url(requestUrl);

    for (std::string_view addr : m_ftpUrls) {
        if (auto pos = url.find(addr); pos != std::string_view::npos) {
            std::string cleaned = std::string(url.substr(0, pos)) + std::string(url.substr(pos + addr.size()));
            return PathFromUrl(cleaned);
        }
    }
    return PathFromUrl(requestUrl);
}

void network_hook_ctl::vfs_request_handler(const nlohmann::basic_json<>& message)
{
    std::string fileContent;
    mime::file_type fileType = mime::file_type::UNKNOWN;

    http_code responseCode = http_code::OK;
    std::string responseMessage = "OK millennium";

    const std::string strRequestFile = message["request"]["url"];

    /** Handle internal virtual FS request (pull virtfs from memory) */
    auto it = INTERNAL_FTP_CALL_DATA.find(strRequestFile);
    if (it != INTERNAL_FTP_CALL_DATA.end()) {
        fileType = mime::file_type::JS;
        fileContent = Base64Encode(it->second());

    }
    /** Handle normal disk request */
    else {
        std::filesystem::path localFilePath = this->path_from_url(strRequestFile);
        std::ifstream localFileStream(localFilePath);

        bool bFailedRead = !localFileStream.is_open();
        if (bFailedRead) {
            responseCode = http_code::NOT_FOUND;
            responseMessage = "millennium couldn't read " + localFilePath.string();
            LOG_ERROR("failed to retrieve file '{}' info from disk.", localFilePath.string());
        }

        fileType = mime::get_file_type(localFilePath.string());

        if (is_bin_file(fileType)) {
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

    std::vector<std::pair<std::string, std::string>> defaultHeaders = {
        { "Access-Control-Allow-Origin", "*"                                 },
        { "Content-Type",                mime::get_mime_str(fileType).data() }
    };

    json headers = make_headers(defaultHeaders);

    const json params = {
        { "responseCode",    responseCode         },
        { "requestId",       message["requestId"] },
        { "responseHeaders", headers              },
        { "responsePhrase",  responseMessage      },
        { "body",            fileContent          }
    };

    cdp->send_host("Fetch.fulfillRequest", params);
}

static void enable_csp_bypass(std::string frameUrl)
{
    std::string targetId;
    const auto targets = cdp->send_host("Target.getTargets").get();

    for (auto& target : targets["targetInfos"]) {
        if (target["url"].get<std::string>() == frameUrl) {
            targetId = target["targetId"].get<std::string>();
        }
    }

    if (targetId.empty()) {
        return;
    }

    json attachParams = {
        { "targetId", targetId },
        { "flatten",  true     }
    };
    const auto attachResult = cdp->send_host("Target.attachToTarget", attachParams).get();

    json cspParams = {
        { "enabled", true }
    };
    cdp->send_host("Page.setBypassCSP", cspParams, attachResult["sessionId"]);
}

void network_hook_ctl::mime_doc_request_handler(const nlohmann::basic_json<>& message)
{
    const std::string requestId = message.value("requestId", std::string{});
    const http_code statusCode = message["responseStatusCode"].get<http_code>();
    const std::string requestUrl = message["request"]["url"].get<std::string>();

    const nlohmann::json params = {
        { "requestId", message["requestId"] }
    };

    // Check if the request URL is a do-not-hook URL.
    for (const auto& doNotHook : g_js_and_css_hook_blacklist) {
        if (std::regex_match(requestUrl, std::regex(doNotHook))) {
            cdp->send_host("Fetch.continueRequest", params);
            return;
        }
    }

    const auto redirect_codes = { http_code::SEE_OTHER, http_code::MOVED_PERMANENTLY, http_code::FOUND, http_code::TEMPORARY_REDIRECT, http_code::PERMANENT_REDIRECT };

    // If the status code is a redirect, we just continue the request.
    for (const auto& code : redirect_codes) {
        if (statusCode == code) {
            cdp->send_host("Fetch.continueRequest", params);
            return;
        }
    }

    auto response = cdp->send_host("Fetch.getResponseBody", params).get();

    std::string responseBody = response.value("body", std::string{});
    if (requestUrl.empty() || responseBody.empty()) {
        return;
    }

    const std::string patchedContent = this->patch_document(requestUrl, Base64Decode(responseBody));
    enable_csp_bypass(requestUrl);

    const std::string responseMessage = message.value("responseStatusText", std::string{ "OK" });
    nlohmann::json responseHeaders = message.value("responseHeaders", nlohmann::json::array());

    const json fullfillParams = {
        { "requestId",       requestId                                        },
        { "responseCode",    statusCode                                       },
        { "responseHeaders", responseHeaders                                  },
        { "responsePhrase",  responseMessage.empty() ? "OK" : responseMessage },
        { "body",            Base64Encode(patchedContent)                     }
    };

    cdp->send_host("Fetch.fulfillRequest", fullfillParams);
}

network_hook_ctl::processed_hooks network_hook_ctl::apply_user_webkit_hooks(const std::string& requestUrl) const
{
    processed_hooks result;
    auto hookList = get_hook_list();

    for (const auto& hookItem : hookList) {
        if (!std::regex_match(requestUrl, hookItem.url_pattern)) {
            continue;
        }

        if (hookItem.type == TagTypes::STYLESHEET) {
            result.cssContent.append(fmt::format("<link rel=\"stylesheet\" href=\"{}\">\n", UrlFromPath(m_ftp_url, hookItem.path)));
        } else if (hookItem.type == TagTypes::JAVASCRIPT) {
            auto jsPath = UrlFromPath(m_ftp_url, hookItem.path);
            result.script_modules.push_back(jsPath);
            result.linkPreloads.append(fmt::format("<link rel=\"modulepreload\" href=\"{}\" fetchpriority=\"high\">\n", jsPath));
        }
    }

    return result;
}

std::string network_hook_ctl::compile_script_module_list(const std::vector<std::string>& scriptModules) const
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

std::string network_hook_ctl::stringify_plugin_names_list() const
{
    std::string result;
    auto settingsStore = std::make_unique<SettingsStore>();

    for (const auto& plugin : settingsStore->GetEnabledPlugins()) {
        result.append(fmt::format("'{}',", plugin.pluginName));
    }

    return result;
}

std::string network_hook_ctl::compile_preload_script(const processed_hooks& hooks, const std::string& millenniumPreloadPath) const
{
    const std::string millenniumAuthToken = GetAuthToken();
    const std::string ftpPath = m_ftp_url + millenniumPreloadPath;

    std::string scriptModuleArray = compile_script_module_list(hooks.script_modules);
    std::string enabledPlugins = stringify_plugin_names_list();

    const std::string scriptContent = fmt::format("(new module.default).StartPreloader('{}', [{}], [{}]);", millenniumAuthToken, scriptModuleArray, enabledPlugins);

    std::string linkPreloads = hooks.linkPreloads;
    linkPreloads.insert(0, fmt::format("<link rel=\"modulepreload\" href=\"{}\" fetchpriority=\"high\">\n", ftpPath));

    const std::string importScript = fmt::format("import('{}').then(module => {{ {} }}).catch(error => window.location.reload())", ftpPath, scriptContent);

    return fmt::format("{}<script type=\"module\" async id=\"millennium-injected\">{}</script>\n{}", linkPreloads, importScript, hooks.cssContent);
}

bool network_hook_ctl::is_url_blacklisted(const std::string& requestUrl) const
{
    for (const auto& blacklistedUrl : g_js_hook_blacklist) {
        if (std::regex_match(requestUrl, std::regex(blacklistedUrl))) {
            return true;
        }
    }
    return false;
}

std::string network_hook_ctl::inject_into_document_head(const std::string& original, const std::string& content) const
{
    const size_t headPos = original.find("<head>");
    if (headPos == std::string::npos) {
        return original;
    }
    return original.substr(0, headPos + 6) + content + original.substr(headPos + 6);
}

std::string network_hook_ctl::patch_document(const std::string& requestUrl, const std::string& original) const
{
    std::string millenniumPreloadPath = SystemIO::GetMillenniumPreloadPath();

    processed_hooks hooks = apply_user_webkit_hooks(requestUrl);
    std::string shimContent = compile_preload_script(hooks, millenniumPreloadPath);

    // Apply blacklist filtering - remove JavaScript for blacklisted URLs
    if (is_url_blacklisted(requestUrl)) {
        shimContent = hooks.cssContent; // Keep only CSS content
    }

    return inject_into_document_head(original, shimContent);
}

void network_hook_ctl::ipc_request_handler(nlohmann::json message)
{
    std::vector<std::pair<std::string, std::string>> defaultHeaders = {
        { "Access-Control-Allow-Origin",  "*"                                                                                },
        { "Access-Control-Allow-Headers", "Origin, X-Requested-With, X-Millennium-Auth, Content-Type, Accept, Authorization" },
        { "Access-Control-Allow-Methods", "GET, POST, PUT, DELETE, OPTIONS"                                                  },
        { "Access-Control-Max-Age",       "86400"                                                                            },
        { "Content-Type",                 "application/json"                                                                 }
    };

    json headers = make_headers(defaultHeaders);

    json parameters = {
        { "responseCode",    http_code::OK        },
        { "requestId",       message["requestId"] },
        { "responseHeaders", headers              },
        { "responsePhrase",  "Millennium"         }
    };

    /** If the HTTP method is OPTIONS, we don't need to require auth token */
    if (message.value(json::json_pointer("/request/method"), std::string{}) == "OPTIONS") {
        cdp->send_host("Fetch.fulfillRequest", parameters);
        return;
    }

    std::string authToken = message.value(json::json_pointer("/request/headers/X-Millennium-Auth"), std::string{});
    if (authToken.empty() || GetAuthToken() != authToken) {
        LOG_ERROR("Invalid or missing X-Millennium-Auth in IPC request. Request: {}", message.dump());

        parameters["responseCode"] = http_code::UNAUTHORIZED;
        cdp->send_host("Fetch.fulfillRequest", parameters);
        return;
    }

    nlohmann::json postData;
    std::string strPostData = message.value(json::json_pointer("/request/postData"), std::string{});

    if (!strPostData.empty()) {
        try {
            postData = nlohmann::json::parse(strPostData);
        } catch (const nlohmann::json::parse_error& e) {
            postData = nlohmann::json::object();
        }
    } else {
        postData = nlohmann::json::object();
    }

    if (postData.is_null() || postData.empty()) {
        LOG_ERROR("IPC request with no post data, this is not allowed.");
        parameters["responseCode"] = http_code::BAD_REQUEST;
        cdp->send_host("Fetch.fulfillRequest", parameters);
        return;
    }

    const auto result = IPCMain::HandleEventMessage(postData);
    parameters["body"] = Base64Encode(result.dump());

    if (result.contains("error")) {
        LOG_ERROR("IPC error: {}", result.dump(4));

        const std::vector<std::pair<IPCMain::ErrorType, http_code>> errorMap = {
            { IPCMain::ErrorType::AUTHENTICATION_ERROR, http_code::UNAUTHORIZED          },
            { IPCMain::ErrorType::INTERNAL_ERROR,       http_code::INTERNAL_SERVER_ERROR }
        };

        parameters["responseCode"] = errorMap[result["type"]];
    }

    cdp->send_host("Fetch.fulfillRequest", parameters);
}

void network_hook_ctl::init()
{
    Logger.Log("Initializing HttpHookManager...");

    cdp->on("Fetch.requestPaused", [this](const nlohmann::json& message)
    {
        if (is_ipc_request(message)) {
            if (m_thread_pool) {
                m_thread_pool->enqueue([this, msg = std::move(message)]() { this->ipc_request_handler(std::move(msg)); });
            } else {
                LOG_ERROR("Thread pool is not initialized, cannot handle IPC message.");
            }
            return;
        }

        if (this->is_vfs_request(message)) {
            this->vfs_request_handler(message);
            return;
        }

        this->mime_doc_request_handler(message);
    });

    const auto hooks = { this->m_ipc_url, this->m_ftp_url, this->m_legacy_hook_url, this->m_legacy_virt_js_url, this->m_legacy_virt_css_url };

    nlohmann::json patterns = nlohmann::json::array();
    for (const auto& hook : hooks) {
        patterns.push_back({
            { "urlPattern", fmt::format("{}*", hook) },
            { "requestStage", "Request" }
        });
    }

    /** hook documents */
    patterns.push_back({
        { "urlPattern",   "*"        },
        { "resourceType", "Document" },
        { "requestStage", "Response" }
    });

    const auto params = nlohmann::json::object({
        { "patterns", patterns }
    });

    cdp->send_host("Fetch.enable", params);
}

void network_hook_ctl::shutdown()
{
    if (m_shutdown.exchange(true)) {
        Logger.Log("HttpHookManager::shutdown() called more than once, ignoring.");
        return;
    }
    Logger.Log("Shutting down HttpHookManager...");
    if (m_thread_pool) {
        m_thread_pool->shutdown();
    }
    Logger.Log("HttpHookManager shut down successfully.");
}

network_hook_ctl::network_hook_ctl() : m_thread_pool(std::make_unique<thread_pool>(std::thread::hardware_concurrency())), m_hook_list_ptr(std::make_shared<std::vector<hook_t>>())
{
}

network_hook_ctl::~network_hook_ctl()
{
/**
 * deconstructor's aren't used on windows as the dll loader lock causes dead locks.
 * we free from the main function instead
 * */
#if defined(__linux__) || defined(MILLENNIUM_32BIT)
    this->shutdown();
#endif
}
