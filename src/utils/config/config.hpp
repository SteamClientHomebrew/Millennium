#pragma once

struct remote_skin
{
    bool is_remote;
    std::string host, username, repo;
};
extern remote_skin millennium_remote;

namespace Settings 
{
    template<typename T> 
    void Set(std::string key, T value) noexcept;

    template<typename T> 
    T Get(std::string key);
};

class themeConfig
{
private:
    std::string m_themesPath, m_activeSkin;

    /// <summary>
    /// append default patches to the patch list if wanted
    /// </summary>
    /// <returns></returns>
    inline const nlohmann::basic_json<> defaultPatches();

    /// <summary>
    /// if default patches are overridden by the user, use the user prompted patches
    /// </summary>
    /// <param name="json_object"></param>
    inline void assignOverrides(nlohmann::basic_json<>& json_object);

public:

    themeConfig(const themeConfig&) = delete;
    themeConfig& operator=(const themeConfig&) = delete;

    static themeConfig& getInstance();

    /// <summary>
    /// event managers for skin change events
    /// </summary>
    class updateEvents {
    public:
        static updateEvents& getInstance();
        using event_listener = std::function<void()>;

        void add_listener(const event_listener& listener);
        void triggerUpdate();

    private:
        updateEvents() {}
        updateEvents(const updateEvents&) = delete;
        updateEvents& operator=(const updateEvents&) = delete;

        std::vector<event_listener> listeners;
    };

    /// <summary>
    /// file watcher on the config files, so millennium knows when to update the skin config when its active
    /// </summary>
    /// <param name="filePath">the filepath</param>
    /// <param name="callback">callback function, when the event triggers</param>
    /// <returns>void</returns>
    static void __fastcall watchPath(const std::string& directoryPath, std::function<void()> callback);

    themeConfig();

    std::string getSkinDir();

    /// <summary>
    /// get the configuration files, as json from the current skin selected
    /// </summary>
    /// <returns>json_object</returns>
    const nlohmann::json getThemeData(bool raw = false) noexcept;

    /// <summary>
    /// setup millennium if it has no configuration types
    /// </summary>
    const void setupMillennium() noexcept;
};

static themeConfig config;