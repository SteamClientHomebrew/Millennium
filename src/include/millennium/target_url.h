#pragma once

#include <string>
#include <vector>
#include "millennium/http_hooks.h"

class target_url {
public:
    explicit target_url(const std::string& url);
    ~target_url();

    // Was parsed successfully?
    bool is_valid() const;

    // Policy queries
    bool is_safe_for_network_hooks() const;
    bool is_safe_for_js() const;
    bool is_steam_owned() const;
    bool allows_webkit_injection(const std::vector<network_hook_ctl::hook_item>& hooks) const;

    static bool is_domain_match(std::string_view host, std::string_view domain);

    const std::string& scheme() const { return m_scheme; }
    const std::string& host() const { return m_host; }
    const std::string& raw_url() const { return m_raw_url; }

private:
    bool m_valid = false;
    std::string m_scheme;
    std::string m_host;
    std::string m_raw_url;
};
