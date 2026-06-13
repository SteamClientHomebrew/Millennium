#include "millennium/target_url.h"
#include <curl/curl.h>
#include <algorithm>
#include <cctype>

bool target_url::is_domain_match(std::string_view host, std::string_view domain) {
    if (host == domain) return true;
    if (host.length() > domain.length() && 
        host[host.length() - domain.length() - 1] == '.' &&
        host.ends_with(domain)) {
        return true;
    }
    return false;
}

target_url::target_url(const std::string& url) : m_raw_url(url) {
    CURLU* h = curl_url();
    if (!h) return;

    if (curl_url_set(h, CURLUPART_URL, url.c_str(), 0) == CURLUE_OK) {
        char* host_ptr = nullptr;
        char* scheme_ptr = nullptr;

        if (curl_url_get(h, CURLUPART_HOST, &host_ptr, 0) == CURLUE_OK &&
            curl_url_get(h, CURLUPART_SCHEME, &scheme_ptr, 0) == CURLUE_OK) {
            
            m_host = host_ptr;
            m_scheme = scheme_ptr;

            curl_free(host_ptr);
            curl_free(scheme_ptr);

            while (!m_host.empty() && m_host.back() == '.') {
                m_host.pop_back();
            }

            std::transform(m_host.begin(), m_host.end(), m_host.begin(),
                           [](unsigned char c){ return std::tolower(c); });
            std::transform(m_scheme.begin(), m_scheme.end(), m_scheme.begin(),
                           [](unsigned char c){ return std::tolower(c); });
            
            m_valid = true;
        } else {
            if (host_ptr) curl_free(host_ptr);
            if (scheme_ptr) curl_free(scheme_ptr);
        }
    }
    curl_url_cleanup(h);
}

target_url::~target_url() = default;

bool target_url::is_valid() const {
    return m_valid;
}

bool target_url::is_safe_for_network_hooks() const {
    if (!m_valid) return false;
    if (m_scheme != "http" && m_scheme != "https") return false;

    static constexpr std::string_view forbidden_domains[] = {
        "paypal.com", "paypalobjects.com", "recaptcha.net",
        "youtube.com", "youtube-nocookie.com", "youtu.be", "ytimg.com", 
        "googlevideo.com", "googleusercontent.com", "studioyoutube.com",
        "chromewebstore.google.com"
    };

    for (const auto& domain : forbidden_domains) {
        if (is_domain_match(m_host, domain)) return false;
    }
    return true;
}

bool target_url::is_safe_for_js() const {
    if (!m_valid) return false;

    if (is_domain_match(m_host, "checkout.steampowered.com")) {
        return false;
    }

    return is_safe_for_network_hooks();
}

bool target_url::is_steam_owned() const {
    if (!m_valid) return false;

    if (m_host == k_steam_loopback) return true;

    for (const auto* domain : k_steam_tlds) {
        if (is_domain_match(m_host, domain)) {
            return true;
        }
    }
    return false;
}

bool target_url::allows_webkit_injection(const std::vector<network_hook_ctl::hook_item>& hooks) const {
    if (!m_valid) return false;
    if (!is_safe_for_js()) return false;
    if (m_host == k_steam_loopback) return false;
    if (is_steam_owned()) return true;

    return std::any_of(hooks.begin(), hooks.end(), [this](const auto& hook_item) {
        return std::regex_match(m_raw_url, hook_item.hook.url_pattern);
    });
}
