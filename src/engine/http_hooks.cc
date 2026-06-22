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

#include "millennium/filesystem.h"
#include "millennium/http_hooks.h"
#include "millennium/auth.h"
#include "millennium/core_ipc.h"
#include "millennium/encoding.h"
#include "millennium/logger.h"
#include "millennium/mime_types.h"
#include "millennium/url_parser.h"
#include "millennium/virtfs.h"
#include "millennium/types.h"

#include <nlohmann/json_fwd.hpp>
#include <thread>
#include <unordered_set>

std::atomic<unsigned long long> g_hookedModuleId{ 0 };

// clang-format off
const std::vector<std::regex> g_js_hook_blacklist = {
    std::regex(R"(https?://checkout\.steampowered\.com/[^\s"']*)"),
};

const std::vector<std::regex> g_js_and_css_hook_blacklist = {
    std::regex(R"(https?://(?:[\w-]+\.)*paypal\.com/[^\s"']*)"),
    std::regex(R"(https?://(?:[\w-]+\.)*paypalobjects\.com/[^\s"']*)"),
    std::regex(R"(https?://(?:[\w-]+\.)*recaptcha\.net/[^\s"']*)"),
    std::regex(R"(https?://(?:[\w-]+\.)*(?:youtube(?:-nocookie)?|youtu|ytimg|googlevideo|googleusercontent|studioyoutube)\.com/[^\s"']*)"),
    std::regex(R"(https?://(?:[\w-]+\.)*youtu\.be/[^\s"']*)"),
    std::regex(R"(https?://(?:[\w-]+\.)*chromewebstore\.google\.com/[^\s"']*)"),
};
// clang-format on

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

std::vector<network_hook_ctl::hook_item> network_hook_ctl::get_hook_list() const
{
    std::shared_lock<std::shared_mutex> lock(m_hook_list_mtx);
    return m_hook_list_ptr ? *m_hook_list_ptr : std::vector<hook_item>();
}

unsigned long long network_hook_ctl::add_hook(const hook_t& hook)
{
    std::unique_lock<std::shared_mutex> lock(m_hook_list_mtx);
    hook_item item{ hook, g_hookedModuleId.fetch_add(1) };

    if (m_hook_list_ptr) {
        m_hook_list_ptr->push_back(item);
    }

    return item.id;
}

bool network_hook_ctl::remove_hook(unsigned long long moduleId)
{
    std::unique_lock<std::shared_mutex> lock(m_hook_list_mtx);
    if (!m_hook_list_ptr) return false;

    size_t originalSize = m_hook_list_ptr->size();
    auto newEnd = std::remove_if(m_hook_list_ptr->begin(), m_hook_list_ptr->end(), [moduleId](const hook_item& hook)
    {
        return hook.id == moduleId;
    });
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

std::filesystem::path network_hook_ctl::path_from_url(const std::string& requestUrl)
{
    std::string_view url(requestUrl);

    std::string_view themes_prefix(m_themes_url);
    if (auto pos = url.find(themes_prefix); pos != std::string_view::npos) {
        std::string relativePath = std::string(url.substr(pos + themes_prefix.size()));
        if (auto q = relativePath.find('?'); q != std::string::npos) {
            relativePath = relativePath.substr(0, q);
        }
        return platform::get_millennium_path() / "themes" / utils::url::decode_url(relativePath);
    }

    for (std::string_view addr : m_ftpUrls) {
        if (auto pos = url.find(addr); pos != std::string_view::npos) {
            std::string cleaned = std::string(url.substr(0, pos)) + std::string(url.substr(pos + addr.size()));
            return utils::url::get_path_from_url(cleaned);
        }
    }
    return utils::url::get_path_from_url(requestUrl);
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
        }

        fileType = mime::get_file_type(localFilePath.string());
        if (fileType == mime::file_type::UNKNOWN) {
            const std::filesystem::path requestPath = this->path_from_url(strRequestFile);
            fileType = mime::get_file_type(requestPath);
        }

        if (is_bin_file(fileType)) {
            try {
                fileContent = Base64Encode(platform::read_file_bytes(localFilePath.string()));
            } catch (const std::exception& error) {
                LOG_ERROR("Failed to read file bytes from disk: {}", error.what());
                bFailedRead = true; /** force fail even if the file exists. */
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

    m_cdp->send_host("Fetch.fulfillRequest", params);
}

void network_hook_ctl::mime_doc_request_handler(const nlohmann::basic_json<>& message)
{
    const std::string requestId = message.value("requestId", std::string{});
    const http_code statusCode = message["responseStatusCode"].get<http_code>();
    const std::string requestUrl = message["request"]["url"].get<std::string>();

    const nlohmann::json params = {
        { "requestId", message["requestId"] }
    };

    /** check if the request URL is a do-not-hook URL. */
    for (const auto& pattern : g_js_and_css_hook_blacklist) {
        if (std::regex_match(requestUrl, pattern)) {
            m_cdp->send_host("Fetch.continueResponse", params);
            return;
        }
    }

    const auto redirect_codes = { http_code::SEE_OTHER, http_code::MOVED_PERMANENTLY, http_code::FOUND, http_code::TEMPORARY_REDIRECT, http_code::PERMANENT_REDIRECT };

    for (const auto& code : redirect_codes) {
        if (statusCode == code) {
            m_cdp->send_host("Fetch.continueResponse", params);
            return;
        }
    }

    const processed_hooks hooks = apply_user_webkit_hooks(requestUrl);
    if (hooks.empty()) {
        m_cdp->send_host("Fetch.continueResponse", params);
        return;
    }

    auto response = m_cdp->send_host("Fetch.getResponseBody", params).get();

    std::string responseBody = response.value("body", std::string{});
    const bool isBase64 = response.value("base64Encoded", false);
    const std::string decodedBody = isBase64 ? Base64Decode(responseBody) : responseBody;

    if (requestUrl.empty() || decodedBody.empty()) {
        m_cdp->send_host("Fetch.continueResponse", params);
        return;
    }

    const std::string patchedContent = inject_into_document_head(decodedBody, hooks.preloads() + hooks.css() + hooks.scripts());
    const std::string responseMessage = message.value("responseStatusText", std::string{ "OK" });
    nlohmann::json responseHeaders = message.value("responseHeaders", nlohmann::json::array());

    const json fullfillParams = {
        { "requestId",       requestId                                        },
        { "responseCode",    statusCode                                       },
        { "responseHeaders", responseHeaders                                  },
        { "responsePhrase",  responseMessage.empty() ? "OK" : responseMessage },
        { "body",            Base64Encode(patchedContent)                     }
    };

    m_cdp->send_host("Fetch.fulfillRequest", fullfillParams);
}

network_hook_ctl::processed_hooks network_hook_ctl::apply_user_webkit_hooks(const std::string& requestUrl) const
{
    processed_hooks result;
    auto hookList = get_hook_list();
    bool anyHookMatched = false;

    for (const auto& hook : hookList) {
        if (!std::regex_match(requestUrl, hook.hook.url_pattern)) continue;
        anyHookMatched = true;

        if (hook.hook.type == TagTypes::STYLESHEET)
            result.add_stylesheet(m_themes_url + utils::url::encode_url(hook.hook.path));
        else if (hook.hook.type == TagTypes::JAVASCRIPT)
            result.add_script_module(m_themes_url + utils::url::encode_url(hook.hook.path));
    }

    if (anyHookMatched && m_dynamic_css_provider) {
        const auto [rootColors, sliderCss] = m_dynamic_css_provider();
        if (!rootColors.empty()) result.add_inline_style("RootColors", rootColors);
        if (!sliderCss.empty()) result.add_inline_style("MillenniumSliderConditions", sliderCss);
    }

    return result;
}

std::string network_hook_ctl::inject_into_document_head(const std::string& original, const std::string& content) const
{
    const size_t headPos = original.find("</head>");
    if (headPos == std::string::npos) {
        return original;
    }
    return original.substr(0, headPos) + content + original.substr(headPos);
}

void network_hook_ctl::set_dynamic_css_provider(std::function<std::pair<std::string, std::string>()> provider)
{
    m_dynamic_css_provider = std::move(provider);
}

void network_hook_ctl::init()
{
    m_cdp->on("Fetch.requestPaused", [this](const nlohmann::json& message)
    {
        try {
            const std::string requestUrl = message["request"]["url"].get<std::string>();

            if (this->is_vfs_request(message)) {
                this->vfs_request_handler(message);
                return;
            }

            this->mime_doc_request_handler(message);
        } catch (const std::exception& ex) {
            LOG_ERROR(ex.what());
        }
    });

    const auto hooks = { this->m_ftp_url, this->m_themes_url, this->m_legacy_hook_url, this->m_legacy_virt_js_url, this->m_legacy_virt_css_url };

    nlohmann::json patterns = nlohmann::json::array();
    for (const auto& hook : hooks) {
        patterns.push_back({
            { "urlPattern", std::format("{}*", hook) },
            { "requestStage", "Request" }
        });
    }

    patterns.push_back({
        { "urlPattern", std::format("https://{}/*", k_steam_loopback) },
        { "resourceType", "Document" },
        { "requestStage", "Response" }
    });
    for (const auto* tld : k_steam_tlds) {
        patterns.push_back({
            { "urlPattern", std::format("https://*.{}/*", tld) },
            { "resourceType", "Document" },
            { "requestStage", "Response" }
        });
        patterns.push_back({
            { "urlPattern", std::format("https://{}/*", tld) },
            { "resourceType", "Document" },
            { "requestStage", "Response" }
        });
    }

    const auto params = nlohmann::json::object({
        { "patterns", patterns }
    });

    try {
        m_cdp->send_host("Fetch.enable", params);
    } catch (const std::exception& e) {
        LOG_ERROR("Exception calling Fetch.enable in HttpHookManager::init(): {}", e.what());
    } catch (...) {
        LOG_ERROR("Unknown exception calling Fetch.enable in HttpHookManager::init()");
    }
}

void network_hook_ctl::shutdown()
{
    if (m_shutdown.exchange(true)) {
        logger.log("HttpHookManager::shutdown() called more than once, ignoring.");
        return;
    }

    if (m_thread_pool) {
        m_thread_pool->shutdown();
    }
    logger.log("Successfully shut down network_hook_ctl...");
}

network_hook_ctl::network_hook_ctl(std::shared_ptr<plugin_manager> plugin_manager)
    : m_plugin_manager(std::move(plugin_manager)), m_thread_pool(std::make_unique<thread_pool>(std::thread::hardware_concurrency())),
      m_hook_list_ptr(std::make_shared<std::vector<hook_item>>())
{
}

network_hook_ctl::~network_hook_ctl()
{
    this->shutdown();
}

void network_hook_ctl::set_cdp_client(std::shared_ptr<cdp_client> cdp)
{
    m_cdp = std::move(cdp);
}

std::string get_cdp_isolated_ctx_script()
{
    return R"(!function(){"use strict";const i="__millennium_cdp_proxy__";window.__millennium_isolated_ctx__={handleResponse:function(n,o,l){void 0!==l?window[i](JSON.stringify({action:"relay_error",callbackId:n,error:l})):window[i](JSON.stringify({action:"relay",callbackId:n,result:o}))}}}();)";
}
