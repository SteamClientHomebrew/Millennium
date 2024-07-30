#include <iostream>
#include <algorithm> 
#include <variant>
#include <CLI/CLI.hpp>
#include <core/plugins.h>
#include <core/config.h>
#include <core/themes.h>
#include <util/reload_steam.h>
#include <util/restart_steam.h>

class Millennium {
private:
    int argumentCount;
    std::unique_ptr<CLI::App> app;
    CLI::Option* version;

    CLI::App* plugins, *enable, *disable, *list;
    CLI::Option* list_enabled, *list_disabled;
    CLI::Option* enable_plugin_name, *disable_plugin_name;

    CLI::App* config;
    CLI::Option* configField, *configValue;

    CLI::App* themes, *use, *theme_list, *theme_current;
    CLI::Option* use_theme_name, *themeValue;

    CLI::App* steam, *restart, *reload;

public:
    Millennium() {
        this->app = std::make_unique<CLI::App>("Millennium@" + std::string(MILLENNIUM_VERSION));
        this->version = app->add_flag("-v,--version", "Print the version of Millennium");

        this->plugins = app->add_subcommand("plugins", "Interface with Millenniums plugin system."); 
            this->enable = plugins->add_subcommand("enable", "Enable a plugin from its name"); 
                this->enable_plugin_name = enable->add_option("plugin_name", "Name of the plugin to enable")->required();
            
            this->disable = plugins->add_subcommand("disable", "Disable a plugin from its name"); 
                this->disable_plugin_name = disable->add_option("plugin_name", "Name of the plugin to disable")->required();
            
        this->list = plugins->add_subcommand("list", "List all plugins and their status"); 
            this->list_enabled = list->add_flag("-e,--enabled", "List all enabled plugins.");
            this->list_disabled = list->add_flag("-d,--disabled", "List all disabled plugins.");

        this->themes = app->add_subcommand("themes", "Customize & manage your theme settings."); 
            this->use = themes->add_subcommand("use", "Use a specific theme"); 
                this->use_theme_name = use->add_option("theme_name", "Name of the theme to enable")->required();
            
            this->theme_list = themes->add_subcommand("list", "List all found user themes."); 
            this->theme_current = themes->add_subcommand("current", "Print the current active theme and exit."); 
            
        this->config = app->add_subcommand("config", "Interact with your local Millennium configuration."); 
            this->configField = config->add_option("field", "Print a specific fields value");
            this->configValue = config->add_option("value", "Assign a value to a field");
        
        // this->themeConfig = app->add_subcommand("theme_config", "Interact with your theme settings."); 
        //     this->themeField = themeConfig->add_option("field", "Print a specific fields value");
        //     this->themeValue = themeConfig->add_option("value", "Assign a value to a field");
        
        this->steam = app->add_subcommand("steam", "Interact with your local Steam installation."); 
            this->restart = steam->add_subcommand("restart", "Restart the Steam client");
            this->reload = steam->add_subcommand("reload", "Reload the Steam client interface");
    }

    int Parse(int argc, char* argv[]) {
        this->argumentCount = argc;
        try {
            (*app).parse(argc, argv); 
        } 
        catch(const CLI::ParseError &e) { 
            return (*app).exit(e); 
        }
        return 0;
    }

    int Steam() {
        if (restart->parsed()) {
            RestartSteam();
            return 0;
        }
        else if (reload->parsed()) {
            ReloadSteam();
            return 0;
        }

        std::cout << steam->help();
        return 0;
    }

    int Config() {
        if (!configValue->as<std::string>().empty()) {
            SetConfig(configField->as<std::string>(), configValue->as<std::string>());
        }
        else {
            GetConfig(configField->as<std::string>());
        }
        return 0;
    }

    int Plugins() {
        std::unique_ptr<PluginManager> pluginManager = std::make_unique<PluginManager>();

        if (enable->parsed()) {
            pluginManager->EnablePlugin(enable_plugin_name->as<std::string>());
        }
        else if (disable->parsed()) {
            pluginManager->DisablePlugin(disable_plugin_name->as<std::string>());
        }
        else if (list->parsed()) {
            if (list_enabled->as<bool>()) {
                pluginManager->ListEnabledPlugins();
            }
            else if (list_disabled->as<bool>()) {
                pluginManager->ListDisabledPlugins();
            }
            else {
                pluginManager->ListAllPlugins();
            }
        }
        else {
            std::cout << plugins->help();
        }
        return 0;
    }

    int ThemeConfig() {
        if (theme_list->parsed()) {
            PrintAllThemes();
        }
        else if (theme_current->parsed()) {
            PrintCurrentTheme();
        }
        else if (use->parsed()) {
            //SetTheme(use_theme_name->as<std::string>());
        }
        else {
            std::cout << themes->help();
        }
        return 0;
    }

    int Run() {
        if (this->argumentCount == 1) {
            std::cout << app->help();
            return 0;
        }

        if (version->as<bool>()) {
            std::cout << MILLENNIUM_VERSION << std::endl;
            return 0;
        }

        if (steam->parsed())   return this->Steam();
        if (config->parsed())  return this->Config();
        if (themes->parsed())  return this->ThemeConfig();
        if (plugins->parsed()) return this->Plugins();
        
        return 0;
    }
};

#ifdef _WIN32
std::string GetSteamPathFromRegistry() {
    HKEY hKey;
    char value[512];
    DWORD valueLength = sizeof(value);
    LONG result;

    result = RegOpenKeyExA(HKEY_CURRENT_USER, "Software\\Valve\\Steam", 0, KEY_READ, &hKey);
    if (result != ERROR_SUCCESS) {
        std::cerr << "Error opening registry key: " << result << std::endl;
        return {};
    }

    result = RegQueryValueExA(hKey, "SteamPath", nullptr, nullptr, (LPBYTE)value, &valueLength);
    if (result != ERROR_SUCCESS) {
        std::cerr << "Error reading registry value: " << result << std::endl;
        RegCloseKey(hKey);
        return {};
    }

    RegCloseKey(hKey);
    return std::string(value, valueLength - 1);
}
#endif

int main(int argc, char* argv[]) {

    // read steam path from registry
    #ifdef _WIN32
    SetEnvironmentVariable("SteamPath", GetSteamPathFromRegistry().c_str());
    #endif

    std::unique_ptr<Millennium> millennium = std::make_unique<Millennium>();
    int parseResult = millennium->Parse(argc, argv);

    if (parseResult != 0) return parseResult;
    return millennium->Run();
}