#ifndef CONFIG
#define CONFIG

#include <filesystem>
#include <include/logger.hpp>

class SkinConfig
{
private:
    Console console;
    std::string SteamSkinPath;
    std::string currentSkin;
public:

    SkinConfig(const SkinConfig&) = delete;
    SkinConfig& operator=(const SkinConfig&) = delete;

    static SkinConfig& getInstance()
    {
        static SkinConfig instance;
        return instance;
    }

    SkinConfig()
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

    // Taken from https://stackoverflow.com/a/24315631/7418213
    std::string ReplaceAll(std::string str, const std::string& from, const std::string& to) {
        size_t start_pos = 0;
        while ((start_pos = str.find(from, start_pos)) != std::string::npos) {
            str.replace(start_pos, from.length(), to);
            start_pos += to.length(); // Handles case where 'to' is a substring of 'from'
        }
        return str;
    }

    nlohmann::json GetSteamuiLocalization(std::string languageName)
    {
        std::ifstream steamuiLangResource(std::format("{}/../localization/steamui_{}-json.js", SteamSkinPath, languageName));
        if (!steamuiLangResource.is_open() || !steamuiLangResource) {
            console.err(std::format(" loading steamui localization data, language file (steamui_{}-json.js) was not found", languageName));
        }

        std::stringstream buffer;
        buffer << steamuiLangResource.rdbuf();
        std::string contentString = buffer.str();
        std::string delimiterStart = "JSON.parse('";
        std::string delimiterEnd = "')}}]);";

        // Remove JavaScript parts and extract pure JSON
        contentString.erase(0, contentString.find(delimiterStart) + delimiterStart.length());
        std::string jsonString = contentString.substr(0, contentString.find(delimiterEnd));
        jsonString = ReplaceAll(jsonString, std::string("\\\\"), std::string("\\"));
        jsonString = ReplaceAll(jsonString, std::string("\\\'"), std::string("\'"));

        try
        {
            if (nlohmann::json::accept(jsonString)) {
                nlohmann::json json_object = nlohmann::json::parse(jsonString);
                return json_object;
            }
        }
        catch (nlohmann::json::exception&) {
            return false;
        }
        return false;
    }

    nlohmann::json GetConfig()
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
        GetConfig();

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
};

#endif