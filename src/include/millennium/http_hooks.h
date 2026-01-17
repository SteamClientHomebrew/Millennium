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

#include "millennium/fwd_decl.h"
#include "millennium/cdp_api.h"
#include "millennium/thread_pool.h"
#include "millennium/sysfs.h"

#include <atomic>
#include <filesystem>
#include <memory>
#include <nlohmann/json.hpp>
#include <regex>
#include <shared_mutex>
#include <string>
#include <vector>
#include <unordered_set>

extern std::atomic<unsigned long long> g_hookedModuleId;

/**
 * Millennium will not load JavaScript into the following URLs to favor user safety.
 * This is a list of URLs that may have sensitive information or are not safe to load JavaScript into.
 */
static const std::vector<std::string> g_js_hook_blacklist = { "https://checkout\\.steampowered\\.com/.*" };

// clang-format off
/** Millennium will not hook the following URLs to favor user safety. (Neither JavaScript nor CSS will be injected into these URLs.) */
static const std::unordered_set<std::string> g_js_and_css_hook_blacklist = {
    /** Ignore paypal related content */
    R"(https?:\/\/(?:[\w-]+\.)*paypal\.com\/[^\s"']*)",
    R"(https?:\/\/(?:[\w-]+\.)*paypalobjects\.com\/[^\s"']*)",
    R"(https?:\/\/(?:[\w-]+\.)*recaptcha\.net\/[^\s"']*)",

    /** Ignore youtube related content */
    R"(https?://(?:[\w-]+\.)*(?:youtube(?:-nocookie)?|youtu|ytimg|googlevideo|googleusercontent|studioyoutube)\.com/[^\s"']*)",
    R"(https?://(?:[\w-]+\.)*youtu\.be/[^\s"']*)",
    
    /** Ignore Chrome Web Store (causes a webhelper crash on Fetch.fulfillRequest) */
    R"(https?:\/\/(?:[\w-]+\.)*chromewebstore\.google\.com\/[^\s"']*)",
};
// clang-format on

class network_hook_ctl
{
  public:
    network_hook_ctl(std::shared_ptr<SettingsStore> settings_store);
    ~network_hook_ctl();

    void set_cdp_client(std::shared_ptr<cdp_client> cdp);

    enum TagTypes
    {
        STYLESHEET,
        JAVASCRIPT
    };

    struct hook_t
    {
        std::string path;
        std::regex url_pattern;
        TagTypes type;
    };

    struct hook_item
    {
        hook_t hook;
        unsigned long long id;
    };

    void init();
    unsigned long long add_hook(const hook_t& hook);
    bool remove_hook(unsigned long long hookId);
    std::vector<hook_item> get_hook_list() const;

    void shutdown();
    const char* get_ftp_url() const
    {
        return m_ftp_url;
    }

  private:
    std::shared_ptr<SettingsStore> m_settings_store;
    std::shared_ptr<cdp_client> m_cdp;

    struct processed_hooks
    {
        std::string cssContent, linkPreloads;
        std::vector<std::string> script_modules;
    };

    std::atomic<bool> m_shutdown{ false };
    mutable std::shared_mutex m_hook_list_mtx;
    std::unique_ptr<thread_pool> m_thread_pool;
    std::shared_ptr<std::vector<hook_item>> m_hook_list_ptr;

    const char* m_ftp_url = "https://millennium.ftp/";

    /** Maintain backwards compatibility for themes that explicitly rely on this url */
    const char* m_legacy_hook_url = "https://pseudo.millennium.app/";
    const char* m_legacy_virt_js_url = "https://js.millennium.app/";
    const char* m_legacy_virt_css_url = "https://css.millennium.app/";

    /** list of all FTP related urls */
    const std::vector<const char*> m_ftpUrls = { this->m_ftp_url, this->m_legacy_virt_js_url, this->m_legacy_virt_css_url, this->m_legacy_hook_url };

    bool is_vfs_request(const nlohmann::basic_json<>& message);
    std::string HandleCssHook(const std::string& body);
    std::string HandleJsHook(const std::string& body);
    void HandleHooks(const nlohmann::basic_json<>& message);
    void vfs_request_handler(const nlohmann::basic_json<>& message);
    void mime_doc_request_handler(const nlohmann::basic_json<>& message);
    std::filesystem::path path_from_url(const std::string& requestUrl);
    processed_hooks apply_user_webkit_hooks(const std::string& requestUrl) const;
    std::string patch_document(const std::string& requestUrl, const std::string& original) const;
    std::string compile_preload_script(const processed_hooks& hooks, const std::string& millenniumPreloadPath) const;
    std::string inject_into_document_head(const std::string& original, const std::string& content) const;
    bool is_url_blacklisted(const std::string& requestUrl) const;
};
