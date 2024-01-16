#define _WINSOCKAPI_   
#include <stdafx.h>

#include <utils/config/config.hpp>

#include <windows.h>
#include <string>
#include <iostream>

#include <core/steam/cef_manager.hpp>
#include <utils/http/http_client.hpp>

#include <format>
#include <filesystem>

remote_skin millennium_remote;

static constexpr const std::string_view registryPath = "Software\\Millennium";

void setRegistry(std::string key, std::string value) noexcept
{
    HKEY hKey;
    const LPCWSTR valueData = (LPWSTR)value.c_str();

    if (RegCreateKeyEx(HKEY_CURRENT_USER, registryPath.data(), 0, NULL, REG_OPTION_NON_VOLATILE, KEY_WRITE, NULL, &hKey, NULL) != ERROR_SUCCESS ||
        RegSetValueEx(hKey, key.c_str(), 0, REG_SZ, reinterpret_cast<const BYTE*>(valueData), (wcslen(valueData) + 1) * sizeof(wchar_t)) != ERROR_SUCCESS)
    {
        MsgBox("registry settings cannot be set from millenniums user-space, try again or ask for help", "error", MB_ICONINFORMATION);
    }

    RegCloseKey(hKey);
}

std::string getRegistry(std::string key)
{
    HKEY hKey;
    TCHAR value[MAX_PATH];
    DWORD dwType = REG_SZ, dwSize = sizeof(value);

    if (RegOpenKeyExA(HKEY_CURRENT_USER, TEXT(registryPath.data()), 0, KEY_READ, &hKey) != ERROR_SUCCESS ||
        RegGetValueA(hKey, NULL, (LPCSTR)key.c_str(), RRF_RT_REG_SZ, &dwType, value, &dwSize) != ERROR_SUCCESS)
    {
        console.log(std::format("failed to read {} value from settings, this MAY be fatal.", key));
        return std::string::basic_string();
    }

    RegCloseKey(hKey);
    return value;
}

namespace Settings 
{
    template <>
    void Set(std::string key, bool value) noexcept { setRegistry(key, value ? "true" : "false"); }

    template <>
    void Set(std::string key, int value) noexcept { setRegistry(key, std::to_string(value)); }

    template <>
    void Set(std::string key, const char* value) noexcept { setRegistry(key, static_cast<std::string>(value)); }

    template <>
    void Set(std::string key, std::string value) noexcept { setRegistry(key, static_cast<std::string>(value)); }

    template<>
    std::string Get<std::string>(std::string key) {    

        std::string registry = getRegistry(key);
        return static_cast<std::string>(registry);
    }

    template<>
    bool Get<bool>(std::string key) {

        std::string registry = getRegistry(key);
        return registry.empty() ? false : registry == "true";
    }

    template <>
    int Get<int>(std::string key) {

        std::string registry = getRegistry(key);
        return registry.empty() ? 0 : (uint16_t)std::stoi(registry);
    }
}



inline const nlohmann::basic_json<> themeConfig::defaultPatches()
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

