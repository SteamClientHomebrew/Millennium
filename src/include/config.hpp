#pragma once
class registry {
public:
    /// <summary>
    /// set registry keys in the millennium hivekey
    /// </summary>
    /// <param name="key">name of the key</param>
    /// <param name="value">the new updated value</param>
    /// <returns>void, noexcept</returns>
    static const void set_registry(std::string key, std::string value) noexcept
    {
        HKEY hKey;
        const LPCWSTR valueData = (LPWSTR)value.c_str();

        if (RegCreateKeyEx(HKEY_CURRENT_USER, "Software\\Millennium", 0, NULL, REG_OPTION_NON_VOLATILE, KEY_WRITE, NULL, &hKey, NULL) != ERROR_SUCCESS ||
            RegSetValueEx(hKey, key.c_str(), 0, REG_SZ, reinterpret_cast<const BYTE*>(valueData), (wcslen(valueData) + 1) * sizeof(wchar_t)) != ERROR_SUCCESS)
        {
            MessageBoxA(GetForegroundWindow(), "registry settings cannot be set from millenniums user-space, try again or ask for help", "error", 0);
        }

        RegCloseKey(hKey);
    }

    /// <summary>
    /// get registry value from the millennium hivekey 
    /// </summary>
    /// <param name="key">the name of the key</param>
    /// <returns>explicitly returns string values, read as reg_sz</returns>
    static const __declspec(noinline) std::string get_registry(std::string key)
    {
        HKEY hKey;
        TCHAR sz_read_value[MAX_PATH];
        DWORD dwType = REG_SZ, dwSize = sizeof(sz_read_value);

        if (RegOpenKeyExA(HKEY_CURRENT_USER, TEXT("Software\\Millennium"), 0, KEY_READ, &hKey) != ERROR_SUCCESS ||
            RegGetValueA(hKey, NULL, (LPCSTR)key.c_str(), RRF_RT_REG_SZ, &dwType, sz_read_value, &dwSize) != ERROR_SUCCESS)
        {
            std::cout << "Failed to read " << key << " value" << std::endl;
            return std::string();
        }

        RegCloseKey(hKey);
        return sz_read_value;
    }
};

class skin_config
{
private:
    std::string steam_skin_path, active_skin;

    /// <summary>
    /// append default patches to the patch list if wanted
    /// </summary>
    /// <returns></returns>
    inline const nlohmann::basic_json<> get_default_patches()
    {
        nlohmann::basic_json<> patches = {
            {"Patches", nlohmann::json::array({
                { {"MatchRegexString", ".*http.*steam.*"}, {"TargetCss", "webkit.css"}, {"TargetJs", "webkit.js"} },
                { {"MatchRegexString", "^Steam$"}, {"TargetCss", "libraryroot.custom.css"}, {"TargetJs", "libraryroot.custom.js"} },
                { {"MatchRegexString", "^OverlayBrowser_Browser$"}, {"TargetCss", "libraryroot.custom.css"}, {"TargetJs", "libraryroot.custom.js"} },
                { {"MatchRegexString", "^SP Overlay:"}, {"TargetCss", "libraryroot.custom.css"}, {"TargetJs", "libraryroot.custom.js"} },
                { {"MatchRegexString", "Menu$"}, {"TargetCss", "libraryroot.custom.css"}, {"TargetJs", "libraryroot.custom.js"} },
                { {"MatchRegexString", "Supernav$"}, {"TargetCss", "libraryroot.custom.css"}, {"TargetJs", "libraryroot.custom.js"} },
                { {"MatchRegexString", "^notificationtoasts_"}, {"TargetCss", "libraryroot.custom.css"}, {"TargetJs", "libraryroot.custom.js"} },
                { {"MatchRegexString", "^SteamBrowser_Find$"}, {"TargetCss", "libraryroot.custom.css"}, {"TargetJs", "libraryroot.custom.js"} },
                { {"MatchRegexString", "^OverlayTab\\d+_Find$"}, {"TargetCss", "libraryroot.custom.css"}, {"TargetJs", "libraryroot.custom.js"} },
                { {"MatchRegexString", "^Steam Big Picture Mode$"}, {"TargetCss", "bigpicture.custom.css"}, {"TargetJs", "bigpicture.custom.js"} },
                { {"MatchRegexString", "^QuickAccess_"}, {"TargetCss", "bigpicture.custom.css"}, {"TargetJs", "bigpicture.custom.js"} },
                { {"MatchRegexString", "^MainMenu_"}, {"TargetCss", "bigpicture.custom.css"}, {"TargetJs", "bigpicture.custom.js"} },
                { {"MatchRegexString", ".friendsui-container"}, {"TargetCss", "friends.custom.css"}, {"TargetJs", "friends.custom.js"} },
                { {"MatchRegexString", ".ModalDialogPopup"}, {"TargetCss", "libraryroot.custom.css"}, {"TargetJs", "libraryroot.custom.js"} },
                { {"MatchRegexString", ".FullModalOverlay"}, {"TargetCss", "libraryroot.custom.css"}, {"TargetJs", "libraryroot.custom.js"} }
            })}
        };

        return patches;
    }

