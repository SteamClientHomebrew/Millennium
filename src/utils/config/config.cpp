#define _WINSOCKAPI_   
#include <stdafx.h>

#include <utils/config/config.hpp>

#include <windows.h>
#include <string>
#include <iostream>

#include <core/steam/cef_manager.hpp>
#include <utils/http/http_client.hpp>

remote_skin millennium_remote;

const void registry::set_registry(std::string key, std::string value) noexcept
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

const __declspec(noinline) std::string registry::get_registry(std::string key)
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

inline const nlohmann::basic_json<> skin_config::get_default_patches()
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

inline void skin_config::decide_overrides(nlohmann::basic_json<>& json_object)
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

skin_config& skin_config::getInstance()
{
    static skin_config instance;
    return instance;
}

void __fastcall skin_config::watch_config(const std::string& filePath, void (*callback)()) {
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

skin_config::skin_config()
{
    char buffer[MAX_PATH];  
    DWORD bufferSize = GetEnvironmentVariableA("SteamPath", buffer, MAX_PATH);

    steam_skin_path = std::format("{}/steamui/skins", std::string(buffer, bufferSize));
}

std::string skin_config::get_steam_skin_path()
{
    return steam_skin_path;
}

class config
{
public:
    static const nlohmann::basic_json<> get_remote(std::basic_ifstream<char, std::char_traits<char>>& remote)
    {
        nlohmann::basic_json<> data; 
        try { 
            remote >> data; 
        }
        catch (nlohmann::json::exception& ex) {
            return { {"config_fail", true} };
        }

        if (!data.contains("gh_username") || !data.contains("gh_repo") || !data.contains("skin-json"))
        {
            MessageBoxA(GetForegroundWindow(), "remote skin has invalid formatting. it requires json keys\ngh_username\ngh_repo\nskin-json", "Millennium", 0);
            return { {"config_fail", true} };
        }

        millennium_remote = {
            true, 
            static_cast<std::filesystem::path>(data["skin-json"].get<std::string>()).parent_path().string(),
            data["gh_username"].get<std::string>(), data["gh_repo"].get<std::string>()
        };

        try {
            data = nlohmann::json::parse(http::get(data["skin-json"]));
            data["config_fail"] = false;
        }
        catch (std::exception& ex) {
            data["config_fail"] = true;
        }

        return data;
    }
    static const nlohmann::basic_json<> get_local(std::basic_ifstream<char, std::char_traits<char>>& local)
    {
        std::stringstream buffer;
        buffer << local.rdbuf();

        if (nlohmann::json::accept(buffer.str())) {
            nlohmann::json json_object = nlohmann::json::parse(buffer.str());
            json_object["config_fail"] = false;
            millennium_remote.is_remote = false;

            return json_object;
        }
    }
};

const nlohmann::json skin_config::get_skin_config() noexcept
{
    const std::string active_skin = registry::get_registry("active-skin");

    std::basic_ifstream<char, std::char_traits<char>> local_skin_config(std::format("{}/{}/skin.json", steam_skin_path, active_skin));
    std::basic_ifstream<char, std::char_traits<char>> remote_skin_config(std::format("{}/{}", steam_skin_path, active_skin));

    if (!local_skin_config.is_open() || !local_skin_config) {

        if (!remote_skin_config.is_open() || !remote_skin_config) {
            return { { "config_fail", true } };
        }

        nlohmann::basic_json<> remote = config::get_remote(remote_skin_config); 

        if (remote.value("UseDefaultPatches", false)) { 
            decide_overrides(remote); 
        }

        return remote;
    }

    nlohmann::basic_json<> local = config::get_local(local_skin_config);

    if (local.value("UseDefaultPatches", false)) {
        decide_overrides(local);
    }

    return local;
}

const void skin_config::setup_millennium() noexcept
{
    try {
        if (!std::filesystem::exists(get_steam_skin_path()))
            std::filesystem::create_directories(get_steam_skin_path());
    }
    catch (const std::filesystem::filesystem_error& e) {
        console.err(std::format("Error creating 'skins' directory. reason: {}", e.what()));
    }

    //create registry key if it doesnt exist
    std::function<void(std::string, std::string)> create_if_empty = ([this](std::string key, std::string value) {
        if (registry::get_registry(key).empty())
            registry::set_registry(key, value);
    });

    create_if_empty("active-skin", "default");
    create_if_empty("allow-javascript", "false");
}


skin_config::skin_change_events& skin_config::skin_change_events::get_instance() {
    static skin_change_events instance;
    return instance;
}

void skin_config::skin_change_events::add_listener(const event_listener& listener) {
    listeners.push_back(listener);
}

void skin_config::skin_change_events::trigger_change() {

    std::cout << std::format("triggering skin event change, executing {} listener", listeners.size()) << std::endl;

    for (const auto& listener : listeners) {
        listener();
    }
}