inline void themeConfig::assignOverrides(nlohmann::basic_json<>& json_object)
{
    nlohmann::basic_json<> default_patches = defaultPatches();
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

themeConfig& themeConfig::getInstance()
{
    static themeConfig instance;
    return instance;
}

class HandleWrapper {
public:
    HandleWrapper(HANDLE handle) : handle(handle) {}

    ~HandleWrapper() {
        if (handle != INVALID_HANDLE_VALUE && handle != NULL) {
            CloseHandle(handle);
        }
    }

    operator HANDLE() const {
        return handle;
    }

private:
    HANDLE handle;
};

void __fastcall themeConfig::watchPath(const std::string& directoryPath, std::function<void()> callback) {


    // notification filters, name, folder created, file attributes changed, file size change
    DWORD notifyFilter = FILE_NOTIFY_CHANGE_FILE_NAME | FILE_NOTIFY_CHANGE_DIR_NAME | FILE_NOTIFY_CHANGE_ATTRIBUTES | FILE_NOTIFY_CHANGE_SIZE;

    console.log(std::format("[bootstrap] sync file watcher starting on dir: {}", directoryPath));

    HandleWrapper hDirectory = CreateFileA(directoryPath.c_str(), GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, NULL, OPEN_EXISTING, FILE_FLAG_BACKUP_SEMANTICS, NULL);
    HandleWrapper hEvent = CreateEvent(NULL, FALSE, FALSE, NULL);

    if (hDirectory == INVALID_HANDLE_VALUE || hEvent == NULL) {
        CloseHandle(hDirectory);
        return;
    }

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

themeConfig::themeConfig()
{
    char buffer[MAX_PATH];  
    DWORD bufferSize = GetEnvironmentVariableA("SteamPath", buffer, MAX_PATH);

    m_steamPath = std::string(buffer, bufferSize);
    m_themesPath = std::format("{}/steamui/skins", std::string(buffer, bufferSize));
}

std::string themeConfig::getSkinDir()
{
    return m_themesPath;
}

const std::string themeConfig::getRootColors()
{
    const auto data = config.getThemeData();

    if (data.contains("GlobalsColors") && data.is_object())
    {
        std::string header = ":root { ";

        for (const auto& color : data["GlobalsColors"])
        {
            if (!color.contains("ColorName") || !color.contains("HexColorCode") || !color.contains("Description"))
            {
                console.err("Couldn't add global color to array. 'ColorName' or 'HexColorCode' or 'Description' doesn't exist");
                continue;
            }

            std::string name = color["ColorName"];
            std::string col = color["HexColorCode"];
            std::string description = color["Description"];

            header += std::format("{}: {}; ", name, col, description);
        }

        header += "}";

        return header;
    }
    else {
        return "[NoColors]";
    }
}

const void themeConfig::installFonts()
{
    const auto config = this->getThemeData();

    if (config.contains("Fonts")) 
    {
        if (!config["Fonts"].is_array())
        {
            console.err("Can't install Fonts. Reason: 'Fonts' key is not an object");
            return;
        }

        for (const auto& font : config["Fonts"])
        {
            if (!font.contains("FileName") || !font.contains("Download"))
            {
                console.err("Font entry doesn't contain 'FileName' or 'Download'");
                continue;
            }

            static std::vector<std::string> fileTypes = { ".ttf", ".oft", ".woff", ".woff2", ".eot", ".svg", "cff", "pfb" };

            auto fileDetails = std::filesystem::path(font["Download"].get<std::string>());

            if (std::find(fileTypes.begin(), fileTypes.end(), fileDetails.extension().string()) != fileTypes.end())
            {
                console.log(std::format("Trying to download font [{}]", font["FileName"].get<std::string>()));

                auto filePath = std::filesystem::path(this->m_steamPath) / "steamui" / "fonts" / font["FileName"];

                if (std::filesystem::exists(filePath)) {
                    console.log(std::format("Font {} is already installed, skipping...", font["FileName"].get<std::string>()));
                    continue;
                }
                
                try {
                    const std::vector<unsigned char> fileContent = http::get_bytes(font["Download"].get<std::string>().c_str());

                    console.log(filePath.string());
                    console.log(std::format("{}", fileContent.size()));

                    std::ofstream fileStream(filePath, std::ios::binary);

                    if (!fileStream) {
                        console.log(std::format("Failed to open file for writing: {}", filePath.string()));
                        return;
                    }

                    fileStream.write(reinterpret_cast<const char*>(fileContent.data()), fileContent.size());

                    if (!fileStream) {
                        console.log(std::format("Error writing to file: {}", filePath.string()));
                    }
                    fileStream.close();
                }
                catch (const http_error& err) {
                    switch (err.code())
                    {
                        case http_error::errors::couldnt_connect: {
                            console.err("Couldn't install font, no internet?");
                            break;
                        }
                        case http_error::errors::miscellaneous: {
                            console.err("Couldn't install font, miscellaneous error.");
                            break;
                        }
                        case http_error::errors::not_allowed: {
                            console.err("Couldn't install font, HTTP requests disabled.");
                            break;
                        }
                    }
                }
            }
            else
            {
                console.err("Can't download font because it is an unverified/unallowed file type");
            }
        }
    }
}

class config
{
public:
    static const nlohmann::basic_json<> get_remote(std::basic_ifstream<char, std::char_traits<char>>& remote)
    {
        nlohmann::basic_json<> data; 
        try {
            std::string file_content((std::istreambuf_iterator<char>(remote)), std::istreambuf_iterator<char>());
            data = nlohmann::json::parse(file_content, nullptr, true, true);
        }
        catch (const nlohmann::json::exception&) {
            // Handle parsing errors here
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
        catch (std::exception&) {
            data["config_fail"] = true;

            console.err("tried to get remote skin config but networking is disabled.");
        }

        return data;
    }
    static const nlohmann::basic_json<> get_local(std::basic_ifstream<char, std::char_traits<char>>& local, bool raw = false)
    {
        nlohmann::basic_json<> data;
        try {
            std::string file_content((std::istreambuf_iterator<char>(local)), std::istreambuf_iterator<char>());
            data = nlohmann::json::parse(file_content, nullptr, true, true);
        }
        catch (const nlohmann::json::exception& err) {
            // Handle parsing errors here
            console.err(std::format("Error while parsing file contents in {}, Message: {}", __func__, err.what()));
            return { {"config_fail", true} };
        }

        if (raw == false) {
            data["config_fail"] = false;
            millennium_remote.is_remote = false;
        }
        return data;
    }
};

const void themeConfig::setThemeData(nlohmann::json& object) noexcept
{
    const std::string m_activeSkin = Settings::Get<std::string>("active-skin");

    std::string filePath = std::format("{}/{}/skin.json", m_themesPath, m_activeSkin);

    std::ofstream outputFile(filePath);

    if (outputFile.is_open())
    {
        outputFile << object.dump(4);
        outputFile.close();
    }
    else
    {
        console.log("Couldn't setThemeData. Couldn't Open the file");
    }
}

const nlohmann::json themeConfig::getThemeData(bool raw) noexcept
{
    const std::string ACTIVE_ITEM = Settings::Get<std::string>("active-skin");

    std::basic_ifstream<char, std::char_traits<char>> localTheme(std::format("{}/{}/skin.json", m_themesPath, ACTIVE_ITEM));
    //std::basic_ifstream<char, std::char_traits<char>> cloudTheme(std::format("{}/{}", m_themesPath, m_activeSkin));

    nlohmann::basic_json<> jsonBuffer;

    if (raw == true) 
    {
        nlohmann::basic_json<> data;
        try {
            std::string file_content((std::istreambuf_iterator<char>(localTheme)), std::istreambuf_iterator<char>());
            data = nlohmann::json::parse(file_content, nullptr, true, true);
        }
        catch (const nlohmann::json::exception&) {
            return { {"config_fail", true} };
        }

        console.log(data.dump(4));
        return data;
    }

    if (localTheme.is_open() && localTheme)
        jsonBuffer = config::get_local(localTheme);
    else
        jsonBuffer = { { "config_fail", true } };

    if (jsonBuffer.value("UseDefaultPatches", false)) {
        this->assignOverrides(jsonBuffer);
    }

    bool hasJavaScriptPatch = std::any_of(jsonBuffer["Patches"].begin(), jsonBuffer["Patches"].end(),
        [](const nlohmann::json& patch) {
            return patch.contains("TargetJs");
        });

    if (hasJavaScriptPatch && Settings::Get<bool>("allow-javascript") == false) {
        int result = MsgBox(
            "The selected theme may be using JavaScript to enhance your Steam experience.\n"
            "You have JavaScript disabled in Millennium settings, therefore, the selected skin may not function properly.\n\n"
            "Enable JavaScript ONLY IF you trust the developer, have manually reviewed the code, or it's an official theme.\n\n"
            "Would you like to enable JavaScript execution?",
            "Notice", 
            MB_YESNO | MB_ICONINFORMATION
        );
        switch (result) {
            case IDYES: {
                Settings::Set("allow-javascript", true);
            }
        }
    }

    return jsonBuffer;
}

const void themeConfig::setupMillennium() noexcept
{
    try {
        if (!std::filesystem::exists(this->getSkinDir()))
            std::filesystem::create_directories(this->getSkinDir());
    }
    catch (const std::filesystem::filesystem_error& e) {
        console.err(std::format("Error creating 'skins' directory. reason: {}", e.what()));
    }

    //create registry key if it doesnt exist
    const auto nullOverwrite = ([this](std::string key, auto value) {
        if (Settings::Get<std::string>(key).empty()) {
            console.log(std::format("[bootstrap] creating settings key: {}", key));
            Settings::Set(key, value);
        }
    });

    //if the following settings dont already exist, create them
    nullOverwrite("active-skin", "default");

    nullOverwrite("allow-javascript", false); //disallow js by default on a skin

    nullOverwrite("allow-stylesheet", true); 
    nullOverwrite("allow-auto-updates", false); 
    nullOverwrite("allow-auto-updates-sound", false);

    nullOverwrite("allow-store-load", true);

    nullOverwrite("enableUrlBar", true); //default value
    nullOverwrite("NotificationsPos", 3); //default position i.e bottom right
}


themeConfig::updateEvents& themeConfig::updateEvents::getInstance() {
    static updateEvents instance;
    return instance;
}

void themeConfig::updateEvents::add_listener(const event_listener& listener) {
    listeners.push_back(listener);
}

void themeConfig::updateEvents::triggerUpdate() {

    console.log(std::format("triggering skin event change, executing {} listener", listeners.size()));

    for (const auto& listener : listeners) {
        listener();
    }
}
