#include <iostream>
#include <filesystem>
#include <vector>
#include <string>
#include <logger/log.h>
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

    void DisablePlugin(std::string plugin) {
        m_settingsStorePtr->TogglePluginStatus(plugin, false);
    }

    void EnablePlugin(std::string plugin) {
        m_settingsStorePtr->TogglePluginStatus(plugin, true);
    }

    void ListAllPlugins() {
        for (const auto& plugin : m_allPlugins) {
            if (std::find(m_enabledPlugins.begin(), m_enabledPlugins.end(), plugin.pluginName) != m_enabledPlugins.end()) {
                std::cout << plugin.pluginName << " - " << BOLD << GREEN << "enabled" << RESET << std::endl;
            }
            else {
                std::cout << plugin.pluginName << " - " << BOLD << RED << "disabled" << RESET << std::endl;
            }
        }
    }

    void ListDisabledPlugins() {
        for (const auto& plugin : m_allPlugins) {
            if (std::find(m_enabledPlugins.begin(), m_enabledPlugins.end(), plugin.pluginName) == m_enabledPlugins.end()) {
                std::cout << plugin.pluginName << std::endl;
            }
        }
    }

    void ListEnabledPlugins() {
        for (const auto& plugin : m_enabledPlugins) {
            std::cout << plugin << std::endl;
        }
    }

    PluginManager() {
        this->m_settingsStorePtr = std::make_unique<SettingsStore>();
        this->m_allPlugins = this->m_settingsStorePtr->ParseAllPlugins();
        this->m_enabledPlugins = this->m_settingsStorePtr->GetEnabledPlugins();
    }
};