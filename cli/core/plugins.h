#include <iostream>
#include <filesystem>
#include <vector>
#include <string>
#include <util/log.h>
#include <mini/ini.h>
#include <core/config.h>
#include <tuple>
#include <nlohmann/json.hpp>
#include <sys/locals.h>

class PluginManager {
private:
    std::unique_ptr<SettingsStore> m_settingsStorePtr;
    std::vector<SettingsStore::PluginTypeSchema> m_allPlugins;
    std::vector<std::string> m_enabledPlugins;

public:

    int DisablePlugin(std::string plugin) {
        m_settingsStorePtr->TogglePluginStatus(plugin, false);
        return 0;
    }

    int EnablePlugin(std::string plugin) {
        m_settingsStorePtr->TogglePluginStatus(plugin, true);
        return 0;
    }

    int ListAllPlugins() {
        for (const auto& plugin : m_allPlugins) {
            const bool bIsEnabled = std::find(m_enabledPlugins.begin(), m_enabledPlugins.end(), plugin.pluginName) != m_enabledPlugins.end();
            std::cout << plugin.pluginName << " - " << BOLD << (bIsEnabled ? GREEN + std::string("enabled") : RED + std::string("disabled")) << RESET << std::endl;
        }
        return 0;
    }

    int ListDisabledPlugins() {
        for (const auto& plugin : m_allPlugins) {
            if (std::find(m_enabledPlugins.begin(), m_enabledPlugins.end(), plugin.pluginName) == m_enabledPlugins.end()) {
                std::cout << plugin.pluginName << std::endl;
            }
        }
        return 0;
    }

    int ListEnabledPlugins() {
        for (const auto& plugin : m_enabledPlugins) {
            std::cout << plugin << std::endl;
        }
        return 0;
    }

    PluginManager() {
        this->m_settingsStorePtr = std::make_unique<SettingsStore>();
        this->m_allPlugins = this->m_settingsStorePtr->ParseAllPlugins();
        this->m_enabledPlugins = this->m_settingsStorePtr->GetEnabledPlugins();
    }
};