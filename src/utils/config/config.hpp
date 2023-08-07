#pragma once

struct remote_skin
{
    bool is_remote;
    std::string host, username, repo;
};
extern remote_skin millennium_remote;

class registry {
public:
    /// <summary>
    /// set registry keys in the millennium hivekey
    /// </summary>
    /// <param name="key">name of the key</param>
    /// <param name="value">the new updated value</param>
    /// <returns>void, noexcept</returns>
    static const void set_registry(std::string key, std::string value) noexcept;

    /// <summary>
    /// get registry value from the millennium hivekey 
    /// </summary>
    /// <param name="key">the name of the key</param>
    /// <returns>explicitly returns string values, read as reg_sz</returns>
    static const __declspec(noinline) std::string get_registry(std::string key);
};

class skin_config
{
private:
    std::string steam_skin_path, active_skin;

    /// <summary>
    /// append default patches to the patch list if wanted
    /// </summary>
    /// <returns></returns>
    inline const nlohmann::basic_json<> get_default_patches();

    /// <summary>
    /// if default patches are overridden by the user, use the user prompted patches
    /// </summary>
    /// <param name="json_object"></param>
    inline void decide_overrides(nlohmann::basic_json<>& json_object);

public:

    skin_config(const skin_config&) = delete;
    skin_config& operator=(const skin_config&) = delete;

    static skin_config& getInstance();

    /// <summary>
    /// event managers for skin change events
    /// </summary>
    class skin_change_events {
    public:
        static skin_change_events& get_instance();
        using event_listener = std::function<void()>;

        void add_listener(const event_listener& listener);
        void trigger_change();

    private:
        skin_change_events() {}
        skin_change_events(const skin_change_events&) = delete;
        skin_change_events& operator=(const skin_change_events&) = delete;

        std::vector<event_listener> listeners;
    };

    /// <summary>
    /// file watcher on the config files, so millennium knows when to update the skin config when its active
    /// </summary>
    /// <param name="filePath">the filepath</param>
    /// <param name="callback">callback function, when the event triggers</param>
    /// <returns>void</returns>
    static void __fastcall watch_config(const std::string& filePath, void (*callback)());

    skin_config();

    std::string get_steam_skin_path();

    /// <summary>
    /// get the configuration files, as json from the current skin selected
    /// </summary>
    /// <returns>json_object</returns>
    const nlohmann::json get_skin_config() noexcept;

    /// <summary>
    /// setup millennium if it has no configuration types
    /// </summary>
    const void setup_millennium() noexcept;
};

static skin_config config;