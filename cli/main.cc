#ifdef _WIN32
#include <windows.h>
#endif
#include <iostream>
#include <algorithm> 
#include <variant>
#include <CLI/CLI.hpp>
#include <core/plugins.h>
#include <core/config.h>
#include <core/themes.h>
#include <util/steam.h>
#include "posix/patch.h"

#define SUBPARSED(subcommand) (subcommand->parsed())
#define str(x) (x->as<std::string>())
#define asbool(x) (x->as<bool>())

class Millennium {
private:
    int argumentCount;
    std::unique_ptr<CLI::App> m_MillenniumApp;
    CLI::Option* version;

    CLI::App* sbPlugins, *sbPluginEnable, *sbPluginDisable, *sbPluginList;
    CLI::Option* optListEnabledPlugins, *optListDisabledPlugins;
    CLI::Option* optEnablePluginName, *optDisablePluginName;

    CLI::App* sbConfig;
    CLI::Option* configField, *configValue;

    CLI::App* sbThemes, *sbUseTheme, *sbListThemes, *sbCurrentTheme;
    CLI::App* sbThemeConfig, *sbThemeColors;
    CLI::Option* sbThemeConfigName, *optThemeColorsName;
    CLI::Option* optThemeConfigView;

    CLI::Option* optUseThemeName, *themeValue;

    CLI::App* asExecApply;
    CLI::Option* force;

public:
    Millennium() {
        m_MillenniumApp = std::make_unique<CLI::App>("Millennium@" + std::string(MILLENNIUM_VERSION));
        m_MillenniumApp->footer("Documentation: https://docs.steambrew.app");
        version = m_MillenniumApp->add_flag("-v,--version", "Print the version of Millennium");

        /** Handle plugins subcommands */
        sbPlugins = m_MillenniumApp->add_subcommand("plugins", "Interface with Millenniums plugin system.")->require_subcommand(); 
        {
            sbPluginEnable = sbPlugins->add_subcommand("enable", "Enable a plugin from its name"); 
            {
                optEnablePluginName = sbPluginEnable->add_option("plugin_name", "Name of the plugin to enable")->required();
            }
            
            sbPluginDisable = sbPlugins->add_subcommand("disable", "Disable a plugin from its name"); 
            {
                optDisablePluginName = sbPluginDisable->add_option("plugin_name", "Name of the plugin to disable")->required();
            }
            
            sbPluginList = sbPlugins->add_subcommand("list", "List all plugins and their status"); 
            {
                optListEnabledPlugins  = sbPluginList->add_flag("-e,--enabled", "List all enabled plugins.");
                optListDisabledPlugins = sbPluginList->add_flag("-d,--disabled", "List all disabled plugins.");
            }
        }

        /** Handle themes subcommand */
        sbThemes = m_MillenniumApp->add_subcommand("themes", "Customize & manage your theme settings.")->require_subcommand(); 
        {
            sbUseTheme = sbThemes->add_subcommand("use", "Use a specific theme"); 
            {
                optUseThemeName = sbUseTheme->add_option("theme_name", "Name of the theme to enable")->required();
            }
            
            sbListThemes   = sbThemes->add_subcommand("list", "List all found user themes.");
            sbCurrentTheme = sbThemes->add_subcommand("current", "Print the current active theme and exit.");

            sbThemeConfig = sbThemes->add_subcommand("config", "Interact with your theme settings.")->require_option(1); 
            {
                sbThemeConfigName  = sbThemeConfig->add_option("theme_name", "Name of the theme to configure");
                optThemeConfigView = sbThemeConfig->add_flag("-v,--view", "List all themes that have config.");
            }

            sbThemeColors = sbThemes->add_subcommand("colors", "Interact with your theme colors."); 
            {
                optThemeColorsName = sbThemeColors->add_option("theme_name", "Name of the theme to configure")->required();
            }
        }
            
        /** Handle config commands */
        sbConfig = m_MillenniumApp->add_subcommand("config", "Interact with your local Millennium configuration."); 
        {
            configField = sbConfig->add_option("field", "Print a specific fields value");
            configValue = sbConfig->add_option("value", "Assign a value to a field");
        }
        
        /** Handle apply commands (Restart & Reload) */
        asExecApply = m_MillenniumApp->add_subcommand("apply", "Apply configuration changes."); 
        {
            force = asExecApply->add_flag("-f,--force", "Force apply changes by restarting Steam.");
        }
    }

    int Parse(int argc, char* argv[]) {
        argumentCount = argc;

        try                             { (*m_MillenniumApp).parse(argc, argv);  } 
        catch(const CLI::ParseError &e) { return (*m_MillenniumApp).exit(e);     }
        return 0;
    }

    int Steam() {

        #ifdef __linux__
        // PatchPosixStartScript();
        #endif

        if (asbool(force)) { return ForceRestartSteam(); }
        return ReloadSteam();
    }

    int Config() {
        if (!str(configValue).empty()) { return SetConfig(str(configField), str(configValue)); } 
        return GetConfig(str(configField));
    }

    int Plugins() {
        std::unique_ptr<PluginManager> pluginManager = std::make_unique<PluginManager>();

        if (SUBPARSED(sbPluginEnable) ) { return pluginManager->EnablePlugin(str(optEnablePluginName));   }
        if (SUBPARSED(sbPluginDisable)) { return pluginManager->DisablePlugin(str(optDisablePluginName)); }

        if (SUBPARSED(sbPluginList)) {
            if (asbool(optListEnabledPlugins))  { return pluginManager->ListEnabledPlugins();  }
            if (asbool(optListDisabledPlugins)) { return pluginManager->ListDisabledPlugins(); }

            return pluginManager->ListAllPlugins();
        }

        return 0;
    }

    int ThemeConfig() {
        if (SUBPARSED(sbListThemes)  ) { return PrintAllThemes();                         }
        if (SUBPARSED(sbCurrentTheme)) { return PrintCurrentTheme();                      }
        if (SUBPARSED(sbUseTheme)    ) { return SetTheme(str(optUseThemeName));           }

        if (SUBPARSED(sbThemeConfig) ) { 
            if (!str(sbThemeConfigName).empty()) { return PrintThemeConfig(str(sbThemeConfigName)); }
            if (asbool(optThemeConfigView))      { return PrintThemesWithConfig();                  }
        }
        if (SUBPARSED(sbThemeColors) ) { std::cout << "theme_colors" << std::endl; }

        return 0;
    }

    int Run() {
        if (argumentCount == 1) {
            std::cout << m_MillenniumApp->help();
            return 0;
        }

        if (asbool(version)) {
            std::cout << MILLENNIUM_VERSION << std::endl;
            return 0;
        }

        if (asExecApply->parsed()) return Steam();
        if (sbConfig->parsed()   ) return Config();
        if (sbThemes->parsed()   ) return ThemeConfig();
        if (sbPlugins->parsed()  ) return Plugins();
        
        return 0;
    }
};

int main(int argc, char* argv[]) {
    #ifdef _WIN32
    SetEnvironmentVariable("SteamPath", GetSteamPathFromRegistry().c_str());
    #endif

    std::unique_ptr<Millennium> millennium = std::make_unique<Millennium>();
    int parseResult = millennium->Parse(argc, argv);

    if (parseResult != 0) return parseResult;
    return millennium->Run();
}