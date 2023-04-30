#ifndef CONFIG
#define CONFIG

class SkinConfig
{
private:
    std::string SteamSkinPath;
    std::string currentSkin;
public:

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

    nlohmann::json GetConfig()
    {
        std::ifstream configFile(std::format("{}/settings.json", SteamSkinPath));
        if (!configFile.is_open() || !configFile) {
            std::cout << "Failed to open config file!" << std::endl;
        }

        std::stringstream buffer;
        buffer << configFile.rdbuf();

        try {
            nlohmann::json json = nlohmann::json::parse(buffer.str());

            currentSkin = json["active-skin"];

            return json;
        }
        catch (const nlohmann::json::exception& e) {
            std::cout << "Error parsing JSON: " << e.what() << std::endl;
        }
        catch (const nlohmann::detail::type_error& e) {
            std::cout << "Error parsing JSON: " << e.what() << std::endl;
        }
    }

    nlohmann::json GetSkinConfig()
    {
        std::ifstream configFile(std::format("{}/{}/config.json", SteamSkinPath, currentSkin));
        if (!configFile.is_open() || !configFile) {
            std::cout << "Failed to open config file!" << std::endl;
        }

        std::stringstream buffer;
        buffer << configFile.rdbuf();

        try {
            nlohmann::json json = nlohmann::json::parse(buffer.str());
            return json;
        }
        catch (const nlohmann::json::exception& e) {
            std::cout << "Error parsing JSON: " << e.what() << std::endl;
        }
        catch (const nlohmann::detail::type_error& e) {
            std::cout << "Error parsing JSON: " << e.what() << std::endl;
        }
    }
};

#endif // CONFIG