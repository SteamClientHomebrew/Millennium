#ifndef CONFIG
#define CONFIG

#include <filesystem>
#include <include/logger.hpp>

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

    skin_config()
    {
        HKEY hKey;
        DWORD dwType = REG_SZ, dwSize = MAX_PATH;
        TCHAR szInstallPath[MAX_PATH];

        auto handleError = [&](const std::string& message) -> void {
            std::cout << "couldn't get steam path, assuming steam path (hope for the best)" << std::endl;
            SteamSkinPath = "C:\\Program Files (x86)\\Steam\\";
        };

        if (RegOpenKeyEx(HKEY_CURRENT_USER, TEXT("Software\\Valve\\Steam"), 0, KEY_READ, &hKey) != ERROR_SUCCESS)
            handleError("Failed to open Steam key");

        if (RegGetValue(hKey, NULL, TEXT("SteamPath"), RRF_RT_REG_SZ, &dwType, szInstallPath, &dwSize) != ERROR_SUCCESS)
            handleError("Failed to read InstallPath value");

        RegCloseKey(hKey);

        SteamSkinPath = std::format("{}/steamui/skins", szInstallPath);
    }

    std::string get_steam_skin_path() {
        return SteamSkinPath;
    }

    nlohmann::json get_settings_config()
    {
        std::ifstream configFile(std::format("{}/settings.json", SteamSkinPath));
        if (!configFile.is_open() || !configFile) {
            console.err(" loading default steam skin, skin settings (settings.json) was not found");
        }

        std::stringstream buffer;
        buffer << configFile.rdbuf();

        try
        {
            if (nlohmann::json::accept(buffer.str())) {
                nlohmann::json json_object = nlohmann::json::parse(buffer.str());
                currentSkin = json_object["active-skin"];
                return json_object;
            }
        }
        catch (nlohmann::json::exception&) { 
            return false;
        }
        return false;
    }

    nlohmann::json get_skin_config()
    {
        get_settings_config();

        std::ifstream configFile(std::format("{}/{}/config.json", SteamSkinPath, currentSkin));
        if (!configFile.is_open() || !configFile) {
            nlohmann::json json;
            json["config_fail"] = true;
            return json;
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


    void check_if_valid_settings(std::string settings_path)
    {
        std::filesystem::path directory_path(settings_path);
        std::string setting_json = std::format("{}/settings.json", settings_path);

        if (!std::filesystem::exists(directory_path)) {
            std::filesystem::create_directory(directory_path);
            std::ofstream settings_file(setting_json);
        }
        else if (!std::filesystem::exists(setting_json))
        {
            std::ofstream settings_file(setting_json);
        }
    }

    void append_skins_to_settings()
    {
        skin_config skin_config;

        std::string steam_skin_path = skin_config.get_steam_skin_path();
        std::string setting_json_path = std::format("{}/settings.json", steam_skin_path);

        check_if_valid_settings(steam_skin_path);
        nlohmann::json skins, data;

        for (const auto& entry : std::filesystem::directory_iterator(steam_skin_path)) {
            if (entry.is_directory()) {
                skins.push_back({ {"name", entry.path().filename().string()} });
            }
        }
        skins.push_back({ {"name", "default"} });

        try {
            data = json::read_json_file(setting_json_path);
        }
        catch (nlohmann::detail::parse_error&) {
            std::ofstream file(setting_json_path, std::ofstream::out | std::ofstream::trunc);
            file.close();
        }
        data["skins"] = skins;

        if (!data.contains("active-skin"))      data["active-skin"] = "default";
        if (!data.contains("allow-javascript")) data["allow-javascript"] = true;
        if (!data.contains("enable-console"))   data["enable-console"] = false;

        if (data["enable-console"]) {
            AllocConsole();
            freopen("CONOUT$", "w", stdout);
        }
        json::write_json_file(setting_json_path, data);
    }
};

#endif