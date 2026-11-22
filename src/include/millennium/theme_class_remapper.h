#pragma once
#include <filesystem>
#include <functional>
#include <mutex>
#include <string>
#include <string_view>
#include <unordered_map>
#include <unordered_set>
#include <vector>

class theme_class_remapper
{
  public:
    struct stats
    {
        size_t remapped = 0;
        size_t expanded = 0;
        size_t unmapped = 0;
    };

    theme_class_remapper(std::function<std::string()> table_loader, std::function<std::unordered_set<std::string>()> live_class_loader);

    bool active() const;
    const std::string& status() const;
    std::string rewrite(std::string_view css, stats* out = nullptr) const;
    std::string rewrite_script(std::string_view script, stats* out = nullptr, bool expand_names = true) const;

    static std::unordered_set<std::string> scan_class_tokens(std::string_view css);
    static std::unordered_set<std::string> scan_directory(const std::filesystem::path& directory);
    static std::unordered_set<std::string> scan_steam_ui(const std::filesystem::path& directory);

  private:
    struct table_data
    {
        long long build = 0;
        std::unordered_map<std::string, std::string> map;
        std::unordered_map<std::string, std::vector<std::string>> multi;
        std::unordered_map<std::string, std::vector<std::string>> names;
    };

    enum class block_kind
    {
        root,
        rules,
        declarations,
        opaque
    };

    std::function<std::string()> m_table_loader;
    std::function<std::unordered_set<std::string>()> m_live_class_loader;

    mutable std::once_flag m_init_flag;
    mutable bool m_active = false;
    mutable std::string m_status;
    mutable table_data m_table;
    mutable std::unordered_set<std::string> m_live;

    void initialise() const;
    std::string expand_name(const std::string& token) const;
    std::string expand_multi(const std::string& token) const;
    std::string rewrite_selector(std::string_view selector, stats& counters) const;
    std::string rewrite_animation_names(std::string_view text, stats& counters) const;
};
