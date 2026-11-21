#include "millennium/theme_class_remapper.h"

#include <nlohmann/json.hpp>

#include <algorithm>
#include <array>
#include <fstream>
#include <format>
#include <sstream>

namespace
{
constexpr double k_min_table_presence = 0.5;
constexpr size_t k_stable_hash_min_length = 21;
constexpr size_t k_stable_hash_max_length = 23;

constexpr std::array<std::string_view, 4> k_unexpanded_names = { "gpfocus", "gpfocuswithin", "Focusable", "Panel" };

constexpr std::array<std::string_view, 8> k_rule_container_at_rules = { "media", "supports", "layer", "container", "document", "-moz-document", "scope", "starting-style" };

bool is_class_start(char c)
{
    return c == '-' || c == '_' || (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z');
}

bool is_class_char(char c)
{
    return c == '-' || c == '_' || (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9');
}

bool is_stable_hash_shape(std::string_view token)
{
    if (token.size() < k_stable_hash_min_length || token.size() > k_stable_hash_max_length) return false;
    bool has_marker = false;
    size_t uppercase = 0;
    for (char c : token) {
        if (!is_class_char(c)) return false;
        if ((c >= '0' && c <= '9') || c == '_' || c == '-') has_marker = true;
        if (c >= 'A' && c <= 'Z') uppercase++;
    }
    if (has_marker) return true;
    return token.find("CustomCSS") == std::string_view::npos && uppercase * 10 >= token.size() * 3;
}

bool is_valid_identifier(std::string_view token)
{
    if (token.empty()) return false;
    if (token[0] == '-') return token.size() > 1 && (token[1] == '-' || token[1] == '_' || std::isalpha(static_cast<unsigned char>(token[1])));
    return token[0] == '_' || std::isalpha(static_cast<unsigned char>(token[0]));
}

size_t skip_string(std::string_view text, size_t i)
{
    const char quote = text[i];
    size_t j = i + 1;
    while (j < text.size() && text[j] != quote) j += text[j] == '\\' ? 2 : 1;
    return std::min(j + 1, text.size());
}

size_t skip_comment(std::string_view text, size_t i)
{
    const size_t end = text.find("*/", i + 2);
    return end == std::string_view::npos ? text.size() : end + 2;
}

size_t skip_url(std::string_view text, size_t i)
{
    size_t j = i + 4;
    while (j < text.size() && (text[j] == ' ' || text[j] == '\t' || text[j] == '\n' || text[j] == '\r')) j++;
    if (j < text.size() && (text[j] == '"' || text[j] == '\'')) return i + 4;
    while (j < text.size() && text[j] != ')') j += text[j] == '\\' ? 2 : 1;
    return std::min(j + 1, text.size());
}

bool starts_with_url(std::string_view text, size_t i)
{
    if (i + 4 > text.size()) return false;
    if (i > 0 && (std::isalnum(static_cast<unsigned char>(text[i - 1])) || text[i - 1] == '-' || text[i - 1] == '_')) return false;
    return (text[i] == 'u' || text[i] == 'U') && (text[i + 1] == 'r' || text[i + 1] == 'R') && (text[i + 2] == 'l' || text[i + 2] == 'L') && text[i + 3] == '(';
}

std::string_view trim(std::string_view text)
{
    size_t begin = 0;
    size_t end = text.size();
    while (begin < end && std::isspace(static_cast<unsigned char>(text[begin]))) begin++;
    while (end > begin && std::isspace(static_cast<unsigned char>(text[end - 1]))) end--;
    return text.substr(begin, end - begin);
}

std::string at_rule_name(std::string_view prelude)
{
    prelude = trim(prelude);
    if (prelude.empty() || prelude[0] != '@') return {};
    size_t end = 1;
    while (end < prelude.size() && (std::isalnum(static_cast<unsigned char>(prelude[end])) || prelude[end] == '-')) end++;
    std::string name(prelude.substr(1, end - 1));
    std::transform(name.begin(), name.end(), name.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    return name;
}

bool is_custom_property_opening(std::string_view prelude)
{
    prelude = trim(prelude);
    if (prelude.size() < 3 || prelude[0] != '-' || prelude[1] != '-') return false;
    const size_t colon = prelude.find(':');
    if (colon == std::string_view::npos) return false;
    return trim(prelude.substr(colon + 1)).empty();
}
bool is_class_chain(std::string_view text, size_t dot_index)
{
    size_t k = dot_index;
    while (k > 0 && is_class_char(text[k - 1])) k--;
    return k > 0 && text[k - 1] == '.';
}

} // namespace

theme_class_remapper::theme_class_remapper(std::function<std::string()> table_loader, std::function<std::unordered_set<std::string>()> live_class_loader)
    : m_table_loader(std::move(table_loader)), m_live_class_loader(std::move(live_class_loader))
{
}

std::unordered_set<std::string> theme_class_remapper::scan_class_tokens(std::string_view css)
{
    std::unordered_set<std::string> found;
    size_t i = 0;
    while (i < css.size()) {
        const char c = css[i];
        if (c == '/' && i + 1 < css.size() && css[i + 1] == '*') { i = skip_comment(css, i); continue; }
        if (c == '"' || c == '\'') { i = skip_string(css, i); continue; }
        if (c == '\\') { i += 2; continue; }
        if (c == '.' && i + 1 < css.size() && is_class_start(css[i + 1])) {
            size_t j = i + 1;
            while (j < css.size() && is_class_char(css[j])) j++;
            found.emplace(css.substr(i + 1, j - i - 1));
            i = j;
            continue;
        }
        i++;
    }
    return found;
}

std::unordered_set<std::string> theme_class_remapper::scan_directory(const std::filesystem::path& directory)
{
    std::unordered_set<std::string> found;
    std::error_code error;
    if (!std::filesystem::exists(directory, error)) return found;

    for (std::filesystem::recursive_directory_iterator it(directory, std::filesystem::directory_options::skip_permission_denied, error), end; it != end; it.increment(error)) {
        if (error) break;
        if (!it->is_regular_file(error) || it->path().extension() != ".css") continue;

        std::ifstream stream(it->path(), std::ios::binary);
        std::ostringstream buffer;
        buffer << stream.rdbuf();
        const std::string content = buffer.str();
        for (auto& token : scan_class_tokens(content)) found.insert(std::move(token));
    }
    return found;
}

std::unordered_set<std::string> theme_class_remapper::scan_steam_ui(const std::filesystem::path& directory)
{
    std::ifstream entry(directory / "index.html", std::ios::binary);
    if (!entry.is_open()) return {};

    std::ostringstream buffer;
    buffer << entry.rdbuf();
    if (buffer.str().find("type=\"module\"") == std::string::npos) return {};

    return scan_directory(directory);
}

void theme_class_remapper::initialise() const
{
    try {
        const std::string raw = m_table_loader();
        if (raw.empty()) {
            m_status = "inactive, no class table is bundled";
            return;
        }

        const auto parsed = nlohmann::json::parse(raw);
        m_table.build = parsed.value("build", 0LL);
        for (const auto& [stable_class, beta_class] : parsed.at("map").items()) m_table.map.emplace(stable_class, beta_class.get<std::string>());
        if (parsed.contains("multi")) {
            for (const auto& [stable_class, beta_classes] : parsed.at("multi").items()) m_table.multi.emplace(stable_class, beta_classes.get<std::vector<std::string>>());
        }
        for (const auto& [name, classes] : parsed.at("names").items()) m_table.names.emplace(name, classes.get<std::vector<std::string>>());
    } catch (const std::exception& error) {
        m_status = std::format("inactive, failed to load the class table: {}", error.what());
        return;
    }

    m_live = m_live_class_loader();

    std::unordered_set<std::string> targets;
    for (const auto& [stable_class, beta_class] : m_table.map) targets.insert(beta_class);
    if (targets.empty()) {
        m_status = "inactive, the class table is empty";
        return;
    }

    size_t present = 0;
    for (const auto& target : targets) present += m_live.contains(target) ? 1 : 0;
    const double ratio = static_cast<double>(present) / static_cast<double>(targets.size());

    if (ratio < k_min_table_presence) {
        m_status = std::format("inactive, only {}/{} mapped classes exist in this Steam UI", present, targets.size());
        return;
    }

    m_active = true;
    m_status = std::format("active for table build {} ({}/{} mapped classes present)", m_table.build, present, targets.size());
}

const std::string& theme_class_remapper::status() const
{
    active();
    return m_status;
}

bool theme_class_remapper::active() const
{
    std::call_once(m_init_flag, [this]() { initialise(); });
    return m_active;
}

std::string theme_class_remapper::expand_multi(const std::string& token) const
{
    const auto found = m_table.multi.find(token);
    if (found == m_table.multi.end()) return {};

    std::vector<std::string> members;
    for (const auto& beta_class : found->second) {
        if (is_valid_identifier(beta_class)) members.push_back("." + beta_class);
    }

    if (members.empty()) return {};
    if (members.size() == 1) return members.front();

    std::string joined = ":is(";
    for (size_t m = 0; m < members.size(); m++) {
        if (m) joined.push_back(',');
        joined.append(members[m]);
    }
    joined.push_back(')');
    return joined;
}

std::string theme_class_remapper::expand_name(const std::string& token) const
{
    const auto named = m_table.names.find(token);
    if (named == m_table.names.end() || std::find(k_unexpanded_names.begin(), k_unexpanded_names.end(), token) != k_unexpanded_names.end()) return {};

    std::vector<std::string> members;
    if (m_live.contains(token)) members.push_back("." + token);
    for (const auto& beta_class : named->second) {
        if (is_valid_identifier(beta_class)) members.push_back("." + beta_class);
    }

    if (members.empty()) return {};
    if (members.size() == 1) return members.front();

    std::string joined = ":is(";
    for (size_t m = 0; m < members.size(); m++) {
        if (m) joined.push_back(',');
        joined.append(members[m]);
    }
    joined.push_back(')');
    return joined;
}

std::string theme_class_remapper::rewrite_selector(std::string_view selector, stats& counters) const
{
    std::string out;
    out.reserve(selector.size() + 32);
    size_t i = 0;

    while (i < selector.size()) {
        const char c = selector[i];

        if (c == '\\') {
            out.append(selector.substr(i, 2));
            i += 2;
            continue;
        }
        if (c == '/' && i + 1 < selector.size() && selector[i + 1] == '*') {
            const size_t end = skip_comment(selector, i);
            out.append(selector.substr(i, end - i));
            i = end;
            continue;
        }
        if (c == '"' || c == '\'') {
            const size_t end = skip_string(selector, i);
            out.append(selector.substr(i, end - i));
            i = end;
            continue;
        }
        if (c == '[') {
            size_t j = i + 1;
            while (j < selector.size() && selector[j] != ']') {
                if (selector[j] == '"' || selector[j] == '\'') j = skip_string(selector, j);
                else j++;
            }
            const size_t end = std::min(j + 1, selector.size());
            out.append(selector.substr(i, end - i));
            i = end;
            continue;
        }
        if (c == '.' && i + 1 < selector.size() && is_class_start(selector[i + 1])) {
            size_t j = i + 1;
            while (j < selector.size() && is_class_char(selector[j])) j++;
            const std::string token(selector.substr(i + 1, j - i - 1));

            if (const auto mapped = m_table.map.find(token); mapped != m_table.map.end() && is_valid_identifier(mapped->second)) {
                out.push_back('.');
                out.append(mapped->second);
                counters.remapped++;
            } else if (const std::string tied = expand_multi(token); !tied.empty()) {
                out.append(tied);
                counters.remapped++;
            } else if (is_stable_hash_shape(token)) {
                out.push_back('.');
                out.append(token);
                counters.unmapped++;
            } else if (const std::string expanded = expand_name(token); !expanded.empty()) {
                out.append(expanded);
                counters.expanded++;
            } else {
                out.push_back('.');
                out.append(token);
            }
            i = j;
            continue;
        }

        out.push_back(c);
        i++;
    }
    return out;
}

std::string theme_class_remapper::rewrite_script(std::string_view script, stats* out_stats) const
{
    stats counters;
    if (!active()) {
        if (out_stats) *out_stats = counters;
        return std::string(script);
    }

    enum class mode
    {
        code,
        quote,
        tmpl
    };
    struct frame
    {
        mode kind;
        char quote;
        int depth;
    };

    std::string out;
    out.reserve(script.size());

    std::vector<frame> stack{ { mode::code, 0, 0 } };
    size_t i = 0;
    while (i < script.size()) {
        const char c = script[i];
        frame& top = stack.back();

        if (top.kind == mode::quote) {
            if (c == '\\' && i + 1 < script.size()) {
                out.push_back(c);
                out.push_back(script[i + 1]);
                i += 2;
                continue;
            }
            if (c == top.quote) {
                stack.pop_back();
                out.push_back(c);
                i++;
                continue;
            }
        } else if (top.kind == mode::tmpl) {
            if (c == '\\' && i + 1 < script.size()) {
                out.push_back(c);
                out.push_back(script[i + 1]);
                i += 2;
                continue;
            }
            if (c == '`') {
                stack.pop_back();
                out.push_back(c);
                i++;
                continue;
            }
            if (c == '$' && i + 1 < script.size() && script[i + 1] == '{') {
                out.append("${");
                stack.push_back({ mode::code, 0, 0 });
                i += 2;
                continue;
            }
        } else {
            if (c == '"' || c == '\'') {
                stack.push_back({ mode::quote, c, 0 });
                out.push_back(c);
                i++;
                continue;
            }
            if (c == '`') {
                stack.push_back({ mode::tmpl, 0, 0 });
                out.push_back(c);
                i++;
                continue;
            }
            if (c == '{') {
                top.depth++;
            } else if (c == '}') {
                if (top.depth == 0 && stack.size() > 1) {
                    stack.pop_back();
                    out.push_back(c);
                    i++;
                    continue;
                }
                if (top.depth > 0) top.depth--;
            }
            if (c == '/' && i + 1 < script.size() && script[i + 1] == '/') {
                const size_t end = script.find('\n', i);
                const size_t stop = end == std::string_view::npos ? script.size() : end;
                out.append(script.substr(i, stop - i));
                i = stop;
                continue;
            }
            if (c == '/' && i + 1 < script.size() && script[i + 1] == '*') {
                const size_t end = script.find("*/", i + 2);
                const size_t stop = end == std::string_view::npos ? script.size() : end + 2;
                out.append(script.substr(i, stop - i));
                i = stop;
                continue;
            }
        }

        if (!is_class_char(c)) {
            out.push_back(c);
            i++;
            continue;
        }

        size_t j = i;
        while (j < script.size() && is_class_char(script[j])) j++;
        const std::string_view token = script.substr(i, j - i);
        const bool in_string = stack.back().kind != mode::code;
        const bool after_dot = in_string && i > 0 && script[i - 1] == '.';
        const bool selector_position = after_dot && (i == 1 || !is_class_char(script[i - 2]) || is_class_chain(script, i - 1));

        if (is_stable_hash_shape(token)) {
            if (const auto mapped = m_table.map.find(std::string(token)); mapped != m_table.map.end()) {
                out.append(mapped->second);
                counters.remapped++;
            } else if (const std::string tied = after_dot ? expand_multi(std::string(token)) : std::string(); !tied.empty()) {
                out.pop_back();
                out.append(tied);
                counters.remapped++;
            } else {
                out.append(token);
                counters.unmapped++;
            }
        } else if (selector_position) {
            const std::string expanded = expand_name(std::string(token));
            if (expanded.empty()) {
                out.append(token);
            } else {
                out.pop_back();
                out.append(expanded);
                counters.expanded++;
            }
        } else {
            out.append(token);
        }
        i = j;
    }

    if (out_stats) *out_stats = counters;
    return out;
}

std::string theme_class_remapper::rewrite(std::string_view css, stats* out_stats) const
{
    stats counters;
    if (!active()) {
        if (out_stats) *out_stats = counters;
        return std::string(css);
    }

    std::string out;
    out.reserve(css.size() + css.size() / 8);

    std::vector<block_kind> stack{ block_kind::root };
    size_t flushed = 0;
    size_t prelude_start = 0;
    size_t i = 0;

    while (i < css.size()) {
        const char c = css[i];

        if (c == '/' && i + 1 < css.size() && css[i + 1] == '*') { i = skip_comment(css, i); continue; }
        if (c == '"' || c == '\'') { i = skip_string(css, i); continue; }
        if (c == '\\') { i += 2; continue; }
        if (starts_with_url(css, i)) { i = skip_url(css, i); continue; }

        if (c == ';') {
            prelude_start = i + 1;
            i++;
            continue;
        }

        if (c == '{') {
            const std::string_view prelude = css.substr(prelude_start, i - prelude_start);
            const block_kind context = stack.back();
            block_kind next = block_kind::opaque;

            if (context == block_kind::opaque) {
                next = block_kind::opaque;
            } else if (context == block_kind::declarations && is_custom_property_opening(prelude)) {
                next = block_kind::opaque;
            } else {
                const std::string at_rule = at_rule_name(prelude);
                if (!at_rule.empty()) {
                    const bool container = std::find(k_rule_container_at_rules.begin(), k_rule_container_at_rules.end(), at_rule) != k_rule_container_at_rules.end();
                    if (!container) next = block_kind::opaque;
                    else next = context == block_kind::declarations ? block_kind::declarations : block_kind::rules;
                } else {
                    out.append(css.substr(flushed, prelude_start - flushed));
                    out.append(rewrite_selector(prelude, counters));
                    flushed = i;
                    next = block_kind::declarations;
                }
            }

            stack.push_back(next);
            prelude_start = i + 1;
            i++;
            continue;
        }

        if (c == '}') {
            if (stack.size() > 1) stack.pop_back();
            prelude_start = i + 1;
            i++;
            continue;
        }

        i++;
    }

    out.append(css.substr(flushed));
    if (out_stats) *out_stats = counters;
    return out;
}