    /// <summary>
    /// if default patches are overridden by the user, use the user prompted patches
    /// </summary>
    /// <param name="json_object"></param>
    inline void decide_overrides(nlohmann::basic_json<>& json_object)
    {
        nlohmann::basic_json<> default_patches = get_default_patches();
        std::map<std::string, nlohmann::json> patches_map;

        for (const auto& patch : default_patches["Patches"]) {
            patches_map[patch["MatchRegexString"]] = patch;
        }
        for (const auto& patch : json_object["Patches"]) {
            patches_map[patch["MatchRegexString"]] = patch;
        }

        // Convert the map values back to a json array
        nlohmann::json patches_final;
        for (const auto& patch : patches_map) {
            patches_final.push_back(patch.second);
        }

        json_object["Patches"] = std::move(patches_final);
    }

public:

    skin_config(const skin_config&) = delete;
    skin_config& operator=(const skin_config&) = delete;

    static skin_config& getInstance()
    {
        static skin_config instance;
        return instance;
    }

    /// <summary>
    /// event managers for skin change events
    /// </summary>
    class skin_change_events {
    public:
        static skin_change_events& get_instance() {
            static skin_change_events instance;
            return instance;
        }

        using event_listener = std::function<void()>;

        void add_listener(const event_listener& listener) {
            listeners.push_back(listener);
        }

        void trigger_change() {
            for (const auto& listener : listeners) {
                listener();
            }
        }

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
    static inline void __fastcall watch_config(const std::string& filePath, void (*callback)()) {
        std::string directoryPath = filePath.substr(0, filePath.find_last_of("\\/"));

        HANDLE hDirectory = CreateFileA(directoryPath.c_str(), GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, NULL, OPEN_EXISTING, FILE_FLAG_BACKUP_SEMANTICS, NULL);
        HANDLE hEvent = CreateEvent(NULL, FALSE, FALSE, NULL);

        if (hDirectory == INVALID_HANDLE_VALUE || hEvent == NULL) {
            CloseHandle(hDirectory);
            return;
        }

        DWORD notifyFilter = FILE_NOTIFY_CHANGE_LAST_WRITE;
        const DWORD bufferSize = sizeof(FILE_NOTIFY_INFORMATION) + MAX_PATH;
        BYTE buffer[bufferSize];

        OVERLAPPED overlapped;
        memset(&overlapped, 0, sizeof(overlapped));
        overlapped.hEvent = hEvent;

        if (!ReadDirectoryChangesW(hDirectory, buffer, bufferSize, FALSE, notifyFilter, NULL, &overlapped, NULL)) {
            CloseHandle(hEvent);
            CloseHandle(hDirectory);
            return;
        }

        while (true) {
            DWORD waitResult = WaitForSingleObject(hEvent, INFINITE);
            if (waitResult != WAIT_OBJECT_0) return;

            DWORD bytesTransferred;
            if (!GetOverlappedResult(hDirectory, &overlapped, &bytesTransferred, FALSE)) break;

            FILE_NOTIFY_INFORMATION* fileInfo = reinterpret_cast<FILE_NOTIFY_INFORMATION*>(buffer);
            while (fileInfo != NULL) 
            {
                callback();
                if (fileInfo->NextEntryOffset == 0) break;

                fileInfo = reinterpret_cast<FILE_NOTIFY_INFORMATION*>(reinterpret_cast<char*>(fileInfo) + fileInfo->NextEntryOffset);
            }

            ResetEvent(overlapped.hEvent);
            if (!ReadDirectoryChangesW(hDirectory, buffer, bufferSize, FALSE, notifyFilter, NULL, &overlapped, NULL)) break;
        }
        CloseHandle(hEvent);
        CloseHandle(hDirectory);
    }

    skin_config()
    {
        char buffer[MAX_PATH];
        DWORD bufferSize = GetEnvironmentVariableA("SteamPath", buffer, MAX_PATH);

        steam_skin_path = std::format("{}/steamui/skins", std::string(buffer, bufferSize));
    }

    std::string get_steam_skin_path() 
    {
        return steam_skin_path;
    }

    /// <summary>
    /// get the configuration files, as json from the current skin selected
    /// </summary>
    /// <returns>json_object</returns>
    const nlohmann::json get_skin_config() noexcept
    {
        std::ifstream configFile(std::format("{}/{}/skin.json", steam_skin_path, registry::get_registry("active-skin")));

        if (!configFile.is_open() || !configFile) {
            return { {"config_fail", true} };
        }

        std::stringstream buffer;
        buffer << configFile.rdbuf();

        if (nlohmann::json::accept(buffer.str())) {
            nlohmann::json json_object = nlohmann::json::parse(buffer.str());
            json_object["config_fail"] = false;

            if (json_object.value("UseDefaultPatches", false)) {
                decide_overrides(json_object);
            }

            return json_object;
        }
        return false;
    }

    /// <summary>
    /// setup millennium if it has no configuration types
    /// </summary>
    inline const void verify_registry() noexcept
    {
        //create registry key if it doesnt exist
        std::function<void(std::string, std::string)> create_if_empty = ([this](std::string key, std::string value) {
            if (registry::get_registry(key).empty())
                registry::set_registry(key, value);
        });

        create_if_empty("active-skin", "default");
        create_if_empty("allow-javascript", "false");
    }
};