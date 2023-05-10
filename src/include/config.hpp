#ifndef CONFIG
#define CONFIG

#include <windows.h>
#include <iostream>
#include <fstream>
#include <string>
#include <filesystem>
#include <Lmcons.h>
#include <string>
#include <sstream>
#include <fstream>
#include <winsock2.h>
#include <chrono>
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
            /*else {

                std::string path = std::format("{}/settings.json", SteamSkinPath);

                MessageBoxA(0, std::format("json from \n[{}] is invalid.\nits likely an extra , at the end of the json", path).c_str(), "Millennium", MB_ICONINFORMATION);
                __fastfail(0);
            }*/
        }
        catch (nlohmann::json::exception& ex) { 
            return 0;
        }
    }

    nlohmann::json get_skin_config()
    {
        GetConfig();

        std::ifstream configFile(std::format("{}/{}/config.json", SteamSkinPath, currentSkin));
        if (!configFile.is_open() || !configFile) {
            //std::cout << "Failed to open config file!" << std::endl;
        }

        std::stringstream buffer;
        buffer << configFile.rdbuf();

        if (nlohmann::json::accept(buffer.str())) {
            nlohmann::json json_object = nlohmann::json::parse(buffer.str());
            return json_object;
        }
        //else {
        //    std::string path = std::format("{}/{}/config.json", SteamSkinPath, currentSkin);
        //    MessageBoxA(0, std::format("json from \n[{}] is invalid.\nits likely an extra , at the end of the json key.", path).c_str(), "Millennium", MB_ICONINFORMATION);
        //}
    }
};

#endif // CONFIG