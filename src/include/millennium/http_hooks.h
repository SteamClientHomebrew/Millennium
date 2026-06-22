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

#pragma once

#include "millennium/fwd_decl.h"
#include "millennium/cdp_api.h"
#include "millennium/plugin_manager.h"
#include "millennium/thread_pool.h"

#include <atomic>
#include <filesystem>
#include <functional>
#include <utility>
#include <format>
#include <memory>
#include <nlohmann/json.hpp>
#include <regex>
#include <shared_mutex>
#include <string>
#include <vector>

extern std::atomic<unsigned long long> g_hookedModuleId;
std::string get_cdp_isolated_ctx_script();

/** Millennium will not load JavaScript into the following URLs (CSS injection is still allowed). */
extern const std::vector<std::regex> g_js_hook_blacklist;

/** Canonical list of Steam-owned TLDs. Both the CDP network interceptor and the webkit world manager derive their domain checks from this. */
static constexpr const char* k_steam_tlds[] = {
    "steampowered.com", "steamcommunity.com", "steamgames.com", "steam-chat.com", "steamstatic.com",
};
static constexpr const char* k_steam_loopback = "steamloopback.host";

/** Millennium will not hook the following URLs to favor user safety. (Neither JavaScript nor CSS will be injected into these URLs.) */
extern const std::vector<std::regex> g_js_and_css_hook_blacklist;

class network_hook_ctl
{
  public:
    network_hook_ctl(std::shared_ptr<plugin_manager> plugin_manager);
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

    void set_dynamic_css_provider(std::function<std::pair<std::string, std::string>()> provider);

    void shutdown();
    const char* get_ftp_url() const
    {
        return m_ftp_url;
    }

  private:
    std::shared_ptr<plugin_manager> m_plugin_manager;
    std::shared_ptr<cdp_client> m_cdp;

    struct processed_hooks
    {
        // clang-format off
        void add_stylesheet(const std::string& href)
        {
            m_css += std::format(R"(<link rel="stylesheet" href="{}">)""\n", href);
        }
        void add_script_module(const std::string& href)
        {
            m_preloads += std::format(R"(<link rel="modulepreload" href="{}" fetchpriority="high">)""\n", href);
            m_scripts += std::format(R"(<script type="module" src="{}"></script>)""\n", href);
        }
        void add_inline_style(const std::string& id, const std::string& css)
        {
            m_css += std::format(R"(<style id="{}">{}</style>)""\n", id, css);
        }
        // clang-format on

        bool empty() const
        {
            return m_css.empty() && m_preloads.empty() && m_scripts.empty();
        }
        const std::string& css() const
        {
            return m_css;
        }
        const std::string& preloads() const
        {
            return m_preloads;
        }
        const std::string& scripts() const
        {
            return m_scripts;
        }

      private:
        std::string m_css, m_preloads, m_scripts;
    };

    std::function<std::pair<std::string, std::string>()> m_dynamic_css_provider;

    std::atomic<bool> m_shutdown{ false };
    mutable std::shared_mutex m_hook_list_mtx;
    std::unique_ptr<thread_pool> m_thread_pool;
    std::shared_ptr<std::vector<hook_item>> m_hook_list_ptr;

    const char* m_ftp_url = "https://millennium.ftp/";
    const char* m_themes_url = "https://millennium.host/v1/themes/";

    /** Maintain backwards compatibility for themes that explicitly rely on this url */
    const char* m_legacy_hook_url = "https://pseudo.millennium.app/";
    const char* m_legacy_virt_js_url = "https://js.millennium.app/";
    const char* m_legacy_virt_css_url = "https://css.millennium.app/";

    /** list of all FTP related urls */
    const std::vector<const char*> m_ftpUrls = { this->m_ftp_url, this->m_themes_url, this->m_legacy_virt_js_url, this->m_legacy_virt_css_url, this->m_legacy_hook_url };

    bool is_vfs_request(const nlohmann::basic_json<>& message);
    void vfs_request_handler(const nlohmann::basic_json<>& message);
    void mime_doc_request_handler(const nlohmann::basic_json<>& message);
    std::filesystem::path path_from_url(const std::string& requestUrl);
    processed_hooks apply_user_webkit_hooks(const std::string& requestUrl) const;
    std::string inject_into_document_head(const std::string& original, const std::string& content) const;
};
