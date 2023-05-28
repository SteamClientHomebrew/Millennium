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
        LPCSTR subKey = "Software\\Millennium";
        LPCSTR valueName = key.c_str();
        LPCWSTR valueData = (LPWSTR)value.c_str();

        if (RegCreateKeyEx(HKEY_CURRENT_USER, subKey, 0, NULL, REG_OPTION_NON_VOLATILE, KEY_WRITE, NULL, &hKey, NULL) == ERROR_SUCCESS) {
            if (RegSetValueEx(hKey, valueName, 0, REG_SZ, reinterpret_cast<const BYTE*>(valueData), (wcslen(valueData) + 1) * sizeof(wchar_t)) == ERROR_SUCCESS) {
                std::cout << "Registry key and value created successfully.\n";
            }
            else {
                std::cout << "Failed to set the registry value.\n";
            }
            RegCloseKey(hKey);
        }
        else {
            std::cout << "Failed to create the registry key.\n";
        }
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

        if (RegOpenKeyExA(HKEY_CURRENT_USER, TEXT("Software\\Millennium"), 0, KEY_READ, &hKey) != ERROR_SUCCESS)
            std::cout << "Failed to open Millennium hive" << std::endl;

        if (RegGetValueA(hKey, NULL, (LPCSTR)key.c_str(), RRF_RT_REG_SZ, &dwType, sz_read_value, &dwSize) != ERROR_SUCCESS)
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
    Console console;
    std::string SteamSkinPath;
    std::string currentSkin;
public:

    skin_config(const skin_config&) = delete;
    skin_config& operator=(const skin_config&) = delete;

    static skin_config& getInstance()
    {
        static skin_config instance;
        return instance;
    }

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
        HKEY hKey;
        DWORD dwType = REG_SZ, dwSize = MAX_PATH;
        TCHAR szInstallPath[MAX_PATH];

        auto handleError = [&]() -> void { SteamSkinPath = "C:\\Program Files (x86)\\Steam\\"; };

        if (RegOpenKeyEx(HKEY_CURRENT_USER, TEXT("Software\\Valve\\Steam"), 0, KEY_READ, &hKey) != ERROR_SUCCESS) 
            handleError();
        if (RegGetValue(hKey, NULL, TEXT("SteamPath"), RRF_RT_REG_SZ, &dwType, szInstallPath, &dwSize) != ERROR_SUCCESS) 
            handleError();

        RegCloseKey(hKey);
        SteamSkinPath = std::format("{}/steamui/skins", szInstallPath);
    }

    std::string get_steam_skin_path() {
        return SteamSkinPath;
    }

    /// <summary>
    /// get the configuration files, as json from the current skin selected
    /// </summary>
    /// <returns>json_object</returns>
    nlohmann::json get_skin_config()
    {
        std::ifstream configFile(std::format("{}/{}/config.json", SteamSkinPath, registry::get_registry("active-skin")));

        if (!configFile.is_open() || !configFile) {
            return { {"config_fail", true} };
        }

        std::stringstream buffer;
        buffer << configFile.rdbuf();

        if (nlohmann::json::accept(buffer.str())) {
            nlohmann::json json_object = nlohmann::json::parse(buffer.str());
            json_object["config_fail"] = false;
            return json_object;
        }
        return false;
    }

    /// <summary>
    /// setup millennium if it has no configuration types
    /// </summary>
    void verify_registry()
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